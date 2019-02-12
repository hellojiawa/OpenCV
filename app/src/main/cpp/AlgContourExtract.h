#ifndef ALG_CONTURE_EXTRACT_H
#define ALG_CONTURE_EXTRACT_H


#include "AlgTk_StructDef.h"




#ifdef __cplusplus
extern "C" {
#endif


	//typedef struct
	//{
	//	int x;		//左上角坐标
	//	int y;
	//	int x2;		//右下角坐标
	//	int y2;
	//}Alg_ptRect;


	/*********************************************************************************************
	\函数名: 算法创建
	\功能:
	\返回值: 返回算法创建的句柄，失败，则返回空
	**********************************************************************************************/

	void* AlgContourExtract_Create();


	/**********************************************************************************************
	\函数名: 算法执行
	\功能:
	\参数	 handle		: 算法句柄
	pBuf		: 图像数据首地址，以图像左上角为起点，分辨率不超过1920*1080
	width		: 图像宽
	height		: 图像高
	rect		: 输入的目标框
	(*_rtOut)	: 输出的目标框
	\返回值: 0表示计算失败；
	1计算成功；
	***********************************************************************************************/

	int AlgContourExtract_GrabCut(void* handle, unsigned char* pBuf, int width, int height, Alg_rect rect, Alg_rect* _rtOut, int iterCount);


	/*********************************************************************************************
	\函数名: 算法销毁
	\功能:
	\返回值: 返回1
	失败，则返回其他
	**********************************************************************************************/
	int AlgContourExtract_Destroy(void** hhandle);



#ifdef __cplusplus
}
#endif











#endif





















