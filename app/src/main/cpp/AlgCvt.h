#ifndef _ALGVQD_CVT_H_
#define _ALGVQD_CVT_H_






// Conversion from RGB24 to YUV420
void InitLookupTable();
//int ConvertRGB2YUV(int w,int h,unsigned char *rgbdata,unsigned int *yuv);
int ConvertRGB2YUV(int w,int h,unsigned char *bmp,unsigned char*yuv) ;
// Conversion from YUV420 to RGB24
void InitConvertTable();
void ConvertYUV2RGB(unsigned char *src0,unsigned char *src1,unsigned char *src2,unsigned char *dst_ori,
					int width,int height);








#endif