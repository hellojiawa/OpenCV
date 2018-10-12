package com.funsnap.opencv.utils;

import android.content.Context;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * TODO
 * version: V1.0 <描述当前版本功能>
 * fileName: com.funsnap.opencv.utils.FileUtils
 * author: liuping
 * date: 2018/9/27 16:37
 */
public class FileUtils {
    public static void copyFile(final Context context, final File file) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    InputStream inputStream = context.getAssets().open(file.getName());
                    FileOutputStream outputStream = new FileOutputStream(file);
                    byte[] bytes = new byte[10240];
                    int count = 0;
                    while ((count = inputStream.read(bytes)) != -1) {
                        outputStream.write(bytes, 0, count);
                    }

                    outputStream.flush();
                    inputStream.close();
                    outputStream.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }).start();
    }
}
