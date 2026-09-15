#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioConverter.h>)
/*!
	@file		AudioConverter.h
	@framework	AudioToolbox.framework
	@copyright	(c) 1985-2015 by Apple, Inc., all rights reserved.
    @abstract   API's to perform audio format conversions.
    
	AudioConverters convert between various linear PCM and compressed
	audio formats. Supported transformations include:

	- PCM float/integer/bit depth conversions
	- PCM sample rate conversion
	- PCM interleaving and deinterleaving
	- encoding PCM to compressed formats
	- decoding compressed formats to PCM

	A single AudioConverter may perform more than one
	of the above transformations.
*/

#ifndef AudioToolbox_AudioConverter_h
#define AudioToolbox_AudioConverter_h

//=============================================================================
//  Includes
//=============================================================================

#include <Availability.h>
#include <CoreAudioTypes/CoreAudioTypes.h>

CF_ASSUME_NONNULL_BEGIN

#if defined(__cplusplus)
extern "C"
{
#endif

//=============================================================================
//  Theory of Operation
//=============================================================================

//=============================================================================
//  Types specific to the Audio Converter API
//=============================================================================

/*!
    @typedef    AudioConverterRef
    @abstract   A reference to an AudioConverter object.
*/
typedef struct OpaqueAudioConverter *   AudioConverterRef;

typedef UInt32                          AudioConverterPropertyID;

//=============================================================================
//  Standard Properties
//=============================================================================

/*!
    @enum       AudioConverter Property IDs
    @abstract   The properties of an AudioConverter, accessible via AudioConverterGetProperty()
                and AudioConverterSetProperty().
    
    @constant   kAudioConverterPropertyMinimumInputBufferSize
                    a UInt32 that indicates the size in bytes of the smallest buffer of input
                    data that can be supplied via the AudioConverterInputProc or as the input to
                    AudioConverterConvertBuffer

    @constant   kAudioConverterPropertyMinimumOutputBufferSize
                    a UInt32 that indicates the size in bytes of the smallest buffer of output
                    data that can be supplied to AudioConverterFillComplexBuffer or as the output to
                    AudioConverterConvertBuffer

    @constant   kAudioConverterPropertyMaximumInputPacketSize
                    a UInt32 that indicates the size in bytes of the largest single packet of
                    data in the input format. This is mostly useful for variable bit rate
                    compressed data (decoders).

    @constant   kAudioConverterPropertyMaximumOutputPacketSize
                    a UInt32 that indicates the size in bytes of the largest single packet of
                    data in the output format. This is mostly useful for variable bit rate
                    compressed data (encoders).
    @constant   kAudioConverterPropertyCalculateInputBufferSize
                    a UInt32 that on input holds a size in bytes that is desired for the output
                    data. On output, it will hold the size in bytes of the input buffer required
                    to generate that much output data. Note that some converters cannot do this
                    calculation.
    @constant   kAudioConverterPropertyCalculateOutputBufferSize
                    a UInt32 that on input holds a size in bytes that is desired for the input
                    data. On output, it will hold the size in bytes of the output buffer
                    required to hold the output data that will be generated. Note that some
                    converters cannot do this calculation.
    @constant   kAudioConverterPropertyInputCodecParameters
                    The value of this property varies from format to format and is considered
                    private to the format. It is treated as a buffer of untyped data.
    @constant   kAudioConverterPropertyOutputCodecParameters
                    The value of this property varies from format to format and is considered
                    private to the format. It is treated as a buffer of untyped data.
    @constant   kAudioConverterSampleRateConverterComplexity
                    An OSType that specifies the sample rate converter algorithm to use (as defined in
                    AudioToolbox/AudioUnitProperties.h)
    @constant   kAudioConverterSampleRateConverterQuality
                    A UInt32 that specifies rendering quality of the sample rate converter (see
                    enum constants below)
    @constant   kAudioConverterSampleRateConverterInitialPhase
                    A Float64 with value 0.0 <= x < 1.0 giving the initial subsample position of the 
                    sample rate converter.
    @constant   kAudioConverterCodecQuality
                    A UInt32 that specifies rendering quality of a codec (see enum constants
                    below)
    @constant   kAudioConverterPrimeMethod
                    a UInt32 specifying priming method (usually for sample-rate converter) see
                    explanation for struct AudioConverterPrimeInfo below along with enum
                    constants
    @constant   kAudioConverterPrimeInfo
                    A pointer to AudioConverterPrimeInfo (see explanation for struct
                    AudioConverterPrimeInfo below)
    @constant   kAudioConverterChannelMap
                    An array of SInt32's.  The size of the array is the number of output
                    channels, and each element specifies which input channel's data is routed to
                    that output channel (using a 0-based index of the input channels), or -1 if
                    no input channel is to be routed to that output channel.  The default
                    behavior is as follows. I = number of input channels, O = number of output
                    channels. When I > O, the first O inputs are routed to the first O outputs,
                    and the remaining puts discarded.  When O > I, the first I inputs are routed
                    to the first O outputs, and the remaining outputs are zeroed.
                    
                    A simple example for splitting mono input to stereo output (instead of routing
                    the input to only the first output channel):
                    
	```
	// this should be as large as the number of output channels:
	SInt32 channelMap[2] = { 0, 0 };
	AudioConverterSetProperty(theConverter, kAudioConverterChannelMap,
	sizeof(channelMap), channelMap);
	```

    @constant   kAudioConverterDecompressionMagicCookie
                    A void * pointing to memory set up by the caller. Required by some formats
                    in order to decompress the input data.
    @constant   kAudioConverterCompressionMagicCookie
                    A void * pointing to memory set up by the caller. Returned by the converter
                    so that it may be stored along with the output data. It can then be passed
                    back to the converter for decompression at a later time.
    @constant   kAudioConverterEncodeBitRate
                    A UInt32 containing the number of bits per second to aim for when encoding
                    data. Some decoders will also allow you to get this property to discover the bit rate.
    @constant   kAudioConverterEncodeAdjustableSampleRate
                    For encoders where the AudioConverter was created with an output sample rate
                    of zero, and the codec can do rate conversion on its input, this provides a
                    way to set the output sample rate. The property value is a Float64.
    @constant   kAudioConverterInputChannelLayout
                    The property value is an AudioChannelLayout.
    @constant   kAudioConverterOutputChannelLayout
                    The property value is an AudioChannelLayout.
    @constant   kAudioConverterApplicableEncodeBitRates
                    The property value is an array of AudioValueRange describing applicable bit
                    rates based on current settings.
    @constant   kAudioConverterAvailableEncodeBitRates
                    The property value is an array of AudioValueRange describing available bit
                    rates based on the input format. You can get all available bit rates from
                    the AudioFormat API.
    @constant   kAudioConverterApplicableEncodeSampleRates
                    The property value is an array of AudioValueRange describing applicable
                    sample rates based on current settings.
    @constant   kAudioConverterAvailableEncodeSampleRates
                    The property value is an array of AudioValueRange describing available
                    sample rates based on the input format. You can get all available sample
                    rates from the AudioFormat API.
    @constant   kAudioConverterAvailableEncodeChannelLayoutTags
                    The property value is an array of AudioChannelLayoutTags for the format and
                    number of channels specified in the input format going to the encoder.
    @constant   kAudioConverterCurrentOutputStreamDescription
                    Returns the current completely specified output AudioStreamBasicDescription.
                    For example when encoding to AAC, your original output stream description
                    will not have been completely filled out.
    @constant   kAudioConverterCurrentInputStreamDescription
                    Returns the current completely specified input AudioStreamBasicDescription.
    @constant   kAudioConverterPropertySettings
                    Returns the a CFArray of property settings for converters.
    @constant   kAudioConverterPropertyBitDepthHint
                    An SInt32 of the source bit depth to preserve. This is a hint to some
                    encoders like lossless about how many bits to preserve in the input. The
                    converter usually tries to preserve as many as possible, but a lossless
                    encoder will do poorly if more bits are supplied than are desired in the
                    output. The bit depth is expressed as a negative number if the source was floating point,
                    e.g. -32 for float, -64 for double.
    @constant   kAudioConverterPropertyFormatList
                    An array of AudioFormatListItem structs describing all the data formats produced by the
                    encoder end of the AudioConverter. If the ioPropertyDataSize parameter indicates that
                    outPropertyData is sizeof(AudioFormatListItem), then only the best format is returned.
                    This property may be used for example to discover all the data formats produced by the AAC_HE2
                    (AAC High Efficiency vers. 2) encoder.
    @constant   kAudioConverterPropertyPerformDownmix
                    A UInt32 with 1 meaning to perform a mix from input to output channels,
                    and 0 meaning to not perform such a mix.  See kAudioConverterPropertyChannelMixMap
                    for more information on how the mix maps inputs to outputs.
                    Defaults to 0 (no channel mixing).
                    Note #1: Mixing may be performed in any conversion where the number
                    and/or layout of input and output channels are not the same, not only
                    in PCM-to-PCM conversions, and also the number of output channels may
                    be more or less than the number of input channels.
                    Note #2: This property is not compatible with kAudioConverterChannelMap,
                    which is used to map input to output channels without any mixing.
                    The kAudioConverterChannelMap property cannot be set unless
                    kAudioConverterPropertyPerformDownmix is first set to 0.
    @constant   kAudioConverterPropertyChannelMixMap
                    An array of Float32 values, size equal to the product of the numbers of
                    input and output channels.  Each element's value is a gain to apply,
                    from 0.0 to 1.0, to one input when mixing it into one output.
                    The elements are ordered in row-major order, where each row holds the
                    gains to apply for one input to each output.
                    Unless explicitly set, a default mix map is used, based on other
                    properties such as numbers of input and output channels as well as
                    channel layouts, if known.
                    This property should not be set unless and until
                    kAudioConverterPropertyPerformDownmix is first set to 1 (enable channel mixing).
*/
CF_ENUM(AudioConverterPropertyID)
{
    kAudioConverterPropertyMinimumInputBufferSize       = 'mibs',
    kAudioConverterPropertyMinimumOutputBufferSize      = 'mobs',
    kAudioConverterPropertyMaximumInputPacketSize       = 'xips',
    kAudioConverterPropertyMaximumOutputPacketSize      = 'xops',
    kAudioConverterPropertyCalculateInputBufferSize     = 'cibs',
    kAudioConverterPropertyCalculateOutputBufferSize    = 'cobs',
    kAudioConverterPropertyInputCodecParameters         = 'icdp',
    kAudioConverterPropertyOutputCodecParameters        = 'ocdp',
    kAudioConverterSampleRateConverterComplexity        = 'srca',
    kAudioConverterSampleRateConverterQuality           = 'srcq',
    kAudioConverterSampleRateConverterInitialPhase      = 'srcp',
    kAudioConverterCodecQuality                         = 'cdqu',
    kAudioConverterPrimeMethod                          = 'prmm',
    kAudioConverterPrimeInfo                            = 'prim',
    kAudioConverterChannelMap                           = 'chmp',
    kAudioConverterDecompressionMagicCookie             = 'dmgc',
    kAudioConverterCompressionMagicCookie               = 'cmgc',
    kAudioConverterEncodeBitRate                        = 'brat',
    kAudioConverterEncodeAdjustableSampleRate           = 'ajsr',
    kAudioConverterInputChannelLayout                   = 'icl ',
    kAudioConverterOutputChannelLayout                  = 'ocl ',
    kAudioConverterApplicableEncodeBitRates             = 'aebr',
    kAudioConverterAvailableEncodeBitRates              = 'vebr',
    kAudioConverterApplicableEncodeSampleRates          = 'aesr',
    kAudioConverterAvailableEncodeSampleRates           = 'vesr',
    kAudioConverterAvailableEncodeChannelLayoutTags     = 'aecl',
    kAudioConverterCurrentOutputStreamDescription       = 'acod',
    kAudioConverterCurrentInputStreamDescription        = 'acid',
    kAudioConverterPropertySettings                     = 'acps',
    kAudioConverterPropertyBitDepthHint                 = 'acbd',
    kAudioConverterPropertyFormatList                   = 'flst',
    kAudioConverterPropertyPerformDownmix               = 'dmix',
    kAudioConverterPropertyChannelMixMap                = 'mmap'
};

#if !TARGET_OS_IPHONE
//=============================================================================
//  
//=============================================================================

/*!
    @enum       macOS AudioConverter Properties

    @constant   kAudioConverterPropertyDithering
					A UInt32. Set to a value from the enum of dithering algorithms below. 
					Zero means no dithering and is the default. (macOS only.)
    @constant   kAudioConverterPropertyDitherBitDepth
					A UInt32. Dither is applied at this bit depth.  (macOS only.)

*/
CF_ENUM(AudioConverterPropertyID)
{
	kAudioConverterPropertyDithering					= 'dith',
	kAudioConverterPropertyDitherBitDepth				= 'dbit'
};

/*!
    @enum       Dithering algorithms

    @abstract   Constants to be used as the value for kAudioConverterPropertyDithering.

    @constant   kDitherAlgorithm_TPDF             Dither signal is generated by a white noise source with a triangular probability density function
    @constant   kDitherAlgorithm_NoiseShaping     Use a static, perceptually weighted noise shaped dither
*/
CF_ENUM(UInt32)
{
	kDitherAlgorithm_TPDF				= 1,	
	kDitherAlgorithm_NoiseShaping		= 2 
};
#endif

/*!
    @enum       Quality constants

    @abstract   Constants to be used with kAudioConverterSampleRateConverterQuality.

    @constant   kAudioConverterQuality_Max
    				maximum quality
    @constant   kAudioConverterQuality_High
    				high quality
    @constant   kAudioConverterQuality_Medium
    				medium quality
    @constant   kAudioConverterQuality_Low
    				low quality
    @constant   kAudioConverterQuality_Min
    				minimum quality
*/
CF_ENUM(UInt32)
{
    kAudioConverterQuality_Max                              = 0x7F,
    kAudioConverterQuality_High                             = 0x60,
    kAudioConverterQuality_Medium                           = 0x40,
    kAudioConverterQuality_Low                              = 0x20,
    kAudioConverterQuality_Min                              = 0
};


/*!
    @enum           Sample Rate Converter Complexity
    @constant       kAudioConverterSampleRateConverterComplexity_Linear
    @discussion         Linear interpolation. lowest quality, cheapest.
						InitialPhase and PrimeMethod properties are not operative with this mode.
    @constant       kAudioConverterSampleRateConverterComplexity_Normal
    @discussion         Normal quality sample rate conversion.
    @constant       kAudioConverterSampleRateConverterComplexity_Mastering
    @discussion         Mastering quality sample rate conversion. More expensive.
    @constant       kAudioConverterSampleRateConverterComplexity_MinimumPhase
    @discussion         Minimum phase impulse response. Stopband attenuation varies with quality setting.
                        The InitialPhase and PrimeMethod properties are not operative with this mode.
                        There are three levels of quality provided.
                            kAudioConverterQuality_Low (or Min)  : noise floor to -96 dB
                            kAudioConverterQuality_Medium        : noise floor to -144 dB
                            kAudioConverterQuality_High (or Max) : noise floor to -160 dB (this uses double precision internally)
						Quality equivalences to the other complexity modes are very roughly as follows:
							MinimumPhase Low    is somewhat better than Normal Medium.
							MinimumPhase Medium is similar to Normal Max.
							MinimumPhase High   is similar to Mastering Low.
						In general, MinimumPhase performs better than Normal and Mastering for the equivalent qualities listed above.
						MinimumPhase High is several times faster than Mastering Low.
 
*/
CF_ENUM(UInt32)
{
    kAudioConverterSampleRateConverterComplexity_Linear             = 'line',   // linear interpolation
    kAudioConverterSampleRateConverterComplexity_Normal             = 'norm',   // normal quality range, the default
    kAudioConverterSampleRateConverterComplexity_Mastering          = 'bats',   // higher quality range, more expensive
    kAudioConverterSampleRateConverterComplexity_MinimumPhase       = 'minp'	// minimum phase impulse response.
};


/*!
    @enum       Prime method constants

    @abstract   Constants to be used with kAudioConverterPrimeMethod.
    
    @constant   kConverterPrimeMethod_Pre
                    Primes with leading + trailing input frames.
    @constant   kConverterPrimeMethod_Normal
                    Only primes with trailing (zero latency). Leading frames are assumed to be
                    silence.
    @constant   kConverterPrimeMethod_None
                    Acts in "latency" mode. Both leading and trailing frames assumed to be
                    silence.
*/
CF_ENUM(UInt32)
{
    kConverterPrimeMethod_Pre       = 0,
    kConverterPrimeMethod_Normal    = 1,
    kConverterPrimeMethod_None      = 2
};

/*!
    @struct     AudioConverterPrimeInfo
    @abstract   Specifies priming information.
    
	When using AudioConverterFillComplexBuffer() (either a single call or a series of calls), some
	conversions, particularly involving sample-rate conversion, ideally require a certain
	number of input frames previous to the normal start input frame and beyond the end of
	the last expected input frame in order to yield high-quality results.
	
	These are expressed in the leadingFrames and trailingFrames members of the structure.
	
	The very first call to AudioConverterFillComplexBuffer(), or first call after
	AudioConverterReset(), will request additional input frames beyond those normally
	expected in the input proc callback to fulfill this first AudioConverterFillComplexBuffer()
	request. The number of additional frames requested, depending on the prime method, will
	be approximately:

	Prime method                  | Additional frames
	------------------------------|----------------------
	kConverterPrimeMethod_Pre     | leadingFrames + trailingFrames
	kConverterPrimeMethod_Normal  | trailingFrames
	kConverterPrimeMethod_None    | 0

	Thus, in effect, the first input proc callback(s) may provide not only the leading
	frames, but also may "read ahead" by an additional number of trailing frames depending
	on the prime method.

	kConverterPrimeMethod_None is useful in a real-time application processing live input,
	in which case trailingFrames (relative to input sample rate) of through latency will be
	seen at the beginning of the output of the AudioConverter.  In other real-time
	applications such as DAW systems, it may be possible to provide these initial extra
	audio frames since they are stored on disk or in memory somewhere and
	kConverterPrimeMethod_Pre may be preferable.  The default method is
	kConverterPrimeMethod_Normal, which requires no pre-seeking of the input stream and
	generates no latency at the output.

    @var        leadingFrames
        Specifies the number of leading (previous) input frames, relative to the normal/desired
        start input frame, required by the converter to perform a high quality conversion. If
        using kConverterPrimeMethod_Pre, the client should "pre-seek" the input stream provided
        through the input proc by leadingFrames. If no frames are available previous to the
        desired input start frame (because, for example, the desired start frame is at the very
        beginning of available audio), then provide "leadingFrames" worth of initial zero frames
        in the input proc.  Do not "pre-seek" in the default case of
        kConverterPrimeMethod_Normal or when using kConverterPrimeMethod_None.

    @var        trailingFrames
        Specifies the number of trailing input frames (past the normal/expected end input frame)
        required by the converter to perform a high quality conversion.  The client should be
        prepared to provide this number of additional input frames except when using
        kConverterPrimeMethod_None. If no more frames of input are available in the input stream
        (because, for example, the desired end frame is at the end of an audio file), then zero
        (silent) trailing frames will be synthesized for the client.
*/
struct AudioConverterPrimeInfo {
    UInt32      leadingFrames;
    UInt32      trailingFrames;
};
typedef struct AudioConverterPrimeInfo AudioConverterPrimeInfo;

/*!	@enum	AudioConverterOptions
	@constant   kAudioConverterOption_Unbuffered
		This is an option for AudioConverterNewWithOptions which removes unnecessary
		buffering, both for input and internally to the converter, saving memory
		at the cost of reduced format support and usage restrictions:
		
		- Input and output formats must be constant bit-rate, non-zero bytes per packet
		  (e.g. linear PCM, a-law, etc.) with the same sample rate and frames per packet.
		- AudioConverterFillBuffer cannot be used.
		- AudioConverterFillComplexBuffer cannot be used.
*/
typedef CF_OPTIONS(UInt32, AudioConverterOptions) {
	kAudioConverterOption_Unbuffered = (1 << 16)
};

//=============================================================================
//  Errors
//=============================================================================

/*!
	@enum AudioConverter errors
*/
CF_ENUM(OSStatus)
{
    kAudioConverterErr_FormatNotSupported       = 'fmt?',
    kAudioConverterErr_OperationNotSupported    = 0x6F703F3F, // 'op??', integer used because of trigraph
    kAudioConverterErr_PropertyNotSupported     = 'prop',
    kAudioConverterErr_InvalidInputSize         = 'insz',
    kAudioConverterErr_InvalidOutputSize        = 'otsz',
        // e.g. byte size is not a multiple of the frame size
    kAudioConverterErr_UnspecifiedError         = 'what',
    kAudioConverterErr_BadPropertySizeError     = '!siz',
    kAudioConverterErr_RequiresPacketDescriptionsError = '!pkd',
    kAudioConverterErr_InputSampleRateOutOfRange    = '!isr',
    kAudioConverterErr_OutputSampleRateOutOfRange   = '!osr'
};

#if TARGET_OS_IPHONE
/*!
    @enum       AudioConverter errors (iPhone only)
    @abstract   iPhone-specific OSStatus results for AudioConverter
    
    @constant   kAudioConverterErr_HardwareInUse
                    Returned from AudioConverterFillComplexBuffer if the underlying hardware codec has
                    become unavailable, probably due to an interruption. In this case, your application
                    must stop calling AudioConverterFillComplexBuffer. If the converter can resume from an
                    interruption (see kAudioConverterPropertyCanResumeFromInterruption), you must
                    wait for an EndInterruption notification from AudioSession, and call AudioSessionSetActive(true)
                    before resuming.
    @constant   kAudioConverterErr_NoHardwarePermission
                    Returned from AudioConverterNew if the new converter would use a hardware codec
                    which the application does not have permission to use.
*/  
CF_ENUM(OSStatus)
{
    kAudioConverterErr_HardwareInUse 		= 'hwiu',
    kAudioConverterErr_NoHardwarePermission = 'perm'
};
#endif

//=============================================================================
//  Routines
//=============================================================================

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterPrepare
    @abstract   Optimizes the subsequent creation of audio converters by the current process.
    @discussion This function performs its work asynchronously.  The optional completion block,
                if provided, is executed once preparation is complete.
                Although a best effort is made to ensure future audio converters will be created quickly,
                there are no guarantees.

    @param      inFlags
                    Reserved for future use.  Pass 0.
    @param      ioReserved
                    Reserved for future use.  Pass NULL.
    @param      inCompletionBlock
                    Optional block to execute once preparation is complete.  May be NULL.
                    The block is given the OSStatus result of the preparation.
    */
extern void
AudioConverterPrepare(  UInt32            inFlags,
                        void * __nullable ioReserved,
                        void (^__nullable inCompletionBlock)(OSStatus))         API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0), visionos(2.0));

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterNew
    @abstract   Create a new AudioConverter.

    @param      inSourceFormat
                    The format of the source audio to be converted.
    @param      inDestinationFormat
                    The destination format to which the audio is to be converted.
    @param      outAudioConverter
                    On successful return, points to a new AudioConverter instance.
    @result     An OSStatus result code.
    
	For a pair of linear PCM formats, the following conversions
	are supported:
	
	<ul>
	<li>addition and removal of channels, when the stream descriptions'
	mChannelsPerFrame does not match. Channels may also be reordered and removed
	using the kAudioConverterChannelMap property.</li>
	<li>sample rate conversion</li>
	<li>interleaving/deinterleaving, when the stream descriptions' (mFormatFlags &
	kAudioFormatFlagIsNonInterleaved) does not match.</li>
	<li>conversion between any pair of the following formats:</li>
		<ul>
		<li>8 bit integer, signed or unsigned</li>
		<li>16, 24, or 32-bit integer, big- or little-endian. Other integral
		bit depths, if high-aligned and non-packed, are also supported</li>
		<li>32 and 64-bit float, big- or little-endian.</li>
		</ul>
	</ul>
	
	Also, encoding and decoding between linear PCM and compressed formats is
	supported. Functions in AudioToolbox/AudioFormat.h return information about the
	supported formats. When using a codec, you can use any supported PCM format (as
	above); the converter will perform any necessary additional conversion between
	your PCM format and the one created or consumed by the codec.
	
	Note that AudioConverter may change the formats to correct any
	inconsistent or erroneous values.  The actual formats expected and used
	by the newly created AudioConverter can be obtained by getting the
	properties `kAudioConverterCurrentInputStreamDescription` and
	`kAudioConverterCurrentOutputStreamDescription` from it.
