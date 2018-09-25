package com.funsnap.opencv_contrib;

import android.Manifest;
import android.content.Intent;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v7.app.AppCompatActivity;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;
import android.widget.CompoundButton;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;
import org.opencv.android.CameraBridgeViewBase;
import org.opencv.android.JavaCameraView;
import org.opencv.android.OpenCVLoader;
import org.opencv.core.CvType;
import org.opencv.core.Mat;
import org.opencv.core.MatOfRect2d;
import org.opencv.core.Rect2d;
import org.opencv.imgproc.Imgproc;
import org.opencv.tracking.MultiTracker;
import org.opencv.tracking.Tracker;
import org.opencv.tracking.TrackerGOTURN;
import org.opencv.tracking.TrackerKCF;
import org.opencv.tracking.TrackerMOSSE;
import org.opencv.tracking.TrackerTLD;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

public class MainActivity extends AppCompatActivity implements
        CameraBridgeViewBase.CvCameraViewListener2,
        CompoundButton.OnCheckedChangeListener {

    private static final int REQUEST_CODE = 100;
    private static String[] PERMISSIONS = {
            Manifest.permission.CAMERA
    };

    private JavaCameraView mJava_camera;
    private RectView mRectView;

    private boolean mTracking = false;
    private Rect2d mRect2d = new Rect2d(0, 0, 0, 0);
    private int mButtonID;
    private boolean mUpdated = false;

    MultiTracker mMultiTracker = null;
    MatOfRect2d mMatOfRect2d = null;
    ArrayList<Rect> mRects = new ArrayList<>();
    ArrayList<TrackerMOSSE> mTrackerMOSSES = new ArrayList<>();
    private RadioGroup mRadioGroup;


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

        if (new PermissionsChecker(this).lacksPermissions(PERMISSIONS)) {
            PermissionsActivity.startActivityForResult(this, REQUEST_CODE, PERMISSIONS);
        }

    }

    private void copyFile(final File file) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    InputStream inputStream = getAssets().open(file.getName());
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

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_CODE && resultCode == PermissionsActivity.PERMISSIONS_DENIED) {
            finish();
        }
    }

    private void initView() {
        mJava_camera = findViewById(R.id.java_camera);
        mJava_camera.setCvCameraViewListener(this);

        mRadioGroup = findViewById(R.id.icon_group);

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

        findViewById(R.id.btn_clear).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                exitTracking();
            }
        });

        RadioButton rbGO = findViewById(R.id.rb_go_turn);
        rbGO.setOnCheckedChangeListener(this);
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

                    tracker.init(mMat, new Rect2d(target.left / ratio, target.top / ratio,
                            target.width() / ratio, target.height() / ratio));


                    mRects.clear();
                    mRects.add(new Rect(target));
                    while (mTracking) {

                        if (mUpdated) {
                            mUpdated = false;

                            tracker.update(mMat, mRect2d);
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
            mMultiTracker.add(trackerMOSSE, mMat, new Rect2d(target.left / ratio, target.top / ratio,
                    target.width() / ratio, target.height() / ratio));
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

                            mMultiTracker.update(mMat, mMatOfRect2d);
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
            mMultiTracker.add(trackerMOSSE, mMat, new Rect2d(target.left / ratio, target.top / ratio,
                    target.width() / ratio, target.height() / ratio));
            mRects.add(new Rect(target));
        }
    }


    private Tracker getTracker() {
        Tracker tracker = null;
        switch (mButtonID) {
            case R.id.rb_go_turn:
                tracker = TrackerGOTURN.create();
                break;

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

    @Override
    protected void onDestroy() {
        super.onDestroy();

        //opencv
        mJava_camera.disableView();
        mTracking = false;

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
    }

    @Override
    public void onCameraViewStopped() {

    }

    @Override
    public Mat onCameraFrame(CameraBridgeViewBase.CvCameraViewFrame inputFrame) {
        Mat rgba = inputFrame.rgba();
        Imgproc.cvtColor(rgba, mMat, Imgproc.COLOR_RGBA2RGB);

        mUpdated = true;
        return rgba;
    }

    private Mat mMat;
    private float ratio = 1;
}
