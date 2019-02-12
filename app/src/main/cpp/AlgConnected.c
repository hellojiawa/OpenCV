//#include <windows.h>
#include <stdlib.h>
//#include <assert.h>
#include <malloc.h>
#include <string.h>
//#include <string.h>
//#include <math.h>
#include "AlgConnected.h"
//#include "Alg_StructDef.h"



#ifndef NULL
#define NULL	(0)
#endif


#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)            (((a) < (b)) ? (b) : (a))
#endif



#define Max_EqualMark_Length    (NUM_PXL_ALS >> 2)  //�ȼ۱����ĸ�����
#define Max_LableNum_OneMerger (32)   //�ȼ۶Ժϲ���һ����ͨ�������32�����
#define Max_Mergers_Num (64)
#define Max_LableRcet_Num (256)
#define OUT
#define IN
#define FD_VALUE (255)
#define BK_VALUE (0)
//һ����ʱ�ȼ۱�ǽṹ��
typedef struct _EqualMark
{    
	short asLable[4];        //һ���ȼ����ĸ����ֵ���ֱ�����ߣ����ϣ��ϣ����ϵı��ֵ
	short IsProcess;
} T_EqualMark;

//�ȼ۱��˳��洢����
typedef struct _EqualMarkList
{
	short sNum;
	T_EqualMark atEqlMrk[Max_EqualMark_Length];//
}T_EqualMarkList;


typedef struct _OneMergerList
{
	short sNum;//һ���ȼ۶Ժϲ��������б�ǵĸ���
	short aiOneMerger[Max_LableNum_OneMerger];//һ���ȼ۶Ժϲ���,�������������Ǹ���Ϊ32
}T_OneMergerList;

static int addLableToOneMrgr(T_OneMergerList *ptOneMrgrList, int iLable);

static int addEqMarkToOneMrgr(T_EqualMark *ptEqualMark, T_OneMergerList *ptOneMrgrList);

typedef struct _MergersList
{
	short sNum;//�ϲ������
	T_OneMergerList atMergers[Max_Mergers_Num];//�洢�ϲ���ĵȼ۶����飬���64���ϲ���
}T__MergersList;


typedef struct _LableRect
{
	short sPixels;//��ǵ����ظ���
	short sIsValid;//�ñ�Ǿ�����Ч��־��0Ϊ��Ч��-1Ϊ��Ч
	short sMinX;
	short sMinY;
	short sMaxX;
	short sMaxY;
}T_LableRect;

typedef struct
{
	short sIsCreate;
	T_EqualMarkList tEqualMarks;//�ȼ۶Դ洢����
	T__MergersList tMergersList;//�ȼ۶Թ鲢���ս��
	T_LableRect atLableRects[Max_LableRcet_Num];

	unsigned char *pucMarkedMask;//��ǹ���ǰ������,��Ϊ����ǽ��
	Alg_fgRectSeq *pfgRectSeq; //ǰ������
}Alg_Connected_handle;


//�ϲ��ȼ۶�
static int mergerEqualMark(IN T_EqualMarkList *ptEqMrkList, OUT T__MergersList *ptMergersList);

//���ݵȼ۶Ժϲ�����Ա�Ǿ���ϲ���ͬʱ�õ�����ǽ��
static int LableRectmerger(IN  T__MergersList *ptMergersList, int iWidth, int iHeight, OUT T_LableRect *ptLableRect, OUT unsigned char *pucMarkedMask);

//���������ɵı�Ǿ���ϲ��������ǰ����������
static int bornRectSeq(IN T_LableRect *ptLableRect, IN int iWid, IN int iHei, OUT Alg_fgRectSeq *pfgRectSeq);

static int modifyRect(T_LableRect *ptLableRect, int sFrstLable, int sCurLable, int iWidth, int iHeight, unsigned char *pucMarkedMask);


