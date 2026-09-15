#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioCodec.h>)
/*==================================================================================================
     File:       AudioToolbox/AudioCodec.h

     Contains:   A component API for encoding/decoding audio data.

     Copyright:  (c) 1985-2015 by Apple, Inc., all rights reserved.

     Bugs?:      For bug reports, consult the following page on
                 the World Wide Web:

                     http://developer.apple.com/bugreporter/

==================================================================================================*/
#ifndef AudioUnit_AudioCodec_h
#define AudioUnit_AudioCodec_h

/*!
	@header AudioCodec
 
	This header defines the property sets and the public API for various audio codecs.

	<h2>Theory of Operation</h2>
 
	AudioCodec components translate audio data from one format to another. There
	are three kinds of AudioCodec components. Decoder components ('adec') 
	translate data that isn't in linear PCM into linear PCM formatted data. 
	Encoder components ('aenc') translate linear PCM data into some other format. 
	Unity codecs ('acdc') translate between different flavors of the same type 
	(e.g. 16 bit signed integer linear PCM into 32 bit floating point linear PCM).
 
	AudioCodec components are standard components and are managed by the Component
	Manager.
 
	Once an AudioCodec is found that implements the translation in question,
	it has to be set up to do the translation. This can be done by setting the
	appropriate properties or by calling AudioCodecInitialize. If the translation
	is specified by properties, AudioCodecInitialize still needs to be called
	prior to appending input data or producing output data.
 
	AudioCodecInitialize puts the codec into the "initialized" state. In this state,
	the format information for the translation cannot be changed. The codec
	has to be in the initialized state for AudioCodecAppendInputData and
	AudioCodecProduceOutputData to work. They will return kAudioCodecStateError
	if the codec isn't initialized.
 
	AudioCodecUninitialize will return the codec to the uninitialized state and
	release any allocated resources. The codec may then be configured freely. It is not
	necessary to call AudioCodecUninitialize prior to closing the codec.
 
	Once in the initialized state, the codec is ready to receive input and produce
	output using the AudioCodecAppendInputData and AudioCodecProduceOutputData
	routines. Input data can be fed into an encoder and some decoders in any size (even 
	byte by byte). Input data fed to a decoder should be in terms of whole packets in the 
	encoded format if the format is variable bit rate and is not self framing (e.g. MPEG-4 AAC). 
	Output data can only be produced in whole packet sizes. Both routines will return 
	the amount of data they consume/produce.
 
	AudioCodecProduceOutputData also returns a status code to the caller that
	indicates the result of the operation (success or failure) as well as the
	state of the input buffer.
	
	The combination of AppendInputData and ProduceOutputPackets can be thought of a "push-pull"
	model of data handling. First, the input data is pushed into the component and the 
	resulting output data gets pulled out of that same component.
 
	Basic Workflow
	1. Find the appropriate codec component
	2. Open the codec component
	3. Configure it (AudioCodecGetPropertyInfo, AudioCodecGetProperty, AudioCodecSetProperty)
	4. AudioCodecInitialize
	5. Loop
		a. AppendInputData (EOF is signaled by passing a 0-sized buffer)
		b. ProduceOutputPackets
	6. Close the codec component
	
 */

//=============================================================================

#include <TargetConditionals.h>
#include <Availability.h>

#include <AudioToolbox/AudioComponent.h>

