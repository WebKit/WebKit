#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioOutputUnit.h>)
/*!
	@file		AudioOutputUnit.h
 	@framework	AudioToolbox.framework
 	@copyright	(c) 2000-2015 Apple, Inc. All rights reserved.
	@brief		Additional Audio Unit API for audio input/output units.
*/

#ifndef AudioUnit_AudioOutputUnit_h
#define AudioUnit_AudioOutputUnit_h

#include <AudioToolbox/AUComponent.h>

CF_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
//	Start/stop methods for output units
//-----------------------------------------------------------------------------
extern OSStatus
AudioOutputUnitStart(	AudioUnit	ci)											API_AVAILABLE(macos(10.0), ios(2.0), watchos(2.0), tvos(9.0));

extern OSStatus
AudioOutputUnitStop(	AudioUnit	ci)											API_AVAILABLE(macos(10.0), ios(2.0), watchos(2.0), tvos(9.0));

//-----------------------------------------------------------------------------
//	Selectors for component and audio plugin calls
//-----------------------------------------------------------------------------
enum {
	kAudioOutputUnitRange						= 0x0200,	// selector range
	kAudioOutputUnitStartSelect					= 0x0201,
	kAudioOutputUnitStopSelect					= 0x0202
};

/*!
*/
typedef OSStatus	(*AudioOutputUnitStartProc) (void *self);

/*!
*/
typedef OSStatus	(*AudioOutputUnitStopProc) (void *self);


#ifdef __cplusplus
}
#endif

CF_ASSUME_NONNULL_END

#endif /* AudioUnit_AudioOutputUnit_h */
#else
#include <AudioToolboxCore/AudioOutputUnit.h>
#endif
