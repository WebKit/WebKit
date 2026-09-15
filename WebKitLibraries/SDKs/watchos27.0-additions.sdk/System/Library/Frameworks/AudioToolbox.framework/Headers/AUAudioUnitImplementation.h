#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AUAudioUnitImplementation.h>)
/*!
	@file		AUAudioUnitImplementation.h
 	@framework	AudioToolbox.framework
 	@copyright	(c) 2015 Apple, Inc. All rights reserved.

	@brief		Additional AUAudioUnit interfaces specific to subclass implementations.
*/


/*!
@page AUExtensionPackaging Audio Unit Extension Packaging
@discussion

Audio Unit app extensions are available beginning with macOS 10.11 and iOS 9.0.
For background on app extensions, see "App Extension Programming Guide."

There are two types of audio unit extensions: UI, and non-UI. The principal class of an
extension with UI must be a subclass of AUViewController. This class is declared
in CoreAudioKit/AUViewController.h. The principal class of a non-UI extension can derive from
NSObject.

The principal class of both UI and non-UI extensions must conform to the AUAudioUnitFactory
protocol.

The Info.plist of the .appex bundle describes the type of extension and the principal class.
It also contains an AudioComponents array (see AudioComponent.h), and an optional
AudioComponentBundle entry, for example:

```
	<key>NSExtension</key>
	<dict>
		<key>NSExtensionAttributes</key>
		<dict>
			<key>NSExtensionActivationRule</key>
			<string>TRUEPREDICATE</string>
			<key>NSExtensionServiceRoleType</key>
			<string>NSExtensionServiceRoleTypeEditor</string>
			<key>AudioComponents</key>
			<array>
				<dict>
					<key>factoryFunction</key>
					<string>____</string>
					<key>manufacturer</key>
					<string>____</string>
					<key>name</key>
					<string>____</string>
					<key>sandboxSafe</key>
					<true/>
					<key>subtype</key>
					<string>____</string>
					<key>tags</key>
					<array>
						<string>____</string>
					</array>
					<key>type</key>
					<string>____</string>
					<key>version</key>
					<integer>____</integer>
				</dict>
			</array>
			<key>AudioComponentBundle</key>
			<string>____</string>
		</dict>
		<key>NSExtensionPointIdentifier</key>
		<string>____</string>
		<key>NSExtensionPrincipalClass</key>
		<string>____</string>
	</dict>
```

NSExtensionPointIdentifier
: `com.apple.AudioUnit` or `com.apple.AudioUnit-UI`.

NSExtensionPrincipalClass
:	The name of the principal class.

AudioComponentBundle
:	Optional. A version 3 audio unit can be loaded into a separate extension process and
	this is the default behavior. To be able to load in-process on macOS (see 
	kAudioComponentInstantiation_LoadInProcess) the audio unit has to be packaged 
	separately. The AudioComponentBundle entry supports loading in-process by designating 
	the identifier of a bundle in the .appex or its enclosing app container in which the 
	factory function and/or principal class are implemented. 

AudioComponents
:	Registration info for each audio component type/subtype/manufacturer
	implemented in the extension. factoryFunction is only necessary
	if the AU is implemented using AUAudioUnitV2Bridge.
	See AudioComponent.h.

Note that as of macOS version 10.12, the system has special support for installing both version 2
(bundle-based) and version 3 (extension) implementations of the same audio unit. When two components
are registered with the same type/subtype/manufacturer and version, normally the first one found
is used. But if one is an audio unit extension and the other is not, then the audio unit extension's
registration "wins", though if an app attempts to open it synchronously, with 
AudioComponentInstanceNew, then the system will fall back to the version 2 implementation.
*/

#ifndef AudioToolbox_AUAudioUnitImplementation_h
#define AudioToolbox_AUAudioUnitImplementation_h
#ifdef __OBJC2__

#import <AudioToolbox/AUAudioUnit.h>

NS_ASSUME_NONNULL_BEGIN

// forward declaration
union AURenderEvent;

// =================================================================================================
// Realtime events - parameters and MIDI

/// Describes the type of a render event.
typedef NS_ENUM(uint8_t, AURenderEventType) {
	AURenderEventParameter		= 1,
	AURenderEventParameterRamp	= 2,
	AURenderEventMIDI			= 8,
	AURenderEventMIDISysEx		= 9,
	AURenderEventMIDIEventList  = 10
};

