#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AUParameters.h>)
/*!
	@file		AUParameters.h
 	@framework	AudioToolbox.framework
 	@copyright	(c) 2015 Apple, Inc. All rights reserved.

	@brief		Objects representing an AUAudioUnit's tree of parameters.
*/

#ifndef AudioToolbox_AUParameters_h
#define AudioToolbox_AUParameters_h
#ifdef __OBJC2__

#import <AudioToolbox/AudioUnitProperties.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class AUParameter;

// =================================================================================================
// typedefs

/*!	@typedef	AUValue
	@brief		A value of an audio unit parameter.
*/
typedef float AUValue;

/*!	@typedef	AUParameterAddress
	@brief		Numeric identifier for audio unit parameter.
	@discussion
		Note that parameter addresses are not necessarily persistent, unless the individual audio
		unit documents a promise to maintain its addressing scheme. Hosts should bind to parameters'
		key paths.
*/
typedef uint64_t AUParameterAddress;

/*!	@enum		AUParameterAutomationEventType
	@brief		Identifies the different types of parameter automation events.
	
	@discussion
		Audio Units may generate parameter changes from their user interfaces. Hosts may attach
		significance to the beginning and end of a UI gesture (typically touching and releasing
		a fader). These gestures are conveyed through these types of automation events.
	
	@constant AUParameterAutomationEventTypeValue
		The event contains an updated value for the parameter.
	@constant AUParameterAutomationEventTypeTouch
		The event marks an initial "touch" gesture on a UI element.
	@constant AUParameterAutomationEventTypeRelease
		The event marks a final "release" gesture on a UI element.
*/
typedef NS_ENUM(uint32_t, AUParameterAutomationEventType) {
	AUParameterAutomationEventTypeValue = 0,
	AUParameterAutomationEventTypeTouch = 1,
	AUParameterAutomationEventTypeRelease = 2
};

/*!	@typedef	AURecordedParameterEvent
	@brief		An event recording the changing of a parameter at a particular host time.
*/
typedef struct AURecordedParameterEvent {
	uint64_t hostTime;				///< The host time at which the event occurred.
	AUParameterAddress address;		///< The address of the parameter whose value changed.
	AUValue value;					///< The value of the parameter at that time.
} AURecordedParameterEvent;

/*!	@typedef	AUParameterAutomationEvent
	@brief		An event recording the changing of a parameter, possibly including events
				such as touch and release gestures, at a particular host time.
*/
typedef struct AUParameterAutomationEvent {
	uint64_t hostTime;				///< The host time at which the event occurred.
	AUParameterAddress address;		///< The address of the parameter whose value changed.
	AUValue value;					///< The value of the parameter at that time.
	AUParameterAutomationEventType eventType; ///< The type of the event.
	uint64_t reserved;
} AUParameterAutomationEvent;

/*!	@typedef	AUParameterObserver
	@brief		A block called to signal that the value of a parameter has changed.
	@discussion	
		See the discussion of -[AUParameterNode tokenByAddingParameterObserver:].
	@param address
		The address of the parameter whose value changed.
	@param value
		The current value of the parameter.
*/
typedef void (^AUParameterObserver)(AUParameterAddress address, AUValue value);

/*!	@typedef	AUParameterRecordingObserver
	@brief		A block called to record parameter changes as automation events.
	@discussion
		See the discussion of -[AUParameterNode tokenByAddingParameterRecordingObserver:].
	@param numberEvents
		The number of events being delivered.
	@param events
		The events being delivered.
*/
typedef void (^AUParameterRecordingObserver)(NSInteger numberEvents, const AURecordedParameterEvent *events);

/*!	@typedef	AUParameterAutomationObserver
	@brief		A block called to record parameter changes as automation events.
	@discussion
		See the discussion of -[AUParameterNode tokenByAddingParameterAutomationObserver:].
	@param numberEvents
		The number of events being delivered.
	@param events
		The events being delivered.
*/
typedef void (^AUParameterAutomationObserver)(NSInteger numberEvents, const AUParameterAutomationEvent *events);

/*!	@typedef	AUParameterObserverToken
	@brief		A token representing an installed AUParameterObserver, AUParameterRecordingObserver,
				or AUParameterAutomationObserver.
*/
typedef void *AUParameterObserverToken;

// =================================================================================================

/*!	@class		AUParameterNode
	@brief		A node in an audio unit's tree of parameters.
	@discussion
		Nodes are instances of either AUParameterGroup or AUParameter.
*/
API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
@interface AUParameterNode : NSObject

