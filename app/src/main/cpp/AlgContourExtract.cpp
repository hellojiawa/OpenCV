#include "AlgContourExtract.h"
#include "AlgStructDef.h"
#include "AlgKmeans.h"
#include "AlgConnected.h"
#include "AlgCvt.h"
#include "AlgTk_baseStruct.h"
//#include <algorithm>
#include <vector>
#include <string.h>
#include <math.h>
//#include <assert.h>

using namespace std;


#define RECTSIZE 0.8

#define WIDTH_GRAB	(144)
#define HEIGHT_GRAB	(176)


static void medianFilter(unsigned char* input, unsigned char* output, int width, int height);
static void medianBlur_SortNet(const unsigned char* _src, unsigned char* _dst);
static void imgBilinear(unsigned char* pSrc, int width, int height, unsigned char* pDst);
static void imgBilinear2(unsigned char* pSrc, int width, int height, unsigned char* pDst, int widthD, int heightD);
static unsigned char pixProtect(float pixIn);
static void imgRectLinear(unsigned char* pSrc, int width, int height, Alg_rect rect, unsigned char* pDst);
static void rectShrink(unsigned char* pMsk, Alg_ObjRect rtStd, Alg_ObjRect rtRoi, Alg_ObjRect* pRtOut);
enum
{
	GC_BGD = 0,  //!< background
	GC_FGD = 1,  //!< foreground
	GC_PR_BGD = 2,  //!< most probably background
	GC_PR_FGD = 3   //!< most probably foreground
};

/*
This is implementation of image segmentation algorithm GrabCut described in
"GrabCut — Interactive Foreground Extraction using Iterated Graph Cuts".
Carsten Rother, Vladimir Kolmogorov, Andrew Blake.
*/

/*
GMM - Gaussian Mixture Model
*/
class GMM2
{
public:
	static const int componentsCount = 5;

	GMM2();
	double operator()(unsigned char r, unsigned char g, unsigned char b) const;
	double operator()(int ci, unsigned char r, unsigned char g, unsigned char b) const;
	double operator()(int ci, double d0, double d1, double d2) const;
	int whichComponent(double d0, double d1, double d2) const;

	void initLearning();
	void addSample(int ci, float c0, float c1, float c2);
	void endLearning();

private:
	void calcInverseCovAndDeterm(int ci);
	//Mat model;
	double model[componentsCount * 13];
	double* coefs;
	double* mean;
	double* cov;

	double inverseCovs[componentsCount][3][3];
	double covDeterms[componentsCount];

	double sums[componentsCount][3];
	double prods[componentsCount][3][3];
	int sampleCounts[componentsCount];
	int totalSampleCount;
};


#define VTX_COUNT (WIDTH_GRAB*HEIGHT_GRAB)
#define EDGE_COUNT (2 * (4 * WIDTH_GRAB*HEIGHT_GRAB - 3 * (WIDTH_GRAB + HEIGHT_GRAB) + 2))



//////////////////////////////////////////////////
class GCGraph2
{
public:
	GCGraph2();
	~GCGraph2();
	int addVtx();//添加空结点
	void reset();
	void addEdges(int i, int j, double w, double revw);//添加点之间的边n-link 
	void addTermWeights(int i, double sourceW, double sinkW);//添加结点到顶点的边t-link
	double maxFlow();//最大流函数
	bool inSourceSegment(int i);//图对象调用最大流函数后，判断结点属不属于属于源点类（前景）
private:
	class Vtx//结点类
	{
	public:
		Vtx *next; // initialized and used in maxFlow() only//在maxflow算法中用于构建先进-先出队列
		int parent;
		int first;//首个相邻边
		int ts;//时间戳
		int dist;//到树根的距离
		double weight;
		unsigned char t;//图中结点的标签，取值0或1，0为源节点（前景点），1为汇节点（背景点）
	};
	class Edge//边类
	{
	public:
		int dst;//边指向的结点 
		int next;//该边的顶点的下一条边 
		double weight;//边的权重
	};

	Vtx vtcs[VTX_COUNT];//存放所有的结点
	Edge edges[EDGE_COUNT];//存放所有的边
	int nVtcs;
	int nEdges;

	double flow;//图的流量
};




typedef struct
{
	unsigned char m_img0[WIDTH_GRAB*HEIGHT_GRAB * 3];
	unsigned char m_img[WIDTH_GRAB*HEIGHT_GRAB * 3];
	double m_imgLeftW[WIDTH_GRAB*HEIGHT_GRAB];
	double m_imgUpleftW[WIDTH_GRAB*HEIGHT_GRAB];
	double m_imgUpW[WIDTH_GRAB*HEIGHT_GRAB];
	double m_imgUprightW[WIDTH_GRAB*HEIGHT_GRAB];
	int m_compIdxs[WIDTH_GRAB*HEIGHT_GRAB];
	unsigned char m_pMask[WIDTH_GRAB*HEIGHT_GRAB];
	void* m_ctHandle;
	Alg_Connected_Res ctRes;
	GCGraph2* m_pGraph;

	int m_nBgd;
	int m_nFgd;
	float m_bgdSpls[WIDTH_GRAB*HEIGHT_GRAB * DIMS];
	float m_fgdSpls[WIDTH_GRAB*HEIGHT_GRAB * DIMS];
	int m_bgdLabels[WIDTH_GRAB*HEIGHT_GRAB];
	int m_fgdLabels[WIDTH_GRAB*HEIGHT_GRAB];

	//unsigned char maskTest[WIDTH_GRAB*HEIGHT_GRAB];
	GMM2* pDgdGMM;
	GMM2* pFgdGMM;
}ST_gbHandle;

static ST_gbHandle* m_handle = NULL;


GMM2::GMM2()
{
	//一个像素的（唯一对应）高斯模型的参数个数或者说一个高斯模型的参数个数  
	//一个像素RGB三个通道值，故3个均值，3*3个协方差，共用一个权值  
	//const int modelSize = 3/*mean*/ + 9/*covariance*/ + 1/*component weight*/;
	coefs = model;
	mean = coefs + componentsCount;
	cov = mean + 3 * componentsCount;

	for (int ci = 0; ci < componentsCount; ci++)
	if (coefs[ci] > 0)
		calcInverseCovAndDeterm(ci);
}

double GMM2::operator()(unsigned char r, unsigned char g, unsigned char b) const
{
	double res = 0;
	for (int ci = 0; ci < componentsCount; ci++)
		res += coefs[ci] * (*this)(ci, r, g, b);
	return res;
}

double GMM2::operator()(int ci, unsigned char r, unsigned char g, unsigned char b) const
{
	double dr, dg, db;
	double res = 0;
	if (coefs[ci] > 0)
	{
		double* m = mean + 3 * ci;
		dr = r - m[0]; dg = g - m[1]; db = b - m[2];
		double mult = dr * (dr * inverseCovs[ci][0][0] + dg * inverseCovs[ci][1][0] + db * inverseCovs[ci][2][0])
					+ dg * (dr * inverseCovs[ci][0][1] + dg * inverseCovs[ci][1][1] + db * inverseCovs[ci][2][1])
					+ db * (dr * inverseCovs[ci][0][2] + dg * inverseCovs[ci][1][2] + db * inverseCovs[ci][2][2]);
		res = 1.0f / sqrt(covDeterms[ci]) * exp(-0.5f*mult);
	}
	return res;
}


double GMM2::operator()(int ci, double d0, double d1, double d2) const
{
	double res = 0;
	if (coefs[ci] > 0)
	{
		double* m = mean + 3 * ci;
		d0 -= m[0]; d1 -= m[1]; d2 -= m[2];
		double mult = d0 * (d0 * inverseCovs[ci][0][0] + d1 * inverseCovs[ci][1][0] + d2 * inverseCovs[ci][2][0])
			+ d1 * (d0 * inverseCovs[ci][0][1] + d1 * inverseCovs[ci][1][1] + d2 * inverseCovs[ci][2][1])
			+ d2 * (d0 * inverseCovs[ci][0][2] + d1 * inverseCovs[ci][1][2] + d2 * inverseCovs[ci][2][2]);
		res = 1.0f / sqrt(covDeterms[ci]) * exp(-0.5f*mult);
	}
	return res;
}


int GMM2::whichComponent(double d0, double d1, double d2) const
{
	int k = 0;
	double max = 0;

	for (int ci = 0; ci < componentsCount; ci++)
	{
		double p = (*this)(ci, d0, d1, d2);
		if (p > max)
		{
			k = ci;
			max = p;
		}
	}
	return k;
}

