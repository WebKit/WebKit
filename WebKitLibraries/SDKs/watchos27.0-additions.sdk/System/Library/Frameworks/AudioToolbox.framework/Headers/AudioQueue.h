/*!
	@file		AudioQueue.h
	@framework	AudioToolbox.framework
	@copyright	(c) 2006-2015 by Apple, Inc., all rights reserved.
    @abstract   API's to record and play audio buffers.

	Audio queues are software objects you use for recording or playing audio. Audio
	queues perform the following tasks:

	- Connect to audio hardware
	- Manage audio data buffers
	- Employ codecs, as necessary, for compressed audio formats
	- Mediate playback or recording

	Audio queues can record and play audio in linear PCM, in compressed formats (such as Apple
	Lossless, AAC, and MP3), and in other formats for which users have installed codecs. The API set
	includes high-level support for the use of hardware recording and playback devices, and lets you
	use sophisticated codecs without knowledge of how they work.

	Additional advanced features support scheduled playback and synchronization of multiple audio
	queues and synchronization of audio with video.
*/

#ifndef AudioToolbox_AudioQueue_h
#define AudioToolbox_AudioQueue_h

#include <Availability.h>
#include <CoreAudioTypes/CoreAudioTypes.h>
#include <CoreFoundation/CoreFoundation.h>

#if defined(__BLOCKS__)
    #include <dispatch/dispatch.h>
#endif

CF_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif


#pragma mark -
#pragma mark Constants


#pragma mark -
#pragma mark Types
//==================================================================================================
//  TYPES
//==================================================================================================

/*!
    @typedef    AudioQueuePropertyID
    @abstract   A value that uniquely identifies an audio queue property.
*/
typedef UInt32      AudioQueuePropertyID;

/*!
    @typedef    AudioQueueParameterID
    @abstract   A value that uniquely identifies an audio queue parameter.
*/
typedef UInt32      AudioQueueParameterID;

/*!
    @typedef    AudioQueueParameterValue
    @abstract   A value for an audio queue parameter.
*/
typedef Float32     AudioQueueParameterValue;

/*!
    @typedef    AudioQueueRef
    @abstract   An opaque data type that represents an audio queue.
*/
typedef struct OpaqueAudioQueue *   AudioQueueRef;

/*!
    @typedef    AudioQueueTimelineRef
    @abstract   An opaque data type that represents an audio queue timeline.

	You can use this object to observe any overloads in the audio device associated with the
	audio queue. A timeline object receives notifications of discontinuities in the audio
	hardware's sample timeline--for instance, a period of silence when sound was expected.
	Causes of discontinuities include changes in the device state or processing overloads.
	See Technical Q & A: QA 1467 for a discussion of Core Audio overload warnings. These
	warnings indicate you are taking too long to process audio data and the system has cut
	you off. You query a timeline object by passing it as a parameter to
	AudioQueueGetCurrentTime, which means a discontinuity has occurred.
*/
typedef struct OpaqueAudioQueueTimeline *   AudioQueueTimelineRef;


//==================================================================================================
//  CONSTANTS
//==================================================================================================
/*!
    @enum Result Codes
    @abstract   The OSStatus result codes returned by Audio Queue functions.

    @constant   kAudioQueueErr_InvalidBuffer
    				The specified buffer does not belong to the audio queue.
    @constant   kAudioQueueErr_BufferEmpty
    				The buffer is empty (that is, the mAudioDataByteSize field = 0).
    @constant   kAudioQueueErr_DisposalPending
    				The function cannot act on the audio queue because it is being asynchronously
    				disposed of.
    @constant   kAudioQueueErr_InvalidProperty
    				The specified property ID is invalid.
    @constant   kAudioQueueErr_InvalidPropertySize
    				The size of the specified property is invalid.
    @constant   kAudioQueueErr_InvalidParameter
    				The specified parameter ID is invalid.
    @constant   kAudioQueueErr_CannotStart
    				The audio queue has encountered a problem and cannot start.
    @constant   kAudioQueueErr_InvalidDevice
    				The device assigned to the queue could not be located, or is not properly
    				configured.
    @constant   kAudioQueueErr_BufferInQueue
    				The buffer cannot be disposed of when it is enqueued.
    @constant   kAudioQueueErr_InvalidRunState
    				The queue is running but the function can only operate on the queue when it is
    				stopped, or vice versa.
    @constant   kAudioQueueErr_InvalidQueueType
    				The queue is an input queue but the function can only operate on an output
    				queue, or vice versa.
    @constant   kAudioQueueErr_Permissions
    				You do not have the required permissions to call the function
    @constant   kAudioQueueErr_InvalidPropertyValue
    				The specified property value is invalid.
    @constant   kAudioQueueErr_PrimeTimedOut
    				During Prime, the queue's AudioConverter failed to convert the requested number
    				of sample frames.
    @constant   kAudioQueueErr_CodecNotFound
    				The required audio codec was not found.
    @constant   kAudioQueueErr_InvalidCodecAccess
    				Access to the required codec is not permitted (possibly due to incompatible
    				AudioSession settings on iOS).
    @constant   kAudioQueueErr_QueueInvalidated
    				On iOS derived platforms (excluding Catalyst), the audio server has exited,
    				causing this audio queue to have become invalid.
    @constant   kAudioQueueErr_TooManyTaps
    				There can only be one processing tap per audio queue.
    @constant   kAudioQueueErr_InvalidTapContext
    				GetTapSourceAudio can only be called from the tap's callback.
    @constant   kAudioQueueErr_InvalidTapType
    				GetTapQueueTime can only be called on an output queue's tap.
    @constant   kAudioQueueErr_RecordUnderrun
    				During recording, data was lost because there was no enqueued buffer into which
    				to store it.
    @constant   kAudioQueueErr_BufferEnqueuedTwice
    				A buffer was enqueued twice on an input queue (before being returned as a result
    				of being filled or from Reset).
    @constant   kAudioQueueErr_CannotStartYet
    				Starting the audio queue failed because an internal reconfiguration (typically
    				initiated by a hardware stream format or sample rate change) was in progress.
    				Sleeping briefly and retrying is recommended.
    @constant   kAudioQueueErr_EnqueueDuringReset
    				During Reset, Stop, or Dispose, it is not permitted to enqueue buffers.
    @constant   kAudioQueueErr_InvalidOfflineMode
    				The operation requires the queue to be in offline mode but it isn't, or vice
    				versa. (Offline mode is entered and exited via
    				AudioQueueSetOfflineRenderFormat).
*/
CF_ENUM(OSStatus) {
    kAudioQueueErr_InvalidBuffer        = -66687,
    kAudioQueueErr_BufferEmpty          = -66686,
    kAudioQueueErr_DisposalPending      = -66685,
    kAudioQueueErr_InvalidProperty      = -66684,
    kAudioQueueErr_InvalidPropertySize  = -66683,
    kAudioQueueErr_InvalidParameter     = -66682,
    kAudioQueueErr_CannotStart          = -66681,
    kAudioQueueErr_InvalidDevice        = -66680,
    kAudioQueueErr_BufferInQueue        = -66679,
    kAudioQueueErr_InvalidRunState      = -66678,
    kAudioQueueErr_InvalidQueueType     = -66677,
    kAudioQueueErr_Permissions          = -66676,
    kAudioQueueErr_InvalidPropertyValue = -66675,
    kAudioQueueErr_PrimeTimedOut        = -66674,
    kAudioQueueErr_CodecNotFound        = -66673,
    kAudioQueueErr_InvalidCodecAccess   = -66672,
    kAudioQueueErr_QueueInvalidated     = -66671,
    kAudioQueueErr_TooManyTaps          = -66670,
    kAudioQueueErr_InvalidTapContext    = -66669,
    kAudioQueueErr_RecordUnderrun       = -66668,
    kAudioQueueErr_InvalidTapType       = -66667,
    kAudioQueueErr_BufferEnqueuedTwice  = -66666,
    kAudioQueueErr_CannotStartYet       = -66665,
    kAudioQueueErr_EnqueueDuringReset   = -66632,
    kAudioQueueErr_InvalidOfflineMode   = -66626,
};