#pragma pack(4)
///	Common header for an AURenderEvent.
typedef struct AURenderEventHeader {
	union AURenderEvent *__nullable next;		//!< The next event in a linked list of events.
	AUEventSampleTime		eventSampleTime;	//!< The sample time at which the event is scheduled to occur.
	AURenderEventType		eventType;			//!< The type of the event.
	uint8_t					reserved;			//!< Must be 0.
} AURenderEventHeader;

/// Describes a scheduled parameter change.
typedef struct AUParameterEvent {
	union AURenderEvent *__nullable next;		//!< The next event in a linked list of events.
	AUEventSampleTime		eventSampleTime;	//!< The sample time at which the event is scheduled to occur.
	AURenderEventType		eventType;			//!< AURenderEventParameter or AURenderEventParameterRamp.
	uint8_t					reserved[3];		//!< Must be 0.
	AUAudioFrameCount		rampDurationSampleFrames;
												//!< If greater than 0, the event is a parameter ramp; 
												/// 	 should be 0 for a non-ramped event.
	AUParameterAddress		parameterAddress;	//!< The parameter to change.
	AUValue					value;				//!< If ramped, the parameter value at the
												///	end of the ramp; for a non-ramped event,
												///	the new value.
} AUParameterEvent;

/// Describes a single scheduled MIDI event.
typedef struct AUMIDIEvent {
	union AURenderEvent *__nullable next;		//!< The next event in a linked list of events.
	AUEventSampleTime		eventSampleTime;	//!< The sample time at which the event is scheduled to occur.
	AURenderEventType		eventType;			//!< AURenderEventMIDI or AURenderEventMIDISysEx.
	uint8_t					reserved;			//!< Must be 0.
	uint16_t				length;				//!< The number of valid MIDI bytes in the data field.
												/// 1, 2 or 3 for most MIDI events, but can be longer
												/// for system-exclusive (sys-ex) events.
	uint8_t					cable;				//!< The virtual cable number.
	uint8_t					data[3];			//!< The bytes of the MIDI event. Running status will not be used.
} AUMIDIEvent;

/// Describes a single scheduled MIDIEventList.
typedef struct AUMIDIEventList {
	union AURenderEvent *__nullable next;		//!< The next event in a linked list of events.
	AUEventSampleTime		eventSampleTime;	//!< The sample time at which the event is scheduled to occur.
	AURenderEventType		eventType;			//!< AURenderEventMIDI or AURenderEventMIDISysEx.
	uint8_t					reserved;			//!< Must be 0.
	uint8_t					cable;				//!< The virtual cable number.
	MIDIEventList			eventList;			//!< A structure containing UMP packets.
} AUMIDIEventList;


/*!	@brief	A union of the various specific render event types.
	@discussion
		Determine which variant to use via head.eventType. AURenderEventParameter and
		AURenderEventParameterRamp use the parameter variant. AURenderEventMIDI and
		AURenderEventMIDISysEx use the MIDI variant.
*/
typedef union AURenderEvent {
	AURenderEventHeader		head;
	AUParameterEvent		parameter;
	AUMIDIEvent				MIDI;
	AUMIDIEventList			MIDIEventsList;
} AURenderEvent;
#pragma pack()

/*!	@brief	Block to render the audio unit.
	@discussion
		Implemented in subclasses; should not be used by clients.

		The other parameters are identical to those of AURenderBlock.
	@param realtimeEventListHead
		A time-ordered linked list of the AURenderEvents to be rendered during this render cycle.
		Note that a ramp event will only appear in the render cycle during which it starts; the
		audio unit is responsible for maintaining continued ramping state for any further render
		cycles.
*/
typedef AUAudioUnitStatus (^AUInternalRenderBlock)(
	AudioUnitRenderActionFlags *							actionFlags,
	const AudioTimeStamp *									timestamp,
	AUAudioFrameCount										frameCount,
	NSInteger												outputBusNumber,
	AudioBufferList *										outputData,
	const AURenderEvent *__nullable 						realtimeEventListHead,
	AURenderPullInputBlock __nullable __unsafe_unretained	pullInputBlock) CA_REALTIME_API;