#if defined(__cplusplus)
extern "C"
{
#endif

CF_ASSUME_NONNULL_BEGIN

//=============================================================================
#pragma mark Types specific to AudioCodecs
//=============================================================================


typedef AudioComponentInstance	AudioCodec;
typedef UInt32					AudioCodecPropertyID;

/*!
    @struct AudioCodecMagicCookieInfo
 
	@abstract Structure holding the <em>magic cookie</em> information.
 
	@discussion Passed as input to AudioCodecGetProperty for kAudioCodecPropertyFormatList.
				The first four + sizeof(void *) bytes of the buffer pointed at by outPropertyData
				will contain this struct.
 
	@var   mMagicCookieSize
        The size of the magic cookie
	@var   mMagicCookie
        Generic const pointer to magic cookie
*/
struct AudioCodecMagicCookieInfo 
{
	UInt32					mMagicCookieSize;
	const void* __nullable	mMagicCookie;
};
typedef struct AudioCodecMagicCookieInfo	AudioCodecMagicCookieInfo;

//=============================================================================
#pragma mark AudioCodec Component Constants
//=============================================================================


#if !TARGET_OS_IPHONE
/*!
	@enum           AudioCodecComponentType
 
	@discussion     Collection of audio codec component types
 
	@constant		kAudioDecoderComponentType
					A codec that translates data in some other format into linear PCM.
					The component subtype specifies the format ID of the other format.
	@constant		kAudioEncoderComponentType
					A codec that translates linear PCM data into some other format
					The component subtype specifies the format ID of the other format
	@constant		kAudioUnityCodecComponentType
					A codec that translates between different flavors of the same format
					The component subtype specifies the format ID of this format.
*/
CF_ENUM(UInt32)
{
	kAudioDecoderComponentType								= 'adec',	
	kAudioEncoderComponentType								= 'aenc',	
	kAudioUnityCodecComponentType							= 'acdc'
};
#endif

//=============================================================================
#pragma	mark Global Codec Properties

//	Used with the AudioCodecXXXXPropertyXXXX family of routines.
//	All Audio Codec properties are readable only.
//=============================================================================

/*!
	@enum		AudioCodecGlobalProperty

	@discussion	These properties reflect the capabilities of the underlying codec.
				The values of these properties are independent of the codec's internal
				state.
				
				These properties can be read at any time the codec is open.

	@constant	kAudioCodecPropertySupportedInputFormats
					An array of AudioStreamBasicDescription structs describing what formats 
					the codec supports for input data
	@constant	kAudioCodecPropertySupportedOutputFormats
					An array of AudioStreamBasicDescription structs describing what formats 
					the codec supports for output data
 	@constant	kAudioCodecPropertyAvailableInputSampleRates
					An array of AudioValueRange indicating the valid ranges for the
					input sample rate of the codec.
					Required for encoders.
					(see also kAudioCodecPropertyApplicableInputSampleRates)
	@constant	kAudioCodecPropertyAvailableOutputSampleRates
					An array of AudioValueRange indicating the valid ranges for the
					output sample rate of the codec.
					Required for encoders.
					(see also kAudioCodecPropertyApplicableOutputSampleRates)
	@constant	kAudioCodecPropertyAvailableBitRateRange
					An array of AudioValueRange that indicate the target bit rates
					supported by the encoder. This can be total bit rate or bit
					rate per channel as appropriate. 
					This property is only relevant to encoders.
					(see also kAudioCodecPropertyApplicableBitRateRange)
	@constant	kAudioCodecPropertyMinimumNumberInputPackets
					A UInt32 indicating the minimum number of input packets
					that need to be supplied to the codec. The actual input the
					codec accepts could be less than this.
					For most codecs this value will be 1.
	@constant	kAudioCodecPropertyMinimumNumberOutputPackets
					A UInt32 indicating the minimum number of output packets
					that need to be handled from the codec. The actual output
					might be less than this.
					For most codecs this value will be 1.
	@constant	kAudioCodecPropertyAvailableNumberChannels
					An array of UInt32 that specifies the number of channels the codec is
					capable of encoding or decoding to. 0xFFFFFFFF means any number
					of channels.
	@constant	kAudioCodecPropertyDoesSampleRateConversion
					A UInt32 indicating if the codec wants to do a sample rate conversion (if 
					necessary) because it can do it in a way that is meaningful for quality.
					Value is 1 if true, 0 otherwise.
	@constant	kAudioCodecPropertyAvailableInputChannelLayoutTags
					An array of AudioChannelLayoutTag that specifies what channel layouts the codec is
					capable of using on input.
	@constant	kAudioCodecPropertyAvailableOutputChannelLayoutTags
					An array of AudioChannelLayoutTag that specifies what channel layouts the codec is
					capable of using on output.
	@constant	kAudioCodecPropertyInputFormatsForOutputFormat
					An array of AudioStreamBasicDescription indicating what the codec supports
					for input data given an output format that's passed in as the first member of
					the array (and is overwritten on the reply). Always a subset of 
					kAudioCodecPropertySupportedInputFormats
	@constant	kAudioCodecPropertyOutputFormatsForInputFormat
					An array of AudioStreamBasicDescription indicating what the codec supports
					for output data given an input format that's passed in as the first member of
					the array (and is overwritten on the reply). Always a subset of 
					kAudioCodecPropertySupportedOutputFormats
	@constant	kAudioCodecPropertyFormatInfo
					Takes an AudioFormatInfo on input. This AudioformatInfo is validated either through
					the provided magic cookie or the AudioStreamBasicDescription and where applicable,
					wildcards are overwritten with default values.
*/
CF_ENUM(AudioCodecPropertyID)
{
	kAudioCodecPropertySupportedInputFormats				= 'ifm#',
	kAudioCodecPropertySupportedOutputFormats				= 'ofm#',
	kAudioCodecPropertyAvailableInputSampleRates			= 'aisr',
	kAudioCodecPropertyAvailableOutputSampleRates			= 'aosr',
	kAudioCodecPropertyAvailableBitRateRange				= 'abrt',
	kAudioCodecPropertyMinimumNumberInputPackets			= 'mnip',
	kAudioCodecPropertyMinimumNumberOutputPackets			= 'mnop',
	kAudioCodecPropertyAvailableNumberChannels				= 'cmnc',
	kAudioCodecPropertyDoesSampleRateConversion				= 'lmrc',
	kAudioCodecPropertyAvailableInputChannelLayoutTags		= 'aicl',
	kAudioCodecPropertyAvailableOutputChannelLayoutTags		= 'aocl',
	kAudioCodecPropertyInputFormatsForOutputFormat			= 'if4o',		
	kAudioCodecPropertyOutputFormatsForInputFormat			= 'of4i',
	kAudioCodecPropertyFormatInfo							= 'acfi'
};

//=============================================================================
#pragma	mark Instance Codec Properties

//	Used with the AudioCodecXXXXPropertyXXXX family of routines.
//=============================================================================

/*!
	@enum			AudioCodecInstanceProperty
 
	@discussion		Properties which can be set or read on an instance of the
					underlying audio codec. These properties are dependent on the 
					codec's current state. A property may be read/write or read
					only, depending on the data format of the codec.
					
					These properties may have different values depending on whether the
					codec is initialized or not. All properties can be read at any time
					the codec is open. However, to ensure the codec is in a valid 
					operational state and therefore the property value is valid the codec
					must be initialized at the time the property is read.
					
					Properties that are writable are only writable when the codec
					is not initialized.
 
	@constant		kAudioCodecPropertyInputBufferSize
						A UInt32 indicating the maximum input buffer size for the codec
						in bytes. 
						Not writable, but can vary on some codecs depending on the bit stream 
						format being handled.
	@constant		kAudioCodecPropertyPacketFrameSize
						A UInt32 indicating the number of frames of audio data encapsulated in each
						packet of data in the codec's format. For encoders, this is the
						output format. For decoders this is the input format.
						Formats with variable frames per packet should return a maximum value 
						for this property.
						Not writable.
	@constant		kAudioCodecPropertyHasVariablePacketByteSizes
						A UInt32 where 0 indicates that all packets in the codec's format
						have the same byte size (sometimes referred to as CBR codecs),
						and 1 indicates that they vary in size (sometimes referred to as 
						VBR codecs). The maximum size of a variable packet is up to 
						the one indicated in kAudioCodecPropertyMaximumPacketByteSize.
						Any codec that reports 1 for this property must be able to handle packet
						descriptions, though it does not have to require them.
						May be writable.
	@constant		kAudioCodecPropertyEmploysDependentPackets
						A UInt32 where 0 indicates that all packets in the codec's format
						are independently decodable, and 1 indicates that some may not be
						independently decodable.
	@constant		kAudioCodecPropertyMaximumPacketByteSize
						A UInt32 indicating the maximum number of bytes a packet of data
						in the codec's format will be. If the format is constant bit rate,
						all packets will be this size. If it is variable bit rate, the packets
						will never exceed this size.
						This always refers to the encoded data, so for encoders it refers to the
						output data and for decoders the input data.
						Not writable.
	@constant		kAudioCodecPropertyPacketSizeLimitForVBR
                        A UInt32 indicating the maximum number of bits in an output packet of an encoder.
                        The output packet size will not exceed this number. The size should be smaller 
                        than kAudioCodecPropertyMaximumPacketByteSize. This property will configure the 
                        encoder to VBR mode with the highest VBR quality that can maintain the packet 
                        size limit. kAudioCodecPropertySoundQualityForVBR can be used to retrieve the 
                        quality setting that will be used given that packet size limit.
                        Writeable if supported.
	@constant		kAudioCodecPropertyCurrentInputFormat
						An AudioStreamBasicDescription describing the format the codec
						expects its input data in
						Almost always writable, but if the codec only supports one unique input format
						it does not have to be
	@constant		kAudioCodecPropertyCurrentOutputFormat
						An AudioStreamBasicDescription describing the format the codec
						provides its output data in
						Almost always writable, but if the codec only supports one unique output format
						it does not have to be
	@constant		kAudioCodecPropertyMagicCookie
						An untyped buffer of out of band configuration data the codec
						requires to process the stream of data correctly. The contents
						of this data is private to the codec. 
						Not all codecs have magic cookies. If a call to AudioCodecGetPropertyInfo
						returns a size greater than 0 then the codec may take one.
						Writable if present.
	@constant		kAudioCodecPropertyUsedInputBufferSize
						A UInt32 indicating the number of bytes in the codec's input
						buffer. The amount of available buffer space is	simply the
						answer from kAudioCodecPropertyInputBufferSize minus the answer
						from this property.
						Not writable.
	@constant		kAudioCodecPropertyIsInitialized
						A UInt32 where 0 means the codec is uninitialized and anything
						else means the codec is initialized. This should never be settable directly.
						Must be set by AudioCodecInitialize and AudioCodecUninitialize.
	@constant		kAudioCodecPropertyCurrentTargetBitRate
						A UInt32 containing the number of bits per second to aim for when encoding
						data. This property is usually only relevant to encoders, but if a decoder
						can know what bit rate it's set to it may report it.
						This property is irrelevant if the encoder is configured as kAudioCodecBitRateControlMode_Variable.
						Writable on encoders if supported.
	@constant		kAudioCodecPropertyCurrentInputSampleRate
						A Float64 containing the current input sample rate in Hz. No Default.
						May be writable. If only one sample rate is supported it does not have to be.
	@constant		kAudioCodecPropertyCurrentOutputSampleRate
						A Float64 containing the current output sample rate in Hz. No Default.
						May be writable. If only one sample rate is supported it does not have to be.
	@constant		kAudioCodecPropertyQualitySetting
						A UInt32 that sets the tradeoff between sound quality and CPU time consumption.
						The property value is between [0 - 0x7F].
						Some enum constants are defined below for convenience.
						Writable if supported.
	@constant		kAudioCodecPropertyApplicableBitRateRange
						An array of AudioValueRange indicating the target bit rates
						supported by the encoder in its current configuration.
						This property is only relevant to encoders.
						See also kAudioCodecPropertyAvailableBitRateRange.
						Not writable.
    @constant		kAudioCodecPropertyRecommendedBitRateRange
                        An array of AudioValueRange indicating the recommended bit rates
                        at given sample rate.
                        This property is only relevant to encoders.
                        Not writable.
	@constant		kAudioCodecPropertyApplicableInputSampleRates
						An array of AudioValueRange indicating the valid ranges for the
						input sample rate of the codec for the current bit rate. 
						This property is only relevant to encoders.
						See also kAudioCodecPropertyAvailableInputSampleRates.
						Not writable.
	@constant		kAudioCodecPropertyApplicableOutputSampleRates
						An array of AudioValueRange indicating the valid ranges for the
						output sample rate of the codec for the current bit rate. 
						This property is only relevant to encoders.
						See also kAudioCodecPropertyAvailableOutputSampleRates.
						Not writable.
	@constant		kAudioCodecPropertyPaddedZeros
						A UInt32 indicating the number of zeros (samples) that were appended
						to the last packet of input data to make a complete packet encoding.
						Encoders only. No default.
						Not writable.
	@constant		kAudioCodecPropertyPrimeMethod
						A UInt32 specifying priming method.
						See enum below.
						May be writable. Some encoders offer the option of padding out the last packet, and this 
						may be set here.
	@constant		kAudioCodecPropertyPrimeInfo
						A pointer to an AudioCodecPrimeInfo struct.
						Not writable on encoders. On decoders this may be writable, telling the decoder to trim the
						first and/or last packet.
	@constant		kAudioCodecPropertyCurrentInputChannelLayout
						An AudioChannelLayout that specifies the channel layout that the codec is using for input.
						May be writable. If only one channel layout is supported it does not have to be.
	@constant		kAudioCodecPropertyCurrentOutputChannelLayout
						An AudioChannelLayout that specifies the channel layout that the codec is using for output.
						If settable on a encoder, it means the encoder can re-map channels
						May be writable. If only one channel layout is supported or the codec does no channel remapping
						(ie, output channel layout always equals the input channel layout) it does not have to be.
	@constant		kAudioCodecPropertySettings
						A CFDictionaryRef that lists both the settable codec settings and their values.
						Encoders only.
						Obviously this will be linked to many of the other properties listed herein and as such
						it potentially will cause synchronization problems. Therefore, when setting this property
						on an encoder a GetProperty should be done first to retrieve the current dictionary, 
						and only one setting within the dictionary should change with each SetProperty call, 
						as it is not guaranteed that changing one property will not have side effects.
						Writable if supported.
	@constant		kAudioCodecPropertyBitRateControlMode
						A UInt32 indicating which bit rate control mode will be applied to encoders that 
						can produce variable packet sizes (sometimes referred to as VBR encoders).
						Although the packet size may be variable, a constant bit rate can be maintained 
						over a transmission channel when decoding in real-time with a fixed end-to-end audio delay. 
						E.g., MP3 and MPEG-AAC use a bit reservoir mechanism to meet that constraint.
						See enum below. 
						Only needs to be settable if the codec supports multiple bit rate control strategies.
	@constant		kAudioCodecPropertyFormatList
						An array of AudioFormatListItem structs list all formats that can be handled by the decoder
						For decoders, takes a Magic Cookie that gets passed in on the GetProperty call. No default.
						On input, the outPropertyData parameter passed to GetProperty should begin with a 
						AudioCodecMagicCookieInfo struct which will be overwritten by the AudioFormatListItems 
						returned from the property. For encoders, returns a list of formats which will be in the
						bitstream. No input data required.
						Important note: this encoder property is only applicable to audio formats which are made of
						two or more layers where the base layers(s) can be decoded by systems which aren't capable of
						handling the enhancement layers. For example, a High Efficiency AAC bitstream which contains 
						an AAC Low Complexity base layer can be decoded by any AAC decoder.
	@constant		kAudioCodecPropertySoundQualityForVBR
						A UInt32 that sets a target sound quality level.
						Unlike kAudioCodecPropertyQualitySetting which is relevant to all BitRate Control Modes,
						this property only needs to be set by an encoder configured at kAudioCodecBitRateControlMode_Variable.
						The property value is between [0 - 0x7F].
						See also kAudioCodecPropertyQualitySetting
						Writable if supported.
    @constant       kAudioCodecPropertyBitRateForVBR
                        A UInt32 that can be used to set the target bit rate when the encoder is configured
                        for VBR mode (kAudioCodecBitRateControlMode_Variable). Writable if supported.
    @constant		kAudioCodecPropertyDelayMode
                        A UInt32 specifying the delay mode. See enum below.                        
                        Writable if supported.
	@constant		kAudioCodecPropertyAdjustLocalQuality
						An SInt32 number in the range [-128, 127] to allow encoding quality adjustements on a packet by packet basis.
						This property can be set on an initialized encoder object without having to uninitialize and re-intialize it
						and allows to adjust the encoder quality level for every packet. This is useful for packets streamed over
						unreliable IP networks where the encoder needs to adapt immediately to network condition changes.
						Escape property ID's start with a '^' symbol as the first char code. This bypasses the initilization check.
    @constant		kAudioCodecPropertyDynamicRangeControlMode
						A UInt32 specifying the decoder DRC mode. Supported modes are defined as enum with the
						prefix kDynamicRangeControlMode (see below). For certain legacy metadata this property controls which
						dynamic range compression scheme is applied if the information is present in
						the bitstream. The default is kDynamicRangeControlMode_None.
    @constant        kAudioCodecPropertyAdjustCompressionProfile
                        A UInt32 specifying the dynamic range compression profile to be applied in the decoder. Profiles are
                        based on the DRC Effect Types in ISO/IEC 23003-4. Supported profiles are defined as DynamicRangeCompressionProfile
                        enum (see below). The default profile is kDynamicRangeCompressionProfile_None.
                        This property can also be set on an initialized decoder object. It will be applied immediately, if supported.
    @constant        kAudioCodecPropertyProgramTargetLevelConstant
                        A UInt32 matching one of the values in the ProgramTargetLevel enumeration, which specifies the
                        program target loudness in LKFS for decoders.
                        This property controls the loudness at the decoder output when supported. It is typically
                        used for loudness normalization. The default is kProgramTargetLevel_None.
    @constant        kAudioCodecPropertyAdjustTargetLevelConstant
                        A UInt32 specifying the program target loudness in LKFS for decoders. It has the same effect
                        as kAudioCodecPropertyProgramTargetLevelConstant, but this property can also be set on an
                        initialized decoder object. It will be applied immediately, if supported.
                        A value of kProgramTargetLevel_None removes a prior target level setting.
    @constant        kAudioCodecPropertyProgramTargetLevel
                        A Float32 specifying the program target loudness in LKFS for decoders.
                        This property controls the loudness at the decoder output when supported. It is typically
                        used for loudness normalization. Compared to kAudioCodecPropertyProgramTargetLevelConstant,
                        this property is not limiting the values to predefined constants.
    @constant        kAudioCodecPropertyAdjustTargetLevel
                        A Float32 specifying the program target loudness in LKFS for decoders. It has the same effect
                        as kAudioCodecPropertyProgramTargetLevel, but this property can also be set on an initialized decoder
                        object. It will be applied immediately, if supported.
    @constant        kAudioCodecPropertyDynamicRangeControlConfiguration
                        A UInt32 specifying the encoder DRC configuration. Configurations are defined as enum with the prefix
                        kAudioCodecDynamicRangeControlConfiguration_. When supported by the encoder, this property controls which
                        configuration is applied when a bitstream is generated. The default configuration for an APAC
                        encoder is kAudioCodecDynamicRangeControlConfiguration_Capture, otherwise it is kAudioCodecDynamicRangeControlConfiguration_None.
    @constant        kAudioCodecPropertyContentSource
                        An SInt32 index to select a pre-defined content source type that describes the content type and how it was generated.
                        This is an encoder property with read/write access, if supported.  Supported values are defined with a prefix kAudioCodecContentSource_.
    @constant        kAudioCodecPropertyASPFrequency
                        A UInt32 to set the frequency of Audio Sync Packets (ASP). The value must be larger than 2.
                        A recommended value is 75 so that each 75th packet is an ASP.
                        This is an encoder property with read/write access, if supported.
*/
CF_ENUM(AudioCodecPropertyID)
{
	kAudioCodecPropertyInputBufferSize											= 'tbuf',
	kAudioCodecPropertyPacketFrameSize											= 'pakf',
	kAudioCodecPropertyHasVariablePacketByteSizes								= 'vpk?',
	kAudioCodecPropertyEmploysDependentPackets									= 'dpk?',
	kAudioCodecPropertyMaximumPacketByteSize									= 'pakb',
    kAudioCodecPropertyPacketSizeLimitForVBR                                    = 'pakl',
	kAudioCodecPropertyCurrentInputFormat										= 'ifmt',
	kAudioCodecPropertyCurrentOutputFormat										= 'ofmt',
	kAudioCodecPropertyMagicCookie												= 'kuki',
	kAudioCodecPropertyUsedInputBufferSize										= 'ubuf',
	kAudioCodecPropertyIsInitialized											= 'init',
	kAudioCodecPropertyCurrentTargetBitRate										= 'brat',
  	kAudioCodecPropertyCurrentInputSampleRate									= 'cisr',
  	kAudioCodecPropertyCurrentOutputSampleRate									= 'cosr',
	kAudioCodecPropertyQualitySetting											= 'srcq',
	kAudioCodecPropertyApplicableBitRateRange									= 'brta',
    kAudioCodecPropertyRecommendedBitRateRange                                  = 'brtr',
	kAudioCodecPropertyApplicableInputSampleRates								= 'isra',	
	kAudioCodecPropertyApplicableOutputSampleRates								= 'osra',
	kAudioCodecPropertyPaddedZeros												= 'pad0',
	kAudioCodecPropertyPrimeMethod												= 'prmm',
	kAudioCodecPropertyPrimeInfo												= 'prim',
	kAudioCodecPropertyCurrentInputChannelLayout								= 'icl ',
	kAudioCodecPropertyCurrentOutputChannelLayout								= 'ocl ',
	kAudioCodecPropertySettings													= 'acs ',
	kAudioCodecPropertyFormatList												= 'acfl',
	kAudioCodecPropertyBitRateControlMode										= 'acbf',
	kAudioCodecPropertySoundQualityForVBR										= 'vbrq',
    kAudioCodecPropertyBitRateForVBR                                            = 'vbrb',
	kAudioCodecPropertyDelayMode                                                = 'dmod',
    kAudioCodecPropertyAdjustLocalQuality										= '^qal',
    kAudioCodecPropertyDynamicRangeControlMode									= 'mdrc',
    kAudioCodecPropertyAdjustCompressionProfile                                 = '^pro',
    kAudioCodecPropertyProgramTargetLevelConstant								= 'ptlc',
    kAudioCodecPropertyAdjustTargetLevelConstant                                = '^tlc',
    kAudioCodecPropertyProgramTargetLevel                                       = 'pptl',
    kAudioCodecPropertyAdjustTargetLevel                                        = '^ptl',
    kAudioCodecPropertyDynamicRangeControlConfiguration                         = 'cdrc',
    kAudioCodecPropertyContentSource                                            = 'csrc',
    kAudioCodecPropertyASPFrequency                                             = 'aspf',
};


/*!
	@enum			AudioCodecQuality
 
	@discussion		Constants to be used with kAudioCodecPropertyQualitySetting
 
	@constant		kAudioCodecQuality_Max
	@constant		kAudioCodecQuality_High
	@constant		kAudioCodecQuality_Medium
	@constant		kAudioCodecQuality_Low
	@constant		kAudioCodecQuality_Min
*/
CF_ENUM(UInt32)
{
	kAudioCodecQuality_Max		= 0x7F,
	kAudioCodecQuality_High		= 0x60,
	kAudioCodecQuality_Medium	= 0x40,
	kAudioCodecQuality_Low		= 0x20,
	kAudioCodecQuality_Min		= 0
};


/*!
	@enum			AudioCodecPrimeMethod
 
	@discussion		Constants to be used with kAudioCodecPropertyPrimeMethod.
 
	@constant		kAudioCodecPrimeMethod_Pre
						Primes with leading and trailing input frames
	@constant		kAudioCodecPrimeMethod_Normal
						Only primes with trailing (zero latency)
						leading frames are assumed to be silence
	@constant		kAudioCodecPrimeMethod_None
						Acts in "latency" mode
						both leading and trailing frames assumed to be silence
*/
CF_ENUM(UInt32)
{
	kAudioCodecPrimeMethod_Pre 		= 0,
	kAudioCodecPrimeMethod_Normal 	= 1,
	kAudioCodecPrimeMethod_None 	= 2
};


/*!
	@enum			kAudioCodecPropertyBitRateControlMode
 
	@discussion		Constants defining various bit rate control modes
					to be used with kAudioCodecPropertyBitRateControlMode.
					These modes are only applicable to encoders that can produce
					variable packet sizes, such as AAC.

	@constant		kAudioCodecBitRateControlMode_Constant
						The encoder maintains a constant bit rate suitable for use over a transmission 
						channel when decoding in real-time with a fixed end-to-end audio delay.  
						Note that while a constant bit rate is maintained in this mode, the number of bits 
						allocated to encode each fixed length of audio data may be variable 
						(ie. packet sizes are variable).
						E.g., MP3 and MPEG-AAC use a bit reservoir mechanism to meet that constraint.
	@constant		kAudioCodecBitRateControlMode_LongTermAverage
						 The provided target bit rate is achieved over a long term average
						 (typically after the first 1000 packets). This mode is similar to 
						 kAudioCodecBitRateControlMode_Constant in the sense that the 
						 target bit rate will be maintained in a long term average. However, it does not 
						 provide constant delay when using constant bit rate transmission. This mode offers 
						 a better sound quality than kAudioCodecBitRateControlMode_Constant 
						 can, that is, a more efficient encoding is performed. 
	@constant		kAudioCodecBitRateControlMode_VariableConstrained
						Encoder dynamically allocates the bit resources according to the characteristics
						of the underlying signal. However, some constraints are applied in order to limit 
						the variation of the bit rate.
	@constant		kAudioCodecBitRateControlMode_Variable
						Similar to the VBR constrained mode, however the packet size is virtually unconstrained.
						The coding process targets constant sound quality, and the sound quality level is 
						set by kAudioCodecPropertySoundQualityForVBR.
						This mode usually provides the best tradeoff between quality and bit rate.
*/
CF_ENUM(UInt32)
{
	kAudioCodecBitRateControlMode_Constant					= 0,
	kAudioCodecBitRateControlMode_LongTermAverage			= 1,
	kAudioCodecBitRateControlMode_VariableConstrained		= 2,
	kAudioCodecBitRateControlMode_Variable					= 3,
};

/*!
    @enum			AudioCodecDelayMode
 
    @discussion		Constants defining various delay modes to be used with kAudioCodecPropertyDelayMode.
                    The resulting priming frames are reflected in the kAudioCodecPropertyPrimeInfo property.
                    Note that for layered streams like aach and aacp, the priming information always refers
                    to the base layer.
    @constant		kAudioCodecDelayMode_Compatibility
                        In compatibility delay mode, the resulting priming corresponds to the default value defined by the
                        underlying codecs. For aac this number equals 2112 regardless of the sample rate and other settings.
    @constant		kAudioCodecDelayMode_Minimum
                        Sets the encoder, where applicable, in it's lowest possible delay mode. Any additional delays, like the one
                        introduced by filters/sample rate converters etc, aren't compensated by the encoder.
    @constant		kAudioCodecDelayMode_Optimal
                        In this mode, the resulting bitstream has the minimum amount of priming necessary for the decoder.
                        For aac this number is 1024 which corresponds to exactly one packet.
*/
CF_ENUM(UInt32)
{
    kAudioCodecDelayMode_Compatibility  = 0,
    kAudioCodecDelayMode_Minimum        = 1,
    kAudioCodecDelayMode_Optimal        = 2
};

/*!
	@enum			ProgramTargetLevel

	@discussion		Constants to be used with kAudioCodecPropertyProgramTargetLevelConstant to set the target loudness

	@constant		kProgramTargetLevel_None
						
	@constant		kProgramTargetLevel_Minus31dB
	@constant		kProgramTargetLevel_Minus23dB
	@constant		kProgramTargetLevel_Minus20dB
*/
CF_ENUM(UInt32)
{
	kProgramTargetLevel_None		= 0,
	kProgramTargetLevel_Minus31dB	= 1,
	kProgramTargetLevel_Minus23dB	= 2,
	kProgramTargetLevel_Minus20dB	= 3
};
    
/*!
	@enum			DynamicRangeControlMode

	@discussion		Constants to be used with kAudioCodecPropertyDynamicRangeControlMode

	@constant		kDynamicRangeControlMode_None
						Dynamic range compression disabled
	@constant		kDynamicRangeControlMode_Light
						Light compression according to MPEG-Audio ISO/IEC 14496
	@constant		kDynamicRangeControlMode_Heavy
						Heavy compression according to ETSI TS 101 154
*/
CF_ENUM(UInt32)
{
	kDynamicRangeControlMode_None	= 0,
	kDynamicRangeControlMode_Light	= 1,
	kDynamicRangeControlMode_Heavy	= 2
};

/*!
    @enum            DynamicRangeCompressionProfile

    @discussion        Constants to be used with kAudioCodecPropertyAdjustCompressionProfile to control
                       the DRC Effect Type based on ISO/IEC 23003-4

    @constant       kDynamicRangeCompressionProfile_None
                        No compression
    @constant        kDynamicRangeCompressionProfile_LateNight
                        Compression to avoid disturbing others and listening at lower volume
    @constant        kDynamicRangeCompressionProfile_NoisyEnvironment
                        Compression suitable for listening in noisy environments
    @constant        kDynamicRangeCompressionProfile_LimitedPlaybackRange
                        Compression for transducers with limited dynamic range
    @constant        kDynamicRangeCompressionProfile_GeneralCompression
                        General purpose compression
*/
CF_ENUM(UInt32)
{
    kDynamicRangeCompressionProfile_None                    = 0,
    kDynamicRangeCompressionProfile_LateNight               = 1,
    kDynamicRangeCompressionProfile_NoisyEnvironment        = 2,
    kDynamicRangeCompressionProfile_LimitedPlaybackRange    = 3,
    kDynamicRangeCompressionProfile_GeneralCompression      = 6
};

/*!
    @enum            AudioCodecDynamicRangeControlConfiguration

    @discussion     Constants to be used with kAudioCodecPropertyDynamicRangeControlConfiguration for encoding

    @constant       kAudioCodecDynamicRangeControlConfiguration_None
                        Dynamic range compression disabled
    @constant       kAudioCodecDynamicRangeControlConfiguration_Music
                        Dynamic range compression for music
    @constant       kAudioCodecDynamicRangeControlConfiguration_Speech
                        Dynamic range compression for speech
    @constant       kAudioCodecDynamicRangeControlConfiguration_Movie
                        Dynamic range compression for movie sound tracks
    @constant       kAudioCodecDynamicRangeControlConfiguration_Capture
                        Dynamic range compression for capture (live encoding)
*/
CF_ENUM(UInt32)
{
    kAudioCodecDynamicRangeControlConfiguration_None     = 0,
    kAudioCodecDynamicRangeControlConfiguration_Music    = 1,
    kAudioCodecDynamicRangeControlConfiguration_Speech   = 2,
    kAudioCodecDynamicRangeControlConfiguration_Movie    = 3,
    kAudioCodecDynamicRangeControlConfiguration_Capture  = 4
};

/*!
    @enum           AudioCodecContentSource

    @discussion     Constants to be used with kAudioCodecPropertyContentSource to indicate the content type.

    @constant       kAudioCodecContentSource_Unspecified
                        Unspecified content source
    @constant       kAudioCodecContentSource_Reserved
                        Reserved index
    @constant       kAudioCodecContentSource_AppleCapture_Traditional
                        Traditional Apple device capture
    @constant       kAudioCodecContentSource_AppleCapture_Spatial
                        Spatial Apple device capture
    @constant       kAudioCodecContentSource_AppleCapture_Spatial_Enhanced
                        Reserved for Apple use
    @constant       kAudioCodecContentSource_AppleMusic_Traditional
                        Traditional Apple music and music video content such as stereo and multichannel
    @constant       kAudioCodecContentSource_AppleMusic_Spatial
                        Spatial Apple music and music video content
    @constant       kAudioCodecContentSource_AppleAV_Traditional_Offline
                        Traditional Apple professional AV offline encoded content such as stereo and multichannel
    @constant       kAudioCodecContentSource_AppleAV_Spatial_Offline
                        Spatial Apple professional AV offline encoded content
    @constant       kAudioCodecContentSource_AppleAV_Traditional_Live
                        Traditional Apple professional AV live content such as stereo and multichannel
    @constant       kAudioCodecContentSource_AppleAV_Spatial_Live
                        Spatial Apple professional AV live content
    @constant       kAudioCodecContentSource_ApplePassthrough
                        Apple passthrough content (use only if source information is not available)

    @constant       kAudioCodecContentSource_Capture_Traditional
                        Traditional device capture
    @constant       kAudioCodecContentSource_Capture_Spatial
                        Spatial device capture
    @constant       kAudioCodecContentSource_Capture_Spatial_Enhanced
                        Reserved for future use
    @constant       kAudioCodecContentSource_Music_Traditional
                        Traditional music and music video content such as stereo and multichannel
    @constant       kAudioCodecContentSource_Music_Spatial
                        Spatial music and music video content
    @constant       kAudioCodecContentSource_AV_Traditional_Offline
                        Traditional professional AV offline encoded content such as stereo and multichannel
    @constant       kAudioCodecContentSource_AV_Spatial_Offline
                        Spatial professional AV offline encoded content
    @constant       kAudioCodecContentSource_AV_Traditional_Live
                        Traditional professional AV live content such as stereo and multichannel
    @constant       kAudioCodecContentSource_AV_Spatial_Live
                        Spatial professional AV live content
    @constant       kAudioCodecContentSource_Passthrough
                        Passthrough content (use only if source information is not available)
*/
CF_ENUM(SInt32)
{
    kAudioCodecContentSource_Unspecified                    = -1,
    kAudioCodecContentSource_Reserved                       = 0,
    kAudioCodecContentSource_AppleCapture_Traditional       = 1,
    kAudioCodecContentSource_AppleCapture_Spatial           = 2,
    kAudioCodecContentSource_AppleCapture_Spatial_Enhanced  = 3,
    kAudioCodecContentSource_AppleMusic_Traditional         = 4,
    kAudioCodecContentSource_AppleMusic_Spatial             = 5,
    kAudioCodecContentSource_AppleAV_Traditional_Offline    = 6,
    kAudioCodecContentSource_AppleAV_Spatial_Offline        = 7,
    kAudioCodecContentSource_AppleAV_Traditional_Live       = 8,
    kAudioCodecContentSource_AppleAV_Spatial_Live           = 9,
    kAudioCodecContentSource_ApplePassthrough               = 10,
    
    kAudioCodecContentSource_Capture_Traditional            = 33,
    kAudioCodecContentSource_Capture_Spatial                = 34,
    kAudioCodecContentSource_Capture_Spatial_Enhanced       = 35,
    kAudioCodecContentSource_Music_Traditional              = 36,
    kAudioCodecContentSource_Music_Spatial                  = 37,
    kAudioCodecContentSource_AV_Traditional_Offline         = 38,
    kAudioCodecContentSource_AV_Spatial_Offline             = 39,
    kAudioCodecContentSource_AV_Traditional_Live            = 40,
    kAudioCodecContentSource_AV_Spatial_Live                = 41,
    kAudioCodecContentSource_Passthrough                    = 42
};

/*!
	@struct			AudioCodecPrimeInfo
 
	@discussion		Specifies the number of leading and trailing empty frames
					which have to be inserted.
 
	@var  			leadingFrames
						An unsigned integer specifying the number of leading empty frames
	@var  			trailingFrames
						An unsigned integer specifying the number of trailing empty frames 
*/
typedef struct AudioCodecPrimeInfo 
{
	UInt32		leadingFrames;
	UInt32		trailingFrames;
} AudioCodecPrimeInfo;


//=============================================================================
#pragma mark -
#pragma mark Constants for kAudioCodecPropertySettings
//=============================================================================

#define kAudioSettings_TopLevelKey		"name"
#define kAudioSettings_Version			"version"
#define kAudioSettings_Parameters 		"parameters"
#define kAudioSettings_SettingKey		"key"
#define kAudioSettings_SettingName 		"name"
#define kAudioSettings_ValueType 		"value type"
#define kAudioSettings_AvailableValues	"available values"
#define kAudioSettings_LimitedValues	"limited values"
#define kAudioSettings_CurrentValue 	"current value"
#define kAudioSettings_Summary 			"summary"
#define kAudioSettings_Hint 			"hint"
#define kAudioSettings_Unit 			"unit"


/*!
	@enum			AudioSettingsFlags
 
	@discussion		Constants to be used with kAudioSettings_Hint
					in the kAudioCodecPropertySettings property dictionary.
					Indicates any special characteristics of each parameter within the dictionary, 

	@constant		kAudioSettingsFlags_ExpertParameter
						If set, then the parameter is an expert parameter.
	@constant		kAudioSettingsFlags_InvisibleParameter
						If set, then the parameter should not be displayed. 
	@constant		kAudioSettingsFlags_MetaParameter
						If set, then changing this parameter may affect the values of other parameters. 
						If not set, then this parameter can be set without affecting the values of other parameters.
	@constant		kAudioSettingsFlags_UserInterfaceParameter
						If set, then this is only a user interface element and not reflected in the codec's bit stream.
*/
typedef CF_OPTIONS(UInt32, AudioSettingsFlags) {
	kAudioSettingsFlags_ExpertParameter			= (1L << 0),
	kAudioSettingsFlags_InvisibleParameter		= (1L << 1),
	kAudioSettingsFlags_MetaParameter			= (1L << 2),
	kAudioSettingsFlags_UserInterfaceParameter	= (1L << 3)
};


//=============================================================================
#pragma mark -
#pragma mark Status values returned from the AudioCodecProduceOutputPackets routine
//=============================================================================
/*!
	@enum			AudioCodecProduceOutputPacketStatus
 
	@discussion		Possible return status
 
	@constant		kAudioCodecProduceOutputPacketFailure
						Couldn't complete the request due to an error. It is possible
						that some output data was produced. This is reflected in the value
						returned in ioNumberPackets. 
	@constant		kAudioCodecProduceOutputPacketSuccess
						The number of requested output packets was produced without incident
						and there isn't any more input data to process
	@constant		kAudioCodecProduceOutputPacketSuccessHasMore
						The number of requested output packets was produced and there is
						enough input data to produce at least one more packet of output data
	@constant		kAudioCodecProduceOutputPacketNeedsMoreInputData
						There was insufficient input data to produce the requested
						number of output packets, The value returned in ioNumberPackets
						holds the number of output packets produced.
	@constant		kAudioCodecProduceOutputPacketAtEOF
						The end-of-file marker was hit during the processing. Fewer
						than the requested number of output packets may have been
						produced. Check the value returned in ioNumberPackets for the
						actual number produced. Note that not all formats have EOF
						markers in them.
     @constant        kAudioCodecProduceOutputPacketSuccessConcealed
                        No input packets were provided, but the decoder supports packet
                        loss concealment, so output packets were still created.
*/
CF_ENUM(UInt32)
{
	kAudioCodecProduceOutputPacketFailure					= 1,		
	kAudioCodecProduceOutputPacketSuccess					= 2,	
	kAudioCodecProduceOutputPacketSuccessHasMore			= 3,		
	kAudioCodecProduceOutputPacketNeedsMoreInputData		= 4,		
	kAudioCodecProduceOutputPacketAtEOF						= 5,
    kAudioCodecProduceOutputPacketSuccessConcealed          = 6
};


//=============================================================================
#pragma mark -
#pragma mark Selectors for the component routines
//=============================================================================
/*!
	@enum			AudioCodecSelectors
 
	@discussion		Allows selection of component routines supported the the AudioCodec API
					Used by the Component Manager.
 
	@constant		kAudioCodecGetPropertyInfoSelect
	@constant		kAudioCodecGetPropertySelect
	@constant		kAudioCodecSetPropertySelect
	@constant		kAudioCodecInitializeSelect
	@constant		kAudioCodecUninitializeSelect
	@constant		kAudioCodecAppendInputDataSelect
	@constant		kAudioCodecProduceOutputDataSelect
	@constant		kAudioCodecResetSelect
	@constant		kAudioCodecAppendInputBufferListSelect
	@constant		kAudioCodecProduceOutputBufferListSelect
*/
CF_ENUM(UInt32)
{
	kAudioCodecGetPropertyInfoSelect						= 0x0001,
	kAudioCodecGetPropertySelect							= 0x0002,
	kAudioCodecSetPropertySelect							= 0x0003,
	kAudioCodecInitializeSelect								= 0x0004,
	kAudioCodecUninitializeSelect							= 0x0005,
	kAudioCodecAppendInputDataSelect						= 0x0006,
	kAudioCodecProduceOutputDataSelect						= 0x0007,
	kAudioCodecResetSelect									= 0x0008,
	kAudioCodecAppendInputBufferListSelect					= 0x0009,
	kAudioCodecProduceOutputBufferListSelect				= 0x000A
};


//=============================================================================
#pragma mark -
#pragma mark Errors
//=============================================================================
/*!
	@enum			AudioCodecErrors
 
	@discussion		Possible errors returned by audio codec components
 
	@constant		kAudioCodecNoError
	@constant		kAudioCodecUnspecifiedError
	@constant		kAudioCodecUnknownPropertyError
	@constant		kAudioCodecBadPropertySizeError
	@constant		kAudioCodecIllegalOperationError
	@constant		kAudioCodecUnsupportedFormatError
	@constant		kAudioCodecStateError
	@constant		kAudioCodecNotEnoughBufferSpaceError
	@constant		kAudioCodecBadDataError
*/
CF_ENUM(OSStatus)
{
	kAudioCodecNoError								= 0,
	kAudioCodecUnspecifiedError						= 'what',
	kAudioCodecUnknownPropertyError					= 'who?',
	kAudioCodecBadPropertySizeError					= '!siz',
	kAudioCodecIllegalOperationError				= 'nope',
	kAudioCodecUnsupportedFormatError				= '!dat',
	kAudioCodecStateError							= '!stt',
	kAudioCodecNotEnoughBufferSpaceError			= '!buf',
	kAudioCodecBadDataError							= 'bada'
};


//=============================================================================
#pragma mark -
#pragma mark Codec Property Management
//=============================================================================

/*!
	@function		AudioCodecGetPropertyInfo
 
	@discussion		Retrieve information about the given property. The outSize argument
					will return the size in bytes of the current value of the property.
					The outWritable argument will return whether or not the property
					in question can be changed.
 
	@param			inCodec
						An AudioCodec instance
	@param			inPropertyID
						Property ID whose value should be read
	@param			outSize
						Size in bytes of the property
	@param			outWritable
						Flag indicating wether the underlying property can be modified or not 
 
	@result			The OSStatus value
*/
extern OSStatus
AudioCodecGetPropertyInfo(	AudioCodec				inCodec,
							AudioCodecPropertyID	inPropertyID,
							UInt32* __nullable		outSize,
							Boolean* __nullable		outWritable)		API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0)) ;