/*!
    @enum Audio Queue Property IDs
    @abstract   Constants that identify properties for audio queues.

    @constant   kAudioQueueProperty_IsRunning
        A read-only property whose value is a UInt32 that indicates whether or not the queue is
        running. A notification is sent when the audio device starts or stops, which is not
        necessarily when the start or stop function is called.
    @constant   kAudioQueueDeviceProperty_SampleRate
        A read-only property whose value is a Float64 that indicates the sampling rate of the
        associated audio device.
    @constant   kAudioQueueDeviceProperty_NumberChannels
        A read-only property whose value is a UInt32 that indicates the number of channels in
        the associated audio device.
    @constant   kAudioQueueProperty_CurrentDevice
        A read/write property whose value is a CFStringRef that contains the unique identifier
        (UID) of the associated audio device.
		If the audio queue is tracking the default system device and the device changes, it will
		generate a property changed notification for this property. You can then query the HAL 
		for info on the new default system device.
    @constant   kAudioQueueProperty_MagicCookie
        A read/write property whose value is an audio format magic cookie. If the audio format
        requires a magic cookie, you must set this property before enqueuing any buffers.
    @constant   kAudioQueueProperty_MaximumOutputPacketSize
        A read-only UInt32 that indicates the size in bytes of the largest single packet of
        data in the output format. This is mostly useful for recording/encoding variable bit rate
        compressed data.
    @constant   kAudioQueueProperty_StreamDescription
        A read-only AudioStreamBasicDescription that indicates the queue's recording format.
        This is useful when recording, where you may specify a sample rate of 0 during
        construction, 
    @constant   kAudioQueueProperty_ChannelLayout
        A read/write property whose value is an audio channel layout structure that describes
        the audio queue's channel layout. The number of channels must match the format of the
        queue. If more than two channels (for instance, a five-channel surround sound) exist in
        the queue, there may be a need to specify a channel layout. This layout indicates the
        specific order in which the channels do appear, such as left, then center, then right.
    @constant   kAudioQueueProperty_EnableLevelMetering
        A read-write property whose value is a UInt32 that indicates whether metering of
        audio levels is enabled for the audio queue. (0=off, 1=on).
    @constant   kAudioQueueProperty_CurrentLevelMeter
        A read-only property whose value is an array of AudioQueueLevelMeter structures, one
        array element per audio channel. The values in the AudioQueueLevelMeters are in the
        range 0-1.
    @constant   kAudioQueueProperty_CurrentLevelMeterDB
        A read-only property whose value is an array of AudioQueueLevelMeter structures, one
        array element per audio channel. The values in the AudioQueueLevelMeters are in
        decibels.
    @constant   kAudioQueueProperty_DecodeBufferSizeFrames
        A read/write property whose value is a UInt32 that is the size of the buffer into which
        an output audio queue decodes buffers. A large buffer provides more reliability and
        better long-term performance at the expense of memory and decreased responsiveness
        in some situations.
    @constant   kAudioQueueProperty_ConverterError
        A read-only property whose value is a UInt32 indicating the most recent error (if any)
        encountered by the queue's internal encoding/decoding process.
    @constant   kAudioQueueProperty_EnableTimePitch
        A read/write property whose value is a UInt32 describing whether there is a time/pitch unit
        inserted into the queue's audio signal chain. This property may only be set while
        the queue is stopped.
    @constant   kAudioQueueProperty_TimePitchAlgorithm
        A read/write property whose value is a UInt32 describing the time/pitch algorithm in use.
        This property is only valid while a time/pitch has been inserted, and may only be changed
        when the queue is not running.
    @constant   kAudioQueueProperty_TimePitchBypass
        A read/write property whose value is a UInt32 describing whether the time/pitch unit
        has been bypassed (1=bypassed, 0=not bypassed).
    @constant   kAudioQueueProperty_IntendedSpatialExperience
        A read/write property whose value is an CASpatialAudioExperience describing this specific
        AudioQueue's intended spatial experience. Only useful for output AudioQueue's not configured
        in offline mode. Setting this property on an input AudioQueue or an offline AudioQueue is
        a no-op. The default value of CAAutomaticSpatialAudio value means the AudioQueue uses its
        AVAudioSession's intended spatial experience. See CASpatialAudioExperience for more details.
*/
CF_ENUM(AudioQueuePropertyID) {
    kAudioQueueProperty_IsRunning               = 'aqrn',       //!< value is UInt32

    kAudioQueueDeviceProperty_SampleRate        = 'aqsr',       // value is Float64
    kAudioQueueDeviceProperty_NumberChannels    = 'aqdc',       // value is UInt32
    kAudioQueueProperty_CurrentDevice           = 'aqcd',       // value is CFStringRef
    
    kAudioQueueProperty_MagicCookie             = 'aqmc',       // value is void*
    kAudioQueueProperty_MaximumOutputPacketSize = 'xops',       // value is UInt32
    kAudioQueueProperty_StreamDescription       = 'aqft',       // value is AudioStreamBasicDescription
       
    kAudioQueueProperty_ChannelLayout           = 'aqcl',       // value is AudioChannelLayout
    kAudioQueueProperty_EnableLevelMetering     = 'aqme',       // value is UInt32
    kAudioQueueProperty_CurrentLevelMeter       = 'aqmv',       // value is array of AudioQueueLevelMeterState, 1 per channel
    kAudioQueueProperty_CurrentLevelMeterDB     = 'aqmd',       // value is array of AudioQueueLevelMeterState, 1 per channel

    kAudioQueueProperty_DecodeBufferSizeFrames  = 'dcbf',       // value is UInt32
    kAudioQueueProperty_ConverterError          = 'qcve',       // value is UInt32

    kAudioQueueProperty_EnableTimePitch         = 'q_tp',       // value is UInt32, 0/1
    kAudioQueueProperty_TimePitchAlgorithm      = 'qtpa',       // value is UInt32. See values below.
    kAudioQueueProperty_TimePitchBypass         = 'qtpb',       // value is UInt32, 1=bypassed
    
    kAudioQueueProperty_IntendedSpatialExperience API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios, watchos, tvos, macos) CF_REFINED_FOR_SWIFT = 'iseo', // value is CASpatialAudioExperience*
};

/*!
    @enum       Time/Pitch algorithms
    @abstract   Constants that identify values of kAudioQueueProperty_TimePitchAlgorithm

    @constant kAudioQueueTimePitchAlgorithm_Spectral
        Highest quality, most computationally expensive. Suitable for music.
        Default algorithm on macOS.
    @constant kAudioQueueTimePitchAlgorithm_TimeDomain
        Modest quality, less expensive. Suitable for voice.
    @constant kAudioQueueTimePitchAlgorithm_Varispeed
        High quality, but pitch varies with rate.
*/
CF_ENUM(UInt32) {
    kAudioQueueTimePitchAlgorithm_Spectral      = 'spec',
    kAudioQueueTimePitchAlgorithm_TimeDomain    = 'tido',
    kAudioQueueTimePitchAlgorithm_Varispeed     = 'vspd'
};

#if TARGET_OS_IPHONE && !TARGET_OS_MACCATALYST
/*!	@enum		Time/Pitch algorithms (iOS only)
    @constant kAudioQueueTimePitchAlgorithm_LowQualityZeroLatency
        Low quality, very inexpensive. Suitable for brief fast-forward/rewind effects,
        low quality voice. Default algorithm on iOS.
*/
CF_ENUM(UInt32) {
    kAudioQueueTimePitchAlgorithm_LowQualityZeroLatency API_DEPRECATED("Low Quality Zero Latency algorithm is now Deprecated. Please choose from other available options", ios(2.0, 13.0), watchos(2.0, 6.0), tvos(9.0, 13.0)) API_UNAVAILABLE(macos)     = 'lqzl',
};
#endif

#if TARGET_OS_IPHONE
/*!
    @enum Audio Queue Property IDs
    @abstract Audio Queue Property IDs (iOS 3.0 or greater only)
    
    @constant   kAudioQueueProperty_HardwareCodecPolicy
        A UInt32 describing how the audio queue is to choose between a hardware or
        software version of the codec required for its audio format. Its value is one of
        the AudioQueueHardwareCodecPolicy constants below.
        
        If the chosen codec is not available, or if a hardware codec is chosen and the 
        AudioSession category does not permit use of hardware codecs, attempts to Prime or Start
        the queue will fail.

        Use kAudioFormatProperty_Encoders or kAudioFormatProperty_Decoders to determine
        whether the codec you are interested in using is available in hardware form,
        software, or both.
        
        Changing this property is not permitted while the queue is primed or running. Changing
        this property at other times may cause any properties set on the codec to be lost.
 
        DEPRECATED: Hardware codecs are no longer supported.
*/
CF_ENUM(AudioQueuePropertyID) {
    kAudioQueueProperty_HardwareCodecPolicy             = 'aqcp'        // value is UInt32
};

/*!
    @enum       AudioQueueHardwareCodecPolicy constants
    @abstract   Values of kAudioQueueProperty_HardwareCodecPolicy (iOS 3.0 or greater only)
    
    @constant kAudioQueueHardwareCodecPolicy_Default
        If the required codec is available in both hardware and software forms, the audio queue
        will choose a hardware codec if its AudioSession category permits, software otherwise.
        If the required codec is only available in one form, that codec is chosen.
    @constant kAudioQueueHardwareCodecPolicy_UseSoftwareOnly
        The audio queue will choose a software codec if one is available.
    @constant kAudioQueueHardwareCodecPolicy_UseHardwareOnly
        The audio queue will choose a hardware codec if one is available and its use permitted
        by the AudioSession category.
    @constant kAudioQueueHardwareCodecPolicy_PreferSoftware
        The audio queue will choose a software codec if one is available; if not, it will choose a
        hardware codec if one is available and its use permitted by the AudioSession category.
    @constant kAudioQueueHardwareCodecPolicy_PreferHardware
        The audio queue will choose a hardware codec if one is available and its use permitted
        by the AudioSession category; otherwise, it will choose a software codec if one is available.

	DEPRECATED: Hardware codecs are no longer supported.
*/
CF_ENUM(UInt32) {
    kAudioQueueHardwareCodecPolicy_Default              = 0,
    kAudioQueueHardwareCodecPolicy_UseSoftwareOnly      = 1,
    kAudioQueueHardwareCodecPolicy_UseHardwareOnly      = 2,
    kAudioQueueHardwareCodecPolicy_PreferSoftware       = 3,
    kAudioQueueHardwareCodecPolicy_PreferHardware       = 4
};
#endif // TARGET_OS_IPHONE

/*!
    @enum Audio Queue Property IDs
    @abstract Audio Queue Property IDs (iOS 6.0 or greater only)

    @constant   kAudioQueueProperty_ChannelAssignments
        A write-only property whose value is an array of AudioQueueChannelAssignment. There must be
        one array element for each channel of the queue's format as specified in the
        AudioStreamBasicDescription passed to AudioQueueNewOutput or AudioQueueNewInput.
        (New in iOS 6.0).
*/
CF_ENUM(AudioQueuePropertyID) {
    kAudioQueueProperty_ChannelAssignments      = 'aqca'
} API_AVAILABLE(ios(9.0), watchos(2.0), tvos(9.0), macos(10.15));