void * Alg_Connected_create( void )
{
	//Alg_Connected_handle *handle	= (Alg_Connected_handle *)_aligned_malloc(sizeof(Alg_Connected_handle),32);
	Alg_Connected_handle *handle = (Alg_Connected_handle *)malloc(sizeof(Alg_Connected_handle));

	if (NULL == handle)
	{
		return NULL;
	}

	memset(handle, 0, sizeof(Alg_Connected_handle));

	//handle->pucMarkedMask = (unsigned char *)_aligned_malloc(NUM_PXL_ALS*sizeof(unsigned char),32);
	handle->pucMarkedMask = (unsigned char *)malloc(NUM_PXL_ALS*sizeof(unsigned char));

	if (NULL == handle->pucMarkedMask)
	{
		return NULL;
	}

	memset(handle->pucMarkedMask,0,NUM_PXL_ALS*sizeof(unsigned char));

	//handle->pfgRectSeq = (Alg_fgRectSeq *)_aligned_malloc(sizeof(Alg_fgRectSeq),32);
	handle->pfgRectSeq = (Alg_fgRectSeq *)malloc(sizeof(Alg_fgRectSeq));

	if (NULL == handle->pfgRectSeq)
	{
		return NULL;
	}

	memset(handle->pfgRectSeq,0,sizeof(Alg_fgRectSeq));

	Alg_FRS_init(handle->pfgRectSeq);

	handle->sIsCreate = 1;

	return (void*)handle;
}

int Alg_Connected_destroy( void **hhandle )
{
	Alg_Connected_handle *handle;

	if (NULL == hhandle)
	{
		return -1;
	}

	handle = (Alg_Connected_handle *)(*hhandle);

	if (1 != handle->sIsCreate)
	{
		return -1;
	}

	if (NULL != handle->pucMarkedMask)
	{
		free(handle->pucMarkedMask);
		handle->pucMarkedMask = NULL;
	}

	if (NULL != handle->pfgRectSeq)
	{
		free(handle->pfgRectSeq);
		handle->pfgRectSeq = NULL;
	}

	free(handle);
	(*hhandle)	= NULL;

	return 0;
}

