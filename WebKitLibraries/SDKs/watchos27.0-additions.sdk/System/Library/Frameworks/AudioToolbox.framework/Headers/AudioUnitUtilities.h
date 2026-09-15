#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioUnitUtilities.h>)
/*!
	@file		AudioUnitUtilities.h
	@framework	AudioToolbox.framework
	@copyright	(c) 2002-2015 by Apple, Inc., all rights reserved.
	@abstract	Higher-level utility functions for the use of AudioUnit clients.

    @discussion

	The AU Parameter Listener is designed to provide notifications when an Audio Unit's parameters
	or other state changes.  It makes it unnecessary for UI components to continually poll an Audio
	Unit to determine if a parameter value has been changed. In order for this notification
	mechanism to work properly, parameter values should be changed using the AUParameterSet call
	(discussed below). This also makes it unnecessary for an Audio Unit to provide and support a
	notification mechanism, particularly as AudioUnitSetParameter may be received by an Audio Unit
	during the render process.

	The AUEventListener API's extend the AUParameterListener API's by supporting event types
	other than parameter changes. Events, including parameter changes are delivered serially to the 
	listener, preserving the time order of the events and parameter changes.

	There are also some utilities for converting between non-linear and linear value ranges. These
	are useful for displaying a non-linear parameter (such as one whose units are in Hertz or
	decibels) using a linear control mechanism, such as a slider, to ensure that the user has a
	wider perceived range of control over the parameter value.
*/

#ifndef AudioToolbox_AudioUnitUtilities_h
#define AudioToolbox_AudioUnitUtilities_h

#include <Availability.h>
#include <AudioToolbox/AudioUnit.h>
#ifdef __BLOCKS__
    #include <dispatch/dispatch.h>
#endif

CF_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================= */

/*!
    @constant   kAUParameterListener_AnyParameter
                    A wildcard value for an AudioUnitParameterID. Note that this is
                    only valid when sending a notification (with AUParameterListenerNotify),
                    not when registering to receive one.
*/
CF_ENUM(AudioUnitParameterID) {
    kAUParameterListener_AnyParameter = 0xFFFFFFFF
};

/*!
    @enum       AudioUnitEventType
    
    @abstract   Types of Audio Unit Events.
    
    @constant   kAudioUnitEvent_ParameterValueChange
                    The event is a change to a parameter value
    @constant   kAudioUnitEvent_BeginParameterChangeGesture
                    The event signifies a gesture (e.g. mouse-down) beginning a potential series of
                    related parameter value change events.
    @constant   kAudioUnitEvent_EndParameterChangeGesture
                    The event signifies a gesture (e.g. mouse-up) ending a series of related
                    parameter value change events.
    @constant   kAudioUnitEvent_PropertyChange
                    The event is a change to a property value.
*/
typedef CF_ENUM(UInt32, AudioUnitEventType) {
    kAudioUnitEvent_ParameterValueChange        = 0,
    kAudioUnitEvent_BeginParameterChangeGesture = 1,
    kAudioUnitEvent_EndParameterChangeGesture   = 2,
    kAudioUnitEvent_PropertyChange              = 3
};

/* ============================================================================= */

/*!
    @typedef        AUParameterListenerRef
    @abstract       An object which receives notifications of Audio Unit parameter value changes.
*/
typedef struct AUListenerBase *AUParameterListenerRef;
    // opaque
    // old-style listener, may not be passed to new functions

/*!
    @typedef        AUEventListenerRef
    @abstract       An object which receives Audio Unit events.
    @discussion     An AUEventListenerRef may be passed to API's taking an AUEventListenerRef
                    as an argument.
*/
typedef AUParameterListenerRef AUEventListenerRef;
    // new-style listener, can be passed to both old and new functions

/*!
    @struct     AudioUnitEvent
    @abstract   Describes a change to an Audio Unit's state.
    @var        mEventType
        The type of event.
    @var        mArgument
        Specifies the parameter or property which has changed.
*/  
struct AudioUnitEvent {
    AudioUnitEventType                  mEventType;
    union {
        AudioUnitParameter  mParameter; // for parameter value change, begin and end gesture
        AudioUnitProperty   mProperty;  // for kAudioUnitEvent_PropertyChange
    }                                   mArgument;
};
typedef struct AudioUnitEvent AudioUnitEvent;

