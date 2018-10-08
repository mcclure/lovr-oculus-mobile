/************************************************************************************

Filename    :   ActionComponents.h
Content     :   Misc. VRMenu Components to handle actions
Created     :   September 12, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "ActionComponents.h"

namespace OVR {

//==============================
// OvrButton_OnUp::OnEvent_Impl
eMsgStatus OvrButton_OnUp::OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
	VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_ASSERT( event.EventType == VRMENU_EVENT_TOUCH_UP );
	OVR_LOG( "Button id %lli clicked", ButtonId.Get() );
	Menu->OnItemEvent( guiSys, vrFrame, ButtonId, event );
	return MSG_STATUS_CONSUMED;
}

}
