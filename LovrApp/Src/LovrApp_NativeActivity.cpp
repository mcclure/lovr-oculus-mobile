/************************************************************************************

Filename	:	LovrApp_NativeActivity.c
Content		:	Based on "VrCubeWorld_NativeActivity.c" from Oculus SDK
                This sample uses the Android NativeActivity class. This sample does
				not use the application framework.
				This sample only uses the VrApi.
Created		:	March, 2015
Authors		:	J.M.P. van Waveren

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>					// for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/window.h>				// for AWINDOW_FLAG_KEEP_SCREEN_ON
#include <android/native_window_jni.h>	// for native window JNI
#include <android_native_app_glue.h>
#include <sys/stat.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <string>
#include <vector>
#include <algorithm>

#include "jni.h"

#define DEBUG 1
#define LOG_TAG "LovrActivity"

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )
#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

// Adapted from libOVRKernel JNIUtils.cpp
std::string ovr_GetPackageCodePath(JNIEnv * jni, jobject activityObject)
{
	std::string result = "ERROR"; // FIXME

	jobject const activityClass_ = jni->GetObjectClass( activityObject );
	jclass activityClass = static_cast< jclass >(activityClass_);
	jmethodID getPackageCodePathId = jni->GetMethodID( activityClass, "getPackageCodePath", "()Ljava/lang/String;" );
	if ( getPackageCodePathId == 0 )
	{
		ALOGE( "Failed to find getPackageCodePath on class %llu, object %llu",
				(long long unsigned int)activityClass, (long long unsigned int)activityObject );
		return result;
	}

	jstring jPathString = (jstring)jni->CallObjectMethod( activityObject, getPackageCodePathId );
	if ( !jni->ExceptionOccurred() )
	{
		result = jni->GetStringUTFChars(jPathString, NULL);
	}
	else 
	{
		jni->ExceptionClear();
		ALOGE( "Cleared JNI exception" );
	}

	//OVR_LOG( "ovr_GetPackageCodePath() = '%s'", packageCodePath );
	return result;
}

extern "C" {

#include "lovr/src/modules/headset/oculus_mobile_bridge.h"

static BridgeLovrDevice currentDevice;

#define FILENAMESIZE 1024

#if !defined( EGL_OPENGL_ES3_BIT_KHR )
#define EGL_OPENGL_ES3_BIT_KHR		0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER			0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR		0x1004
#endif

#if !defined( GL_EXT_multisampled_render_to_texture )
typedef void (GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
#endif

#if !defined( GL_OVR_multiview )
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR       = 0x9630;
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR = 0x9632;
static const int GL_MAX_VIEWS_OVR                                      = 0x9631;
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews);
#endif

#if !defined( GL_OVR_multiview_multisampled_render_to_texture )
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews);
#endif

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

static const int CPU_LEVEL			= 2;
static const int GPU_LEVEL			= 3;
static const int NUM_MULTI_SAMPLES	= 4;

#define MULTI_THREADED			0

/*
================================================================================

System Clock Time

================================================================================
*/

static double GetTimeInSeconds()
{
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return ( now.tv_sec * 1e9 + now.tv_nsec ) * 0.000000001;
}

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

typedef struct
{
	bool multi_view;					// GL_OVR_multiview, GL_OVR_multiview2
} OpenGLExtensions_t;

OpenGLExtensions_t glExtensions;

void BridgeLovrUnpack(ovrRigidBodyPosef &HeadPose, BridgeLovrPose &Pose, BridgeLovrAngularVector &Velocity, BridgeLovrAngularVector &Acceleration) {
	Pose.x = HeadPose.Pose.Position.x;
	Pose.y = HeadPose.Pose.Position.y;
	Pose.z = HeadPose.Pose.Position.z;
	Pose.q[0] = HeadPose.Pose.Orientation.x;
	Pose.q[1] = HeadPose.Pose.Orientation.y;
	Pose.q[2] = HeadPose.Pose.Orientation.z;
	Pose.q[3] = HeadPose.Pose.Orientation.w;

	Velocity.x = HeadPose.LinearVelocity.x;
	Velocity.y = HeadPose.LinearVelocity.y;
	Velocity.z = HeadPose.LinearVelocity.z;
	Velocity.ax = HeadPose.AngularVelocity.x;
	Velocity.ay = HeadPose.AngularVelocity.y;
	Velocity.az = HeadPose.AngularVelocity.z;

	Acceleration.x = HeadPose.LinearAcceleration.x;
	Acceleration.y = HeadPose.LinearAcceleration.y;
	Acceleration.z = HeadPose.LinearAcceleration.z;
	Acceleration.ax = HeadPose.AngularAcceleration.x;
	Acceleration.ay = HeadPose.AngularAcceleration.y;
	Acceleration.az = HeadPose.AngularAcceleration.z;
}

static void EglInitExtensions()
{
	const char * allExtensions = (const char *)glGetString( GL_EXTENSIONS );
	__android_log_print(ANDROID_LOG_DEBUG, "LOVR", "In nativeactivity, version %s, extensions:\n%s\n", glGetString(GL_VERSION), allExtensions);
	if ( allExtensions != NULL )
	{
		glExtensions.multi_view = strstr( allExtensions, "GL_OVR_multiview2" ) &&
								  strstr( allExtensions, "GL_OVR_multiview_multisampled_render_to_texture" );
	}
}

static const char * EglErrorString( const EGLint error )
{
	switch ( error )
	{
		case EGL_SUCCESS:				return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED:		return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:			return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:				return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:			return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT:			return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG:			return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE:	return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:			return "EGL_BAD_DISPLAY";
		case EGL_BAD_SURFACE:			return "EGL_BAD_SURFACE";
		case EGL_BAD_MATCH:				return "EGL_BAD_MATCH";
		case EGL_BAD_PARAMETER:			return "EGL_BAD_PARAMETER";
		case EGL_BAD_NATIVE_PIXMAP:		return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:		return "EGL_BAD_NATIVE_WINDOW";
		case EGL_CONTEXT_LOST:			return "EGL_CONTEXT_LOST";
		default:						return "unknown";
	}
}

/*
================================================================================

ovrEgl

================================================================================
*/

typedef struct
{
	EGLint		MajorVersion;
	EGLint		MinorVersion;
	EGLDisplay	Display;
	EGLConfig	Config;
	EGLSurface	TinySurface;
	EGLSurface	MainSurface;
	EGLContext	Context;
} ovrEgl;

static void ovrEgl_Clear( ovrEgl * egl )
{
	egl->MajorVersion = 0;
	egl->MinorVersion = 0;
	egl->Display = 0;
	egl->Config = 0;
	egl->TinySurface = EGL_NO_SURFACE;
	egl->MainSurface = EGL_NO_SURFACE;
	egl->Context = EGL_NO_CONTEXT;
}

