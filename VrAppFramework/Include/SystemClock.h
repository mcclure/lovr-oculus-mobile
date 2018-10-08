/************************************************************************************

Filename    :   SystemClock.h
Content     :
Created     :
Authors     :

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#if !defined( OVR_SystemClock )
#define OVR_SystemClock

class SystemClock
{
public:
	static double GetTimeInNanoSeconds();
	static double GetTimeInSeconds();
	static double GetTimeOfDaySeconds();
	static double DeltaTimeInSeconds( double startTime );
	static double DeltaTimeInSeconds( double startTime, double endTime );
};

#endif // OVR_SystemClock