/*!
    @enum       AudioQueue parameter IDs
    @abstract   Constants that identify the parameters for audio queues.

	You can set a parameter in one of two ways:
	
	- Assign a parameter value to an audio queue to take effect immediately using
			AudioQueueSetParameter.
	- Schedule a parameter to take effect when a buffer is enqueued. You supply the
			parameter when you queue a buffer, and the new value of the parameter is applied
			when that buffer is rendered.
	
	AudioQueueGetParameter always returns the current value of the parameter

    @constant   kAudioQueueParam_Volume
        A value from 0.0 to 1.0 indicating the linearly scaled gain for the queue. A value of
        1.0 (the default) indicates unity gain. A value of 0.0 indicates zero gain, or silence.
    @constant   kAudioQueueParam_PlayRate
        A value from 0.5 to 2.0 indicating the rate at which the queue is to play. A value of
        1.0 (the default) indicates that the queue should play at its normal rate. Only
        applicable when the time/pitch processor has been enabled and on macOS 10.6 and higher.
    @constant   kAudioQueueParam_Pitch
        A value from -2400 to 2400 indicating the number of cents to pitch-shift the queue's
        playback. (1200 cents is one octave.) Only applicable when the time/pitch processor has 
        been enabled with the spectral algorithm, and on macOS 10.6 and higher.
    @constant   kAudioQueueParam_VolumeRampTime
        A value indicating the number of seconds over which subsequent volume changes will be
        ramped. For example, to fade out from full unity gain to silence over the course of 1
        second, set kAudioQueueParam_VolumeRampTime to 1 then kAudioQueueParam_Volume to 0.
    @constant   kAudioQueueParam_Pan
        A value from -1 to 1 indicating the pan position of a mono source (-1 = hard left, 0 =
        center, 1 = hard right). For a stereo source this parameter affects left/right balance.
        For multi-channel sources, this parameter has no effect.
*/
CF_ENUM(AudioQueueParameterID)
{
    kAudioQueueParam_Volume         = 1,
    kAudioQueueParam_PlayRate       = 2,
    kAudioQueueParam_Pitch          = 3,
    kAudioQueueParam_VolumeRampTime = 4,
    kAudioQueueParam_Pan            = 13
};

/*!
    @enum       AudioQueueProcessingTapFlags
    @abstract   Flags used in conjunction with processing taps

	In the flags passed to AudioQueueProcessingTapNew, either the PreEffects
	or PostEffects flag must be set, but not both. 

    @constant   kAudioQueueProcessingTap_PreEffects
        Signifies that the processing tap is inserted before any effects.
        Passed to AudioQueueProcessingTapNew and to the callback.
    @constant   kAudioQueueProcessingTap_PostEffects
        Signifies that the processing tap is inserted after any effects.
        Passed to AudioQueueProcessingTapNew and to the callback.
    @constant   kAudioQueueProcessingTap_Siphon
        Signifies that the processing tap is a siphon; it does not call
        GetSourceAudio. The callback instead receives the source audio
        and may not modify it. Passed to AudioQueueProcessingTapNew and to the callback.
    @constant   kAudioQueueProcessingTap_StartOfStream
        Signifies that the source audio is the beginning of a continuous stream,
        i.e. following the beginning or resumption of playback or recording.
        Returned from GetSourceAudio.
    @constant   kAudioQueueProcessingTap_EndOfStream
        Signifies that the source audio is past the end of stream. This happens when
        the audio queue is being stopped asynchronously and has finished playing
        all of its data. Returned from GetSourceAudio and should be propagated
        on return from the callback.
*/
typedef CF_OPTIONS(UInt32, AudioQueueProcessingTapFlags) {
    //  these are flags that are passed to both the constructor and the callback
    kAudioQueueProcessingTap_PreEffects         = (1 << 0),     // 0x01
    kAudioQueueProcessingTap_PostEffects        = (1 << 1),     // 0x02
    kAudioQueueProcessingTap_Siphon             = (1 << 2),     // 0x04

    //  these are flags that are passed to the callback and from GetSourceAudio
    kAudioQueueProcessingTap_StartOfStream      = (1 << 8),     // 0x100
    kAudioQueueProcessingTap_EndOfStream        = (1 << 9),     // 0x200
};

#pragma mark -
#pragma mark Structs
//==================================================================================================
//  STRUCTS
//==================================================================================================

/*!
    @struct     AudioQueueBuffer
    @abstract   Defines a buffer of audio data to be managed by an audio queue.

	Each audio queue has an associated set of audio queue buffers. You can request that a
	queue allocate buffers using the AudioQueueAllocateBuffer function and dispose of them
	using the AudioQueueFreeBuffer function.
	
	You may also use AudioQueueAllocateBufferWithPacketDescriptions to allocate buffers
	with space for AudioPacketDescriptions, as used in VBR formats. The 
	mPacketDescriptionCapacity, mmPacketDescriptions, and mPacketDescriptionCount
	fields may only be used with buffers allocated with this function.
	
    @var        mAudioDataBytesCapacity
        The size of the buffer, in bytes. This size is set when the buffer is allocated and
        cannot be changed.
    @var        mAudioData
       A pointer to the audio data in the buffer. Although you can write data to this buffer,
       you cannot make it point to another address.
    @var        mAudioDataByteSize
        The number of bytes of valid audio data in the buffer. You set this value when providing
        data for playback; the audio queue sets this value when recording data from a recording
        queue.
    @var        mUserData
        A value you may specify to identify the buffer when it is passed back in recording or
        playback callback functions.
    @var        mPacketDescriptionCapacity
        The maximum number of packet descriptions that can be stored in mPacketDescriptions.
    @var        mPacketDescriptions
        An array of AudioStreamPacketDescriptions associated with the buffer.
    @var        mPacketDescriptionCount
        The number of valid packet descriptions in the buffer. You set this value when providing
        buffers for playback; the audio queue sets this value when returning buffers from
        a recording queue.
*/

typedef struct AudioQueueBuffer {
    const UInt32                    mAudioDataBytesCapacity;
    void * const                    mAudioData;
    UInt32                          mAudioDataByteSize;
    void * __nullable               mUserData;

    const UInt32                    mPacketDescriptionCapacity;
    AudioStreamPacketDescription * const __nullable mPacketDescriptions;
    UInt32                          mPacketDescriptionCount;
#ifdef __cplusplus
    AudioQueueBuffer() : mAudioDataBytesCapacity(0), mAudioData(0), mPacketDescriptionCapacity(0), mPacketDescriptions(0) { }

	AudioQueueBuffer(void* dataPtr, UInt32 dataCapacity, AudioStreamPacketDescription* packetDescs,
		UInt32 packetDescCapacity)
		: mAudioDataBytesCapacity(dataCapacity),
		  mAudioData(dataPtr),
		  mAudioDataByteSize(0),
		  mUserData(nullptr),
		  mPacketDescriptionCapacity(packetDescCapacity),
		  mPacketDescriptions(packetDescs),
		  mPacketDescriptionCount(0)
	{
	}
#endif
} AudioQueueBuffer;

/*!
    @typedef    AudioQueueBufferRef
    @abstract   An pointer to an AudioQueueBuffer.
*/
typedef AudioQueueBuffer *AudioQueueBufferRef;

/*!
    @struct     AudioQueueParameterEvent
    @abstract   Specifies a value for an audio queue parameter.

	Two ways are available to supply an audio queue with parameters:

	- Provide one or more parameters by calling the AudioQueueEnqueueBufferWithParameters
	function. In this case, the parameters are applied to the specified buffer when it is
	played.

	- Assign a parameter value immediately to an audio queue by calling the
	AudioQueueSetParameter function.
	
	Note that the AudioQueueGetParameter function always returns the actual value of the
	parameter.

	In macOS v10.5, audio queues have one parameter available: kAudioQueueParam_Volume,
	which controls the queue's playback volume.
	
    @var        mID
        The parameter.
    @var        mValue
        The value of the specified parameter.
*/
struct AudioQueueParameterEvent {
    AudioQueueParameterID           mID;
    AudioQueueParameterValue        mValue;
};
typedef struct AudioQueueParameterEvent AudioQueueParameterEvent;


/*!
    @struct     AudioQueueLevelMeterState
    @abstract   Specifies the current level metering information for one channel of an audio queue.
    @var        mAveragePower
        The audio channel's average RMS power.
    @var        mPeakPower
        The audio channel's peak RMS power
*/
typedef struct AudioQueueLevelMeterState {
    Float32     mAveragePower;
    Float32     mPeakPower;
} AudioQueueLevelMeterState;

/*!
    @typedef    AudioQueueProcessingTapRef
    @abstract   An object for intercepting and processing audio within an audio queue.
*/
typedef struct OpaqueAudioQueueProcessingTap *   AudioQueueProcessingTapRef;

/*!
    @struct     AudioQueueChannelAssignment
    @abstract   Specifies an audio device channel to which the queue will play or from which
                it will record.
    @var        mDeviceUID
        On iOS, this is a port UID obtained from AVAudioSession. On macOS, this is the UID
        obtained from an AudioDeviceID.
    @var        mChannelNumber
        The 1-based index of the channel.
*/
typedef struct AudioQueueChannelAssignment {
    CFStringRef     mDeviceUID;
    UInt32          mChannelNumber;
} AudioQueueChannelAssignment;

