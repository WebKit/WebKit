#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AUAudioUnit.h>)
/*!
	@file		AUAudioUnit.h
 	@framework	AudioToolbox.framework
 	@copyright	(c) 2015 Apple, Inc. All rights reserved.

	@brief		Objective-C interfaces for hosting and implementing Audio Units.
*/

#ifndef AudioToolbox_AUAudioUnit_h
#define AudioToolbox_AUAudioUnit_h
#ifdef __OBJC2__

#import <AudioToolbox/AUParameters.h>
#import <AudioToolbox/AudioUnitProperties.h>
#import <Foundation/NSExtensionRequestHandling.h>
#import <CoreMIDI/MIDIServices.h>

#if !TARGET_OS_IPHONE
typedef UInt32 AUAudioObjectID; // AudioObjectID
#endif

NS_ASSUME_NONNULL_BEGIN

// forward declarations
@class AVAudioFormat;
@class AUAudioUnitBusArray;
@class AUAudioUnitBus;
@class AUAudioUnitPreset;
@class MIDICIProfile;
@class MIDICIProfileState;
typedef uint8_t MIDIChannelNumber;

@protocol AUAudioUnitFactory;

@protocol AUMessageChannel;

// =================================================================================================

/*!	@typedef	AUAudioUnitStatus
	@brief		A result code returned from an audio unit's render function.
*/
typedef OSStatus AUAudioUnitStatus;

/*!	@var		AUEventSampleTimeImmediate
	@brief		A special value of AUEventSampleTime indicating "immediately."
	@discussion
		Callers of AUScheduleParameterBlock, AUMIDIEventListBlock, and AUScheduleMIDIEventBlock can pass
		AUEventSampleTimeImmediate to indicate that the event should be rendered as soon as
		possible, in the next cycle. A caller may also add a small (less than 4096) sample frame
		offset to this constant. The base AUAudioUnit implementation translates this constant to a
		true AUEventSampleTime; subclasses will not see it.
*/
enum : AUEventSampleTime {
	AUEventSampleTimeImmediate = (AUEventSampleTime)0xffffffff00000000LL
};

/*!	@typedef	AUAudioFrameCount
	@brief		A number of audio sample frames.
	@discussion
		This is `uint32_t` for impedence-matching with the pervasive use of `UInt32` in AudioToolbox
		and C AudioUnit API's, as well as `AVAudioFrameCount`.
*/
typedef uint32_t AUAudioFrameCount;

/*!	@typedef	AUAudioChannelCount
	@brief		A number of audio channels.
	@discussion
		This is `uint32_t` for impedence-matching with the pervasive use of `UInt32` in AudioToolbox
		and C AudioUnit API's, as well as `AVAudioChannelCount`.
*/
typedef uint32_t AUAudioChannelCount;

// =================================================================================================

/*!	@enum		AUAudioUnitBusType
	@brief		Describes whether a bus array is for input or output.
*/
typedef NS_ENUM(NSInteger, AUAudioUnitBusType) {
	AUAudioUnitBusTypeInput		= 1,
	AUAudioUnitBusTypeOutput	= 2
};

// =================================================================================================

/*!	@typedef	AURenderPullInputBlock
	@brief		Block to supply audio input to AURenderBlock.
	@param actionFlags
		Pointer to action flags.
	@param timestamp
		The HAL time at which the input data will be rendered. If there is a sample rate conversion
		or time compression/expansion downstream, the sample time will not be valid.
	@param frameCount
		The number of sample frames of input requested.
	@param inputBusNumber
		The index of the input bus being pulled.
	@param inputData
		The input audio data.

		The caller must supply valid buffers in inputData's mBuffers' mData and mDataByteSize.
		mDataByteSize must be consistent with frameCount. This block may provide input in those
		specified buffers, or it may replace the mData pointers with pointers to memory which it
		owns and guarantees will remain valid until the next render cycle.
	@return
		An AUAudioUnitStatus result code. If an error is returned, the input data should be assumed 
		to be invalid.
*/
typedef AUAudioUnitStatus (^AURenderPullInputBlock)(AudioUnitRenderActionFlags *actionFlags,
	const AudioTimeStamp *timestamp, AUAudioFrameCount frameCount, NSInteger inputBusNumber,
	AudioBufferList *inputData) CA_REALTIME_API;

/*!	@typedef	AURenderBlock
	@brief		Block to render the audio unit.
	@discussion
		All realtime operations are implemented using blocks to avoid ObjC method dispatching and
		the possibility of blocking.
	@param actionFlags
		Pointer to action flags.
	@param timestamp
		The HAL time at which the output data will be rendered. If there is a sample rate conversion
		or time compression/expansion downstream, the sample time will not have a defined
		correlation with the AudioDevice sample time.
	@param frameCount
		The number of sample frames to render.
	@param outputBusNumber
		The index of the output bus to render.
	@param outputData
		The output bus's render buffers and flags.

		The buffer pointers (outputData->mBuffers[x].mData) may be null on entry, in which case the
		block will render into memory it owns and modify the mData pointers to point to that memory.
		The block is responsible for preserving the validity of that memory until it is next called
		to render, or deallocateRenderResources is called.

		If, on entry, the mData pointers are non-null, the block will render into those buffers.
	@param pullInputBlock
		A block which the AU will call in order to pull for input data. May be nil for instrument
		and generator audio units (which do not have input busses).
	@return
		An `AUAudioUnitStatus` result code. If an error is returned, the output data should be assumed
		to be invalid.
*/
typedef AUAudioUnitStatus (^AURenderBlock)(AudioUnitRenderActionFlags *actionFlags,
	const AudioTimeStamp *timestamp, AUAudioFrameCount frameCount, NSInteger outputBusNumber,
	AudioBufferList *outputData, AURenderPullInputBlock __nullable pullInputBlock) CA_REALTIME_API;

/*!	@typedef	AURenderObserver
	@brief		Block called when an audio unit renders.
	@discussion
		This block is called by the base class's AURenderBlock before and after each render cycle.
		The observer can distinguish between before and after using the PreRender and PostRender
		flags.
		
		The parameters are identical to those of AURenderBlock.
*/
typedef void (^AURenderObserver)(AudioUnitRenderActionFlags actionFlags,
	const AudioTimeStamp *timestamp, AUAudioFrameCount frameCount, NSInteger outputBusNumber)
	CA_REALTIME_API;

/*!	@typedef	AUScheduleParameterBlock
	@brief		Block to schedule parameter changes.
	@discussion
		Not all parameters are rampable; check the parameter's flags.
		Note: If the parameter is not rampable, a rampDuration of zero will result in an immediate change to
		the target value, however, if rampDuration is non-zero, the parameter will not change.
 
	@param eventSampleTime
		The sample time (timestamp->mSampleTime) at which the parameter is to begin changing. When
		scheduling parameters during the render cycle (e.g. via a render observer) this time can be
		AUEventSampleTimeImmediate plus an optional buffer offset, in which case the event is
		scheduled at that position in the current render cycle.
	@param rampDurationSampleFrames
		The number of sample frames over which the parameter's value is to ramp, or 0 if the 
		parameter change should take effect immediately.
	@param parameterAddress
		The parameter's address.
	@param value
		The parameter's new value if the ramp duration is 0; otherwise, the value at the end
		of the scheduled ramp.
*/
typedef void (^AUScheduleParameterBlock)(AUEventSampleTime eventSampleTime,
	AUAudioFrameCount rampDurationSampleFrames, AUParameterAddress parameterAddress, AUValue value)
	CA_REALTIME_API;

/*!	@typedef	AUScheduleMIDIEventBlock
	@brief		Block to schedule MIDI events.
	@param eventSampleTime
		The sample time (timestamp->mSampleTime) at which the MIDI event is to occur. When
		scheduling events during the render cycle (e.g. via a render observer) this time can be
		AUEventSampleTimeImmediate plus an optional buffer offset, in which case the event is
		scheduled at that position in the current render cycle.
	@param cable
		The virtual cable number.
	@param length
		The number of bytes of MIDI data in the provided event(s).
	@param midiBytes
		One or more valid MIDI 1.0 events, except sysex which must always be sent as the only event
		in the chunk. Also, running status is not allowed.
*/
typedef void (^AUScheduleMIDIEventBlock)(AUEventSampleTime eventSampleTime, uint8_t cable,
	NSInteger length, const uint8_t *midiBytes) CA_REALTIME_API;

