LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := minirev

LOCAL_SRC_FILES := \
	minirev.c \

LOCAL_STATIC_LIBRARIES := uthash

include $(BUILD_EXECUTABLE)
