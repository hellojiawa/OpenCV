package com.funsnap.opencv.ui;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
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
import com.thinkjoy.zhthinkjoygesturedetectlib.GestureInfo;
import com.thinkjoy.zhthinkjoygesturedetectlib.ZHThinkjoyGesture;

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
import org.opencv.core.MatOfRect;
import org.opencv.core.MatOfRect2d;
import org.opencv.core.Rect2d;
import org.opencv.core.Scalar;
import org.opencv.core.Size;
import org.opencv.dnn.Dnn;
import org.opencv.dnn.Net;
import org.opencv.imgproc.Imgproc;
import org.opencv.me.ObjectDetector;
import org.opencv.tracking.MultiTracker;
import org.opencv.tracking.Tracker;
import org.opencv.tracking.TrackerKCF;
import org.opencv.tracking.TrackerMOSSE;
import org.opencv.tracking.TrackerTLD;
import org.tensorflow.demo.Classifier;
import org.tensorflow.demo.TFLiteObjectDetectionAPIModel;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

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


    private static final int TF_OD_API_INPUT_SIZE = 224;//300;
    private static final boolean TF_OD_API_IS_QUANTIZED = true;
    private static final String TF_OD_API_MODEL_FILE_LITE = "detect.tflite";
    private static final String TF_OD_API_LABELS_FILE_LITE = "file:///android_asset/labels_hand.txt";


    private static final Float MINIMUM_CONFIDENCE_TF_OD_API = 0.3f;

    private JavaCameraView mJava_camera;
    private RectView mRectView;

    private boolean mTracking = false;
    private boolean mDetection, mClassify = false, mDetectionLite = false;
    private Rect2d mRect2d = new Rect2d(0, 0, 0, 0);
    private int mButtonID;
    private boolean mUpdated = false;

    MultiTracker mMultiTracker = null;
    MatOfRect2d mMatOfRect2d = null;
    Rect mRect = new Rect();
    ArrayList<TrackerMOSSE> mTrackerMOSSES = new ArrayList<>();
    private RadioGroup mRadioGroup;
    private ObjectDetector mPalmDetector;
    private MatOfRect mObject;
    private TensorFlowObjectDetectionAPIModel detector;
    private ImageView mPreImageView;
    private ZHThinkjoyGesture mGesture;
    private Bitmap mBitmap, mBitmapDraw, mBitmapLite;
    private Handler mHandler;
    private HandlerThread mHandlerThread;
    private Classifier detector_lite;
    private float mRatioWidth;
    private float mRatioHeight;

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

        //手势识别
        mGesture = ZHThinkjoyGesture.getInstance(this);
        mGesture.init();

        //tensorflow lite 手势识别
        detector_lite = TFLiteObjectDetectionAPIModel.create(
                getResources().getAssets(),
                TF_OD_API_MODEL_FILE_LITE,
                TF_OD_API_LABELS_FILE_LITE,
                TF_OD_API_INPUT_SIZE,
                TF_OD_API_IS_QUANTIZED);

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
                exitTracking();
            }

            @Override
            public void onUp(Rect target) {
                startSingleTrack(target);
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
                    startDetection();
                } else {
                    mDetection = false;
                }
            }
        });

        Switch switchClassify = findViewById(R.id.switch_classify);
        switchClassify.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    mClassify = true;
                    mPreImageView.setVisibility(View.VISIBLE);
                } else {
                    mClassify = false;
                    mPreImageView.setVisibility(View.GONE);
                }
            }
        });

        Switch switchHand = findViewById(R.id.switch_2);
        switchHand.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    mDetectionLite = true;
                } else {
                    mDetectionLite = false;
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
        mRectView.reset();
        mTracking = false;
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

                    while (mTracking) {

                        if (mUpdated) {
                            mUpdated = false;

                            //克隆一份
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

                            mRect.set((int) (mRect2d.x * ratio), (int) (mRect2d.y * ratio),
                                    (int) ((mRect2d.x + mRect2d.width) * ratio), (int) ((mRect2d.y + mRect2d.height) * ratio));
                            EventBus.getDefault().post(mRect);
                        } else {
                            SystemClock.sleep(4);
                        }

                    }

                    mRect.set(0, 0, 0, 0);
                    EventBus.getDefault().post(mRect);
                    tracker.clear();
                }
            }).start();
        }
    }