void GMM2::initLearning()
{
	for (int ci = 0; ci < componentsCount; ci++)
	{
		sums[ci][0] = sums[ci][1] = sums[ci][2] = 0;
		prods[ci][0][0] = prods[ci][0][1] = prods[ci][0][2] = 0;
		prods[ci][1][0] = prods[ci][1][1] = prods[ci][1][2] = 0;
		prods[ci][2][0] = prods[ci][2][1] = prods[ci][2][2] = 0;
		sampleCounts[ci] = 0;
	}
	totalSampleCount = 0;
}


//增加样本，即为前景或者背景GMM的第ci个高斯模型的像素集（这个像素集是来用估  
//计计算这个高斯模型的参数的）增加样本像素。计算加入color这个像素后，像素集  
//中所有像素的RGB三个通道的和sums（用来计算均值），还有它的prods（用来计算协方差），  
//并且记录这个像素集的像素个数和总的像素个数（用来计算这个高斯模型的权值）
void GMM2::addSample(int ci, float c0, float c1, float c2)
{
	sums[ci][0] += c0; sums[ci][1] += c1; sums[ci][2] += c2;
	prods[ci][0][0] += c0 * c0; prods[ci][0][1] += c0 * c1; prods[ci][0][2] += c0 * c2;
	prods[ci][1][0] += c1 * c0; prods[ci][1][1] += c1 * c1; prods[ci][1][2] += c1 * c2;
	prods[ci][2][0] += c2 * c0; prods[ci][2][1] += c2 * c1; prods[ci][2][2] += c2 * c2;
	sampleCounts[ci]++;
	totalSampleCount++;
}

//从图像数据中学习GMM的参数：每一个高斯分量的权值、均值和协方差矩阵；
void GMM2::endLearning()
{
	const double variance = 0.01;
	for (int ci = 0; ci < componentsCount; ci++)
	{
		int n = sampleCounts[ci];//第ci个高斯模型的样本像素个数 
		if (n == 0)
			coefs[ci] = 0;
		else
		{
			//计算第ci个高斯模型的权值系数 
			coefs[ci] = (double)n / totalSampleCount;

			//计算第ci个高斯模型的均值
			double* m = mean + 3 * ci;
			m[0] = sums[ci][0] / n; m[1] = sums[ci][1] / n; m[2] = sums[ci][2] / n;

			//计算第ci个高斯模型的协方差
			double* c = cov + 9 * ci;
			c[0] = prods[ci][0][0] / n - m[0] * m[0]; c[1] = prods[ci][0][1] / n - m[0] * m[1]; c[2] = prods[ci][0][2] / n - m[0] * m[2];
			c[3] = prods[ci][1][0] / n - m[1] * m[0]; c[4] = prods[ci][1][1] / n - m[1] * m[1]; c[5] = prods[ci][1][2] / n - m[1] * m[2];
			c[6] = prods[ci][2][0] / n - m[2] * m[0]; c[7] = prods[ci][2][1] / n - m[2] * m[1]; c[8] = prods[ci][2][2] / n - m[2] * m[2];

			//计算第ci个高斯模型的协方差的行列式 
			double dtrm = c[0] * (c[4] * c[8] - c[5] * c[7]) - c[1] * (c[3] * c[8] - c[5] * c[6]) + c[2] * (c[3] * c[7] - c[4] * c[6]);
			if (dtrm <= 2.2204460492503131e-016/*std::numeric_limits<double>::epsilon()*/)
			{
				//相当于如果行列式小于等于0，（对角线元素）增加白噪声，避免其变  
				//为退化（降秩）协方差矩阵（不存在逆矩阵，但后面的计算需要计算逆矩阵）。
				// Adds the white noise to avoid singular covariance matrix.
				c[0] += variance;
				c[4] += variance;
				c[8] += variance;
			}

			//计算第ci个高斯模型的协方差的逆Inverse和行列式Determinant 
			calcInverseCovAndDeterm(ci);
		}
	}
}

//计算协方差的逆Inverse和行列式Determinant 
void GMM2::calcInverseCovAndDeterm(int ci)
{
	if (coefs[ci] > 0)
	{
		//取第ci个高斯模型的协方差的起始指针 
		double *c = cov + 9 * ci;
		double dtrm =
			covDeterms[ci] = c[0] * (c[4] * c[8] - c[5] * c[7]) - c[1] * (c[3] * c[8] - c[5] * c[6]) + c[2] * (c[3] * c[7] - c[4] * c[6]);

		//在C++中，每一种内置的数据类型都拥有不同的属性, 使用<limits>库可以获  
		//得这些基本数据类型的数值属性。因为浮点算法的截断，所以使得，当a=2，  
		//b=3时 10*a/b == 20/b不成立。那怎么办呢？  
		//这个小正数（epsilon）常量就来了，小正数通常为可用给定数据类型的  
		//大于1的最小值与1之差来表示。若dtrm结果不大于小正数，那么它几乎为零。  
		//所以下式保证dtrm>0，即行列式的计算正确（协方差对称正定，故行列式大于0）。
		inverseCovs[ci][0][0] = (c[4] * c[8] - c[5] * c[7]) / dtrm;
		inverseCovs[ci][1][0] = -(c[3] * c[8] - c[5] * c[6]) / dtrm;
		inverseCovs[ci][2][0] = (c[3] * c[7] - c[4] * c[6]) / dtrm;
		inverseCovs[ci][0][1] = -(c[1] * c[8] - c[2] * c[7]) / dtrm;
		inverseCovs[ci][1][1] = (c[0] * c[8] - c[2] * c[6]) / dtrm;
		inverseCovs[ci][2][1] = -(c[0] * c[7] - c[1] * c[6]) / dtrm;
		inverseCovs[ci][0][2] = (c[1] * c[5] - c[2] * c[4]) / dtrm;
		inverseCovs[ci][1][2] = -(c[0] * c[5] - c[2] * c[3]) / dtrm;
		inverseCovs[ci][2][2] = (c[0] * c[4] - c[1] * c[3]) / dtrm;
	}
}




static void initMaskWithRect(unsigned char* mask, Alg_ObjRect rect)
{
	int i, j, step;
	int exc_x, exc_y;

	memset(mask, GC_BGD, HEIGHT_GRAB*WIDTH_GRAB);

	for (i = max(0, rect.y), step = i*WIDTH_GRAB; i < min(rect.y + rect.height, HEIGHT_GRAB - 1); i++)
	{
		for (j = max(0, rect.x); j < min(rect.x + rect.width, WIDTH_GRAB - 1); j++)
		{
			mask[step + j] = GC_PR_FGD;
		}

		step += WIDTH_GRAB;
	}
	exc_x = rect.width*0.35 + 0.5;
	exc_y = rect.height*0.35 + 0.5;
	for (i = max(0, rect.y + exc_y), step = i*WIDTH_GRAB; i < min(rect.y + rect.height - exc_y, HEIGHT_GRAB - 1); i++)
	{
		for (j = max(0, rect.x + exc_x); j < min(rect.x + rect.width - exc_x, WIDTH_GRAB - 1); j++)
		{
			mask[step + j] = GC_FGD;
		}

		step += WIDTH_GRAB;
	}



	return;
}


/*
Initialize GMM background and foreground models using kmeans algorithm.
*/
static void initGMMs(const unsigned char* img, const unsigned char* mask, GMM2& bgdGMM, GMM2& fgdGMM)
{
	const int kMeansItCount = 10;
	const int kMeansType = 2;

	int i, j, k, step1, step2;
	int nBgd, nFgd;
	float *pGddSpls, *pFgdSpls;

	pGddSpls = m_handle->m_bgdSpls;
	pFgdSpls = m_handle->m_fgdSpls;

	nBgd = nFgd = 0;
	for (i = 0, step1 = step2 = 0; i < HEIGHT_GRAB; i++)
	{
		for (j = 0, k = 0; j < WIDTH_GRAB; j++, k+=3)
		{
			if (0 == mask[step1 + j] || 2 == mask[step1 + j])
			{
				pGddSpls[nBgd * 3 + 0] = img[step2 + k + 0];
				pGddSpls[nBgd * 3 + 1] = img[step2 + k + 1];
				pGddSpls[nBgd * 3 + 2] = img[step2 + k + 2];
				nBgd++;
			} 
			else
			{
				pFgdSpls[nFgd * 3 + 0] = img[step2 + k + 0];
				pFgdSpls[nFgd * 3 + 1] = img[step2 + k + 1];
				pFgdSpls[nFgd * 3 + 2] = img[step2 + k + 2];
				nFgd++;
			}
		}

		step1 += WIDTH_GRAB;
		step2 += WIDTH_GRAB*3;
	}

	m_handle->m_nBgd = nBgd;
	m_handle->m_nFgd = nFgd;

	kmeans(m_handle->m_bgdSpls, m_handle->m_nBgd, N_GAUSS, m_handle->m_bgdLabels, 0, kMeansType);
	kmeans(m_handle->m_fgdSpls, m_handle->m_nFgd, N_GAUSS, m_handle->m_fgdLabels, 0, kMeansType);

	bgdGMM.initLearning();
	for (i = 0; i < m_handle->m_nBgd; i++)
	{
		//printf("\t labels:%d", m_handle->m_bgdLabels[i]);
		bgdGMM.addSample(m_handle->m_bgdLabels[i], m_handle->m_bgdSpls[i*DIMS + 0], m_handle->m_bgdSpls[i*DIMS + 1], m_handle->m_bgdSpls[i*DIMS + 2]);
	}
	bgdGMM.endLearning();

	fgdGMM.initLearning();
	for (i = 0; i < m_handle->m_nFgd; i++)
	{
		fgdGMM.addSample(m_handle->m_fgdLabels[i], m_handle->m_fgdSpls[i*DIMS + 0], m_handle->m_fgdSpls[i*DIMS + 1], m_handle->m_fgdSpls[i*DIMS + 2]);
	}
	fgdGMM.endLearning();
}