/*!	@typedef	AUMIDIOutputEventBlock
	@brief		Block to provide MIDI output events to the host.
	@param eventSampleTime
		The timestamp associated with the MIDI data in this chunk.
	@param cable
		The virtual cable number associated with this MIDI data.
	@param length
		The number of bytes of MIDI data in the provided event(s).
	@param midiBytes
		One or more valid MIDI 1.0 events, except sysex which must always be sent as the only event
		in the chunk.
*/
typedef OSStatus (^AUMIDIOutputEventBlock)(AUEventSampleTime eventSampleTime, uint8_t cable,
	NSInteger length, const uint8_t *midiBytes) CA_REALTIME_API;

/*!	@typedef	AUHostMusicalContextBlock
	@brief		Block by which hosts provide musical tempo, time signature, and beat position.
	@param	currentTempo
		The current tempo in beats per minute.
	@param	timeSignatureNumerator
		The numerator of the current time signature.
	@param	timeSignatureDenominator
		The denominator of the current time signature.
	@param	currentBeatPosition
		The precise beat position of the beginning of the current buffer being rendered.
	@param	sampleOffsetToNextBeat
		The number of samples between the beginning of the buffer being rendered and the next beat
		(can be 0).
	@param	currentMeasureDownbeatPosition
		The beat position corresponding to the beginning of the current measure.
	@return
		YES for success.
	@discussion
		If the host app provides this block to an AUAudioUnit (as its musicalContextBlock), then
		the block may be called at the beginning of each render cycle to obtain information about
		the current render cycle's musical context.
		
		Any of the provided parameters may be null to indicate that the audio unit is not interested
		in that particular piece of information.
*/
typedef BOOL (^AUHostMusicalContextBlock)(double * __nullable currentTempo,
	double * __nullable timeSignatureNumerator, NSInteger * __nullable timeSignatureDenominator,
	double * __nullable currentBeatPosition, NSInteger * __nullable sampleOffsetToNextBeat,
	double * __nullable currentMeasureDownbeatPosition) CA_REALTIME_API;

/*!	@typedef	AUMIDICIProfileChangedBlock
	@brief		Block by which hosts are informed of an audio unit having enabled or disabled a
				MIDI-CI profile.
	@param cable
		The virtual MIDI cable on which the event occured.
	@param channel
		The MIDI channel on which the profile was enabled or disabled.
	@param profile
		The MIDI-CI profile.
	@param enabled
		YES if the profile was enabled, NO if the profile was disabled.
*/
typedef void (^AUMIDICIProfileChangedBlock)(uint8_t cable, MIDIChannelNumber channel, MIDICIProfile *profile, BOOL enabled) API_AVAILABLE(macos(10.14), ios(12.0)) __WATCHOS_PROHIBITED __TVOS_PROHIBITED;

/*!	@enum		AUHostTransportStateFlags
	@brief		Flags describing the host's transport state.
	@constant	AUHostTransportStateChanged
		True if, since the callback was last called, there was a change to the state of, or
		discontinuities in, the host's transport. Can indicate such state changes as
		start/stop, or seeking to another position in the timeline.
	@constant	AUHostTransportStateMoving
		True if the transport is moving.
	@constant	AUHostTransportStateRecording
		True if the host is recording, or prepared to record. Can be true with or without the
		transport moving.
	@constant	AUHostTransportStateCycling
		True if the host is cycling or looping.
*/
typedef NS_OPTIONS(NSUInteger, AUHostTransportStateFlags) {
	AUHostTransportStateChanged			= 1,
	AUHostTransportStateMoving			= 2,
	AUHostTransportStateRecording		= 4,
	AUHostTransportStateCycling			= 8
};

/*!	@typedef	AUHostTransportStateBlock
	@brief		Block by which hosts provide information about their transport state.
	@param	transportStateFlags
		The current state of the transport.
	@param	currentSamplePosition
		The current position in the host's timeline, in samples at the audio unit's output sample
		rate.
	@param	cycleStartBeatPosition
		If cycling, the starting beat position of the cycle.
	@param	cycleEndBeatPosition
		If cycling, the ending beat position of the cycle.
	@discussion
		If the host app provides this block to an AUAudioUnit (as its transportStateBlock), then
		the block may be called at the beginning of each render cycle to obtain information about
		the current transport state.
		
		Any of the provided parameters may be null to indicate that the audio unit is not interested
		in that particular piece of information.
*/
typedef BOOL (^AUHostTransportStateBlock)(AUHostTransportStateFlags * __nullable transportStateFlags,
	double * __nullable currentSamplePosition, double * __nullable cycleStartBeatPosition,
	double * __nullable cycleEndBeatPosition) CA_REALTIME_API;

// =================================================================================================

/*!	@class		AUAudioUnit
	@brief		An audio unit instance.
	@discussion
		AUAudioUnit is a host interface to an audio unit. Hosts can instantiate either version 2 or
		version 3 units with this class, and to some extent control whether an audio unit is
		instantiated in-process or in a separate extension process.
		
		Implementors of version 3 audio units can and should subclass AUAudioUnit. To port an
		existing version 2 audio unit easily, AUAudioUnitV2Bridge can be subclassed.
		
		These are the ways in which audio unit components can be registered:
		
		- (v2) Packaged into a component bundle containing an `AudioComponents` Info.plist entry,
		referring to an `AudioComponentFactoryFunction`. See AudioComponent.h.
		
		- (v2) AudioComponentRegister(). Associates a component description with an
		AudioComponentFactoryFunction. See AudioComponent.h.
		
		- (v3) Packaged into an app extension containing an AudioComponents Info.plist entry.
		The principal class must conform to the AUAudioUnitFactory protocol, which will typically
		instantiate an AUAudioUnit subclass.

		- (v3) `+[AUAudioUnit registerSubclass:asComponentDescription:name:version:]`. Associates
		a component description with an AUAudioUnit subclass.
		
		A host need not be aware of the concrete subclass of AUAudioUnit that is being instantiated.
		`initWithComponentDescription:options:error:` ensures that the proper subclass is used.
		
		When using AUAudioUnit with a v2 audio unit, or the C AudioComponent and AudioUnit API's
		with a v3 audio unit, all major pieces of functionality are bridged between the
		two API's. This header describes, for each v3 method or property, the v2 equivalent.
*/
API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
@interface AUAudioUnit : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*!	@method		initWithComponentDescription:options:error:
	@brief		Designated initializer.
	@param componentDescription
		A single AUAudioUnit subclass may implement multiple audio units, for example, an effect
		that can also function as a generator, or a cluster of related effects. The component
		description specifies the component which was instantiated.
	@param options
		Options for loading the unit in-process or out-of-process.
	@param outError
		Returned in the event of failure.
*/
- (nullable instancetype)initWithComponentDescription:(AudioComponentDescription)componentDescription options:(AudioComponentInstantiationOptions)options error:(NSError **)outError NS_DESIGNATED_INITIALIZER;

/*!	@method		initWithComponentDescription:error:
	@brief		Convenience initializer (omits options).
*/
- (nullable instancetype)initWithComponentDescription:(AudioComponentDescription)componentDescription error:(NSError **)outError;

/*!	@method	instantiateWithComponentDescription:options:completionHandler:
	@brief	Asynchronously create an AUAudioUnit instance.
	@param componentDescription
		The AudioComponentDescription of the audio unit to instantiate.
	@param options
		See the discussion of AudioComponentInstantiationOptions in AudioToolbox/AudioComponent.h.
	@param completionHandler
		Called in a thread/dispatch queue context internal to the implementation. The client should
		retain the supplied AUAudioUnit.
	@discussion
		Certain types of AUAudioUnits must be instantiated asynchronously -- see 
		the discussion of kAudioComponentFlag_RequiresAsyncInstantiation in
		AudioToolbox/AudioComponent.h.

		Note: Do not block the main thread while waiting for the completion handler to be called;
		this can deadlock.
*/
+ (void)instantiateWithComponentDescription:(AudioComponentDescription)componentDescription options:(AudioComponentInstantiationOptions)options completionHandler:(void (^)(AUAudioUnit * __nullable audioUnit, NSError * __nullable error))completionHandler;