/*!
	@function		AudioCodecGetProperty
 
	@discussion		Retrieve the indicated property data. On input, ioDataSize has the size
					of the data pointed to by outPropertyData. On output, ioDataSize will contain
					the amount written.
 
	@param			inCodec
						An AudioCodec instance
	@param			inPropertyID
						Property ID whose value should be read
	@param			ioPropertyDataSize
						Size in bytes of the property data
	@param			outPropertyData
						Pointer to the property data buffer

	@result			The OSStatus value
*/
extern OSStatus
AudioCodecGetProperty(	AudioCodec				inCodec,
						AudioCodecPropertyID	inPropertyID,
						UInt32*					ioPropertyDataSize,
						void*					outPropertyData)		API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0)) ;


/*!
	@function		AudioCodecSetProperty

	@discussion		Set the indicated property data.
 
	@param			inCodec
						An AudioCodec instance
	@param			inPropertyID
						Property ID whose value should be changed
	@param			inPropertyDataSize
						Size in bytes of the property data
	@param			inPropertyData
						Pointer to the property data buffer
 
	@result			The OSStatus value
*/
extern OSStatus
AudioCodecSetProperty(	AudioCodec				inCodec,
						AudioCodecPropertyID	inPropertyID,
						UInt32					inPropertyDataSize,
						const void*				inPropertyData)			API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0)) ;