#ifdef __BLOCKS__
/*!
    @typedef    AUParameterListenerBlock
    @abstract   A block called when a parameter value changes.
    @param  inObject
                The object which generated the parameter change.
    @param  inParameter
                Signifies the parameter whose value changed.
    @param  inValue
                The parameter's new value.
*/
typedef void (^AUParameterListenerBlock)(   void * __nullable           inObject,
                                            const AudioUnitParameter *  inParameter,
                                            AudioUnitParameterValue     inValue);

/*!
    @typedef    AUEventListenerBlock
    @abstract   A block called when an Audio Unit event occurs.
    @param  inObject
                The object which generated the parameter change.
    @param  inEvent
                The event which occurred.
    @param  inEventHostTime
                The host time at which the event occurred.
    @param  inParameterValue
                If the event is parameter change, the parameter's new value (otherwise, undefined).
*/
typedef void (^AUEventListenerBlock)(       void * __nullable           inObject,
                                            const AudioUnitEvent *      inEvent,
                                            UInt64                      inEventHostTime,
                                            AudioUnitParameterValue     inParameterValue);
#endif

/*!
    @typedef    AUParameterListenerProc
    @abstract   A function called when a parameter value changes.
    @param  inUserData
                The value passed to AUListenerCreate when the callback function was installed.
    @param  inObject
                The object which generated the parameter change.
    @param  inParameter
                Signifies the parameter whose value changed.
    @param  inValue
                The parameter's new value.
*/
typedef void (*AUParameterListenerProc)(    void * __nullable           inUserData,
                                            void * __nullable           inObject,
                                            const AudioUnitParameter *  inParameter,
                                            AudioUnitParameterValue     inValue);

/*!
    @typedef    AUEventListenerProc
    @abstract   A function called when an Audio Unit event occurs.
    @param  inUserData
                The value passed to AUListenerCreate when the callback function was installed.
    @param  inObject
                The object which generated the parameter change.
    @param  inEvent
                The event which occurred.
    @param  inEventHostTime
                The host time at which the event occurred.
    @param  inParameterValue
                If the event is parameter change, the parameter's new value (otherwise, undefined).
*/
typedef void (*AUEventListenerProc)(void * __nullable           inUserData,
                                    void * __nullable           inObject,
                                    const AudioUnitEvent *      inEvent,
                                    UInt64                      inEventHostTime,
                                    AudioUnitParameterValue     inParameterValue);


/* ============================================================================= */

/*!
    @functiongroup  AUListener
*/

#ifdef __BLOCKS__
/*!
    @function   AUListenerCreateWithDispatchQueue
    @abstract   Create an object for fielding notifications when AudioUnit parameter values change.
    @param      outListener
                    On successful return, an AUParameterListenerRef.
    @param      inNotificationInterval
                    The minimum time interval, in seconds, at which the callback will be called.
                    If multiple parameter value changes occur within this time interval, the
                    listener will only receive a notification for the last value change that
                    occurred before the callback.  If inNotificationInterval is 0, the inRunLoop
                    and inRunLoopMode arguments are ignored, and the callback will be issued
                    immediately, on the thread on which the parameter was changed.
    @param      inDispatchQueue
                    Dispatch queue on which the callback is called.
    @param      inBlock
                    Block called when the parameter's value changes.
    @discussion 
        Note that only parameter changes issued through AUParameterSet will generate
        notifications to listeners; thus, in most cases, AudioUnit clients should use
        AUParameterSet in preference to AudioUnitSetParameterValue.
*/
extern OSStatus
AUListenerCreateWithDispatchQueue(  AUParameterListenerRef __nullable * __nonnull outListener,
                                    Float32                                       inNotificationInterval,
                                    dispatch_queue_t                              inDispatchQueue,
                                    AUParameterListenerBlock                      inBlock)        API_AVAILABLE(macos(10.6), ios(6.0), watchos(2.0), tvos(9.0));
