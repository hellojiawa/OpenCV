#include <jni.h>
#include <string>
#include <opencv2/opencv.hpp>
#include<vector>
#include<algorithm>
#include<unordered_map>
#include <android/log.h>
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, "liuping", __VA_ARGS__)

using namespace std;


extern "C" JNIEXPORT void
JNICALL
Java_com_utility_MatUtil_getMat(JNIEnv *env, jobject jobject1,
                                jbyteArray byteArray, jint width, jint height, jlong address) {

    jbyte *srcLumaPtr = env->GetByteArrayElements(byteArray, NULL);

    if (srcLumaPtr == nullptr) {
        return;
    }
    cv::Mat mYuv(height + height / 2, width, CV_8UC1, srcLumaPtr);

    cv::Mat &mat = *(cv::Mat *) address;

    mYuv.copyTo(mat);

    mYuv.release();
    env->ReleaseByteArrayElements(byteArray, srcLumaPtr, 0);

};