#pragma mark -
#pragma mark Callbacks
//==================================================================================================
//  CALLBACKS
//==================================================================================================

#if defined(__BLOCKS__)
/*!
    @typedef    AudioQueueOutputCallbackBlock
    @abstract   Defines a pointer to a block that is called when a playback audio
                queue has finished taking data from a buffer.

	A playback buffer callback is invoked when the audio queue has finished with the data to
	be played and the buffer is available to your application for reuse. Your application
	might want to immediately refill and re-enqueue the completed buffer at this time.

    @param      inAQ
        The audio queue that invoked the callback.
    @param      inBuffer
        The audio queue buffer made available by the audio queue.
*/
typedef void (^AudioQueueOutputCallbackBlock)(
                                    AudioQueueRef                   inAQ,
                                    AudioQueueBufferRef             inBuffer);

/*!
    @typedef    AudioQueueInputCallbackBlock
    @abstract   Defines a pointer to a block that is called when a recording audio
                queue has finished filling a buffer.

	You specify a recording buffer callback when calling AudioQueueNewInput. Your callback
	is invoked each time the recording audio queue has filled a buffer with input data.
	Typically, your callback should write the audio queue buffer's data to a file or other
	buffer, and then re-queue the audio queue buffer to receive more data.
        
    @param      inAQ
        The audio queue that invoked the callback.
    @param      inBuffer
        An audio queue buffer, newly filled by the audio queue, containing the new audio data
        your callback needs to write.
    @param      inStartTime
        A pointer to an audio time stamp structure corresponding to the first sample contained
        in the buffer. This contains the sample time of the first sample in the buffer.
    @param      inNumberPacketDescriptions
        The number of audio packets contained in the data provided to the callback
    @param      inPacketDescs
        For compressed formats which require packet descriptions, the packet descriptions
        produced by the encoder for the incoming buffer.
*/
typedef void (^AudioQueueInputCallbackBlock)(
                                    AudioQueueRef                   inAQ,
                                    AudioQueueBufferRef             inBuffer,
                                    const AudioTimeStamp *          inStartTime,
                                    UInt32                          inNumberPacketDescriptions,
                                    const AudioStreamPacketDescription * __nullable inPacketDescs);
#endif // defined(__BLOCKS__)

/*!
    @typedef    AudioQueueOutputCallback
    @abstract   Defines a pointer to a callback function that is called when a playback audio
                queue has finished taking data from a buffer.

	A playback buffer callback is invoked when the audio queue has finished with the data to
	be played and the buffer is available to your application for reuse. Your application
	might want to immediately refill and re-enqueue the completed buffer at this time.

    @param      inUserData
        The value specified by the inUserData parameter of the AudioQueueNewOutput function.
    @param      inAQ
        The audio queue that invoked the callback.
    @param      inBuffer
        The audio queue buffer made available by the audio queue.
*/
typedef void (*AudioQueueOutputCallback)(
                                    void * __nullable       inUserData,
                                    AudioQueueRef           inAQ,
                                    AudioQueueBufferRef     inBuffer);

/*!
    @typedef    AudioQueueInputCallback
    @abstract   Defines a pointer to a callback function that is called when a recording audio
                queue has finished filling a buffer.

	You specify a recording buffer callback when calling AudioQueueNewInput. Your callback
	is invoked each time the recording audio queue has filled a buffer with input data.
	Typically, your callback should write the audio queue buffer's data to a file or other
	buffer, and then re-queue the audio queue buffer to receive more data.
        
    @param      inUserData
        The value you've specified in the inUserData parameter of the AudioQueueNewInput
        function.
    @param      inAQ
        The audio queue that invoked the callback.
    @param      inBuffer
        An audio queue buffer, newly filled by the audio queue, containing the new audio data
        your callback needs to write.
    @param      inStartTime
        A pointer to an audio time stamp structure corresponding to the first sample contained
        in the buffer. This contains the sample time of the first sample in the buffer.
    @param      inNumberPacketDescriptions
        The number of audio packets contained in the data provided to the callback
    @param      inPacketDescs
        For compressed formats which require packet descriptions, the packet descriptions
        produced by the encoder for the incoming buffer.
*/
typedef void (*AudioQueueInputCallback)(
                                    void * __nullable               inUserData,
                                    AudioQueueRef                   inAQ,
                                    AudioQueueBufferRef             inBuffer,
                                    const AudioTimeStamp *          inStartTime,
                                    UInt32                          inNumberPacketDescriptions,
                                    const AudioStreamPacketDescription * __nullable inPacketDescs);


/*!
    @typedef    AudioQueuePropertyListenerProc
    @abstract   Defines a pointer to a callback function that is called when a specified
                property changes value.

	You assign a property listener callback when calling AudioQueueAddPropertyListener.
        
    @param      inUserData
        A pointer to the data specified by the inUserData parameter of the
        AudioQueueAddPropertyListener function.
    @param      inAQ
        The audio queue that invoked the callback.
    @param      inID
        The ID of the property that invoked the callback.
*/
typedef void (*AudioQueuePropertyListenerProc)(  
                                    void * __nullable       inUserData,
                                    AudioQueueRef           inAQ,
                                    AudioQueuePropertyID    inID);

/*!
	@typedef    AudioQueueProcessingTapCallback
	@abstract   A function called when an audio queue has data to be processed by its tap

    A processing callback is invoked when the audio queue has data that can be processed by a given
    tap.

    The audio queue will call the processing callback when it has sufficient data to provide for
    processing.

    In the case of a siphoning tap, the callback function can inspect the audio data in ioData, but
    should not otherwise modify it. The callback should not call
    AudioQueueProcessingTapGetSourceAudio.

    A non-siphoning callback should call AudioQueueProcessingTapGetSourceAudio to request from the
    queue as much source data as it needs in order to produce the requested number of output
    samples. When the callback requests source data it may receive less data than it requests.

    In the case of a tap on an audio output queue, the tap must emit the exact number of sample
    frames that the queue requests. In normal circumstances, the tap's requests for source data will
    be satisfied (as the client running the audio queue is also providing the queue with the audio
    source material). If there is insufficient source data available (this is indicated by the
    outNumberFrames from the GetSource call), then the processing tap should deal as best as it can;
    it can either return less data than was requested or insert silence, noise, etc. itself. If it
    returns less data than requested, the hosting audio queue will fill in the remainder with
    silence.

    In the case of a tap on an audio input queue, the tap may provide back less audio data than is
    being requested. Typically this will occur because the tap will ask for source data that is not
    available at this time (the audio input hasn't arrived yet), so the tap should cache the source
    data that it needs and return as many processed samples as it can. If the processing tap falls
    behind and is not providing data quickly enough silence will be generated in the data provided
    to the client (and there is no signal about this either).

    A processing tap executes in a semi-real-time context, so the general limitations for real-time
    processing apply. Avoid using API's which may block. In particular, it is not safe to call the
    audio queue on which the tap was installed, with the exceptions of
    AudioQueueProcessingTapGetSourceAudio and AudioQueueProcessingTapGetQueueTime.

    In normal operation the source data will be continuous from the last time the callback was
    called and the processed samples should be continuous from the previous samples returned. If
    there is any discontinuity between the last samples provided for processing the audio queue will
    set the bit for kAudioQueueProcessing_StartOfStream in the inFlags. After a discontinuity the
    first sample that the processing tap outputs should correspond to the first sample that was
    provided in the source samples (so a reset and then consequent process serves to re-anchor a
    relationship between the processing tap's source and processed samples). In this case the
    processing tap will typically discard any previous state (for instance, if a processing tap was
    adding a reverb to a signal, then the discontinuity flag would act the same as AudioUnitReset;
    any previous source information in the processing tap should be discarded).

    The caller is responsible for absorbing any processing delays. For example, if the processing is
    to be done by an audio unit that reports a processing latency, then the caller should remove
    those latency samples from the audio unit's rendering and not return them to the audio queue.

    The processing tap is able to operate on the provided source data in place (that is, it can do
    "in place processing") and return pointers to that buffer rather than its own. This works in a
    similar way as AudioUnit render operations.

    When an output audio queue is being stopped asynchronously, the processing tap will see the
    kAudioQueueProcessingTap_EndOfStream bit set on return from GetSourceAudio, and is responsible
    for propagating this bit from the callback when its processing has reached this point.

    A processing tap will NEVER see the same source data again, so, it should keep its own copy if
    it needs to keep it for further reference past the duration of this call. It also cannot assume
    that the pointers to the source data that it retrieves will remain valid AFTER the processing
    tap has executed.

    The processing tap should ensure that the data pointers it provides in outProcessedData remain
    valid until the tap is executed again.

    A processing tap is destroyed implicitly when its audio queue is disposed. It may also be
    removed explicitly, via AudioQueueProcessingTapDispose.

    @param      inClientData
                    the client data pointer passed to AudioQueueProcessingTapNew
    @param      inAQTap
                    The tap for this callback.
    @param      inNumberFrames
                    The requested number of sample frames to be rendered.
    @param      ioFlags
                    On entry, the flags passed at construction time are provided. On exit,
                    the start/end of stream flags should be set when appropriate.
    @param      ioTimeStamp
                    On an input audio queue, the timestamp must be returned from this function.
                    On an output audio queue, the callback is provided a continuous timestamp.
    @param      outNumberFrames
                    The number of frames of audio data provided in the processed data. Can be 0.
    @param      ioData
                    For non-siphoning taps, on entry, the buffer pointers are null and the lengths
                    are zero. On exit, they should contain the tap's output.
                    
                    Siphoning taps receive valid buffers which they must not alter.
*/
typedef void (*AudioQueueProcessingTapCallback)(
                                    void *                          inClientData,
                                    AudioQueueProcessingTapRef      inAQTap,
                                    UInt32                          inNumberFrames,
                                    AudioTimeStamp *                ioTimeStamp,
                                    AudioQueueProcessingTapFlags *  ioFlags,
                                    UInt32 *                        outNumberFrames,
                                    AudioBufferList *               ioData);

