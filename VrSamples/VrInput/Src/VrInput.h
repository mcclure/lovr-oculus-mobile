/************************************************************************************

Filename    :   VrInput.h
Content     :   Trivial use of the application framework.
Created     :   2/9/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>
#include <string>
#include <memory>

#include "Appl.h"
#include "OVR_FileSys.h"

#include "Model/SceneView.h"
#include "Render/SurfaceRender.h"
#include "Render/DebugLines.h"
#include "Render/TextureAtlas.h"
#include "Render/BeamRenderer.h"
#include "Render/ParticleSystem.h"
#include "Render/Ribbon.h"
#include "GUI/GuiSys.h"

#include "VrApi_Input.h"

#include "OVR_ArmModel.h"

namespace OVRFW {

class ovrLocale;
class ovrTextureAtlas;
class ovrParticleSystem;
class ovrTextureAtlas;
class ovrBeamRenderer;


typedef std::vector< ovrPairT< ovrParticleSystem::handle_t, ovrBeamRenderer::handle_t > > jointHandles_t;

//==============================================================
// ovrInputDeviceBase
// Abstract base class for generically tracking controllers of different types.
class ovrInputDeviceBase
{
public:
	ovrInputDeviceBase()
	{
	}

	virtual	~ovrInputDeviceBase()
	{
	}

	virtual const ovrInputCapabilityHeader *	GetCaps() const = 0;	
	virtual ovrControllerType 					GetType() const = 0;
	virtual ovrDeviceID							GetDeviceID() const = 0;
	virtual const char *						GetName() const = 0;
};

//==============================================================
// ovrInputDevice_TrackedRemote
class ovrInputDevice_TrackedRemote : public ovrInputDeviceBase
{
public:
	ovrInputDevice_TrackedRemote( const ovrInputTrackedRemoteCapabilities & caps, 
			const uint8_t lastRecenterCount )
		: ovrInputDeviceBase()
		, MinTrackpad( FLT_MAX )
		, MaxTrackpad( -FLT_MAX )
		, Caps( caps )
		, LastRecenterCount( lastRecenterCount )
	{
		IsActiveInputDevice = false;
	}

	virtual ~ovrInputDevice_TrackedRemote()
	{
	}

	static ovrInputDeviceBase * 				Create( OVRFW::ovrAppl & app, OvrGuiSys & guiSys, 
														VRMenu & menu, 
														const ovrInputTrackedRemoteCapabilities & capsHeader );
	void										UpdateHaptics( ovrMobile * ovr, const ovrApplFrameIn & vrFrame );
	virtual const ovrInputCapabilityHeader *	GetCaps() const override { return &Caps.Header; }
	virtual ovrControllerType 					GetType() const override { return Caps.Header.Type; }
	virtual ovrDeviceID 						GetDeviceID() const override { return Caps.Header.DeviceID; }
	virtual const char *						GetName() const override { return "TrackedRemote"; }

	ovrArmModel::ovrHandedness					GetHand() const
	{
		return ( Caps.ControllerCapabilities & ovrControllerCaps_LeftHand ) != 0 ? ovrArmModel::HAND_LEFT : ovrArmModel::HAND_RIGHT;
	}

	const ovrTracking &							GetTracking() const { return Tracking; }
	void										SetTracking( const ovrTracking & tracking ) { Tracking = tracking; }

	uint8_t										GetLastRecenterCount() const { return LastRecenterCount; }
	void										SetLastRecenterCount( const uint8_t c ) { LastRecenterCount = c; }

	ovrArmModel & 								GetArmModel() { return ArmModel; }
	jointHandles_t & 							GetJointHandles() { return JointHandles; }
	std::vector< ovrDrawSurface > &				GetControllerSurfaces() { return Surfaces; }
	const ovrInputTrackedRemoteCapabilities &	GetTrackedRemoteCaps() const { return Caps; }

	OVR::Vector2f								MinTrackpad;
	OVR::Vector2f								MaxTrackpad;
	bool										IsActiveInputDevice;

private:
	ovrInputTrackedRemoteCapabilities	Caps;
	std::vector< ovrDrawSurface >		Surfaces;
	uint8_t								LastRecenterCount;
	ovrArmModel							ArmModel;
	jointHandles_t						JointHandles;
	ovrTracking							Tracking;
	uint32_t							HapticState;
	float								HapticsSimpleValue;
};

//==============================================================
// ovrInputDevice_Headset
class ovrInputDevice_Headset : public ovrInputDeviceBase
{
public:
	ovrInputDevice_Headset( const ovrInputHeadsetCapabilities & caps )
		: ovrInputDeviceBase()
		, Caps( caps )
	{
	}

	virtual ~ovrInputDevice_Headset()
	{
	}

	static ovrInputDeviceBase * 				Create( OVRFW::ovrAppl & app, OvrGuiSys & guiSys, 
														VRMenu & menu, 
														const ovrInputHeadsetCapabilities & headsetCapabilities );

	virtual const ovrInputCapabilityHeader *	GetCaps() const override { return &Caps.Header; }
	virtual ovrControllerType 					GetType() const override { return Caps.Header.Type; }
	virtual ovrDeviceID  						GetDeviceID() const override { return Caps.Header.DeviceID; }
	virtual const char *						GetName() const override { return "Headset"; }

	const ovrInputHeadsetCapabilities &			GetHeadsetCaps() const { return Caps; }

private:
	ovrInputHeadsetCapabilities	Caps;
};

//==============================================================
// ovrInputDevice_Gamepad
class ovrInputDevice_Gamepad : public ovrInputDeviceBase
{
public:
	ovrInputDevice_Gamepad( const ovrInputGamepadCapabilities & caps )
		: ovrInputDeviceBase()
		, Caps( caps )
	{
	}

	virtual ~ovrInputDevice_Gamepad()
	{
	}

	static ovrInputDeviceBase * 				Create( OVRFW::ovrAppl & app, OvrGuiSys & guiSys,
		VRMenu & menu,
		const ovrInputGamepadCapabilities & gamepadCapabilities );

	virtual const ovrInputCapabilityHeader *	GetCaps() const override { return &Caps.Header; }
	virtual ovrControllerType 					GetType() const override { return Caps.Header.Type; }
	virtual ovrDeviceID  						GetDeviceID() const override { return Caps.Header.DeviceID; }
	virtual const char *						GetName() const override { return "Gamepad"; }

	const ovrInputGamepadCapabilities &			GetGamepadCaps() const { return Caps; }

private:
	ovrInputGamepadCapabilities	Caps;
};

//==============================================================
// ovrControllerRibbon
class ovrControllerRibbon
{
public:
	ovrControllerRibbon() = delete;
	ovrControllerRibbon( const int numPoints, const float width, const float length, const OVR::Vector4f & color );
	~ovrControllerRibbon();

	void Update( const OVR::Matrix4f & centerViewMatrix, const OVR::Vector3f & anchorPoint, const float deltaSeconds );

	ovrRibbon *		Ribbon = nullptr;
	ovrPointList *	Points = nullptr;
	ovrPointList *	Velocities = nullptr;
	int 			NumPoints = 0;
	float 			Length = 1.0f;
};

//==============================================================
// ovrVrInput
class ovrVrInput : public OVRFW::ovrAppl
{
public:
	ovrVrInput(  
		const int32_t mainThreadTid, 
		const int32_t renderThreadTid,
		const int cpuLevel, 
		const int gpuLevel );

	virtual ~ovrVrInput();

	// Called when the application initializes.
	// Must return true if the application initializes successfully.
	virtual bool                  	AppInit( const OVRFW::ovrAppContext * context ) override;
	// Called when the application shuts down
	virtual void                  	AppShutdown( const OVRFW::ovrAppContext * context ) override;
	// Called when the application is resumed by the system.
	virtual void                  	AppResumed( const OVRFW::ovrAppContext * contet ) override;
	// Called when the application is paused by the system.
	virtual void                  	AppPaused( const OVRFW::ovrAppContext * context ) override;
	// Called once per frame when the VR session is active.
	virtual OVRFW::ovrApplFrameOut  AppFrame( const OVRFW::ovrApplFrameIn & in ) override;
	// Called once per frame to allow the application to render eye buffers.
	virtual void                	AppRenderFrame( const OVRFW::ovrApplFrameIn & in, OVRFW::ovrRendererOutput & out ) override;
	// Called once per eye each frame for default renderer
	virtual void 					AppRenderEye( const OVRFW::ovrApplFrameIn & in, OVRFW::ovrRendererOutput & out, int eye ) override;

	class OvrGuiSys &				GetGuiSys() { return *GuiSys; }
	class ovrLocale &				GetLocale() { return *Locale; }

private:
	ovrRenderState				RenderState;
	ovrFileSys *				FileSys;
	OvrDebugLines *				DebugLines;
	OvrGuiSys::SoundEffectPlayer * SoundEffectPlayer;
	OvrGuiSys *					GuiSys;
	ovrLocale *					Locale;

	ModelFile *					SceneModel;
	OvrSceneView				Scene;

	ovrTextureAtlas *			SpriteAtlas;
	ovrParticleSystem *			ParticleSystem;
	ovrTextureAtlas *			BeamAtlas;
	ovrBeamRenderer *			RemoteBeamRenderer;

	ovrBeamRenderer::handle_t	LaserPointerBeamHandle;
	ovrParticleSystem::handle_t	LaserPointerParticleHandle;
	bool						LaserHit;

	GlProgram					ProgSingleTexture;
	GlProgram					ProgLitSpecularWithHighlight;
	GlProgram					ProgLitSpecular;
	GlProgram					ProgOculusTouch;

	ModelFile *					ControllerModelGear;
	ModelFile *					ControllerModelGearPreLit;

	ModelFile *					ControllerModelOculusGo;
	ModelFile *					ControllerModelOculusGoPreLit;
	ModelFile *					ControllerModelOculusTouchLeft;
	ModelFile *					ControllerModelOculusTouchRight;

	OVR::Vector3f				SpecularLightDirection;
	OVR::Vector3f				SpecularLightColor;
	OVR::Vector3f				AmbientLightColor;
	OVR::Vector4f				HighLightMask;
	OVR::Vector4f				HighLightMaskLeft;
	OVR::Vector4f				HighLightMaskRight;
	OVR::Vector3f				HighLightColor;

	double						LastGamepadUpdateTimeInSeconds;

	VRMenu *					Menu;

	// because a single Gear VR controller can be a left or right controller dependent on the
	// user's handedness (dominant hand) setting, we can't simply track controllers using a left
	// or right hand slot look up, because on any frame a Gear VR controller could change from
	// left handed to right handed and switch slots.
	std::vector< ovrInputDeviceBase* >	InputDevices;

	ovrControllerRibbon *		Ribbons[ovrArmModel::HAND_MAX];


	ovrDeviceType				DeviceType;

	uint32_t					ActiveInputDeviceID;

	OVRFW::ovrSurfaceRender		SurfaceRender;

private:
	void					ClearAndHideMenuItems();
	ovrResult				PopulateRemoteControllerInfo( ovrInputDevice_TrackedRemote & trDevice, bool recenteredController );
	void					ResetLaserPointer();

	int 					FindInputDevice( const ovrDeviceID deviceID ) const;
	void 					RemoveDevice( const ovrDeviceID deviceID );
	bool 					IsDeviceTracked( const ovrDeviceID deviceID ) const;

	void					EnumerateInputDevices();
	void                	RenderRunningFrame( const OVRFW::ovrApplFrameIn & in, OVRFW::ovrRendererOutput & out );

	void 					OnDeviceConnected( const ovrInputCapabilityHeader & capsHeader );	
	void					OnDeviceDisconnected( ovrDeviceID const disconnectedDeviceID );
	bool 					OnKeyEvent( const int keyCode, const int action );
};

} // namespace OVRFW

