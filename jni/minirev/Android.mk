LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# Enable PIE manually. Will get reset on $(CLEAR_VARS).
LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE -pie

LOCAL_MODULE := minirev

LOCAL_SRC_FILES := \
	minirev.c \

LOCAL_STATIC_LIBRARIES := uthash

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE := minirev-nopie

LOCAL_SRC_FILES := \
	minirev.c \

LOCAL_STATIC_LIBRARIES := uthash

include $(BUILD_EXECUTABLE)
