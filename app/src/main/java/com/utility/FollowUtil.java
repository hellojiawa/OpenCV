package com.utility;

import android.graphics.Bitmap;
import android.graphics.Rect;

/**
 * TODO
 * version: V1.0 <描述当前版本功能>
 * fileName: com.utility.FollowUtil
 * author: liuping
 * date: 2019/1/24 13:57
 */
public class FollowUtil {

    static {
        System.loadLibrary("funsnap_target");
    }

    /**
     *
     * @param bitmap  RGB8888
     * @param width
     * @param height
     * @param rectIn
     * @param rectOut
     * @return
     */
    public static native int getTarget(Bitmap bitmap, int width, int height, Rect rectIn, Rect rectOut);
}