// =================================================================================================

/// Aspects of AUAudioUnit of interest only to subclassers.
@interface AUAudioUnit (AUAudioUnitImplementation)

/*!	@brief	Register an audio unit component implemented as an AUAudioUnit subclass.
	@discussion
		This method dynamically registers the supplied AUAudioUnit subclass with the Audio Component
		system, in the context of the current process (only). After registering the subclass, it can
		be instantiated via AudioComponentInstanceNew, 
		-[AUAudioUnit initWithComponentDescription:options:error:], and via any other API's which
		instantiate audio units via their component descriptions (e.g. <AudioToolbox/AUGraph.h>, or
		<AVFoundation/AVAudioUnitEffect.h>).
*/
+ (void)registerSubclass:(Class)cls asComponentDescription:(AudioComponentDescription)componentDescription name:(NSString *)name version:(UInt32)version;

/// Block which subclassers must provide (via a getter) to implement rendering.
@property (nonatomic, readonly) AUInternalRenderBlock internalRenderBlock;

/*!	@property	renderContextObserver
	@brief		Block called by the OS when the rendering context changes.
	
	Audio Units which create auxiliary realtime rendering threads should implement this property to
	return a block which will be called by the OS when the render context changes. Audio Unit hosts
	must not attempt to interact with the AudioUnit through this block; it is for the exclusive use
	of the OS. See <AudioToolbox/AudioWorkInterval.h> for more information.
*/
@property (nonatomic, readonly) AURenderContextObserver renderContextObserver
	API_AVAILABLE(macos(11.0), ios(14.0), watchos(7.0), tvos(14.0))
	__SWIFT_UNAVAILABLE_MSG("Swift is not supported for use with audio realtime threads");

/*! @property	MIDIOutputBufferSizeHint
	@brief		Hint to control the size of the allocated buffer for outgoing MIDI events.
	@discussion
        This property allows the plug-in to provide a hint to the framework regarding the size of
        its outgoing MIDI data buffer.

        If the plug-in produces more MIDI output data than the default size of the allocated buffer,
        then the plug-in can provide this property to increase the size of this buffer.

        The value represents the number of 3-byte Legacy MIDI messages that fit into the buffer or
		a single MIDIEventList containing 1 MIDIEventPacket of 2 words when using MIDI 2.0 (MIDIEventList based API's).
 
        This property is set to the default value by the framework.

        In case of kAudioUnitErr_MIDIOutputBufferFull errors caused by producing too much MIDI
        output in one render call, set this property to increase the buffer.

        This only provides a recommendation to the framework.
 
        Bridged to kAudioUnitProperty_MIDIOutputBufferSizeHint.
*/
@property (NS_NONATOMIC_IOSONLY) NSInteger MIDIOutputBufferSizeHint API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

/*!	@method	shouldChangeToFormat:forBus:
    @param format
        An AVAudioFormat which is proposed as the new format.
    @param bus
        The AUAudioUnitBus on which the format will be changed.
	@discussion
        This is called when setting the format on an AUAudioUnitBus.
        The bus has already checked that the format meets the channel constraints of the bus.
        The AU can override this method to check before allowing a new format to be set on the bus.
        If this method returns NO, then the new format will not be set on the bus.
        The default implementation returns NO if the unit has renderResourcesAllocated, otherwise it results YES.
*/
- (BOOL)shouldChangeToFormat:(AVAudioFormat *)format forBus:(AUAudioUnitBus *)bus;

/*!	@method	setRenderResourcesAllocated:
    @param flag
        In the base class implementation of allocateRenderResourcesAndReturnError:, the property renderResourcesAllocated is set to YES.
        If allocateRenderResourcesAndReturnError: should fail in a subclass, subclassers must use this method to set renderResourcesAllocated to NO.
*/
- (void)setRenderResourcesAllocated:(BOOL)flag;

@end

// =================================================================================================

/// Aspects of AUAudioUnitBus of interest only to the implementation of v3 AUs.
@interface AUAudioUnitBus (AUAudioUnitImplementation)

/*!	@method		initWithFormat:error:
	@brief		initialize with a default format.
	@param format	The initial format for the bus.
	@param outError	An error if the format is unsupported for the bus.
*/
- (nullable instancetype)initWithFormat:(AVAudioFormat *)format error:(NSError **)outError;

