/*!
	@file		AudioServices.h
	@framework	AudioToolbox.framework
	@copyright	(c) 2006-2015 by Apple, Inc., all rights reserved.
    @abstract   API's for general high level audio services.

    @discussion

    AudioServices provides a means to play audio for things such as UI sound effects.
*/

#ifndef AudioToolbox_AudioServices_h
#define AudioToolbox_AudioServices_h

//==================================================================================================
#pragma mark    Includes

#include <TargetConditionals.h>
#include <Availability.h>
#if TARGET_OS_OSX
    #include <CoreAudio/AudioHardware.h>
    #include <AudioToolbox/AudioHardwareService.h>
#elif TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    #include <AudioToolbox/AudioSession.h>
#endif
#include <CoreFoundation/CoreFoundation.h>

//==================================================================================================
CF_ASSUME_NONNULL_BEGIN

#if defined(__cplusplus)
extern "C"
{
#endif

//==================================================================================================
#pragma mark    AudioServices Error Constants

/*!
    @enum           AudioServices error codes
    @abstract       Error codes returned from the AudioServices portion of the API.
    @constant       kAudioServicesNoError
                        No error has occurred
    @constant       kAudioServicesUnsupportedPropertyError 
                        The property is not supported.
    @constant       kAudioServicesBadPropertySizeError 
                        The size of the property data was not correct.
    @constant       kAudioServicesBadSpecifierSizeError 
                        The size of the specifier data was not correct.
    @constant       kAudioServicesSystemSoundUnspecifiedError 
                        A SystemSound unspecified error has occurred.
    @constant       kAudioServicesSystemSoundClientTimedOutError 
                        The SystemSound client message timed out
    @constant       kAudioServicesSystemSoundExceededMaximumDurationError
                        The SystemSound's duration exceeded the allowable threshold
*/
CF_ENUM(OSStatus)
{
	kAudioServicesNoError									=  0,
	kAudioServicesUnsupportedPropertyError					= 'pty?',
	kAudioServicesBadPropertySizeError						= '!siz',
	kAudioServicesBadSpecifierSizeError 					= '!spc',

	kAudioServicesSystemSoundUnspecifiedError				= -1500,
	kAudioServicesSystemSoundClientTimedOutError			= -1501,
    kAudioServicesSystemSoundExceededMaximumDurationError   = -1502
};

//==================================================================================================
#pragma mark    AudioServices Types

/*!
    @typedef        SystemSoundID
    @abstract       SystemSoundIDs are created by the System Sound client application
                    for playback of a provided AudioFile.
*/
typedef UInt32      SystemSoundID;

/*!
    @typedef        AudioServicesPropertyID
    @abstract       Type used for specifying an AudioServices property.
*/
typedef UInt32      AudioServicesPropertyID;

/*!
    @typedef        AudioServicesSystemSoundCompletionProc
    @abstract       A function to be executed when a SystemSoundID finishes playing.
    @discussion     AudioServicesSystemSoundCompletionProc may be provided by client application to be
                    called when a SystemSoundID has completed playback.
    @param          ssID
                        The SystemSoundID that completed playback
    @param          clientData
                        Client application user data
*/
typedef void
(*AudioServicesSystemSoundCompletionProc)(  SystemSoundID       ssID,
                                        	void* __nullable    clientData);

//==================================================================================================
#pragma mark    AudioServices Constants

#if !TARGET_OS_IPHONE
/*!
    @enum           AudioServices constants
    @abstract       Constants for use with System Sound portion of the AudioServices APIs.
    @constant       kSystemSoundID_UserPreferredAlert 
                        Use this constant with the play sound APIs to
                        playback the alert sound selected by the User in System Preferences.
    @constant       kSystemSoundID_FlashScreen
                        Use this constant with the play sound APIs to flash the screen
                        - Desktop systems only
*/
CF_ENUM(SystemSoundID)
{
    kSystemSoundID_UserPreferredAlert   = 0x00001000,
    kSystemSoundID_FlashScreen          = 0x00000FFE,
        // this has been renamed to be consistent
    kUserPreferredAlert     = kSystemSoundID_UserPreferredAlert
};
#endif

/*!
    @enum           AudioServices constants
    @constant       kSystemSoundID_Vibrate
                        Use this constant with the play sound APIs to vibrate the device
                        - iOS only 
                            - on a device with no vibration capability (like iPod Touch) this will 
                            do nothing
*/
CF_ENUM(SystemSoundID)
{
    kSystemSoundID_Vibrate              = 0x00000FFF
};

//==================================================================================================
#pragma mark    AudioServices Properties

/*!
    @enum           AudioServices property codes
    @abstract       These are the property codes used with the AudioServices API.
    @constant       kAudioServicesPropertyIsUISound
                        a UInt32 where 1 means that the SystemSoundID passed in the
                        inSpecifier parameter will respect the 'Play user interface sounds effects'
                        check box in System Preferences and be silent when the user turns off UI
                        sounds. This property is set to 1 by default. Set to 0 if it is desired for
                        an SystemSoundID to always be heard when passed to AudioServicesPlaySystemSound(), 
                        regardless of user's setting in the Sound Preferences.
    @constant       kAudioServicesPropertyCompletePlaybackIfAppDies 
                        a UInt32 where 1 means that the SystemSoundID passed in the
                        inSpecifier parameter will finish playing even if the client application goes away.
*/
CF_ENUM(AudioServicesPropertyID)
{
    kAudioServicesPropertyIsUISound                   = 'isui',
    kAudioServicesPropertyCompletePlaybackIfAppDies   = 'ifdi'
};

//==================================================================================================
#pragma mark    AudioServices Functions

/*!
    @functiongroup  AudioServices
*/

/*!
    @function       AudioServicesCreateSystemSoundID
    @abstract       Allows the application to designate an audio file for playback by the System Sound server.
    @discussion     Returned SystemSoundIDs are passed to AudioServicesPlayAlertSoundWithCompletion() 
                    and AudioServicesPlaySystemSoundWithCompletion() to be played.
 
                    The maximum supported duration for a system sound is 30 secs.
    @param          inFileURL
                        A CFURLRef for an AudioFile.
    @param          outSystemSoundID
                        Returns a SystemSoundID.
*/
extern OSStatus 
AudioServicesCreateSystemSoundID(   CFURLRef                    inFileURL,
                                    SystemSoundID*              outSystemSoundID)
                                                                API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0))
                                                                ;
	
