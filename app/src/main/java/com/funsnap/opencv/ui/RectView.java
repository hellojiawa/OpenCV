package com.funsnap.opencv.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
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

    //是否可以触摸绘制框
    public boolean isTouchMode = true;
    private int downX;
    private int downY;
    private int moveX;
    private int moveY;
    private ITouchEvent mTouchListener;
    private Paint mPaintText;

    public RectView(Context context) {
        super(context);
        init();
    }

    private void init() {
        mPaint = new Paint();
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(4.0f);
        mPaint.setColor(Color.parseColor("#00ffff"));
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

        canvas.drawRect(mRect, mPaint);
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
