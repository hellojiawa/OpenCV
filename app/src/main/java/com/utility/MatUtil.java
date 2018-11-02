package com.utility;

/**
 * TODO
 * version: V1.0 <描述当前版本功能>
 * fileName: com.utility.MatUtil
 * author: liuping
 * date: 2018/9/26 19:20
 */
public class MatUtil {

    static {
        System.loadLibrary("mat_util");
    }


    /**
     * @param yuv
     * @param width
     * @param height
     * @param matAddress mat地址
     */
    public static native void getMat(byte[] yuv, int width, int height, long matAddress);
}