/*!	@property	componentDescription
	@brief		The AudioComponentDescription with which the audio unit was created.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) AudioComponentDescription componentDescription;

/*!	@property	component
	@brief		The AudioComponent which was found based on componentDescription when the
				audio unit was created.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) AudioComponent component;

/*!	@property	componentName
	@brief		The unit's component's name.
	@discussion
		By convention, an audio unit's component name is its manufacturer's name, plus ": ",
		plus the audio unit's name. The audioUnitName and manufacturerName properties are derived
		from the component name.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSString *componentName;

/*!	@property	audioUnitName
	@brief		The audio unit's name.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSString *audioUnitName;

/*!	@property	manufacturerName
	@brief		The manufacturer's name.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSString *manufacturerName;

/*!	@property	audioUnitShortName
	@brief		A short name for the audio unit.
	@discussion
		Audio unit host applications can display this name in situations where the audioUnitName 
		might be too long. The recommended length is up to 16 characters. Host applications may 
		truncate it.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSString *audioUnitShortName API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

/*!	@property	componentVersion
	@brief		The unit's component's version.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) uint32_t componentVersion;

/*!	@method		allocateRenderResourcesAndReturnError:
	@brief		Allocate resources required to render.
	@discussion
		Hosts must call this before beginning to render. Subclassers should call the superclass
		implementation.
		
		Bridged to the v2 API AudioUnitInitialize().
*/
- (BOOL)allocateRenderResourcesAndReturnError:(NSError **)outError;

/*!	@method		deallocateRenderResources
	@brief		Deallocate resources allocated by allocateRenderResourcesAndReturnError:
	@discussion
		Hosts should call this after finishing rendering. Subclassers should call the superclass
		implementation.
		
		Bridged to the v2 API AudioUnitUninitialize().
*/
- (void)deallocateRenderResources;

/*!	@property	renderResourcesAllocated
	@brief		returns YES if the unit has render resources allocated.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL renderResourcesAllocated;

/*!	@method		reset
	@brief		Reset transitory rendering state to its initial state.
	@discussion
		Hosts should call this at the point of a discontinuity in the input stream being provided to
		an audio unit, for example, when seeking forward or backward within a track. In response,
		implementations should clear delay lines, filters, etc. Subclassers should call the
		superclass implementation.
		
		Bridged to the v2 API AudioUnitReset(), in the global scope.
*/
- (void)reset;

/*!	@property	inputBusses
	@brief		An audio unit's audio input connection points.
	@discussion
		Subclassers must override this property's getter. The implementation should return the same
		object every time it is asked for it, since clients can install KVO observers on it.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) AUAudioUnitBusArray *inputBusses;

/*!	@property	outputBusses
	@brief		An audio unit's audio output connection points.
	@discussion
		Subclassers must override this property's getter. The implementation should return the same
		object every time it is asked for it, since clients can install KVO observers on it.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) AUAudioUnitBusArray *outputBusses;

/*!	@property	renderBlock
	@brief		Block which hosts use to ask the unit to render.
	@discussion
		Before invoking an audio unit's rendering functionality, a host should fetch this block and
		cache the result. The block can then be called from a realtime context without the
		possibility of blocking and causing an overload at the Core Audio HAL level.
		
		This block will call a subclass' internalRenderBlock, providing all realtime events
		scheduled for the current render time interval, bracketed by calls to any render observers.

		Subclassers should override internalRenderBlock, not this property.
		
		Bridged to the v2 API AudioUnitRender().
*/
@property (NS_NONATOMIC_IOSONLY, readonly) AURenderBlock renderBlock;

/*!	@property	scheduleParameterBlock
	@brief		Block which hosts use to schedule parameters.
	@discussion
		As with renderBlock, a host should fetch and cache this block before calling
		allocateRenderResources, if it intends to schedule parameters.
				
		The block is safe to call from any thread context, including realtime audio render
		threads.
		
		Subclassers should not override this; it is implemented in the base class and will schedule
		the events to be provided to the internalRenderBlock.
		
		Bridged to the v2 API AudioUnitScheduleParameters().
*/
@property (NS_NONATOMIC_IOSONLY, readonly) AUScheduleParameterBlock scheduleParameterBlock;

/*!	@method		tokenByAddingRenderObserver:
	@brief		Add a block to be called on each render cycle.
	@discussion
		The supplied block is called at the beginning and ending of each render cycle. It should
		not make any blocking calls.
		
		This method is implemented in the base class AUAudioUnit, and should not be overridden.
		
		Bridged to the v2 API AudioUnitAddRenderNotify().
	@param observer
		The block to call.
	@return
		A token to be used when removing the observer.
*/
- (NSInteger)tokenByAddingRenderObserver:(AURenderObserver)observer;

/*!	@method		removeRenderObserver:
	@brief		Remove an observer block added via tokenByAddingRenderObserver:
	@param token
		The token previously returned by tokenByAddingRenderObserver:

		Bridged to the v2 API AudioUnitRemoveRenderNotify().
*/
- (void)removeRenderObserver:(NSInteger)token;

/*!	@property	maximumFramesToRender
	@brief		The maximum number of frames which the audio unit can render at once.
	@discussion
		This must be set by the host before render resources are allocated. It cannot be changed
		while render resources are allocated.
		
		Bridged to the v2 property kAudioUnitProperty_MaximumFramesPerSlice.
*/
@property (NS_NONATOMIC_IOSONLY) AUAudioFrameCount maximumFramesToRender;

/*!	@property	parameterTree
	@brief		An audio unit's parameters, organized in a hierarchy.
	@return
		A parameter tree object, or nil if the unit has no parameters.
	@discussion
		Audio unit hosts can fetch this property to discover a unit's parameters. KVO notifications
		are issued on this member to notify the host of changes to the set of available parameters.
		
		AUAudioUnit has an additional pseudo-property, "allParameterValues", on which KVO
		notifications are issued in response to certain events where potentially all parameter
		values are invalidated. This includes changes to currentPreset, fullState, and
		fullStateForDocument.
 
		Hosts should not attempt to set this property.

		Subclassers should implement the parameterTree getter to expose parameters to hosts. They
		should cache as much as possible and send KVO notifications on "parameterTree" when altering
		the structure of the tree or the static information (ranges, etc) of parameters.
		
		This is similar to the v2 properties kAudioUnitProperty_ParameterList and
		kAudioUnitProperty_ParameterInfo.
 
		Note that it is not safe to modify this property in a real-time context.
*/
@property (NS_NONATOMIC_IOSONLY, nullable, retain) AUParameterTree *parameterTree;

/*!	@method		parametersForOverviewWithCount:
	@brief		Returns the audio unit's `count` most important parameters.
	@discussion
		This property allows a host to query an audio unit for some small number of parameters which
		are its "most important", to be displayed in a compact generic view.

		An audio unit subclass should return an array of NSNumbers representing the addresses
		of the `count` most important parameters.

		The base class returns an empty array regardless of count.
		
		Partially bridged to kAudioUnitProperty_ParametersForOverview (v2 hosts can use that
		property to access this v3 method of an audio unit).
*/
- (NSArray<NSNumber *> *)parametersForOverviewWithCount:(NSInteger)count;

@property (NS_NONATOMIC_IOSONLY, readonly) BOOL allParameterValues;	/// special pseudo-property for KVO

