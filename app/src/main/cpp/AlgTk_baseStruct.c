#include "AlgTk_baseStruct.h"
#include "AlgTk_Data.h"
#include <string.h>
#include <math.h>




//extern float g_divArr[4111];


//轨迹队列初始化
int AlgTk_SMP_init(AlgTK_SMP *smpArr)
{
	memset(smpArr, 0, sizeof(AlgTK_SMP));

	smpArr->head = 0;
	smpArr->tail = 0;

	return 0;
}




int AlgTk_SMP_add(AlgTK_SMP *smpArr, ST_mat smp)
{
	memcpy(&smpArr->arr[smpArr->tail], &smp, sizeof(ST_mat));

	smpArr->tail++;

	if (smpArr->tail%AlgTK_SMP_CAP_INIT == 0)
	{
		smpArr->tail = 0;
	}

	if (smpArr->num < AlgTK_SMP_CAP_INIT)
	{
		smpArr->num++;
	}
	else
	{
		smpArr->head++;

		if (smpArr->head%AlgTK_SMP_CAP_INIT == 0)
		{
			smpArr->head = 0;
		}
	}


	return 0;
}



//获取一个轨迹
ST_mat *AlgTk_SMP_getCell(AlgTK_SMP *smpArr, int i)
{
	int j;

	if (i >= 0 && i < smpArr->num)
	{
		j = 0;
	}
	else
	{
		j = 1;

		return NULL;
	}

	//return &smpArr->arr[(i + smpArr->head) % AlgTK_SMP_CAP_INIT];

	j = smpArr->tail - (i + 1);

	if (j < 0)
	{
		return &smpArr->arr[AlgTK_SMP_CAP_INIT - 1];
	}
	else
	{
		return &smpArr->arr[j];
	}

}







//清空
int AlgTk_SMP_clear(AlgTK_SMP *smpArr)
{
	AlgTk_SMP_init(smpArr);

	return 0;
}






void _integral(const unsigned char* src, unsigned int _srcStep,
	float* sum, unsigned int _sumStep,
	float* sqsum, unsigned int _sqsumStep,
	int width, int height)
{
	int x, y;
	float s, sq, t, tq;
	unsigned char it;

	int srcStep = _srcStep;//(int)(_srcStep / sizeof(unsigned char));//320,15
	int sumStep = _sumStep;//(int)(_sumStep / sizeof(float));//321,16
	int sqsumStep = _sqsumStep;//(int)(_sqsumStep / sizeof(float));//321,15

	//size.width *= cn;

	memset(sum, 0, (width + 1)*sizeof(sum[0]));
	sum += sumStep + 1;

	memset(sqsum, 0, (width + 1)*sizeof(sqsum[0]));
	sqsum += sqsumStep + 1;

	for (y = 0; y < height; y++,
		src += srcStep - 1,
		sum += sumStep - 1,
		sqsum += sqsumStep - 1)
	{
		s = sum[-1] = 0;
		sq = sqsum[-1] = 0;
		for (x = 0; x < width; x++)
		{
			it = src[x];
			s += it;
			sq += (float)it*it;
			t = sum[x - sumStep] + s;
			tq = sqsum[x - sqsumStep] + sq;
			sum[x] = t;
			sqsum[x] = tq;
		}

		src++, sum++, sqsum++;
	}

}





void matchTemplate(unsigned char* pImg1, unsigned char* pImg2, int width, int height, float* _ncc)
{
	int i, n;
	float mea1, mea2;
	float d1, d2, d3, d4, d5, d6, d7, d8;
	float sq1, sq2, sq3;

	n = width*height;

	mea1 = mea2 = 0;
	for (i = 0; i < n; i += 4)
	{
		mea1 += pImg1[i] + pImg1[i + 1] + pImg1[i + 2] + pImg1[i + 3];
		mea2 += pImg2[i] + pImg2[i + 1] + pImg2[i + 2] + pImg2[i + 3];
	}

	mea1 *= g_divArr[n];
	mea2 *= g_divArr[n];

	sq1 = sq2 = sq3 = 0;

	for (i = 0; i < n; i += 4)
	{
		d1 = pImg1[i] - mea1;
		d2 = pImg1[i + 1] - mea1;
		d3 = pImg1[i + 2] - mea1;
		d4 = pImg1[i + 3] - mea1;

		d5 = pImg2[i] - mea2;
		d6 = pImg2[i + 1] - mea2;
		d7 = pImg2[i + 2] - mea2;
		d8 = pImg2[i + 3] - mea2;

		sq1 += (d1*d5 + d2*d6 + d3*d7 + d4*d8);
		sq2 += (d1*d1 + d2*d2 + d3*d3 + d4*d4);
		sq3 += (d5*d5 + d6*d6 + d7*d7 + d8*d8);
	}

	(*_ncc) = sq1 / (sqrtf(sq2*sq3) + 0.0001);

	return;
}







