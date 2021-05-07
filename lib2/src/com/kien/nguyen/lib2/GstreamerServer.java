package com.kien.nguyen.lib2;

import android.content.Context;
import android.util.Size;

import org.freedesktop.gstreamer.GStreamer;

public class GstreamerServer {
    private native void nativeInit(Size size, int framerate);

    private native void nativePlay();

    private native void nativePause();

    private native void nativeStop();

    private native void nativeFinalize();

    private static native boolean nativeClassInit();

    private long native_custom_data;

    private native void nativeReceiveVideoData(byte[] byteArray, int width, int height);

    private native void nativeInitServer();

    public GstreamerServer(Context context){
        try {
            GStreamer.init(context);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    static {
        System.loadLibrary("gstreamer_android");  //TODO: go to infra layer
        System.loadLibrary("gstreamer_server");
        nativeClassInit();
    }

    public void initServer(){
        nativeInitServer();
    }
    public void initCustomdata(Size size, int fr) {nativeInit(size, fr);}
}