/*!
    @function       AudioServicesDisposeSystemSoundID
    @abstract       Allows the System Sound server to dispose any resources needed for the provided
                    SystemSoundID.
    @discussion     Allows the application to tell the System Sound server that the resources for the
                    associated audio file are no longer required.
    @param          inSystemSoundID
                        A SystemSoundID that the application no longer needs to use.
*/
extern OSStatus 
AudioServicesDisposeSystemSoundID(SystemSoundID inSystemSoundID)                                    
                                                                API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0))
                                                                ;

/*!
    @function       AudioServicesPlayAlertSoundWithCompletion
    @abstract       Play an alert sound
    @discussion     Play the sound designated by the provided SystemSoundID with alert sound behavior.
    @param          inSystemSoundID
                        The SystemSoundID to be played. On the desktop the kSystemSoundID_UserPreferredAlert
                        constant can be passed in to play back the alert sound selected by the user
                        in System Preferences. On iOS there is no preferred user alert sound.
    @param          inCompletionBlock
                        The completion block gets executed for every attempt to play a system sound irrespective
                        of success or failure. The callbacks are issued on a serial queue and the client is
                        responsible for handling thread safety.
*/
extern void
AudioServicesPlayAlertSoundWithCompletion(  SystemSoundID inSystemSoundID,
                                            void (^__nullable inCompletionBlock)(void))
                                                                    API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
                                                                    ;
                                                                
/*!
    @function       AudioServicesPlaySystemSoundWithCompletion
    @abstract       Play a system sound
    @discussion     Play the sound designated by the provided SystemSoundID.
    @param          inSystemSoundID
                        The SystemSoundID to be played.
    @param          inCompletionBlock
                        The completion block gets executed for every attempt to play a system sound irrespective 
                        of success or failure. The callbacks are issued on a serial queue and the client is 
                        responsible for handling thread safety.
*/
extern void
AudioServicesPlaySystemSoundWithCompletion(     SystemSoundID inSystemSoundID,
                                                void (^__nullable inCompletionBlock)(void))
                                                                        API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
                                                                        ;
                                                                
