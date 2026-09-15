#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioUnit.h>)
/*!
	@file		AudioUnit.h
 	@framework	AudioToolbox.framework
 	@copyright	(c) 2002-2015 Apple, Inc. All rights reserved.

	@brief		Former umbrella header for AudioUnit framework; now part of AudioToolbox.
*/

#ifndef _AudioToolbox_AudioUnit_h
#define _AudioToolbox_AudioUnit_h

#include <TargetConditionals.h>
#ifndef AUDIO_UNIT_VERSION
#define AUDIO_UNIT_VERSION 1070
#endif

#include <AudioToolbox/AudioComponent.h>
#include <AudioToolbox/AUComponent.h>
#include <AudioToolbox/AudioOutputUnit.h>
#include <AudioToolbox/AudioUnitProperties.h>
#include <AudioToolbox/AudioUnitParameters.h>
#include <AudioToolbox/MusicDevice.h>

#ifdef __OBJC2__
	#import <AudioToolbox/AUAudioUnit.h>
	#import <AudioToolbox/AUAudioUnitImplementation.h>
	#import <AudioToolbox/AUParameters.h>
#endif

#if !TARGET_OS_IPHONE
	#include <AudioToolbox/AudioCodec.h>
#endif

#endif /* _AudioToolbox_AudioUnit_h */
#else
#include <AudioToolboxCore/AudioUnit.h>
#endif
