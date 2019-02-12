#ifndef _ALGTK_BASE_STRUCT_H_
#define _ALGTK_BASE_STRUCT_H_

#include "AlgTk_StructDef.h"


#define WIDTH_ALY	/*(240)*//*(280)*//*(320)*//*(210)*/(176)/*(176)*//*(144)*//*(144)*/
#define HEIGHT_ALY	/*(180)*//*(210)*//*(240)*//*(168)*/(144)/*(144)*//*(108)*//*(84)*/


#define FERNS_N_STRUCTS	(10)		//10颗树
#define FERNS_SIZE_STRUCT (13)		//每棵树13个节点


#ifndef NULL
#define NULL	(0)
#endif


#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)            (((a) < (b)) ? (b) : (a))
#endif



#define ALG_PI   3.1415926535897932384626433832795

typedef unsigned char	 uchar;
typedef unsigned short	ushort;
typedef char signed schar;
//typedef unsigned __int64 uint64;

#ifdef __cplusplus
extern "C"{
#endif

typedef struct
{
	int width;
	int height;
}ST_size;


typedef Alg_rect ST_rect;


typedef struct
{
	int x;
	int y;
}ST_point;


typedef struct
{
	float x;
	float y;
}ST_point2f;


typedef struct
{
	float x;
	float y;
	float width;
	float height;
	unsigned short area;
}Alg_fRect;




typedef struct
{
	uchar x;
	uchar y;
	uchar w;
	uchar h;
}ST_ucRect;


typedef struct
{
	uchar x;
	uchar y;
}ST_ucXY;


typedef struct
{
	uchar w;
	uchar h;
}ST_ucWH;




#define ALG_PATCH_W (12)
#define ALG_PATCH_H (16)

typedef struct
{
	float mean;
	unsigned char pData[ALG_PATCH_W*ALG_PATCH_H];
}ST_mat;


//////////////////////////////////


#define FERNS_N_STRUCTS	(10)

typedef struct
{	
	ushort fern[FERNS_N_STRUCTS];
	ushort label;
}ST_fern;


#define MAX_CAPACITY_FERNS_PX	(200)
#define MAX_CAPACITY_FERNS_NX	(20000 + 5000)
#define MAX_CAPACITY_NN_EX		(100)


#define MAX_CAPACITY_NN_EX	(100)



#define MAX_CAPACITY_VLD_BOX		(200)
#define MAX_CAPACITY_GRID_INIT		(50000/*32000*/)
#define MAX_CAPACITY_SCALES_INIT	(30)
#define MAX_CAPACITY_GOODBOX_INIT	(500)
#define MAX_CAPACITY_BADBOX_INIT	(/*500*/MAX_CAPACITY_GRID_INIT)



////////////////////////////////////////////////////////////////////

#define MAX_CAPACITY_TEMP_DT_INIT	(MAX_CAPACITY_GRID_INIT)

typedef struct
{
	int n;
	ST_fern patt[MAX_CAPACITY_TEMP_DT_INIT];//特征值
	float conf[MAX_CAPACITY_TEMP_DT_INIT]; //特征值对应的后验概率累加值
}ST_tempDt;//通过方差过滤的结果



///////////////

#define MAX_CAPACITY_DET_INIT	(32)


typedef struct
{
	ushort n;
	ushort bb[MAX_CAPACITY_DET_INIT];	//在Grid中的索引
	//ST_fern patt[MAX_CAPACITY_DET_INIT];
	//float conf1[MAX_CAPACITY_DET_INIT];//相关相似度
	//float conf2[MAX_CAPACITY_DET_INIT];//保守相似度
	//ST_isin isin[MAX_CAPACITY_DET_INIT];
	ST_mat patch[MAX_CAPACITY_DET_INIT];
}ST_det;//通过集合分类器过滤的结果



///////////////////////////////////////////////////////////////////////

typedef struct
{
	int noise_init;
	float angle_init;
	float shift_init;
	float scale_init;
}ST_AffineParam;



#define WIDTH_INTER	(WIDTH_ALY + 4)
#define HEIGHT_INTER (HEIGHT_ALY + 4)

typedef struct
{
	float  iisum[WIDTH_INTER*HEIGHT_INTER];
	float iisqsum[WIDTH_INTER*HEIGHT_INTER];
	float var;	//best_box方差关联项，做为阈值  
	float mea;
}ST_Integral;


typedef struct
{
	int nFrsPosX;
	ST_fern frsPosXArr[MAX_CAPACITY_FERNS_PX];

	ST_mat posEx;  //positive NN example    一个, 正样本
	int nNNEx;
	ST_mat nnExArr[MAX_CAPACITY_NN_EX];
}ST_TrainData;



//全是负样本?
typedef struct
{
	int nFrsNegXT;
	ST_fern frsNegXTArr[MAX_CAPACITY_FERNS_NX >> 1];
	int nNegNNExT;
	ST_mat negNNExTArr[MAX_CAPACITY_NN_EX >> 1];
}ST_TestData;


typedef struct
{
	//int nGrid;
	//ST_BoundingBox gridArr[MAX_CAPACITY_GRID_INIT];

	int nScales;
	ST_size scalesAll[MAX_CAPACITY_SCALES_INIT];

	ushort nGoodBox;
	ushort goodBoxesArr[MAX_CAPACITY_GOODBOX_INIT];

	ushort nBadBox;
	ushort badBoxesArr[MAX_CAPACITY_BADBOX_INIT];
}ST_GridData;



/////////////////////////////////////////////////////////////////////


void _integral(const unsigned char* src, unsigned int _srcfloatep, float* sum, unsigned int _sumfloatep,
	float* sqsum, unsigned int _sqsumfloatep,
	int width, int height);


void RspImgLinear(const unsigned char* pbSrc, const ST_size szSrc,
	int nBpp, const ST_size szDst, unsigned char* pbDst, int exc);


void matchTemplate(unsigned char* pImg1, unsigned char* pImg2, int width, int height, float* _ncc);




///////////////////////////////////////////////////////////////////////

enum{ AlgTK_SMP_CAP_INIT = 64};


//目标轨迹定义，队列
typedef struct
{
	int num;
	int head;
	int tail;
	ST_mat arr[AlgTK_SMP_CAP_INIT];
}AlgTK_SMP;



int AlgTk_SMP_init(AlgTK_SMP *smpArr);
int AlgTk_SMP_add(AlgTK_SMP *smpArr, ST_mat smp);
ST_mat *AlgTk_SMP_getCell(AlgTK_SMP *smpArr, int i);
int AlgTk_SMP_clear(AlgTK_SMP *smpArr);

//////////////////////////////////////////




#ifdef __cplusplus
}
#endif



#endif

