*/
extern OSStatus
AudioConverterNew(      const AudioStreamBasicDescription * inSourceFormat,
                        const AudioStreamBasicDescription * inDestinationFormat,
                        AudioConverterRef __nullable * __nonnull outAudioConverter)      API_AVAILABLE(macos(10.1), ios(2.0), watchos(2.0), tvos(9.0));


//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterNewSpecific
    @abstract   Create a new AudioConverter using specific codecs.

    @param      inSourceFormat
                    The format of the source audio to be converted.
    @param      inDestinationFormat
                    The destination format to which the audio is to be converted.
    @param      inNumberClassDescriptions
                    The number of class descriptions.
    @param      inClassDescriptions
                    AudioClassDescriptions specifiying the codec to instantiate.
    @param      outAudioConverter
                    On successful return, points to a new AudioConverter instance.
    @result     An OSStatus result code.
    
	This function is identical to AudioConverterNew(), except that the client may
	explicitly choose which codec to instantiate if there is more than one choice.
*/
extern OSStatus
AudioConverterNewSpecific(  const AudioStreamBasicDescription * inSourceFormat,
                            const AudioStreamBasicDescription * inDestinationFormat,
                            UInt32                              inNumberClassDescriptions,
                            const AudioClassDescription *       inClassDescriptions,
                            AudioConverterRef __nullable * __nonnull outAudioConverter)
                                                                                API_AVAILABLE(macos(10.4), ios(2.0), watchos(2.0), tvos(9.0));

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterNewWithOptions
    @abstract   Create a new AudioConverter with one or more options enabled.

    @param      inSourceFormat
                    The format of the source audio to be converted.
    @param      inDestinationFormat
                    The destination format to which the audio is to be converted.
    @param      inOptions
                    Flags selecting one or more optional configurations for the AudioConverter.
    @param      outAudioConverter
                    On successful return, points to a new AudioConverter instance.
    @result     An OSStatus result code.
    
	This is an alternative to AudioConverterNew which supports enabling
	one or more optional configurations for the new AudioConverter.