/*
Assign GMMs components for each pixel.
*/
static void assignGMMsComponents(const unsigned char* img, const unsigned char* mask, const GMM2& bgdGMM, const GMM2& fgdGMM, int* compIdxs)
{
	int i, j, k, step1, step2;

	for (i = 0, step1 = step2 = 0; i < HEIGHT_GRAB; i++)
	{
		for (j = 0, k = 0; j < WIDTH_GRAB; j++, k += 3)
		{
			compIdxs[step2 + j] = GC_BGD == mask[step2 + j] || GC_PR_BGD == mask[step2 + j] ?
				bgdGMM.whichComponent(img[step1 + k], img[step1 + k + 1], img[step1 + k + 2]) : fgdGMM.whichComponent(img[step1 + k], img[step1 + k + 1], img[step1 + k + 2]);
		}

		step1 += WIDTH_GRAB * 3;
		step2 += WIDTH_GRAB;
	}

	return;
}

/*
Learn GMMs parameters.
*/

static void learnGMMs(unsigned char* img, unsigned char* mask, int* compIdxs, GMM2& bgdGMM, GMM2& fgdGMM)
{
	bgdGMM.initLearning();
	fgdGMM.initLearning();

	int i, j, k, ci;
	int step1, step2;

	for (ci = 0; ci < GMM2::componentsCount; ci++)
	{
		for (i = 0, step1 = step2 = 0; i < HEIGHT_GRAB; i++)
		{
			for (j = 0, k = 0; j < WIDTH_GRAB; j++, k += 3)
			{
				if (ci == compIdxs[step2 + j])
				{
					if (GC_BGD == mask[step2 + j] || GC_PR_BGD == mask[step2 + j])
					{
						bgdGMM.addSample(ci, img[step1 + k], img[step1 + k + 1], img[step1 + k + 2]);
					} 
					else
					{
						fgdGMM.addSample(ci, img[step1 + k], img[step1 + k + 1], img[step1 + k + 2]);
					}
				}
			}

			step1 += WIDTH_GRAB * 3;
			step2 += WIDTH_GRAB;
		}
	}

	bgdGMM.endLearning();
	fgdGMM.endLearning();
}





GCGraph2::GCGraph2()
{
	flow = 0;
	nVtcs = nEdges = 0;
}



GCGraph2::~GCGraph2()
{
}


void GCGraph2::reset()
{
	nVtcs = nEdges = flow = 0;
}


int GCGraph2::addVtx()
{
	memset(&vtcs[nVtcs], 0, sizeof(Vtx));
	nVtcs++;

	return nVtcs-1;
}


void GCGraph2::addEdges(int i, int j, double w, double revw)
{
	Edge fromI, toI;
	fromI.dst = j;
	fromI.next = vtcs[i].first;
	fromI.weight = w;
	vtcs[i].first = nEdges;

	edges[nEdges] = fromI;
	nEdges++;

	toI.dst = i;
	toI.next = vtcs[j].first;
	toI.weight = revw;
	vtcs[j].first = nEdges;

	edges[nEdges] = toI;
	nEdges++;

	return;
}


void GCGraph2::addTermWeights(int i, double sourceW, double sinkW)
{
	double dw = vtcs[i].weight;
	if (dw > 0)
		sourceW += dw;
	else
		sinkW -= dw;
	flow += (sourceW < sinkW) ? sourceW : sinkW;
	vtcs[i].weight = sourceW - sinkW;
}


double GCGraph2::maxFlow()
{
	const int TERMINAL = -1, ORPHAN = -2;
	Vtx stub, *nilNode = &stub, *first = nilNode, *last = nilNode;
	int curr_ts = 0;
	stub.next = nilNode;
	Vtx *vtxPtr = &vtcs[0];
	Edge *edgePtr = &edges[0];

	/*std::*/vector<Vtx*> orphans;

	// initialize the active queue and the graph vertices
	for (int i = 0; i < nVtcs; i++)
	{
		Vtx* v = vtxPtr + i;
		v->ts = 0;
		if (v->weight != 0)
		{
			last = last->next = v;
			v->dist = 1;
			v->parent = TERMINAL;
			v->t = v->weight < 0;
		}
		else
			v->parent = 0;
	}
	first = first->next;
	last->next = nilNode;
	nilNode->next = 0;

	// run the search-path -> augment-graph -> restore-trees loop
	for (;;)
	{
		Vtx* v, *u;
		int e0 = -1, ei = 0, ej = 0;
		double minWeight, weight;
		unsigned char vt;

		// grow S & T search trees, find an edge connecting them
		while (first != nilNode)
		{
			v = first;
			if (v->parent)
			{
				vt = v->t;
				for (ei = v->first; ei != 0; ei = edgePtr[ei].next)
				{
					if (edgePtr[ei^vt].weight == 0)
						continue;
					u = vtxPtr + edgePtr[ei].dst;
					if (!u->parent)
					{
						u->t = vt;
						u->parent = ei ^ 1;
						u->ts = v->ts;
						u->dist = v->dist + 1;
						if (!u->next)
						{
							u->next = nilNode;
							last = last->next = u;
						}
						continue;
					}

					if (u->t != vt)
					{
						e0 = ei ^ vt;
						break;
					}

					if (u->dist > v->dist + 1 && u->ts <= v->ts)
					{
						// reassign the parent
						u->parent = ei ^ 1;
						u->ts = v->ts;
						u->dist = v->dist + 1;
					}
				}
				if (e0 > 0)
					break;
			}
			// exclude the vertex from the active list
			first = first->next;
			v->next = 0;
		}

		if (e0 <= 0)
			break;

		// find the minimum edge weight along the path
		minWeight = edgePtr[e0].weight;
		//assert(minWeight > 0);
		// k = 1: source tree, k = 0: destination tree
		for (int k = 1; k >= 0; k--)
		{
			for (v = vtxPtr + edgePtr[e0^k].dst;; v = vtxPtr + edgePtr[ei].dst)
			{
				if ((ei = v->parent) < 0)
					break;
				weight = edgePtr[ei^k].weight;
				minWeight = min(minWeight, weight);
				//assert(minWeight > 0);
			}
			weight = fabs(v->weight);
			minWeight = min(minWeight, weight);
			//assert(minWeight > 0);
		}

		// modify weights of the edges along the path and collect orphans
		edgePtr[e0].weight -= minWeight;
		edgePtr[e0 ^ 1].weight += minWeight;
		flow += minWeight;

		// k = 1: source tree, k = 0: destination tree
		for (int k = 1; k >= 0; k--)
		{
			for (v = vtxPtr + edgePtr[e0^k].dst;; v = vtxPtr + edgePtr[ei].dst)
			{
				if ((ei = v->parent) < 0)
					break;
				edgePtr[ei ^ (k ^ 1)].weight += minWeight;
				if ((edgePtr[ei^k].weight -= minWeight) == 0)
				{
					orphans.push_back(v);
					v->parent = ORPHAN;
				}
			}

			v->weight = v->weight + minWeight*(1 - k * 2);
			if (v->weight == 0)
			{
				orphans.push_back(v);
				v->parent = ORPHAN;
			}
		}

		// restore the search trees by finding new parents for the orphans
		curr_ts++;
		while (!orphans.empty())
		{
			Vtx* v2 = orphans.back();
			orphans.pop_back();

			int d, minDist = INT_MAX;
			e0 = 0;
			vt = v2->t;

			for (ei = v2->first; ei != 0; ei = edgePtr[ei].next)
			{
				if (edgePtr[ei ^ (vt ^ 1)].weight == 0)
					continue;
				u = vtxPtr + edgePtr[ei].dst;
				if (u->t != vt || u->parent == 0)
					continue;
				// compute the distance to the tree root
				for (d = 0;;)
				{
					if (u->ts == curr_ts)
					{
						d += u->dist;
						break;
					}
					ej = u->parent;
					d++;
					if (ej < 0)
					{
						if (ej == ORPHAN)
							d = INT_MAX - 1;
						else
						{
							u->ts = curr_ts;
							u->dist = 1;
						}
						break;
					}
					u = vtxPtr + edgePtr[ej].dst;
				}

				// update the distance
				if (++d < INT_MAX)
				{
					if (d < minDist)
					{
						minDist = d;
						e0 = ei;
					}
					for (u = vtxPtr + edgePtr[ei].dst; u->ts != curr_ts; u = vtxPtr + edgePtr[u->parent].dst)
					{
						u->ts = curr_ts;
						u->dist = --d;
					}
				}
			}

			if ((v2->parent = e0) > 0)
			{
				v2->ts = curr_ts;
				v2->dist = minDist;
				continue;
			}

			/* no parent is found */
			v2->ts = 0;
			for (ei = v2->first; ei != 0; ei = edgePtr[ei].next)
			{
				u = vtxPtr + edgePtr[ei].dst;
				ej = u->parent;
				if (u->t != vt || !ej)
					continue;
				if (edgePtr[ei ^ (vt ^ 1)].weight && !u->next)
				{
					u->next = nilNode;
					last = last->next = u;
				}
				if (ej > 0 && vtxPtr + edgePtr[ej].dst == v2)
				{
					orphans.push_back(u);
					u->parent = ORPHAN;
				}
			}
		}
	}
	return flow;
}