/*!	@property	musicDeviceOrEffect
	@brief		Specifies whether an audio unit responds to MIDI events.
	@discussion
		This is implemented in the base class and returns YES if the component type is music
		device or music effect.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, getter=isMusicDeviceOrEffect) BOOL musicDeviceOrEffect;

/*!	@property	virtualMIDICableCount
	@brief		The number of virtual MIDI cables implemented by a music device or effect.
	@discussion
		A music device or MIDI effect can support up to 256 virtual MIDI cables of input; this
		property expresses the number of cables supported by the audio unit.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) NSInteger virtualMIDICableCount;

/*!	@property	scheduleMIDIEventBlock
	@brief		Block used to schedule MIDI events.
	@discussion
		As with renderBlock, a host should fetch and cache this block before calling
		allocateRenderResources if it intends to schedule MIDI events.

		This is implemented in the base class. It is nil when musicDeviceOrEffect is NO.

		Subclasses should not override. When hosts schedule events via this block, they are
		sent to the Audio Unit via the list of AURenderEvents delivered to
		internalRenderBlock.
 
		All events sent via this block will be delivered to the internalRenderBlock in the MIDI
		protocol returned by the AudioUnitMIDIProtocol property. For example, if AudioUnitMIDIProtocol
		returns kMIDIProtocol_2_0, incoming events will be translated to MIDI 2.0 if necessary.
		If AudioUnitMIDIProtocol is not set, events will be delivered as legacy MIDI.
		
		This bridged to the v2 API MusicDeviceMIDIEvent.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, nullable) AUScheduleMIDIEventBlock scheduleMIDIEventBlock;

/*! @property	scheduleMIDIEventListBlock
	@brief		Block used to schedule MIDIEventLists.
	@discussion
		As with renderBlock, a host should fetch and cache this block before calling
		allocateRenderResources, if it intends to schedule MIDI events.

		When scheduling events during the render cycle (e.g. via a render observer) eventSampleTime can be
		AUEventSampleTimeImmediate plus an optional buffer offset, in which case the event is
		scheduled at the provided offset position within the current render cycle.

		This is implemented in the base class. It is nil when musicDeviceOrEffect is NO.

		Subclassers should not override. When hosts schedule events via this block, they are
		delivered to the Audio Unit via the list of AURenderEvents delivered to
		internalRenderBlock.
 
		All events sent via this block will be delivered to the internalRenderBlock in the MIDI protocol returned by
		the AudioUnitMIDIProtocol property. For example, if this block is called with MIDI-1.0 events but
		AudioUnitMIDIProtocol returns kMIDIProtocol_2_0, incoming events will be translated to MIDI 2.0.
		If AudioUnitMIDIProtocol is not set, events will be delivered as legacy MIDI.
 
		Note: This block should be preferred over scheduleMIDIEventBlock going forward.

		This bridged to the v2 API MusicDeviceMIDIEventList.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, nullable) AUMIDIEventListBlock scheduleMIDIEventListBlock API_AVAILABLE(macos(12.0), ios(15.0), tvos(15.0)) API_UNAVAILABLE(watchos);

/*!	@property	MIDIOutputNames
	@brief		Count, and names of, a plug-in's MIDI outputs.
	@discussion
		A plug-in may override this method to inform hosts about its MIDI outputs. The size of the
		array is the number of outputs the Audio Unit supports. Each item in the array is the name
		of the MIDI output at that index.

		This is bridged to the v2 API property kAudioUnitProperty_MIDIOutputCallbackInfo.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray<NSString *> *MIDIOutputNames API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

/*!	@property	providesUserInterface
	@brief		Specifies whether an audio unit provides UI (normally in the form of a view controller).
	@discussion
		Implemented in the framework and should not be overridden by implementators. The
		framework detects whether any subclass has implemented
		`requestViewControllerWithCompletionHandler:` or is implemented by an AU extension whose
		extension point identifier is `com.apple.AudioUnit-UI`. See also
		`requestViewControllerWithCompletionHandler:` in <CoreAudioKit/AUViewController.h>
*/
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL providesUserInterface API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

// ------------------------
// These properties and methods are generally optional.

/*!	@property	MIDIOutputEventBlock
	@brief		Block used by the host to access the MIDI output generated by an Audio Unit.
	@discussion
 		The host can set this block and the plug-in can call it in its renderBlock to provide to the
 		host the MIDI data associated with the current render cycle.
 
		All events sent via this block will be delivered to the host in the MIDI protocol returned by
		the hostMIDIProtocol property. For example, if hostMIDIProtocol is set to
		kMIDIProtocol_2_0, incoming events will be translated to MIDI 2.0. If hostMIDIProtocol
		is not set, events will be delivered as legacy MIDI.

		Note: AUMIDIEventListBlock should be preferred over this block going forward.

		This is bridged to the v2 API property kAudioUnitProperty_MIDIOutputCallback.
 */
@property (NS_NONATOMIC_IOSONLY, copy, nullable) AUMIDIOutputEventBlock MIDIOutputEventBlock API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

/*! @property	MIDIOutputEventListBlock
	@brief		Block used by the host to access the MIDIEventList output generated by an Audio Unit.
	@discussion
		The host can set this block and the plug-in can call it in its renderBlock to provide to the
		host the MIDIEventList data associated with the current render cycle.

		All events sent via this block will be delivered to the host in the MIDI protocol returned by
		the hostMIDIProtocol property. For example, if hostMIDIProtocol is set to
		kMIDIProtocol_2_0, incoming events will be translated to MIDI 2.0. If hostMIDIProtocol
		is not set, events will be delivered as legacy MIDI.

		Note: This block is cross-compatible with Audio Units using MIDIOutputEventBlock and should be
		preferred over MIDIOutputEventBlock by hosts going forward. The framework will provide the Audio Unit
		with both a MIDIOutputEventBlock and MIDIOutputEventListBlock, the Audio Unit is free to call either
		block as all messages will be translated as described above.
 
		Host should setup in the following order:
		 - Set hostMIDIProtocol
		 - Set MIDIOutputEventListBlock
		 - Call allocateRenderResourcesAndReturnError
 
		This is bridged to the v2 API property kAudioUnitProperty_MIDIOutputEventListCallback.
*/
@property (NS_NONATOMIC_IOSONLY, copy, nullable) AUMIDIEventListBlock MIDIOutputEventListBlock API_AVAILABLE(macos(12.0), ios(15.0), tvos(15.0)) API_UNAVAILABLE(watchos);

/*! @property	AudioUnitMIDIProtocol
	@brief		The MIDI protocol used by the Audio Unit for receiving MIDIEventList data.
	@discussion
		Subclassers should override to return the desired protocol in which the Audio Unit wants
		to receive input MIDI data, otherwise the Audio Unit will default to receiving legacy MIDI.
		
		All translatable messages will be converted (if necessary) to this protocol prior to delivery
		to the Audio Unit.
 
		This is bridged to the v2 API property kAudioUnitProperty_AudioUnitMIDIProtocol.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) MIDIProtocolID AudioUnitMIDIProtocol API_AVAILABLE(macos(12.0), ios(15.0), tvos(15.0)) API_UNAVAILABLE(watchos);

/*! @property	hostMIDIProtocol
	@brief		The MIDI protocol to be used by the host for receiving MIDIEventList data.
	@discussion
		Hosts should set this property to the protocol they wish to receive MIDIEventList data
		from the Audio Unit. This should be set prior to initialization, all translatable messages
		will be converted  (if necessary) to this property's protocol prior to delivery to the host.
 
		Host should setup in the following order:
		- Set hostMIDIProtocol
		- Set MIDIOutputEventListBlock
		- Call allocateRenderResourcesAndReturnError

		This is bridged to the v2 API property kAudioUnitProperty_HostMIDIProtocol.
 
		Notes:
		- If overriding this property, subclassers must call [super setHostMIDIProtocol:]
		- hostMIDIProtocol should be set before attempting to query AudioUnitMIDIProtocol
		or calling allocateRenderResourcesAndReturnError to allow Audio Units to
		optionally match their input MIDI protocol to the desired host protocol and prevent
		protocol conversion.

*/
@property (NS_NONATOMIC_IOSONLY) MIDIProtocolID hostMIDIProtocol API_AVAILABLE(macos(12.0), ios(15.0), tvos(15.0)) API_UNAVAILABLE(watchos);

/*!	@property	fullState
	@brief		A persistable snapshot of the Audio Unit's properties and parameters, suitable for
				saving as a user preset.
	@discussion
		Hosts may use this property to save and restore the state of an Audio Unit being used in a
		user preset or document. The Audio Unit should not persist transitory properties such as
		stream formats, but should save and restore all parameters and custom properties.
		
		The base class implementation of this property saves the values of all parameters 
		currently in the parameter tree. A subclass which dynamically produces multiple variants
		of the parameter tree needs to be aware that the serialization method does a depth-first
		preorder traversal of the tree.
		
		Bridged to the v2 property kAudioUnitProperty_ClassInfo.
*/
@property (NS_NONATOMIC_IOSONLY, copy, nullable) NSDictionary<NSString *, id> *fullState;

/*!	@property	fullStateForDocument
	@brief		A persistable snapshot of the audio unit's properties and parameters, suitable for
				saving in a user's document.
	@discussion
		This property is distinct from fullState in that some state is suitable for saving in user
		presets, while other state is not. For example, a synthesizer's master tuning setting could
		be considered global state, inappropriate for storing in reusable presets, but desirable
		for storing in a document for a specific live performance.
		
		Hosts saving documents should use this property. If the audio unit does not implement it,
		the base class simply sets/gets fullState.

		Bridged to the v2 property kAudioUnitProperty_ClassInfoFromDocument.
*/
@property (NS_NONATOMIC_IOSONLY, copy, nullable) NSDictionary<NSString *, id> *fullStateForDocument;

