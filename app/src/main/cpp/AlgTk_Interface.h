#ifndef	AlgTK_ALGTK_PROCESS_H
#define AlgTK_ALGTK_PROCESS_H

//#include <stdint.h>
#include "AlgTk_StructDef.h"



#ifdef __cplusplus
extern "C"{
#endif


	/*********************************************************************************************/

	//enum
	//{
	//	ALGTK_IMAGE_FORMATE_YUV = 1,
	//	ALGTK_IMAGE_FORMATE_RGB,
	//	ALGTK_IMAGE_FORMATE_GRAY
	//};




	/*********************************************************************************************/

#define IN	
#define OUT

	typedef struct
	{
		////内存外部分配
		IN char* pMem; //7M
		IN int szMem;

		IN unsigned char* src_phy;//数据首地址RGB
		OUT unsigned char* dst_phy;//没用
		IN int width;
		IN int height;
		IN int mode;//第一帧为0，以后置为1
		IN Alg_rect rtIn;//初始化跟踪框
		IN Alg_rect rtGrab;//初始化跟踪框
		
		OUT Alg_rect pRtOut;//输出的框
		OUT int version;

		OUT short state;//跟踪状态,取值0、1、2，0表示跟踪未开始，1表示跟踪进行中，2表示跟踪结束
		OUT short bValid;//取值1或0，表示结果有效或者无效
		OUT short detaX;//[-1024, 1024]，正表示向右飞
		OUT short detaY;//[-1024, 1024]，正表示向下飞
		OUT short detaZ;//[-1024, 1024]，正表示后退

		OUT short bRequest;//取值0或1，表示请求人工确认是否继续跟踪
		IN  short bAnswer;//取值0或1， 表示人工确认是否继续跟踪，如果回答为0或者一段时间内没有答复，跟踪立即停止		

	}AlgTk_param;



	/*********************************************************************************************/

	//返回1表示成功，返回0表示跟踪失败，返回-1表示未初始化或者算法内存分配失败
	int AlgTk_process(void* pParam);

	/*********************************************************************************************/

#ifdef __cplusplus
}
#endif





#endif