bool GCGraph2::inSourceSegment(int i)
{
	return vtcs[i].t == 0;
}



static void constructGCGraph(unsigned char* img, unsigned char* mask, const GMM2& bgdGMM, const GMM2& fgdGMM, double lambda,
					double* leftW, double* upleftW, double* upW, double* uprightW, GCGraph2* pGraph)
{	
	int i, j, k, step1, step2;
	for (i = 0, step1 = step2 = 0; i < HEIGHT_GRAB; i++)
	{
		for (j = 0, k = 0; j < WIDTH_GRAB; j++, k += 3)
		{
			// add node
			int vtxIdx = pGraph->addVtx();

			// set t-weights
			double fromSource, toSink;
			if (mask[step2 + j] == GC_PR_BGD || mask[step2 + j] == GC_PR_FGD)
			{
				fromSource = -log(bgdGMM(img[step1 + k], img[step1 + k + 1], img[step1 + k + 2]));
				toSink = -log(fgdGMM(img[step1 + k], img[step1 + k + 1], img[step1 + k + 2]));
			}
			else if (mask[step2 + j] == GC_BGD)
			{
				fromSource = 0;
				toSink = lambda;
			}
			else // GC_FGD
			{
				fromSource = lambda;
				toSink = 0;
			}
			pGraph->addTermWeights(vtxIdx, fromSource, toSink);

			// set n-weights
			if (j > 0)
			{
				double w = leftW[step2 + j];
				pGraph->addEdges(vtxIdx, vtxIdx - 1, w, w);
			}
			if (j > 0 && i > 0)
			{
				double w = upleftW[step2 + j];
				pGraph->addEdges(vtxIdx, vtxIdx - WIDTH_GRAB - 1, w, w);
			}
			if (i > 0)
			{
				double w = upW[step2 + j];
				pGraph->addEdges(vtxIdx, vtxIdx - WIDTH_GRAB, w, w);
			}
			if (j < WIDTH_GRAB - 1 && i > 0)
			{
				double w = uprightW[step2 + j];
				pGraph->addEdges(vtxIdx, vtxIdx - WIDTH_GRAB + 1, w, w);
			}
		}

		step1 += WIDTH_GRAB * 3;
		step2 += WIDTH_GRAB;
	}
}



static void estimateSegmentation(GCGraph2* pGraph, unsigned char* mask)
{
	int i, j, step;
	pGraph->maxFlow();

	for (i = 0, step = 0; i < HEIGHT_GRAB; i++)
	{
		for (j = 0; j < WIDTH_GRAB; j++)
		{
			if (GC_PR_BGD == mask[step + j] || GC_PR_FGD == mask[step + j])
			{
				if (pGraph->inSourceSegment(step + j))
				{
					mask[step + j] = 255/*GC_PR_FGD*/;
				}
				else
				{
					mask[step + j] = 0/*GC_PR_BGD*/;
				}
			}
			else if (GC_FGD == mask[step + j])
			{
				mask[step + j] = 255;
			}

		}

		step += WIDTH_GRAB;
	}

	return;
}




//计算beta，也就是Gibbs能量项中的第二项（平滑项）中的指数项的beta，用来调整  
//高或者低对比度时，两个邻域像素的差别的影响的，例如在低对比度时，两个邻域  
//像素的差别可能就会比较小，这时候需要乘以一个较大的beta来放大这个差别，  
//在高对比度时，则需要缩小本身就比较大的差别。  
//所以我们需要分析整幅图像的对比度来确定参数beta，具体的见论文公式（5）。
/*
	Calculate beta - parameter of GrabCut algorithm.
	beta = 1/(2*avg(sqr(||color[i] - color[j]||)))
*/
static double calcBeta(unsigned char* img)
{
	int x, y, k, step;
	unsigned char r, g, b;
	double beta;	
	
	beta = 0;
	for (y = 0, step = 0; y < HEIGHT_GRAB; y++)
	{
		for (x = 0, k = 0; x < WIDTH_GRAB; x++, k += 3)
		{
			r = img[step + k];
			g = img[step + k + 1];
			b = img[step + k + 2];
			if (x>0) // left
			{				
				beta += (r - img[step + k - 3])*(r - img[step + k - 3]) + (g - img[step + k - 3 + 1])*(g - img[step + k - 3 + 1]) +
								(b - img[step + k - 3 + 2])*(b - img[step + k - 3 + 2]);

			}
			if (y>0 && x > 0) // upleft
			{
				beta += (r - img[step + k - WIDTH_GRAB * 3 - 3])*(r - img[step + k - WIDTH_GRAB * 3 - 3]) + 
						(g - img[step + k - WIDTH_GRAB * 3 - 3 + 1])*(g - img[step + k - WIDTH_GRAB * 3 - 3 + 1]) +
						(b - img[step + k - WIDTH_GRAB * 3 - 3 + 2])*(b - img[step + k - WIDTH_GRAB * 3 - 3 + 2]);
			}
			if (y > 0) // up
			{

				beta += (r - img[step + k - WIDTH_GRAB * 3])*(r - img[step + k - WIDTH_GRAB * 3]) +
						(g - img[step + k - WIDTH_GRAB * 3 + 1])*(g - img[step + k - WIDTH_GRAB * 3 + 1]) +
						(b - img[step + k - WIDTH_GRAB * 3 + 2])*(b - img[step + k - WIDTH_GRAB * 3 + 2]);
			}
			if (y > 0 && x < WIDTH_GRAB - 1) // upright
			{

				beta += (r - img[step + k - WIDTH_GRAB * 3 + 3])*(r - img[step + k - WIDTH_GRAB * 3 + 3]) +
					(g - img[step + k - WIDTH_GRAB * 3 + 3 + 1])*(g - img[step + k - WIDTH_GRAB * 3 + 3 + 1]) +
					(b - img[step + k - WIDTH_GRAB * 3 + 3 + 2])*(b - img[step + k - WIDTH_GRAB * 3 + 3 + 2]);
			}
		}

		step += WIDTH_GRAB * 3;
	}
	if (beta <= 2.2204460492503131e-016)
		beta = 0;
	else
	{
		beta = 1.f / (2 * beta / (4 * WIDTH_GRAB*HEIGHT_GRAB - 3 * WIDTH_GRAB - 3 * HEIGHT_GRAB + 2));
	}


	return beta;
}