/*!	@property	factoryPresets
	@brief		A collection of presets provided by the audio unit's developer.
	@discussion
		A preset provides users of an audio unit with an easily-selectable, fine-tuned set of
		parameters provided by the developer. This property returns all of the available factory presets.

		Bridged to the v2 property kAudioUnitProperty_FactoryPresets.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSArray<AUAudioUnitPreset *> *factoryPresets;

/*!	@property	userPresets
	@brief		A collection of presets saved by the user
	@discussion
		In addition to factory presets, provided by the audio unit vendor, users have the ability to
		save the values of the parameters of an audio unit into a user preset. These users presets
		can be accessed using this property.

		The default implementation of this method will load the user presets from an internal
		location that might not be directly accessible to the audio unit host application or to the
		audio unit. Instead of accessing this path directly, the audio unit should rely on the
		superclass implementation of this method to retrieve the presets.

		Audio Units are free to override this method to load their user presets via different means
		(e.g. from their iCloud container).
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray<AUAudioUnitPreset *> *userPresets API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));

/*!	@method		saveUserPreset:error
	@brief		Persistently save the current state of the audio unit into a userPreset
	@discussion
		The new preset will be added to userPresets and will become selectable by assigning it
		to the currentPreset property.
		If a preset with the provided name already exists then it will be overwritten.

		For user presets, the preset number is required to be negative.
		If a positive number is passed, the sign will be changed to negative.
		If zero is passed, the number will be set to -1.
		These changes will be reflected on the userPreset argument.

		The default implementation of this method will save the user preset to an internal
		location.

		Audio Units are free to override this method to operate on a different location (e.g. their
		iCloud container).
	@param	userPreset
		The preset under which the current state will be saved.
	@param outError
		In the event of a failure, the method will return NO and outError will be set to an 
		NSError, describing the problem. 
		Some possible errors: 
				- domain: NSOSStatusErrorDomain code: kAudioUnitErr_NoConnection
				- domain: NSOSStatusErrorDomain	code: kAudioUnitErr_InvalidFilePath
				- domain: NSOSStatusErrorDomain	code: kAudioUnitErr_MissingKey
	@return
		YES for success. NO in the event of a failure, in which case the error is returned in
		outError.
 */
- (BOOL)saveUserPreset:(AUAudioUnitPreset *)userPreset error:(NSError **)outError API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));

/*!	@method		deleteUserPreset:error
	@brief		Remove a user preset.
	@discussion
		The user preset will be removed from userPresets and will be permanently deleted.

		The default implementation of this method will delete the user preset from an internal
		location.

		Audio Units are free to override this method to operate on a different location (e.g. their
		iCloud container).
	@param	userPreset
		The preset to be deleted.
	@param	outError
		In the event of a failure, the method will return NO and outError will be set to an 
		NSError, describing the problem. 
		Some possible errors: 
				- domain: NSOSStatusErrorDomain code: kAudioUnitErr_NoConnection
				- domain: NSPOSIXErrorDomain	code: ENOENT
				- domain: NSOSStatusErrorDomain	code: kAudioUnitErr_InvalidFilePath
	@return
		YES for success. NO in the event of a failure, in which case the error is returned in
		outError.
*/
- (BOOL)deleteUserPreset:(AUAudioUnitPreset *)userPreset error:(NSError **)outError API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));

/*! @method		presetStateFor:error
	@brief		Retrieve the state stored in a user preset
 	@discussion
		This method allows access to the contents of a preset without having to set that preset as
		current. The returned dictionary is assignable to the audio unit's fullState and/or
		fullStateForDocument properties.
 
		Audio units can override this method in order to vend user presets from a different location
		(e.g. their iCloud container).

		In order to restore the state from a user preset, the audio unit should override the setter
		for the currentPreset property and check the preset number to determine the type of preset.
		If the preset number is >= 0 then the preset is a factory preset.
		If the preset number is < 0 then it is a user preset.

		This method can then be called to retrieve the state stored in a user preset and the audio
		unit can assign this to fullState or fullStateForDocument.

	@param	userPreset
		The preset to be selected.
	@param	outError
		In the event of a failure, the method will return nil and outError will be set to an 
		NSError, describing the problem. 
		Some possible errors: 
				- domain: NSOSStatusErrorDomain code: kAudioUnitErr_NoConnection
				- domain: NSPOSIXErrorDomain	code: ENOENT
				- domain: NSCocoaErrorDomain	code: NSCoderReadCorruptError
	@return
		Returns nil if there was an error, otherwise returns a dictionary containing the full state
		of the audio unit saved in the preset.
		For details on the possible keys present in the full state dictionary, please see the
		documentation for kAudioUnitProperty_ClassInfo.
 		The minimal set of keys and their type is:	@kAUPresetTypeKey : NSNumber,
													@kAUPresetSubtypeKey : NSNumber,
 													@kAUPresetManufacturerKey : NSNumber,
 											   		@kAUPresetVersionKey : NSNumber,
 													@kAUPresetNameKey : NSString,
 													@kAUPresetNumberKey: NSNumber,
													@kAUPresetDataKey : NSData
*/
- (nullable NSDictionary<NSString *, id> *)presetStateFor:(AUAudioUnitPreset *)userPreset error:(NSError **)outError API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));

/*!	@property	supportsUserPresets
	@brief		Specifies whether an audio unit supports loading and saving user presets
	@discussion
		The audio unit should set this property to YES if a user preset can be assigned to
		currentPreset.

		Audio unit host applications should query this property to determine whether the audio unit
		supports user presets.

		Assigning a user preset to the currentPreset property of an audio unit that does not support
		restoring state from user presets may result in incorrect behavior.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL supportsUserPresets API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));

/*!	@property	isLoadedInProcess
	@brief		Set to YES when an AUAudioUnit is loaded in-process
	@discussion
		If the AUAudioUnit is instantiated with kAudioComponentInstantiation_LoadInProcess, but the
		audio unit is not packaged properly to support loading in-process, the system will silently
		fall back to loading the audio unit out-of-process.

		This property can be used to determine whether the instantiation succeeded as intended and
		the audio unit is running in-process.

		The presence of an extension process is not sufficient indication that the audio unit failed
		to load in-process, since the framework might launch the audio unit extension process to
		fulfill auxiliary functionality. If the audio unit is loaded in-process then rendering is
		done in the host process. Other operations that are not essential to rendering audio, might
		be done in the audio unit's extension process.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL isLoadedInProcess API_AVAILABLE(macos(10.15)) API_UNAVAILABLE(ios, watchos, tvos);

/*!	@property	currentPreset
	@brief		The audio unit's last-selected preset.
	@discussion
		Hosts can let the user select a preset by setting this property. Note that when getting
		this property, it does not reflect whether parameters may have been modified since the
		preset was selected.

		Bridged to the v2 property kAudioUnitProperty_PresentPreset.
*/
@property (NS_NONATOMIC_IOSONLY, retain, nullable) AUAudioUnitPreset *currentPreset;

/*!	@property	latency
	@brief		The audio unit's processing latency, in seconds.
	@discussion
		This property reflects the delay between when an impulse in the unit's audio unit stream
		arrives in the input vs. output streams. This should reflect the delay due
		to signal processing (e.g. filters, FFT's, etc.), not delay or reverberation which is
		being applied as an effect.
		
		Note that a latency that varies with parameter settings, including bypass, is generally not
		useful to hosts. A host is usually only prepared to add delays before starting to render and
		those delays need to be fixed. A variable delay would introduce artifacts even if the host
		could track it. If an algorithm has a variable latency it should be adjusted upwards to some
		fixed latency within the audio unit. If for some reason this is not possible, then latency
		could be regarded as an unavoidable consequence of the algorithm and left unreported (i.e.
		with a value of 0).

		Bridged to the v2 property kAudioUnitProperty_Latency.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) NSTimeInterval latency;

/*!	@property	tailTime
	@brief		The audio unit's tail time, in seconds.
	@discussion
		This property reflects the time interval between when the input stream ends or otherwise
		transitions to silence, and when the output stream becomes silent. Unlike latency, this
		should reflect the duration of a delay or reverb effect.
		
		Bridged to the v2 property kAudioUnitProperty_TailTime.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) NSTimeInterval tailTime;

/*!	@property	renderQuality
	@brief		Provides a trade-off between rendering quality and CPU load.
	@discussion
		The range of valid values is 0-127.
		
		Bridged to the v2 property kAudioUnitProperty_RenderQuality.
*/
@property (NS_NONATOMIC_IOSONLY) NSInteger renderQuality;

