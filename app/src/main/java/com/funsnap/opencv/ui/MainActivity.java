package com.funsnap.opencv.ui;

import android.Manifest;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v7.app.AppCompatActivity;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.Switch;

import com.funsnap.opencv.R;
import com.funsnap.opencv.permission.PerActivity;
import com.funsnap.opencv.permission.PerChecker;
import com.funsnap.opencv.tensorflower.classifier.Recognition;
import com.funsnap.opencv.tensorflower.classifier.impl.TensorFlowObjectDetectionAPIModel;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;
import org.opencv.android.CameraBridgeViewBase;
import org.opencv.android.JavaCameraView;
import org.opencv.android.OpenCVLoader;
import org.opencv.android.Utils;
import org.opencv.core.Core;
import org.opencv.core.CvType;
import org.opencv.core.Mat;
import org.opencv.core.MatOfInt;
import org.opencv.core.MatOfInt4;
import org.opencv.core.MatOfPoint;
import org.opencv.core.MatOfPoint2f;
import org.opencv.core.MatOfRect;
import org.opencv.core.MatOfRect2d;
import org.opencv.core.Rect2d;
import org.opencv.core.RotatedRect;
import org.opencv.core.Scalar;
import org.opencv.core.Size;
import org.opencv.imgproc.Imgproc;
import org.opencv.me.ObjectDetector;
import org.opencv.tracking.MultiTracker;
import org.opencv.tracking.Tracker;
import org.opencv.tracking.TrackerKCF;
import org.opencv.tracking.TrackerMOSSE;
import org.opencv.tracking.TrackerTLD;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import static java.lang.Math.abs;