*/
extern OSStatus
AudioConverterNewWithOptions(const AudioStreamBasicDescription * inSourceFormat,
                             const AudioStreamBasicDescription * inDestinationFormat,
                             AudioConverterOptions inOptions,
                             AudioConverterRef __nullable * __nonnull outAudioConverter)      API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0), visionos(2.0));

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterDispose
    @abstract   Destroy an AudioConverter.

    @param      inAudioConverter
                    The AudioConverter to dispose.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioConverterDispose(  AudioConverterRef   inAudioConverter)                   API_AVAILABLE(macos(10.1), ios(2.0), watchos(2.0), tvos(9.0));

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterReset
    @abstract   Reset an AudioConverter

    @param      inAudioConverter
                    The AudioConverter to reset.
    @result     An OSStatus result code.
    
	Should be called whenever there is a discontinuity in the source audio stream
	being provided to the converter. This will flush any internal buffers in the
	converter.
*/

extern OSStatus
AudioConverterReset(    AudioConverterRef   inAudioConverter)                   API_AVAILABLE(macos(10.1), ios(2.0), watchos(2.0), tvos(9.0));

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterGetPropertyInfo
    @abstract   Returns information about an AudioConverter property.

    @param      inAudioConverter
                    The AudioConverter to query.
    @param      inPropertyID
                    The property to query.
    @param      outSize
                    If non-null, on exit, the maximum size of the property value in bytes.
    @param      outWritable
                    If non-null, on exit, indicates whether the property value is writable.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioConverterGetPropertyInfo(  AudioConverterRef           inAudioConverter,
                                AudioConverterPropertyID    inPropertyID,
                                UInt32 * __nullable         outSize,
                                Boolean * __nullable        outWritable)        API_AVAILABLE(macos(10.1), ios(2.0), watchos(2.0), tvos(9.0));

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterGetProperty
    @abstract   Returns an AudioConverter property value.

    @param      inAudioConverter
                    The AudioConverter to query.
    @param      inPropertyID
                    The property to fetch.
    @param      ioPropertyDataSize
                    On entry, the size of the memory pointed to by outPropertyData. On 
                    successful exit, the size of the property value.
    @param      outPropertyData
                    On exit, the property value.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioConverterGetProperty(  AudioConverterRef           inAudioConverter,
                            AudioConverterPropertyID    inPropertyID,
                            UInt32 *                    ioPropertyDataSize,
                            void *                      outPropertyData)        API_AVAILABLE(macos(10.1), ios(2.0), watchos(2.0), tvos(9.0));

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterSetProperty
    @abstract   Sets an AudioConverter property value.

    @param      inAudioConverter
                    The AudioConverter to modify.
    @param      inPropertyID
                    The property to set.
    @param      inPropertyDataSize
                    The size in bytes of the property value.
    @param      inPropertyData
                    Points to the new property value.
    @result     An OSStatus result code.