/*!	@property	shouldBypassEffect
	@brief		Directs an effect to route input directly to output, without any processing.
	@discussion
		Bridged to the v2 property kAudioUnitProperty_BypassEffect.
*/
@property (NS_NONATOMIC_IOSONLY) BOOL shouldBypassEffect;

/*!	@property	canProcessInPlace
	@brief		Expresses whether an audio unit can process in place.
	@discussion
		In-place processing is the ability for an audio unit to transform an input signal to an
		output signal in-place in the input buffer, without requiring a separate output buffer.
		
		A host can express its desire to process in place by using null mData pointers in the output
		buffer list. The audio unit may process in-place in the input buffers. See the discussion of
		renderBlock.
		
		Partially bridged to the v2 property kAudioUnitProperty_InPlaceProcessing; in v3 it is not
		settable.
		
		Defaults to NO. Subclassers can override to return YES.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL canProcessInPlace;

/*!	@property	renderingOffline
	@brief		Communicates to an audio unit that it is rendering offline.
	@discussion
		A host should set this property when using an audio unit in a context where there are no
		realtime deadlines, before asking the unit to allocate render resources. An audio unit may
		respond by using a more expensive signal processing algorithm, or allowing itself to block
		at render time if data being generated on secondary work threads is not ready in time.
		(Normally, in a realtime thread, this data would have to be dropped).

		Bridged to the v2 property kAudioUnitProperty_OfflineRender.
*/
@property (NS_NONATOMIC_IOSONLY, getter=isRenderingOffline) BOOL renderingOffline;

/*!	@property	channelCapabilities
	@brief		Expresses valid combinations of input and output channel counts.
	@discussion
		Elements are NSNumber containing integers; [0]=input count, [1]=output count, [2]=2nd input
		count, [3]=2nd output count, etc.

		An input, output count of (2, 2) signifies that the audio unit can process 2 input channels
		to 2 output channels.
		
		Negative numbers (-1, -2) indicate *any* number of channels. (-1, -1) means any number
		of channels on input and output as long as they are the same. (-1, -2) means any number of
		channels on input or output, without the requirement that the counts are the same.
		
		A negative number less than -2 is used to indicate a total number of channels across every
		bus in that scope, regardless of how many channels are set on any particular bus. For
		example, (-16, 2) indicates that a unit can accept up to 16 channels of input across its
		input busses, but will only produce 2 channels of output.
		
		Zero on either side (though typically input) means "not applicable", because there are no
		elements on that side.

		Bridged to the v2 property kAudioUnitProperty_SupportedNumChannels.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSArray<NSNumber *> *channelCapabilities;

/*!	@property	musicalContextBlock
	@brief		A callback for the AU to call the host for musical context information.
	@discussion
		Note that an audio unit implementation accessing this property should cache it in
		realtime-safe storage before beginning to render.
		
		Bridged to the HostCallback_GetBeatAndTempo and HostCallback_GetMusicalTimeLocation
		callback members in kAudioUnitProperty_HostCallbacks.
*/
@property (NS_NONATOMIC_IOSONLY, copy, nullable) AUHostMusicalContextBlock musicalContextBlock;

/*!	@property	transportStateBlock
	@brief		A callback for the AU to call the host for transport state information.
	@discussion
		Note that an audio unit implementation accessing this property should cache it in
		realtime-safe storage before beginning to render.
		
		Bridged to the HostCallback_GetTransportState and HostCallback_GetTransportState2
		callback members in kAudioUnitProperty_HostCallbacks.
*/
@property (NS_NONATOMIC_IOSONLY, copy, nullable) AUHostTransportStateBlock transportStateBlock;

/*!	@property	contextName
	@brief		Information about the host context in which the audio unit is connected, for display
				in the audio unit's view.
	@discussion
		For example, a host could set "track 3" as the context, so that the audio unit's view could
		then display to the user "My audio unit on track 3".

		Bridged to the v2 property kAudioUnitProperty_ContextName.
*/
@property (NS_NONATOMIC_IOSONLY, copy, nullable) NSString *contextName;

/*!	@property	migrateFromPlugin
	@brief		Information for migrating data from other audio plug-ins to the v3 Audio Unit architecture.
	@discussion
		This can be used to migrate settings from an older Audio Unit; this allows manufacturers
		to deprecate older Audio Units and replace them with new ones. The data for the older Audio Unit is
		an array of NSData representing byte encoded AudioUnitOtherPluginDescs to migrate from.
		Can also be used to migrate from a v2 to a v3 Audio Unit.

		Bridged to the v2 property kAudioUnitMigrateProperty_FromPlugin.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) NSArray* migrateFromPlugin API_AVAILABLE(macos(13.0), ios(16.0), watchos(9.0), tvos(16.0));


/*!	@property	supportsMPE
	@brief		Specifies whether an audio unit supports Multi-dimensional Polyphonic Expression.
	@discussion
		Bridged to the v2 property kAudioUnitProperty_SupportsMPE.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL supportsMPE API_AVAILABLE(macos(10.12), ios(10.0), watchos(3.0), tvos(10.0));

/*!	@property	channelMap
	@brief		Specify a mapping of input channels to output channels.
	@discussion
		Converter and input/output audio units may support re-ordering or splitting of input
		channels to output channels. The number of channels in the channel map is the number of
		channels of the destination (output format). The channel map entries contain a channel
		number of the source channel that should be mapped to that destination channel. If -1 is
		specified, then that destination channel will not contain any channel from the source (so it
		will be silent).
		
		If the property value is nil, then the audio unit does not support this property.
 
		Bridged to the v2 property kAudioOutputUnitProperty_ChannelMap.
*/
@property (NS_NONATOMIC_IOSONLY, copy, nullable) NSArray<NSNumber *> *channelMap API_AVAILABLE(macos(10.12), ios(10.0), watchos(3.0), tvos(10.0));

/*!	@method		profileStateForCable:channel:
	@brief		Given a MIDI cable and channel number, return the supported MIDI-CI Profiles.
	@param cable
		The virtual MIDI cable for which the profiles are requested.
	@param channel
		The MIDI channel for which the profiles are requested.
	@return
		A MIDICIProfileState object containing all the supported MIDI-CI profiles for this channel
		on this cable.
*/
- (MIDICIProfileState *)profileStateForCable:(uint8_t)cable channel:(MIDIChannelNumber)channel API_AVAILABLE(macos(10.14), ios(12.0)) __WATCHOS_PROHIBITED __TVOS_PROHIBITED;

/*!	@method		enableProfile:cable:onChannel:error:
	@brief		Enable a MIDI-CI Profile on the specified cable/channel.
	@param	profile
		The MIDI-CI profile to be enabled.
	@param cable
		The virtual MIDI cable.
	@param channel
		The MIDI channel.
	@param outError
		Returned in the event of failure.
	@return
		YES for success. NO in the event of a failure, in which case the error is returned
		in outError.
*/
- (BOOL)enableProfile:(MIDICIProfile *)profile
                cable:(uint8_t)cable
            onChannel:(MIDIChannelNumber)channel
                error:(NSError **)outError API_AVAILABLE(macos(10.14), ios(12.0)) __WATCHOS_PROHIBITED __TVOS_PROHIBITED;

/*!	@method		disableProfile:cable:onChannel:error:
	@brief		Disable a MIDI-CI Profile on the specified cable/channel.
	@param	profile
		The MIDI-CI profile to be disabled.
	@param cable
		The virtual MIDI cable.
	@param channel
		The MIDI channel.
	@param outError
		Returned in the event of failure.
	@return
		YES for success. NO in the event of a failure, in which case the error is returned
		in outError.
*/
- (BOOL)disableProfile:(MIDICIProfile *)profile
                 cable:(uint8_t)cable
             onChannel:(MIDIChannelNumber)channel
                 error:(NSError **)outError API_AVAILABLE(macos(10.14), ios(12.0)) __WATCHOS_PROHIBITED __TVOS_PROHIBITED;

/*!	@property	profileChangedBlock
	@brief		A block called when a device notifies that a MIDI-CI profile has been enabled or 
				disabled.
	@discussion
		Since enabling / disabling MIDI-CI profiles is an asynchronous operation, the host can set 
		this block and the audio unit is expected to call it every time the state of a MIDI-CI 
		profile has changed.
*/
@property (nonatomic, nullable) AUMIDICIProfileChangedBlock profileChangedBlock API_AVAILABLE(macos(10.14), ios(12.0)) __WATCHOS_PROHIBITED __TVOS_PROHIBITED;

