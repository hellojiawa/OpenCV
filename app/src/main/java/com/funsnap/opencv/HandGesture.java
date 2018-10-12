package com.funsnap.opencv;

import org.opencv.core.Rect;

/**
 * TODO
 * version: V1.0 <描述当前版本功能>
 * fileName: com.funsnap.opencv.HandGesture
 * author: liuping
 * date: 2018/9/26 19:20
 */
public class HandGesture {

    static {
        System.loadLibrary("hand-lib");
    }

    /**
     * @param mat
     * @param rect
     * @return 结果
     */
    public static native String detection(long mat, Rect rect);

    public static native void init();
}