/*!	@property	supportedChannelCounts
	@brief		An array of numbers giving the supported numbers of channels for this bus.
	@discussion
		If supportedChannelCounts is nil, then any number less than or equal to maximumChannelCount
		is supported. If setting supportedChannelCounts makes the current format unsupported, then
		format will be set to nil. The default value is nil.
*/
@property (nonatomic, retain, nullable) NSArray<NSNumber *> *supportedChannelCounts;

/*!	@property	maximumChannelCount
	@brief		The maximum numbers of channels supported for this bus.
	@discussion
		If supportedChannelCounts is set, then this value is derived from supportedChannelCounts. If
		setting maximumChannelCount makes the current format unsupported, then format will be set to
		nil. The default value is UINT_MAX.
*/
@property (nonatomic) AUAudioChannelCount maximumChannelCount;

@end


// =================================================================================================

/// Aspects of AUAudioUnitBusArray of interest only to subclassers.
@interface AUAudioUnitBusArray (AUAudioUnitBusImplementation)

/// Sets the bus array to be a copy of the supplied array. The base class issues KVO notifications.
- (void)replaceBusses:(NSArray <AUAudioUnitBus *> *)busArray;

@end

// =================================================================================================

/*!	Factory methods for building parameters, parameter groups, and parameter trees.

	@note In non-ARC code, "create" methods return unretained objects (unlike "create" 
	C functions); the caller should generally retain them.
*/
@interface AUParameterTree (Factory)

///	Create an AUParameter.
/// See AUParameter's properties for descriptions of the arguments.
+ (AUParameter *)createParameterWithIdentifier:(NSString *)identifier name:(NSString *)name address:(AUParameterAddress)address min:(AUValue)min max:(AUValue)max unit:(AudioUnitParameterUnit)unit unitName:(NSString * __nullable)unitName flags:(AudioUnitParameterOptions)flags valueStrings:(NSArray<NSString *> *__nullable)valueStrings dependentParameters:(NSArray<NSNumber *> *__nullable)dependentParameters;

/*!	@brief	Create an AUParameterGroup.
	@param identifier	An identifier for the group (non-localized, persistent).
	@param name			The group's human-readable name (localized).
	@param children		The group's child nodes.
*/
+ (AUParameterGroup *)createGroupWithIdentifier:(NSString *)identifier name:(NSString *)name children:(NSArray<AUParameterNode *> *)children;

/*!	@brief		Create a template group which may be used as a prototype for further group instances.

	@discussion
		Template groups provide a way to construct multiple instances of identical parameter
		groups, sharing certain immutable state between the instances.

		Template groups may not appear in trees except at the root.
*/
+ (AUParameterGroup *)createGroupTemplate:(NSArray<AUParameterNode *> *)children;

/*!	@brief	Initialize a group as a copied instance of a template group.
	@param templateGroup	A group to be used as a template and largely copied.
	@param identifier		An identifier for the new group (non-localized, persistent).
	@param name				The new group's human-readable name (localized).
	@param addressOffset	The new group's parameters' addresses will be offset from those in
							the template by this value.
*/
+ (AUParameterGroup *)createGroupFromTemplate:(AUParameterGroup *)templateGroup identifier:(NSString *)identifier name:(NSString *)name addressOffset:(AUParameterAddress)addressOffset;

/*!	@brief	Create an AUParameterTree.
	@param children		The tree's top-level child nodes.
*/
+ (AUParameterTree *)createTreeWithChildren:(NSArray<AUParameterNode *> *)children;

@end

// =================================================================================================

/// A block called to notify the AUAudioUnit implementation of changes to AUParameter values.
typedef void (^AUImplementorValueObserver)(AUParameter *param, AUValue value);

/// A block called to fetch an AUParameter's current value from the AUAudioUnit implementation.
typedef AUValue (^AUImplementorValueProvider)(AUParameter *param);

/// A block called to convert an AUParameter's value to a string.
typedef NSString *__nonnull (^AUImplementorStringFromValueCallback)(AUParameter *param, const AUValue *__nullable value);

/// A block called to convert a string to an AUParameter's value.
typedef AUValue (^AUImplementorValueFromStringCallback)(AUParameter *param, NSString *string);

