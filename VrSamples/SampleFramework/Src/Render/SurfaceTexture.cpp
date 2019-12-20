/************************************************************************************

Filename    :   SurfaceTexture.cpp
Content     :   Interface to Android SurfaceTexture objects
Created     :   September 17, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "SurfaceTexture.h"

#include <stdlib.h>

#include "Misc/Log.h"
#include "Egl.h"
#include "GlTexture.h"

namespace OVRFW {

SurfaceTexture::SurfaceTexture( JNIEnv * jni_ ) :
	textureId( 0 ),
	javaObject( NULL ),
	jni( NULL ),
	nanoTimeStamp( 0 ),
	updateTexImageMethodId( NULL ),
	getTimestampMethodId( NULL ),
	setDefaultBufferSizeMethodId( NULL )
{
	jni = jni_;

	glGenTextures( 1, &textureId );
	glBindTexture( GL_TEXTURE_EXTERNAL_OES, GetTextureId() );
	glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );

	static const char * className = "android/graphics/SurfaceTexture";
	const jclass surfaceTextureClass = jni->FindClass( className );
	if ( surfaceTextureClass == 0 )
	{
		ALOGE_FAIL( "FindClass( %s ) failed", className );
	}

	// find the constructor that takes an int
	const jmethodID constructor = jni->GetMethodID( surfaceTextureClass, "<init>", "(I)V" );
	if ( constructor == 0 )
	{
		ALOGE_FAIL( "GetMethodID( <init> ) failed" );
	}

	jobject obj = jni->NewObject( surfaceTextureClass, constructor, GetTextureId() );
	if ( obj == 0 )
	{
		ALOGE_FAIL( "NewObject() failed" );
	}

	javaObject = jni->NewGlobalRef( obj );
	if ( javaObject == 0 )
	{
		ALOGE_FAIL( "NewGlobalRef() failed" );
	}

	// Now that we have a globalRef, we can free the localRef
	jni->DeleteLocalRef( obj );

	updateTexImageMethodId = jni->GetMethodID( surfaceTextureClass, "updateTexImage", "()V" );
	if ( !updateTexImageMethodId )
	{
		ALOGE_FAIL( "couldn't get updateTexImageMethodId" );
	}

	getTimestampMethodId = jni->GetMethodID( surfaceTextureClass, "getTimestamp", "()J" );
	if ( !getTimestampMethodId )
	{
		ALOGE_FAIL( "couldn't get getTimestampMethodId" );
	}

	setDefaultBufferSizeMethodId = jni->GetMethodID( surfaceTextureClass, "setDefaultBufferSize", "(II)V" );
	if ( !setDefaultBufferSizeMethodId )
	{
		ALOGE_FAIL( "couldn't get setDefaultBufferSize" );
	}

	// jclass objects are localRefs that need to be freed
	jni->DeleteLocalRef( surfaceTextureClass );
}

SurfaceTexture::~SurfaceTexture()
{
	if ( textureId != 0 )
	{
		glDeleteTextures( 1, &textureId );
		textureId = 0;
	}
	if ( javaObject )
	{
		jni->DeleteGlobalRef( javaObject );
		javaObject = 0;
	}
}

void SurfaceTexture::SetDefaultBufferSize( const int width, const int height )
{
	jni->CallVoidMethod( javaObject, setDefaultBufferSizeMethodId, width, height );
}

void SurfaceTexture::Update()
{
    // latch the latest movie frame to the texture
	if ( !javaObject )
	{
		return;
	}

	jni->CallVoidMethod( javaObject, updateTexImageMethodId );
	nanoTimeStamp = jni->CallLongMethod( javaObject, getTimestampMethodId );
}

unsigned int SurfaceTexture::GetTextureId()
{
	return textureId;
}

jobject SurfaceTexture::GetJavaObject()
{
	return javaObject;
}

long long SurfaceTexture::GetNanoTimeStamp()
{
	return nanoTimeStamp;
}

}	// namespace OVRFW