*/
extern OSStatus
AudioConverterSetProperty(  AudioConverterRef           inAudioConverter,
                            AudioConverterPropertyID    inPropertyID,
                            UInt32                      inPropertyDataSize,
                            const void *                inPropertyData)         API_AVAILABLE(macos(10.1), ios(2.0), watchos(2.0), tvos(9.0));

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterConvertBuffer
    @abstract   Converts data from an input buffer to an output buffer.

    @param      inAudioConverter
                    The AudioConverter to use.
    @param      inInputDataSize
                    The size of the buffer inInputData.
    @param      inInputData
                    The input audio data buffer.
    @param      ioOutputDataSize
                    On entry, the size of the buffer outOutputData. On exit, the number of bytes
                    written to outOutputData.
    @param      outOutputData
                    The output data buffer.
    @result
                Produces a buffer of output data from an AudioConverter, using the supplied
                input buffer.

	WARNING: this function will fail for any conversion where there is a
	variable relationship between the input and output data buffer sizes. This
	includes sample rate conversions and most compressed formats. In these cases,
	use AudioConverterFillComplexBuffer. Generally this function is only appropriate for
	PCM-to-PCM conversions where there is no sample rate conversion.
*/
extern OSStatus
AudioConverterConvertBuffer(    AudioConverterRef               inAudioConverter,
                                UInt32                          inInputDataSize,
                                const void *                    inInputData,
                                UInt32 *                        ioOutputDataSize,
                                void *                          outOutputData)
                        CA_REALTIME_API
                        API_AVAILABLE(macos(10.1), ios(2.0), watchos(2.0), tvos(9.0));

