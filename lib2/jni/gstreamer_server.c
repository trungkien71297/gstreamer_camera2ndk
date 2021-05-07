//
// Created by kien.nguyen on 4/29/2021.
//

#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <gst/gst.h>
#include <pthread.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <stdlib.h>
#include <gst/app/gstappsrc.h>
#include <stdarg.h>
#include <android/log.h>

#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "gstreamer_server.h"

GST_DEBUG_CATEGORY_STATIC (debug_category);

#if GLIB_SIZEOF_VOID_P == 8
# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(*env)->GetLongField (env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)data)
#else
# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(jint)(*env)->GetLongField (env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)(jint)data)
#endif

#define IS_DEBUG 0
#if IS_DEBUG == 1
#define PRINT(fmt, ...) g_print("GSTREAMER ,%s, " fmt, __func__, ##__VA_ARGS__)
#else
#define PRINT(fmt, ...)
#endif

#define TAG "GSTREAMER"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,    TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,     TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,     TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,    TAG, __VA_ARGS__)

typedef struct _CustomData {
    jobject app;
    GMainLoop *main_loop;
    GstClockTime timestamp;
    GstElement *pipeline;
    GstRTSPMedia *media;
    GstElement *appsrc;
    GstRTSPMediaFactory *factory_video;
    GstRTSPServer *server;
    gchar *pipeline_string;
    gchar *hw_avc_encoder;
    gboolean is_need_data;
    int width;
    int height;
} CustomData;

static pthread_t gst_app_thread;
static pthread_key_t current_jni_env;
static JavaVM *java_vm;
static jfieldID custom_data_field_id;
static jmethodID need_data_callback_method_id;
static CustomData *custom_data;

static JNIEnv *attach_current_thread(void) {
    JNIEnv *env;
    JavaVMAttachArgs args;

    PRINT ("Attaching thread %p", g_thread_self());
    args.version = JNI_VERSION_1_6;
    args.name = NULL;
    args.group = NULL;

    if ((*java_vm)->AttachCurrentThread(java_vm, &env, &args) < 0) {
        GST_ERROR("Failed to attach current thread");
        return NULL;
    }
    return env;
}

static void detach_current_thread(void *env) {
    LOGD("Detaching thread %p", g_thread_self());
    PRINT ("Detaching thread %p", g_thread_self());
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

static void
need_data(GstElement *appsrc, guint unused, CustomData *data) {
    custom_data->is_need_data = TRUE;
    LOGD("Need-data");
}

static void
enough_data(GstElement *appsrc, CustomData *data) {
    custom_data->is_need_data = FALSE;
    LOGD("Enough-data");
}

static void
media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media,
                gpointer user_data) {
    custom_data->media = media;
    custom_data->pipeline = gst_rtsp_media_get_element(media);
    custom_data->appsrc = gst_bin_get_by_name(GST_BIN(custom_data->pipeline), "appsrc");
    gst_util_set_object_arg(G_OBJECT(custom_data->appsrc), "format", "bytes");
    g_object_set(G_OBJECT(custom_data->appsrc), "caps",
                 gst_caps_new_simple("video/x-raw",
                                     "format", G_TYPE_STRING, "NV12",
                                     "width", G_TYPE_INT, 1920,
                                     "height", G_TYPE_INT, 1080,
                                     "framerate", GST_TYPE_FRACTION,
                                     30, 1, NULL), NULL);

    g_signal_connect(custom_data->appsrc, "need-data", (GCallback) need_data, custom_data);
    g_signal_connect(custom_data->appsrc, "enough-data", (GCallback) enough_data, custom_data);
}

static void app_function(void *userdata) {
    GstRTSPServer *server = gst_rtsp_server_new();
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
    ((CustomData *) userdata)->factory_video = gst_rtsp_media_factory_new();
    ((CustomData *) userdata)->server = server;
    g_object_unref(mounts);
    gst_rtsp_server_attach(server, NULL);
    LOGD("stream ready at rtsp://127.0.0.1:8554/video or /audio\n");
}

