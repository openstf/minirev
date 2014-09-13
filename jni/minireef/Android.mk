LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := minireef

LOCAL_SRC_FILES := \
	minireef.c \

include $(BUILD_EXECUTABLE)