int Alg_Connected_process( void *_handle, const unsigned char *pFgd,int iWidth, int iHeight )
{
	int i,j;
	int FDsteps;
	int FDstepWidth;
	int wid,hei;

	int label = 1;
	int iCurLable = 0;
	int iEqualFlag = 0;//�ȼ۱�Ǵ��ڱ�־
	int iNum = 0;

	const unsigned char *pucCurRow = NULL;//ָ��ǰ��
	const unsigned char *pucCurPixel = NULL;//��ǰ����

	unsigned char *pucCurTmpMarkRow = NULL;//ָ��ǰ��ʱ�����
	unsigned char *pucTopTmpMarkRow = NULL;
	unsigned char *pucCurTmpMark = NULL;//��ǰ���
	unsigned char *pucTopTmpMark = NULL;

	T_EqualMark *ptEqMark = NULL;
	unsigned char *pucMarked = NULL;
	Alg_fgRectSeq *pfgRectSeq = NULL;
	T_LableRect *ptLableRect = NULL;
	int aiFourLable[4];

	Alg_Connected_handle *handle = NULL;

	if (NULL == _handle || NULL == pFgd)
	{
		return -1;
	}

	handle = (Alg_Connected_handle *)_handle;

	//�����һ֡����Ϣ
	memset(&handle->tMergersList,0,sizeof(T__MergersList));
	memset(handle->pfgRectSeq,0,sizeof(Alg_fgRectSeq));
	memset(handle->pucMarkedMask,0,NUM_PXL_ALS*sizeof(unsigned char));
	handle->tEqualMarks.sNum = 0;


	pucMarked = handle->pucMarkedMask;//����ǽ��
	pfgRectSeq = handle->pfgRectSeq;
	ptEqMark = &handle->tEqualMarks.atEqlMrk[0];
	ptLableRect = &handle->atLableRects[0];

	//	tEqualMarkTmp.IsProcess = 0;
	aiFourLable[0] = -1;
	aiFourLable[1] = -1;
	aiFourLable[2] = -1;
	aiFourLable[3] = -1;

	wid = iWidth;
	hei = iHeight;
	FDstepWidth = WIDTHBYTES(wid);

	for (i = 0; i < Max_LableRcet_Num; i++)
	{
		ptLableRect[i].sIsValid = 0;
		ptLableRect[i].sMaxX = -1;
		ptLableRect[i].sMaxY = -1;
		ptLableRect[i].sMinX = 10000;
		ptLableRect[i].sMinY = 10000;
		ptLableRect[i].sPixels = 0;
	}

	for (i = 1, FDsteps = FDstepWidth; i < hei && label <= Max_LableRcet_Num; i++, FDsteps += FDstepWidth)
	{
		pucCurRow = pFgd + FDsteps;
		pucCurTmpMarkRow = pucMarked + FDsteps;
		pucTopTmpMarkRow = pucCurTmpMarkRow - FDstepWidth;
		for (j = 1; j < wid - 1 && label <= Max_LableRcet_Num; j++)
		{
			pucCurPixel = pucCurRow + j;
			pucCurTmpMark = pucCurTmpMarkRow + j;
			pucTopTmpMark = pucTopTmpMarkRow + j;
//			*pucCurTmpMark = 0;
			if (0 != *pucCurPixel)
			{
				aiFourLable[0] = *(pucCurTmpMark - 1);
				aiFourLable[1] = *(pucTopTmpMark - 1);
				aiFourLable[2] = *(pucTopTmpMark);
				aiFourLable[3] = *(pucTopTmpMark + 1);

				iCurLable = (aiFourLable[3] != 0) ? aiFourLable[3] : 0;
				iCurLable = (aiFourLable[2] != 0) ? aiFourLable[2] : iCurLable;
				iCurLable = (aiFourLable[1] != 0) ? aiFourLable[1] : iCurLable;
				iCurLable = (aiFourLable[0] != 0) ? aiFourLable[0] : iCurLable;

				iEqualFlag = 0;
				iEqualFlag += (aiFourLable[3] != 0 && aiFourLable[3] != iCurLable);
				iEqualFlag += (aiFourLable[2] != 0 && aiFourLable[2] != iCurLable);
				iEqualFlag += (aiFourLable[1] != 0 && aiFourLable[1] != iCurLable);

				if (0 == iCurLable)
				{
					*pucCurTmpMark = label;
					ptLableRect[label - 1].sPixels++;
					ptLableRect[label - 1].sMaxX = max(j, ptLableRect[label - 1].sMaxX);
					ptLableRect[label - 1].sMaxY = max(i, ptLableRect[label - 1].sMaxY);
					ptLableRect[label - 1].sMinX = min(j, ptLableRect[label - 1].sMinX);
					ptLableRect[label - 1].sMinY = min(i, ptLableRect[label - 1].sMinY);
					label++;
				}
				else if (iEqualFlag != 0)
				{
					*pucCurTmpMark = iCurLable;
					iNum = handle->tEqualMarks.sNum;
					ptEqMark[iNum].asLable[0] = aiFourLable[0];
					ptEqMark[iNum].asLable[1] = aiFourLable[1];
					ptEqMark[iNum].asLable[2] = aiFourLable[2];
					ptEqMark[iNum].asLable[3] = aiFourLable[3];
					ptEqMark[iNum].IsProcess = 0;
					handle->tEqualMarks.sNum++;

					ptLableRect[iCurLable - 1].sPixels++;
					ptLableRect[iCurLable - 1].sMaxX = max(j, ptLableRect[iCurLable - 1].sMaxX);
					ptLableRect[iCurLable - 1].sMaxY = max(i, ptLableRect[iCurLable - 1].sMaxY);
					ptLableRect[iCurLable - 1].sMinX = min(j, ptLableRect[iCurLable - 1].sMinX);
					ptLableRect[iCurLable - 1].sMinY = min(i, ptLableRect[iCurLable - 1].sMinY);
				}
				else
				{
					*pucCurTmpMark = iCurLable;
					ptLableRect[iCurLable - 1].sPixels++;
					ptLableRect[iCurLable - 1].sMaxX = max(j, ptLableRect[iCurLable - 1].sMaxX);
					ptLableRect[iCurLable - 1].sMaxY = max(i, ptLableRect[iCurLable - 1].sMaxY);
					ptLableRect[iCurLable - 1].sMinX = min(j, ptLableRect[iCurLable - 1].sMinX);
					ptLableRect[iCurLable - 1].sMinY = min(i, ptLableRect[iCurLable - 1].sMinY);
				}
			}



		}
	}



	mergerEqualMark(&handle->tEqualMarks,&handle->tMergersList);//�鲢�ȼ۶�


	LableRectmerger(&handle->tMergersList, iWidth, iHeight, ptLableRect,pucMarked);


	bornRectSeq(ptLableRect,iWidth,iHeight,pfgRectSeq);
	//	bornMarkedMask(&handle->tMergerEqualMarkMat,piMarkMatTmp,wid,hei,pfgRectSeq,pucMarked);

	return 0;
}

