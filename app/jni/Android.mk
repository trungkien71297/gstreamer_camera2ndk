LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := camera
LOCAL_SRC_FILES := camera.c
LOCAL_LDLIBS := -llog -landroid -lcamera2ndk
include $(BUILD_SHARED_LIBRARY)