//==================================================================================================
//  FUNCTIONS
//==================================================================================================

#pragma mark -
#pragma mark Creating/destroying queues
//=============================================================================
//  Creating/destroying queues
//=============================================================================

/*!
    @function   AudioQueueNewOutput
    @abstract   Creates a new audio queue for playing audio data.

	To create an playback audio queue, you allocate buffers, then queue buffers (using
	AudioQueueEnqueueBuffer). The callback receives buffers and typically queues them again.
	To schedule a buffer for playback, providing parameter and start time information, call
	AudioQueueEnqueueBufferWithParameters.
       
    @param      inFormat
        A pointer to a structure describing the format of the audio data to be played. For
        linear PCM, only interleaved formats are supported. Compressed formats are supported.
    @param      inCallbackProc
        A pointer to a callback function to be called when the audio queue has finished playing
        a buffer.
    @param      inUserData
        A value or pointer to data that you specify to be passed to the callback function.
    @param      inCallbackRunLoop
        The event loop on which inCallbackProc is to be called. If you specify NULL, the
        callback is called on one of the audio queue's internal threads.
    @param      inCallbackRunLoopMode
        The run loop mode in which to call the callback. Typically, you pass
        kCFRunLoopCommonModes. (NULL also specifies kCFRunLoopCommonModes). Other
        possibilities are implementation specific. You can choose to create your own thread with
        your own run loops. For more information on run loops, see Run Loops or CFRunLoop
        Reference.
    @param      inFlags
        Reserved for future use. Pass 0.
    @param      outAQ
        On return, this variable contains a pointer to the newly created playback audio queue
        object.
    @result     An OSStatus result code.
*/
extern OSStatus             
AudioQueueNewOutput(                const AudioStreamBasicDescription *inFormat,
                                    AudioQueueOutputCallback        inCallbackProc,
                                    void * __nullable               inUserData,
                                    CFRunLoopRef __nullable         inCallbackRunLoop,
                                    CFStringRef __nullable          inCallbackRunLoopMode,
                                    UInt32                          inFlags,
                                    AudioQueueRef __nullable * __nonnull outAQ)          API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));


/*!
    @function   AudioQueueNewInput
    @abstract   Creates a new audio queue for recording audio data.

	Outline of how to use the queue for input:
	
	- create input queue
	- allocate buffers
	- enqueue buffers (AudioQueueEnqueueBuffer, not with parameters, no packet descriptions)
	- the callback receives buffers and re-enqueues them
	
    @param      inFormat
        A pointer to a structure describing the format of the audio data to be recorded. For
        linear PCM, only interleaved formats are supported. Compressed formats are supported.
    @param      inCallbackProc
        A pointer to a callback function to be called when the audio queue has finished filling
        a buffer.
    @param      inUserData
        A value or pointer to data that you specify to be passed to the callback function.
    @param      inCallbackRunLoop
        The event loop on which inCallbackProc is to be called. If you specify NULL, the
        callback is called on one of the audio queue's internal threads.
    @param      inCallbackRunLoopMode
        The run loop mode in which to call the callback. Typically, you pass
        kCFRunLoopCommonModes. (NULL also specifies kCFRunLoopCommonModes). Other
        possibilities are implementation specific. You can choose to create your own thread with
        your own run loops. For more information on run loops, see Run Loops or CFRunLoop
        Reference.
    @param      inFlags
        Reserved for future use. Pass 0.
    @param      outAQ
        On return, this variable contains a pointer to the newly created recording audio queue
        object.
    @result     An OSStatus result code.
*/
extern OSStatus             
AudioQueueNewInput(                 const AudioStreamBasicDescription *inFormat,
                                    AudioQueueInputCallback         inCallbackProc,
                                    void * __nullable               inUserData,
                                    CFRunLoopRef __nullable         inCallbackRunLoop,
                                    CFStringRef __nullable          inCallbackRunLoopMode,
                                    UInt32                          inFlags,
                                    AudioQueueRef __nullable * __nonnull outAQ)          API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

#if defined(__BLOCKS__)
/*!
    @function   AudioQueueNewOutputWithDispatchQueue
    @abstract   Creates a new audio queue for playing audio data.

	To create an playback audio queue, you allocate buffers, then queue buffers (using
	AudioQueueEnqueueBuffer). The callback receives buffers and typically queues them again.
	To schedule a buffer for playback, providing parameter and start time information, call
	AudioQueueEnqueueBufferWithParameters.
       
    @param      outAQ
        On return, this variable contains a pointer to the newly created playback audio queue
        object.
    @param      inFormat
        A pointer to a structure describing the format of the audio data to be played. For
        linear PCM, only interleaved formats are supported. Compressed formats are supported.
    @param      inFlags
        Reserved for future use. Pass 0.
    @param      inCallbackDispatchQueue
        The dispatch queue from which inCallbackBlock is to be called.
    @param      inCallbackBlock
        A pointer to a callback block to be called when the audio queue has finished playing
        a buffer.
    @result     An OSStatus result code.
*/
extern OSStatus             
AudioQueueNewOutputWithDispatchQueue(AudioQueueRef __nullable * __nonnull outAQ,
                                    const AudioStreamBasicDescription *inFormat,
                                    UInt32                          inFlags,
                                    dispatch_queue_t                inCallbackDispatchQueue,
                                    AudioQueueOutputCallbackBlock   inCallbackBlock)
										API_AVAILABLE(macos(10.6), ios(10.0), watchos(3.0), tvos(10.0));

/*!
    @function   AudioQueueNewInputWithDispatchQueue
    @abstract   Creates a new audio queue for recording audio data.

	Outline of how to use the queue for input:
	
	- create input queue
	- allocate buffers
	- enqueue buffers (AudioQueueEnqueueBuffer, not with parameters, no packet descriptions)
	- the callback receives buffers and re-enqueues them
        
    @param      outAQ
        On return, this variable contains a pointer to the newly created recording audio queue
        object.
    @param      inFormat
        A pointer to a structure describing the format of the audio data to be recorded. For
        linear PCM, only interleaved formats are supported. Compressed formats are supported.
    @param      inFlags
        Reserved for future use. Pass 0.
    @param      inCallbackDispatchQueue
        The dispatch queue from which inCallbackBlock is to be called.
    @param      inCallbackBlock
        A pointer to a callback block to be called when the audio queue has finished filling
        a buffer.
    @result     An OSStatus result code.
*/
extern OSStatus             
AudioQueueNewInputWithDispatchQueue(AudioQueueRef __nullable * __nonnull outAQ,
                                    const AudioStreamBasicDescription *inFormat,
                                    UInt32                          inFlags,
                                    dispatch_queue_t                inCallbackDispatchQueue,
                                    AudioQueueInputCallbackBlock    inCallbackBlock)
										API_AVAILABLE(macos(10.6), ios(10.0), watchos(3.0), tvos(10.0));
#endif // defined(__BLOCKS__)