//-----------------------------------------------------------------------------
/*!
    @typedef    AudioConverterComplexInputDataProc
    @abstract   Callback function for supplying input data to AudioConverterFillComplexBuffer.

    @param      inAudioConverter
                    The AudioConverter requesting input.
    @param      ioNumberDataPackets
                    On entry, the minimum number of packets of input audio data the converter
                    would like in order to fulfill its current FillBuffer request. On exit, the
                    number of packets of audio data actually being provided for input, or 0 if
                    there is no more input.
    @param      ioData
                    This points to an audio buffer list to be filled in by the callback to refer to the
                    buffer(s) provided by the callback.
                    On exit, the members of ioData should be set to point to the audio data
                    being provided for input.
    @param      outDataPacketDescription
                    If non-null, on exit, the callback is expected to fill this in with
                    an AudioStreamPacketDescription for each packet of input data being provided.
    @param      inUserData
                    The inInputDataProcUserData parameter passed to AudioConverterFillComplexBuffer().
    @result     An OSStatus result code.
    
	This callback function supplies input to AudioConverterFillComplexBuffer.
	
	The AudioConverter requests a minimum number of packets (*ioNumberDataPackets).
	The callback may return one or more packets. If this is less than the minimum,
	the callback will simply be called again in the near future. Note that ioNumberDataPackets counts
	packets in terms of the converter's input format (not its output format).
	Also note that the callback must provide a whole number of packets.

	The callback may be asked to provide multiple input packets in a single call, even for compressed
	formats.  The callback must update the number of packets pointed to by ioNumberDataPackets
	to indicate the number of packets actually being provided, and if the packets require packet
	descriptions, these must be filled into the array pointed to by outDataPacketDescription, one
	packet description per packet.

	The callback is given an audio buffer list pointed to by ioData.  This buffer list may refer to
	existing buffers owned and allocated by the audio converter, in which case the callback may
	use them and copy input audio data into them.  However, the buffer list may also be empty
	(mDataByteSize == 0 and/or mData == NULL), in which case the callback must provide its own
	buffers.  The callback manipulates the members of ioData to point to one or more buffers
	of audio data (multiple buffers are used with non-interleaved PCM data). The
	callback is responsible for not freeing or altering this buffer until it is called again.

	For input data that varies from one packet to another in either size (bytes per packet)
	or duration (frames per packet), such as when decoding compressed audio, the callback
	should expect outDataPacketDescription to be non-null and point to array of packet descriptions,
	which the callback must fill in, one for every packet provided by the callback.  Each packet must
	have a valid packet description, regardless of whether or not these descriptions are different
	from each other.  Packet descriptions are required even if there is only one packet.

	If the callback returns an error, it must return zero packets of data.
	AudioConverterFillComplexBuffer will stop producing output and return whatever
	output has already been produced to its caller, along with the error code. This
	mechanism can be used when an input proc has temporarily run out of data, but
	has not yet reached end of stream.
*/
typedef OSStatus
(*AudioConverterComplexInputDataProc)(  AudioConverterRef               inAudioConverter,
                                        UInt32 *                        ioNumberDataPackets,
                                        AudioBufferList *               ioData,
                                        AudioStreamPacketDescription * __nullable * __nullable outDataPacketDescription,
                                        void * __nullable               inUserData);