//void matchTemplate(unsigned char* pImg1, unsigned char* pImg2, int width, int height, float* _ncc)
//{
//	int i, n;
//	int iMea1, iMea2;
//	int d1, d2, d3, d4, d5, d6, d7, d8;
//	long long sq1, sq2, sq3;
//	float flSq1, flSq2, flSq3;
//	int ier;
//
//	n = width*height;
//
//	iMea1 = iMea2 = 0;
//	for (i = 0; i < n; i += 4)
//	{
//		iMea1 += pImg1[i] + pImg1[i + 1] + pImg1[i + 2] + pImg1[i + 3];
//		iMea2 += pImg2[i] + pImg2[i + 1] + pImg2[i + 2] + pImg2[i + 3];
//	}
//
//
//	iMea1 = (iMea1 + (2 << 0) >> 2);
//	iMea2 = (iMea2 + (2 << 0) >> 2);
//	ier = 36;
//	sq1 = sq2 = sq3 = 0;
//
//	for (i = 0; i < n; i += 4)
//	{
//		d1 = (pImg1[i] * ier - iMea1);
//		d2 = (pImg1[i + 1] * ier - iMea1);
//		d3 = (pImg1[i + 2] * ier - iMea1);
//		d4 = (pImg1[i + 3] * ier - iMea1);
//
//		d5 = (pImg2[i] * ier - iMea2);
//		d6 = (pImg2[i + 1] * ier - iMea2);
//		d7 = (pImg2[i + 2] * ier - iMea2);
//		d8 = (pImg2[i + 3] * ier - iMea2);
//
//		sq1 += (d1*d5 + d2*d6 + d3*d7 + d4*d8);
//		sq2 += (d1*d1 + d2*d2 + d3*d3 + d4*d4);
//		sq3 += (d5*d5 + d6*d6 + d7*d7 + d8*d8);
//	}
//
//	flSq1 = sq1*g_divArr[1296];
//	flSq2 = sq2*g_divArr[1296];
//	flSq3 = sq3*g_divArr[1296];
//
//	(*_ncc) = flSq1 / sqrtf(flSq2*flSq3 + 0.001);
//	//(*_ncc) = (float)(sq1 >> 16) / sqrtf((max(sq2, sq3) >> 18)*(min(sq2, sq3) >> 14) + 0.001);
//
//	return;
//}

//#define CONSTRAINT_SQ	(150*150*72*72)
//
//void matchTemplate(unsigned char* pImg1, unsigned char* pImg2, int width, int height, float* _ncc)
//{
//	int i, n;
//	int iMea1, iMea2;
//	int d1, d2, d3, d4, d5, d6, d7, d8;
//	int sq1;
//	unsigned int sq2, sq3;
//	float flSq1, flSq2, flSq3;
//	int ier;
//
//
//	n = width*height;
//
//	iMea1 = iMea2 = 0;
//	for (i = 0; i < n; i += 4)
//	{
//		iMea1 += pImg1[i] + pImg1[i + 1] + pImg1[i + 2] + pImg1[i + 3];
//		iMea2 += pImg2[i] + pImg2[i + 1] + pImg2[i + 2] + pImg2[i + 3];
//	}
//
//	//iMea1 = (iMea1 + (1 << 1) >> 2);
//	//iMea2 = (iMea2 + (1 << 1) >> 2);
//	iMea1 = (iMea1 + (1 << 0) >> 1);
//	iMea2 = (iMea2 + (1 << 0) >> 1);
//
//	ier = 72;
//	sq1 = sq2 = sq3 = 0;
//
//	for (i = 0; i < n; i += 4)
//	{
//		d1 = (pImg1[i] * ier - iMea1);
//		d2 = (pImg1[i + 1] * ier - iMea1);
//		d3 = (pImg1[i + 2] * ier - iMea1);
//		d4 = (pImg1[i + 3] * ier - iMea1);
//
//		d5 = (pImg2[i] * ier - iMea2);
//		d6 = (pImg2[i + 1] * ier - iMea2);
//		d7 = (pImg2[i + 2] * ier - iMea2);
//		d8 = (pImg2[i + 3] * ier - iMea2);
//
//		//sq1 += (d1*d5 + d2*d6 + d3*d7 + d4*d8 + 16 >> 5);
//		//sq2 += (d1*d1 + d2*d2 + d3*d3 + d4*d4 + 16 >> 5);
//		//sq3 += (d5*d5 + d6*d6 + d7*d7 + d8*d8 + 16 >> 5);
//		sq1 += (d1*d5 + d2*d6 + d3*d7 + d4*d8 + 4 >> 3);
//		sq2 += (d1*d1 + d2*d2 + d3*d3 + d4*d4 + 4 >> 3);
//		sq3 += (d5*d5 + d6*d6 + d7*d7 + d8*d8 + 4 >> 3);
//	}
//
//
//	flSq1 = sq1*g_divArr[81];
//	flSq2 = sq2*g_divArr[81];
//	flSq3 = sq3*g_divArr[81];
//
//	(*_ncc) = flSq1 / sqrtf(flSq2*flSq3 + 0.001);
//
//	//printf("\niMea1:%d, iMea2:%d, sq1:%d, sq2:%d, sq3:%d, sq1_2:%d, sq2_2:%d, sq3_2:%d, flSq1:%.2f, flSq2:%.2f, flSq3:%.2f, _ncc%.6f\n",
//	//	iMea1, iMea2, sq1, sq2, sq3, sq1_2, sq2_2, sq3_2, flSq1, flSq2, flSq3, (*_ncc));
//
//
//	return;
//}






