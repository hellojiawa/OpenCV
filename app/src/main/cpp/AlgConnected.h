#ifndef Alg_CONNECTED_H
#define Alg_CONNECTED_H






#ifdef __cplusplus
extern "C" {
#endif



#include "AlgStructDef.h"




/*********************************************************************************************/



/*********************************************************************************************/



typedef struct
{
	unsigned char *pucMarkedMask;//标记过的前景掩码
	Alg_fgRectSeq *pfgRectSeq;//前景序列
}Alg_Connected_Res;

/*********************************************************************************************
\函数名: 算法创建
\功能:
\参数:
\返回值: 成功，返回算法创建的句柄
			失败，则返回空
**********************************************************************************************/




void *Alg_Connected_create(void);





/**********************************************************************************************
\函数名: 算法执行
\功能:
\参数	 handle : 算法句柄
	     pFgd   : 前景图数据首地址
	     width   : 图像宽
	     height  : 图像高
\返回值: 成功，返回0
			失败，则返回非0
***********************************************************************************************/




int Alg_Connected_process(void *_handle, 
					const unsigned char *pFgd,int iWidth, int iHeight);







/*********************************************************************************************
\函数名: 算法结果获取
\功能:
\参数:
\返回值:
**********************************************************************************************/




int Alg_Connected_getResult(void *_handle,Alg_Connected_Res *pRes);






/*********************************************************************************************
\函数名: 算法销毁
\功能:
\参数:
\返回值: 成功，返回0
			失败，则返回非0
**********************************************************************************************/





int Alg_Connected_destroy(void **hhandle);



#ifdef __cplusplus
}
#endif





#endif