static void ovrEgl_CreateContext( ovrEgl * egl, const ovrEgl * shareEgl )
{
	if ( egl->Display != 0 )
	{
		return;
	}

	egl->Display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	ALOGV( "        eglInitialize( Display, &MajorVersion, &MinorVersion )" );
	eglInitialize( egl->Display, &egl->MajorVersion, &egl->MinorVersion );
	// Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
	// flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
	// settings, and that is completely wasted for our warp target.
	const int MAX_CONFIGS = 1024;
	EGLConfig configs[MAX_CONFIGS];
	EGLint numConfigs = 0;
	if ( eglGetConfigs( egl->Display, configs, MAX_CONFIGS, &numConfigs ) == EGL_FALSE )
	{
		ALOGE( "        eglGetConfigs() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	const EGLint configAttribs[] =
	{
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
		EGL_ALPHA_SIZE,		8, // need alpha for the multi-pass timewarp compositor
		EGL_DEPTH_SIZE,		0,
		EGL_STENCIL_SIZE,	0,
		EGL_SAMPLES,		0,
		EGL_NONE
	};
	egl->Config = 0;
	for ( int i = 0; i < numConfigs; i++ )
	{
		EGLint value = 0;

		eglGetConfigAttrib( egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value );
		if ( ( value & EGL_OPENGL_ES3_BIT_KHR ) != EGL_OPENGL_ES3_BIT_KHR )
		{
			continue;
		}

		// The pbuffer config also needs to be compatible with normal window rendering
		// so it can share textures with the window context.
		eglGetConfigAttrib( egl->Display, configs[i], EGL_SURFACE_TYPE, &value );
		if ( ( value & ( EGL_WINDOW_BIT | EGL_PBUFFER_BIT ) ) != ( EGL_WINDOW_BIT | EGL_PBUFFER_BIT ) )
		{
			continue;
		}

		int	j = 0;
		for ( ; configAttribs[j] != EGL_NONE; j += 2 )
		{
			eglGetConfigAttrib( egl->Display, configs[i], configAttribs[j], &value );
			if ( value != configAttribs[j + 1] )
			{
				break;
			}
		}
		if ( configAttribs[j] == EGL_NONE )
		{
			egl->Config = configs[i];
			break;
		}
	}
	if ( egl->Config == 0 )
	{
		ALOGE( "        eglChooseConfig() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	ALOGV( "        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )" );
	egl->Context = eglCreateContext( egl->Display, egl->Config, ( shareEgl != NULL ) ? shareEgl->Context : EGL_NO_CONTEXT, contextAttribs );
	if ( egl->Context == EGL_NO_CONTEXT )
	{
		ALOGE( "        eglCreateContext() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	const EGLint surfaceAttribs[] =
	{
		EGL_WIDTH, 16,
		EGL_HEIGHT, 16,
		EGL_NONE
	};
	ALOGV( "        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )" );
	egl->TinySurface = eglCreatePbufferSurface( egl->Display, egl->Config, surfaceAttribs );
	if ( egl->TinySurface == EGL_NO_SURFACE )
	{
		ALOGE( "        eglCreatePbufferSurface() failed: %s", EglErrorString( eglGetError() ) );
		eglDestroyContext( egl->Display, egl->Context );
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
	ALOGV( "        eglMakeCurrent( Display, TinySurface, TinySurface, Context )" );
	if ( eglMakeCurrent( egl->Display, egl->TinySurface, egl->TinySurface, egl->Context ) == EGL_FALSE )
	{
		ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		eglDestroySurface( egl->Display, egl->TinySurface );
		eglDestroyContext( egl->Display, egl->Context );
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
}

static void ovrEgl_DestroyContext( ovrEgl * egl )
{
	if ( egl->Display != 0 )
	{
		ALOGE( "        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )" );
		if ( eglMakeCurrent( egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) == EGL_FALSE )
		{
			ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		}
	}
	if ( egl->Context != EGL_NO_CONTEXT )
	{
		ALOGE( "        eglDestroyContext( Display, Context )" );
		if ( eglDestroyContext( egl->Display, egl->Context ) == EGL_FALSE )
		{
			ALOGE( "        eglDestroyContext() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->Context = EGL_NO_CONTEXT;
	}
	if ( egl->TinySurface != EGL_NO_SURFACE )
	{
		ALOGE( "        eglDestroySurface( Display, TinySurface )" );
		if ( eglDestroySurface( egl->Display, egl->TinySurface ) == EGL_FALSE )
		{
			ALOGE( "        eglDestroySurface() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->TinySurface = EGL_NO_SURFACE;
	}
	if ( egl->Display != 0 )
	{
		ALOGE( "        eglTerminate( Display )" );
		if ( eglTerminate( egl->Display ) == EGL_FALSE )
		{
			ALOGE( "        eglTerminate() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->Display = 0;
	}
}

/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct
{
	int						Width;
	int						Height;
	int						Multisamples;
	int						TextureSwapChainLength;
	int						TextureSwapChainIndex;
	bool					UseMultiview;
	ovrTextureSwapChain *	ColorTextureSwapChain;
} ovrFramebuffer;

static void ovrFramebuffer_Clear( ovrFramebuffer * frameBuffer )
{
	frameBuffer->Width = 0;
	frameBuffer->Height = 0;
	frameBuffer->Multisamples = 0;
	frameBuffer->TextureSwapChainLength = 0;
	frameBuffer->TextureSwapChainIndex = 0;
	frameBuffer->UseMultiview = false;
	frameBuffer->ColorTextureSwapChain = NULL;
}

static bool ovrFramebuffer_Create( ovrFramebuffer * frameBuffer, const bool useMultiview, const GLenum colorFormat, const int width, const int height, const int multisamples )
{
	frameBuffer->Width = width;
	frameBuffer->Height = height;
	frameBuffer->Multisamples = multisamples;
	frameBuffer->UseMultiview = ( useMultiview ) ? true : false;

	frameBuffer->ColorTextureSwapChain = vrapi_CreateTextureSwapChain3( frameBuffer->UseMultiview ? VRAPI_TEXTURE_TYPE_2D_ARRAY : VRAPI_TEXTURE_TYPE_2D, colorFormat, width, height, 1, 3 );
	frameBuffer->TextureSwapChainLength = vrapi_GetTextureSwapChainLength( frameBuffer->ColorTextureSwapChain );

	return true;
}

static void ovrFramebuffer_Destroy( ovrFramebuffer * frameBuffer )
{
	vrapi_DestroyTextureSwapChain( frameBuffer->ColorTextureSwapChain );
}

static void ovrFramebuffer_SetCurrent( ovrFramebuffer * frameBuffer )
{
}

static void ovrFramebuffer_SetNone()
{
}

static void ovrFramebuffer_Resolve( ovrFramebuffer * frameBuffer )
{
}

static void ovrFramebuffer_Advance( ovrFramebuffer * frameBuffer )
{
	// Advance to the next texture from the set.
	frameBuffer->TextureSwapChainIndex = ( frameBuffer->TextureSwapChainIndex + 1 ) % frameBuffer->TextureSwapChainLength;
}

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct
{
	ovrFramebuffer	FrameBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
	int				NumBuffers;
} ovrRenderer;

static void ovrRenderer_Clear( ovrRenderer * renderer )
{
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer_Clear( &renderer->FrameBuffer[eye] );
	}
	renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;
}

static void ovrRenderer_Create( ovrRenderer * renderer, const ovrJava * java, const bool useMultiview )
{
	renderer->NumBuffers = useMultiview ? 1 : VRAPI_FRAME_LAYER_EYE_MAX;

	// Create the frame buffers.
	for ( int eye = 0; eye < renderer->NumBuffers; eye++ )
	{
		ovrFramebuffer_Create( &renderer->FrameBuffer[eye], useMultiview,
								GL_SRGB8_ALPHA8,
								vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH ),
								vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT ),
								NUM_MULTI_SAMPLES );

	}
}

static void ovrRenderer_Destroy( ovrRenderer * renderer )
{
	for ( int eye = 0; eye < renderer->NumBuffers; eye++ )
	{
		ovrFramebuffer_Destroy( &renderer->FrameBuffer[eye] );
	}
}

// Haptics handling for bridge

#define VIBRATE_DEVICES 2 // Max supported vibrating devices

typedef enum {
	VIBRATE_NONE,
	VIBRATE_SIMPLE,
	VIBRATE_BUFFERED
} VibrateMode;

// There are two APIs for vibration: "simple" which only lets you set a "current" vibration value, "buffer" which lets you queue an array of vibration samples.
// In Simple mode we set the requested value and preserve a "when to stop" timestamp. This will be imprecise because it will cut only on frames, which can be inconsistent.
// In Buffered mode we calculate buffers for the entire span of the vibration (fading out linearly) and submit them all with calculated timestamps.
//
// FIXME: In buffered mode, it doesn't do the right thing with long vibrations. I think this is because it can't call vrapi_SetHapticVibrationBuffer more than once per frame.
//        The solution might be to only queue one buffer at a time and then queue the next at some later point.
// FIXME: Lovr has no documented behavior for vibrating for exactly 0 seconds. This code in both modes interprets this as "vibrate for the smallest possible unit of time"
struct {
	ovrMobile *ovr;
	double currentTime; // Timestamp of frame being processed now
	bool anyFrames;     // False on first frame
	ovrHapticBuffer *buffer;            // Only used in buffered mode. Metadata object submitted to SetHapticVibrationBuffer
	std::vector<uint8_t> bufferBacking; // Only used in buffered mode. Storage for buffer in ovrHapticBuffer object 
	struct {
		ovrDeviceID device;
		VibrateMode mode; // Per the API, you can have buffer-supporting and buffer-nonsupporting devices connected at once. It is unclear if this ever happens in the real world
		bool canCall; // True if no vibrate call has yet been made this frame
		union {
			struct {
				bool vibrating;             // Only used in simple mode
				double clearTime;           // Only used in simple mode
			} simple;
			struct {
				// Every buffered-mode device reports its own sample rate and max-samples-per-buffer-submission
				uint32_t samplesMax;		// Only used in buffered mode
				uint32_t sampleDurationMs;  // Only used in buffered mode
				// Vibration state machine
				bool vibrating;
				uint32_t tailLength;
				uint32_t remainingLength;
				float strength;
				float submitTime;
				uint32_t tailAt;
			} buffered;
		};
	} deviceState[VIBRATE_DEVICES];
} vibrateFunctionState;

// One step in the loop of feeding in haptic buffers. We can only call this once per frame.
static void vibrateBufferedStep(int controller) {
	auto &state = vibrateFunctionState.deviceState[controller].buffered;

	// The buffered vibration feature is not fully documented. The function takes a start time and a list of samples.
	// The documentation says it can only be called once per frame. But there seem to be additional restrictions.
	// It appears you can only have one "pending" vibration buffer at a time. It is as if there is a "current" vibration
	// buffer, and a "next" vibration buffer. Overwriting the "next" vibration buffer when it is already filled appears
	// to truncate vibration. Therefore we wait for the "current" to finish and the "next" to free up before calling again.
	if (vibrateFunctionState.currentTime <= state.submitTime)
		return;

	vibrateFunctionState.deviceState[controller].canCall = false; // Ensure no more calls this frame
	float downStep = state.strength/state.tailLength;  // Difference between one sample and the next

	uint32_t sampleLength = std::min(state.tailLength, state.samplesMax); // Samples to process this frame
	state.remainingLength -= sampleLength;
	if (sampleLength > vibrateFunctionState.bufferBacking.size()) {
		vibrateFunctionState.bufferBacking.reserve(sampleLength);
		vibrateFunctionState.buffer->HapticBuffer = &vibrateFunctionState.bufferBacking[0];
	}
	vibrateFunctionState.buffer->BufferTime = state.submitTime;
	vibrateFunctionState.buffer->NumSamples = sampleLength;
	vibrateFunctionState.buffer->Terminated = state.remainingLength == 0;
	for(int c = 0; c < sampleLength; c++, state.tailAt++) {
		vibrateFunctionState.bufferBacking[c] = std::min(std::max(state.strength - state.tailAt*downStep, 0.f) * 256.f, 255.f);
	}
	vrapi_SetHapticVibrationBuffer( vibrateFunctionState.ovr, vibrateFunctionState.deviceState[controller].device, vibrateFunctionState.buffer );
	state.submitTime += sampleLength * state.sampleDurationMs / 1000.f;

	state.vibrating = state.remainingLength > 0;
}

// Call when vibrate() is called by developer
static bool vibrateFunction(int controller, float strength, float duration) {
	if (vibrateFunctionState.deviceState[controller].mode == VIBRATE_NONE) // Fail if haptics aren't supported
		return false;
	if (!vibrateFunctionState.deviceState[controller].canCall) // Per docs may not call more than once a frame
		return false;
	vibrateFunctionState.deviceState[controller].canCall = false; // Don't call again or clear this at the end of the frame
	if (vibrateFunctionState.deviceState[controller].mode == VIBRATE_BUFFERED) {
		auto &state = vibrateFunctionState.deviceState[controller].buffered;
		if (strength > 0) {
			if (strength > 1) // Prevent overflow
				strength = 1;
			// How many samples must we submit to cover entire vibration?
			state.tailLength = duration*1000/state.sampleDurationMs;
			if (state.tailLength == 0) // Prevent divide by zero
				state.tailLength = 1;
			state.remainingLength = state.tailLength; // Count down to 0 as we generate samples
			state.tailAt = 0;                   // How many samples have we generated?
			state.submitTime = vibrateFunctionState.currentTime; // Timestamp for sample we are building currently

			state.vibrating = true;
			state.strength = strength;

			vibrateBufferedStep(controller);
		} else { // Strength 0, no need to use buffers
			vrapi_SetHapticVibrationSimple( vibrateFunctionState.ovr, vibrateFunctionState.deviceState[controller].device, strength );
			state.vibrating = false;
		}
	} else {
		vibrateFunctionState.deviceState[controller].simple.clearTime = vibrateFunctionState.currentTime + duration;
		vrapi_SetHapticVibrationSimple( vibrateFunctionState.ovr, vibrateFunctionState.deviceState[controller].device, strength );
		vibrateFunctionState.deviceState[controller].simple.vibrating = strength > 0;
	}
	return true;
}

// Shared code for frame init and whole-program init
static void vibrateFunctionFrameSetup(double currentTime) {
	vibrateFunctionState.currentTime = currentTime;
	for ( int idx = 0; idx < VIBRATE_DEVICES; idx++ ) {
		vibrateFunctionState.deviceState[idx].canCall = true;
	}
}

// Call once at start of program
static void vibrateFunctionInit(ovrMobile *ovr, double currentTime) {
	vibrateFunctionState.ovr = ovr;
	vibrateFunctionFrameSetup(currentTime);
}

// Call once per controller at start of program
// Note: At present due to a bug this is currently called once per controller per frame
void vibrateFunctionInitController(int controller, ovrDeviceID device, VibrateMode mode, uint32_t samplesMax, uint32_t sampleDurationMs) {
	vibrateFunctionState.deviceState[controller].device = device;
	if (mode == VIBRATE_BUFFERED && vibrateFunctionState.deviceState[controller].mode != VIBRATE_BUFFERED) {
		ALOGV("Enabling buffered vibrate, controller %d: sampleMax %d sampleDurationMs %d\n", controller, samplesMax, sampleDurationMs);
		vibrateFunctionState.deviceState[controller].buffered.samplesMax = samplesMax;
		vibrateFunctionState.deviceState[controller].buffered.sampleDurationMs = sampleDurationMs;
		if (!vibrateFunctionState.buffer)
			vibrateFunctionState.buffer = (ovrHapticBuffer *)malloc(sizeof(ovrHapticBuffer));
	}
	vibrateFunctionState.deviceState[controller].mode = mode;
}

// Call once at start of each frame
void vibrateFunctionPreframe(double currentTime) {
	if (vibrateFunctionState.anyFrames) // Ignore first call to this function (so lovr.load and first lovr.update are same frame)
		vibrateFunctionFrameSetup(currentTime);
}

// Call once at end of each frame
void vibrateFunctionPostframe() {
	for ( int idx = 0; idx < VIBRATE_DEVICES; idx++ ) {
		// The duration isn't exposed in the Simple API so we emulate it ourselves.
		if (vibrateFunctionState.deviceState[idx].mode == VIBRATE_SIMPLE && // This is for simple
			vibrateFunctionState.deviceState[idx].canCall && // May not call more than once a frame
			vibrateFunctionState.deviceState[idx].simple.vibrating &&
			vibrateFunctionState.deviceState[idx].simple.clearTime <= vibrateFunctionState.currentTime) {
			vrapi_SetHapticVibrationSimple( vibrateFunctionState.ovr, vibrateFunctionState.deviceState[idx].device, 0 );
			vibrateFunctionState.deviceState[idx].simple.vibrating = false;
		} else if (vibrateFunctionState.deviceState[idx].mode == VIBRATE_BUFFERED &&
				   vibrateFunctionState.deviceState[idx].canCall &&
			       vibrateFunctionState.deviceState[idx].buffered.vibrating) {
			vibrateBufferedStep(idx);
		}
	}
	vibrateFunctionState.anyFrames = true;
}

static ovrLayerProjection2 ovrRenderer_RenderFrame( ovrRenderer * renderer, const ovrJava * java,
											const ovrTracking2 * tracking, ovrMobile * ovr,
											unsigned long long * completionFence, BridgeLovrUpdateData &updateData )
{
	ovrTracking2 updatedTracking = *tracking;

	ovrMatrix4f eyeViewMatrixTransposed[2];
	eyeViewMatrixTransposed[0] = ovrMatrix4f_Transpose( &updatedTracking.Eye[0].ViewMatrix );
	eyeViewMatrixTransposed[1] = ovrMatrix4f_Transpose( &updatedTracking.Eye[1].ViewMatrix );

	ovrMatrix4f projectionMatrixTransposed[2];
	projectionMatrixTransposed[0] = ovrMatrix4f_Transpose( &updatedTracking.Eye[0].ProjectionMatrix );
	projectionMatrixTransposed[1] = ovrMatrix4f_Transpose( &updatedTracking.Eye[1].ProjectionMatrix );

	// Unpack data for Lovr update
	
	BridgeLovrUnpack(updatedTracking.HeadPose, updateData.lastHeadPose, updateData.lastHeadMovement.velocity, updateData.lastHeadMovement.acceleration);

	memcpy(updateData.eyeViewMatrix[0], &eyeViewMatrixTransposed[0].M[0][0], 16*sizeof(float));
	memcpy(updateData.eyeViewMatrix[1], &eyeViewMatrixTransposed[1].M[0][0], 16*sizeof(float));
	memcpy(updateData.projectionMatrix[0], &projectionMatrixTransposed[0].M[0][0], 16*sizeof(float));
	memcpy(updateData.projectionMatrix[1], &projectionMatrixTransposed[1].M[0][0], 16*sizeof(float));

	vibrateFunctionPreframe(updateData.displayTime);

	bridgeLovrUpdate(&updateData);

	ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
	layer.HeadPose = updatedTracking.HeadPose;
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer * frameBuffer = &renderer->FrameBuffer[renderer->NumBuffers == 1 ? 0 : eye];
		layer.Textures[eye].ColorSwapChain = frameBuffer->ColorTextureSwapChain;
		layer.Textures[eye].SwapChainIndex = frameBuffer->TextureSwapChainIndex;
		layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection( &updatedTracking.Eye[eye].ProjectionMatrix );
	}
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	// Render the eye images.
	for ( int eye = 0; eye < renderer->NumBuffers; eye++ )
	{
		// NOTE: In the non-mv case, latency can be further reduced by updating the sensor prediction
		// for each eye (updates orientation, not position)
		ovrFramebuffer * frameBuffer = &renderer->FrameBuffer[eye];
		ovrFramebuffer_SetCurrent( frameBuffer );

		BridgeLovrDrawData drawData;
		drawData.eye = eye;
		drawData.textureIndex = frameBuffer->TextureSwapChainIndex;

		bridgeLovrDraw(&drawData);

		ovrFramebuffer_Resolve( frameBuffer );
		ovrFramebuffer_Advance( frameBuffer );
	}

	vibrateFunctionPostframe();

	ovrFramebuffer_SetNone();

	return layer;
}

/*
================================================================================

ovrRenderThread

================================================================================
*/

#if MULTI_THREADED

typedef enum
{
	RENDER_FRAME,
	RENDER_LOADING_ICON,
	RENDER_BLACK_FINAL
} ovrRenderType;

typedef struct
{
	JavaVM *			JavaVm;
	jobject				ActivityObject;
	const ovrEgl *		ShareEgl;
	pthread_t			Thread;
	int					Tid;
	bool				UseMultiview;
	// Synchronization
	bool				Exit;
	bool				WorkAvailableFlag;
	bool				WorkDoneFlag;
	pthread_cond_t		WorkAvailableCondition;
	pthread_cond_t		WorkDoneCondition;
	pthread_mutex_t		Mutex;
	// Latched data for rendering.
	ovrMobile *			Ovr;
	ovrRenderType		RenderType;
	long long			FrameIndex;
	double				DisplayTime;
	int					SwapInterval;
	ovrTracking2		Tracking;
} ovrRenderThread;

void * RenderThreadFunction( void * parm )
{
	ovrRenderThread * renderThread = (ovrRenderThread *)parm;
	renderThread->Tid = gettid();

	ovrJava java;
	java.Vm = renderThread->JavaVm;
	(*java.Vm)->AttachCurrentThread( java.Vm, &java.Env, NULL );
	java.ActivityObject = renderThread->ActivityObject;

	// Note that AttachCurrentThread will reset the thread name.
	prctl( PR_SET_NAME, (long)"OVR::Renderer", 0, 0, 0 );

	ovrEgl egl;
	ovrEgl_CreateContext( &egl, renderThread->ShareEgl );

	setEGLattrib(EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR);

	ovrRenderer renderer;
	ovrRenderer_Create( &renderer, &java, renderThread->UseMultiview );

	for( ; ; )
	{
		// Signal work completed.
		pthread_mutex_lock( &renderThread->Mutex );
		renderThread->WorkDoneFlag = true;
		pthread_cond_signal( &renderThread->WorkDoneCondition );
		pthread_mutex_unlock( &renderThread->Mutex );

		// Wait for work.
		pthread_mutex_lock( &renderThread->Mutex );
		while ( !renderThread->WorkAvailableFlag )
		{
			pthread_cond_wait( &renderThread->WorkAvailableCondition, &renderThread->Mutex );
		}
		renderThread->WorkAvailableFlag = false;
		pthread_mutex_unlock( &renderThread->Mutex );

		// Check for exit.
		if ( renderThread->Exit )
		{
			break;
		}

		// Render.
		ovrLayer_Union2 layers[ovrMaxLayerCount] = { 0 };
		int layerCount = 0;
		int frameFlags = 0;

		if ( renderThread->RenderType == RENDER_FRAME )
		{
			ovrLayerProjection2 layer;
			layer = ovrRenderer_RenderFrame( &renderer, &java,
					renderThread->Scene, &renderThread->Simulation,
					&renderThread->Tracking, renderThread->Ovr );

			layers[layerCount++].Projection = layer;
		}
		else if ( renderThread->RenderType == RENDER_LOADING_ICON )
		{
			ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
			blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
			layers[layerCount++].Projection = blackLayer;

			ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
			iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
			layers[layerCount++].LoadingIcon = iconLayer;

			frameFlags |= VRAPI_FRAME_FLAG_FLUSH;
		}
		else if ( renderThread->RenderType == RENDER_BLACK_FINAL )
		{
			ovrLayerProjection2 layer = vrapi_DefaultLayerBlackProjection2();
			layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
			layers[layerCount++].Projection = layer;

			frameFlags |= VRAPI_FRAME_FLAG_FLUSH | VRAPI_FRAME_FLAG_FINAL;
		}

		const ovrLayerHeader2 * layerList[ovrMaxLayerCount] = { 0 };
		for ( int i = 0; i < layerCount; i++ )
		{
			layerList[i] = &layers[i].Header;
		}

		ovrSubmitFrameDescription2 frameDesc = { 0 };
		frameDesc.Flags = frameFlags;
		frameDesc.SwapInterval = renderThread->SwapInterval;
		frameDesc.FrameIndex = renderThread->FrameIndex;
		frameDesc.DisplayTime = renderThread->DisplayTime;
		frameDesc.LayerCount = layerCount;
		frameDesc.Layers = layerList;

		vrapi_SubmitFrame2( renderThread->Ovr, &frameDesc );
	}

	ovrRenderer_Destroy( &renderer );
	ovrEgl_DestroyContext( &egl );

	(*java.Vm)->DetachCurrentThread( java.Vm );

	return NULL;
}

static void ovrRenderThread_Clear( ovrRenderThread * renderThread )
{
	renderThread->JavaVm = NULL;
	renderThread->ActivityObject = NULL;
	renderThread->ShareEgl = NULL;
	renderThread->Thread = 0;
	renderThread->Tid = 0;
	renderThread->UseMultiview = false;
	renderThread->Exit = false;
	renderThread->WorkAvailableFlag = false;
	renderThread->WorkDoneFlag = false;
	renderThread->Ovr = NULL;
	renderThread->RenderType = RENDER_FRAME;
	renderThread->FrameIndex = 1;
	renderThread->DisplayTime = 0;
	renderThread->SwapInterval = 1;
	renderThread->Scene = NULL;
}

static void ovrRenderThread_Create( ovrRenderThread * renderThread, const ovrJava * java,
									const ovrEgl * shareEgl, const bool useMultiview )
{
	renderThread->JavaVm = java->Vm;
	renderThread->ActivityObject = java->ActivityObject;
	renderThread->ShareEgl = shareEgl;
	renderThread->Thread = 0;
	renderThread->Tid = 0;
	renderThread->UseMultiview = useMultiview;
	renderThread->Exit = false;
	renderThread->WorkAvailableFlag = false;
	renderThread->WorkDoneFlag = false;
	pthread_cond_init( &renderThread->WorkAvailableCondition, NULL );
	pthread_cond_init( &renderThread->WorkDoneCondition, NULL );
	pthread_mutex_init( &renderThread->Mutex, NULL );

	const int createErr = pthread_create( &renderThread->Thread, NULL, RenderThreadFunction, renderThread );
	if ( createErr != 0 )
	{
		ALOGE( "pthread_create returned %i", createErr );
	}
}

static void ovrRenderThread_Destroy( ovrRenderThread * renderThread )
{
	pthread_mutex_lock( &renderThread->Mutex );
	renderThread->Exit = true;
	renderThread->WorkAvailableFlag = true;
	pthread_cond_signal( &renderThread->WorkAvailableCondition );
	pthread_mutex_unlock( &renderThread->Mutex );

	pthread_join( renderThread->Thread, NULL );
	pthread_cond_destroy( &renderThread->WorkAvailableCondition );
	pthread_cond_destroy( &renderThread->WorkDoneCondition );
	pthread_mutex_destroy( &renderThread->Mutex );
}

static void ovrRenderThread_Submit( ovrRenderThread * renderThread, ovrMobile * ovr,
		ovrRenderType type, long long frameIndex, double displayTime, int swapInterval,
		const ovrTracking2 * tracking )
{
	// Wait for the renderer thread to finish the last frame.
	pthread_mutex_lock( &renderThread->Mutex );
	while ( !renderThread->WorkDoneFlag )
	{
		pthread_cond_wait( &renderThread->WorkDoneCondition, &renderThread->Mutex );
	}
	renderThread->WorkDoneFlag = false;
	// Latch the render data.
	renderThread->Ovr = ovr;
	renderThread->RenderType = type;
	renderThread->FrameIndex = frameIndex;
	renderThread->DisplayTime = displayTime;
	renderThread->SwapInterval = swapInterval;
	renderThread->Scene = scene;
	if ( simulation != NULL )
	{
		renderThread->Simulation = *simulation;
	}
	if ( tracking != NULL )
	{
		renderThread->Tracking = *tracking;
	}
	// Signal work is available.
	renderThread->WorkAvailableFlag = true;
	pthread_cond_signal( &renderThread->WorkAvailableCondition );
	pthread_mutex_unlock( &renderThread->Mutex );
}

static void ovrRenderThread_Wait( ovrRenderThread * renderThread )
{
	// Wait for the renderer thread to finish the last frame.
	pthread_mutex_lock( &renderThread->Mutex );
	while ( !renderThread->WorkDoneFlag )
	{
		pthread_cond_wait( &renderThread->WorkDoneCondition, &renderThread->Mutex );
	}
	pthread_mutex_unlock( &renderThread->Mutex );
}

static int ovrRenderThread_GetTid( ovrRenderThread * renderThread )
{
	ovrRenderThread_Wait( renderThread );
	return renderThread->Tid;
}

#endif // MULTI_THREADED

/*
================================================================================

ovrApp

================================================================================
*/

typedef struct
{
	ovrJava				Java;
	ovrEgl				Egl;
	ANativeWindow *		NativeWindow;
	bool				Resumed;
	ovrMobile *			Ovr;
	bool 				Started;
	long long			FrameIndex;
	double 				DisplayTime;
	int					SwapInterval;
	int					CpuLevel;
	int					GpuLevel;
	int					MainThreadTid;
	int					RenderThreadTid;
	bool				BackButtonDownLastFrame;
#if MULTI_THREADED
	ovrRenderThread		RenderThread;
#else
	ovrRenderer			Renderer;
#endif
	bool				UseMultiview;
} ovrApp;

static void ovrApp_Clear( ovrApp * app )
{
	app->Java.Vm = NULL;
	app->Java.Env = NULL;
	app->Java.ActivityObject = NULL;
	app->NativeWindow = NULL;
	app->Resumed = false;
	app->Ovr = NULL;
	app->FrameIndex = 1;
	app->DisplayTime = 0;
	app->SwapInterval = 1;
	app->CpuLevel = 2;
	app->GpuLevel = 2;
	app->MainThreadTid = 0;
	app->RenderThreadTid = 0;
	app->BackButtonDownLastFrame = false;
	app->UseMultiview = true;

	ovrEgl_Clear( &app->Egl );
	// TODO: Or maybe stop Lovr here?
	app->Started = false;
#if MULTI_THREADED
	ovrRenderThread_Clear( &app->RenderThread );
#else
	ovrRenderer_Clear( &app->Renderer );
#endif
}

static void ovrApp_PushBlackFinal( ovrApp * app )
{
#if MULTI_THREADED
	ovrRenderThread_Submit( &app->RenderThread, app->Ovr,
			RENDER_BLACK_FINAL, app->FrameIndex, app->DisplayTime, app->SwapInterval,
			NULL, NULL, NULL );
#else
	int frameFlags = 0;
	frameFlags |= VRAPI_FRAME_FLAG_FLUSH | VRAPI_FRAME_FLAG_FINAL;

	ovrLayerProjection2 layer = vrapi_DefaultLayerBlackProjection2();
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

	const ovrLayerHeader2 * layers[] =
	{
		&layer.Header
	};

	ovrSubmitFrameDescription2 frameDesc = { 0 };
	frameDesc.Flags = frameFlags;
	frameDesc.SwapInterval = 1;
	frameDesc.FrameIndex = app->FrameIndex;
	frameDesc.DisplayTime = app->DisplayTime;
	frameDesc.LayerCount = 1;
	frameDesc.Layers = layers;

	vrapi_SubmitFrame2( app->Ovr, &frameDesc );
#endif
}

static void ovrApp_HandleVrModeChanges( ovrApp * app )
{
	if ( app->Resumed != false && app->NativeWindow != NULL )
	{
		if ( app->Ovr == NULL )
		{
			ovrModeParms parms = vrapi_DefaultModeParms( &app->Java );
			// No need to reset the FLAG_FULLSCREEN window flag when using a View
			parms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

			parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
			parms.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
			parms.Display = (size_t)app->Egl.Display;
			parms.WindowSurface = (size_t)app->NativeWindow;
			parms.ShareContext = (size_t)app->Egl.Context;

			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			ALOGV( "        vrapi_EnterVrMode()" );

			app->Ovr = vrapi_EnterVrMode( &parms );

			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			// If entering VR mode failed then the ANativeWindow was not valid.
			if ( app->Ovr == NULL )
			{
				ALOGE( "Invalid ANativeWindow!" );
				app->NativeWindow = NULL;
			}

			// Set performance parameters once we have entered VR mode and have a valid ovrMobile.
			if ( app->Ovr != NULL )
			{
				vrapi_SetClockLevels( app->Ovr, app->CpuLevel, app->GpuLevel );

				ALOGV( "		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid );

				ALOGV( "		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid );

				ALOGV( "		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid );
			}
		}
	}
	else
	{
		if ( app->Ovr != NULL )
		{
#if MULTI_THREADED
			// Make sure the renderer thread is no longer using the ovrMobile.
			ovrRenderThread_Wait( &app->RenderThread );
#endif
			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			ALOGV( "        vrapi_LeaveVrMode()" );

			vrapi_LeaveVrMode( app->Ovr );
			app->Ovr = NULL;

			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );
		}
	}
}

// TODO: Should unpack input based on device capabilities. Current code unpacks Go controller specifically
static void ovrApp_HandleInput( ovrApp * app, BridgeLovrUpdateData &updateData, double predictedDisplayTime )
{
	bool backButtonDownThisFrame = false;

	updateData.controllerCount = 0;

	for ( int i = 0; ; i++ )
	{
		if (updateData.controllerCount >= BRIDGE_LOVR_CONTROLLERMAX)
			break;
		BridgeLovrController &controller = updateData.controllers[updateData.controllerCount];

		ovrInputCapabilityHeader cap;
		ovrResult result = vrapi_EnumerateInputDevices( app->Ovr, i, &cap );
		if ( result < 0 )
		{
			break;
		}

		if ( cap.Type == ovrControllerType_Headset )
		{
			memset(&controller, 0, sizeof(controller));

			ovrInputStateHeadset headsetInputState;
			headsetInputState.Header.ControllerType = ovrControllerType_Headset;
			result = vrapi_GetCurrentInputState( app->Ovr, cap.DeviceID, &headsetInputState.Header );
			if ( result == ovrSuccess )
			{
				backButtonDownThisFrame |= headsetInputState.Buttons & ovrButton_Back;

				updateData.controllerCount++;
			}
		}
		else if ( cap.Type == ovrControllerType_TrackedRemote )
		{
			ovrInputStateTrackedRemote trackedRemoteState;
			ovrTracking hmtTracking;

			trackedRemoteState.Header.ControllerType = ovrControllerType_TrackedRemote;
			result = vrapi_GetCurrentInputState( app->Ovr, cap.DeviceID, &trackedRemoteState.Header );

			if (result == ovrSuccess)
				result = vrapi_GetInputTrackingState( app->Ovr, cap.DeviceID, predictedDisplayTime, &hmtTracking );

			if ( result == ovrSuccess )
			{
				uint32_t lastButtons = controller.buttonDown;
				memset(&controller, 0, sizeof(controller));

				ovrInputTrackedRemoteCapabilities remoteCaps;
				remoteCaps.Header = cap;
				result = vrapi_GetInputDeviceCapabilities( app->Ovr, &remoteCaps.Header );
				if (result == ovrSuccess) {
					controller.hand = (BridgeLovrHand)remoteCaps.ControllerCapabilities;
				}

				// Determine best supported vibration mode
				VibrateMode vibrateMode;
				if (remoteCaps.ControllerCapabilities & ovrControllerCaps_HasBufferedHapticVibration) {
					vibrateMode = VIBRATE_BUFFERED;
				} else if (remoteCaps.ControllerCapabilities & ovrControllerCaps_HasSimpleHapticVibration) {
					vibrateMode = VIBRATE_SIMPLE;
				} else {
					vibrateMode = VIBRATE_NONE;
				}

				// FIXME: Will get very confused if a controller is ever not left or right hand
				vibrateFunctionInitController(remoteCaps.ControllerCapabilities & ovrControllerCaps_RightHand ? 1 : 0, cap.DeviceID,
					vibrateMode, remoteCaps.HapticSamplesMax, remoteCaps.HapticSampleDurationMS);

				controller.handset = true;
				controller.buttonDown = (BridgeLovrButton)(unsigned int)trackedRemoteState.Buttons;
				controller.buttonTouch = (BridgeLovrTouch)(unsigned int)trackedRemoteState.Touches;
				controller.buttonChanged = (BridgeLovrButton) (controller.buttonDown ^ lastButtons);
				if (currentDevice == BRIDGE_LOVR_DEVICE_QUEST) {
					controller.trackpad.x = trackedRemoteState.Joystick.x;
					controller.trackpad.y = trackedRemoteState.Joystick.y;
					controller.trigger = trackedRemoteState.IndexTrigger;
					controller.grip = trackedRemoteState.GripTrigger;
				} else {
					controller.trackpad.x = trackedRemoteState.TrackpadPosition.x;
					controller.trackpad.y = trackedRemoteState.TrackpadPosition.y;
				}
				BridgeLovrUnpack(hmtTracking.HeadPose, controller.pose, controller.movement.velocity, controller.movement.acceleration);

				updateData.controllerCount++;
				backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Back;
			}
		}
		else if ( cap.Type == ovrControllerType_Gamepad )
		{
			ovrInputStateGamepad gamepadState;
			gamepadState.Header.ControllerType = ovrControllerType_Gamepad;
			result = vrapi_GetCurrentInputState( app->Ovr, cap.DeviceID, &gamepadState.Header );
			if ( result == ovrSuccess )
			{
				backButtonDownThisFrame |= ( ( gamepadState.Buttons & ovrButton_Back ) != 0 ) || ( ( gamepadState.Buttons & ovrButton_B ) != 0 );
			}
		}
	}

	bool backButtonDownLastFrame = app->BackButtonDownLastFrame;
	app->BackButtonDownLastFrame = backButtonDownThisFrame;

	if ( backButtonDownLastFrame && !backButtonDownThisFrame )
	{
		ALOGV( "back button short press" );
		ALOGV( "        ovrApp_PushBlackFinal()" );
		ovrApp_PushBlackFinal( app );
		ALOGV( "        vrapi_ShowSystemUI( confirmQuit )" );
		vrapi_ShowSystemUI( &app->Java, VRAPI_SYS_UI_CONFIRM_QUIT_MENU );
	}
}

/*
================================================================================

Native Activity

================================================================================
*/

/**
 * Process the next main command.
 */
static void app_handle_cmd( struct android_app * app, int32_t cmd )
{
	ovrApp * appState = (ovrApp *)app->userData;

	switch ( cmd )
	{
		// There is no APP_CMD_CREATE. The ANativeActivity creates the
		// application thread from onCreate(). The application thread
		// then calls android_main().
		case APP_CMD_START:
		{
			ALOGV( "onStart()" );
			ALOGV( "    APP_CMD_START" );
			break;
		}
		case APP_CMD_RESUME:
		{
			ALOGV( "onResume()" );
			ALOGV( "    APP_CMD_RESUME" );
			appState->Resumed = true;
			bridgeLovrPaused(false);
			break;
		}
		case APP_CMD_PAUSE:
		{
			ALOGV( "onPause()" );
			ALOGV( "    APP_CMD_PAUSE" );
			appState->Resumed = false;
			bridgeLovrPaused(true);
			break;
		}
		case APP_CMD_STOP:
		{
			ALOGV( "onStop()" );
			ALOGV( "    APP_CMD_STOP" );
			break;
		}
		case APP_CMD_DESTROY:
		{
			ALOGV( "onDestroy()" );
			ALOGV( "    APP_CMD_DESTROY" );
			appState->NativeWindow = NULL;
			break;
		}
		case APP_CMD_INIT_WINDOW:
		{
			ALOGV( "surfaceCreated()" );
			ALOGV( "    APP_CMD_INIT_WINDOW" );
			appState->NativeWindow = app->window;
			break;
		}
		case APP_CMD_TERM_WINDOW:
		{
			ALOGV( "surfaceDestroyed()" );
			ALOGV( "    APP_CMD_TERM_WINDOW" );
			appState->NativeWindow = NULL;
			break;
		}
	}
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main( struct android_app * app )
{
	ALOGV( "----------------------------------------------------------------" );
	ALOGV( "android_app_entry()" );
	ALOGV( "    android_main()" );

	ANativeActivity_setWindowFlags( app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0 );

	ovrJava java;
	java.Vm = app->activity->vm;
	java.Vm->AttachCurrentThread( &java.Env, NULL );
	java.ActivityObject = app->activity->clazz;

	// Note that AttachCurrentThread will reset the thread name.
	prctl( PR_SET_NAME, (long)"OVR::Main", 0, 0, 0 );

	const ovrInitParms initParms = vrapi_DefaultInitParms( &java );
	int32_t initResult = vrapi_Initialize( &initParms );
	if ( initResult != VRAPI_INITIALIZE_SUCCESS )
	{
		// If intialization failed, vrapi_* function calls will not be available.
		exit( 0 );
	}

	ovrApp appState;
	ovrApp_Clear( &appState );
	appState.Java = java;

	ovrEgl_CreateContext( &appState.Egl, NULL );

	EglInitExtensions();

	appState.UseMultiview &= ( glExtensions.multi_view &&
							vrapi_GetSystemPropertyInt( &appState.Java, VRAPI_SYS_PROP_MULTIVIEW_AVAILABLE ) );

	ALOGV( "AppState UseMultiview : %d", appState.UseMultiview );

	appState.CpuLevel = CPU_LEVEL;
	appState.GpuLevel = GPU_LEVEL;
	appState.MainThreadTid = gettid();

#if MULTI_THREADED
	ovrRenderThread_Create( &appState.RenderThread, &appState.Java, &appState.Egl, appState.UseMultiview );
	// Also set the renderer thread to SCHED_FIFO.
	appState.RenderThreadTid = ovrRenderThread_GetTid( &appState.RenderThread );
#else
	ovrRenderer_Create( &appState.Renderer, &java, appState.UseMultiview );
#endif

	app->userData = &appState;
	app->onAppCmd = app_handle_cmd;

	const double startTime = GetTimeInSeconds();

	while ( app->destroyRequested == 0 )
	{
		// Read all pending events.
		for ( ; ; )
		{
			int events;
			struct android_poll_source * source;
			const int timeoutMilliseconds = ( appState.Ovr == NULL && app->destroyRequested == 0 ) ? -1 : 0;
			if ( ALooper_pollAll( timeoutMilliseconds, NULL, &events, (void **)&source ) < 0 )
			{
				break;
			}

			// Process this event.
			if ( source != NULL )
			{
				source->process( app, source );
			}

			ovrApp_HandleVrModeChanges( &appState );
		}

		// Boot Lovr
		if ( appState.Ovr == NULL )
		{
			continue;
		}

		// Create the scene if not yet created.
		// The scene is created here to be able to show a loading icon.
		if ( !appState.Started )
		{
			BridgeLovrInitData bridgeData;

#if MULTI_THREADED
			// Show a loading icon.
			ovrRenderThread_Submit( &appState.RenderThread, appState.Ovr,
					RENDER_LOADING_ICON, appState.FrameIndex, appState.DisplayTime, appState.SwapInterval,
					NULL, NULL, NULL );
#else
			// Show a loading icon.
			int frameFlags = 0;
			frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

			ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
			blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

			ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
			iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

			const ovrLayerHeader2 * layers[] =
			{
				&blackLayer.Header,
				&iconLayer.Header,
			};

			ovrSubmitFrameDescription2 frameDesc = { 0 };
			frameDesc.Flags = frameFlags;
			frameDesc.SwapInterval = 1;
			frameDesc.FrameIndex = appState.FrameIndex;
			frameDesc.DisplayTime = appState.DisplayTime;
			frameDesc.LayerCount = 2;
			frameDesc.Layers = layers;

			vrapi_SubmitFrame2( appState.Ovr, &frameDesc );
#endif

			bridgeData.writablePath = app->activity->internalDataPath;

			std::string apkPath = ovr_GetPackageCodePath(java.Env, java.ActivityObject);
			bridgeData.apkPath = apkPath.c_str();

			// FIXME: Use larger screen-size numbers instead of the "recommended" sizes?
			// Possibly convince lovr main project to accept a dichotomy between 'display size' and 'recommended renderbuffer size'?
			bridgeData.suggestedEyeTexture.width = vrapi_GetSystemPropertyInt( &appState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH );
			bridgeData.suggestedEyeTexture.height = vrapi_GetSystemPropertyInt( &appState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT );
			bridgeData.displayFrequency = (float) vrapi_GetSystemPropertyInt( &appState.Java, VRAPI_SYS_PROP_DISPLAY_REFRESH_RATE );
			bridgeData.zeroDisplayTime = vrapi_GetPredictedDisplayTime( appState.Ovr, 0 );

			// What type of device is this? Collapse device ranges into general types
			ovrDeviceType deviceType = (ovrDeviceType)vrapi_GetSystemPropertyInt( &appState.Java, VRAPI_SYS_PROP_HEADSET_TYPE );
			if (deviceType >= VRAPI_DEVICE_TYPE_GEARVR_START && deviceType <= VRAPI_DEVICE_TYPE_GEARVR_END) {
				bridgeData.deviceType = BRIDGE_LOVR_DEVICE_GEAR;
			} else if (deviceType >= VRAPI_DEVICE_TYPE_OCULUSGO_START && deviceType <= VRAPI_DEVICE_TYPE_OCULUSGO_END) {
				bridgeData.deviceType = BRIDGE_LOVR_DEVICE_GO;
			} else if (deviceType >= VRAPI_DEVICE_TYPE_OCULUSQUEST_START && deviceType <= VRAPI_DEVICE_TYPE_OCULUSQUEST_END) {
				bridgeData.deviceType = BRIDGE_LOVR_DEVICE_QUEST;
			} else {
				bridgeData.deviceType = BRIDGE_LOVR_DEVICE_UNKNOWN;
			}
			currentDevice = bridgeData.deviceType;

			bridgeData.vibrateFunction = vibrateFunction;
			vibrateFunctionInit(appState.Ovr, bridgeData.zeroDisplayTime);

			ovrTextureSwapChain* swapchain = appState.Renderer.FrameBuffer[0].ColorTextureSwapChain;
			bridgeData.textureCount = vrapi_GetTextureSwapChainLength(swapchain);
			for (uint32_t i = 0; i < bridgeData.textureCount; i++) {
				bridgeData.textureHandles[i] = vrapi_GetTextureSwapChainHandle(swapchain, i);
			}

			bridgeLovrInit(&bridgeData);

			appState.Started = true;
		}

		// This is the only place the frame index is incremented, right before
		// calling vrapi_GetPredictedDisplayTime().
		appState.FrameIndex++;

		// Get the HMD pose, predicted for the middle of the time period during which
		// the new eye images will be displayed. The number of frames predicted ahead
		// depends on the pipeline depth of the engine and the synthesis rate.
		// The better the prediction, the less black will be pulled in at the edges.
		const double predictedDisplayTime = vrapi_GetPredictedDisplayTime( appState.Ovr, appState.FrameIndex );
		const ovrTracking2 tracking = vrapi_GetPredictedTracking2( appState.Ovr, predictedDisplayTime );

		appState.DisplayTime = predictedDisplayTime;

		// TODO: Advance LOVR here?

#if MULTI_THREADED
		// Render the eye images on a separate thread.
		ovrRenderThread_Submit( &appState.RenderThread, appState.Ovr,
				RENDER_FRAME, appState.FrameIndex, appState.DisplayTime, appState.SwapInterval,
				&tracking );
#else

		unsigned long long completionFence = 0;

		BridgeLovrUpdateData updateData;

		updateData.displayTime = predictedDisplayTime;

		// Boundary
		ovrPosef boundsPose;
		ovrVector3f boundsScale;
		if (vrapi_GetBoundaryOrientedBoundingBox(appState.Ovr, &boundsPose, &boundsScale) == ovrSuccess) {
			updateData.boundsWidth = boundsScale.x * 2.f;
			updateData.boundsDepth = boundsScale.z * 2.f;
		}

		ovrApp_HandleInput( &appState, updateData, predictedDisplayTime );

		// Render eye images and setup the primary layer using ovrTracking2.
		const ovrLayerProjection2 worldLayer = ovrRenderer_RenderFrame( &appState.Renderer, &appState.Java,
				&tracking,
				appState.Ovr, &completionFence, updateData );

		const ovrLayerHeader2 * layers[] =
		{
			&worldLayer.Header
		};

		ovrSubmitFrameDescription2 frameDesc = { 0 };
		frameDesc.Flags = 0;
		frameDesc.SwapInterval = appState.SwapInterval;
		frameDesc.FrameIndex = appState.FrameIndex;
		frameDesc.DisplayTime = appState.DisplayTime;
		frameDesc.LayerCount = 1;
		frameDesc.Layers = layers;

		// Hand over the eye images to the time warp.
		vrapi_SubmitFrame2( appState.Ovr, &frameDesc );
#endif
	}

#if MULTI_THREADED
	ovrRenderThread_Destroy( &appState.RenderThread );
#else
	ovrRenderer_Destroy( &appState.Renderer );
#endif

	bridgeLovrClose();

	// TODO DESTROY LOVR

	ovrEgl_DestroyContext( &appState.Egl );

	vrapi_Shutdown();

	java.Vm->DetachCurrentThread( );
}

}