public class MainActivity extends AppCompatActivity implements
        CameraBridgeViewBase.CvCameraViewListener2,
        CompoundButton.OnCheckedChangeListener {

    private static final int REQUEST_CODE = 100;
    private static String[] PERMISSIONS = {
            Manifest.permission.CAMERA,
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
    };

//    private static final String TF_OD_API_MODEL_FILE = "file:///android_asset/hand.pb";
//    private static final String TF_OD_API_LABELS_FILE = "hand_labels.txt";

    private static final String TF_OD_API_MODEL_FILE = "file:///android_asset/ssd5_optimized_inference_graph.pb";
    private static final String TF_OD_API_LABELS_FILE = "hand_label_map.txt";

//    private static final String TF_OD_API_MODEL_FILE = "file:///android_asset/frozen_inference_graph.pb";
//    private static final String TF_OD_API_LABELS_FILE = "hand_detection.txt";


    private static final Float MINIMUM_CONFIDENCE_TF_OD_API = 0.3f;

    private JavaCameraView mJava_camera;
    private RectView mRectView;

    private boolean mTracking = false;
    private boolean mDetection = false;
    private Rect2d mRect2d = new Rect2d(0, 0, 0, 0);
    private int mButtonID;
    private boolean mUpdated = false;
    private boolean mDetectionV = false;

    MultiTracker mMultiTracker = null;
    MatOfRect2d mMatOfRect2d = null;
    ArrayList<Rect> mRects = new ArrayList<>();
    ArrayList<TrackerMOSSE> mTrackerMOSSES = new ArrayList<>();
    private RadioGroup mRadioGroup;
    private ObjectDetector mPalmDetector;
    private MatOfRect mObject;
    private TensorFlowObjectDetectionAPIModel detector;
    private ImageView mPreImageView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);

        initView();

        //opencv
        OpenCVLoader.initDebug();
        mJava_camera.setCameraIndex(CameraBridgeViewBase.CAMERA_ID_BACK);
        mJava_camera.enableFpsMeter();
        mJava_camera.enableView();

        //eventbus
        EventBus.getDefault().register(this);

        if (new PerChecker(this).lacksPermissions(PERMISSIONS)) {
            PerActivity.startActivityForResult(this, REQUEST_CODE, PERMISSIONS);
        }

    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_CODE && resultCode == PerActivity.PERMISSIONS_DENIED) {
            finish();
        } else {
//            File file = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + File.separator + "opencv_test");
//            file.mkdirs();
//
//            String path = file.getAbsolutePath();
//            copyFile(new File(path + File.separator + "fist.svm"));
        }
    }

    private void initView() {
        mJava_camera = findViewById(R.id.java_camera);
        mJava_camera.setCvCameraViewListener(this);

        mRadioGroup = findViewById(R.id.icon_group);

        mPreImageView = findViewById(R.id.image_view);

        mRectView = findViewById(R.id.rect_view);
        mRectView.setTouchListener(new RectView.ITouchEvent() {
            @Override
            public void onDown() {
                if (mButtonID != R.id.rb_multi) {
                    exitTracking();
                }
            }

            @Override
            public void onUp(Rect target) {
                if (mButtonID != R.id.rb_multi) {
                    startSingleTrack(target);
                } else {
                    startMultiTrack(target);
                }
                disableRadioGroup(mRadioGroup);
            }
        });

        findViewById(R.id.btn_start_palm).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startDetectionPlam();
                disableRadioGroup(mRadioGroup);
            }
        });

        findViewById(R.id.btn_start_hand).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startDetectionHand();
                disableRadioGroup(mRadioGroup);
            }
        });

        findViewById(R.id.btn_clear).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                exitTracking();
            }
        });

        Switch aSwitch = findViewById(R.id.switch_v);
        aSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    mDetectionV = true;
                } else {
                    mDetectionV = false;
                }
            }
        });

        RadioButton rbKFC = findViewById(R.id.rb_kfc);
        rbKFC.setOnCheckedChangeListener(this);
        rbKFC.setChecked(true);
        mButtonID = R.id.rb_kfc;
        RadioButton rbMosse = findViewById(R.id.rb_mosse);
        rbMosse.setOnCheckedChangeListener(this);
        RadioButton rbTLD = findViewById(R.id.rb_tld);
        rbTLD.setOnCheckedChangeListener(this);
        RadioButton rbMulti = findViewById(R.id.rb_multi);
        rbMulti.setOnCheckedChangeListener(this);
    }

    public void disableRadioGroup(RadioGroup testRadioGroup) {
        for (int i = 0; i < testRadioGroup.getChildCount(); i++) {
            testRadioGroup.getChildAt(i).setEnabled(false);
        }
    }

    public void enableRadioGroup(RadioGroup testRadioGroup) {
        for (int i = 0; i < testRadioGroup.getChildCount(); i++) {
            testRadioGroup.getChildAt(i).setEnabled(true);
        }
    }

    private void exitTracking() {
        mRectView.clearRects();
        mTracking = false;
        mDetection = false;
        enableRadioGroup(mRadioGroup);
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        if (isChecked) {
            mButtonID = buttonView.getId();
        }
    }

    private void startSingleTrack(final Rect target) {
        if (!mTracking) {
            new Thread(new Runnable() {
                @Override
                public void run() {
                    mTracking = true;

                    Tracker tracker = getTracker();

                    synchronized (MainActivity.class) {
                        tracker.init(mMat, new Rect2d(target.left / ratio, target.top / ratio,
                                target.width() / ratio, target.height() / ratio));
                    }

                    mRects.clear();
                    mRects.add(new Rect(target));
                    while (mTracking) {

                        if (mUpdated) {
                            mUpdated = false;

                            if (mButtonID == R.id.rb_tld || mButtonID == R.id.rb_kfc) {
                                Mat clone = null;
                                synchronized (MainActivity.class) {
                                    clone = mMat.clone();
                                }
                                tracker.update(clone, mRect2d);
                                clone.release();
                            } else {
                                synchronized (MainActivity.class) {
                                    tracker.update(mMat, mRect2d);
                                }
                            }

                            mRects.get(0).set((int) (mRect2d.x * ratio), (int) (mRect2d.y * ratio),
                                    (int) ((mRect2d.x + mRect2d.width) * ratio), (int) ((mRect2d.y + mRect2d.height) * ratio));
                            EventBus.getDefault().post(mRects);
                        } else {
                            SystemClock.sleep(4);
                        }

                    }

                    tracker.clear();
                }
            }).start();
        }
    }


    private void startMultiTrack(final Rect target) {
        if (!mTracking) {
            mMultiTracker = MultiTracker.create();
            TrackerMOSSE trackerMOSSE = TrackerMOSSE.create();
            mTrackerMOSSES.add(trackerMOSSE);

            synchronized (MainActivity.class) {
                mMultiTracker.add(trackerMOSSE, mMat, new Rect2d(target.left / ratio, target.top / ratio,
                        target.width() / ratio, target.height() / ratio));
            }

            mMatOfRect2d = new MatOfRect2d();

            mRects.clear();
            mRects.add(new Rect(target));

            new Thread(new Runnable() {
                @Override
                public void run() {
                    mTracking = true;

                    while (mTracking) {
                        if (mUpdated) {     //图像数据有更新
                            mUpdated = false;

                            synchronized (MainActivity.class) {
                                mMultiTracker.update(mMat, mMatOfRect2d);
                            }
                            Rect2d[] rect2ds = mMatOfRect2d.toArray();

                            for (int i = 0; i < mRects.size(); i++) {
                                Rect2d rect2d = rect2ds[i];
                                mRects.get(i).set((int) (rect2d.x * ratio), (int) (rect2d.y * ratio),
                                        (int) ((rect2d.x + rect2d.width) * ratio), (int) ((rect2d.y + rect2d.height) * ratio));
                            }

                            EventBus.getDefault().post(mRects);

                        } else {
                            SystemClock.sleep(4);
                        }
                    }

                    mMultiTracker.clear();
                    for (TrackerMOSSE trackerMOSSE : mTrackerMOSSES) {
                        trackerMOSSE.clear();
                    }
                    mTrackerMOSSES.clear();
                }
            }).start();
        } else {
            TrackerMOSSE trackerMOSSE = TrackerMOSSE.create();
            mTrackerMOSSES.add(trackerMOSSE);
            synchronized (MainActivity.class) {
                mMultiTracker.add(trackerMOSSE, mMat, new Rect2d(target.left / ratio, target.top / ratio,
                        target.width() / ratio, target.height() / ratio));
            }
            mRects.add(new Rect(target));
        }
    }


    /**
     * 开启手掌识别
     */
    private void startDetectionPlam() {
        if (!mDetection) {
            mRects.clear();
            mRects.add(new Rect(0, 0, 0, 0));

            new Thread(new Runnable() {

                private Mat mCloneGray;

                @Override
                public void run() {
                    mDetection = true;
                    while (mDetection) {
                        synchronized (MainActivity.class) {
                            mCloneGray = mMatGray.clone();
                        }

                        org.opencv.core.Rect[] rectPalm = mPalmDetector.detectObject(mCloneGray, mObject);

                        if (rectPalm.length > 0) {
                            org.opencv.core.Rect rect = rectPalm[0];
                            mRects.get(0).set((int) (rect.x * ratio), (int) (rect.y * ratio),
                                    (int) ((rect.x + rect.width) * ratio), (int) ((rect.y + rect.height) * ratio));

//                            startSingleTrack(mRects.get(0));
//                            mDetection = false;
                        } else {
                            mRects.get(0).set(0, 0, 0, 0);
                        }

                        mCloneGray.release();
                        EventBus.getDefault().post(mRects);
                    }
                }
            }).start();
        }
    }


    /**
     * 开启手识别
     */
    private void startDetectionHand() {
        if (!mDetection) {
            mRects.clear();
            mRects.add(new Rect(0, 0, 0, 0));

            new Thread(new Runnable() {

                private Mat mClone;

                @Override
                public void run() {
                    mDetection = true;
                    if (detector == null) {
                        try {
                            detector = new TensorFlowObjectDetectionAPIModel(getAssets(), TF_OD_API_MODEL_FILE, TF_OD_API_LABELS_FILE, 320, 240);
                        } catch (IOException e) {

                        }
                    }

                    while (mDetection) {
                        synchronized (MainActivity.class) {
                            mClone = mMat.clone();
                            Imgproc.resize(mClone, mMatTensor, new Size(320, 240));
                        }

                        List<Recognition> recognitions = detector.recognizeImage(mMatTensor);

                        Recognition recognition = recognitions.get(0);
                        final RectF rect = recognition.getLocation();

                        if (rect != null && recognition.getConfidence() >= MINIMUM_CONFIDENCE_TF_OD_API) {
                            mRectView.text = recognition.getTitle() + "\n" + String.valueOf(recognition.getConfidence());
                            mRects.get(0).set((int) (rect.left * ratio * 2), (int) (rect.top * ratio * 2),
                                    (int) (rect.right * ratio * 2), (int) (rect.bottom * ratio * 2));

                            Mat submat = mMatTensor.submat(new org.opencv.core.Rect((int) rect.left, (int) rect.top, (int) rect.width(), (int) rect.height()));

                            //canny边缘检测
                            Imgproc.GaussianBlur(submat, submat, new Size(3, 3), 0);
                            Mat gray = new Mat();
                            Imgproc.cvtColor(submat, gray, Imgproc.COLOR_RGB2GRAY);
                            Mat edges = new Mat();
                            Imgproc.Canny(gray, edges, 50, 150, 3, true);


                            EventBus.getDefault().post(edges);

                        } else {
                            mRects.get(0).set(0, 0, 0, 0);
                        }

                        mClone.release();
                        EventBus.getDefault().post(mRects);
                    }
                }
            }).start();
        }
    }


    private Tracker getTracker() {
        Tracker tracker = null;
        switch (mButtonID) {
            case R.id.rb_kfc:
                tracker = TrackerKCF.create();
                break;

            case R.id.rb_mosse:
                tracker = TrackerMOSSE.create();
                break;

            case R.id.rb_tld:
                tracker = TrackerTLD.create();
                break;

            default:
                tracker = TrackerKCF.create();
                break;
        }

        return tracker;
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    public void refreshRect(ArrayList<Rect> rect) {
        mRectView.setRect(rect);
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    public void submat(Mat mat) {
        Bitmap bitmap = Bitmap.createBitmap(mat.width(), mat.height(), Bitmap.Config.ARGB_8888);
        Mat m = new Mat();
        Imgproc.cvtColor(mat, m, Imgproc.COLOR_GRAY2RGBA);
        Utils.matToBitmap(m, bitmap);
        mPreImageView.setImageBitmap(bitmap);

        mat.release();
        m.release();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        //opencv
        mJava_camera.disableView();
        exitTracking();

        //eventbus
        EventBus.getDefault().unregister(this);
    }

    @Override
    public void onCameraViewStarted(int width, int height) {
        Display display = getWindowManager().getDefaultDisplay();
        Point point = new Point();
        display.getSize(point);
        ratio = point.y / 480.0f;

        mMat = new Mat(480, 640, CvType.CV_8UC3);
        mMatGray = new Mat(480, 640, CvType.CV_8UC1);
        mMatTensor = new Mat(240, 320, CvType.CV_8UC3);
        hsvMat = new Mat(height, width, CvType.CV_8UC4);
        mask = new Mat(height, width, CvType.CV_8UC4);

        mPalmDetector = new ObjectDetector(getApplicationContext(), R.raw.palm, 6, 0.04F, 0.08F, new Scalar(255, 0, 255, 255));
        mObject = new MatOfRect();
    }

    @Override
    public void onCameraViewStopped() {

    }


    //V字检测相关
    Mat hsvMat, mask, contourMat;
    private float[] depths;
    private org.opencv.core.Point[] startPts, endPts, centerPts;
    //开闭操作的大小和中心点
    private Size erodeSize = new Size(3, 3);
    private org.opencv.core.Point erodePoint = new org.opencv.core.Point(1, 1);
    Scalar hsvMin = new Scalar(95, 10, 100);
    Scalar hsvMax = new Scalar(133, 142, 232);
    private ArrayList<Integer> fingerTips = new ArrayList<Integer>();
    private int division = 5;
    private static final int MAX_FINGER_DISTANCE = 20;

    @Override
    public Mat onCameraFrame(CameraBridgeViewBase.CvCameraViewFrame inputFrame) {
        Mat mRgba = inputFrame.rgba();

        synchronized (MainActivity.class) {
            mMatGray = inputFrame.gray();
            Imgproc.cvtColor(mRgba, mMat, Imgproc.COLOR_RGBA2RGB);
        }


        if (mDetectionV) {
            //二值化
            Imgproc.GaussianBlur(mRgba, mRgba, new Size(3, 3), 0);
            Imgproc.cvtColor(mRgba, hsvMat, Imgproc.COLOR_BGR2HSV);
            Core.inRange(hsvMat, hsvMin, hsvMax, mask);

//            //canny边缘检测
//            Mat gray = new Mat();
//            Imgproc.cvtColor(mRgba, gray, Imgproc.COLOR_RGBA2GRAY);
//            Mat edges = new Mat();
//            Imgproc.Canny(gray, edges, 50, 150, 3, true);

            //开闭操作
            Mat element = Imgproc.getStructuringElement(Imgproc.MORPH_RECT, erodeSize, erodePoint);
            Imgproc.morphologyEx(mask, mask, Imgproc.MORPH_OPEN, element);
            Imgproc.morphologyEx(mask, mask, Imgproc.MORPH_CLOSE, element);

            List<MatOfPoint> contours = new ArrayList<MatOfPoint>();
            Mat hierarchy = new Mat();
            Imgproc.findContours(mask, contours, hierarchy, Imgproc.RETR_EXTERNAL, Imgproc.CHAIN_APPROX_SIMPLE);

            for (Iterator<MatOfPoint> iterator = contours.iterator(); iterator.hasNext(); ) {
                MatOfPoint contour = iterator.next();
                //根据面积删除
                double contourArea = Imgproc.contourArea(contour);

                //根据宽高比删除
                RotatedRect rotatedRect = Imgproc.minAreaRect(new MatOfPoint2f(contour.toArray()));
                double ratio = rotatedRect.size.width / rotatedRect.size.height;

                if (contourArea < 3000 || contourArea > 50000
//                        || (ratio > 0.55f && ratio < 1.8f)
                        )
                    iterator.remove();

            }

            for (int j = 0; j < contours.size(); j++) {
                MatOfPoint bigContour = contours.get(j);

                org.opencv.core.Rect rect = Imgproc.boundingRect(bigContour);
                Imgproc.rectangle(mRgba, new org.opencv.core.Point(rect.x, rect.y),
                        new org.opencv.core.Point(rect.x + rect.width, rect.y + rect.height), new Scalar(255, 255, 255));

                //寻找手指
                findFingerTips(bigContour);

                if (fingerTips.size() != 0) {
                    for (int i = 0; i < fingerTips.size(); i++) {

                        Integer index = fingerTips.get(i);
                        Imgproc.line(mRgba, centerPts[index], startPts[index], new Scalar(255, 0, 0), 4);
                        Imgproc.line(mRgba, centerPts[index], endPts[index], new Scalar(255, 0, 0), 4);
                        Imgproc.line(mRgba, startPts[index], endPts[index], new Scalar(0, 255, 0), 2);

                        Imgproc.circle(mRgba, startPts[index], 16, new Scalar(255, 204, 0));
                        Imgproc.circle(mRgba, endPts[index], 16, new Scalar(255, 204, 0));
                        Imgproc.circle(mRgba, centerPts[index], 16, new Scalar(0, 0, 255));

                    }
                }
            }
        }


        mUpdated = true;
        return mRgba;
    }

    private boolean firstFPS = true;
    private Mat mMat;
    private Mat mMatTensor;
    private Mat mMatGray;
    private float ratio = 1;        //屏幕分辨率与视频分辨率之比


    private void findFingerTips(MatOfPoint approxContour) {

        MatOfInt hull = new MatOfInt();

        Imgproc.convexHull(approxContour, hull, false);

        MatOfInt4 defects = new MatOfInt4();
        Imgproc.convexityDefects(approxContour, hull, defects);

        int defectsTotal = (int) defects.total();

        startPts = new org.opencv.core.Point[defectsTotal];
        endPts = new org.opencv.core.Point[defectsTotal];
        centerPts = new org.opencv.core.Point[defectsTotal];
        depths = new float[defectsTotal];

        int[] defectBytes = defects.toArray();
        org.opencv.core.Point[] points = approxContour.toArray();

        for (int i = 0; i < defectsTotal; i++) {
            final int a = defectBytes[4 * i];        //起始点
            final int b = defectBytes[4 * i + 1];    //终止点
            final int c = defectBytes[4 * i + 2];    //凹点
            int d = defectBytes[4 * i + 3] / 256;    //距离

            startPts[i] = points[a];
            endPts[i] = points[b];
            centerPts[i] = points[c];
            depths[i] = d;
        }

        reduceTips(defectsTotal, startPts, endPts, centerPts, depths);
    }

    private void reduceTips(int numPoints, org.opencv.core.Point[] startPts, org.opencv.core.Point[] endPts, org.opencv.core.Point[] centerPts, float[] depths) {
        fingerTips.clear();

        for (int i = 0; i < numPoints; i++) {

            //起始位置距离剔除
            if ((abs(startPts[i].x - endPts[i].x)) < MAX_FINGER_DISTANCE
                    && (abs(startPts[i].y - endPts[i].y) < MAX_FINGER_DISTANCE)) {
                continue;
            }

            //屏幕边界剔除
            if (startPts[i].y > (division - 1) * mask.height() / division ||
                    endPts[i].y > (division - 1) * mask.height() / division) {
                continue;
            }

            //角度剔除
            double degree1 = getDegree(startPts[i], centerPts[i], endPts[i]);
            if (degree1 < 15 || degree1 > 65) continue;

            //两边长度剔除
            int distance1 = getDistance(startPts[i], centerPts[i]);
            int distance2 = getDistance(endPts[i], centerPts[i]);
            if (Math.abs(distance1 - distance2) > depths[i] / 3) continue;

            fingerTips.add(i);
        }

        //只检测到一个V才算是正确的手势
        if (fingerTips.size() != 1) fingerTips.clear();
    }

    public int getDistance(org.opencv.core.Point start, org.opencv.core.Point stop) {
        return (int) Math.sqrt(Math.pow(start.x - stop.x, 2) + Math.pow(start.y - stop.y, 2));
    }

    //根据三个点，计算角度
    public double getDegree(org.opencv.core.Point start, org.opencv.core.Point center, org.opencv.core.Point end) {
        //三角形的两边
        int distance1 = (int) Math.sqrt(Math.pow(start.x - center.x, 2) + Math.pow(start.y - center.y, 2));
        int distance2 = (int) Math.sqrt(Math.pow(end.x - center.x, 2) + Math.pow(end.y - center.y, 2));
        //三角形第三边
        int distance3 = (int) Math.sqrt(Math.pow(start.x - end.x, 2) + Math.pow(start.y - end.y, 2));

        //余弦定理计算角度
        double t = (distance1 * distance1 + distance2 * distance2 - distance3 * distance3) / (2 * distance1 * distance2 + 0f);
        return Math.toDegrees(Math.acos(t));
    }
}