/*!	@property	identifier
	@brief		A non-localized, permanent name for a parameter or group.
	@discussion
		The identifier must be unique for all child nodes under any given parent. From release to
		release, an audio unit must not change its parameters' identifiers; this will invalidate any
		hosts' documents that refer to the parameters.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString *identifier;

/*!	@property	keyPath
	@brief		Generated by concatenating the identifiers of a node's parents with its own.
	@discussion
		Unless an audio unit specifically documents that its parameter addresses are stable and
		persistent, hosts, when recording parameter values, should bind to the keyPath.

		The individual node identifiers in a key path are separated by periods. (".")
		
		Passing a node's keyPath to -[tree valueForKeyPath:] should return the same node.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString *keyPath;

/*!	@property	displayName
	@brief		A localized name to display for the parameter.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString *displayName;

/*!	@method		displayNameWithLength:
	@brief		A version of displayName possibly abbreviated to the given desired length, in characters.
	@discussion
		The default implementation simply returns displayName.
*/
- (NSString *)displayNameWithLength:(NSInteger)maximumLength;

/*!	@method	tokenByAddingParameterObserver:
	@brief	Add an observer for a parameter or all parameters in a group/tree.
	@discussion
		An audio unit view can use an AUParameterObserver to be notified of changes
		to a single parameter, or to all parameters in a group/tree.
		
		These callbacks are throttled so as to limit the rate of redundant notifications
		in the case of frequent changes to a single parameter.
		
		This block is called in an arbitrary thread context. It is responsible for thread-safety.
		It must not make any calls to add or remove other observers, including itself;
		this will deadlock.
		
		An audio unit's implementation should interact with the parameter object via
		implementorValueObserver and implementorValueProvider.

		Parameter observers are bound to a specific parameter instance. If this parameter is
		destroyed, e.g. if the parameter tree is re-constructed, the previously set parameter
		observers will no longer be valid. Clients can observe changes to the parameter tree
		via KVO. See the discussion of -[AUAudioUnit parameterTree].
	@param observer
		A block to call after the value of a parameter has changed.
	@return
		A token which can be passed to removeParameterObserver: or to -[AUParameter setValue:originator:]
*/
- (AUParameterObserverToken)tokenByAddingParameterObserver:(AUParameterObserver)observer;

/*!	@method tokenByAddingParameterRecordingObserver:
	@brief	Add a recording observer for a parameter or all parameters in a group/tree.
	@discussion
		This is a variant of tokenByAddingParameterAutomationObserver where the callback receives
		AURecordedParameterEvents instead of AUParameterAutomationEvents.
		
		This will be deprecated in favor of tokenByAddingParameterAutomationObserver in a future
		release.
*/
- (AUParameterObserverToken)tokenByAddingParameterRecordingObserver:(AUParameterRecordingObserver)observer;

/*!	@method tokenByAddingParameterAutomationObserver:
	@brief	Add a recording observer for a parameter or all parameters in a group/tree.
	@discussion
		An audio unit host can use an AUParameterAutomationObserver or AUParameterRecordingObserver
		to capture a series of changes to a parameter value, including the timing of the events, as
		generated by a UI gesture in a view, for example.
		
		Unlike AUParameterObserver, these callbacks are not throttled.
		
		This block is called in an arbitrary thread context. It is responsible for thread-safety.
		It must not make any calls to add or remove other observers, including itself;
		this will deadlock.
		
		An audio unit's engine should interact with the parameter object via
		implementorValueObserver and implementorValueProvider.
	@param observer
		A block to call to record the changing of a parameter value.
	@return
		A token which can be passed to removeParameterObserver: or to -[AUParameter
		setValue:originator:]
*/
- (AUParameterObserverToken)tokenByAddingParameterAutomationObserver:(AUParameterAutomationObserver)observer API_AVAILABLE(macos(10.12), ios(10.0), watchos(3.0), tvos(10.0));

/*!	@method removeParameterObserver:
	@brief	Remove an observer created with tokenByAddingParameterObserver,
		tokenByAddingParameterRecordingObserver, or tokenByAddingParameterAutomationObserver.
	@discussion
		This call will remove the callback corresponding to the supplied token. Note that this
		will block until any callbacks currently in flight have completed.
*/
- (void)removeParameterObserver:(AUParameterObserverToken)token;

@end

// =================================================================================================

/*!	@class	AUParameterGroup
	@brief	A group of related parameters.
	@discussion
		A parameter group is KVC-compliant for its children; e.g. valueForKey:@"volume" will
		return a child parameter whose identifier is "volume".
*/
API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
@interface AUParameterGroup : AUParameterNode <NSSecureCoding>

/// The group's child nodes (AUParameterGroupNode).
@property (NS_NONATOMIC_IOSONLY, readonly) NSArray<AUParameterNode *> *children;

/// Returns a flat array of all parameters in the group, including those in child groups.
@property (NS_NONATOMIC_IOSONLY, readonly) NSArray<AUParameter *> *allParameters;

@end

// =================================================================================================

/*!	@class	AUParameterTree
	@brief	The top level group node, representing all of an audio unit's parameters.
	@discussion
		An audio unit's parameters are organized into a tree containing groups and parameters.
		Groups may be nested.
		
		The tree is KVO-compliant. For example, if the tree contains a group named "LFO" ,
		containing a parameter named rate, then "LFO.rate" refers to that parameter object, and
		"LFO.rate.value" refers to that parameter's value.

		An audio unit may choose to dynamically rearrange the tree. When doing so, it must
		issue a KVO notification on the audio unit's parameterTree property. The tree's elements are
		mostly immutable (except for values and implementor hooks); the only way to modify them
		is to publish a new tree.
*/
API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
@interface AUParameterTree : AUParameterGroup <NSSecureCoding>