/*!
    @function   AudioQueueDispose
    @abstract   Disposes an existing audio queue.

	Disposing of the audio queue also disposes of all its resources, including its buffers.

    @param      inAQ
        The audio queue you want to dispose of
    @param      inImmediate
        If you pass true, the audio queue is disposed of immediately (that is, synchronously).
        If you pass false, disposal does not take place until all enqueued buffers are
        processed. Whether you call AudioQueueDispose synchronously or asynchronously, you can
        no longer interact with the queue, and the queue no longer invokes any callbacks to your
        application after the function returns.
		
		Note that if AudioQueueDispose is called from a buffer completion callback or property
		listener, you may receive further callbacks afterwards.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueDispose(                  AudioQueueRef           inAQ, 
                                    Boolean                 inImmediate)            API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

#pragma mark -
#pragma mark Buffer Management
//=============================================================================
//  Buffer Management
//=============================================================================

/*!
    @function   AudioQueueAllocateBuffer
    @abstract   Asks an audio queue to allocate a buffer.

	Once allocated, the pointer to the buffer and the buffer's size are fixed and cannot be
	changed. The mAudioDataByteSize field in the audio queue buffer structure,
	AudioQueueBuffer, is initially set to 0.
        
    @param      inAQ
        The audio queue you want to allocate a buffer.
    @param      inBufferByteSize
        The desired size of the new buffer, in bytes. An appropriate buffer size depends on the
        processing you will perform on the data as well as on the audio data format.
    @param      outBuffer
        On return, points to the newly created audio buffer. The mAudioDataByteSize field in the
        audio queue buffer structure, AudioQueueBuffer, is initially set to 0.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueAllocateBuffer(           AudioQueueRef           inAQ,
                                    UInt32                  inBufferByteSize,
                                    AudioQueueBufferRef __nullable * __nonnull outBuffer)              API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueAllocateBufferWithPacketDescriptions
    @abstract   Asks an audio queue to allocate a buffer with space for packet descriptions.

	Once allocated, the pointer to the buffer and the buffer's size are fixed and cannot be
	changed. The mAudioDataByteSize field in the audio queue buffer structure,
	AudioQueueBuffer, is initially set to 0.
        
    @param      inAQ
        The audio queue you want to allocate a buffer.
    @param      inBufferByteSize
        The desired size of the new buffer, in bytes. An appropriate buffer size depends on the
        processing you will perform on the data as well as on the audio data format.
    @param      inNumberPacketDescriptions
        The desired capacity of the packet description array in the new buffer.
    @param      outBuffer
        On return, points to the newly created audio buffer. The mAudioDataByteSize field in the
        audio queue buffer structure, AudioQueueBuffer, is initially set to 0.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueAllocateBufferWithPacketDescriptions(
                                    AudioQueueRef           inAQ,
                                    UInt32                  inBufferByteSize,
                                    UInt32                  inNumberPacketDescriptions,
                                    AudioQueueBufferRef __nullable * __nonnull outBuffer)              API_AVAILABLE(macos(10.6), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueFreeBuffer
    @abstract   Disposes of an audio queue buffer.

	This function disposes of the buffer allocated by AudioQueueAllocateBuffer. Disposing of
	an audio queue also automatically disposes of any associated buffers and timeline
	objects. Call this function only if you want to dispose of a particular buffer while
	continuing to use an audio queue. You can dispose of buffers only when the associated
	queue is stopped (that is, not processing audio data).

    @param      inAQ
        The queue from which the buffer was allocated.
    @param      inBuffer
        The buffer to be disposed.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueFreeBuffer(               AudioQueueRef           inAQ,
                                    AudioQueueBufferRef     inBuffer)           API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));



/*!
    @function   AudioQueueEnqueueBuffer
    @abstract   Assigns a buffer to an audio queue for recording or playback.

	If the buffer was allocated with AudioQueueAllocateBufferWithPacketDescriptions,
	the client should provide packet descriptions in the buffer's mPacketDescriptions
	and mPacketDescriptionCount fields rather than in inPacketDescs and
	inNumPacketDescs, which should be NULL and 0, respectively, in this case.
	
	For an input queue, pass 0 and NULL for inNumPacketDescs and inPacketDescs,
	respectively. Your callback will receive packet descriptions owned by the audio queue.

    @param      inAQ
        The audio queue you are assigning the buffer to.
    @param      inBuffer
        The buffer to queue (that is, to be recorded into or played from).
    @param      inNumPacketDescs
        The number of packet descriptions pointed to by the inPacketDescs pointer. Applicable
        only for output queues and required only for variable-bit-rate (VBR) audio formats. Pass
        0 for input queues (no packet descriptions are required).
    @param      inPacketDescs
        An array of packet descriptions. Applicable only for output queues and required only for
        variable-bit-rate (VBR) audio formats. Pass NULL for input queues (no packet
        descriptions are required).
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueEnqueueBuffer(            AudioQueueRef                       inAQ,
                                    AudioQueueBufferRef                 inBuffer,
                                    UInt32                              inNumPacketDescs,
                                    const AudioStreamPacketDescription * __nullable inPacketDescs)      API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueEnqueueBufferWithParameters
    @abstract   Assigns a buffer to an audio queue for playback, providing parameters
                and start time information.

	You can exert some control of the buffer queue by using this function. You can assign
	audio queue settings that are in effect carried by an audio queue buffer as you enqueue
	it. Hence, these changes only take effect when an audio queue buffer begins playing.
	
	This function queues a buffer for playback only, not for recording. Audio queues for
	recording have no parameters, do not support variable-bit-rate (VBR) formats (which
	might require trimming), and have a different way to handle timing. When queued for
	playback, the buffer must contain the audio data to be played back. See
	AudioQueueEnqueueBuffer for details on queuing a buffer for recording.

	If the buffer was allocated with AudioQueueAllocateBufferWithPacketDescriptions,
	the client should provide packet descriptions in the buffer's mPacketDescriptions
	and mPacketDescriptionCount fields rather than in inPacketDescs and
	inNumPacketDescs, which should be NULL and 0, respectively, in this case.

    @param      inAQ
        The audio queue associated with the buffer.
    @param      inBuffer
        The buffer to be played from.
    @param      inNumPacketDescs
        The number of packet descriptions pointed to by the inPacketDescs parameter. Required
        only for variable-bit-rate (VBR) audio formats. Pass 0 if no packet descriptions are
        required.
    @param      inPacketDescs
        A pointer to an array of audio stream packet descriptions. Required only for VBR audio
        formats. Pass NULL if no packet descriptions are required.
    @param      inTrimFramesAtStart
        The number of priming frames to skip at the start of the buffer.
    @param      inTrimFramesAtEnd
        The number of frames to skip at the end of the buffer.
    @param      inNumParamValues
        The number of parameter values pointed to by the inParamValues parameter.
    @param      inParamValues
        An array of parameter values. (In macOS v10.5, there is only one parameter,
        kAudioQueueParam_Volume.) These values are set before buffer playback and cannot be
        changed while the buffer is playing. How accurately changes in parameters can be
        scheduled depends on the size of the buffer. If there are no parameters to set
        (inNumParamValues = 0), pass NULL.
    @param      inStartTime
        A pointer to a structure containing the desired start time for playing the buffer. If
        you specify the time using the mSampleTime field of the AudioTimeStamp structure, the
        sample time is relative to the time the queue started. If you pass NULL for the start
        time, the buffer starts immediately after the previously queued buffer, or as soon as
        possible if no buffers are queued ahead of it. Buffers are played in the order they are
        queued. If multiple buffers are queued, their times must be in ascending order or NULL;
        otherwise, an error occurs. The start time indicates when the actual audio data in the
        buffer is to be played (that is, the trim frames are not counted).
        
        Note: When specifying a start time for a buffer, if the buffer is not the first enqueued
        since AudioQueueStop or AudioQueueReset, it is normally necessary to call AudioQueueFlush
        before AudioQueueEnqueueBufferWithParameters.
    @param      outActualStartTime
        On return, points to an AudioTimeStamp structure indicating when the buffer will
        actually play.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueEnqueueBufferWithParameters(
                                    AudioQueueRef                                inAQ,
                                    AudioQueueBufferRef                          inBuffer,
                                    UInt32                                       inNumPacketDescs,
                                    const AudioStreamPacketDescription * __nullable inPacketDescs,
                                    UInt32                                       inTrimFramesAtStart,
                                    UInt32                                       inTrimFramesAtEnd,
                                    UInt32                                       inNumParamValues,
                                    const AudioQueueParameterEvent * __nullable  inParamValues,
                                    const AudioTimeStamp * __nullable            inStartTime,
                                    AudioTimeStamp * __nullable                  outActualStartTime)     API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

#pragma mark -
#pragma mark Queue Control
//=============================================================================
//  Queue Control
//=============================================================================

/*!
    @function   AudioQueueStart
    @abstract   Begins playing or recording audio.

	If the audio hardware is not already running, this function starts it.

    @param      inAQ
        The audio queue to start.
    @param      inStartTime
        A pointer to the time at which the audio queue should start. If you specify the time
        using the mSampleTime field of the AudioTimeStamp structure, the sample time is
        referenced to the sample frame timeline of the associated audio device. May be NULL.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueStart(                    AudioQueueRef                     inAQ,
                                    const AudioTimeStamp * __nullable inStartTime)        API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueuePrime
    @abstract   Begins decoding buffers in preparation for playback.

	This function begins decoding buffers in preparation for playback. It returns when at
	least the number of audio sample frames are decoded and ready to play or when all
	enqueued buffers have been completely decoded. To ensure that a buffer has been decoded
	and is completely ready for playback, before playback:

	1.  Call AudioQueueEnqueueBuffer.
	2.  Call AudioQueuePrime, which waits if you pass 0 to have a default number of
		frames decoded.
	3.  Call AudioQueueStart.

	Calls to AudioQueuePrime following AudioQueueStart/AudioQueuePrime, and before
	AudioQueueReset/AudioQueueStop, will have no useful effect. In this situation,
	outNumberOfFramesPrepared will not have a useful return value.

    @param      inAQ
        The audio queue to be primed.
    @param      inNumberOfFramesToPrepare
        The number of frames to decode before returning. Pass 0 to decode all enqueued buffers.
    @param      outNumberOfFramesPrepared
        If not NULL, on return, a pointer to the number of frames actually decoded and prepared
        for playback.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueuePrime(                    AudioQueueRef           inAQ,
                                    UInt32                  inNumberOfFramesToPrepare,
                                    UInt32 * __nullable     outNumberOfFramesPrepared)  API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueStop
    @abstract   Stops playing or recording audio.

	This function resets the audio queue and stops the audio hardware associated with the
	queue if it is not in use by other audio services. Synchronous stops occur immediately,
	regardless of previously buffered audio data. Asynchronous stops occur after all queued
	buffers have been played or recorded.

    @param      inAQ
        The audio queue to stop.
    @param      inImmediate
        If you pass true, the stop request occurs immediately (that is, synchronously), and the
        function returns when the audio queue has stopped. Buffer callbacks are invoked during
        the stopping. If you pass false, the function returns immediately, but the queue does
        not stop until all its queued buffers are played or filled (that is, the stop occurs
        asynchronously). Buffer callbacks are invoked as necessary until the queue actually
        stops. Also, a playback audio queue callback calls this function when there is no more
        audio to play.

        Note that when stopping immediately, all pending buffer callbacks are normally invoked
        during the process of stopping. But if the calling thread is responding to a buffer
        callback, then it is possible for additional buffer callbacks to occur after
        AudioQueueStop returns.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueStop(                     AudioQueueRef           inAQ,
                                    Boolean                 inImmediate)            API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueuePause
    @abstract   Pauses audio playback or recording.

	Pausing the queue does not affect buffers or reset the audio queue. To resume playback
	or recording using the audio queue, call AudioQueueStart.

    @param      inAQ
        The queue to be paused.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueuePause(                    AudioQueueRef           inAQ)       API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueFlush
    @abstract   Resets the audio queue's decoder state.

	After all queued buffers have been played, the function cleans up all decoder state
	information. You must call this function following a sequence of buffers of encoded
	audio; otherwise, some of the audio might not play in the next set of queued buffers.
	The only time it is not necessary to call AudioQueueFlush is following AudioQueueStop
	with inImmediate=false. (This action internally calls AudioQueueFlush.)
	
	Also, you might wish to call this function before calling AudioQueueStop depending on
	whether you want to stop immediately regardless of what has played or whether you want
	to ensure that all buffered data and all data that is in the middle of processing gets
	recorded or played before stopping.
        
    @param      inAQ
        The audio queue to be flushed.
        
    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueFlush(                    AudioQueueRef           inAQ)            API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueReset
    @abstract   Resets an audio queue.

	This function immediately resets an audio queue, flushes any queued buffer, removes all
	buffers from previously scheduled use, and resets any decoder and digital signal
	processing (DSP) state information. It also invokes callbacks for any flushed buffers.
	If you queue any buffers after calling this function, processing does not occur until
	the decoder and DSP state information is reset. Hence, a discontinuity (that is, a
	"glitch") might occur.

	Note that when resetting, all pending buffer callbacks are normally invoked
	during the process of resetting. But if the calling thread is responding to a buffer
	callback, then it is possible for additional buffer callbacks to occur after
	AudioQueueReset returns.

    @param      inAQ
        The audio queue to reset.

    @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueReset(                    AudioQueueRef           inAQ)            API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

#pragma mark -
#pragma mark Parameter Management
//=============================================================================
//  Parameter Management
//=============================================================================

/*!
    @function   AudioQueueGetParameter
    @abstract   Obtains an audio queue parameter value.

	You can access the current parameter values for an audio queue at any time with this
	function.

    @param      inAQ
        The audio queue whose parameter value you want to obtain.
    @param      inParamID
        The ID of the parameter you want to obtain. In macOS v10.5, audio queues have one
        parameter available: kAudioQueueParam_Volume, which controls the queue's playback
        volume.
    @param      outValue
        On return, points to the current value of the specified parameter.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueGetParameter(             AudioQueueRef               inAQ,
                                    AudioQueueParameterID       inParamID,
                                    AudioQueueParameterValue *  outValue)       API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueSetParameter
    @abstract   Sets an audio queue parameter value.
    @param      inAQ
        The audio queue whose parameter value you want to set.
    @param      inParamID
        The ID of the parameter you want to set.
    @param      inValue
        The parameter value to set.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueSetParameter(             AudioQueueRef               inAQ,
                                    AudioQueueParameterID       inParamID,
                                    AudioQueueParameterValue    inValue)        API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

                                                                        
#pragma mark -
#pragma mark Property Management
//=============================================================================
//  Property Management
//=============================================================================

/*!
    @function   AudioQueueGetProperty
    @abstract   Obtains an audio queue property value.
    @param      inAQ
        The audio queue whose property value you want to obtain.
    @param      inID
        The ID of the property you want to obtain. See "Audio Queue Property IDs."
    @param      outData
        On return, points to the desired property value.
    @param      ioDataSize
        A pointer to the size of the property data. On input, points to the maximum bytes of
        space the caller expects to receive. On return, points to the actual data size.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueGetProperty(              AudioQueueRef           inAQ,
                                    AudioQueuePropertyID    inID,
                                    void *                  outData,
                                    UInt32 *                ioDataSize)             API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueSetProperty
    @abstract   Sets an audio queue property value.
    @param      inAQ
        The audio queue whose property value you want to set.
    @param      inID
        The ID of the property you want to set. See "Audio Queue Property IDs" for the various
        audio queue properties.
    @param      inData
        A pointer to the property value to set.
    @param      inDataSize
        The size of the property data.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueSetProperty(              AudioQueueRef           inAQ,
                                    AudioQueuePropertyID    inID,
                                    const void *            inData,
                                    UInt32                  inDataSize)             API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));


/*!
    @function   AudioQueueGetPropertySize
    @abstract   Obtains the size of an audio queue property.
    @param      inAQ
        The audio queue containing the property value whose size you want to obtain.
    @param      inID
        The ID of the property value whose size you want to obtain. See "Audio Queue Property
        IDs" for possible values.
    @param      outDataSize
        On return, points to the size of the specified property value.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueGetPropertySize(          AudioQueueRef           inAQ,
                                    AudioQueuePropertyID    inID,
                                    UInt32 *                outDataSize)            API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueAddPropertyListener
    @abstract   Adds a listener callback for a property.

	This callback is used to act upon a change in an audio queue property such as
	kAudioQueueProperty_IsRunning. For instance, if your application has a user interface
	with a Play/Stop button, and kAudioQueueProperty_IsRunning changes, you need to update
	your button.

    @param      inAQ
        The audio queue that owns the property you want to assign the listener callback to.
    @param      inID
        The ID of the property to which you want to assign a listener callback. See "Audio Queue Property IDs".
    @param      inProc
        The listener callback to be called when the property value changes.
    @param      inUserData
        A value to be passed to the listener callback when it is called.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueAddPropertyListener(      AudioQueueRef                   inAQ,
                                    AudioQueuePropertyID            inID,
                                    AudioQueuePropertyListenerProc  inProc,
                                    void * __nullable               inUserData)     API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueRemovePropertyListener
    @abstract   Removes a listener callback for a property.
    @param      inAQ
        The audio queue that owns the property from which you want to remove a listener.
    @param      inID
        The ID of the property from which you want to remove a listener.
    @param      inProc
        The listener being removed.
    @param      inUserData
        The same inUserData value that was previously passed to AudioQueueAddPropertyListener.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueRemovePropertyListener(   AudioQueueRef                   inAQ,
                                    AudioQueuePropertyID            inID,
                                    AudioQueuePropertyListenerProc  inProc,
                                    void * __nullable               inUserData)     API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

                                    

#pragma mark -
#pragma mark Handling Timing
//=============================================================================
//  Handling Timing
//=============================================================================

/*!
    @function   AudioQueueCreateTimeline
    @abstract   Creates a timeline object.

	You need to instantiate a timeline object if you want to know about any timeline
	discontinuities. See AudioQueueGetCurrentTime for more details.

    @param      inAQ
        The audio queue to associate with the new timeline object.
    @param      outTimeline
        On return, points to the newly created timeline object.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueCreateTimeline(           AudioQueueRef           inAQ,
                                    AudioQueueTimelineRef __nullable * __nonnull outTimeline)            API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueDisposeTimeline
    @abstract   Disposes of a timeline object.

	Disposing of an audio queue automatically disposes of any associated timeline objects.
	Call this function only if you want to dispose of a timeline object and not the audio
	queue associated with it.

    @param      inAQ
        The audio queue associated with the timeline object you want to dispose of.
    @param      inTimeline
        The timeline object to dispose of.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueDisposeTimeline(          AudioQueueRef           inAQ,
                                    AudioQueueTimelineRef   inTimeline)             API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueGetCurrentTime
    @abstract   Obtains the current audio queue time.

	You must specify a timeline object if you want to be notified about any timeline
	discontinuities in the outTimelineDiscontinuity parameter. If you don't care about
	discontinuities, pass NULL in the inTimeLine and outTimelineDiscontinuity parameters.

    @param      inAQ
        The audio queue whose current time you want to obtain.
    @param      inTimeline
        The audio queue timeline object to which any timeline discontinuities are reported. May
        be NULL.
    @param      outTimeStamp
        On return, points to an audio timestamp structure containing the current audio queue
        time. The mSampleTime field is in terms of the audio queue's sample rate, and relative
        to the time at which the queue has started or will start.
    @param      outTimelineDiscontinuity
        Can be NULL. On return, only set to true or false if the inTimeLine parameter is not
        NULL. Set to true if a discontinuity has occurred in the sample timeline of the audio
        queue. For instance, the device's sample rate changed and a gap occurred in playback or
        recording, or the audio queue was unable to prepare and playback in time because it was
        late.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueGetCurrentTime(           AudioQueueRef                    inAQ,
                                    AudioQueueTimelineRef __nullable inTimeline,
                                    AudioTimeStamp * __nullable      outTimeStamp,
                                    Boolean * __nullable             outTimelineDiscontinuity)       API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueDeviceGetCurrentTime
    @abstract   Obtains the current time of the audio device associated with an audio queue.

	If the audio device associated with the audio queue is not running, the only valid field
	in the audio timestamp structure is mHostTime. This result differentiates the action of
	this function from that of the AudioDeviceGetCurrentTime function, (declared in
	AudioHardware.h) which returns an error if the audio device is not running.

    @param      inAQ
        The audio queue whose audio device is to be queried.
    @param      outTimeStamp
        A pointer to a structure that, on return, contains the current time of the audio device
        associated with the audio queue.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueDeviceGetCurrentTime(     AudioQueueRef           inAQ,
                                    AudioTimeStamp *        outTimeStamp)      API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueDeviceTranslateTime
    @abstract   Converts the time in the time base of the associated audio device from one
                representation to another.

	This function converts from one time representation to another (for example, from sample
	time to host time or vice versa):
	
	- Sample time is the absolute sample frame time. Sample numbers are the count of the samples
		on the audio device.
	- Host time is the time base of the host machine such as the time of the bus clock on the CPU.

	The mSampleTime field in the AudioTimestamp structure (described in Core Audio Data
	Types Reference) is always in device time, not in audio queue time. Audio queue time is
	relative to the audio queue's start time. The associated audio device has to be running
	for the AudioQueueDeviceTranslateTime function to provide a result.

    @param      inAQ
        The queue whose audio device is to perform the requested time translation.
    @param      inTime
        A pointer to a structure containing the time to be translated.
    @param      outTime
        On entry, mFlags indicate the desired translations. On exit, mFlags indicates which
        of the requested translated fields were successfully populated.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueDeviceTranslateTime(      AudioQueueRef           inAQ,
                                    const AudioTimeStamp *  inTime,
                                    AudioTimeStamp *        outTime)        API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueDeviceGetNearestStartTime
    @abstract   Obtains an audio device's start time that is closest to a requested start time.
    @param      inAQ
        The audio queue whose device's nearest start time you want to obtain.
    @param      ioRequestedStartTime
        On entry, points to the requested start time. On return, points to the actual start time.
    @param      inFlags
        Reserved for future use. Pass 0.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueDeviceGetNearestStartTime(AudioQueueRef           inAQ,
                                    AudioTimeStamp *        ioRequestedStartTime,
                                    UInt32                  inFlags)        API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

#pragma mark -
#pragma mark Offline Rendering
//=============================================================================
//  Offline Rendering
//=============================================================================

/*!
    @function   AudioQueueSetOfflineRenderFormat
    @abstract   Specify an audio format to which the queue will perform subsequent offline rendering,
                or disable offline rendering.

	An output queue's audio playback can be redirected for capture to an audio file,
	to support an export function, for example. AudioQueueSetOfflineRenderFormat switches
	a queue between normal and offline rendering modes.

    @param      inAQ
        The output queue whose offline rendering mode is to be changed.
    @param      inFormat
        The desired format for offline rendering. Pass NULL to disable offline rendering and return the
        queue to normal output to an audio device. This format must be linear PCM and (if not mono)
        interleaved.
    @param      inLayout
        The desired channel layout for offline rendering; also NULL when disabling offline rendering.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueSetOfflineRenderFormat(   AudioQueueRef                                  inAQ,
                                    const AudioStreamBasicDescription * __nullable inFormat,
                                    const AudioChannelLayout * __nullable          inLayout)
																		API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueOfflineRender
    @abstract   Obtain a buffer of audio output from a queue in offline rendering mode.
    @param      inAQ
        The output queue from which to obtain output.
    @param      inTimestamp
        The point in time corresponding to the beginning of the output buffer. Only mSampleTime
        is used. mFlags must include kAudioTimeStampSampleTimeValid.
    @param      ioBuffer
        The buffer into which the queue will render.
    @param      inNumberFrames
        The number of frames of audio to render. Note that fewer frames than requested may be returned.
        This can happen if insufficient data was enqueued.
    @result
        An OSStatus result code.
*/
extern OSStatus
AudioQueueOfflineRender(            AudioQueueRef           inAQ,
                                    const AudioTimeStamp *  inTimestamp,
                                    AudioQueueBufferRef     ioBuffer,
                                    UInt32                  inNumberFrames)     API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

