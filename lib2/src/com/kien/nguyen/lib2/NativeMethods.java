package com.kien.nguyen.lib2;

import android.view.Surface;

public class NativeMethods {
    static {
        System.loadLibrary("camera");
    }
    private native void nativeStartPreview(Surface surface);

    public void startPreview(Surface sf) {
        nativeStartPreview(sf);
    }
}