/*!	@method	parameterWithAddress:
	@brief	Search a tree for a parameter with a specific address.
	@return
		The parameter corresponding to the supplied address, or nil if no such parameter exists.
*/
- (AUParameter *__nullable)parameterWithAddress:(AUParameterAddress)address;

/*!	@method	parameterWithID:scope:element:
	@brief	Search a tree for a specific v2 audio unit parameter.
	@discussion
		V2 audio units publish parameters identified by a parameter ID, scope, and element.
		A host that knows that it is dealing with a v2 unit can locate parameters using this method,
		for example, for the Apple-supplied system audio units.
	@return
		The parameter corresponding to the supplied ID/scope/element, or nil if no such parameter
		exists, or if the audio unit is not a v2 unit.
*/
- (AUParameter *__nullable)parameterWithID:(AudioUnitParameterID)paramID scope:(AudioUnitScope)scope element:(AudioUnitElement)element;

@end

// =================================================================================================
// AUParameter

/*!	@class	AUParameter
	@brief	A node representing a single parameter.
*/
API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
@interface AUParameter : AUParameterNode <NSSecureCoding>

/// The parameter's minimum value.
@property (NS_NONATOMIC_IOSONLY, readonly) AUValue minValue;

/// The parameter's maximum value.
@property (NS_NONATOMIC_IOSONLY, readonly) AUValue maxValue;

/// The parameter's unit of measurement.
@property (NS_NONATOMIC_IOSONLY, readonly) AudioUnitParameterUnit unit;

/// A localized name for the parameter's unit. Supplied by the AU if kAudioUnitParameterUnit_CustomUnit; else by the framework.
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSString *unitName;

/// Various details of the parameter.
@property (NS_NONATOMIC_IOSONLY, readonly) AudioUnitParameterOptions flags;

/// The parameter's address.
@property (NS_NONATOMIC_IOSONLY, readonly) AUParameterAddress address;

/// For parameters with kAudioUnitParameterUnit_Indexed, localized strings corresponding
///	to the values.
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSArray<NSString *> *valueStrings;

/*!	@brief		Parameters whose values may change as a side effect of this parameter's value
				changing.
	@discussion
		Each array value is an NSNumber representing AUParameterAddress.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSArray<NSNumber *> *dependentParameters;

/// The parameter's current value.
@property (NS_NONATOMIC_IOSONLY) AUValue value;

///
/*!	@brief	Set the parameter's value, avoiding redundant notifications to the originator.
	@discussion
			Bridged to the v2 function AudioUnitSetParameter.
*/
- (void)setValue:(AUValue)value originator:(AUParameterObserverToken __nullable)originator;

/*!	@brief	Convenience for setValue:originator:atHostTime:eventType:
	@discussion
			Bridged to the v2 function AudioUnitSetParameter.
*/
- (void)setValue:(AUValue)value originator:(AUParameterObserverToken __nullable)originator atHostTime:(uint64_t)hostTime;

/*!	@brief	Set the parameter's value, preserving the host time of the gesture that initiated the
			change, and associating an event type such as touch/release.
	@discussion
		In general, this method should only be called from a user interface. It initiates a change
		to a parameter in a way that captures the gesture such that it can be recorded later --
		any AUParameterAutomationObservers will receive the host time and event type associated
		with the parameter change.

		From an audio playback engine, a host should schedule automated parameter changes through
		AUAudioUnit's scheduleParameterBlock.
 
		Bridged to the v2 function AudioUnitSetParameter.
*/
- (void)setValue:(AUValue)value originator:(AUParameterObserverToken __nullable)originator atHostTime:(uint64_t)hostTime eventType:(AUParameterAutomationEventType)eventType API_AVAILABLE(macos(10.12), ios(10.0), watchos(3.0), tvos(10.0));

/*!	@brief Get a textual representation of a value for the parameter. Use value==nil to use the
		   current value. Bridged to the v2 property kAudioUnitProperty_ParameterStringFromValue.
	@discussion
		This is currently only supported for parameters whose flags include
		kAudioUnitParameterFlag_ValuesHaveStrings.
*/
- (NSString *)stringFromValue:(const AUValue *__nullable)value;

/*!	@brief Convert a textual representation of a value to a numeric one.
	@discussion
		This is currently only supported for parameters whose flags include
		kAudioUnitParameterFlag_ValuesHaveStrings.
*/
- (AUValue)valueFromString:(NSString *)string;

@end

NS_ASSUME_NONNULL_END

#endif // __OBJC2__
#endif // AudioToolbox_AUParameters_h
#else
#include <AudioToolboxCore/AUParameters.h>
#endif
