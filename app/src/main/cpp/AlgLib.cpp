//
// Created by admin on 2019/1/24.
//

#include <jni.h>
#include <cstring>
#include "AlgContourExtract.h"
#include "AlgTk_StructDef.h"
#include <android/bitmap.h>
#include <cstdlib>
#include <android/log.h>

#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, "liuping", __VA_ARGS__)


void RGB8888ToRGB(char *input, char *output,
                  const int width, const int height) {

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel = y * width + x;

            output[pixel * 3 + 0] = input[pixel * 4 + 0];
            output[pixel * 3 + 1] = input[pixel * 4 + 1];
            output[pixel * 3 + 2] = input[pixel * 4 + 2];
        }
    }
}

extern "C"
JNIEXPORT jint JNICALL Java_com_utility_FollowUtil_getTarget(JNIEnv *env, jclass clsThis,
                                                                   jobject bitmap, jint width,
                                                                   jint height,
                                                                   jobject rectIn,
                                                                   jobject rectOut) {

    //rect
    Alg_rect algRectIn;
    memset(&algRectIn, 0, sizeof(algRectIn));
    Alg_rect algRectOut;
    memset(&algRectOut, 0, sizeof(algRectOut));

    //获取rect值
    jclass jrect = env->FindClass("android/graphics/Rect");
    jfieldID jleft = env->GetFieldID(jrect, "left", "I");
    jfieldID jtop = env->GetFieldID(jrect, "top", "I");
    jfieldID jright = env->GetFieldID(jrect, "right", "I");
    jfieldID jbottom = env->GetFieldID(jrect, "bottom", "I");
    algRectIn.x = env->GetIntField(rectIn, jleft);
    algRectIn.y = env->GetIntField(rectIn, jtop);
    algRectIn.width = env->GetIntField(rectIn, jright) - algRectIn.x;
    algRectIn.height = env->GetIntField(rectIn, jbottom) - algRectIn.y;


    void *m_gbHandle = AlgContourExtract_Create();

    if (NULL == m_gbHandle) {
        return 0;
    }

    AndroidBitmapInfo infoIn;
    void *pixels;

    // Get image info
    if (AndroidBitmap_getInfo(env, bitmap, &infoIn) != ANDROID_BITMAP_RESULT_SUCCESS) {
        return 0;
    }
    if (infoIn.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        return 0;
    }


    // Lock all images
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
        return 0;
    }

    void *rgb = malloc(width * height * 3 * sizeof(char));    //分配内存
    RGB8888ToRGB((char *) pixels, (char *) rgb, width, height);


//    int len = width * height * 3;

//    jbyteArray jarrRV = env->NewByteArray(len);
//    env->SetByteArrayRegion(jarrRV, 0, len, reinterpret_cast<const jbyte *>(rgb));


    AndroidBitmap_unlockPixels(env, bitmap);

    //执行
    jint result = AlgContourExtract_GrabCut(m_gbHandle, (unsigned char *) (rgb), width, height,
                                            algRectIn,
                                            &algRectOut, 1);

    //提取结果
    env->SetIntField(rectOut, jleft, algRectOut.x);
    env->SetIntField(rectOut, jtop, algRectOut.y);
    env->SetIntField(rectOut, jright, algRectOut.x + algRectOut.width);
    env->SetIntField(rectOut, jbottom, algRectOut.y + algRectOut.height);


    //释放资源
    free(rgb);
    AlgContourExtract_Destroy(&m_gbHandle);

    return result;
}