//=============================================================================
#pragma mark -
#pragma mark Codec Data Handling Routines
//=============================================================================

/*!
	@function		AudioCodecInitialize
 
	@discussion		This call will allocate any buffers needed and otherwise set the codec
					up to perform the indicated translation. If an argument is NULL, any
					previously set properties will be used for preparing the codec for work.
					Note that this routine will also validate the format information as useable.
 
	@param			inCodec
						An AudioCodec instance
	@param			inInputFormat
						Pointer to an input format structure
	@param			inOutputFormat
						Pointer to an output format structure
	@param			inMagicCookie
						Pointer to the magic cookie
	@param			inMagicCookieByteSize
						Size in bytes of the magic cookie
  
	@result			The OSStatus value
*/
extern OSStatus
AudioCodecInitialize(	AudioCodec										inCodec,
						const AudioStreamBasicDescription* __nullable	inInputFormat,
						const AudioStreamBasicDescription* __nullable	inOutputFormat,
						const void*	__nullable							inMagicCookie,
						UInt32											inMagicCookieByteSize)
																		API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0)) ;


/*!
	@function		AudioCodecUninitialize
  
	@discussion		This call will move the codec from the initialized state back to the
					uninitialized state. The codec will release any resources it allocated
					or claimed in AudioCodecInitialize.
 
	@param			inCodec
						An AudioCodec instance
 
	@result			The OSStatus value
*/
extern OSStatus
AudioCodecUninitialize(AudioCodec inCodec)								API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0)) ;