static void
gst_native_init(JNIEnv *env, jobject thiz, jobject param, int framerate) {
    CustomData *data = g_new0(CustomData, 1);
    custom_data = data;
    SET_CUSTOM_DATA (env, thiz, custom_data_field_id, data);
    data->app = (*env)->NewGlobalRef(env, thiz);
    jclass cls = (*env)->GetObjectClass(env, param);
    jmethodID getWidth = (*env)->GetMethodID(env, cls, "getWidth", "()I");
    jmethodID getHeight = (*env)->GetMethodID(env, cls, "getHeight", "()I");
    data->width = (*env)->CallIntMethod(env, param, getWidth);
    data->height = (*env)->CallIntMethod(env, param, getHeight);
    data->timestamp = 0;
    data->is_need_data = FALSE;
    app_function(data);
    GST_DEBUG_CATEGORY_INIT(debug_category, "gst-rtsp-server", 0, "gst-rtsp-server");
    gst_debug_set_default_threshold(GST_LEVEL_ERROR);
}

static void gst_native_finalize(JNIEnv *env, jobject thiz) {
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data) return;
    LOGD("gst_native_finalize ,Start finalizing");
    gst_rtsp_media_set_pipeline_state(data->media, GST_STATE_NULL);
    g_main_loop_quit(data->main_loop);
    pthread_join(gst_app_thread, NULL);
    g_main_loop_unref(data->main_loop);
    (*env)->DeleteGlobalRef(env, data->app);
    g_free(data);
    SET_CUSTOM_DATA (env, thiz, custom_data_field_id, NULL);
    LOGD("gst_native_finalize ,Done finalizing");
}

static void gst_native_play(JNIEnv *env, jobject thiz) {
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data) {
        LOGD("NO DATA for playing");
        PRINT ("NO DATA for playing");
        return;
    }
    LOGD("Setting state to PLAYING");
    PRINT ("Setting state to PLAYING");
    gst_rtsp_media_set_pipeline_state(data->media, GST_STATE_PLAYING);
}

static void gst_native_pause(JNIEnv *env, jobject thiz) {
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data) return;
    LOGD("Setting state to PAUSED");
    PRINT ("Setting state to PAUSED");
    gst_rtsp_media_set_pipeline_state(data->media, GST_STATE_PAUSED);
}

static void gst_native_stop(JNIEnv *env, jobject thiz) {
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data) return;
    LOGD("Setting state to READY");
    PRINT ("Setting state to READY");
    gst_rtsp_media_set_pipeline_state(data->media, GST_STATE_READY);
}

static void
gst_native_receive_video_data(JNIEnv *env, jobject thiz, jbyteArray array, int width, int height) {

}

static void get_hw_avc_encoder_info_on_device(CustomData *customData) {
    LOGD("get_hw_avc_encoder_info_on_device");
    GList *elelist = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_ENCODER,
                                                           GST_RANK_PRIMARY); // GST_RANK_PRIMARY means priority order
    GstCaps *caps = gst_caps_new_empty_simple("video/x-h264");
    GList *h264_encoders = gst_element_factory_list_filter(elelist, caps, GST_PAD_SRC, FALSE);

    int encoder_cnt = g_list_length(h264_encoders);
    if (encoder_cnt <= 0) {
        LOGE("There is NO HW Encoder for avc(h.264)");
        return;
    }

    GstElement *data = NULL;
    for (int i = 0; i < encoder_cnt; i++) {
        data = (GstElement *) g_list_nth_data(elelist, i);
        LOGD("hw encoder =====> %s", GST_ELEMENT_NAME(data));
    }

    customData->hw_avc_encoder = GST_ELEMENT_NAME(g_list_nth_data(elelist, 0));
    LOGD("Get HW Encoder : %s", customData->hw_avc_encoder);

    gst_caps_unref(caps);
    gst_plugin_feature_list_free(h264_encoders);
    gst_plugin_feature_list_free(elelist);
}

