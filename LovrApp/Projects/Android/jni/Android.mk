
LOCAL_PATH:= $(call my-dir)

# Include libcmake
# This is weird. gradle is able to compile both cmake and ndkbuild
# But it doesn't pass information about libraries etc from one to the other.
# Cheat by reporting cmake's intermediate liblovr.so as a prebuilt library:
# TODO: Install to a known location instead of depending on the cmake temp files

include $(CLEAR_VARS)
ifdef NDK_DEBUG
	LOVR_LIB_SUFFIX=d
else
	LOVR_LIB_SUFFIX=
endif
LOCAL_MODULE := lovr
LOCAL_SRC_FILES := ../../../../cmakelib/build/intermediates/cmake/debug/obj/$(TARGET_ARCH_ABI)/liblovr$(LOVR_LIB_SUFFIX).so
include $(PREBUILT_SHARED_LIBRARY)

#--------------------------------------------------------
# lovractivity.so
#--------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += ../../../cmakelib/include

LOCAL_MODULE			:= lovractivity
LOCAL_CFLAGS			:= -Werror
LOCAL_SRC_FILES			:= ../../../Src/LovrApp_NativeActivity.cpp
LOCAL_LDLIBS			:= -llog -landroid -lGLESv3 -lEGL		# include default libraries

LOCAL_LDFLAGS			:= -u ANativeActivity_onCreate

LOCAL_STATIC_LIBRARIES	:= android_native_app_glue 
LOCAL_SHARED_LIBRARIES	:= vrapi lovr

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