/*!
	@function		AudioCodecAppendInputData
 
	@discussion		Append as much of the given data in inInputData to the codec's input buffer as possible
					and return in ioInputDataByteSize the amount of data used.
 
					The inPacketDescription argument is an array of AudioStreamPacketDescription
					structs that describes the packet layout. The number of elements in this array
					is indicated on input by ioNumberPackets. On return, this number indicates the number
					of packets consumed.
 
					Note also in this case that it is an error to supply less than a full packet
					of data at a time.
 
	@param			inCodec
						An AudioCodec instance
	@param			inInputData
						A const pointer to the input data
	@param			ioInputDataByteSize
						The size in bytes of the input data in inInputData on input,
						the number of bytes consumed on output
	@param			ioNumberPackets
						The number of packets
	@param			inPacketDescription
						The packet description pointer
 
	@result			The OSStatus value
*/
extern OSStatus
AudioCodecAppendInputData(	AudioCodec										inCodec,
							const void*										inInputData,
							UInt32*											ioInputDataByteSize,
							UInt32*											ioNumberPackets,
							const AudioStreamPacketDescription*	__nullable	inPacketDescription)
																		API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0)) ;


/*!
	@function		AudioCodecProduceOutputPackets

	@discussion		Produce as many output packets as requested and the amount of input data
					allows for. The outStatus argument returns information about the codec's
					status to allow for proper data management. See the constants above for
					the possible values that can be returned.
 
					The outPacketDescription argument is an array of AudioStreamPacketDescription
					structs that describes the packet layout returned in outOutputData. This
					argument is optional. Pass NULL if this information is not to be returned.
					Note that this information is only provided when the output format isn't
					linear PCM.

					Note that decoders will always only produce linear PCM data in multiples of
					the number frames in a packet of the encoded format (as returned by
					kAudioCodecPropertyPacketFrameSize). Encoders will consume this many frames
					of linear PCM data to produce a packet of their format.
 
	@param			inCodec
						The AudioCodec instance
	@param			outOutputData
						Pointer to the output data buffer
	@param			ioOutputDataByteSize
						A pointer to the size
	@param			ioNumberPackets
						number of input/output packets
	@result			The OSStatus value
*/
extern OSStatus
AudioCodecProduceOutputPackets(	AudioCodec									inCodec,
								void*										outOutputData,
								UInt32*										ioOutputDataByteSize,
								UInt32*										ioNumberPackets,
								AudioStreamPacketDescription* __nullable	outPacketDescription,
								UInt32*										outStatus)
																		API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0)) ;