#endif

/*!
    @function   AUListenerCreate
    @abstract   Create an object for fielding notifications when AudioUnit parameter values change.
    @param      inProc
                    Function called when the parameter's value changes.
    @param      inUserData
                    A reference value for the use of the callback function.
    @param      inRunLoop
                    The run loop on which the callback is called.  If NULL,
                    CFRunLoopGetCurrent() is used.
    @param      inRunLoopMode
                    The run loop mode in which the callback's underlying run loop source will be
                    attached.  If NULL, kCFRunLoopDefaultMode is used.
    @param      inNotificationInterval
                    The minimum time interval, in seconds, at which the callback will be called.
                    If multiple parameter value changes occur within this time interval, the
                    listener will only receive a notification for the last value change that
                    occurred before the callback.  If inNotificationInterval is 0, the inRunLoop
                    and inRunLoopMode arguments are ignored, and the callback will be issued
                    immediately, on the thread on which the parameter was changed.
    @param      outListener
                    On successful return, an AUParameterListenerRef.
    @discussion 
        Note that only parameter changes issued through AUParameterSet will generate
        notifications to listeners; thus, in most cases, AudioUnit clients should use
        AUParameterSet in preference to AudioUnitSetParameter.
*/
extern OSStatus
AUListenerCreate(                   AUParameterListenerProc         inProc,
                                    void *                          inUserData,
                                    CFRunLoopRef __nullable         inRunLoop,
                                    CFStringRef __nullable          inRunLoopMode,
                                    Float32                         inNotificationInterval,
                                    AUParameterListenerRef __nullable * __nonnull outListener)    API_AVAILABLE(macos(10.2), ios(6.0), watchos(2.0), tvos(9.0));

/*!
    @function   AUListenerDispose
    @abstract   Dispose a parameter listener object.
    @param      inListener
                    The parameter listener to dispose.
*/
extern OSStatus
AUListenerDispose(                  AUParameterListenerRef          inListener)     API_AVAILABLE(macos(10.2), ios(6.0), watchos(2.0), tvos(9.0));

/*!
    @function   AUListenerAddParameter
    @abstract   Connect a parameter to a listener.
    @param      inListener
                    The parameter listener which will receive the callback.
    @param      inObject
                    The object which is interested in the value of the parameter.  This will be
                    passed as the inObject parameter to the listener callback function when the
                    parameter changes.
    @param      inParameter
                    The parameter whose value changes are to generate callbacks.
    @discussion 
        Associates an arbitrary object (often a user interface widget) with an
        AudioUnitParameter, and delivers notifications to the specified listener, telling it
        that the object needs to be informed of the parameter's value change.
*/
extern OSStatus
AUListenerAddParameter(             AUParameterListenerRef          inListener, 
                                    void * __nullable               inObject,
                                    const AudioUnitParameter *      inParameter)    API_AVAILABLE(macos(10.2), ios(6.0), watchos(2.0), tvos(9.0));

/*!
    @function   AUListenerRemoveParameter
    @abstract   Remove a parameter/listener connection.
    @param      inListener
                    The parameter listener to stop receiving callbacks.
    @param      inObject
                    The object which is no longer interested in the value of the parameter.
    @param      inParameter
                    The parameter whose value changes are to stop generating callbacks.
*/
extern OSStatus
AUListenerRemoveParameter(          AUParameterListenerRef          inListener, 
                                    void * __nullable               inObject,
                                    const AudioUnitParameter *      inParameter)    API_AVAILABLE(macos(10.2), ios(6.0), watchos(2.0), tvos(9.0));



