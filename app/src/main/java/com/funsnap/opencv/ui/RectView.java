package com.funsnap.opencv.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

/**
 * TODO
 * version: V1.0 <描述当前版本功能>
 * fileName: view.RectImageView
 * author: liuping
 * date: 2016/12/6 14:23
 */

public class RectView extends View {

    Rect mRect = new Rect();
    private Paint mPaint;
    public String text;
    public boolean mDetectionFail = false, mClicked = false;
    private Path mPath = new Path();
    private int mLineLength = 20;

    //是否可以触摸绘制框
    public boolean isTouchMode = true;
    private int downX, downY, moveX, moveY;
    private ITouchEvent mTouchListener;
    private Paint mPaintText;

    public RectView(Context context) {
        super(context);
        init();
    }

    private void init() {
        mPaint = new Paint();
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setStrokeWidth(6.0f);
//        mPaint.setPathEffect(new DashPathEffect(new float[]{10, 10}, 0));

        mPaintText = new Paint();
        mPaintText.setStrokeWidth(4.0f);
        mPaintText.setColor(Color.parseColor("#ff0000"));
        mPaintText.setTextSize(40);
    }

    public RectView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (mDetectionFail) {
            mPaint.setColor(Color.RED);
        } else {
            mPaint.setColor(Color.GREEN);
        }

        if (mRect.centerX() != 0) {
            mPaint.setAlpha(255);
            mPath.reset();
            mPath.moveTo(mRect.left, mRect.top + mLineLength);
            mPath.lineTo(mRect.left, mRect.top);
            mPath.lineTo(mRect.left + mLineLength, mRect.top);

            mPath.moveTo(mRect.right - mLineLength, mRect.top);
            mPath.lineTo(mRect.right, mRect.top);
            mPath.lineTo(mRect.right, mRect.top + mLineLength);

            mPath.moveTo(mRect.right, mRect.bottom - mLineLength);
            mPath.lineTo(mRect.right, mRect.bottom);
            mPath.lineTo(mRect.right - mLineLength, mRect.bottom);

            mPath.moveTo(mRect.left + mLineLength, mRect.bottom);
            mPath.lineTo(mRect.left, mRect.bottom);
            mPath.lineTo(mRect.left, mRect.bottom - mLineLength);

            canvas.drawPath(mPath, mPaint);

            mPaint.setAlpha(30);
            canvas.drawRect(mRect,mPaint);
        }

        if (text != null) {
            canvas.drawText(text, mRect.right, mRect.top, mPaintText);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        //如果不能触摸，直接返回
        if (!isTouchMode) {
            return false;
        }

        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                if (!mClicked) return false;

                if (null != mTouchListener) {
                    mTouchListener.onDown();
                }

                downX = (int) event.getX();
                downY = (int) event.getY();
                break;
            case MotionEvent.ACTION_MOVE:
                moveX = (int) event.getX();
                moveY = (int) event.getY();

                if (moveX > downX) {
                    mRect.left = downX;
                    mRect.right = moveX;
                } else {
                    mRect.left = moveX;
                    mRect.right = downX;
                }

                if (moveY > downY) {
                    mRect.top = downY;
                    mRect.bottom = moveY;
                } else {
                    mRect.top = moveY;
                    mRect.bottom = downY;
                }
                invalidate();
                break;

            case MotionEvent.ACTION_UP:
                if (null != mTouchListener) {
                    mTouchListener.onUp(mRect);
                }
                break;
        }
        return true;
    }

    public void setRect(Rect rect) {
        mRect.set(rect);
        invalidate();
    }

    public void reset() {
        mRect.setEmpty();
        text = null;
        invalidate();
    }

    public void setTouchListener(ITouchEvent event) {
        mTouchListener = event;
    }

    public interface ITouchEvent {
        void onDown();

        void onUp(Rect rect);
    }
}
