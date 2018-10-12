package com.funsnap.opencv.tensorflower.classifier.impl;

import android.content.res.AssetManager;

import com.funsnap.opencv.tensorflower.classifier.Classifier;
import com.funsnap.opencv.tensorflower.classifier.Recognition;

import org.opencv.core.Mat;
import org.tensorflow.contrib.android.TensorFlowInferenceInterface;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import static com.funsnap.opencv.tensorflower.classifier.impl.TensorFlowObjectDetectionUtil.INPUT_NAME;
import static com.funsnap.opencv.tensorflower.classifier.impl.TensorFlowObjectDetectionUtil.OUTPUTS;
import static com.funsnap.opencv.tensorflower.classifier.impl.TensorFlowObjectDetectionUtil.OUTPUT_DETECTION_BOXES;
import static com.funsnap.opencv.tensorflower.classifier.impl.TensorFlowObjectDetectionUtil.OUTPUT_DETECTION_CLASSES;
import static com.funsnap.opencv.tensorflower.classifier.impl.TensorFlowObjectDetectionUtil.OUTPUT_DETECTION_SCORES;
import static com.funsnap.opencv.tensorflower.classifier.impl.TensorFlowObjectDetectionUtil.OUTPUT_NUM_DETECTION;
import static com.funsnap.opencv.tensorflower.classifier.impl.TensorFlowObjectDetectionUtil.initializeLabels;
import static com.funsnap.opencv.tensorflower.classifier.impl.TensorFlowObjectDetectionUtil.populateRecognitions;

public class TensorFlowObjectDetectionAPIModel implements Classifier {

    private static final int MAX_RESULTS = 100;

    private TensorFlowInferenceInterface tensorFlowInferenceInterface;

    private ArrayList<String> labels = new ArrayList<>();

    private float[] detectionBoxes;
    private float[] detectionClasses;
    private float[] detectionScores;
    private float[] numDetection;

    private byte[] buffer;
    private ArrayList<Recognition> recognitions = new ArrayList<>();

    private int width;
    private int height;

    public TensorFlowObjectDetectionAPIModel(AssetManager assetManager, String modelFileName, String labelsFileName, int width, int height) throws IOException {
        this.width = width;
        this.height = height;

        initializeLabels(labels, assetManager, labelsFileName);

        tensorFlowInferenceInterface = new TensorFlowInferenceInterface(assetManager, modelFileName);

        buffer = new byte[width * height * 3];

        detectionBoxes = new float[MAX_RESULTS * 4];
        detectionClasses = new float[MAX_RESULTS];
        detectionScores = new float[MAX_RESULTS];
        numDetection = new float[1];
    }

    public void close() {
        tensorFlowInferenceInterface.close();
    }

    @Override
    public List<Recognition> recognizeImage(Mat matRGB) {
        matRGB.get(0, 0, buffer);

        tensorFlowInferenceInterface.feed(INPUT_NAME, buffer, 1, height, width, 3);

        //500ms
        tensorFlowInferenceInterface.run(OUTPUTS);

        tensorFlowInferenceInterface.fetch(OUTPUT_DETECTION_BOXES, detectionBoxes);
        tensorFlowInferenceInterface.fetch(OUTPUT_DETECTION_CLASSES, detectionClasses);
        tensorFlowInferenceInterface.fetch(OUTPUT_DETECTION_SCORES, detectionScores);
        tensorFlowInferenceInterface.fetch(OUTPUT_NUM_DETECTION, numDetection);

        populateRecognitions(recognitions, Math.round(numDetection[0]), labels, detectionClasses, detectionScores, detectionBoxes, width, height);

        return recognitions;
    }

}