/*!
    @function       AudioServicesGetPropertyInfo
    @abstract       Get information about the size of an AudioServices property and whether it can
                    be set.
    @param          inPropertyID
                        a AudioServicesPropertyID constant.
    @param          inSpecifierSize
                        The size of the specifier data.
    @param          inSpecifier
                        A specifier is a buffer of data used as an input argument to some of the
                        properties.
    @param          outPropertyDataSize
                        The size in bytes of the current value of the property. In order to get the
                        property value, you will need a buffer of this size.
    @param          outWritable
                        Will be set to 1 if writable, or 0 if read only.
    @result         returns kAudioServicesNoError if successful.
*/                            
extern OSStatus 
AudioServicesGetPropertyInfo( AudioServicesPropertyID   inPropertyID,
                              UInt32                    inSpecifierSize,
                              const void * __nullable   inSpecifier,
                              UInt32 * __nullable       outPropertyDataSize,
                              Boolean * __nullable      outWritable)
                                                                    API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0))
                                                                    ;

/*!
    @function       AudioServicesGetProperty
    @abstract       Retrieve the indicated property data
    @param          inPropertyID
                        a AudioServicesPropertyID constant.
    @param          inSpecifierSize
                        The size of the specifier data.
    @param          inSpecifier
                        A specifier is a buffer of data used as an input argument to some of the
                        properties.
    @param          ioPropertyDataSize
                        On input, the size of the outPropertyData buffer. On output the number of
                        bytes written to the buffer.
    @param          outPropertyData
                        The buffer in which to write the property data. May be NULL if caller only
                        wants ioPropertyDataSize to be filled with the amount that would have been
                        written.
    @result         returns kAudioServicesNoError if successful.
*/                        
extern OSStatus 
AudioServicesGetProperty(   AudioServicesPropertyID         inPropertyID,
                            UInt32                          inSpecifierSize,
                            const void * __nullable         inSpecifier,
                            UInt32 *                        ioPropertyDataSize,
                            void * __nullable               outPropertyData)
                                                                API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0))
                                                                ;

/*!
    @function       AudioServicesSetProperty
    @abstract       Set the indicated property data
    @param          inPropertyID
                        a AudioServicesPropertyID constant.
    @param          inSpecifierSize
                        The size of the specifier data.
    @param          inSpecifier
                        A specifier is a buffer of data used as an input argument to some of the
                        properties.
    @param          inPropertyDataSize
                        The size of the inPropertyData buffer.
    @param          inPropertyData
                        The buffer containing the property data.
    @result         returns kAudioServicesNoError if successful.
*/
extern OSStatus 
AudioServicesSetProperty(   AudioServicesPropertyID             inPropertyID,
                            UInt32                              inSpecifierSize,
                            const void * __nullable             inSpecifier,
                            UInt32                              inPropertyDataSize,
                            const void *                        inPropertyData)
                                                                API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0))
                                                                ;
                                                                
/*!
    This function will be deprecated in a future release. Use AudioServicesPlayAlertSoundWithCompletion instead.
 
    @function       AudioServicesPlayAlertSound
    @abstract       Play an Alert Sound
    @discussion     Play the provided SystemSoundID with AlertSound behavior.
    @param          inSystemSoundID
                        A SystemSoundID for the System Sound server to play. On the desktop you
                        can pass the kSystemSoundID_UserPreferredAlert constant to playback the alert sound 
                        selected by the user in System Preferences. On iOS there is no preferred user alert sound.
*/
extern void 
AudioServicesPlayAlertSound(SystemSoundID inSystemSoundID)                                          
                                                                API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0))
                                                                ;
                                                                
/*!
    This function will be deprecated in a future release. Use AudioServicesPlaySystemSoundWithCompletion instead.
 
    @function       AudioServicesPlaySystemSound
    @abstract       Play the sound designated by the provided SystemSoundID.
    @discussion     A SystemSoundID indicating the desired System Sound to be played.
    @param          inSystemSoundID
                        A SystemSoundID for the System Sound server to play.
*/
extern void 
AudioServicesPlaySystemSound(SystemSoundID inSystemSoundID)                                         
                                                                API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0))
                                                                ;
                                                                
