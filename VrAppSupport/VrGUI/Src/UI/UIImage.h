/************************************************************************************

Filename    :   UIImage.h
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#if !defined( UIImage_h )
#define UIImage_h

#include "VRMenu.h"
#include "UI/UIObject.h"

namespace OVR {

class VrAppInterface;

class UIImage : public UIObject
{
public:
										UIImage( OvrGuiSys &guiSys );
										~UIImage();

	void 								AddToMenu( UIMenu *menu, UIObject *parent = NULL );
};

} // namespace OVR

#endif // UIImage_h
