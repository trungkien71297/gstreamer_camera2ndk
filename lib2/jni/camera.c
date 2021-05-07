//
// Created by kien.nguyen on 4/29/2021.
//

#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraError.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCameraWindowType.h>
#include <camera/NdkCaptureRequest.h>
#include <jni.h>
#include <pthread.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <media/NdkImageReader.h>
#include "camera.h"
#include "gstreamer_server.h"
#include <stdio.h>
#include <string.h>
#define TAG "Camera2NDK"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,    TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,     TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,     TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,    TAG, __VA_ARGS__)

static ANativeWindow *theNativeWindow;
static ACameraDevice *cameraDevice;
static ACaptureRequest *captureRequest;
static ACameraOutputTarget *cameraOutputTarget;
static ACameraOutputTarget *iROutputTarget;
static ACaptureSessionOutput *sessionOutput;
static ACaptureSessionOutput *iRSessionOutput;
static ACaptureSessionOutputContainer *captureSessionOutputContainer;
static ACameraCaptureSession *captureSession;
static ACameraDevice_StateCallbacks deviceStateCallbacks;
static ACameraCaptureSession_stateCallbacks captureSessionStateCallbacks;
static AImageReader *imageReader;
static ANativeWindow *imageReaderNativeWindow;
static AImageReader_ImageListener listener;
static JavaVM *java_vm;
static pthread_key_t current_jni_env;

static void camera_device_on_disconnected(void *context, ACameraDevice *device) {
    LOGE("camera_device_on_disconnected");
}

static void camera_device_on_error(void *context, ACameraDevice *device, int error) {
    LOGE("camera_device_on_error");
}

static void capture_session_on_active(void *context, ACameraCaptureSession *session) {
    LOGE("capture_session_on_active");
}

static void capture_session_on_closed(void *context, ACameraCaptureSession *session) {
    LOGE("capture_session_on_closed");
}

static void capture_session_on_ready(void *context, ACameraCaptureSession *session) {
    LOGE("capture_session_on_ready");
}

static void on_image_callback(void *context, AImageReader *reader) {
    AImage *image;
    AImageReader_acquireLatestImage(reader, &image);
    push_data(image);
    if(image != NULL){
        LOGD("AImage_delete");
        AImage_delete(image);
    }
}

static void open_camera(ACameraDevice_request_template request_template) {
    ACameraManager *manager = ACameraManager_create();
    ACameraIdList *cameraIdList = NULL;
    ACameraMetadata *metadata;
    camera_status_t status = ACAMERA_OK;
    media_status_t mediaStatus;
    const char *selectedCamera;
    status = ACameraManager_getCameraIdList(manager, &cameraIdList);
    if (status != ACAMERA_OK) {
        LOGE("ACameraManager_getCameraIdList");
    }

    if (cameraIdList->numCameras < 1) {
        LOGE("numCameras");
    }

    selectedCamera = cameraIdList->cameraIds[0];
    status = ACameraManager_getCameraCharacteristics(manager, selectedCamera, &metadata);
    if (status != ACAMERA_OK) {
        LOGE("ACameraManager_getCameraCharacteristics");
    }

    deviceStateCallbacks.onDisconnected = camera_device_on_disconnected;
    deviceStateCallbacks.onError = camera_device_on_error;

    status = ACameraManager_openCamera(manager, selectedCamera, &deviceStateCallbacks,
                                       &cameraDevice);

    if (status != ACAMERA_OK) {
        LOGE("ACameraManager_openCamera");
    }

    status = ACameraDevice_createCaptureRequest(cameraDevice, request_template, &captureRequest);

    if (status != ACAMERA_OK) {
        LOGE("ACameraDevice_createCaptureRequest");
    }

    mediaStatus = AImageReader_new(1920, 1080, AIMAGE_FORMAT_YUV_420_888, 4, &imageReader);

    if (mediaStatus != AMEDIA_OK) {
        LOGE("AImageReader_new");
    }

    listener.onImageAvailable = on_image_callback;

    mediaStatus = AImageReader_setImageListener(imageReader, &listener);

    if (mediaStatus != AMEDIA_OK) {
        LOGE("AImageReader_setImageListener");
    }

    mediaStatus = AImageReader_getWindow(imageReader, &imageReaderNativeWindow);

    if (mediaStatus != AMEDIA_OK) {
        LOGE("AImageReader_getWindow");
    }

    ACaptureSessionOutputContainer_create(&captureSessionOutputContainer);
    captureSessionStateCallbacks.onActive = capture_session_on_active;
    captureSessionStateCallbacks.onClosed = capture_session_on_closed;
    captureSessionStateCallbacks.onReady = capture_session_on_ready;

    ACameraMetadata_free(metadata);
    ACameraManager_deleteCameraIdList(cameraIdList);
    ACameraManager_delete(manager);
}

static void close_camera() {
    //TODO
}

static void start_preview(JNIEnv *env, jobject thiz, jobject surface) {
    LOGD("start_preview");
    camera_status_t status;
    theNativeWindow = ANativeWindow_fromSurface(env, surface);
    open_camera(TEMPLATE_RECORD);

    ACameraOutputTarget_create(theNativeWindow, &cameraOutputTarget);
    ACameraOutputTarget_create(imageReaderNativeWindow, &iROutputTarget);
    ACaptureRequest_addTarget(captureRequest, cameraOutputTarget);
    ACaptureRequest_addTarget(captureRequest, iROutputTarget);
    ACaptureSessionOutput_create(theNativeWindow, &sessionOutput);
    ACaptureSessionOutput_create(imageReaderNativeWindow, &iRSessionOutput);
    ACaptureSessionOutputContainer_add(captureSessionOutputContainer, sessionOutput);
    ACaptureSessionOutputContainer_add(captureSessionOutputContainer, iRSessionOutput);
    ACameraDevice_createCaptureSession(cameraDevice, captureSessionOutputContainer,
                                       &captureSessionStateCallbacks, &captureSession);


    int32_t fps_range[2];
    fps_range[0] = 30;
    fps_range[1] = 30;
    status = ACaptureRequest_setEntry_i32(captureRequest, ACAMERA_CONTROL_AE_TARGET_FPS_RANGE, 2,
                                          fps_range);

    if (status != ACAMERA_OK) {
        LOGE("ACaptureRequest_setEntry_i32");
    }

    ACameraCaptureSession_setRepeatingRequest(captureSession, NULL, 1, &captureRequest, NULL);
}

static JNIEnv *attach_current_thread(void) {
    JNIEnv *env;
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_6;
    args.name = NULL;
    args.group = NULL;

    if ((*java_vm)->AttachCurrentThread(java_vm, &env, &args) < 0) {
        return NULL;
    }
    return env;
}

static void detach_current_thread(void *env) {
    (*java_vm)->DetachCurrentThread(java_vm);
}

static JNIEnv *get_jni_env(void) {
    JNIEnv *env;
    if ((env = pthread_getspecific(current_jni_env)) == NULL) {
        env = attach_current_thread();
        pthread_setspecific(current_jni_env, env);
    }
    return env;
}

static JNINativeMethod native_methods[] = {
        {"nativeStartPreview", "(Landroid/view/Surface;)V", (void *) start_preview}
};

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;
    java_vm = vm;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("JNI_OnLoad");
        return 0;
    }
    jclass klass = (*env)->FindClass(env, "com/kien/nguyen/lib2/NativeMethods");
    (*env)->RegisterNatives(env, klass, native_methods, 1);
    pthread_key_create(&current_jni_env, detach_current_thread);
    return JNI_VERSION_1_6;
}