/*!
    @function   AUParameterSet
    @abstract   Set an AudioUnit parameter value and notify listeners.
    @param      inSendingListener
                    A parameter listener generating the change and which does not want to
                    receive a callback as a result of it. May be NULL.
    @param      inSendingObject
                    The object generating the change and which does not want to receive a
                    callback as a result of it. NULL is treated specially when inListener is
                    non-null; it signifies that none of the specified listener's objects will
                    receive notifications.
    @param      inParameter
                    The parameter being changed.
    @param      inValue
                    The new value of the parameter.
	@param		inBufferOffsetInFrames
					The offset into the next rendered buffer at which the parameter change will take
					effect.
    @discussion 
        Calls AudioUnitSetParameter, and performs/schedules notification callbacks to all
        parameter listeners, for that parameter -- except that no callback will be generated to
        the inListener/inObject pair.
*/
extern OSStatus
AUParameterSet(                     AUParameterListenerRef __nullable inSendingListener,
                                    void * __nullable                 inSendingObject,
                                    const AudioUnitParameter *        inParameter,
                                    AudioUnitParameterValue           inValue,
                                    UInt32                            inBufferOffsetInFrames)
                                    	CA_REALTIME_API
										API_AVAILABLE(macos(10.2), ios(6.0), watchos(2.0), tvos(9.0));

/*!
    @function   AUParameterListenerNotify
    @abstract   Notify listeners of a past parameter change.
    @param      inSendingListener
                    A parameter listener generating the change and which does not want to
                    receive a callback as a result of it. May be NULL.
    @param      inSendingObject
                    The object generating the change and which does not want to receive a
                    callback as a result of it. NULL is treated specially when inListener is
                    non-null; it signifies that none of the specified listener's objects will
                    receive notifications.
    @param      inParameter
                    The parameter which was changed.
    @discussion 
        Performs and schedules the notification callbacks of AUParameterSet, without
        actually setting an AudioUnit parameter value.
        
        Clients scheduling ramped parameter changes to AudioUnits must make this call
        dynamically during playback in order for AudioUnitViews to be updated.  When the view's
        listener receives a notification, it will be passed the current value of the parameter.

        A special meaning is applied if the mParameterID value of inParameter is equal to
        kAUParameterListener_AnyParameter. In this case, ANY listener for ANY parameter value
        changes on the specified AudioUnit will be notified of the current value of that
        parameter.
*/
extern OSStatus
AUParameterListenerNotify(          AUParameterListenerRef __nullable inSendingListener,
                                    void * __nullable                 inSendingObject,
                                    const AudioUnitParameter *        inParameter)
                                    	CA_REALTIME_API
                                    	API_AVAILABLE(macos(10.2), ios(6.0), watchos(2.0), tvos(9.0));

/* ============================================================================= */

/*!
    @functiongroup  AUEventListener
*/

#ifdef __BLOCKS__
/*!
    @function   AUEventListenerCreateWithDispatchQueue
    @abstract   Creates an Audio Unit event listener.
    @param      outListener
                    On successful return, an AUEventListenerRef.
    @param      inNotificationInterval
                    The minimum time interval, in seconds, at which the callback will be called.
    @param      inValueChangeGranularity
                    Determines how parameter value changes occurring within this interval are
                    queued; when an event follows a previous one by a smaller time interval than
                    the granularity, then the listener will only be notified for the second
                    parameter change.
    @param      inDispatchQueue
                    The dispatch queue on which the callback is called.
    @param      inBlock
                    Block called when an event occurs.
    
    @discussion
        AUEventListener is a specialization of AUParameterListener; use AUListenerDispose to
        dispose of an AUEventListener. You may use AUListenerAddParameter and
        AUListenerRemoveParameter with AUEventListerRef's, in addition to
        AUEventListenerAddEventType / AUEventListenerRemoveEventType.

        Some examples illustrating inNotificationInterval and inValueChangeGranularity:

        [1] a UI receiver: inNotificationInterval = 100 ms, inValueChangeGranularity = 100 ms.
            User interfaces almost never care about previous values, only the current one,
            and don't wish to perform redraws too often.

        [2] An automation recorder: inNotificationInterval = 200 ms, inValueChangeGranularity = 10 ms.
            Automation systems typically wish to record events with a high degree of timing precision,
            but do not need to be woken up for each event.
        
        In case [1], the listener will be called within 100 ms (the notification interval) of an
        event. It will only receive one notification for any number of value changes to the
        parameter concerned, occurring within a 100 ms window (the granularity).

        In case [2], the listener will be received within 200 ms (the notification interval) of
        an event It can receive more than one notification per parameter, for the last of each
        group of value changes occurring within a 10 ms window (the granularity).

        In both cases, thread scheduling latencies may result in more events being delivered to
        the listener callback than the theoretical maximum (notification interval /
        granularity).
*/
extern OSStatus
AUEventListenerCreateWithDispatchQueue(
                                    AUEventListenerRef __nullable * __nonnull outListener,
                                    Float32                                   inNotificationInterval,     // seconds
                                    Float32                                   inValueChangeGranularity,   // seconds
                                    dispatch_queue_t                          inDispatchQueue,
                                    AUEventListenerBlock                      inBlock)            API_AVAILABLE(macos(10.6), ios(6.0), watchos(2.0), tvos(9.0));