extern OSStatus
AudioCodecAppendInputBufferList(	AudioCodec							inCodec,
									const AudioBufferList *				inBufferList,
									UInt32*								ioNumberPackets,
									const AudioStreamPacketDescription*	__nullable	inPacketDescription,
									UInt32*								outBytesConsumed)
																		API_AVAILABLE(macos(10.7), ios(4.0), watchos(2.0), tvos(9.0)) ;

extern OSStatus
AudioCodecProduceOutputBufferList(	AudioCodec									inCodec,
									AudioBufferList *							ioBufferList,
									UInt32*										ioNumberPackets,
									AudioStreamPacketDescription* __nullable	outPacketDescription,
									UInt32*										outStatus)
																		API_AVAILABLE(macos(10.7), ios(4.0), watchos(2.0), tvos(9.0)) ;

/*!
	@function		AudioCodecReset

	@discussion		Flushes all the data in the codec and clears the input buffer. Note that
					the formats, and magic cookie will be retained so they won't need to be
					set up again to decode the same data.
 
	@param			inCodec The audio codec descriptor
 
	@result			the OSStatus value
*/
extern OSStatus
AudioCodecReset(AudioCodec inCodec)										API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0)) ;

//=====================================================================================================================
typedef OSStatus
(*AudioCodecGetPropertyInfoProc)(void *self, AudioCodecPropertyID inPropertyID, UInt32 * __nullable outSize, Boolean * __nullable outWritable);