int Alg_Connected_getResult(void *_handle,Alg_Connected_Res *pRes)
{
	Alg_Connected_handle *handle = NULL;

	if (NULL == _handle)
	{
		return -1;
	}

	handle = (Alg_Connected_handle *)_handle;

	pRes->pfgRectSeq = handle->pfgRectSeq;
	pRes->pucMarkedMask = handle->pucMarkedMask;

	return 0;
}

static int mergerEqualMark(IN T_EqualMarkList *ptEqMrkList, OUT T__MergersList *ptMergersList)
{
	int i,j,k;
	int iNum = 0;
	int iStartIndex;
	int iIsFind;
	T_OneMergerList *ptOneMerger = NULL;
	T_EqualMark *ptCurEqMark = NULL;

	int iEqlMrkNum = ptEqMrkList->sNum;

	int iSavedLable;

	int iIsEqual;

	k = 0;
	iStartIndex = 0;
	while(1)
	{
		if (0 == iEqlMrkNum)
		{
			break;
		}
		ptOneMerger = &ptMergersList->atMergers[k];
		ptCurEqMark = &ptEqMrkList->atEqlMrk[iStartIndex];
		addEqMarkToOneMrgr(ptCurEqMark,ptOneMerger);
		ptCurEqMark->IsProcess = 1;
		for (i = 0; i < ptOneMerger->sNum; i++)
		{

			iSavedLable = ptOneMerger->aiOneMerger[i];
			for (j = iStartIndex + 1; j < iEqlMrkNum; j++)
			{				
				ptCurEqMark = &ptEqMrkList->atEqlMrk[j];
				if (1 == ptCurEqMark->IsProcess)
				{
					continue;
				}
				iIsEqual = 0;
				iIsEqual = (iSavedLable == ptCurEqMark->asLable[0]) + (iSavedLable == ptCurEqMark->asLable[1]) + 
					(iSavedLable == ptCurEqMark->asLable[2]) + (iSavedLable == ptCurEqMark->asLable[3]);

				if (iIsEqual > 0)
				{
					addEqMarkToOneMrgr(ptCurEqMark,ptOneMerger);
					ptCurEqMark->IsProcess = 1;
				}			
			}
		}
		//���������������û�б�����ĵȼ۶�
		iIsFind = 0;
		for (i = 0; i < iEqlMrkNum; i++)
		{
			if (!ptEqMrkList->atEqlMrk[i].IsProcess)
			{
				iStartIndex = i;
				iIsFind = 1;
				k++;
				break;
			}
		}
		if (!iIsFind)
		{
			ptMergersList->sNum = k + 1;
			break;
		}
	}

	return 0;
}

int LableRectmerger( IN T__MergersList *ptMergersList, int iWidth, int iHeight, OUT T_LableRect *ptLableRect, OUT unsigned char *pucMarkedMask )
{
	int sMrgrsNum;
	int sOneMrgrNum;
	int i,j;
	int sFrstLable;
	int sOthrLable;

	T_OneMergerList *ptOneMrgr = NULL;

	short *psLable = NULL;
	ptOneMrgr = &ptMergersList->atMergers[0];
	sMrgrsNum = ptMergersList->sNum;

	for (i = 0; i < sMrgrsNum; i++)
	{
		sFrstLable = ptOneMrgr[i].aiOneMerger[0];
		sOneMrgrNum = ptOneMrgr[i].sNum;
		psLable = &ptOneMrgr[i].aiOneMerger[0];
		for (j = 1; j < sOneMrgrNum; j++)
		{
			sOthrLable = psLable[j];
			ptLableRect[sOthrLable - 1].sIsValid = -1;
			ptLableRect[sFrstLable - 1].sPixels += ptLableRect[sOthrLable - 1].sPixels;
			ptLableRect[sFrstLable - 1].sMaxX = max(ptLableRect[sFrstLable - 1].sMaxX, ptLableRect[sOthrLable - 1].sMaxX);
			ptLableRect[sFrstLable - 1].sMaxY = max(ptLableRect[sFrstLable - 1].sMaxY, ptLableRect[sOthrLable - 1].sMaxY);
			ptLableRect[sFrstLable - 1].sMinX = min(ptLableRect[sFrstLable - 1].sMinX, ptLableRect[sOthrLable - 1].sMinX);
			ptLableRect[sFrstLable - 1].sMinY = min(ptLableRect[sFrstLable - 1].sMinY, ptLableRect[sOthrLable - 1].sMinY);

			modifyRect(&ptLableRect[sOthrLable - 1], sFrstLable, sOthrLable, iWidth, iHeight, pucMarkedMask);


		}
	}

	return 1;
}