#endif

/*!
    @function   AUEventListenerCreate
    @abstract   Creates an Audio Unit event listener.
    @param      inProc
                    Function called when an event occurs.
    @param      inUserData
                    A reference value for the use of the callback function.
    @param      inRunLoop
                    The run loop on which the callback is called.  If NULL,
                    CFRunLoopGetCurrent() is used.
    @param      inRunLoopMode
                    The run loop mode in which the callback's underlying run loop source will be
                    attached.  If NULL, kCFRunLoopDefaultMode is used.
    @param      inNotificationInterval
                    The minimum time interval, in seconds, at which the callback will be called.
    @param      inValueChangeGranularity
                    Determines how parameter value changes occurring within this interval are
                    queued; when an event follows a previous one by a smaller time interval than
                    the granularity, then the listener will only be notified for the second
                    parameter change.
    @param      outListener
                    On successful return, an AUEventListenerRef.
    
    @discussion
        See the discussion of AUEventListenerCreateWithDispatchQueue.
*/
extern OSStatus
AUEventListenerCreate(              AUEventListenerProc                       inProc,
                                    void * __nullable                         inUserData,
                                    CFRunLoopRef __nullable                   inRunLoop,
                                    CFStringRef __nullable                    inRunLoopMode,
                                    Float32                                   inNotificationInterval,
                                    Float32                                   inValueChangeGranularity,
                                    AUEventListenerRef __nullable * __nonnull outListener)        API_AVAILABLE(macos(10.3), ios(6.0), watchos(2.0), tvos(9.0));

/*!
    @function   AUEventListenerAddEventType
    @abstract   Begin delivering a particular type of events to a listener.
    @param      inListener
                    The parameter listener which will receive the events.
    @param      inObject
                    The object which is interested in the value of the parameter.  This will be
                    passed as the inObject parameter to the listener callback function when the
                    parameter changes.
    @param      inEvent
                    The type of event to listen for.
    @result     An OSStatus error code.
*/
extern OSStatus
AUEventListenerAddEventType(        AUEventListenerRef          inListener,
                                    void * __nullable           inObject,
                                    const AudioUnitEvent *      inEvent)        API_AVAILABLE(macos(10.3), ios(6.0), watchos(2.0), tvos(9.0));
    
/*!
    @function   AUEventListenerRemoveEventType
    @abstract   Stop delivering a particular type of events to a listener.
    @param      inListener
                    The parameter listener to stop receiving events.
    @param      inObject
                    The object which is no longer interested in the value of the parameter.
    @param      inEvent
                    The type of event to stop listening for.
    @result     An OSStatus error code.
*/
extern OSStatus
AUEventListenerRemoveEventType(     AUEventListenerRef          inListener,
                                    void * __nullable           inObject,
                                    const AudioUnitEvent *      inEvent)        API_AVAILABLE(macos(10.3), ios(6.0), watchos(2.0), tvos(9.0));           

