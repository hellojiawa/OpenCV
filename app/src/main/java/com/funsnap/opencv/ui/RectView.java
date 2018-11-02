package com.funsnap.opencv.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;

/**
 * TODO
 * version: V1.0 <描述当前版本功能>
 * fileName: view.RectImageView
 * author: liuping
 * date: 2016/12/6 14:23
 */

public class RectView extends View {

    ArrayList<Rect> mRects = new ArrayList<>();
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

        for (Rect rect : mRects) {
            canvas.drawRect(rect, mPaint);
            if (text != null){
                canvas.drawText(text,rect.right,rect.top,mPaintText);
            }
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

                mRects.add(new Rect());
                break;
            case MotionEvent.ACTION_MOVE:
                moveX = (int) event.getX();
                moveY = (int) event.getY();

                Rect rect = mRects.get(mRects.size() - 1);

                if (moveX > downX) {
                    rect.left = downX;
                    rect.right = moveX;
                } else {
                    rect.left = moveX;
                    rect.right = downX;
                }

                if (moveY > downY) {
                    rect.top = downY;
                    rect.bottom = moveY;
                } else {
                    rect.top = moveY;
                    rect.bottom = downY;
                }
                invalidate();
                break;

            case MotionEvent.ACTION_UP:
                if (null != mTouchListener) {
                    mTouchListener.onUp(mRects.get(mRects.size() - 1));
                }
                break;
        }
        return true;
    }

    public void setRect(ArrayList<Rect> rects) {
        if (mRects.size() < rects.size()){
            for (int i = 0;i <rects.size()-mRects.size();i++){
                mRects.add(new Rect());
            }
        }

        for (int i = 0; i < rects.size(); i++) {
            mRects.get(i).set(rects.get(i));
        }
        invalidate();
    }

    public void clearRects() {
        mRects.clear();
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
