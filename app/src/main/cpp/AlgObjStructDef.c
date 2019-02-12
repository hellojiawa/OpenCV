#include "AlgStructDef.h"
//#include <stdlib.h>
#include <string.h>
//#include <assert.h>
//#include <math.h>













int Alg_FRS_init(Alg_fgRectSeq *pSeq)
{
	//assert(NULL != pSeq);

	pSeq->num = 0;

	return 0;
}






int Alg_FRS_add(Alg_fgRectSeq *pSeq, Alg_ObjRect *pRectS)
{
	int n;

	//assert(NULL != pSeq && NULL != pRectS);

	n = pSeq->num;

	if (n >= Alg_OBJ_MAXNUM_FRSEQ)
	{
		return -1;
	}

	memcpy(&pSeq->fgRect[pSeq->num], pRectS, sizeof(Alg_ObjRect));

	pSeq->num++;


	return 0;
}




Alg_ObjRect *Alg_FRS_getCell(Alg_fgRectSeq *pSeq, int i)
{
	//assert(NULL != pSeq);
	//assert(i >= 0 && i < pSeq->num);

	return &pSeq->fgRect[i];
}





int Alg_FRS_clear(Alg_fgRectSeq *pSeq)
{
	//assert(NULL != pSeq);

	pSeq->num = 0;

	return 0;
}