//计算图每个非端点顶点（也就是每个像素作为图的一个顶点，不包括源点s和汇点t）与邻域顶点  
//的边的权值。由于是无向图，我们计算的是八邻域，那么对于一个顶点，我们计算四个方向就行，  
//在其他的顶点计算的时候，会把剩余那四个方向的权值计算出来。这样整个图算完后，每个顶点  
//与八邻域的顶点的边的权值就都计算出来了。  
//这个相当于计算Gibbs能量的第二个能量项（平滑项），具体见论文中公式（4） 
/*
Calculate weights of noterminal vertices of graph.
beta and gamma - parameters of GrabCut algorithm.
*/
static void calcNWeights(const unsigned char* img, double* imgLeftW, double* imgUpleftW, double* imgUpW, double* imgUprightW, double beta, double gamma)
{
	const double gammaDivSqrt2 = gamma / 1.41421356;

	int x, y, k, step1, step2;
	unsigned char r, g, b;

	for (y = 0, step2 = 0, step1 = 0; y < HEIGHT_GRAB; y++)
	{
		for (x = 0, k = 0; x < WIDTH_GRAB; x++, k += 3)
		{
			r = img[step2 + k];
			g = img[step2 + k + 1];
			b = img[step2 + k + 2];

			if (x - 1 >= 0) // left
			{
				double clDot = (r - img[step2 + k - 3])*(r - img[step2 + k - 3]) +
								(g - img[step2 + k - 3 + 1])*(g - img[step2 + k - 3 + 1]) +
								(b - img[step2 + k - 3 + 2])*(b - img[step2 + k - 3 + 2]);
				imgLeftW[step1 + x] = gamma*exp(-beta*clDot);
			}
			else
				imgLeftW[step1 + x] = 0;

			if (x - 1 >= 0 && y - 1 >= 0) // upleft
			{
				double clDot = (r - img[step2 + k - WIDTH_GRAB * 3 - 3])*(r - img[step2 + k - WIDTH_GRAB * 3 - 3]) +
								(g - img[step2 + k + 1 - WIDTH_GRAB * 3 - 3])*(g - img[step2 + k + 1 - WIDTH_GRAB * 3 - 3]) +
								(b - img[step2 + k + 2 - WIDTH_GRAB * 3 - 3])*(b - img[step2 + k + 2 - WIDTH_GRAB * 3 - 3]);
				imgUpleftW[step1 + x] = gammaDivSqrt2*exp(-beta*clDot);
			}
			else
				imgUpleftW[step1 + x] = 0;

			if (y - 1 >= 0) // up
			{
				double clDot = (r - img[step2 + k - WIDTH_GRAB * 3])*(r - img[step2 + k - WIDTH_GRAB * 3]) +
								(g - img[step2 + k + 1 - WIDTH_GRAB * 3])*(g - img[step2 + k + 1 - WIDTH_GRAB * 3]) +
								(b - img[step2 + k + 2 - WIDTH_GRAB * 3])*(b - img[step2 + k + 2 - WIDTH_GRAB * 3]);
				imgUpW[step1 + x] = gamma*exp(-beta*clDot);
			}
			else
				imgUpW[step1 + x] = 0;

			if (x + 1 < WIDTH_GRAB && y - 1 >= 0) // upright
			{
				double clDot = (r - img[step2 + k - WIDTH_GRAB * 3 + 3])*(r - img[step2 + k - WIDTH_GRAB * 3 + 3]) +
							(g - img[step2 + k + 1 - WIDTH_GRAB * 3 + 3])*(g - img[step2 + k + 1 - WIDTH_GRAB * 3 + 3]) +
							(b - img[step2 + k + 2 - WIDTH_GRAB * 3 + 3])*(b - img[step2 + k + 2 - WIDTH_GRAB * 3 + 3]);
				imgUprightW[step1 + x] = gammaDivSqrt2*exp(-beta*clDot);
			}
			else
				imgUprightW[step1 + x] = 0;
		}

		step1 += WIDTH_GRAB, step2 += WIDTH_GRAB * 3;
	}
}



//#define TEST
#ifdef TEST
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/core/internal.hpp"
#include <cv.h>
#include <highgui.h>
using namespace cv;
#endif


void* AlgContourExtract_Create()
{
	m_handle = (ST_gbHandle*)malloc(sizeof(ST_gbHandle));

	memset(m_handle, 0, sizeof(ST_gbHandle));

	m_handle->m_pGraph = new GCGraph2;

	m_handle->m_ctHandle = Alg_Connected_create();

	m_handle->pDgdGMM = new GMM2;
	m_handle->pFgdGMM = new GMM2;

	return m_handle;
}



int AlgContourExtract_Destroy(void** hhandle)
{
	if (NULL == hhandle || NULL == (*hhandle))
	{
		return 0;
	}

	if (NULL != m_handle->m_pGraph)
	{
		delete m_handle->m_pGraph;
		m_handle->m_pGraph = NULL;
	}

	if (NULL != m_handle->m_ctHandle)
	{
		Alg_Connected_destroy(&m_handle->m_ctHandle);
	}

	free(m_handle);
	delete m_handle->pDgdGMM;
	delete m_handle->pFgdGMM;

	m_handle = NULL;
	(*hhandle) = NULL;

	return 1;
}



//static unsigned char imgSrc[1080 * 1920 * 3] = { 205 };
int AlgContourExtract_GrabCut(void* handle, unsigned char* _img, int width, int height, Alg_rect rect, Alg_rect* _rtOut, int iterCount)
{
	int widthRect;
	int heightRect;
	Alg_rect rectSrc, rectSc;
	float scale_x;
	float scale_y; 
	Alg_ObjRect rtIn;
	
	int k, p;
	Alg_ObjRect* pRect;

	float inv_scale_x;
	float inv_scale_y;

	Alg_ObjRect rectOut;

	if (NULL == _rtOut || rect.width < 5 || rect.height < 5 || NULL == handle || NULL == _img || width <= 0 || height <= 0 || iterCount <= 0)
	{
		return 0;
	}
	 widthRect = rect.width;
	 heightRect = rect.height;	

#ifdef TEST
		Mat matImg0(HEIGHT_GRAB, WIDTH_GRAB, CV_8UC3, m_handle->m_img0);
		Mat matImg(heightRect, widthRect, CV_8UC3, /*imgSrc*/m_handle->m_img0);
		Mat matImgMedianBlur(HEIGHT_GRAB, WIDTH_GRAB, CV_8UC3, m_handle->m_img);
		Mat matImgMask(HEIGHT_GRAB, WIDTH_GRAB, CV_8U, m_handle->m_pMask);
		Mat mat0(height, width, CV_8UC3, _img);
#endif

	rectSrc = rect;
	//rectSc = { int(widthRect*0.1 + 0.5), int(heightRect*0.1 + 0.5), int(widthRect*0.8 + 0.5), int(heightRect*0.8 + 0.5) };
	rectSc.x = widthRect*0.1 + 0.5;
	rectSc.y = heightRect*0.1 + 0.5;
	rectSc.width = widthRect*0.8 + 0.5;
	rectSc.height = heightRect*0.8 + 0.5;

	scale_x = (float)WIDTH_GRAB / (float)widthRect;
	scale_y = (float)HEIGHT_GRAB / (float)heightRect;
	
	rtIn.x = scale_x*rectSc.x + 0.5f;
	rtIn.y = scale_y*rectSc.y + 0.5f;
	rtIn.width = rectSc.width*scale_x + 0.5f;
	rtIn.height = rectSc.height*scale_y + 0.5f;
	
	imgRectLinear(_img, width, height, rectSrc, m_handle->m_img0);
	medianBlur_SortNet(m_handle->m_img0, m_handle->m_img);
	initMaskWithRect(m_handle->m_pMask, rtIn);
	initGMMs(m_handle->m_img, m_handle->m_pMask, *m_handle->pDgdGMM, *m_handle->pFgdGMM);

	if (iterCount <= 0)
		return 0;

	const double gamma = 50;
	const double lambda = 9 * gamma;
	const double beta = calcBeta(m_handle->m_img);

	calcNWeights(m_handle->m_img, m_handle->m_imgLeftW, m_handle->m_imgUpleftW, m_handle->m_imgUpW, m_handle->m_imgUprightW, beta, gamma);

	for (int i = 0; i < iterCount; i++)
	{
		m_handle->m_pGraph->reset();
		assignGMMsComponents(m_handle->m_img, m_handle->m_pMask, *m_handle->pDgdGMM, *m_handle->pFgdGMM, m_handle->m_compIdxs);
		learnGMMs(m_handle->m_img, m_handle->m_pMask, m_handle->m_compIdxs, *m_handle->pDgdGMM, *m_handle->pFgdGMM);
		constructGCGraph(m_handle->m_img, m_handle->m_pMask, *m_handle->pDgdGMM, *m_handle->pFgdGMM, lambda, m_handle->m_imgLeftW, m_handle->m_imgUpleftW, m_handle->m_imgUpW, m_handle->m_imgUprightW, m_handle->m_pGraph);
		estimateSegmentation(m_handle->m_pGraph, m_handle->m_pMask);
	}

	Alg_Connected_process(m_handle->m_ctHandle, m_handle->m_pMask, WIDTH_GRAB, HEIGHT_GRAB);
	Alg_Connected_getResult(m_handle->m_ctHandle, &m_handle->ctRes);

	k = p = 0;
	for (int j = 0; j < m_handle->ctRes.pfgRectSeq->num; j++)
	{
		pRect = Alg_FRS_getCell(m_handle->ctRes.pfgRectSeq, j);

		if (pRect->pxls > k)
		{
			k = pRect->pxls;
			p = j;
		}
	}

	if (0 == m_handle->ctRes.pfgRectSeq->num)
	{
		float exc = min(rect.width, rect.height)*/*0.08*//*0.06*/0.1/*0.1*/;

		_rtOut->x += exc;
		_rtOut->width -= exc * 2;
		_rtOut->y += exc;
		_rtOut->height -= exc * 2;
	}	
	else// (0 != m_handle->ctRes.pfgRectSeq->num)
	{
		 inv_scale_x = (float)widthRect / (float)WIDTH_GRAB;
		 inv_scale_y = (float)heightRect / (float)HEIGHT_GRAB;

		pRect = Alg_FRS_getCell(m_handle->ctRes.pfgRectSeq, p);
		
		//if ((pRect->width << 1) > rtIn.width && (pRect->height << 1) > rtIn.height && (pRect->width*pRect->height * 6) > rtIn.width*rtIn.height)
		{ 	

			/*四边收缩*/
			rectShrink(m_handle->m_pMask, rtIn, *pRect, &rectOut);
// 			if (rectOut.width < (pRect->width >> 1) || rectOut.height < (pRect->height >> 1))
// 			{
// 				rectOut.x = pRect->x + pRect->width*((1 - RECTSIZE) / 2);
// 				rectOut.y = pRect->y + pRect->height*((1 - RECTSIZE) / 2);
// 				rectOut.width = (pRect->width*RECTSIZE + 0.5);
// 				rectOut.height = (pRect->height*RECTSIZE + 0.5);
// 				//		(*_rtOut) = rect;
// 			}

			_rtOut->x = rectOut.x*inv_scale_x + rect.x + 0.5f;
			_rtOut->y = rectOut.y*inv_scale_y + rect.y + 0.5f;
			_rtOut->width = rectOut.width*inv_scale_x + 0.5f;
			_rtOut->height = rectOut.height*inv_scale_y + 0.5f;

			//if (_rtOut->width * 2 < _rtOut->height)
			//{
			//	_rtOut->y += (_rtOut->height - _rtOut->width * 2 >> 1);
			//	_rtOut->height = _rtOut->width * 2;
			//}
#ifdef TEST
			rectangle(matImgMask, cvPoint(rectOut.x, rectOut.y), cvPoint(rectOut.x + rectOut.width + 1, rectOut.y + rectOut.height + 1), Scalar(255, 255, 255), 1, 1, 0);
			rectangle(mat0, cvPoint(_rtOut->x, _rtOut->y), cvPoint(_rtOut->x + _rtOut->width - 1, _rtOut->y + _rtOut->height - 1), Scalar(255, 0, 0), 1, 1, 0);

#endif

		}
	}

 	//if (_rtOut->width < (pRect->width >> 1) || _rtOut->height < (pRect->height >> 1))
  //	{
 	//	_rtOut->x = (pRect->x + pRect->width*((1 - RECTSIZE) / 2) + 0.5)*inv_scale_x + rect.x + 0.5f;
 	//	_rtOut->y = (pRect->y + pRect->height*((1 - RECTSIZE) / 2) + 0.5)*inv_scale_y + rect.y + 0.5f;
 	//	_rtOut->width = (pRect->width*RECTSIZE + 0.5)*inv_scale_x + 0.5f;
 	//	_rtOut->height = (pRect->height*RECTSIZE + 0.5)*inv_scale_y + 0.5f;
  //		//		(*_rtOut) = rect;
  //	}

#ifdef TEST
	Mat matImgSrc(height, width, CV_8UC3, _img);
	rectangle(matImgSrc, cvPoint(_rtOut->x, _rtOut->y), cvPoint(_rtOut->x + _rtOut->width + 1, _rtOut->y + _rtOut->height + 1), Scalar(255, 0, 0), 1, 1, 0);
	rectangle(matImgSrc, cvPoint(rectSrc.x, rectSrc.y), cvPoint(rectSrc.x + rectSrc.width + 1, rectSrc.y + rectSrc.height + 1), Scalar(0,255,  0), 1, 1, 0);
#endif


	float exc = _rtOut->height - _rtOut->width * 1.5;

	if (exc > 0)
	{
		exc /= 2.f;
		_rtOut->y += (exc + 0.5);
		_rtOut->height -= (exc * 2 + 0.5);
	}

 	return 1;
}