typedef OSStatus
(*AudioCodecGetPropertyProc)(void *self, AudioCodecPropertyID inPropertyID, UInt32 *ioPropertyDataSize, 
								void *outPropertyData);

typedef OSStatus
(*AudioCodecSetPropertyProc)(void *self, AudioCodecPropertyID inPropertyID, UInt32 inPropertyDataSize, 
								const void *inPropertyData);

typedef OSStatus
(*AudioCodecInitializeProc)(void *self, const AudioStreamBasicDescription * __nullable inInputFormat,
								const AudioStreamBasicDescription * __nullable inOutputFormat, const void * __nullable inMagicCookie,
								UInt32 inMagicCookieByteSize);

typedef OSStatus
(*AudioCodecUninitializeProc)(void *self);

typedef OSStatus
(*AudioCodecAppendInputDataProc)(void *self, const void *inInputData, UInt32 *ioInputDataByteSize, UInt32 *ioNumberPackets, 
								const AudioStreamPacketDescription * __nullable inPacketDescription);

typedef OSStatus
(*AudioCodecProduceOutputPacketsProc)(void *self, void *outOutputData, UInt32 *ioOutputDataByteSize, UInt32 *ioNumberPackets, 
								AudioStreamPacketDescription * __nullable outPacketDescription, UInt32 *outStatus);

typedef OSStatus
(*AudioCodecResetProc)(void *self);

typedef OSStatus
(*AudioCodecAppendInputBufferListProc)(void *self, const AudioBufferList *ioBufferList, UInt32 *inNumberPackets, 
								const AudioStreamPacketDescription * __nullable inPacketDescription, UInt32 *outBytesConsumed);

typedef OSStatus
(*AudioCodecProduceOutputBufferListProc)(void *self, AudioBufferList *ioBufferList, UInt32 *ioNumberPackets, 
								AudioStreamPacketDescription *__nullable outPacketDescription, UInt32 *outStatus);


//=====================================================================================================================
#pragma mark -
#pragma mark Deprecated Properties

/*!
    @enum		AudioCodecProperty
    @deprecated

    @constant	kAudioCodecPropertyMinimumDelayMode
                    A UInt32 equal 1 sets the encoder, where applicable, in it's lowest possible delay mode. An encoder
                    may prepend zero valued samples to the input signal in order to make additional delays, like e.g.
                    from a filter, coincide on a block boundary. This operation, however, results in an increased
                    encoding/ decoding delay which may be undesired and turned off with this property.
                    Use the kAudioCodecPropertyDelayMode property instead with the value set to kAudioCodecDelayMode_Minimum
*/
CF_ENUM(AudioCodecPropertyID)
{
	kAudioCodecPropertyMinimumDelayMode                 = 'mdel'
};

