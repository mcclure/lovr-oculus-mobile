/************************************************************************************

Filename    :   VrFrameBuilder.cpp
Content     :   Builds the input for VrAppInterface::Frame()
Created     :   April 26, 2015
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "VrFrameBuilder.h"
#include "Kernel/OVR_LogUtils.h"
#include "Android/JniUtils.h"
#include "Kernel/OVR_Alg.h"
#include "VrApi.h"
#include "VrApi_Input.h"
#include "VrApi_Helpers.h"
#include "Kernel/OVR_String.h"
#include "OVR_Input.h"

#if defined ( OVR_OS_ANDROID )
#include <android/input.h>
#endif

namespace OVR
{

static struct
{
	ovrKeyCode	KeyCode;
	int			ButtonBit;
} buttonMappings[] = {
	{ OVR_KEY_BUTTON_A, 			BUTTON_A },
	{ OVR_KEY_BUTTON_B,				BUTTON_B },
	{ OVR_KEY_BUTTON_X, 			BUTTON_X },
	{ OVR_KEY_BUTTON_Y,				BUTTON_Y },
	{ OVR_KEY_BUTTON_START, 		BUTTON_START },
	{ OVR_KEY_ESCAPE,				BUTTON_BACK },
	{ OVR_KEY_BUTTON_SELECT, 		BUTTON_SELECT },
	{ OVR_KEY_MENU,					BUTTON_MENU },				// not really sure if left alt is the same as android OVR_KEY_MENU, but this is unused
	{ OVR_KEY_RIGHT_TRIGGER,		BUTTON_RIGHT_TRIGGER },
	{ OVR_KEY_LEFT_TRIGGER, 		BUTTON_LEFT_TRIGGER },
	{ OVR_KEY_DPAD_UP,				BUTTON_DPAD_UP },
	{ OVR_KEY_DPAD_DOWN,			BUTTON_DPAD_DOWN },
	{ OVR_KEY_DPAD_LEFT,			BUTTON_DPAD_LEFT },
	{ OVR_KEY_DPAD_RIGHT,			BUTTON_DPAD_RIGHT },
	{ OVR_KEY_LSTICK_UP,			BUTTON_LSTICK_UP },
	{ OVR_KEY_LSTICK_DOWN,			BUTTON_LSTICK_DOWN },
	{ OVR_KEY_LSTICK_LEFT,			BUTTON_LSTICK_LEFT },
	{ OVR_KEY_LSTICK_RIGHT,			BUTTON_LSTICK_RIGHT },
	{ OVR_KEY_RSTICK_UP,			BUTTON_RSTICK_UP },
	{ OVR_KEY_RSTICK_DOWN,			BUTTON_RSTICK_DOWN },
	{ OVR_KEY_RSTICK_LEFT,			BUTTON_RSTICK_LEFT },
	{ OVR_KEY_RSTICK_RIGHT,			BUTTON_RSTICK_RIGHT },
	/// FIXME: the xbox controller doesn't generate OVR_KEY_RIGHT_TRIGGER / OVR_KEY_LEFT_TRIGGER
	// because they are analog axis instead of buttons.  For now, map shoulders to our triggers.
	{ OVR_KEY_BUTTON_LEFT_SHOULDER, BUTTON_LEFT_TRIGGER },
	{ OVR_KEY_BUTTON_RIGHT_SHOULDER, BUTTON_RIGHT_TRIGGER },
	/// FIXME: the following joypad buttons are not mapped yet because they would require extending the
	/// bit flags to 64 bits.
	{ OVR_KEY_BUTTON_C,				0 },
	{ OVR_KEY_BUTTON_Z,				0 },
	{ OVR_KEY_BUTTON_LEFT_THUMB,	0 },
	{ OVR_KEY_BUTTON_RIGHT_THUMB,	0 },

	{ OVR_KEY_MAX, 0 }
};

static ovrHeadSetPluggedState HeadPhonesPluggedState = OVR_HEADSET_PLUGGED_UNKNOWN;

VrFrameBuilder::VrFrameBuilder() :
	lastTouchpadTime( 0.0 ),
	touchpadTimer( 0.0 ),
	lastTouchDown( false ),
	touchState( 0 ),
	BackKeyDownLastFrame( false )
{
	RemoteTouchpadTouchedLastFrame[0] = false;
	RemoteTouchpadTouchedLastFrame[1] = false;
	TreatRemoteTouchpadTouchedAsButtonDown[0] = false;
	TreatRemoteTouchpadTouchedAsButtonDown[1] = false;
}

void VrFrameBuilder::Init( ovrJava * java )
{
	OVR_ASSERT( java != NULL );
}

void VrFrameBuilder::InterpretTouchpad( VrInput & input, const double currentTime, const float min_swipe_distance )
{
	// 1) Down -> Up w/ Motion = Slide
	// 1) Down -> Timeout w/o Motion = Long Press
	// 2) Down -> Up w/out Motion -> Timeout = Single Tap
	// 3) Down -> Up w/out Motion -> Down -> Timeout = Nothing
	// 4) Down -> Up w/out Motion -> Down -> Up = Double Tap
	static const double timer_finger_down = 0.3;
	static const double timer_finger_up = 0.3;

	const double deltaTime = currentTime - lastTouchpadTime;
	lastTouchpadTime = currentTime;
	touchpadTimer = touchpadTimer + deltaTime;

	const bool currentTouchDown = ( input.buttonState & BUTTON_TOUCH ) != 0;

	bool down = false;
	if ( currentTouchDown && !lastTouchDown )
	{
		//OVR_LOG( "DOWN" );
		down = true;
		touchOrigin = input.touch;
	}

	bool up = false;
	if ( !currentTouchDown && lastTouchDown )
	{
		//OVR_LOG( "UP" );
		up = true;
	}

	lastTouchDown = currentTouchDown;

	input.touchRelative = input.touch - touchOrigin;
	float touchMagnitude = input.touchRelative.Length();
	input.swipeFraction = touchMagnitude / min_swipe_distance;

	switch ( touchState )
	{
	case 0:
		if ( down )
		{
			touchState = 1;
			touchpadTimer = 0.0;
		}
		break;
	case 1:
		if ( touchMagnitude >= min_swipe_distance )
		{
			int dir = 0;
			if ( fabs( input.touchRelative[0] ) > fabs( input.touchRelative[1] ) )
			{
				if ( input.touchRelative[0] < 0 )
				{
					//OVR_LOG( "SWIPE FORWARD" );
					dir = BUTTON_SWIPE_FORWARD | BUTTON_TOUCH_WAS_SWIPE;
				}
				else
				{
					//OVR_LOG( "SWIPE BACK" );
					dir = BUTTON_SWIPE_BACK | BUTTON_TOUCH_WAS_SWIPE;
				}
			}
			else
			{
				if ( input.touchRelative[1] > 0 )
				{
					//OVR_LOG( "SWIPE DOWN" );
					dir = BUTTON_SWIPE_DOWN | BUTTON_TOUCH_WAS_SWIPE;
				}
				else
				{
					//OVR_LOG( "SWIPE UP" );
					dir = BUTTON_SWIPE_UP | BUTTON_TOUCH_WAS_SWIPE;
				}
			}
			input.buttonPressed |= dir;
			input.buttonReleased |= dir & ~BUTTON_TOUCH_WAS_SWIPE;
			input.buttonState |= dir;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		else if ( up )
		{
			if ( touchpadTimer < timer_finger_down )
			{
				touchState = 2;
				touchpadTimer = 0.0;
			}
			else
			{
				input.buttonPressed |= BUTTON_TOUCH_SINGLE;
				input.buttonReleased |= BUTTON_TOUCH_SINGLE;
				input.buttonState |= BUTTON_TOUCH_SINGLE;
				touchState = 0;
				touchpadTimer = 0.0;
			}
		}
		else if ( touchpadTimer > 0.75f ) // TODO: BUTTON_TOUCH_LONGPRESS actually used?
		{
			input.buttonPressed |= BUTTON_TOUCH_LONGPRESS;
			input.buttonReleased |= BUTTON_TOUCH_LONGPRESS;
			input.buttonState |= BUTTON_TOUCH_LONGPRESS;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		break;
	case 2:
		if ( touchpadTimer >= timer_finger_up )
		{
			input.buttonPressed |= BUTTON_TOUCH_SINGLE;
			input.buttonReleased |= BUTTON_TOUCH_SINGLE;
			input.buttonState |= BUTTON_TOUCH_SINGLE;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		else if ( down )
		{
			touchState = 3;
			touchpadTimer = 0.0;
		}
		break;
	case 3:
		if ( touchpadTimer >= timer_finger_down )
		{
			touchState = 0;
			touchpadTimer = 0.0;
		}
		else if ( up )
		{
			input.buttonPressed |= BUTTON_TOUCH_DOUBLE;
			input.buttonReleased |= BUTTON_TOUCH_DOUBLE;
			input.buttonState |= BUTTON_TOUCH_DOUBLE;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		break;
	}
}

void VrFrameBuilder::AddKeyEventToFrame( ovrKeyCode const keyCode, KeyEventType const eventType, int const repeatCount )
{
	if ( eventType == KEY_EVENT_NONE )
	{
		return;
	}

	OVR_LOG( "AddKeyEventToFrame: keyCode = %i, eventType = %i", keyCode, eventType );

	vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = keyCode;
	vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = repeatCount;
	vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = eventType;
	vrFrame.Input.NumKeyEvents++;
}

void VrFrameBuilder::AdvanceVrFrame( const ovrInputEvents & inputEvents, ovrMobile * ovr,
									const ovrJava & java,
									const long long enteredVrModeFrameNumber )
{
	const VrInput lastVrInput = vrFrame.Input;

	// check before incrementing FrameNumber because it will be the previous frame's number
	vrFrame.EnteredVrMode = vrFrame.FrameNumber == enteredVrModeFrameNumber;

	// Use the vrapi input api to check to see if a short back button press happened, and to fill in the touch data
	bool backButtonDownThisFrame = false;
	vrFrame.Input.buttonState &= ~BUTTON_TOUCH;

	bool injectLeftStick = false;

	float touchpadMinSwipe = 100.0f;

	// Copy JoySticks
	for ( int i = 0; i < 2; i++ )
	{
		for ( int j = 0; j < 2; j++ )
		{
			vrFrame.Input.sticks[i][j] = inputEvents.JoySticks[i][j];
		}
	}

#if defined( OVR_OS_ANDROID )
	int trackedRemoteIndex = 0;

	for ( int i = 0; ; i++ )
	{
		ovrInputCapabilityHeader cap;
		ovrResult result = vrapi_EnumerateInputDevices( ovr, i, &cap );
		if ( result < 0 )
		{
			break;
		}

		if ( cap.Type == ovrControllerType_Headset )
		{
			ovrInputStateHeadset headsetInputState;
			headsetInputState.Header.ControllerType = ovrControllerType_Headset;
			result = vrapi_GetCurrentInputState( ovr, cap.DeviceID, &headsetInputState.Header );
			if ( result == ovrSuccess )
			{
				backButtonDownThisFrame |= headsetInputState.Buttons & ovrButton_Back;

				if ( headsetInputState.TrackpadStatus )
				{
					ovrInputHeadsetCapabilities headsetCapabilities;
					headsetCapabilities.Header.Type = ovrControllerType_Headset;
					headsetCapabilities.Header.DeviceID = cap.DeviceID;
					ovrResult result = vrapi_GetInputDeviceCapabilities( ovr, &headsetCapabilities.Header );

					if ( result == ovrSuccess )
					{
						vrFrame.Input.buttonState |= BUTTON_TOUCH;
						vrFrame.Input.touch[0] = headsetCapabilities.TrackpadMaxX - headsetInputState.TrackpadPosition.x;
						vrFrame.Input.touch[1] = headsetCapabilities.TrackpadMaxY - headsetInputState.TrackpadPosition.y;
					}
				}
			}
		}
		else if ( cap.Type == ovrControllerType_TrackedRemote )
		{
			float minTouchMove = 5.0f;

			ovrInputStateTrackedRemote trackedRemoteState;
			trackedRemoteState.Header.ControllerType = ovrControllerType_TrackedRemote;
			result = vrapi_GetCurrentInputState( ovr, cap.DeviceID, &trackedRemoteState.Header );
			if ( result == ovrSuccess )
			{
				backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Back;

				bool setTouched = trackedRemoteState.Buttons & ovrButton_A || trackedRemoteState.Buttons & ovrButton_Enter || ( trackedRemoteState.TrackpadStatus && TreatRemoteTouchpadTouchedAsButtonDown[trackedRemoteIndex] );

				ovrInputTrackedRemoteCapabilities remoteCapabilities;
				remoteCapabilities.Header.Type = ovrControllerType_TrackedRemote;
				remoteCapabilities.Header.DeviceID = cap.DeviceID;
				ovrResult result = vrapi_GetInputDeviceCapabilities( ovr, &remoteCapabilities.Header );
				OVR_UNUSED( result );

				setTouched |= trackedRemoteState.Buttons & ovrButton_Trigger;

				if ( result == ovrSuccess )
				{
					if ( remoteCapabilities.ControllerCapabilities & ovrControllerCaps_HasJoystick )
					{
						trackedRemoteState.TrackpadStatus = 0;
						trackedRemoteState.TrackpadPosition.x = 0.0f;
						trackedRemoteState.TrackpadPosition.y = 0.0f;

						if ( remoteCapabilities.ControllerCapabilities & ovrControllerCaps_LeftHand )
						{
							vrFrame.Input.sticks[0][0] = trackedRemoteState.Joystick.x;
							vrFrame.Input.sticks[0][1] = trackedRemoteState.Joystick.y;
						}
						else
						{
							vrFrame.Input.sticks[1][0] = trackedRemoteState.Joystick.x;
							vrFrame.Input.sticks[1][1] = trackedRemoteState.Joystick.y;
						}
					}

					if ( remoteCapabilities.ControllerCapabilities & ovrControllerCaps_ModelOculusTouch )
					{
						// to match the Rift, the menu button returns ovrButton_Enter
						backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Enter;

						// to match design, the b button and the y button will also be treated as back buttons for the app framework apps when using the Oculus Touch controllers.
						backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_B;
						backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Y;

						setTouched = trackedRemoteState.Buttons & ovrButton_A || trackedRemoteState.Buttons & ovrButton_Trigger || ( trackedRemoteState.TrackpadStatus && TreatRemoteTouchpadTouchedAsButtonDown[trackedRemoteIndex] );

						injectLeftStick = true;
					}
				}

				if ( trackedRemoteState.Buttons & ovrButton_Enter || !trackedRemoteState.TrackpadStatus )
				{
					TreatRemoteTouchpadTouchedAsButtonDown[trackedRemoteIndex] = false;
				}
				else if ( !setTouched && trackedRemoteState.TrackpadStatus && RemoteTouchpadTouchedLastFrame[trackedRemoteIndex] )
				{
					// determine if we've moved far enough to count it as a touch, don't do this first frame because there will be a big jump.

					const float diffMoveX = RemoteTouchpadPositionLastFrame[trackedRemoteIndex][0] - trackedRemoteState.TrackpadPosition.x;
					const float diffMoveY = RemoteTouchpadPositionLastFrame[trackedRemoteIndex][1] - trackedRemoteState.TrackpadPosition.y;

					if ( minTouchMove < fmax( fabsf( diffMoveX ), fabsf( diffMoveY ) ) )
					{
						setTouched = true;
						TreatRemoteTouchpadTouchedAsButtonDown[trackedRemoteIndex] = true;
					}
				}

				if ( setTouched )
				{
					vrFrame.Input.buttonState |= BUTTON_TOUCH;
					if ( trackedRemoteState.Buttons & ovrButton_Enter )
					{
						vrFrame.Input.touch = touchOrigin;
					}
					else
					{
						vrFrame.Input.touch[0] = remoteCapabilities.TrackpadMaxX - trackedRemoteState.TrackpadPosition.x;
						vrFrame.Input.touch[1] = remoteCapabilities.TrackpadMaxY - trackedRemoteState.TrackpadPosition.y;
					}
				}
				else
				{
					TreatRemoteTouchpadTouchedAsButtonDown[trackedRemoteIndex] = false;
				}
			}

			RemoteTouchpadTouchedLastFrame[trackedRemoteIndex] = trackedRemoteState.TrackpadStatus;
			RemoteTouchpadPositionLastFrame[trackedRemoteIndex][0] = trackedRemoteState.TrackpadPosition.x;
			RemoteTouchpadPositionLastFrame[trackedRemoteIndex][1] = trackedRemoteState.TrackpadPosition.y;

			if ( trackedRemoteIndex == 0 )
			{
				trackedRemoteIndex = 1;
			}
		}
		else if ( cap.Type == ovrControllerType_Gamepad )
		{
			ovrInputStateGamepad gamepadState;
			gamepadState.Header.ControllerType = ovrControllerType_Gamepad;
			result = vrapi_GetCurrentInputState( ovr, cap.DeviceID, &gamepadState.Header );
			if ( result == ovrSuccess )
			{
				backButtonDownThisFrame |= gamepadState.Buttons & ovrButton_Back;
			}
		}
	}
#endif

	// Clear the key events.
	vrFrame.Input.NumKeyEvents = 0;

	// Copy the key events.
	for ( int i = 0; i < inputEvents.NumKeyEvents && vrFrame.Input.NumKeyEvents < MAX_KEY_EVENTS_PER_FRAME; i++ )
	{
		// The back key is already handled.
		if ( inputEvents.KeyEvents[i].KeyCode == OVR_KEY_ESCAPE )
		{
			continue;
		}
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = inputEvents.KeyEvents[i].KeyCode;
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = inputEvents.KeyEvents[i].RepeatCount;
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = inputEvents.KeyEvents[i].Down ? KEY_EVENT_DOWN : KEY_EVENT_UP;
		vrFrame.Input.NumKeyEvents++;
	}

	if ( injectLeftStick )
	{
		vrFrame.Input.buttonState &= ~BUTTON_LSTICK_UP;
		vrFrame.Input.buttonState &= ~BUTTON_LSTICK_DOWN;
		vrFrame.Input.buttonState &= ~BUTTON_LSTICK_RIGHT;
		vrFrame.Input.buttonState &= ~BUTTON_LSTICK_LEFT;

		LStick.ResetDirection();

		LStick.StickPos.x = vrFrame.Input.sticks[0][0];
		LStick.StickPos.y = vrFrame.Input.sticks[0][1];
		ovrKeyCode leftkeycode = OVR_KEY_NONE;

		// if LeftJoystick is being used then determine the direction
		if ( LStick.StickPos.x != 0 && LStick.StickPos.y != 0 )
		{
			if ( LStick.StickPos.x > LStick.DeadZone )
			{
				LStick.StickRight = true;
				leftkeycode = OVR_KEY_LSTICK_RIGHT;
			}
			else if ( LStick.StickPos.x < ( -1.0f * LStick.DeadZone ) )
			{
				LStick.StickLeft = true;
				leftkeycode = OVR_KEY_LSTICK_LEFT;
			}
			else if ( LStick.StickPos.y < ( -1.0f * LStick.DeadZone ) )
			{
				LStick.StickUp = true;
				leftkeycode = OVR_KEY_LSTICK_UP;
			}
			else if ( LStick.StickPos.y > LStick.DeadZone )
			{
				LStick.StickDown = true;
				leftkeycode = OVR_KEY_LSTICK_DOWN;
			}

			if ( leftkeycode != OVR_KEY_NONE )
			{
				LStick.SetCurrStick( leftkeycode, true );
			}
		}
	}


	vrFrame.Input.buttonState &= ~BUTTON_RSTICK_UP;
	vrFrame.Input.buttonState &= ~BUTTON_RSTICK_DOWN;
	vrFrame.Input.buttonState &= ~BUTTON_RSTICK_RIGHT;
	vrFrame.Input.buttonState &= ~BUTTON_RSTICK_LEFT;

	RStick.ResetDirection();

	RStick.StickPos.x = vrFrame.Input.sticks[1][0];
	RStick.StickPos.y = vrFrame.Input.sticks[1][1];
	ovrKeyCode rightkeycode = OVR_KEY_NONE;

	// if RightJoystick is being used then determine the direction
	if ( RStick.StickPos.x != 0 && RStick.StickPos.y != 0 )
	{
		if ( RStick.StickPos.x > RStick.DeadZone )
		{
			RStick.StickRight = true;
			rightkeycode = OVR_KEY_RSTICK_RIGHT;
		}
		else if ( RStick.StickPos.x < ( -1.0f * RStick.DeadZone ) )
		{
			RStick.StickLeft = true;
			rightkeycode = OVR_KEY_RSTICK_LEFT;
		}
		else if ( RStick.StickPos.y < ( -1.0f * RStick.DeadZone ) )
		{
			RStick.StickUp = true;
			rightkeycode = OVR_KEY_RSTICK_UP;
		}
		else if ( RStick.StickPos.y > RStick.DeadZone )
		{
			RStick.StickDown = true;
			rightkeycode = OVR_KEY_RSTICK_DOWN;
		}

		if ( rightkeycode != OVR_KEY_NONE )
		{
			RStick.SetCurrStick( rightkeycode, true );
		}
	}

	// Create a fake event as if Joystick was used.
	if ( vrFrame.Input.NumKeyEvents == 0 )
	{
		if ( injectLeftStick )
		{
			// if the LeftJoystick is pressed
			if ( LStick.CurrStickState )
			{
				vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = LStick.CurrStickCode;
				vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = 0;
				vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = KEY_EVENT_DOWN;
				vrFrame.Input.NumKeyEvents++;
			}
			else if ( ( !LStick.CurrStickState && LStick.LastStickState ) ) // if the LeftJoystick is released
			{
				vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = LStick.LastStickCode;
				vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = 0;
				vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = KEY_EVENT_UP;
				vrFrame.Input.NumKeyEvents++;
			}
		}

		// if the RightJoystick is pressed
		if ( RStick.CurrStickState )
		{
			vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = RStick.CurrStickCode;
			vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = 0;
			vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = KEY_EVENT_DOWN;
			vrFrame.Input.NumKeyEvents++;
		}
		else if ( ( !RStick.CurrStickState && RStick.LastStickState ) ) // if the RightJoystick is released
		{
			vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = RStick.LastStickCode;
			vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = 0;
			vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = KEY_EVENT_UP;
			vrFrame.Input.NumKeyEvents++;
		}
	}

	// Clear previously set swipe buttons.
	vrFrame.Input.buttonState &= ~(	BUTTON_SWIPE_UP |
									BUTTON_SWIPE_DOWN |
									BUTTON_SWIPE_FORWARD |
									BUTTON_SWIPE_BACK |
									BUTTON_TOUCH_WAS_SWIPE |
									BUTTON_TOUCH_SINGLE |
									BUTTON_TOUCH_DOUBLE |
									BUTTON_TOUCH_LONGPRESS );

	// Update the joypad buttons using the key events.
	for ( int i = 0; i < vrFrame.Input.NumKeyEvents; i++ )
	{
		const ovrKeyCode keyCode = vrFrame.Input.KeyEvents[i].KeyCode;
		bool down = inputEvents.KeyEvents[i].Down;

		if ( RStick.CurrStickState ) // if RStick is used
		{
			down = true;
			RStick.SetLastStick();
			RStick.ResetCurrStick();
		}
		else if ( !RStick.CurrStickState && RStick.LastStickState ) // if RStick is Released
		{
			down = false;
			RStick.ResetLastStick();
		} 
		else if ( injectLeftStick )
		{
			if ( LStick.CurrStickState ) // if LStick is used
			{
				down = true;
				LStick.SetLastStick();
				LStick.ResetCurrStick();
			}
			else if ( !LStick.CurrStickState && LStick.LastStickState ) // if LStick is Released
			{
				down = false;
				LStick.ResetLastStick();
			}
		}

		// Keys always map to joystick buttons right now.
		for ( int j = 0; buttonMappings[j].KeyCode != OVR_KEY_MAX; j++ )
		{
			if ( keyCode == buttonMappings[j].KeyCode )
			{
				if ( down )
				{
					vrFrame.Input.buttonState |= buttonMappings[j].ButtonBit;
				}
				else
				{
					vrFrame.Input.buttonState &= ~buttonMappings[j].ButtonBit;
				}
				break;
			}
		}

		if ( down && 0 /* keyboard swipes */ )
		{
			if ( keyCode == OVR_KEY_CLOSE_BRACKET )
			{
				vrFrame.Input.buttonState |= BUTTON_SWIPE_FORWARD;
			} 
			else if ( keyCode == OVR_KEY_OPEN_BRACKET )
			{
				vrFrame.Input.buttonState |= BUTTON_SWIPE_BACK;
			}
		}
	}
	// Note joypad button transitions.
	vrFrame.Input.buttonPressed = vrFrame.Input.buttonState & ( ~lastVrInput.buttonState );
	vrFrame.Input.buttonReleased = ~vrFrame.Input.buttonState & ( lastVrInput.buttonState & ~BUTTON_TOUCH_WAS_SWIPE );

	if ( lastVrInput.buttonState & BUTTON_TOUCH_WAS_SWIPE )
	{
		if ( lastVrInput.buttonReleased & BUTTON_TOUCH )
		{
			vrFrame.Input.buttonReleased |= BUTTON_TOUCH_WAS_SWIPE;
		}
		else
		{
			// keep it around this frame
			vrFrame.Input.buttonState |= BUTTON_TOUCH_WAS_SWIPE;
		}
	}

	// Synthesize swipe gestures as buttons.
	const double currentTime = vrapi_GetTimeInSeconds();
	InterpretTouchpad( vrFrame.Input, currentTime, touchpadMinSwipe );

	// Add the short back press to the event list
	if ( BackKeyDownLastFrame && !backButtonDownThisFrame )
	{
		AddKeyEventToFrame( OVR_KEY_BACK, KEY_EVENT_SHORT_PRESS, 0 );
	}

	BackKeyDownLastFrame = backButtonDownThisFrame;

	// This is the only place FrameNumber gets incremented,
	// right before calling vrapi_GetPredictedDisplayTime().
	vrFrame.FrameNumber++;

	// Get the latest head tracking state, predicted ahead to the midpoint of the display refresh
	// at which the next frame will be displayed.  It will always be corrected to the real values
	// by the time warp, but the closer we get, the less black will be pulled in at the edges.
	double predictedDisplayTime = vrapi_GetPredictedDisplayTime( ovr, vrFrame.FrameNumber );

	// Make sure time always moves forward.
	if ( predictedDisplayTime <= vrFrame.PredictedDisplayTimeInSeconds )
	{
		predictedDisplayTime = vrFrame.PredictedDisplayTimeInSeconds + 0.001;
	}

	vrFrame.Tracking = vrapi_GetPredictedTracking2( ovr, predictedDisplayTime );
	vrFrame.DeltaSeconds = Alg::Clamp( (float)( predictedDisplayTime - vrFrame.PredictedDisplayTimeInSeconds ), 0.0f, 0.1f );
	vrFrame.PredictedDisplayTimeInSeconds = predictedDisplayTime;

	const ovrPosef trackingPose = vrapi_LocateTrackingSpace( ovr, vrapi_GetTrackingSpace( ovr ) );
	const ovrPosef eyeLevelTrackingPose = vrapi_LocateTrackingSpace( ovr, VRAPI_TRACKING_SPACE_LOCAL );
	vrFrame.EyeHeight = vrapi_GetEyeHeight( &eyeLevelTrackingPose, &trackingPose );

	vrFrame.IPD = vrapi_GetInterpupillaryDistance( &vrFrame.Tracking );

	// Update device status.
	vrFrame.DeviceStatus.HeadPhonesPluggedState		= HeadPhonesPluggedState;
	vrFrame.DeviceStatus.DeviceIsDocked				= ( vrapi_GetSystemStatusInt( &java, VRAPI_SYS_STATUS_DOCKED ) != VRAPI_FALSE );
	vrFrame.DeviceStatus.HeadsetIsMounted			= ( vrapi_GetSystemStatusInt( &java, VRAPI_SYS_STATUS_MOUNTED ) != VRAPI_FALSE );
}

}	// namespace OVR

#if defined( OVR_OS_ANDROID )
extern "C"
{
JNIEXPORT void Java_com_oculus_vrappframework_HeadsetReceiver_headsetStateChanged( JNIEnv * jni, jclass clazz, jint state )
{
	OVR_LOG( "nativeHeadsetEvent(%i)", state );
	OVR::HeadPhonesPluggedState = ( state == 1 ) ? OVR::OVR_HEADSET_PLUGGED : OVR::OVR_HEADSET_UNPLUGGED;
}
}	// extern "C"
#endif
