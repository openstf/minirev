LOCAL_PATH := $(abspath $(call my-dir))

include $(CLEAR_VARS)

LOCAL_MODULE := uthash

LOCAL_EXPORT_C_INCLUDES = \
    $(LOCAL_PATH)/source/src

include $(BUILD_STATIC_LIBRARY)