int modifyRect( T_LableRect *ptLableRect, int sFrstLable, int sCurLable, int iWidth, int iHeight, unsigned char *pucMarkedMask )
{
	int i,j;
	short sMinX, sMinY, sMaxX, sMaxY;

	int sStepWidth;
	int sStep;

	unsigned char *pucCurRow = NULL;

	sMinX = ptLableRect->sMinX;
	sMinY = ptLableRect->sMinY;
	sMaxX = ptLableRect->sMaxX;
	sMaxY = ptLableRect->sMaxY;

	sStepWidth = WIDTHBYTES(iWidth);

	for (i = sMinY, sStep = i * sStepWidth; i <= sMaxY; i++, sStep += sStepWidth)
	{
		pucCurRow = pucMarkedMask + sStep;

		for (j = sMinX; j <= sMaxX; j++)
		{
			if (pucCurRow[j] == sCurLable)
			{
				pucCurRow[j] = sFrstLable;
			}
		}
	}

	return 1;
}

int bornRectSeq( IN T_LableRect *ptLableRect, IN int iWid, IN int iHei, OUT Alg_fgRectSeq *pfgRectSeq )
{
	Alg_ObjRect tRect;
	int i,j;

	for (i = 0; i < Max_LableRcet_Num; i++)
	{
		if (0 == ptLableRect[i].sIsValid && ptLableRect[i].sPixels > 0)//Ϊ0��ʾ��Ч
		{
			tRect.height = ptLableRect[i].sMaxY - ptLableRect[i].sMinY + 1;
			tRect.width  = ptLableRect[i].sMaxX - ptLableRect[i].sMinX + 1;
			tRect.pxls	 = ptLableRect[i].sPixels;
			tRect.x = ptLableRect[i].sMinX;
			tRect.y = ptLableRect[i].sMinY;

			Alg_FRS_add(pfgRectSeq, &tRect);
		}
	}

	return 1;
}

int addEqMarkToOneMrgr( T_EqualMark *ptEqualMark, T_OneMergerList *ptOneMrgrList )
{
	int iNum;
	int i,j;
	int iFlag = 0;
	int iIsEqual = 0;
	int iLablePreSave;
	int iLbaleSaved;

	for (i = 0; i < 4; i++)
	{
		iNum = ptOneMrgrList->sNum;
		iLablePreSave = ptEqualMark->asLable[i];
		if (0 == iLablePreSave)
		{
			continue;
		}
		if (0 == iNum)
		{
			addLableToOneMrgr(ptOneMrgrList,iLablePreSave);
			continue;
		}
		iFlag = 0;
		for (j = 0; j < iNum; j++)
		{
			iLbaleSaved = ptOneMrgrList->aiOneMerger[j];
			if (iLablePreSave == iLbaleSaved)
			{
				iFlag = 1;
				break;
			}
		}

		if (!iFlag)
		{
			addLableToOneMrgr(ptOneMrgrList,iLablePreSave);
		}


	}

	return 0;
}

int addLableToOneMrgr( T_OneMergerList *ptOneMrgrList, int iLable )
{
	int iNum;

	if(NULL == ptOneMrgrList)
	{
		return -1;
	}

	iNum = ptOneMrgrList->sNum;

	if(iNum >= Max_LableNum_OneMerger)
	{
		return -1;
	}

	ptOneMrgrList->aiOneMerger[iNum] = iLable;

	ptOneMrgrList->sNum++;
	return 0;
}