/*!
	@typedef	AudioConverterComplexInputDataProcRealtimeSafe
	@abstract	Realtime-safe variant of AudioConverterComplexInputDataProc.
	
	See the discussions of AudioConverterComplexInputDataProc and AudioConverterFillComplexBuffer.
*/
typedef OSStatus
(*AudioConverterComplexInputDataProcRealtimeSafe)(
                                        AudioConverterRef               inAudioConverter,
                                        UInt32 *                        ioNumberDataPackets,
                                        AudioBufferList *               ioData,
                                        AudioStreamPacketDescription * __nullable * __nullable outDataPacketDescription,
                                        void * __nullable               inUserData) CA_REALTIME_API;

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterFillComplexBuffer
    @abstract   Converts data supplied by an input callback function, supporting non-interleaved
                and packetized formats.

    @param      inAudioConverter
                    The AudioConverter to use.
    @param      inInputDataProc
                    A callback function which supplies the input data.
    @param      inInputDataProcUserData
                    A value for the use of the callback function.
    @param      ioOutputDataPacketSize
                    On entry, the capacity of outOutputData expressed in packets in the
                    converter's output format. On exit, the number of packets of converted
                    data that were written to outOutputData.
    @param      outOutputData
                    The converted output data is written to this buffer. On entry, the buffers'
                    mDataByteSize fields (which must all be the same) reflect buffer capacity.
                    On exit, mDataByteSize is set to the number of bytes written.
    @param      outPacketDescription
                    If non-null, and the converter's output uses packet descriptions, then
                    packet descriptions are written to this array. It must point to a memory
                    block capable of holding *ioOutputDataPacketSize packet descriptions.
                    (See AudioFormat.h for ways to determine whether an audio format
                    uses packet descriptions).
    @result     An OSStatus result code.

	Produces a buffer list of output data from an AudioConverter. The supplied input
	callback function is called whenever necessary.
	
	If the output format uses packet descriptions, such as most compressed formats where packets
	vary in size or duration, the caller is expected to provide a buffer for holding packet descriptions,
	pointed to by outPacketDescription.  The array must have the capacity to hold a packet description
	for each output packet that may be written.  A packet description array is expected even if only
	a single output packet is to be written.
