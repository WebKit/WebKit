/*!
	@file		AudioToolbox.h
	@framework	AudioToolbox.framework
	@copyright	(c) 2002-2018 by Apple, Inc., all rights reserved.
	@abstract	Umbrella header for AudioToolbox framework.
*/


#ifndef AudioToolbox_AudioToolbox_h
#define AudioToolbox_AudioToolbox_h

#define AUDIO_TOOLBOX_VERSION 1060

#include <Availability.h>
#include <TargetConditionals.h>

#include <AudioToolbox/AudioCodec.h>
#include <AudioToolbox/AUAudioUnit.h>
#include <AudioToolbox/AUAudioUnitImplementation.h>
#include <AudioToolbox/AUComponent.h>
#include <AudioToolbox/AUGraph.h>
#include <AudioToolbox/AUHeadTrackingBinauralRenderer.h>
#include <AudioToolbox/AUParameters.h>
#include <AudioToolbox/AudioComponent.h>
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFile.h>
#include <AudioToolbox/AudioFileStream.h>
#include <AudioToolbox/AudioFormat.h>
#include <AudioToolbox/AudioOutputUnit.h>
#include <AudioToolbox/AudioQueue.h>
#include <AudioToolbox/AudioServices.h>
#include <AudioToolbox/AudioUnitParameters.h>
#include <AudioToolbox/AudioUnitProperties.h>
#include <AudioToolbox/AudioUnitUtilities.h>
#include <AudioToolbox/AudioWorkInterval.h>
#include <AudioToolbox/CAFFile.h>
#include <AudioToolbox/CAShow.h>
#if !TARGET_OS_OSX && !TARGET_OS_MACCATALYST
#include <AudioToolbox/CASpatialAudioExperience.h>
#endif
#include <AudioToolbox/ExtendedAudioFile.h>
#include <AudioToolbox/MusicDevice.h>
#include <AudioToolbox/MusicPlayer.h>

#if !TARGET_OS_IPHONE
	// macOS only
	#include <AudioToolbox/AudioFileComponent.h>
	#include <AudioToolbox/AUMIDIController.h>
	#include <AudioToolbox/CoreAudioClock.h>
#endif

#if TARGET_OS_OSX || TARGET_OS_IOS
	#include <AudioToolbox/AudioSession.h>
#endif

/*!	@mainpage

	@section section_intro			Introduction

	The AudioUnit framework contains a set of related API's dealing with:
	
	- Audio components, providing various types of plug-in functionality.
	- Audio Units, audio processing plug-ins.
	- Audio codecs, plug-ins which decode and encode compressed audio.
	
	@section section_component		Audio Components
	
	See AudioComponent.h for API's to find and use audio components, as well as information
	on how audio components are packaged and built.
	
	In addition, `<AVFoundation/AVAudioUnitComponent.h>` provides a higher-level interface for
	finding audio unit components.
	
	See @ref AUExtensionPackaging and AUAudioUnitImplementation.h for information on creating
	version 3 audio units.
	
	@section section_audiounit		Audio Units
*/

#include <stdio.h>

CF_ASSUME_NONNULL_BEGIN

#if defined(__cplusplus)
extern "C"
{
#endif

#if !TARGET_OS_IPHONE
// this will return the name of a sound bank from a sound bank file
// the name should be released by the caller
struct FSRef;
OS_EXPORT OSStatus GetNameFromSoundBank (const struct FSRef *inSoundBankRef, CFStringRef __nullable * __nonnull outName)
											API_DEPRECATED("no longer supported", macos(10.2, 10.5)) API_UNAVAILABLE(ios, watchos, tvos);
#endif

/*!
    @function		CopyNameFromSoundBank
	 
    @discussion		This will return the name of a sound bank from a DLS or SF2 bank.
					The name should be released by the caller.

    @param			inURL
						The URL for the sound bank.
    @param			outName
						A pointer to a CFStringRef to be created and returned by the function.
    @result			returns noErr if successful.
*/

OS_EXPORT OSStatus
CopyNameFromSoundBank (CFURLRef inURL, CFStringRef __nullable * __nonnull outName)
											API_AVAILABLE(macos(10.5), ios(7.0), watchos(2.0), tvos(9.0));

/*!
    @function		CopyInstrumentInfoFromSoundBank
	 
    @discussion		This will return a CFArray of CFDictionaries, one per instrument found in the DLS or SF2 bank.
					Each dictionary will contain four items accessed via CFStringRef versions of the keys kInstrumentInfoKey_MSB,
 					kInstrumentInfoKey_LSB, kInstrumentInfoKey_Program, and kInstrumentInfoKey_Name.
 						MSB: An NSNumberRef for the most-significant byte of the bank number.  GM melodic banks will return 120 (0x78).
 							 GM percussion banks will return 121 (0x79).  Custom banks will return their literal value.
						LSB: An NSNumberRef for the least-significant byte of the bank number.  All GM banks will return
							 the bank variation number (0-127).
 						Program Number: An NSNumberRef for the program number (0-127) of an instrument within a particular bank.
 						Name: A CFStringRef containing the name of the instrument.
 
					Using these MSB, LSB, and Program values will guarantee that the correct instrument is loaded by the DLS synth
					or Sampler Audio Unit.
					The CFArray should be released by the caller.

    @param			inURL
	 					The URL for the sound bank.
    @param			outInstrumentInfo
						A pointer to a CFArrayRef to be created and returned by the function.
    @result			returns noErr if successful.
*/

OS_EXPORT OSStatus
CopyInstrumentInfoFromSoundBank (CFURLRef inURL, CFArrayRef __nullable * __nonnull outInstrumentInfo)
														API_AVAILABLE(macos(10.8), ios(7.0), watchos(2.0), tvos(9.0));
	
#define kInstrumentInfoKey_Name		"name"
#define kInstrumentInfoKey_MSB		"MSB"
#define kInstrumentInfoKey_LSB		"LSB"
#define kInstrumentInfoKey_Program	"program"

#if defined(__cplusplus)
}
#endif

CF_ASSUME_NONNULL_END

#endif // AudioToolbox_AudioToolbox_h