/*!
    @function   AUEventListenerNotify
    @abstract   Deliver an AudioUnitEvent to all listeners registered to receive it.
    @discussion This is only to be used for notifications about parameter changes (and gestures).
                It can not be used for notifying changes to property values as these are 
                internal to an audio unit and should not be issued outside of the audio unit itself.
    @param      inSendingListener
                    A parameter listener generating the change and which does not want to
                    receive a callback as a result of it. May be NULL.
    @param      inSendingObject
                    The object generating the change and which does not want to receive a
                    callback as a result of it. NULL is treated specially when inListener is
                    non-null; it signifies that none of the specified listener's objects will
                    receive notifications.
    @param      inEvent
                    The event to be delivered.
    @result     An OSStatus error code.
*/
extern OSStatus
AUEventListenerNotify(              AUEventListenerRef __nullable  inSendingListener,
                                    void * __nullable              inSendingObject,
                                    const AudioUnitEvent *         inEvent)
                                    	CA_REALTIME_API
                                    	API_AVAILABLE(macos(10.3), ios(6.0), watchos(2.0), tvos(9.0));
                                    
/* ============================================================================= */

/*!
    @functiongroup  Parameter value utilities
*/


/*!
    @function   AUParameterValueFromLinear
    @abstract   Converts a linear value to a parameter value according to the parameter's units.
    
    @param      inLinearValue
                    The linear value (0.0-1.0) to convert.
    @param      inParameter
                    The parameter, including its Audio Unit, that will define the conversion of
                    the supplied linear value to a value that is natural to that parameter.
    @result
                The converted parameter value, in the parameter's natural units.
*/
extern AudioUnitParameterValue
AUParameterValueFromLinear(         Float32                     inLinearValue,
                                    const AudioUnitParameter *  inParameter)    API_AVAILABLE(macos(10.2), ios(6.0), watchos(2.0), tvos(9.0));

/*!
    @function   AUParameterValueToLinear
    @abstract   Converts a parameter value to a linear value according to the parameter's units.
    
    @param      inParameterValue
                    The value in the natural units of the specified parameter.
        
    @param      inParameter
                    The parameter, including its Audio Unit, that will define the conversion of
                    the supplied parameter value to a corresponding linear value.
    @result
                A number 0.0-1.0.
*/
extern Float32
AUParameterValueToLinear(           AudioUnitParameterValue     inParameterValue,
                                    const AudioUnitParameter *  inParameter)    API_AVAILABLE(macos(10.2), ios(6.0), watchos(2.0), tvos(9.0));
                                        // returns 0-1

/*!
    @function   AUParameterFormatValue
    @abstract   Format a parameter value into a string.
    @param      inParameterValue
                    The parameter value to be formatted.
    @param      inParameter
                    The Audio Unit, scope, element, and parameter whose value this is.
    @param      inTextBuffer
                    The character array to receive the formatted text.  Should be at least 32
                    characters.
    @param      inDigits
                    The resolution of the string (see example above).
    @result
                `inTextBuffer`

    @discussion 
        Formats a floating point value into a string.  Computes a power of 10 to which the value
        will be rounded and displayed as follows:  if the the parameter is logarithmic (Hertz),
        the number of significant digits is inDigits - pow10(inParameterValue) + 1.  Otherwise,
        it is inDigits - pow10(maxValue - minValue) + 1.

        Example for inDigits=3:

        pow10 | range        |  digits after decimal place display
        ------|--------------|------------------------------------
        -2    | .0100-.0999  |  4
        -1    | .100-.999    |  3
        0     | 1.00-9.99    |  2
        1     | 10.0-99.9    |  1
        2     | 100-999      |  0
        3     | 1000-9990    |  -1
        4     | 10000-99900  |  -2
*/                              
extern char *
AUParameterFormatValue(             Float64                     inParameterValue,
                                    const AudioUnitParameter *  inParameter,
                                    char *                      inTextBuffer,
                                    UInt32                      inDigits)       API_AVAILABLE(macos(10.2), ios(6.0), watchos(2.0), tvos(9.0));

#ifdef __cplusplus
}
#endif

CF_ASSUME_NONNULL_END

#endif // AudioToolbox_AudioUnitUtilities_h
#else
#include <AudioToolboxCore/AudioUnitUtilities.h>
#endif