/*!	@method		messageChannelFor:
	@brief		Returns an object for bidirectional communication between an Audio Unit and its host.
	@discussion
		Message channels provide a flexible way for custom data exchange between an Audio Unit and its host.
		An Audio Unit can support multiple message channels which are identified by the `channelName`.
		The message channel object's lifetime is managed by the host. Message channel objects should be designed
		in such a way that they could outlive the AU that vended them.
		For further details see discussion for `AUMessageChannel`.
	@param	channelName
		The name of the message channel to be returned by the Audio Unit if supported.
	@return
		An object that conforms to the `AUMessageChannel` protocol.
*/
- (id<AUMessageChannel>)messageChannelFor:(NSString *)channelName API_AVAILABLE(macos(13.0), ios(16.0)) __WATCHOS_PROHIBITED __TVOS_PROHIBITED;

@end

// =================================================================================================

/*!	@typedef	AUInputHandler
	@brief		Block to notify the client of an I/O unit that input is available.
	@param actionFlags
		Pointer to action flags.
	@param timestamp
		The HAL time at which the input data was captured. If there is a sample rate conversion
		or time compression/expansion downstream, the sample time will not be valid.
	@param frameCount
		The number of sample frames of input available.
	@param inputBusNumber
		The index of the input bus from which input is available.
	@discussion	The input data is obtained by calling the render block of the audio unit.
				The AUAudioUnit is not provided since it is not safe to message an Objective C 
				object in a real time context.
*/
typedef void (^AUInputHandler)(AudioUnitRenderActionFlags *actionFlags,
	const AudioTimeStamp *timestamp, AUAudioFrameCount frameCount, NSInteger inputBusNumber)
	CA_REALTIME_API;

/*!	@brief		Additional methods for audio units which can do input/output.
	@discussion	These methods will fail if the audio unit is not an input/output audio unit.
*/
@interface AUAudioUnit (AUAudioInputOutputUnit)

/*!	@property	canPerformInput
	@brief		Whether the I/O device can perform input.
*/
@property (nonatomic, readonly) BOOL canPerformInput;

/*!	@property	canPerformOutput
	@brief		Whether the I/O device can perform output.
*/
@property (nonatomic, readonly) BOOL canPerformOutput;

/*!	@property	inputEnabled
	@brief		Flag enabling audio input from the unit.
	@discussion	Input is disabled by default. This must be set to YES if input audio is desired. 
				Setting to YES will have no effect if canPerformInput is false.
*/
@property (nonatomic, getter=isInputEnabled) BOOL inputEnabled;

/*!	@property	outputEnabled
	@brief		Flag enabling audio output from the unit.
	@discussion	Output is enabled by default.
				Setting to YES will have no effect if canPerformOutput is false.
*/
@property (nonatomic, getter=isOutputEnabled) BOOL outputEnabled;

/*!	@property	outputProvider
	@brief		The block that the output unit will call to get audio to send to the output.
	@discussion	This block must be set if output is enabled.
*/
@property (nonatomic, copy, nullable) AURenderPullInputBlock outputProvider;

/*!	@property	inputHandler
	@brief		The block that the output unit will call to notify when input is available.
	@discussion	See discussion for AUInputHandler.
*/
@property (nonatomic, copy, nullable) AUInputHandler inputHandler;

#if !TARGET_OS_IPHONE
/*!	@property	device
	@brief		Get the I/O hardware device.
*/
@property (nonatomic, readonly) AUAudioObjectID deviceID;

/*!	@method		setDeviceID:error:
	@brief		Set the I/O hardware device.
	@param deviceID
		The device to select.
	@param outError
		Returned in the event of failure.
*/
- (BOOL)setDeviceID:(AUAudioObjectID)deviceID error:(NSError **)outError;

/*!	@property	deviceInputLatency
	@brief		The audio device's input latency, in seconds.
	@discussion
		Bridged to the HAL property kAudioDevicePropertyLatency, which is implemented 
		by v2 input/output units.
*/
@property (nonatomic, readonly) NSTimeInterval deviceInputLatency API_AVAILABLE(macos(10.13));

/*!	@property	deviceOutputLatency
	@brief		The audio device's output latency, in seconds.
	@discussion
		Bridged to the HAL property kAudioDevicePropertyLatency, which is implemented
		by v2 input/output units.
*/
@property (nonatomic, readonly) NSTimeInterval deviceOutputLatency API_AVAILABLE(macos(10.13));
#endif // TARGET_OS_IPHONE

/*!	@property	running
	@brief		The audio device's running state.
*/
@property (nonatomic, readonly, getter=isRunning) BOOL running API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

/*!	@method		startHardwareAndReturnError:
	@brief		Starts the audio hardware.
	@param outError
		Returned in the event of failure.
*/
- (BOOL)startHardwareAndReturnError:(NSError **)outError;

/*!	@method		stopHardware
	@brief		Stops the audio hardware.
*/
- (void)stopHardware;

/*!	@property	osWorkgroup
	@brief		The os_workgroup_t to which the input/output audio unit belongs.
	@discussion
		For further background, see <AudioToolbox/AudioWorkInterval.h>.

		Bridged to the v2 property kAudioOutputUnitProperty_OSWorkgroup.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) os_workgroup_t osWorkgroup
	API_AVAILABLE(macos(11.0), ios(14.0), watchos(7.0), tvos(14.0))
	__SWIFT_UNAVAILABLE_MSG("Swift is not supported for use with audio realtime threads");

@end

// =================================================================================================

/*!	@class	AUAudioUnitBusArray
	@brief	Container for an audio unit's input or output busses.
	@discussion
		Hosts can observe a bus property across all busses by using KVO on this object, without
		having to observe it on each individual bus. (One could add listeners to individual busses,
		but that means one has to observe bus count changes and add/remove listeners in response.
		Also, NSArray's addObserver:toObjectsAtIndexes:forKeyPath:options:context: is problematic;
		it does not let the individual objects override the observation request, and so a bus which
		is proxying a bus in an extension process does not get the message.)

		Some audio units (e.g. mixers) support variable numbers of busses, via subclassing. When the
		bus count changes, a KVO notification is sent on "inputBusses" or "outputBusses," as
		appropriate.

		Subclassers should see also the AUAudioUnitBusImplementation category.
		
		The bus array is bridged to the v2 property kAudioUnitProperty_ElementCount.
*/
API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
@interface AUAudioUnitBusArray : NSObject <NSFastEnumeration>

- (instancetype)init NS_UNAVAILABLE;

/*!	@method		initWithAudioUnit:busType:busses:
	@brief		Initializes by making a copy of the supplied bus array.
*/
- (instancetype)initWithAudioUnit:(AUAudioUnit *)owner busType:(AUAudioUnitBusType)busType busses:(NSArray <AUAudioUnitBus *> *)busArray NS_DESIGNATED_INITIALIZER;

/*!	@method		initWithAudioUnit:busType:
	@brief		Initializes an empty bus array.
*/
- (instancetype)initWithAudioUnit:(AUAudioUnit *)owner busType:(AUAudioUnitBusType)busType;

/*!	@property	count
*/
@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger count;

/*!	@method		objectAtIndexedSubscript:
*/
- (AUAudioUnitBus *)objectAtIndexedSubscript:(NSUInteger)index;

/*!	@property	countChangeable
	@brief		Whether the array can have a variable number of busses.
	@discussion
		The base implementation returns false.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, getter=isCountChangeable) BOOL countChangeable;

/*!	@property	setBusCount:error:
	@brief		Change the number of busses in the array.
*/
- (BOOL)setBusCount:(NSUInteger)count error:(NSError **)outError;

/*!	@method		addObserverToAllBusses:forKeyPath:options:context:
	@brief		Add a KVO observer for a property on all busses in the array.
*/
- (void)addObserverToAllBusses:(NSObject *)observer forKeyPath:(NSString *)keyPath options:(NSKeyValueObservingOptions)options context:(void * __nullable)context;

/*!	@method		removeObserverFromAllBusses:forKeyPath:context:
	@brief		Remove a KVO observer for a property on all busses in the array.
*/
- (void)removeObserverFromAllBusses:(NSObject *)observer forKeyPath:(NSString *)keyPath context:(void * __nullable)context;

/// The audio unit that owns the bus.
@property (NS_NONATOMIC_IOSONLY, readonly, assign) AUAudioUnit *ownerAudioUnit;

/// Which bus array this is (input or output).
@property (NS_NONATOMIC_IOSONLY, readonly) AUAudioUnitBusType busType;

@end

// =================================================================================================

