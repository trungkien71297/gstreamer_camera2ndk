LOCAL_PATH := $(call my-dir)

#include $(CLEAR_VARS)
#OPENCV_INSTALL_MODULES:=on
#include C:\Prj2\cam2ndk2\sdk\native\jni\OpenCV.mk
#LOCAL_MODULE    := opencv_ndk
#LOCAL_CFLAGS    := -Werror -Wno-write-strings -std=c++11
#LOCAL_SRC_FILES := opencv_lib.cpp
#LOCAL_C_INCLUDES := opencv_lib.h
#LOCAL_LDLIBS    := -llog -landroid -lcamera2ndk -lmediandk
#include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := gstreamer_server
LOCAL_SRC_FILES := gstreamer_server.c
LOCAL_C_INCLUDES := gstreamer_server.h
LOCAL_SHARED_LIBRARIES := gstreamer_android
LOCAL_LDLIBS := -llog -landroid -lmediandk
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := camera
LOCAL_SRC_FILES := camera.c
LOCAL_C_INCLUDES := camera.h
LOCAL_STATIC_LIBRARIES := gstreamer_server
LOCAL_LDLIBS := -llog -landroid -lcamera2ndk -lmediandk
include $(BUILD_SHARED_LIBRARY)

ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif

        ifeq ($(TARGET_ARCH_ABI),armeabi)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm
else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86_64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

        GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/
include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk
#GSTREAMER_PLUGINS         := $(GSTREAMER_PLUGINS_CORE) $(GSTREAMER_PLUGINS_CODECS) $(GSTREAMER_PLUGINS_NET)
        GSTREAMER_PLUGINS         := coreelements app videoconvert androidmedia rtp rtpmanager udp opensles audioconvert audioresample mulaw x264 videofilter vpx inter videoscale rtsp opengl videotestsrc autodetect rtsp videoparsersbad playback libav rtsp sdpelem opengl mpegtsmux pango
        GSTREAMER_EXTRA_DEPS      := gstreamer-rtsp-server-1.0 gstreamer-net-1.0 gstreamer-video-1.0 gstreamer-app-1.0
GSTREAMER_EXTRA_LIBS      := -liconv
        include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk