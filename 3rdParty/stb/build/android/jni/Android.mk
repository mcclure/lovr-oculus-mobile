LOCAL_PATH := $(call my-dir)/../../..

include $(CLEAR_VARS)

LOCAL_MODULE := stb
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

include $(LOCAL_PATH)/../../cflags.mk
LOCAL_CFLAGS   := -w
LOCAL_CPPFLAGS := -w

define recursive_wildcard
  $(wildcard $(1)) $(foreach e, $(wildcard $(1)/*), $(call recursive_wildcard, $(e)))
endef

# Include headers.
HEADER_LIST := $(filter %.h, $(call recursive_wildcard, $(LOCAL_PATH)/src))
LOCAL_HEADERS := $(HEADER_LIST:$(LOCAL_PATH)/%=%)

LOCAL_SRC_FILES := \
  src/stb_image.c \
  src/stb_image_write.c \
  src/stb_vorbis.c

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src

include $(BUILD_STATIC_LIBRARY)
