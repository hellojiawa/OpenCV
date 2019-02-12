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
		////�ڴ��ⲿ����
		IN char* pMem; //7M
		IN int szMem;

		IN unsigned char* src_phy;//�����׵�ַRGB
		OUT unsigned char* dst_phy;//û��
		IN int width;
		IN int height;
		IN int mode;//��һ֡Ϊ0���Ժ���Ϊ1
		IN Alg_rect rtIn;//��ʼ�����ٿ�
		IN Alg_rect rtGrab;//��ʼ�����ٿ�
		
		OUT Alg_rect pRtOut;//����Ŀ�
		OUT int version;

		OUT short state;//����״̬,ȡֵ0��1��2��0��ʾ����δ��ʼ��1��ʾ���ٽ����У�2��ʾ���ٽ���
		OUT short bValid;//ȡֵ1��0����ʾ�����Ч������Ч
		OUT short detaX;//[-1024, 1024]������ʾ���ҷ�
		OUT short detaY;//[-1024, 1024]������ʾ���·�
		OUT short detaZ;//[-1024, 1024]������ʾ����

		OUT short bRequest;//ȡֵ0��1����ʾ�����˹�ȷ���Ƿ��������
		IN  short bAnswer;//ȡֵ0��1�� ��ʾ�˹�ȷ���Ƿ�������٣�����ش�Ϊ0����һ��ʱ����û�д𸴣���������ֹͣ		

	}AlgTk_param;



	/*********************************************************************************************/

	//����1��ʾ�ɹ�������0��ʾ����ʧ�ܣ�����-1��ʾδ��ʼ�������㷨�ڴ����ʧ��
	int AlgTk_process(void* pParam);

	/*********************************************************************************************/

#ifdef __cplusplus
}
#endif





#endif



