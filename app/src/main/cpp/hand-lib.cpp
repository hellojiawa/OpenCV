#include <jni.h>
#include <string>
#include <opencv2/opencv.hpp>
#include<vector>
#include<string>
#include<algorithm>
#include<iostream>
#include <opencv2/tracking.hpp>
#include<unordered_map>
#include <android/log.h>

#define SCALE 2
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, "liuping", __VA_ARGS__)

using namespace std;


cv::Rect overlap(cv::Rect rect1, cv::Rect rect2) {
    int x1 = rect1.tl().x, x2 = rect2.tl().x, y1 = rect1.tl().y, y2 = rect2.tl().y;
    int x3 = x1 + rect1.width, x4 = x2 + rect2.width, y3 = y1 + rect1.height, y4 =
            y2 + rect2.height;
    int width = min(x3, x4) - max(x1, x2);
    int height = min(y3, y4) - max(y1, y2);
    return cv::Rect(max(x1, x2), max(y1, y2), width, height);
}

bool compare_bbox(cv::Rect rect1, cv::Rect rect2) {
    //to determin wheter the track function is going on well
    double scale_1 = rect1.width * 1.0 / rect1.height;
    double scale_2 = rect2.width * 1.0 / rect2.height;
    if (abs(scale_1 - scale_2) > 0.1)return false;
    int x1 = rect1.tl().x, x2 = rect2.tl().x, y1 = rect1.tl().y, y2 = rect2.tl().y;
    int x3 = x1 + rect1.width, x4 = x2 + rect2.width, y3 = y1 + rect1.height, y4 =
            y2 + rect2.height;
    int width = min(x3, x4) - max(x1, x2);
    int height = min(y3, y4) - max(y1, y2);
    int area = width * height;
    cout << area * 1.0 / rect1.area() << endl;
    if (area * 1.0 / rect1.area() < 0.75)return false;

    return true;
}


extern "C" JNIEXPORT void
JNICALL Java_com_funsnap_opencv_HandGesture_init(JNIEnv *env, jobject jobject1) {

}

extern "C" JNIEXPORT jstring
JNICALL
Java_com_funsnap_opencv_HandGesture_detection(JNIEnv *env, jobject jobject1,
                                              jlong matRGB, jobject rect) {



    double Minification = SCALE;
    cv::Ptr<cv::TrackerMedianFlow> tracker = cv::TrackerMedianFlow::create();

    cv::Mat test = (*((cv::Mat *) matRGB));



    return env->NewStringUTF("");
};

