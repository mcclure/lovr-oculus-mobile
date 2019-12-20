/************************************************************************************

Filename    :   OVR_Skeleton.h
Content     :   skeleton for arm model implementation
Created     :   2/20/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>

#include "VrApi_Types.h"
#include "OVR_Math.h"

namespace OVRFW {

template< typename T1, typename T2 = T1 >
struct ovrPairT 
{
	ovrPairT()
		: First()
		, Second()
	{
	}

	ovrPairT( T1 first, T2 second )
		: First( first )
		, Second( second )
	{
	}

	T1	First;
	T2	Second;
};

class ovrJoint
{
public:
	ovrJoint()
		: Name( nullptr )
		, Color( 1.0f )
		, ParentIndex( -1 )
	{
	}

	ovrJoint( const char * name, const OVR::Vector4f & color, const OVR::Posef & pose, const int parentIndex )
		: Name( name )
		, Color( color )
		, Pose( pose )
		, ParentIndex( parentIndex )		
	{
	}

	char const *	Name;			// name of the joint
	OVR::Vector4f	Color;
	OVR::Posef		Pose;			// pose of this joint
	int				ParentIndex;	// index of this joint's parent
};

class ovrJointMod
{
public:
	enum ovrJointModType
	{
		MOD_INVALID = -1,
		MOD_LOCAL,	// apply in joint local space
		MOD_WORLD	// apply in world space
	};

	ovrJointMod()
		: Type( MOD_INVALID )
		, JointIndex( -1 )
	{
	}

	ovrJointMod( const ovrJointModType type, const int jointIndex, const OVR::Posef & pose )
		: Type( type )
		, JointIndex( jointIndex )
		, Pose( pose )
	{
	}

	ovrJointModType	Type;
	int				JointIndex;
	OVR::Posef		Pose;
};

class ovrSkeleton
{
public:
	ovrSkeleton();

	const ovrJoint &				GetJoint( int const idx ) const;

	int								GetParentIndex( int const idx ) const;

	std::vector< ovrJoint > &		GetJoints() { return Joints;  }
	const std::vector< ovrJoint > &	GetJoints() const { return Joints;  }

	static void						Transform( const OVR::Posef & transformPose, const OVR::Posef & inPose, OVR::Posef & outPose );

	static void						TransformByParent( const OVR::Posef & parentPose, const int jointIndex, const OVR::Posef & inPose, 
										const std::vector< ovrJointMod > & jointMods, OVR::Posef & outPose );
	static void						Transform( const OVR::Posef & worldPose, const std::vector< ovrJoint > & inJoints, 
										const std::vector< ovrJointMod > & jointMods, std::vector< ovrJoint > & outJoints );

	static void						ApplyJointMods( const std::vector< ovrJointMod > & jointMods, std::vector< ovrJoint > & joints );

private:
	std::vector< ovrJoint >			Joints;
};

void InitArmSkeleton( ovrSkeleton & skel );

} // namespace OVRFW
