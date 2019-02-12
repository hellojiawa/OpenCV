#include "AlgKmeans.h"
#include "AlgStructDef.h"
#include <algorithm>

using namespace std;


#ifndef DBL_MAX
#define DBL_MAX         1.7976931348623158e+308 
#endif

static float m_fTmp[WIDTH_ALY*HEIGHT_ALY * DIMS];
static double m_dfTmp[WIDTH_ALY*HEIGHT_ALY];
static int m_tmplabels[WIDTH_ALY*HEIGHT_ALY];

static float normL2Sqr(const float* a, const float* b, int n);


////////////////////////////////////////// AlgRNG ////////////////////////////////////////////

#define RNG_COEFF 4164903690U

class  AlgRNG2
{
public:
	enum { UNIFORM = 0, NORMAL = 1 };

	AlgRNG2();
	AlgRNG2(unsigned long long state);
	//! updates the state and returns the next 32-bit unsigned integer random number
	unsigned next();

	unsigned operator ()(unsigned N);
	unsigned operator ()();
	operator int();
	operator unsigned();
	operator float();
	operator double();
	//! returns uniformly distributed integer random number from [a,b) range
	int uniform(int a, int b);
	//! returns uniformly distributed floating-point random number from [a,b) range
	float uniform(float a, float b);
	//! returns uniformly distributed double-precision floating-point random number from [a,b) range
	double uniform(double a, double b);

	unsigned long long state;
};
inline AlgRNG2& theAlgRNG()
{
	AlgRNG2*rng = new AlgRNG2;

	return *rng;
}

inline AlgRNG2::AlgRNG2() { state = 0xffffffff; }
inline AlgRNG2::operator unsigned() { return next(); }
inline AlgRNG2::AlgRNG2(unsigned long long _state) { state = _state ? _state : 0xffffffff; }
inline unsigned AlgRNG2::next()
{
	state = (unsigned long long)(unsigned)state*RNG_COEFF + (unsigned)(state >> 32);
	return (unsigned)state;
}


inline unsigned AlgRNG2::operator ()(unsigned N) { return (unsigned)uniform(0, N); }
inline unsigned AlgRNG2::operator ()() { return next(); }
inline AlgRNG2::operator int() { return (int)next(); }
// * (2^32-1)^-1
inline AlgRNG2::operator float() { return next()*2.3283064365386962890625e-10f; }
inline AlgRNG2::operator double()
{
	unsigned t = next();
	return (((unsigned long long)t << 32) | next())*5.4210108624275221700372640043497e-20;
}

inline int AlgRNG2::uniform(int a, int b) { return a == b ? a : (int)(next() % (b - a) + a); }
inline float AlgRNG2::uniform(float a, float b) { return ((float)*this)*(b - a) + a; }




////////////////////////////////////////// kmeans ////////////////////////////////////////////

//static void generateRandomCenter(const vector<Vec2f>& box, float* center, RNG& rng)
static void generateRandomCenter(const ST_vec2f* box, int size, float* center, AlgRNG2& rng)
{
	float margin = 1.f / size;
	for (int j = 0; j < size; j++)
		center[j] = ((float)rng*(1.f + margin*2.f) - margin)*(box[j].v[1] - box[j].v[0]) + box[j].v[0];
}


/*
k-means center initialization using the following algorithm:
Arthur & Vassilvitskii (2007) k-means++: The Advantages of Careful Seeding
*/
static void generateCentersPP(float* pData, int n, float* _out_centers, int K, AlgRNG2& rng, int trials)
{
	int i, j, k, dims = 3;
	int centers[5];;
	float* dist = &m_fTmp[0], *tdist = dist + n, *tdist2 = tdist + n;
	double sum0 = 0;

	centers[0] = (unsigned)rng % n;

	for (i = 0; i < n; i++)
	{
		dist[i] = normL2Sqr(pData + dims*i, pData + dims*centers[0], dims);
		sum0 += dist[i];
	}

	for (k = 1; k < K; k++)
	{
		double bestSum = DBL_MAX;
		int bestCenter = -1;

		for (j = 0; j < trials; j++)
		{
			double p = (double)rng*sum0, s = 0;
			for (i = 0; i < n - 1; i++)
			if ((p -= dist[i]) <= 0)
				break;
			int ci = i;

			for (i = 0; i < n; i++)
			{
				tdist2[i] = std::min(normL2Sqr(pData + dims*i, pData + dims*ci, dims), dist[i]);
			}
			
			for (i = 0; i < n; i++)
			{
				s += tdist2[i];
			}

			if (s < bestSum)
			{
				bestSum = s;
				bestCenter = ci;
				std::swap(tdist, tdist2);
			}
		}
		centers[k] = bestCenter;
		sum0 = bestSum;
		std::swap(dist, tdist);
	}

	for (k = 0; k < K; k++)
	{
		const float* src = pData + dims*centers[k];
		float* dst = _out_centers + k*dims;
		for (j = 0; j < dims; j++)
			dst[j] = src[j];
	}

	return;
}