*/
extern OSStatus
AudioConverterFillComplexBuffer(    AudioConverterRef                   inAudioConverter,
                                    AudioConverterComplexInputDataProc  inInputDataProc,
                                    void * __nullable                   inInputDataProcUserData,
                                    UInt32 *                            ioOutputDataPacketSize,
                                    AudioBufferList *                   outOutputData,
                                    AudioStreamPacketDescription * __nullable outPacketDescription)
                                                                                API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function   AudioConverterFillComplexBufferRealtimeSafe
    @abstract   Identical to AudioConverterFillComplexBuffer, with the addition of a realtime-safety
    			guarantee.
	
	Conversions involving only PCM formats -- interleaving, deinterleaving, channel count changes,
	sample rate conversions -- are realtime-safe. Such conversions may use this API in order to
	obtain compiler checks involving the `CA_REALTIME_API` attributes.
	
	At runtime, this function returns `kAudioConverterErr_OperationNotSupported` if the conversion 
	requires non-realtime-safe functionality.
*/
extern OSStatus
AudioConverterFillComplexBufferRealtimeSafe(
                                    AudioConverterRef                   inAudioConverter,
                                    AudioConverterComplexInputDataProcRealtimeSafe inInputDataProc,
                                    void * __nullable                   inInputDataProcUserData,
                                    UInt32 *                            ioOutputDataPacketSize,
                                    AudioBufferList *                   outOutputData,
                                    AudioStreamPacketDescription * __nullable outPacketDescription)
                                        CA_REALTIME_API
                                        API_AVAILABLE(macos(26.0), ios(26.0), watchos(26.0), tvos(26.0), visionos(26.0));

/*!
    @function   AudioConverterFillComplexBufferWithPacketDependencies
    @abstract   Converts audio data supplied by a callback function, supporting non-interleaved and
                packetized formats, and also supporting packet dependency descriptions.
    @discussion For output formats that use packet dependency descriptions, this must be used instead of
                AudioConverterFillComplexBuffer, which will return an error for such formats.
    @param inAudioConverter         The audio converter to use for format conversion.
    @param inInputDataProc          A callback function that supplies audio data to convert.
                                    This callback is invoked repeatedly as the converter is ready for
                                    new input data.
    @param inInputDataProcUserData  Custom data for use by your application when receiving a
                                    callback invocation.
    @param ioOutputDataPacketSize   On input, the size of the output buffer (in the `outOutputData`
                                    parameter), expressed in number packets in the audio converter’s
                                    output format.  On output, the number of packets of converted data
                                    that were written to the output buffer.
    @param outOutputData            The converted output data is written to this buffer. On entry, the
                                    buffers' `mDataByteSize` fields (which must all be the same) reflect
                                    buffer capacity.  On exit, `mDataByteSize` is set to the number of
                                    bytes written.
    @param outPacketDescriptions    If not `NULL`, and if the audio converter's output format uses packet
                                    descriptions, this must point to a block of memory capable of holding
                                    the number of packet descriptions specified in the `ioOutputDataPacketSize`
                                    parameter.  (See _Audio Format Services Reference_ for functions that
                                    let you determine whether an audio format uses packet descriptions).
                                    If not `NULL` on output and if the audio converter's output format
                                    uses packet descriptions, then this parameter contains an array of
                                    packet descriptions.
    @param outPacketDependencies    Should point to a memory block capable of holding the number of
                                    packet dependency description structures specified in the
                                    `ioOutputDataPacketSize` parameter.  Must not be `NULL`.  This array
                                    will be filled out only by encoders that produce a format which has a
                                    non-zero value for `kAudioFormatProperty_FormatEmploysDependentPackets`.
    @result                         A result code.
*/
OSStatus
AudioConverterFillComplexBufferWithPacketDependencies(
    AudioConverterRef                            inAudioConverter,
    AudioConverterComplexInputDataProc           inInputDataProc,
    void * __nullable                            inInputDataProcUserData,
    UInt32 *                                     ioOutputDataPacketSize,
    AudioBufferList *                            outOutputData,
    AudioStreamPacketDescription * __nullable    outPacketDescriptions,
    AudioStreamPacketDependencyDescription *     outPacketDependencies)