/// A block called to return a group or parameter's localized display name, abbreviated to the requested length.
typedef NSString *__nonnull (^AUImplementorDisplayNameWithLengthCallback)(AUParameterNode *node, NSInteger desiredLength);

/// Aspects of AUParameterNode of interest only to AUAudioUnit subclassers.
@interface AUParameterNode (AUParameterNodeImplementation)
/*!	@brief		Called when a parameter changes value.
	@discussion
		This block, used only in an audio unit implementation, receives all externally-generated
		changes to parameter values. It should store the new value in its audio signal processing
		state (assuming that that state is separate from the AUParameter object).
*/
@property (NS_NONATOMIC_IOSONLY, copy) AUImplementorValueObserver implementorValueObserver;

/*!	@brief		Called when a value of a parameter in the tree is known to have a stale value
				needing to be refreshed.
	@discussion
		The audio unit should return the current value for this parameter; the AUParameterNode will
		store the value.
*/
@property (NS_NONATOMIC_IOSONLY, copy) AUImplementorValueProvider implementorValueProvider;

///	Called to provide string representations of parameter values.
///	If value is nil, the callback uses the current value of the parameter.
@property (NS_NONATOMIC_IOSONLY, copy) AUImplementorStringFromValueCallback implementorStringFromValueCallback;

/// Called to convert string to numeric representations of parameter values.
@property (NS_NONATOMIC_IOSONLY, copy) AUImplementorValueFromStringCallback implementorValueFromStringCallback;

/// Called to obtain an abbreviated version of a parameter or group name.
@property (NS_NONATOMIC_IOSONLY, copy) AUImplementorDisplayNameWithLengthCallback implementorDisplayNameWithLengthCallback;
@end

// =================================================================================================

/*!	@brief	Wraps a v2 audio unit in an AUAudioUnit subclass.
	@discussion
		Implementors of version 3 audio units may derive their implementations from
		AUAudioUnitV2Bridge. It expects the component description with which it is initialized to
		refer to a registered component with a v2 implementation using an
		AudioComponentFactoryFunction. The bridge will instantiate the v2 audio unit via the factory
		function and communicate it with it using the v2 AudioUnit API's (AudioUnitSetProperty,
		etc.)

		Hosts should not access this class; it will be instantiated when needed when creating an
		AUAudioUnit.
*/
API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
@interface AUAudioUnitV2Bridge : AUAudioUnit

/*! @property audioUnit
    @brief	  The underlying v2 AudioUnit
	@discussion
		We generally discourage interacting with the underlying v2 AudioUnit directly and
		recommend using the v3 equivalent methods and properties from AUAudioUnitV2Bridge.
		
		In some rare cases it may be desirable to interact with the v2 AudioUnit.
		For example, a v2 plugin may define custom properties that are not bridged to v3.
		Implementors can sublcass AUAudioUnitV2Bridge and call the v2 API methods
		AudioUnitGetProperty / AudioUnitSetProperty with the v2 AudioUnit.
*/
@property (nonatomic, readonly) AudioUnit audioUnit API_AVAILABLE(macos(11.0), ios(14.0), watchos(7.0), tvos(14.0));

@end

// =================================================================================================

/*!	@brief	Protocol to which principal classes of v3 audio units (extensions) must conform.
	@discussion
		The principal class of a non-UI v3 audio unit extension will generally derive from NSObject
		and implement this protocol.
		
		The principal class of a UI v3 audio unit extension must derive from AUViewController and
		implement this protocol.
*/
@protocol AUAudioUnitFactory <NSExtensionRequestHandling>

/*!	@brief	Create an instance of an extension's AUAudioUnit.
	@discussion
		This method should create and return an instance of its audio unit.
		
		This method will be called only once per instance of the factory.
		
		Note that in non-ARC code, "create" methods return unretained objects (unlike "create" 
		C functions); the implementor should return an object with reference count 1 but
		autoreleased.
*/
- (nullable AUAudioUnit *)createAudioUnitWithComponentDescription:(AudioComponentDescription)desc error:(NSError **)error;
@end


NS_ASSUME_NONNULL_END

#endif // __OBJC2__
#endif // AudioToolbox_AUAudioUnitImplementation_h
#else
#include <AudioToolboxCore/AUAudioUnitImplementation.h>
#endif