#pragma mark -
#pragma mark Processing Taps
//=============================================================================
//  Processing Taps
//=============================================================================


/*!
    @function   AudioQueueProcessingTapNew
    @abstract   Create a new processing tap

	This function creates a processing tap on a given audio queue. A
	processing tap can only be established (or removed) on an audio queue that is
	stopped (paused is not sufficient). The processing tap will then be used to
	process either decoded data in the case of an output queue, or input data
	(before it is encoded) in the case of an input queue.

	The processing is performed on audio either before or after any effects or other
	processing (varispeed, etc) is applied by the audio queue, depending on inFlags.

    @param      inAQ    
                    The audio queue from which to create the processing tap
    @param      inCallback
                    A callback which the queue will call to process the audio
    @param      inClientData
                    Client data provided to the callback
    @param      inFlags
                    Flags that are used to control aspects of the processing tap.
                    Valid flags are:
                        - kAudioQueueProcessingTap_PreEffects: processing is done before any
                            further effects are applied by the audio queue to the audio
                        - kAudioQueueProcessingTap_PostEffects: processing is done after all
                            processing is done, including that of other taps.
                        - kAudioQueueProcessingTap_Siphon
    @param      outMaxFrames
                    The maximum number of sample frames that can be requested of a processing
                    tap at any one time. Typically this will be approximately 50 msec of audio
                    (2048 samples @ 44.1kHz)
    @param      outProcessingFormat
                    The format in which the client will receive the audio data to be processed.
                    This will always be the same sample rate as the client format and usually
                    the same number of channels as the client format of the audio queue. (NOTE:
                    the number of channels may be different in some cases if the client format
                    has some channel count restrictions, for instance the client provides 5.1
                    AAC, but the decoder can only produce stereo). The channel order, if the
                    same as the client format, will be the same as the client channel order. If
                    the channel count is changed, it will be to either 1 (mono) or 2 (stereo, in
                    which case the first channel is left, the second right).

                    If the data is not in a convenient format for the client to process in, then
                    the client should convert the data to and from that format. This is the most
                    efficient mechanism to use (as the audio queue can chose a format that is
                    most efficient from its playback (or recording) requirement.
    @param      outAQTap
                    The processing tap object.
                    
    @result     An OSStatus result code.
*/
extern OSStatus 
AudioQueueProcessingTapNew(         AudioQueueRef                   inAQ,
                                    AudioQueueProcessingTapCallback inCallback,
                                    void * __nullable               inClientData,
                                    AudioQueueProcessingTapFlags    inFlags,
                                    UInt32 *                        outMaxFrames,
                                    AudioStreamBasicDescription *   outProcessingFormat,
                                    AudioQueueProcessingTapRef __nullable * __nonnull outAQTap)
                                        API_AVAILABLE(macos(10.7), ios(6.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueProcessingTapDispose
    @abstract   Dispose a processing tap object

	As with AudioQueueProcessingTapNew(), this call can only be made on an
	audio queue that is stopped (paused is not sufficient)
    
    @param      inAQTap
                    The processing tap to dispose.

   @result     An OSStatus result code.
*/
extern OSStatus 
AudioQueueProcessingTapDispose(     AudioQueueProcessingTapRef      inAQTap)
                                        API_AVAILABLE(macos(10.7), ios(6.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueProcessingTapGetSourceAudio
    @abstract   Used by a processing tap to retrieve source audio.

	This function may only be called from the processing tap's callback.
    
    @param      inAQTap
                    the processing tap
    @param      inNumberFrames
                    the number of frames the processing tap requires for its processing
    @param      ioTimeStamp
                    On an input audio queue, the timestamp is returned from this function.
                    On an output audio queue, the caller must provide a continuous timestamp.
    @param      outFlags
                    flags to describe state about the input requested, e.g.
                    discontinuity/complete
    @param      outNumberFrames
                    the number of source frames that have been provided by the parent audio
                    queue. This can be less than the number of requested frames specified in
                    inNumberFrames
    @param      ioData
                    the audio buffer list which will contain the source data. The audio queue owns
                    the buffer pointers if NULL pointers were provided (recommended). In this case
                    the source buffers are only valid for the duration of the processing tap
                    callback. If the buffer pointers are non-NULL, then they must be big enough to
                    hold inNumberFrames, and the audio queue will copy its source data into those
                    buffers.
                    
   @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueProcessingTapGetSourceAudio(
                                    AudioQueueProcessingTapRef      inAQTap,
                                    UInt32                          inNumberFrames,
                                    AudioTimeStamp *                ioTimeStamp,
                                    AudioQueueProcessingTapFlags *  outFlags,
                                    UInt32 *                        outNumberFrames,
                                    AudioBufferList *               ioData)
                                        API_AVAILABLE(macos(10.7), ios(6.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioQueueProcessingTapGetQueueTime
    @abstract   Used by a processing tap to retrieve the queue's current time.

	This function may only be called from the processing tap's callback, and only
	for audio output queues. It must be called after calling
	AudioQueueProcessingTapGetSourceAudio().
    
    @param      inAQTap
                    the processing tap
    @param      outQueueSampleTime
                    the current sample time of the audio queue. This will appear to be stationary
                    if the queue is paused.
    @param      outQueueFrameCount
                    the number of sample frames of queue time corresponding to the current chunk of
                    audio being processed by the tap. This will differ from the frame count passed
                    to the tap if the queue's playback rate is currently other than 1.0, due to the
                    use of time compression/expansion. The frame count can also be 0 if the queue is
                    paused.
                    
   @result     An OSStatus result code.
*/
extern OSStatus
AudioQueueProcessingTapGetQueueTime(    AudioQueueProcessingTapRef  inAQTap,
                                        Float64 *                   outQueueSampleTime,
                                        UInt32 *                    outQueueFrameCount)
                                        API_AVAILABLE(macos(10.8), ios(6.0), watchos(2.0), tvos(9.0));

#ifdef __cplusplus
}
#endif

CF_ASSUME_NONNULL_END

#endif // AudioToolbox_AudioQueue_h