/*!
    This function will be deprecated in a future release. Use AudioServicesPlayAlertSoundWithCompletion 
    or AudioServicesPlaySystemSoundWithCompletion instead.
 
    @function       AudioServicesAddSystemSoundCompletion
    @abstract       Call the provided Completion Routine when provided SystemSoundID
                    finishes playing.
    @discussion     Once set, the System Sound server will send a message to the System Sound client
                    indicating which SystemSoundID has finished playing.
    @param          inSystemSoundID
                        The SystemSoundID to associate with the provided completion
                        routine.
    @param          inRunLoop
                        A CFRunLoopRef indicating the desired run loop the completion routine should
                        be run on. Pass NULL to use the main run loop.
    @param          inRunLoopMode
                        A CFStringRef indicating the run loop mode for the runloop where the
                        completion routine will be executed. Pass NULL to use kCFRunLoopDefaultMode.
    @param          inCompletionRoutine
                        An AudioServicesSystemSoundCompletionProc to be called when the provided
                        SystemSoundID has completed playing in the server.
    @param          inClientData
                        A void* to pass client data to the completion routine.
*/
extern OSStatus 
AudioServicesAddSystemSoundCompletion(  SystemSoundID                                       inSystemSoundID,
                                    	CFRunLoopRef __nullable                             inRunLoop,
                                    	CFStringRef __nullable                              inRunLoopMode,
                                    	AudioServicesSystemSoundCompletionProc              inCompletionRoutine,
                                    	void * __nullable                                   inClientData)
                                                                    API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0))
                                                                    ;

/*!
    This function will be deprecated in a future release. Use AudioServicesPlayAlertSoundWithCompletion
    or AudioServicesPlaySystemSoundWithCompletion instead.
 
    @function       AudioServicesRemoveSystemSoundCompletion
    @abstract       Disassociate any completion proc for the specified SystemSoundID
    @discussion     Tells the SystemSound client to remove any completion proc associated with the
                    provided SystemSoundID
    @param          inSystemSoundID
                        The SystemSoundID for which completion routines should be
                        removed.
*/
extern void 
AudioServicesRemoveSystemSoundCompletion(SystemSoundID inSystemSoundID)                             
                                                                    API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0))
                                                                    ;

/*!
    @enum AudioServicesPlaySystemSoundWithDetails Dictionary Keys
    @abstract   Keys that are passed in a dictionary to AudioServicesPlaySystemSoundWithDetails
    @constant   kAudioServicesDetailIntendedSpatialExperience
                    Must be any non-nil CASpatialAudioExperience. The system sound
                    will have this spatial experience for the duration of its
                    playback and cannot change mid-playback.
*/
extern const CFStringRef kAudioServicesDetailIntendedSpatialExperience API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios, watchos, tvos, macos) CF_REFINED_FOR_SWIFT;

/*!
    @function       AudioServicesPlaySystemSoundWithDetails
    @abstract       Play the sound designated by the provided SystemSoundID.
    @param          inSystemSoundID
                        A SystemSoundID for the system sound server to play.
    @param            inDetails
                        A set of details as described above.
    @param          inCompletionBlock
                        The completion block gets executed for every attempt to play a system sound irrespective
                        of success or failure. The callbacks are issued on a serial queue and the client is
                        responsible for handling thread safety.
*/
extern void AudioServicesPlaySystemSoundWithDetails(SystemSoundID inSystemSoundID,
                                                    CFDictionaryRef _Nullable inDetails,
                                                    void (^_Nullable inCompletionBlock)(void)) API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios, watchos, tvos, macos) CF_REFINED_FOR_SWIFT;

/*!
    @function       AudioServicesPlayAlertSoundWithDetails
    @abstract       Play the alert designated by the provided SystemSoundID.
    @param          inSystemSoundID
                        A SystemSoundID for the system sound server to play with alert sound behavior.
    @param            inDetails
                        A set of details as described above.
    @param          inCompletionBlock
                        The completion block gets executed for every attempt to play a system sound irrespective
                        of success or failure. The callbacks are issued on a serial queue and the client is
                        responsible for handling thread safety.
*/
extern void AudioServicesPlayAlertSoundWithDetails(SystemSoundID inSystemSoundID,
                                                   CFDictionaryRef _Nullable inDetails,
                                                   void (^_Nullable inCompletionBlock)(void)) API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios, watchos, tvos, macos) CF_REFINED_FOR_SWIFT;

CF_ASSUME_NONNULL_END
    
#ifdef __cplusplus
}
#endif

#endif /* AudioToolbox_AudioServices_h */
