package com.thinkjoy.zhthinkjoygesturedetectlib;

import android.os.Parcel;
import android.os.Parcelable;


/**
 * TODO
 * version: V1.0 <描述当前版本功能>
 * fileName: thinkjoy.zhthinkjoygesturedetectlib.GestureInfo
 * author: liuping
 * date: 2019/9/20 10:49
 */
public class GestureInfo implements Parcelable {
    public Point[] gestureRectangle;
    public int type;
    public static final int FIST = 0;
    public static final int PALM = 1;
    public static final int OK_GESTURE = 2;
    public static final int V_GESTURE = 3;
    public static final int INDEX_FINGER = 4;
    public static final Creator<GestureInfo> CREATOR = new Creator<GestureInfo>() {
        public GestureInfo createFromParcel(Parcel in) {
            return new GestureInfo(in);
        }

        public GestureInfo[] newArray(int size) {
            return new GestureInfo[size];
        }
    };

    public GestureInfo() {
        this.gestureRectangle = new Point[2];
        this.type = -1;
    }

    protected GestureInfo(Parcel in) {
        this.type = in.readInt();
        this.gestureRectangle = new Point[2];
        this.gestureRectangle[0].x = in.readFloat();
        this.gestureRectangle[0].y = in.readFloat();
        this.gestureRectangle[1].x = in.readFloat();
        this.gestureRectangle[1].y = in.readFloat();
    }

    public void setGesture(float[] points, int type) {
        this.gestureRectangle[0].x = points[0];
        this.gestureRectangle[0].y = points[1];
        this.gestureRectangle[1].x = points[2];
        this.gestureRectangle[1].y = points[3];
        this.type = type;
    }

    public GestureInfo(float[] points, int shape) {
        this.gestureRectangle = new Point[2];
        this.gestureRectangle[0] = new Point(points[1], points[0]);
        this.gestureRectangle[1] = new Point(points[3], points[2]);
        this.type = shape;
    }

    public int describeContents() {
        return 0;
    }

    public void writeToParcel(Parcel parcel, int i) {
        parcel.writeInt(this.type);
        parcel.writeFloat(this.gestureRectangle[0].x);
        parcel.writeFloat(this.gestureRectangle[0].y);
        parcel.writeFloat(this.gestureRectangle[1].x);
        parcel.writeFloat(this.gestureRectangle[1].y);
    }
}
