#ifndef ALG_STRUCT_DEF
#define ALG_STRUCT_DEF




#ifdef __cplusplus
extern "C"{
#endif



typedef struct
{
	float v[2];
}ST_vec2f;

typedef struct
{
	float v[3];
}ST_vec3f;


#define WIDTH_ALY	(176)
#define HEIGHT_ALY	(144)

#define DIMS	(3)
#define N_GAUSS	(5)



#define NUM_PXL_ALS	(WIDTH_ALY*HEIGHT_ALY)

#ifndef WIDTHBYTES
#define WIDTHBYTES(bytes)	(((bytes) + 3) >> 2 << 2)
#endif





/*********************************************************************************************/

//��
typedef struct
{
	int x;		//���Ͻ�����
	int y;
	int width;	//��
	int height;	//��
	int pxls;
}Alg_ObjRect;



/*********************************************************************************************/



/*********************************************************************************************/


enum{ Alg_OBJ_MAXNUM_FRSEQ = 256, };


//������
typedef struct
{
	int num;
	Alg_ObjRect fgRect[Alg_OBJ_MAXNUM_FRSEQ];
}Alg_fgRectSeq;



int Alg_FRS_init(Alg_fgRectSeq *pSeq);
int Alg_FRS_add(Alg_fgRectSeq *pSeq, Alg_ObjRect *pRectS);
Alg_ObjRect *Alg_FRS_getCell(Alg_fgRectSeq *pSeq, int i);
int Alg_FRS_clear(Alg_fgRectSeq *pSeq);


/*********************************************************************************************/



/*********************************************************************************************/




#ifdef __cplusplus
}
#endif





#endif