static void set_pipeline_string(CustomData *data) {

    get_hw_avc_encoder_info_on_device(data);
    data->pipeline_string = g_strdup_printf(
            "( appsrc name=appsrc ! videoconvert n-threads=4 name=before_encoder ! %s bitrate=8000000 i-frame-interval=10 name=encoder ! h264parse config-interval=-1 name=after_encoder ! capsfilter caps=video/x-h264,stream-format=byte-stream,alignment=au name=caps_pay ! rtph264pay name=pay0 pt=96 )",
            data->hw_avc_encoder);
}

void push_data(AImage *image) {
    if (custom_data->is_need_data) {
        GstBuffer *buffer = gst_buffer_new ();
        for (int i = 0; i < 3; i++) {
            guint8 *data;
            gint length;
            GstMemory *mem;
            AImage_getPlaneData(image, i, &data, &length);
            mem = gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
                                         data, length, 0, length,
                                         NULL, NULL);
            gst_buffer_append_memory(buffer, mem);
        }
        LOGD("g_signal_emit_by_name");
        GstFlowReturn ret;
        g_signal_emit_by_name(custom_data->appsrc, "push-buffer", buffer, &ret);
        gst_buffer_unref(buffer);
        if(ret != GST_FLOW_OK) {
            g_main_loop_quit(custom_data->main_loop);
        }
    }
}

static void *start_server(CustomData *data) {
    LOGD("GST thread, start_server");
    set_pipeline_string(data);
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(data->server);
    gst_rtsp_media_factory_set_launch(data->factory_video, data->pipeline_string);
    g_signal_connect(data->factory_video, "media-configure", (GCallback) media_configure, data);
    gst_rtsp_mount_points_add_factory(mounts, "/video", data->factory_video);
    data->main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(data->main_loop);
    g_object_unref(mounts);
    return NULL;
}

static void gst_init_server(JNIEnv *env, jobject thiz) {
    LOGD("gst version : %s", gst_version_string());
    LOGD("gst_init_server");
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    pthread_create(&gst_app_thread, NULL, &start_server, data);
}

static jboolean gst_native_class_init(JNIEnv *env, jclass klass) {
    custom_data_field_id = (*env)->GetFieldID(env, klass, "native_custom_data", "J");

    if (!custom_data_field_id || !need_data_callback_method_id) {
        LOGD("The calling class does not implement all necessary interface methods");
        PRINT ("gst-rtsp-server",
               "The calling class does not implement all necessary interface methods");
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

static JNINativeMethod native_methods[] = {
        {"nativeInit",             "(Landroid/util/Size;I)V", (void *) gst_native_init},
        {"nativeFinalize",         "()V",                     (void *) gst_native_finalize},
        {"nativePlay",             "()V",                     (void *) gst_native_play},
        {"nativePause",            "()V",                     (void *) gst_native_pause},
        {"nativeStop",             "()V",                     (void *) gst_native_stop},
        {"nativeClassInit",        "()Z",                     (void *) gst_native_class_init},
        {"nativeReceiveVideoData", "([BII)V",                 (void *) gst_native_receive_video_data},
        {"nativeInitServer",       "()V",                     (void *) gst_init_server},
};

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;
    java_vm = vm;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Could not retrieve JNIEnv");
        return 0;
    }
    jclass klass = (*env)->FindClass(env, "com/kien/nguyen/lib2/GstreamerServer");
    (*env)->RegisterNatives(env, klass, native_methods, G_N_ELEMENTS(native_methods));

    pthread_key_create(&current_jni_env, detach_current_thread);
    if (IS_DEBUG) {
        setenv("GST_DEBUG", "*SCHED*:5",
               1);
    }
    return JNI_VERSION_1_6;
}
