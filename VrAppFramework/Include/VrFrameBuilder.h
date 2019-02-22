/************************************************************************************

Filename    :   VrFrameBuilder.h
Content     :   Builds the input for VrAppInterface::Frame()
Created     :   April 26, 2015
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#ifndef OVR_VrFrameBuilder_h
#define OVR_VrFrameBuilder_h

#include "OVR_Input.h"
#include "KeyState.h"

namespace OVR {

static const int MAX_INPUT_KEY_EVENTS = 16;

struct ovrInputEvents
{
	ovrInputEvents() :
		JoySticks(),
		TouchPosition(),
		TouchAction( -1 ),
		NumKeyEvents( 0 ),
		KeyEvents() {}

	float JoySticks[2][2];
	float TouchPosition[2];
	int TouchAction;
	int NumKeyEvents;
	struct ovrKeyEvents_t
	{
		ovrKeyEvents_t()
			: KeyCode( OVR_KEY_NONE )
			, RepeatCount( 0 )
			, Down( false ) 
			, IsJoypadButton( false )
		{
		}

		ovrKeyCode	KeyCode;
		int			RepeatCount;
		bool		Down : 1;
		bool		IsJoypadButton : 1;
	} KeyEvents[MAX_INPUT_KEY_EVENTS];
};

class VrFrameBuilder
{
public:
						VrFrameBuilder();

	void				Init( ovrJava * java );

	void				AdvanceVrFrame( const ovrInputEvents & inputEvents, ovrMobile * ovr,
										const ovrJava & java,
										const long long enteredVrModeFrameNumber );
	const ovrFrameInput &		Get() const { return vrFrame; }

private:
	ovrFrameInput vrFrame;

	double		lastTouchpadTime;
	double		touchpadTimer;
	bool		lastTouchDown;
	int			touchState;
	Vector2f	touchOrigin;

	bool		BackKeyDownLastFrame;
	bool		RemoteTouchpadTouchedLastFrame[2];
	Vector2f	RemoteTouchpadPositionLastFrame[2];
	bool		TreatRemoteTouchpadTouchedAsButtonDown[2];

	void 		InterpretTouchpad( VrInput & input, const double currentTime, const float min_swipe_distance );
	void		AddKeyEventToFrame( ovrKeyCode const keyCode, KeyEventType const eventType, int const repeatCount );

	// Joystick support to enable BUTTON_*STICK_UP, BUTTON_*STICK_DOWN, BUTTON_*STICK_LEFT and BUTTON_*STICK_RIGHT
	struct ovrJoyStick_t
	{

		const float DeadZone = 0.1f;
		bool LastStickState = false;
		bool CurrStickState = false;
		ovrKeyCode LastStickCode = OVR_KEY_NONE;
		ovrKeyCode CurrStickCode = OVR_KEY_NONE;
		Vector2f StickPos; // (x, y) position of Right Joystick

		bool StickLeft = false;
		bool StickRight = false;
		bool StickUp = false;
		bool StickDown = false;

		void ResetCurrStick()
		{
			CurrStickState = false;
			CurrStickCode = OVR_KEY_NONE;
		}

		void ResetLastStick()
		{
			LastStickState = false;
			LastStickCode = OVR_KEY_NONE;
		}

		void SetLastStick()
		{
			LastStickState = CurrStickState;
			LastStickCode = CurrStickCode;
		}

		void SetCurrStick( const ovrKeyCode keycode, const bool state )
		{
			CurrStickState = state;
			CurrStickCode = keycode;
		}

		void ResetDirection()
		{
			StickLeft = false;
			StickRight = false;
			StickUp = false;
			StickDown = false;
		}
	};
	
	ovrJoyStick_t RStick;
	ovrJoyStick_t LStick;
};

}	// namespace OVR

#endif // OVR_VrFrameBuilder_h