//    private void startMultiTrack(final Rect target) {
//        if (!mTracking) {
//            mMultiTracker = MultiTracker.create();
//            TrackerMOSSE trackerMOSSE = TrackerMOSSE.create();
//            mTrackerMOSSES.add(trackerMOSSE);
//
//            synchronized (MainActivity.class) {
//                mMultiTracker.add(trackerMOSSE, mMat, new Rect2d(target.left / ratio, target.top / ratio,
//                        target.width() / ratio, target.height() / ratio));
//            }
//
//            mMatOfRect2d = new MatOfRect2d();
//
//            new Thread(new Runnable() {
//                @Override
//                public void run() {
//                    mTracking = true;
//
//                    while (mTracking) {
//                        if (mUpdated) {     //图像数据有更新
//                            mUpdated = false;
//
//                            synchronized (MainActivity.class) {
//                                mMultiTracker.update(mMat, mMatOfRect2d);
//                            }
//                            Rect2d[] rect2ds = mMatOfRect2d.toArray();
//
//                            for (int i = 0; i < mRects.size(); i++) {
//                                Rect2d rect2d = rect2ds[i];
//                                mRects.get(i).set((int) (rect2d.x * ratio), (int) (rect2d.y * ratio),
//                                        (int) ((rect2d.x + rect2d.width) * ratio), (int) ((rect2d.y + rect2d.height) * ratio));
//                            }
//
//                            EventBus.getDefault().post(mRects);
//
//                        } else {
//                            SystemClock.sleep(4);
//                        }
//                    }
//
//                    mMultiTracker.clear();
//                    for (TrackerMOSSE trackerMOSSE : mTrackerMOSSES) {
//                        trackerMOSSE.clear();
//                    }
//                    mTrackerMOSSES.clear();
//                }
//            }).start();
//        } else {
//            TrackerMOSSE trackerMOSSE = TrackerMOSSE.create();
//            mTrackerMOSSES.add(trackerMOSSE);
//            synchronized (MainActivity.class) {
//                mMultiTracker.add(trackerMOSSE, mMat, new Rect2d(target.left / ratio, target.top / ratio,
//                        target.width() / ratio, target.height() / ratio));
//            }
//            mRects.add(new Rect(target));
//        }
//    }

    List<GestureInfo> mGestureInfos = new ArrayList<>();

    private void startDetection() {
        if (!mDetection) {
            mDetection = true;

            new Thread(new Runnable() {

                @Override
                public void run() {
                    while (mDetection) {
                        mGestureInfos.clear();
                        mGesture.gestureDetect(mBitmap, mGestureInfos);

                        if (mGestureInfos.size() > 0) {
                            GestureInfo info = mGestureInfos.get(0);
                            mRect.set((int) (info.gestureRectangle[0].x * ratio), (int) (info.gestureRectangle[0].y * ratio),
                                    (int) (info.gestureRectangle[1].x * ratio), (int) (info.gestureRectangle[1].y * ratio));

                            switch (info.type) {
                                case 0:
                                    mRectView.text = "FIST";
                                    break;
                                case 1:
                                    mRectView.text = "PALM";
                                    break;
                                case 2:
                                    mRectView.text = "OK";
                                    break;
                                case 3:
                                    mRectView.text = "YEAH";
                                    break;
                                case 4:
                                    mRectView.text = "ONE FINGER";
                                    break;
                                default:
                                    mRectView.text = "NO GESTURE";
                                    break;
                            }

                        } else {
                            mRect.set(0, 0, 0, 0);
                        }

                        EventBus.getDefault().post(mRect);
                    }
                }
            }).start();
        }
    }

    /**
     * 开启手掌识别
     */
    private void startDetectionPalm() {

        if (!mDetection) {
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
                            mRect.set((int) (rect.x * ratio), (int) (rect.y * ratio),
                                    (int) ((rect.x + rect.width) * ratio), (int) ((rect.y + rect.height) * ratio));

//                            startSingleTrack(mRects.get(0));
//                            mDetection = false;
                        } else {
                            mRect.set(0, 0, 0, 0);
                        }

                        mCloneGray.release();
                        EventBus.getDefault().post(mRect);
                    }

                    mRect.set(0, 0, 0, 0);
                    EventBus.getDefault().post(mRect);
                }
            }).start();
        }
    }

    /**
     * 开启手识别
     */
    private void startDetectionHand() {
        if (!mDetection) {
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
                            mRect.set((int) (rect.left * ratio * 2), (int) (rect.top * ratio * 2),
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
                            mRect.set(0, 0, 0, 0);
                        }

                        mClone.release();
                        EventBus.getDefault().post(mRect);
                    }

                    mRect.set(0, 0, 0, 0);
                    EventBus.getDefault().post(mRect);
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
    public void refreshRect(Rect rect) {
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

    private static String getPath(String file, Context context) {
        AssetManager assetManager = context.getAssets();
        BufferedInputStream inputStream = null;
        try {
            // Read data from assets.
            inputStream = new BufferedInputStream(assetManager.open(file));
            byte[] data = new byte[inputStream.available()];
            inputStream.read(data);
            inputStream.close();
            // Create copy file in storage.
            File outFile = new File(context.getFilesDir(), file);
            FileOutputStream os = new FileOutputStream(outFile);
            os.write(data);
            os.close();
            // Return a path to file which may be read in common way.
            return outFile.getAbsolutePath();
        } catch (IOException ex) {
            String message = ex.getMessage();
        }
        return "";
    }

    @Override
    protected void onResume() {
        super.onResume();

        mHandlerThread = new HandlerThread("back");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    @Override
    protected void onPause() {
        super.onPause();

        mHandlerThread.quitSafely();
        mHandler = null;
    }

    private synchronized void runInBackground(final Runnable r) {
        if (mHandler != null) mHandler.post(r);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        //opencv
        mJava_camera.disableView();
        exitTracking();
        mDetection = false;
        mDetectionLite = false;
        mClassify = false;

        //hand
//        mGesture.release();

        //eventbus
        EventBus.getDefault().unregister(this);
    }

    @Override
    public void onCameraViewStarted(int width, int height) {
        Display display = getWindowManager().getDefaultDisplay();
        Point point = new Point();
        display.getSize(point);
        ratio = point.y / 480.0f;

        mRatioWidth = (width + 0f) / TF_OD_API_INPUT_SIZE * 3;
        mRatioHeight = (height + 0f) / TF_OD_API_INPUT_SIZE * 3;

        mMat = new Mat(480, 640, CvType.CV_8UC3);
        mMatDraw = new Mat(480, 640, CvType.CV_8UC4);
        mMatGray = new Mat(480, 640, CvType.CV_8UC1);
        mMatTensor = new Mat(240, 320, CvType.CV_8UC3);
        mMatLite = new Mat(TF_OD_API_INPUT_SIZE, TF_OD_API_INPUT_SIZE, CvType.CV_8UC4);

        mBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        mBitmapDraw = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        mBitmapLite = Bitmap.createBitmap(TF_OD_API_INPUT_SIZE, TF_OD_API_INPUT_SIZE, Bitmap.Config.ARGB_8888);

        mPalmDetector = new ObjectDetector(getApplicationContext(), R.raw.haarcascade_fullbody, 4, 0.04F, 0.08F, new Scalar(255, 0, 255, 255));
        mObject = new MatOfRect();

        String proto = getPath("MobileNetSSD_deploy.prototxt", this);
        String weights = getPath("MobileNetSSD_deploy.caffemodel", this);
        net = Dnn.readNetFromCaffe(proto, weights);
    }

    @Override
    public void onCameraViewStopped() {

    }


    @Override
    public Mat onCameraFrame(CameraBridgeViewBase.CvCameraViewFrame inputFrame) {
        Mat mRgba = inputFrame.rgba();

        synchronized (MainActivity.class) {
            mMatGray = inputFrame.gray();
            Imgproc.cvtColor(mRgba, mMat, Imgproc.COLOR_RGBA2RGB);
        }

        if (mDetection) {
            Utils.matToBitmap(mRgba, mBitmap);
        }

        //利用tensorflo分类物体
        if (mClassify) {
            if (!detection) {
                detection = true;
                runInBackground(new Runnable() {
                    @Override
                    public void run() {
                        // Forward image through network.
                        Mat blob = Dnn.blobFromImage(mMat, IN_SCALE_FACTOR,
                                new Size(IN_WIDTH, IN_HEIGHT),
                                new Scalar(MEAN_VAL, MEAN_VAL, MEAN_VAL), false, false);

                        net.setInput(blob);
                        Mat detections = net.forward();

                        int cols = mMat.cols();
                        int rows = mMat.rows();
                        mMatDraw.setTo(mScalar);

                        //矩阵变换
                        detections = detections.reshape(1, (int) detections.total() / 7);
                        for (int i = 0; i < detections.rows(); ++i) {
                            double confidence = detections.get(i, 2)[0];
                            if (confidence > THRESHOLD) {
                                int classId = (int) detections.get(i, 1)[0];
                                int xLeftBottom = (int) (detections.get(i, 3)[0] * cols);
                                int yLeftBottom = (int) (detections.get(i, 4)[0] * rows);
                                int xRightTop = (int) (detections.get(i, 5)[0] * cols);
                                int yRightTop = (int) (detections.get(i, 6)[0] * rows);

                                // Draw rectangle around detected object.
                                Imgproc.rectangle(mMatDraw, new org.opencv.core.Point(xLeftBottom, yLeftBottom),
                                        new org.opencv.core.Point(xRightTop, yRightTop),
                                        new Scalar(0, 255, 0));
                                String label = classNames[classId] + ": " + String.format("%.5f", confidence);
                                int[] baseLine = new int[1];
                                Size labelSize = Imgproc.getTextSize(label, Core.FONT_HERSHEY_SIMPLEX, 0.5, 2, baseLine);
                                // Draw background for label.
                                Imgproc.rectangle(mMatDraw, new org.opencv.core.Point(xLeftBottom, yLeftBottom - labelSize.height),
                                        new org.opencv.core.Point(xLeftBottom + labelSize.width, yLeftBottom + baseLine[0]),
                                        new Scalar(0, 0, 0, 255), Core.FILLED);
                                // Write class name and confidence.
                                Imgproc.putText(mMatDraw, label, new org.opencv.core.Point(xLeftBottom, yLeftBottom),
                                        Core.FONT_HERSHEY_SIMPLEX, 0.5, new Scalar(255, 255, 255));

                            }
                        }

                        Utils.matToBitmap(mMatDraw, mBitmapDraw);
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                mPreImageView.setImageBitmap(mBitmapDraw);
                            }
                        });

                        detection = false;
                    }
                });
            }
        }

        //利用tensorflowLite识别手势
        if (mDetectionLite) {
            if (!detection) {
                detection = true;
                final Mat clone = mRgba.clone();

                runInBackground(new Runnable() {
                    @Override
                    public void run() {
                        Mat ss = clone.submat(new org.opencv.core.Rect(208, 128, 224, 224));

//                        Imgproc.resize(clone, mMatLite, mMatLite.size());
                        Utils.matToBitmap(ss, mBitmapLite);

                        final List<Classifier.Recognition> results = detector_lite.recognizeImage(mBitmapLite);
                        Classifier.Recognition recognition = results.get(0);

                        if (recognition.getConfidence() > 0.4) {
                            mRectView.text = recognition.getTitle();
                            RectF rectF = recognition.getLocation();

//                            mRect.set((int) (rectF.left * mRatioWidth), (int) (rectF.top * mRatioHeight),
//                                    (int) (rectF.right * mRatioWidth), (int) (rectF.bottom * mRatioHeight));

                            mRect.set((int) (rectF.left * 3 + 208), (int) (rectF.top * 3 + 128),
                                    (int) (rectF.right * 3 + 208), (int) (rectF.bottom * 3 + 128));
                        } else {
                            mRect.setEmpty();
                        }

                        EventBus.getDefault().post(mRect);
                        clone.release();
                        detection = false;
                    }
                });
            }
        }

        mUpdated = true;
        return mRgba;
    }

    private Mat mMat, mMatDraw;
    private Mat mMatTensor, mMatLite;
    private Mat mMatGray;
    private Scalar mScalar = new Scalar(0, 0, 0, 0);
    private float ratio = 1;        //屏幕分辨率与视频分辨率之比
    private boolean detection = false;

    private Net net;
    final int IN_WIDTH = 320;
    final int IN_HEIGHT = 240;

    final double IN_SCALE_FACTOR = 0.007843;
    final double MEAN_VAL = 127.5;
    final double THRESHOLD = 0.4;
    private static final String[] classNames = {"background",
            "aeroplane", "bicycle", "bird", "boat",
            "bottle", "bus", "car", "cat", "chair",
            "cow", "diningtable", "dog", "horse",
            "motorbike", "person", "pottedplant",
            "sheep", "sofa", "train", "tvmonitor"};
}