double kmeans(float* data, int n, int K, int* bestLabels, int attempts, int flags)
{
	int N = n;

	attempts = std::max(attempts, 1);
	
	int* labels = m_tmplabels;

	float centers[N_GAUSS*DIMS];
	float old_centers[N_GAUSS*DIMS];
	float temp[N_GAUSS*DIMS];
	
	int counters[N_GAUSS];
	ST_vec2f _box[3];
	ST_vec2f* box = &_box[0];
	double best_compactness = DBL_MAX, compactness = 0;
	AlgRNG2& rng = theAlgRNG();
	int a, iter, i, j, k;


	const float* sample = data;
	for (j = 0; j < DIMS; j++)
	{
		box[j].v[0] = sample[j];
		box[j].v[1] = sample[j];
	}
		
	for (i = 1; i < N; i++)
	{
		sample = data + i*DIMS;
		for (j = 0; j < DIMS; j++)
		{
			float v = sample[j];
			box[j].v[0] = std::min(box[j].v[0], v);
			box[j].v[1] = std::max(box[j].v[1], v);
		}
	}

	for (a = 0; a < attempts; a++)
	{
		double max_center_shift = DBL_MAX;
		for (iter = 0;;)
		{
			//swap(centers, old_centers);
			memcpy(temp, centers, sizeof(centers));
			memcpy(centers, old_centers, sizeof(centers));
			memcpy(old_centers, temp, sizeof(old_centers));

			if (iter == 0 && (a > 0 || !(flags & 1)))
			{
				//static void generateCentersPP(float* pData, int n, float* _out_centers, int K, RNG& rng, int trials)
				if (flags & 2)
					generateCentersPP(data, n, centers, K, rng, 3);
				else
				{
					for (k = 0; k < K; k++)
						generateRandomCenter(_box, 3, centers + k*DIMS, rng);
				}
			}
			else
			{

				// compute centers;
				memset(centers, 0, sizeof(centers));
				for (k = 0; k < K; k++)
					counters[k] = 0;

				for (i = 0; i < N; i++)
				{
					sample = data + i*DIMS;
					k = labels[i];
					float* center = centers + k*DIMS;
					j = 0;

					for (; j < DIMS; j++)
						center[j] += sample[j];
					counters[k]++;
				}

				if (iter > 0)
					max_center_shift = 0;

				for (k = 0; k < K; k++)
				{
					if (counters[k] != 0)
						continue;

					// if some cluster appeared to be empty then:
					//   1. find the biggest cluster
					//   2. find the farthest from the center point in the biggest cluster
					//   3. exclude the farthest point from the biggest cluster and form a new 1-point cluster.
					int max_k = 0;
					for (int k1 = 1; k1 < K; k1++)
					{
						if (counters[max_k] < counters[k1])
							max_k = k1;
					}

					double max_dist = 0;
					int farthest_i = -1;
					float* new_center = centers + k*DIMS;
					float* old_center = centers + max_k*DIMS;
					float* _old_center = temp; // normalized
					float scale = 1.f / counters[max_k];
					for (j = 0; j < DIMS; j++)
						_old_center[j] = old_center[j] * scale;

					for (i = 0; i < N; i++)
					{
						if (labels[i] != max_k)
							continue;
						sample = data + i*DIMS;
						double dist = normL2Sqr(sample, _old_center, DIMS);

						if (max_dist <= dist)
						{
							max_dist = dist;
							farthest_i = i;
						}
					}

					counters[max_k]--;
					counters[k]++;
					labels[farthest_i] = k;
					sample = data + farthest_i*DIMS;

					for (j = 0; j < DIMS; j++)
					{
						old_center[j] -= sample[j];
						new_center[j] += sample[j];
					}
				}

				for (k = 0; k < K; k++)
				{
					float* center = centers + k*DIMS;

					float scale = 1.f / counters[k];
					for (j = 0; j < DIMS; j++)
						center[j] *= scale;

					if (iter > 0)
					{
						double dist = 0;
						const float* old_center = old_centers + k*DIMS;
						for (j = 0; j < DIMS; j++)
						{
							double t = center[j] - old_center[j];
							dist += t*t;
						}
						max_center_shift = std::max(max_center_shift, dist);
					}
				}
			}

			if (++iter == 10 || max_center_shift <= 1.192092896e-07F)
			{
				break;
			}

			// assign labels
			//Mat dists(1, N, CV_64F);
			double* dist = m_dfTmp;// dists.ptr<double>(0);

			for (int i = 0; i < N; ++i)
			{
				sample = data + i*DIMS;
				int k_best = 0;
				double min_dist = DBL_MAX;

				for (int k = 0; k < K; k++)
				{
					const float* center = centers + k*DIMS;
					const double dist = normL2Sqr(sample, center, DIMS);

					if (min_dist > dist)
					{
						min_dist = dist;
						k_best = k;
					}
				}

				dist[i] = min_dist;
				labels[i] = k_best;
			}

			compactness = 0;
			for (i = 0; i < N; i++)
			{
				compactness += dist[i];
			}
		}

		if (compactness < best_compactness)
		{
			best_compactness = compactness;
			//if (_centers.needed())
			//	centers.copyTo(_centers);
			//_labels.copyTo(best_labels);

			memcpy(bestLabels, labels, N*sizeof(int));
		}
	}

	return best_compactness;
}


static float normL2Sqr(const float* a, const float* b, int n)
{
	int j = 0; float d = 0.f;

	for (; j <= n - 4; j += 4)
	{
		float t0 = a[j] - b[j], t1 = a[j + 1] - b[j + 1], t2 = a[j + 2] - b[j + 2], t3 = a[j + 3] - b[j + 3];
		d += t0*t0 + t1*t1 + t2*t2 + t3*t3;
	}

	for (; j < n; j++)
	{
		float t = a[j] - b[j];
		d += t*t;
	}
	return d;
}