/**********************************************************************************************
\函数功能:四边收缩
\参数	 mask		:  输入掩码
rectIn		: 输入矩形框
rectOut		: 输出矩形框
实现过程    ：在输入掩码中截取出一个矩形框大小的掩码矩阵，对这个矩形框大小的掩码矩阵分别进行
			上、下、左和右进行收缩，对于每一条边，直到条边上的非零点的个数小于总个数的一定的比率时
			收缩结束，为了保证收缩结果不至于太小，收缩不得超过原rtRoi的一定比率。
***********************************************************************************************/
#define COEF_X_CUT	(0.3)
#define COEF_Y_CUT	(/*0.25*/0.2)
#define COEF_X_LIMIT	(0.25)
#define COEF_Y_LIMIT	(0.2)

void rectShrink(unsigned char* pMsk, Alg_ObjRect rtStd, Alg_ObjRect rtRoi, Alg_ObjRect* pRtOut)
{
	int i, j, step_s, step_e, step;
	int n1, n2, n;
	int up, bt, lf, rt;
	float th;
	int cut;


	up = bt = lf = rt = -1;

	//上下收缩
	th = COEF_Y_CUT*rtRoi.width;
	cut = min(rtRoi.y + rtRoi.height, int(rtStd.y + rtStd.height*COEF_Y_LIMIT + 0.5f));
	for (i = rtRoi.y, step = i*WIDTH_GRAB; i < cut; i++)
	{
		for (j = rtRoi.x, n = 0; j < rtRoi.x + rtRoi.width; j++)
		{
			n += (0 != pMsk[step + j]);
		}

		if (n > th)
		{
			up = i;
			break;
		}

		step += WIDTH_GRAB;
	}

	up = -1 != up ? up : cut;

	cut = max(rtRoi.y, int(rtStd.y + rtStd.height*(1.f - COEF_Y_LIMIT) + 0.5f));
	bt = -1;
	for (i = rtRoi.y + rtRoi.height - 1, step = i*WIDTH_GRAB; i > cut; i--)
	{
		for (j = rtRoi.x, n = 0; j < rtRoi.x + rtRoi.width; j++)
		{
			n += (0 != pMsk[step + j]);
		}

		if (n > th)
		{
			bt = i;
			break;
		}

		step -= WIDTH_GRAB;
	}

	bt = -1 != bt ? bt : cut;

	////左右
	th = COEF_Y_CUT*rtRoi.height;
	cut = min(rtRoi.x + rtRoi.width, int(rtStd.x + rtStd.width*COEF_X_LIMIT + 0.5f));

	for (j = rtRoi.x; j < cut; j++)
	{
		for (i = rtRoi.y, n = 0, step = i*WIDTH_GRAB; i < rtRoi.y + rtRoi.height; i++, step += WIDTH_GRAB)
		{
			n += (0 != pMsk[step + j]);
		}

		if (n > th)
		{
			lf = j;
			break;
		}
	}

	lf = -1 != lf ? lf : cut;


	cut = max(rtRoi.x, int(rtStd.x + rtStd.width*(1.f - COEF_X_LIMIT) + 0.5f));

	for (j = rtRoi.x + rtRoi.width-1; j > cut; j--)
	{
		for (i = rtRoi.y, n = 0, step = i*WIDTH_GRAB; i < rtRoi.y + rtRoi.height; i++, step += WIDTH_GRAB)
		{
			n += (0 != pMsk[step + j]);
		}

		if (n > th)
		{
			rt = j;
			break;
		}
	}

	rt = -1 != rt ? rt : cut;

	//上下再次收缩
	rtRoi.x = lf;
	rtRoi.y = up;
	rtRoi.width = rt - lf + 1;
	rtRoi.height = bt - up + 1;
	up = bt = -1;
	float c = (float)rtRoi.height / (float)(rtRoi.width/**1.3*/);
	th = min(COEF_Y_CUT*rtRoi.width*c, 0.75*rtRoi.width);
	cut = min(rtRoi.y + rtRoi.height, int(rtStd.y + rtStd.height*COEF_Y_LIMIT/**c*/ + 0.5f));
	for (i = rtRoi.y, step = i*WIDTH_GRAB; i < cut; i++)
	{
		for (j = rtRoi.x, n = 0; j < rtRoi.x + rtRoi.width; j++)
		{
			n += (0 != pMsk[step + j]);
		}

		if (n > th)
		{
			up = i;
			break;
		}

		step += WIDTH_GRAB;
	}

	up = -1 != up ? up : cut;

	cut = max(rtRoi.y, int(rtStd.y + rtStd.height*(1.f - COEF_Y_LIMIT/**c*/) + 0.5f));
	bt = -1;
	for (i = rtRoi.y + rtRoi.height - 1, step = i*WIDTH_GRAB; i > cut; i--)
	{
		for (j = rtRoi.x, n = 0; j < rtRoi.x + rtRoi.width; j++)
		{
			n += (0 != pMsk[step + j]);
		}

		if (n > th)
		{
			bt = i;
			break;
		}

		step -= WIDTH_GRAB;
	}

	bt = -1 != bt ? bt : cut;
	bt = min(bt, (int)(up + rtRoi.width*1.3));

	
	pRtOut->x = lf;
	pRtOut->y = up;
	pRtOut->width = rt - lf + 1;
	pRtOut->height = bt - up + 1;


	
	return;
}




