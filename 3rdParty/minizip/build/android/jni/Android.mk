LOCAL_PATH := $(call my-dir)/../../..

include $(CLEAR_VARS)

LOCAL_MODULE := minizip
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

include $(LOCAL_PATH)/../../cflags.mk

define recursive_wildcard
  $(wildcard $(1)) $(foreach e, $(wildcard $(1)/*), $(call recursive_wildcard, $(e)))
endef

# Include headers.
HEADER_LIST := $(filter %.h, $(call recursive_wildcard, $(LOCAL_PATH)))
LOCAL_HEADERS := $(HEADER_LIST:$(LOCAL_PATH)/%=%)

LOCAL_SRC_FILES := \
  src/ioapi.c \
  src/miniunz.c \
  src/mztools.c \
  src/unzip.c \
  src/zip.c

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src

LOCAL_EXPORT_LDLIBS += -lz

include $(BUILD_STATIC_LIBRARY)