API_AVAILABLE(macos(26.0), ios(26.0), watchos(26.0), tvos(26.0), visionos(26.0));

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterConvertComplexBuffer
    @abstract   Converts PCM data from an input buffer list to an output buffer list.

    @param      inAudioConverter
                    The AudioConverter to use.
    @param      inNumberPCMFrames
                    The number of PCM frames to convert.
    @param      inInputData
                    The source audio buffer list.
    @param      outOutputData
                    The converted output data is written to this buffer list.
    @result     An OSStatus result code.

	@warning	This function will fail for any conversion where there is a
	variable relationship between the input and output data buffer sizes. This
	includes sample rate conversions and most compressed formats. In these cases,
	use AudioConverterFillComplexBuffer. Generally this function is only appropriate for
	PCM-to-PCM conversions where there is no sample rate conversion.
*/
extern OSStatus
AudioConverterConvertComplexBuffer( AudioConverterRef               inAudioConverter,
                                    UInt32                          inNumberPCMFrames,
                                    const AudioBufferList *         inInputData,
                                    AudioBufferList *               outOutputData)
                                    	CA_REALTIME_API
										API_AVAILABLE(macos(10.7), ios(5.0), watchos(2.0), tvos(9.0));

// =================================================================================================
// DEPRECATED
// =================================================================================================

/*
	Deprecated properties:
	
    @constant   kAudioConverterPropertyMaximumInputBufferSize
                    DEPRECATED. The AudioConverter input proc may be passed any number of packets of data.
                    If fewer are packets are returned than required, then the input proc will be called again.
                    If more packets are passed than required, they will remain in the client's buffer and be 
                    consumed as needed.
    @constant   kAudioConverterSampleRateConverterAlgorithm
                    DEPRECATED: please use kAudioConverterSampleRateConverterComplexity instead
	
*/
CF_ENUM(AudioConverterPropertyID)
{
    kAudioConverterPropertyMaximumInputBufferSize       = 'xibs',
    kAudioConverterSampleRateConverterAlgorithm         = 'srci',
};

#if TARGET_OS_IPHONE
/*!
    @enum       AudioConverterPropertyID (iOS only)
    @abstract   iOS-specific properties of an AudioConverter, accessible via AudioConverterGetProperty()
                and AudioConverterSetProperty().
 
    @constant   kAudioConverterPropertyCanResumeFromInterruption
                    A read-only UInt32 signifying whether the underlying codec supports resumption following
                    an interruption. If the property is unimplemented (i.e. AudioConverterGetProperty
                    returns an error), then the codec is not a hardware codec. If the property's value
                    is 1, then the codec can resume work following an interruption. If the property's
                    value is 0, then interruptions destroy the codec's state.
                    
                    DEPRECATED: Hardware codecs are no longer supported.
*/
CF_ENUM(AudioConverterPropertyID)
{
    kAudioConverterPropertyCanResumeFromInterruption    = 'crfi'
};
#endif

//-----------------------------------------------------------------------------
/*!
    @typedef    AudioConverterInputDataProc
    @abstract   Callback function for supplying input data to AudioConverterFillBuffer.

    @param      inAudioConverter
                    The AudioConverter requesting input.
    @param      ioDataSize
                    On entry, the minimum number of bytes of audio data the converter
                    would like in order to fulfill its current FillBuffer request.
                    On exit, the number of bytes of audio data actually being provided
                    for input, or 0 if there is no more input.
    @param      outData
                    On exit, *outData should point to the audio data being provided
                    for input.
    @param      inUserData
                    The inInputDataProcUserData parameter passed to AudioConverterFillBuffer().
    @result     An OSStatus result code.
    
	This callback function supplies input to AudioConverterFillBuffer.
	
	The AudioConverter requests a minimum amount of data (*ioDataSize). The callback
	may return any amount of data. If it is less than than the minimum, the callback
	will simply be called again in the near future.

	The callback supplies a pointer to a buffer of audio data. The callback is
	responsible for not freeing or altering this buffer until it is called again.
	
	If the callback returns an error, it must return zero bytes of data.
	AudioConverterFillBuffer will stop producing output and return whatever output
	has already been produced to its caller, along with the error code. This
	mechanism can be used when an input proc has temporarily run out of data, but
	has not yet reached end of stream.

	@deprecated This API is now deprecated,  use AudioConverterFillComplexBuffer instead.
*/
typedef OSStatus
(*AudioConverterInputDataProc)( AudioConverterRef           inAudioConverter,
                                UInt32 *                    ioDataSize,
                                void * __nonnull * __nonnull outData,
                                void * __nullable           inUserData);

//-----------------------------------------------------------------------------
/*!
    @function   AudioConverterFillBuffer
    @abstract   Converts data supplied by an input callback function.

    @param      inAudioConverter
                    The AudioConverter to use.
    @param      inInputDataProc
                    A callback function which supplies the input data.
    @param      inInputDataProcUserData
                    A value for the use of the callback function.
    @param      ioOutputDataSize
                    On entry, the size of the buffer pointed to by outOutputData.
                    On exit, the number of bytes written to outOutputData
    @param      outOutputData
                    The buffer into which the converted data is written.
    @result     An OSStatus result code.
    
	Produces a buffer of output data from an AudioConverter. The supplied input
	callback function is called whenever necessary.             

	@deprecated This API is now deprecated,  use AudioConverterFillComplexBuffer instead.
*/
#if !TARGET_OS_IPHONE
extern OSStatus
AudioConverterFillBuffer(   AudioConverterRef               inAudioConverter,
                            AudioConverterInputDataProc     inInputDataProc,
                            void * __nullable               inInputDataProcUserData,
                            UInt32 *                        ioOutputDataSize,
                            void *                          outOutputData)
                            
                                API_DEPRECATED("no longer supported", macos(10.1, 10.5)) API_UNAVAILABLE(ios, watchos, tvos);
#endif // !TARGET_OS_IPHONE

#if defined(__cplusplus)
}
#endif

CF_ASSUME_NONNULL_END

#endif // AudioToolbox_AudioConverter_h
#else
#include <AudioToolboxCore/AudioConverter.h>
#endif