//void rectShrink(unsigned char* mask, Alg_ObjRect* rectIn, Alg_ObjRect* rectOut)
//{
//	/*四边收缩量*/
//	int nLeft = 0;
//	int nRight = 0;
//	int nTop = 0;
//	int nBottom = 0;
//
//	/*掩码中一个点的临时标记*/
//	unsigned char tempMask1, tempMask2;
//
//	Alg_ObjRect* _pRect = rectIn;
//
//	/*掩码中四条边的的前景点的个数*/
//	int nCntMaskTop = 0;
//	int nCntMaskBottom = 0;
//	int nCntMaskLeft = 0;
//	int nCntMaskRight = 0;
//
//	/*上下两边收缩*/
//	for (int i = 1; i < (_pRect->height - 1); i++)
//	{
//		/*对于每一行计算其非零点的个数*/
//		nCntMaskTop = 0;
//		nCntMaskBottom = 0;
//		for (int j = 1; j < (_pRect->width - 1); j++)
//		{
//			nCntMaskTop += (0 != mask[(i + _pRect->y)*WIDTH_GRAB + j + _pRect->x]);
//			nCntMaskBottom += (0 != mask[((_pRect->height - 1 - i) + _pRect->y)*WIDTH_GRAB + j + _pRect->x]);
//		}
//		/*非零点的个数小于宽的一半且只收缩一次*/
//		if ((nCntMaskTop > (_pRect->width)*COEF_X_CUT) && nTop == 0)
//		{
//			nTop = i;
//			if (nTop >= (_pRect->height - 1)*COEF_Y_LIMIT + 0.5)
//				nTop = (_pRect->height - 1)*COEF_Y_LIMIT + 0.5;
//		}
//		if ((nCntMaskBottom > (_pRect->width)*COEF_X_CUT) && nBottom == 0)
//		{
//			nBottom = _pRect->height - i;
//			if (nBottom <= (_pRect->height - 1)*(1.f - COEF_Y_LIMIT) + 0.5)
//				nBottom = (_pRect->height - 1)*(1.f - COEF_Y_LIMIT) + 0.5;
//		}
//	}
//	/*左右两边进行收缩*/
//	for (int i = 1; i < (_pRect->width - 1); i++)
//	{
//		/*对于每一列计算其非零点的个数*/
//		nCntMaskLeft = 0;
//		nCntMaskRight = 0;
//		for (int j = 1; j < (_pRect->height - 1); j++)
//		{
//			tempMask1 = mask[(j + _pRect->y)*WIDTH_GRAB + i + _pRect->x];
//			tempMask2 = mask[(j + _pRect->y)*WIDTH_GRAB + (_pRect->width - 1 - i) + _pRect->x];
//
//			if (int(tempMask1) != 0)
//				nCntMaskLeft++;
//			if (int(tempMask2) != 0)
//				nCntMaskRight++;
//		}
//		/*非零点的个数小于高的一半且只收缩一次*/
//		if ((nCntMaskLeft > (_pRect->height - 1)*COEF_Y_CUT) && nLeft == 0)
//		{
//			nLeft = i;
//			if (nLeft >= (_pRect->width - 1)*COEF_X_LIMIT + 0.5)
//				nLeft = (_pRect->width - 1)*COEF_X_LIMIT + 0.5;
//		}
//		if ((nCntMaskRight > (_pRect->height - 1)*COEF_Y_CUT) && nRight == 0)
//		{
//			nRight = _pRect->width - i;
//			if (nRight <= (_pRect->width - 1)*(1.f - COEF_X_LIMIT) + 0.5)
//				nRight = (_pRect->width - 1)*(1.f - COEF_X_LIMIT) + 0.5;
//		}
//	}
//
//	rectOut->x = _pRect->x + nLeft;
//	rectOut->y = _pRect->y + nTop;
//	rectOut->width = nRight - nLeft;
//	rectOut->height = nBottom - nTop;
//
//}






const unsigned char g_Saturate8u[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
	128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
	144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
	176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
	192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
	208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
	224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255
};

#define FAST_CAST_8U(t)   (/*assert(-256 <= (t) && (t) <= 512), */g_Saturate8u[(t)+256])




inline void FAST_OP(int& a, int& b)
{
	int t = FAST_CAST_8U(a - b);
	b += t; a -= t;
	return;
}



static void medianBlur_SortNet(const unsigned char* _src, unsigned char* _dst)
{
	const unsigned char* src = (const unsigned char*)_src;
	unsigned char* dst = (unsigned char*)_dst;
	int i, j, k, cn = 3;

	int widthbytes = WIDTH_GRAB * 3;

	for (i = 0; i < HEIGHT_GRAB; i++, dst += widthbytes)
	{
		const unsigned char* row0 = src + max(i - 1, 0)*widthbytes;
		const unsigned char* row1 = src + i*widthbytes;
		const unsigned char* row2 = src + min(i + 1, HEIGHT_GRAB - 1)*widthbytes;

		for (j = 0; j < widthbytes; j++)
		{
			int j0 = j >= cn ? j - cn : j;
			int j2 = j < widthbytes - cn ? j + cn : j;
			int p0 = row0[j0], p1 = row0[j], p2 = row0[j2];
			int p3 = row1[j0], p4 = row1[j], p5 = row1[j2];
			int p6 = row2[j0], p7 = row2[j], p8 = row2[j2];

			FAST_OP(p1, p2); FAST_OP(p4, p5); FAST_OP(p7, p8); FAST_OP(p0, p1);
			FAST_OP(p3, p4); FAST_OP(p6, p7); FAST_OP(p1, p2); FAST_OP(p4, p5);
			FAST_OP(p7, p8); FAST_OP(p0, p3); FAST_OP(p5, p8); FAST_OP(p4, p7);
			FAST_OP(p3, p6); FAST_OP(p1, p4); FAST_OP(p2, p5); FAST_OP(p4, p7);
			FAST_OP(p4, p2); FAST_OP(p6, p4); FAST_OP(p4, p2);
			dst[j] = (unsigned char)p4;
		}
	}

	return;
}


static unsigned char pixProtect(float pixIn)
{
	if (pixIn > 255)
		return 255;
	if (pixIn < 0)
		return 0;
	else return pixIn;
}