/*!	@class	AUAudioUnitBus
	@brief	An input or output connection point on an audio unit.
*/
API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
@interface AUAudioUnitBus : NSObject

/*!	@property	format
	@brief		The audio format and channel layout of audio being transferred on the bus.
	@discussion
		Bridged to the v2 property kAudioUnitProperty_StreamFormat.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) AVAudioFormat *format;

/*!	@property	setFormat:error:
	@brief		Sets the bus's audio format.
	@discussion
		Audio units can generally be expected to support AVAudioFormat's standard format
		(deinterleaved 32-bit float), at any sample rate. Channel counts can be more complex;
		see AUAudioUnit.channelCapabilities.
*/
- (BOOL)setFormat:(AVAudioFormat *)format error:(NSError **)outError;

/*! @property	shouldAllocateBuffer
    @brief		Controls the audio unit's allocation strategy for a bus.
    @discussion
        Hosts can set this flag to communicate whether an audio unit should allocate its own buffer.
        By default this flag is set to true.

        On the output side, shouldAllocateBuffer=false means the AU can assume that it will be
        called with non-null output buffers. If shouldAllocateBuffer=true (the default), the AU must
        be prepared to be called with null pointers and replace them with pointers to its internally
        allocated buffer.

        On the input side, shouldAllocateBuffer=false means the AU can pull for input using a buffer
        list with null buffer pointers, and assume that the pull input block will provide pointers.
        If shouldAllocateBuffer=true (the default), the AU must pull with non-null pointers while
        still being prepared for the source to replace them with pointers of its own.

        Bridged to the v2 property kAudioUnitProperty_ShouldAllocateBuffer.
*/
@property (NS_NONATOMIC_IOSONLY) BOOL shouldAllocateBuffer API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

/*!	@property	enabled
	@brief		Whether the bus is active.
	@discussion
		Hosts must enable input busses before using them. The reason for this is to allow a unit
		such as a mixer to be prepared to render a large number of inputs, but avoid the work
		of preparing to pull inputs which are not in use.
		
		Bridged to the v2 properties kAudioUnitProperty_MakeConnection and
		kAudioUnitProperty_SetRenderCallback.
*/
@property (NS_NONATOMIC_IOSONLY, getter=isEnabled) BOOL enabled;

/*!	@property	name
	@brief		A name for the bus. Can be set by host.
*/
@property (NS_NONATOMIC_IOSONLY, copy, nullable) NSString *name;

/*! @property   index
    @brief      The index of this bus in the containing array.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger index;

/*! @property   busType
    @brief      The AUAudioUnitBusType.
*/
@property (NS_NONATOMIC_IOSONLY, readonly) AUAudioUnitBusType busType;

/*! @property   ownerAudioUnit
	@brief      The audio unit that owns the bus.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, assign) AUAudioUnit *ownerAudioUnit;

/*!	@property	supportedChannelLayoutTags
	@discussion
		This is an array of NSNumbers representing AudioChannelLayoutTag.
*/
@property (NS_NONATOMIC_IOSONLY, readonly, copy, nullable) NSArray<NSNumber *> *supportedChannelLayoutTags;

/*!	@property	contextPresentationLatency
	@brief		Information about latency in the audio unit's processing context.
	@discussion
		This should not be confused with the audio unit's latency property, where the audio unit
		describes to the host any processing latency it introduces between its input and its output.
		
		A host may set this property to describe to the audio unit the presentation latency of its
		input and/or output audio data. Latency is described in seconds. A value of zero means
		either no latency or an unknown latency.
		
		A host should set this property on each active bus, since, for example, the audio routing
		path to each of multiple output busses may differ.
		
		For input busses:
			Describes how long ago the audio arriving on this bus was acquired. For instance, when
			reading from a file to the first audio unit in a chain, the input presentation latency
			is zero. For audio input from a device, this initial input latency is the presentation
			latency of the device itself, i.e. the device's safety offset and latency.
			
			A second chained audio unit's input presentation latency will be the input presentation
			latency of the first unit, plus the processing latency of the first unit.
			
		For output busses:
			Describes how long it will be before the output audio of an audio unit is presented. For
			instance, when writing to a file, the output presentation latency of the last audio unit
			in a chain is zero. When the audio from that audio unit is to be played to a device,
			then that initial presentation latency will be the presentation latency of the device
			itself, which is the I/O buffer size, plus the device's safety offset and latency
			
			A previous chained audio unit's output presentation latency is the last unit's
			presentation latency plus its processing latency.
			
		So, for a given audio unit anywhere within a mixing graph, the input and output presentation 
		latencies describe to that unit how long from the moment of generation it has taken for its 
		input to arrive, and how long it will take for its output to be presented.
		
		Bridged to the v2 property kAudioUnitProperty_PresentationLatency.
*/
@property (NS_NONATOMIC_IOSONLY) NSTimeInterval contextPresentationLatency;

@end

// =================================================================================================

/*!	@class	AUAudioUnitPreset
	@brief	A collection of parameter settings provided by the audio unit implementor, producing a
			useful sound or starting point.
*/
API_AVAILABLE(macos(10.11), ios(9.0), watchos(2.0), tvos(9.0))
@interface AUAudioUnitPreset : NSObject <NSSecureCoding>

/*!	@property	number
	@brief		The preset's unique numeric identifier.
*/
@property (NS_NONATOMIC_IOSONLY) NSInteger number;

/*!	@property	name
	@brief		The preset's name.
*/
@property (NS_NONATOMIC_IOSONLY, copy) NSString *name;

@end


// =================================================================================================


/*!	@typedef	CallHostBlock
	@brief		Block that hosts provide to AU message channels to be called back by the AU.
	@param	message
		An NSDictionary with custom data. The allowed classes for key and value types are
		NSArray, NSDictionary, NSOrderedSet, NSSet, NSString, NSData, NSNull, NSNumber, NSDate
	@return
		An NSDictionary with custom data. The allowed classes for key and value types are
		NSArray, NSDictionary, NSOrderedSet, NSSet, NSString, NSData, NSNull, NSNumber, NSDate
*/
typedef NSDictionary * _Nonnull (^CallHostBlock)(NSDictionary *message);


/*!	@protocol	AUMessageChannel
	@brief		The protocol which objects returned from `[AUAudioUnit messageChannelFor:]` have to conform to.
	@discussion
		Audio Units and hosts that have special needs of communication, e.g. to exchange musical context required for better audio processing,
		can implement a communication object to exchange messages in form of NSDictionaries. An Audio Unit would need to implement
		a class conforming to the AUMessageChannel protocol and return an instance via `[AUAudioUnit messageChannelFor:]`. A host can query
		the instance via the channel name.
		The protocol offers a method to send messages to the AU and a block to send messages to the host.
*/
API_AVAILABLE(macos(13.0), ios(16.0)) __WATCHOS_PROHIBITED __TVOS_PROHIBITED
@protocol AUMessageChannel

@optional

/*!	@method		callAudioUnit:
	@brief		Calls the Audio Unit with custom data message.
	@param	message
		An NSDictionary with custom data. The allowed classes for key and value types are
		NSArray, NSDictionary, NSOrderedSet, NSSet, NSString, NSData, NSNull, NSNumber, NSDate
	@return
		An NSDictionary with custom data. The allowed classes for key and value types are
		NSArray, NSDictionary, NSOrderedSet, NSSet, NSString, NSData, NSNull, NSNumber, NSDate
*/
- (NSDictionary *)callAudioUnit:(NSDictionary *)message;

/*!	@property	sendMessageToHostBlock
	@brief		A callback for the AU to send a message to the host.
	@discussion
		The host has to set a block on this property.
*/
@property (NS_NONATOMIC_IOSONLY, copy, nullable) CallHostBlock callHostBlock;

@end

@class CASpatialAudioExperience;

API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios, watchos, tvos, macos)
@interface AUAudioUnit (IntendedSpatialExperience)

/// The AUAudioUnit's intended spatial experience.
///
/// Only useful for output AUAudioUnits - setting on a non-output AU is a
/// no-op. The default value of CAAutomaticSpatialAudio means the output AUAudioUnit
/// uses its AVAudioSession's spatial experience. See CASpatialAudioExperience for
/// more details.
@property (nonnull, copy) CASpatialAudioExperience *intendedSpatialExperience NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END

#endif // __OBJC2__
#endif // AudioToolbox_AUAudioUnit_h
#else
#include <AudioToolboxCore/AUAudioUnit.h>
#endif
