package com.funsnap.opencv.tensorflower.classifier.impl;

import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.RectF;

import com.funsnap.opencv.tensorflower.classifier.Recognition;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;


public class TensorFlowObjectDetectionUtil {

    static final String OUTPUT_DETECTION_BOXES = "detection_boxes";
    static final String OUTPUT_DETECTION_SCORES = "detection_scores";
    static final String OUTPUT_DETECTION_CLASSES = "detection_classes";
    static final String OUTPUT_NUM_DETECTION = "num_detections";

    static final String[] OUTPUTS = new String[]{
            OUTPUT_DETECTION_BOXES, OUTPUT_DETECTION_CLASSES, OUTPUT_DETECTION_SCORES, OUTPUT_NUM_DETECTION
    };

    static final String INPUT_NAME = "image_tensor";

    private static final Comparator<Recognition> COMPARATOR = new CompareByConfidenceDescending();

    static void initializeLabels(List<String> labels, AssetManager assetManager, String labelsFileName) throws IOException {
        BufferedReader br = new BufferedReader(new InputStreamReader(assetManager.open(labelsFileName)));
        String l;
        while((l = br.readLine()) != null) {
            labels.add(l);
        }
    }

    static void populateRecognitions(List<Recognition> recognitions,
                                     int resultCount,
                                     List<String> labels,
                                     float[] detectionClasses,
                                     float[] detectionScores,
                                     float[] detectionBoxes,
                                     int width,
                                     int height) {
        recognitions.clear();
        for (int i = 0; i < resultCount; ++i) {
            recognitions.add(new Recognition(
                    ""+i,
                    labels.get((int) (detectionClasses[i] - 1)),
                    detectionScores[i],
                    new RectF(
                            detectionBoxes[4 * i + 1] * width,
                            detectionBoxes[4 * i] * height,
                            detectionBoxes[4 * i + 3] * width,
                            detectionBoxes[4 * i + 2] * height
                    )       // boxes[y_min, x_min, y_max, x_max]
            ));
        }

        Collections.sort(recognitions, COMPARATOR);
    }

    static void processBitmap(Bitmap bitmap, int[] pixelBuffer, byte[] outputBufferByChannel) {
        bitmap.getPixels(pixelBuffer, 0, bitmap.getWidth(), 0, 0, bitmap.getWidth(), bitmap.getHeight());

        for (int i = 0; i < pixelBuffer.length; i++) {
            outputBufferByChannel[i * 3 + 2] = (byte) (pixelBuffer[i] & 0xFF);              //B
            outputBufferByChannel[i * 3 + 1] = (byte) ((pixelBuffer[i] >> 8) & 0xFF);       //G
            outputBufferByChannel[i * 3    ] = (byte) ((pixelBuffer[i] >> 16) & 0xFF);      //R
        }
    }

    private static class CompareByConfidenceDescending implements Comparator<Recognition> {
        @Override
        public int compare(Recognition recognition, Recognition t1) {
            return Float.compare(t1.getConfidence(), recognition.getConfidence());
        }
    }
}
