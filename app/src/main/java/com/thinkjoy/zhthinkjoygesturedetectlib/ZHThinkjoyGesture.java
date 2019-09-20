package com.thinkjoy.zhthinkjoygesturedetectlib;

import android.graphics.Bitmap;

import java.lang.ref.WeakReference;
import java.util.List;

/**
 * TODO
 * version: V1.0 <描述当前版本功能>
 * fileName: thinkjoy.zhthinkjoygesturedetectlib.ZHThinkjoyGesture
 * author: liuping
 * date: 2019/9/20 10:54
 */
public class ZHThinkjoyGesture {
    private static ZHThinkjoyGesture mInstance;
    public static final int IMAGE_FORMAT_NV21 = 1;
    private long gestureHand;

    private ZHThinkjoyGesture() {
        this.gestureHand = this.nativeGestureCreate(new WeakReference(this));
    }

    public static ZHThinkjoyGesture getInstance() {
        if (mInstance == null) {
            mInstance = new ZHThinkjoyGesture();
        }

        return mInstance;
    }

    public void gestureDetect(Bitmap bitmap, List<GestureInfo> gestureInfoList) {
        this.nativeGestureDetect(this.gestureHand, bitmap, gestureInfoList);
    }

    public void gestureDetect(byte[] imageArray, int imageType, int height, int width, List<GestureInfo> gestureInfoList) {
        this.nativeGestureDetect2(this.gestureHand, imageArray, imageType, height, width, gestureInfoList);
    }


    public void init() {
        this.nativeGestureInit(this.gestureHand);
    }

    public void release() {
        this.nativeGestureRelease(this.gestureHand);
    }

    public void setConfig(GestureConfig gestureConfig) {
        int[] configs = new int[]{gestureConfig.Rotation, gestureConfig.ResultMode};
        this.nativeSetConfig(this.gestureHand, configs);
    }

    public GestureConfig getConfig() {
        GestureConfig gestureConfig = new GestureConfig();
        int[] configs = this.nativeGetConfig(this.gestureHand);
        gestureConfig.Rotation = configs[0];
        gestureConfig.ResultMode = configs[1];
        return gestureConfig;
    }

    private native long nativeGestureCreate(WeakReference<ZHThinkjoyGesture> var1);

    private native void nativeGestureRelease(long var1);

    private native int nativeGestureInit(long var1);

    private native void nativeGestureDetect(long var1, Bitmap var3, List<GestureInfo> var4);

    private native void nativeGestureDetect2(long var1, byte[] var3, int var4, int var5, int var6, List<GestureInfo> var7);

    private native void nativeSetConfig(long var1, int[] var3);

    private native int[] nativeGetConfig(long var1);


    static {
        System.loadLibrary("gesture");
    }

}
