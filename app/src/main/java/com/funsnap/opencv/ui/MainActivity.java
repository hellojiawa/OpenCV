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
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.Switch;

import com.funsnap.opencv.R;
import com.funsnap.opencv.permission.PerActivity;
import com.funsnap.opencv.permission.PerChecker;
import com.funsnap.opencv.utils.Classifier;
import com.funsnap.opencv.utils.TFLiteObjectDetectionAPIModel;
import com.thinkjoy.zhthinkjoygesturedetectlib.GestureInfo;
import com.thinkjoy.zhthinkjoygesturedetectlib.ZHThinkjoyGesture;
import com.utility.FollowUtil;
import com.utility.MatUtil;

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
import org.opencv.tracking.TrackerMOSSE;
import org.opencv.tracking.TrackerTLD;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

public class MainActivity extends AppCompatActivity implements
        CameraBridgeViewBase.CvCameraViewListener2 {

    private static final int REQUEST_CODE = 100;
    private static String[] PERMISSIONS = {
            Manifest.permission.CAMERA,
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
    };

    private static final String TF_OD_API_MODEL_FILE = "file:///android_asset/ssd5_optimized_inference_graph.pb";
    private static final String TF_OD_API_LABELS_FILE = "hand_label_map.txt";


    private static final int TF_OD_API_INPUT_SIZE = 224;//300;
    private static final boolean TF_OD_API_IS_QUANTIZED = true;
    private static final String TF_OD_API_MODEL_FILE_LITE = "detect.tflite";
    private static final String TF_OD_API_LABELS_FILE_LITE = "file:///android_asset/labels_hand.txt";


    private static final Float MINIMUM_CONFIDENCE_TF_OD_API = 0.5f;

    private JavaCameraView mJava_camera;
    private RectView mRectView;

    private boolean mTracking = false;
    private boolean mDetection, mClassify = false, mDetectionLite = false;
    private Rect2d mRect2d = new Rect2d(0, 0, 0, 0);
    private boolean mUpdated = false;

    MultiTracker mMultiTracker = null;
    MatOfRect2d mMatOfRect2d = null;
    Rect mRect = new Rect();
    LinkedList<Rect> mRects = new LinkedList<>();
    ArrayList<TrackerMOSSE> mTrackerMOSSES = new ArrayList<>();
    private ObjectDetector mPalmDetector;
    private MatOfRect mObject;
    private ImageView mPreImageView;
    private ZHThinkjoyGesture mGesture;
    private Bitmap mBitmap, mBitmapDraw, mBitmapLite;
    private Handler mHandler;
    private HandlerThread mHandlerThread;
    private Classifier detector_lite;
    private float mRatioWidth;
    private float mRatioHeight;

    private boolean followFail, mLockTarget;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);

        initView();

        //opencv
        OpenCVLoader.initDebug();
        mJava_camera.enableFpsMeter();
        mJava_camera.enableView();

        //eventbus
        EventBus.getDefault().register(this);

        //手势识别
        mGesture = ZHThinkjoyGesture.getInstance();
        mGesture.init();

        //tensorflow lite 手势识别
        try {
            detector_lite = TFLiteObjectDetectionAPIModel.create(
                    getResources().getAssets(),
                    TF_OD_API_MODEL_FILE_LITE,
                    TF_OD_API_LABELS_FILE_LITE,
                    TF_OD_API_INPUT_SIZE,
                    TF_OD_API_IS_QUANTIZED);
        } catch (IOException e) {
            e.printStackTrace();
        }

        if (new PerChecker(this).lacksPermissions(PERMISSIONS)) {
            PerActivity.startActivityForResult(this, REQUEST_CODE, PERMISSIONS);
        }
        Mat mat = new Mat(100, 100, CvType.CV_8U);
        MatUtil.getMat(new byte[100 * 100 * 3 / 2], 100, 100, mat.nativeObj);
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

        mPreImageView = findViewById(R.id.image_view);

        mRectView = findViewById(R.id.rect_view);
        mRectView.setTouchListener(new RectView.ITouchEvent() {
            @Override
            public void onDown() {
                mRectView.reset();
                mTracking = false;
            }

            @Override
            public void onUp(Rect target) {
                startTracking(target);
//                getTarget(target);
            }
        });

        CheckBox cbCamera = findViewById(R.id.cb_camera);
        cbCamera.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    mJava_camera.setCameraIndex(CameraBridgeViewBase.CAMERA_ID_FRONT);
                } else {
                    mJava_camera.setCameraIndex(CameraBridgeViewBase.CAMERA_ID_BACK);
                }

                mJava_camera.disableView();
                mJava_camera.enableView();
            }
        });

        Switch switch_follow = findViewById(R.id.switch_follow);
        switch_follow.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    mRectView.mClicked = true;
                } else {
                    mRectView.mClicked = false;
                    mRectView.reset();
                    mTracking = false;
                }
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
    }


    /*
     * byte[] data保存的是纯RGB的数据，而非完整的图片文件数据
     */
    static public Bitmap createMyBitmap(byte[] data, int width, int height) {
        int[] colors = convertByteToColor(data);
        if (colors == null) {
            return null;
        }

        Bitmap bmp = null;

        try {
            bmp = Bitmap.createBitmap(colors, 0, width, width, height,
                    Bitmap.Config.ARGB_8888);
        } catch (Exception e) {
            // TODO: handle exception

            return null;
        }

        return bmp;
    }


    /*
     * 将RGB数组转化为像素数组
     */
    private static int[] convertByteToColor(byte[] data) {
        int size = data.length;
        if (size == 0) {
            return null;
        }


        // 理论上data的长度应该是3的倍数，这里做个兼容
        int arg = 0;
        if (size % 3 != 0) {
            arg = 1;
        }

        int[] color = new int[size / 3 + arg];
        int red, green, blue;


        if (arg == 0) {                                    //  正好是3的倍数
            for (int i = 0; i < color.length; ++i) {

                color[i] = (data[i * 3] << 16 & 0x00FF0000) |
                        (data[i * 3 + 1] << 8 & 0x0000FF00) |
                        (data[i * 3 + 2] & 0x000000FF) |
                        0xFF000000;
            }
        } else {                                        // 不是3的倍数
            for (int i = 0; i < color.length - 1; ++i) {
                color[i] = (data[i * 3] << 16 & 0x00FF0000) |
                        (data[i * 3 + 1] << 8 & 0x0000FF00) |
                        (data[i * 3 + 2] & 0x000000FF) |
                        0xFF000000;
            }

            color[color.length - 1] = 0xFF000000;                    // 最后一个像素用黑色填充
        }

        return color;
    }

    private void startTracking(final Rect target) {
        if (!mTracking) {
            mTracking = true;

            //锁定目标
            Rect rectIn = new Rect((int) (target.left / ratio), (int) (target.top / ratio),
                    (int) (target.right / ratio), (int) (target.bottom / ratio));
            final Rect rectOut = new Rect();

            Mat matRgba = new Mat(mMat.size(), CvType.CV_8UC4);
            Imgproc.cvtColor(mMat, matRgba, Imgproc.COLOR_RGB2RGBA);
            Utils.matToBitmap(matRgba, mBitmap);
            FollowUtil.getTarget(mBitmap, mBitmap.getWidth(), mBitmap.getHeight(), rectIn, rectOut);


            //mosse 跟踪
            new Thread(new Runnable() {
                private int errorCount = 0;

                @Override
                public void run() {
                    Tracker trackerMosse = TrackerMOSSE.create();

                    //初始化目标
                    synchronized (MainActivity.class) {
                        trackerMosse.init(mMat, new Rect2d(rectOut.left, rectOut.top,
                                rectOut.width(), rectOut.height()));
                    }

                    while (mTracking) {
                        if (mUpdated) {
                            mUpdated = false;

                            if (followFail) {
                                //TLD识别到目标，则初始化
                                if (mLockTarget) {
                                    followFail = false;
                                    mLockTarget = false;
                                    errorCount = 0;

                                    trackerMosse.clear();
                                    trackerMosse = TrackerMOSSE.create();
                                    synchronized (MainActivity.class) {
                                        Rect rect = mRects.getLast();
                                        trackerMosse.init(mMat, new Rect2d(rect.left / ratio, rect.top / ratio,
                                                rect.width() / ratio, rect.height() / ratio));
                                    }

                                    mRects.clear();
                                    Log.d("liuping", "重新初始化:" + "");
                                } else {
                                    SystemClock.sleep(5);
                                }

                                mRectView.mDetectionFail = true;
                            } else {
                                synchronized (MainActivity.class) {
                                    trackerMosse.update(mMat, mRect2d);
                                }

                                if (mRect2d.width > 0) {
                                    mRect.set((int) (mRect2d.x * ratio), (int) (mRect2d.y * ratio),
                                            (int) ((mRect2d.x + mRect2d.width) * ratio), (int) ((mRect2d.y + mRect2d.height) * ratio));
                                } else {
                                    if (++errorCount > 10) {
                                        followFail = true;
                                    }
                                }

                                mRectView.mDetectionFail = false;
                            }

                            EventBus.getDefault().post(mRect);
                        } else {
                            SystemClock.sleep(5);
                        }

                    }

                    mRect.setEmpty();
                    EventBus.getDefault().post(mRect);
                    trackerMosse.clear();
                }
            }).start();

            //当mosse跟丢的时候，利用TLC重新初始化跟踪
            new Thread(new Runnable() {
                @Override
                public void run() {

                    Tracker trackerTLD = TrackerTLD.create();
                    synchronized (MainActivity.class) {
                        trackerTLD.init(mMat, new Rect2d(rectOut.left, rectOut.top,
                                rectOut.width(), rectOut.height()));
                    }

                    while (mTracking) {
                        Mat clone = mMat.clone();
                        trackerTLD.update(clone, mRect2d);

                        Rect rect = new Rect((int) (mRect2d.x * ratio), (int) (mRect2d.y * ratio),
                                (int) ((mRect2d.x + mRect2d.width) * ratio), (int) ((mRect2d.y + mRect2d.height) * ratio));

                        mRects.addLast(rect);
                        if (mRects.size() > 3) mRects.removeFirst();
                        if (mRects.size() == 3 && lockTarget(mRects)) {
                            mLockTarget = true;
                        } else {
                            mLockTarget = false;
                        }

                        clone.release();
                    }
                    trackerTLD.clear();
                }
            }).start();
        }
    }

    //是否锁定目标
    private boolean lockTarget(LinkedList<Rect> list) {
        int xMin = 1000, xMax = 0, yMin = 1000, yMax = 0;

        for (Rect rect : list) {
            int centerX = rect.centerX();
            int centerY = rect.centerY();
            if (centerX < xMin) xMin = centerX;
            if (centerX > xMax) xMax = centerX;

            if (centerY < yMin) yMin = centerY;
            if (centerY > yMax) yMax = centerY;
        }

        int t = (xMax - xMin) * (yMax - yMin);
        return t < 1000;
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

//                            startTracking(mRects.get(0));
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

//    /**
//     * 开启手识别
//     */
//    private void startDetectionHand() {
//        if (!mDetection) {
//            new Thread(new Runnable() {
//
//                private Mat mClone;
//
//                @Override
//                public void run() {
//                    mDetection = true;
//                    if (detector == null) {
//                        try {
//                            detector = new TensorFlowObjectDetectionAPIModel(getAssets(), TF_OD_API_MODEL_FILE, TF_OD_API_LABELS_FILE, 320, 240);
//                        } catch (IOException e) {
//
//                        }
//                    }
//
//                    while (mDetection) {
//                        synchronized (MainActivity.class) {
//                            mClone = mMat.clone();
//                            Imgproc.resize(mClone, mMatTensor, new Size(320, 240));
//                        }
//
//                        List<Recognition> recognitions = detector.recognizeImage(mMatTensor);
//
//                        Recognition recognition = recognitions.get(0);
//                        final RectF rect = recognition.getLocation();
//
//                        if (rect != null && recognition.getConfidence() >= MINIMUM_CONFIDENCE_TF_OD_API) {
//                            mRectView.text = recognition.getTitle() + "\n" + String.valueOf(recognition.getConfidence());
//                            mRect.set((int) (rect.left * ratio * 2), (int) (rect.top * ratio * 2),
//                                    (int) (rect.right * ratio * 2), (int) (rect.bottom * ratio * 2));
//
//                            Mat submat = mMatTensor.submat(new org.opencv.core.Rect((int) rect.left, (int) rect.top, (int) rect.width(), (int) rect.height()));
//
//
//                        } else {
//                            mRect.setEmpty();
//                        }
//
//                        mClone.release();
//                        EventBus.getDefault().post(mRect);
//                    }
//
//                    mRect.set(0, 0, 0, 0);
//                    EventBus.getDefault().post(mRect);
//                }
//            }).start();
//        }
//    }

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
        mTracking = false;
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
                        Imgproc.resize(clone, mMatLite, mMatLite.size());
                        Utils.matToBitmap(mMatLite, mBitmapLite);

                        final List<Classifier.Recognition> results = detector_lite.recognizeImage(mBitmapLite);
                        Classifier.Recognition recognition = results.get(0);

                        if (recognition.getConfidence() > 0.3) {
                            mRectView.text = recognition.getTitle();
                            RectF rectF = recognition.getLocation();
//
//                            mRect.set((int) (rectF.left * mRatioWidth), (int) (rectF.top * mRatioHeight),
//                                    (int) (rectF.right * mRatioWidth), (int) (rectF.bottom * mRatioHeight));

                            mRect.set((int) ((rectF.left + 208) * ratio), (int) ((rectF.top + 128) * ratio),
                                    (int) ((rectF.right + 208) * ratio), (int) ((rectF.bottom + 128) * ratio));
                            Log.d("liuping", ":" + "识别成功");
                        } else {
                            mRect.setEmpty();
                            Log.d("liuping", "识别失败:" + "");
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