/*!
	@enum		AudioCodecProperty
	@deprecated	in version 10.7

	@constant	kAudioCodecPropertyNameCFString
					The name of the codec component as a CFStringRef. The CFStringRef
					retrieved via this property must be released by the caller.
	@constant	kAudioCodecPropertyManufacturerCFString
					The manufacturer of the codec as a CFStringRef. The CFStringRef 
					retrieved via this property must be released by the caller.
	@constant	kAudioCodecPropertyFormatCFString
					The name of the codec's format as a CFStringRef. The CFStringRef
					retrieved via this property must be released by the caller.

*/
CF_ENUM(AudioCodecPropertyID)
{
	kAudioCodecPropertyNameCFString			= 'lnam',
	kAudioCodecPropertyManufacturerCFString = 'lmak',
	kAudioCodecPropertyFormatCFString		= 'lfor'
};

/*!
	@enum		AudioCodecProperty
	@deprecated	in version 10.5
 
	@constant	kAudioCodecPropertyRequiresPacketDescription
					A UInt32 where a non-zero value indicates that the format the codec implements
					requires that an AudioStreamPacketDescription array must be supplied with any data
					in that format. Note that this implies that data must also be handled strictly in
					packets. For a decoder, this applies to input data. For an encoder, it applies to
					output data, which means that the encoder will be filling out provided packet descriptions
					on output.
					A decoder must be able to handle packet descriptions even if it does not require them.
					An encoder does not have to fill out packet descriptions if it does not require them.
					Redundant due to kAudioCodecPropertyHasVariablePacketByteSizes. Codecs with variable-sized 
					packets must handle packet descriptions while codecs with constant-sized packets do not 
					have to.
	@constant	kAudioCodecPropertyAvailableBitRates
					An array of UInt32 that indicates the target bit rates
					supported by the encoder. This property is only relevant to
					encoders. Replaced with kAudioCodecPropertyAvailableBitRateRange
	@constant	kAudioCodecExtendFrequencies
					A UInt32 indicating whether an encoder should extend its cutoff frequency
					if such an option exists. 0 == extended frequencies off, 1 == extended frequencies on
					e.g. some encoders normally cut off the signal at 16 kHz but can encode up to 20 kHz if 
					asked to.
					Redundant.
	@constant	kAudioCodecUseRecommendedSampleRate
					For encoders that do sample rate conversion, a UInt32 indicating whether or
					not the encoder is using the recommended sample rate for the given input. 
					A value of 0 indicates it isn't, 1 indicates it is.
					This property is read only and indicates whether or not a user has explicitly set an output 
					sample rate.
					Redundant as 0.0 for a sample rate means let the codec decide.
	@constant	kAudioCodecOutputPrecedence
					For encoders that do sample rate conversion, a UInt32 indicating whether the
					bit rate, sample rate, or neither have precedence over the other. See enum below.
					Redundant because precedence is implicitly set by either providing a non-zero bit rate or 
					sample rate and setting the other to zero (which allows the encoder to choose any applicable rate). 
					If both values are set to non-zero, neither value has precedence.
	@constant	kAudioCodecDoesSampleRateConversion
					Renamed to kAudioCodecPropertyDoesSampleRateConversion
	@constant	kAudioCodecBitRateFormat
					Renamed to kAudioCodecPropertyBitRateControlMode
	@constant	kAudioCodecInputFormatsForOutputFormat
					Renamed to kAudioCodecPropertyInputFormatsForOutputFormat
	@constant	kAudioCodecOutputFormatsForInputFormat
					Renamed to kAudioCodecPropertyOutputFormatsForInputFormat
	@constant	kAudioCodecPropertyInputChannelLayout
					Renamed to kAudioCodecPropertyCurrentInputChannelLayout
	@constant	kAudioCodecPropertyOutputChannelLayout
					Renamed to kAudioCodecPropertyCurrentOutputChannelLayout
	@constant	kAudioCodecPropertyZeroFramesPadded
					Renamed to kAudioCodecPropertyPaddedZeros
 */
CF_ENUM(AudioCodecPropertyID)
{
	kAudioCodecPropertyRequiresPacketDescription			= 'pakd',
	kAudioCodecPropertyAvailableBitRates					= 'brt#',
	kAudioCodecExtendFrequencies							= 'acef',
	kAudioCodecUseRecommendedSampleRate						= 'ursr',
	kAudioCodecOutputPrecedence								= 'oppr',
	kAudioCodecBitRateFormat								= kAudioCodecPropertyBitRateControlMode,
	kAudioCodecDoesSampleRateConversion						= kAudioCodecPropertyDoesSampleRateConversion,
	kAudioCodecInputFormatsForOutputFormat					= kAudioCodecPropertyInputFormatsForOutputFormat,
	kAudioCodecOutputFormatsForInputFormat					= kAudioCodecPropertyOutputFormatsForInputFormat,
	kAudioCodecPropertyInputChannelLayout					= kAudioCodecPropertyCurrentInputChannelLayout,
	kAudioCodecPropertyOutputChannelLayout					= kAudioCodecPropertyCurrentOutputChannelLayout,
	kAudioCodecPropertyAvailableInputChannelLayouts			= kAudioCodecPropertyAvailableInputChannelLayoutTags,
	kAudioCodecPropertyAvailableOutputChannelLayouts		= kAudioCodecPropertyAvailableOutputChannelLayoutTags,	
	kAudioCodecPropertyZeroFramesPadded						= kAudioCodecPropertyPaddedZeros
};

/*!
	@enum		AudioCodecBitRateFormat

	@deprecated	in version 10.5

	@discussion	Constants to be used with kAudioCodecBitRateFormat.
					This is deprecated. 
					Use kAudioCodecPropertyBitRateControlMode instead.
 
	@constant	kAudioCodecBitRateFormat_CBR is mapped to kAudioCodecBitRateControlMode_Constant
	@constant	kAudioCodecBitRateFormat_ABR is mapped to kAudioCodecBitRateControlMode_LongTermAverage
	@constant	kAudioCodecBitRateFormat_VBR is mapped to kAudioCodecBitRateControlMode_VariableConstrained
 */
CF_ENUM(UInt32)
{	
	kAudioCodecBitRateFormat_CBR 	=	kAudioCodecBitRateControlMode_Constant,
	kAudioCodecBitRateFormat_ABR 	=	kAudioCodecBitRateControlMode_LongTermAverage,
	kAudioCodecBitRateFormat_VBR 	=	kAudioCodecBitRateControlMode_VariableConstrained
};

/*!
	@enum		AudioCodecOutputPrecedence

	@deprecated	in version 10.5

	@discussion	Constants to be used with kAudioCodecOutputPrecedence
 
	@constant	kAudioCodecOutputPrecedenceNone
					Change in the bit rate or the sample rate are constrained by
					the other value.
	@constant	kAudioCodecOutputPrecedenceBitRate
					The bit rate may be changed freely,
					adjusting the sample rate if necessary
	@constant	kAudioCodecOutputPrecedenceSampleRate
					The sample rate may be changed freely,
					adjusting the bit rate if necessary
 */
CF_ENUM(UInt32)
{
	kAudioCodecOutputPrecedenceNone			= 0,
	kAudioCodecOutputPrecedenceBitRate		= 1,
	kAudioCodecOutputPrecedenceSampleRate 	= 2
};

/*!
	@typedef	MagicCookieInfo
 
	@deprecated	in version 10.5
 
	@discussion	renamed to AudioCodecMagicCookieInfo 
 */
typedef struct AudioCodecMagicCookieInfo MagicCookieInfo __attribute__((deprecated));

/*!
	@enum		AudioCodecSettingsHint
 
	@deprecated	in version 10.4
 
	@discussion	Constants to be used with kAudioSettings_Hint.
				This is deprecated.
				Use	AudioSettingsFlag instead.
 
	@constant	kHintBasic
	@constant	kHintAdvanced
	@constant	kHintHidden
 */
CF_ENUM(UInt32)
{
	kHintBasic		= 0,
	kHintAdvanced	= 1,
	kHintHidden		= 2
};

#if defined(__cplusplus)
}
#endif

CF_ASSUME_NONNULL_END

#endif	//	AudioUnit_AudioCodec_h
#else
#include <AudioToolboxCore/AudioCodec.h>
#endif
