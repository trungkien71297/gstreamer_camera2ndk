package com.kien.nguyen.cam2ndk

import android.Manifest
import android.content.pm.PackageManager
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.util.Size
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.core.app.ActivityCompat
import com.kien.nguyen.lib2.GstreamerServer
import com.kien.nguyen.lib2.NativeMethods

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {
    private lateinit var nativeMethods: NativeMethods
    private lateinit var gstreamerServer: GstreamerServer
    private lateinit var gStreamerSurfaceView: SurfaceView
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        checkGrant()
        nativeMethods = NativeMethods()
        gstreamerServer =
            GstreamerServer(
                application
            )

        gStreamerSurfaceView = findViewById(R.id.sf_view)
        gStreamerSurfaceView.holder.addCallback(this)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "surfaceChanged")
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceDestroyed")
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceCreated")
        gstreamerServer.initCustomdata(Size(1920,1080), 30)
        nativeMethods.startPreview(holder.surface)
        gstreamerServer.initServer()
    }

    private fun checkGrant() {
        for (permission in PERMISSION_LIST) {
            val grantResult = ActivityCompat.checkSelfPermission(this, permission)
            Log.d("MainFragment", "grant result:$grantResult")
            if (grantResult == PackageManager.PERMISSION_DENIED) {
                ActivityCompat.requestPermissions(
                    this,
                    PERMISSION_LIST,
                    REQUEST_PERMISSION_CODE
                )
            }
        }
    }

    companion object {
        const val TAG = "MainActivity"
        private val PERMISSION_LIST = arrayOf(
            Manifest.permission.CAMERA,
            Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.ACCESS_NETWORK_STATE, Manifest.permission.ACCESS_WIFI_STATE,
            Manifest.permission.INTERNET, Manifest.permission.RECORD_AUDIO,
            Manifest.permission.CHANGE_WIFI_MULTICAST_STATE,
            Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION
        )
        private const val REQUEST_PERMISSION_CODE = 0
    }
}