static void imgBilinear(unsigned char* pSrc, int width, int height, unsigned char* pDst)
{
#ifdef TEST
	Mat matRsz;
	Mat matImg(height, width, CV_8UC3, pSrc);
	resize(matImg, matRsz, Size(WIDTH_GRAB, HEIGHT_GRAB));
	Mat matImg0(HEIGHT_GRAB, WIDTH_GRAB, CV_8UC3, pDst);
#endif

	int i, j, k, step_s, step_d, step;
	int iFixed, jFixed;
	float ifl, inv_i, jfl, inv_j;
	float scale_x, scale_y;

	scale_x = (float)width / (float)WIDTH_GRAB;
	scale_y = (float)height / (float)HEIGHT_GRAB;

	for (i = 0, step_d = 0; i < HEIGHT_GRAB; i++)
	{

		ifl = (float)((i + 0.5)*scale_y - 0.5);
		iFixed = ifl;
		ifl = ifl - iFixed;
		inv_i = 1.f - ifl;

		step_s = iFixed*(width * 3);

		for (j = 0, k = 0; j < WIDTH_GRAB; j++, k += 3)
		{

			jfl = (float)((j + 0.5)*scale_x - 0.5);
			jFixed = jfl;
			jfl = jfl - jFixed;
			inv_j = 1.f - jfl;
			jFixed = jFixed * 3;
			if ((jFixed != (width - 1) * 3) || (iFixed != height - 1))
			{
				pDst[step_d + k + 0] = pixProtect(pSrc[step_s + jFixed] * inv_i*inv_j + pSrc[step_s + jFixed + 3] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed + 3] * ifl*jfl);
				pDst[step_d + k + 1] = pixProtect(pSrc[step_s + jFixed + 1] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 1] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 1] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed + 3 + 1] * ifl*jfl);
				pDst[step_d + k + 2] = pixProtect(pSrc[step_s + jFixed + 2] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 2] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 2] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed + 3 + 2] * ifl*jfl);


			}			

			if (jFixed == (width - 1) * 3)
			{
				pDst[step_d + k + 0] = pixProtect(pSrc[step_s + jFixed] * inv_i*inv_j + pSrc[step_s + jFixed - 3] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed - 3] * ifl*jfl) ;
				pDst[step_d + k + 1] = pixProtect( pSrc[step_s + jFixed + 1] * inv_i*inv_j + pSrc[step_s + jFixed - 3 + 1] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 1] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed - 3 + 1] * ifl*jfl);
				pDst[step_d + k + 2] = pixProtect(pSrc[step_s + jFixed + 2] * inv_i*inv_j + pSrc[step_s + jFixed - 3 + 2] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 2] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed - 3 + 2] * ifl*jfl); 

			}
			if (iFixed==height-1)
			{
				pDst[step_d + k + 0] = pixProtect( pSrc[step_s + jFixed] * inv_i*inv_j + pSrc[step_s + jFixed + 3] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed + 3] * ifl*jfl);
				pDst[step_d + k + 1] = pixProtect(pSrc[step_s + jFixed + 1] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 1] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed + 1] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed + 3 + 1] * ifl*jfl); 
				pDst[step_d + k + 2] = pixProtect(pSrc[step_s + jFixed + 2] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 2] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed + 2] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed + 3 + 2] * ifl*jfl); 

			}
			if ((jFixed == (width - 1) * 3) && (iFixed == height - 1))
			{
				pDst[step_d + k + 0] = pixProtect(pSrc[step_s + jFixed] * inv_i*inv_j + pSrc[step_s + jFixed - 3] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed - 3] * ifl*jfl);
				pDst[step_d + k + 1] = pixProtect(pSrc[step_s + jFixed + 1] * inv_i*inv_j + pSrc[step_s + jFixed - 3 + 1] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed + 1] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed - 3 + 1] * ifl*jfl);
				pDst[step_d + k + 2] = pixProtect(pSrc[step_s + jFixed + 2] * inv_i*inv_j + pSrc[step_s + jFixed - 3 + 2] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed + 2] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed - 3 + 2] * ifl*jfl); 

			}


		}

		step_d += WIDTH_GRAB * 3;
	}



	return;
}

static void imgRectLinear(unsigned char* pSrc, int width, int height, Alg_rect rect, unsigned char* pDst)
{
#ifdef TEST
	Mat matRsz;
	Mat matImg(height, width, CV_8UC3, pSrc);
	resize(matImg, matRsz, Size(WIDTH_GRAB, HEIGHT_GRAB));
	Mat matImg0(HEIGHT_GRAB, WIDTH_GRAB, CV_8UC3, pDst);
#endif

	int i, j, k, step_s, step_d, step;
	int iFixed, jFixed;
	float ifl, inv_i, jfl, inv_j;
	float scale_x, scale_y;

	int widthRect = rect.width;
	int heightRect = rect.height;
	
	scale_x = (float)widthRect / (float)WIDTH_GRAB;
	scale_y = (float)heightRect / (float)HEIGHT_GRAB;

	for (i = 0, step_d = 0; i < HEIGHT_GRAB; i++)
	{

		ifl = (float)((i + 0.5)*scale_y - 0.5);
		iFixed = ifl;
		ifl = ifl - iFixed;
		inv_i = 1.f - ifl;

		step_s = (iFixed + rect.y)*(width * 3);

		for (j = 0, k = 0; j < WIDTH_GRAB; j++, k += 3)
		{

			jfl = (float)((j + 0.5)*scale_x - 0.5);
			jFixed = jfl;
			jfl = jfl - jFixed;
			inv_j = 1.f - jfl;
			jFixed = (jFixed + rect.x) * 3;

			pDst[step_d + k + 0] = pixProtect(pSrc[step_s + jFixed] * inv_i*inv_j + pSrc[step_s + jFixed + 3] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed + 3] * ifl*jfl);
			pDst[step_d + k + 1] = pixProtect(pSrc[step_s + jFixed + 1] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 1] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 1] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed + 3 + 1] * ifl*jfl);
			pDst[step_d + k + 2] = pixProtect(pSrc[step_s + jFixed + 2] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 2] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 2] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed + 3 + 2] * ifl*jfl);

			//if (jFixed == (widthRect - 1) * 3)
			//{
			//	pDst[step_d + k + 0] = pixProtect(pSrc[step_s + jFixed] * inv_i*inv_j + pSrc[step_s + jFixed - 3] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed - 3] * ifl*jfl);
			//	pDst[step_d + k + 1] = pixProtect(pSrc[step_s + jFixed + 1] * inv_i*inv_j + pSrc[step_s + jFixed - 3 + 1] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 1] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed - 3 + 1] * ifl*jfl);
			//	pDst[step_d + k + 2] = pixProtect(pSrc[step_s + jFixed + 2] * inv_i*inv_j + pSrc[step_s + jFixed - 3 + 2] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 2] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed - 3 + 2] * ifl*jfl);

			//}
			//if (iFixed == heightRect - 1)
			//{
			//	pDst[step_d + k + 0] = pixProtect(pSrc[step_s + jFixed] * inv_i*inv_j + pSrc[step_s + jFixed + 3] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed + 3] * ifl*jfl);
			//	pDst[step_d + k + 1] = pixProtect(pSrc[step_s + jFixed + 1] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 1] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed + 1] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed + 3 + 1] * ifl*jfl);
			//	pDst[step_d + k + 2] = pixProtect(pSrc[step_s + jFixed + 2] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 2] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed + 2] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed + 3 + 2] * ifl*jfl);

			//}
			//if ((jFixed == (widthRect - 1) * 3) && (iFixed == heightRect - 1))
			//{
			//	pDst[step_d + k + 0] = pixProtect(pSrc[step_s + jFixed] * inv_i*inv_j + pSrc[step_s + jFixed - 3] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed - 3] * ifl*jfl);
			//	pDst[step_d + k + 1] = pixProtect(pSrc[step_s + jFixed + 1] * inv_i*inv_j + pSrc[step_s + jFixed - 3 + 1] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed + 1] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed - 3 + 1] * ifl*jfl);
			//	pDst[step_d + k + 2] = pixProtect(pSrc[step_s + jFixed + 2] * inv_i*inv_j + pSrc[step_s + jFixed - 3 + 2] * inv_i*jfl + pSrc[step_s - width * 3 + jFixed + 2] * ifl*inv_j + pSrc[step_s - width * 3 + jFixed - 3 + 2] * ifl*jfl);

			//}


		}

		step_d += WIDTH_GRAB * 3;
	}



	return;
}



static void imgBilinear2(unsigned char* pSrc, int width, int height, unsigned char* pDst, int widthD, int heightD)
{
	int i, j, k, step_s, step_d, step;
	int iFixed, jFixed;
	float ifl, inv_i, jfl, inv_j;
	float scale_x, scale_y;

	scale_x = (float)width / (float)widthD;
	scale_y = (float)height / (float)heightD;

	for (i = 0, step_d = 0; i < heightD; i++)
	{
		ifl = i*scale_y;
		iFixed = ifl;

		ifl = ifl - iFixed;
		inv_i = 1.f - ifl;

		step_s = iFixed*(width /** 3*/);

		for (j = 0, k = 0; j < widthD; j++, /*k += 3*/k++)
		{
			jfl = j*scale_x;
			jFixed = jfl;

			jfl = jfl - jFixed;
			inv_j = 1.f - jfl;

			//jFixed = jFixed * 3;

			pDst[step_d + k + 0] = pSrc[step_s + jFixed] * inv_i*inv_j + pSrc[step_s + jFixed + /*3*/1] * inv_i*jfl + pSrc[step_s + width /** 3*/ + jFixed] * ifl*inv_j + pSrc[step_s + width /** 3*/ + jFixed + 1/*3*/] * ifl*jfl;
			//pDst[step_d + k + 1] = pSrc[step_s + jFixed + 1] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 1] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 1] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed + 3 + 1] * ifl*jfl;
			//pDst[step_d + k + 2] = pSrc[step_s + jFixed + 2] * inv_i*inv_j + pSrc[step_s + jFixed + 3 + 2] * inv_i*jfl + pSrc[step_s + width * 3 + jFixed + 2] * ifl*inv_j + pSrc[step_s + width * 3 + jFixed + 3 + 2] * ifl*jfl;
		}

		step_d += widthD/* * 3*/;
	}



	return;
}





