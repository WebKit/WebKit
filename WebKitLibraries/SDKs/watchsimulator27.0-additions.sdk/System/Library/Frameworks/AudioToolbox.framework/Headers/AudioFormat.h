#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioFormat.h>)
/*!
	@file		AudioFormat.h
	@framework	AudioToolbox.framework
	@copyright	(c) 1985-2015 by Apple, Inc., all rights reserved.
    @abstract	API for finding things out about audio formats.
*/

#ifndef AudioToolbox_AudioFormat_h
#define AudioToolbox_AudioFormat_h

//=============================================================================
//	Includes
//=============================================================================

//	System Includes
#include <Availability.h>
#include <CoreAudioTypes/CoreAudioTypes.h>

CF_ASSUME_NONNULL_BEGIN

#if defined(__cplusplus)
extern "C"
{
#endif

//=============================================================================
//	Types
//=============================================================================

/*!
    @typedef	AudioFormatPropertyID
    @abstract   A type for four char codes for property IDs
*/
typedef UInt32	AudioFormatPropertyID;

/*!
    @enum		AudioPanningMode
    @abstract   Different panning algorithms.
    @constant   kPanningMode_SoundField
					Sound field panning algorithm
    @constant   kPanningMode_VectorBasedPanning
					Vector based panning algorithm
*/
typedef CF_ENUM(UInt32, AudioPanningMode) {
	kPanningMode_SoundField		 					= 3,
	kPanningMode_VectorBasedPanning					= 4
};

/*!
    @struct		AudioPanningInfo
    @abstract   This struct is for use with kAudioFormatProperty_PanningMatrix.
    @var        mPanningMode			the PanningMode to be used for the pan
    @var        mCoordinateFlags		the coordinates are specified as in the AudioChannelDescription struct in CoreAudioTypes.h
    @var        mCoordinates			the coordinates are specified as in the AudioChannelDescription struct in CoreAudioTypes.h
    @var        mGainScale				
					mGainScale is used to multiply the panning values.
					In typical usage you are applying an existing volume.
					value in 0 -> 1 (where 1 is unity gain) to the panned values.
					1 would give you panning at unity.
					0 would give you back a matrix of zeroes.
    @var        mOutputChannelMap				
					This is the channel map that is going to be used to determine channel volumes for this pan.
*/
struct AudioPanningInfo {
	AudioPanningMode			mPanningMode;
	UInt32						mCoordinateFlags;
	Float32						mCoordinates[3];	
	Float32						mGainScale;
	const AudioChannelLayout *	mOutputChannelMap;
};
typedef struct AudioPanningInfo AudioPanningInfo;

/*!
    @enum		AudioBalanceFadeType
    @abstract   used for mType field of AudioBalanceFade struct
    @constant   kAudioBalanceFadeType_MaxUnityGain
					the gain value never exceeds 1.0, the opposite channel fades out. 
					This can reduce overall loudness when the balance or fade is not in the center.
    @constant   kAudioBalanceFadeType_EqualPower
					The overall loudness remains constant, but gain can exceed 1.0.
					the gain value is 1.0 when the balance and fade are in the center.
					From there they can increase to +3dB (1.414) and decrease to -inf dB (0.0).
*/
typedef CF_ENUM(UInt32, AudioBalanceFadeType) {
	kAudioBalanceFadeType_MaxUnityGain = 0,
	kAudioBalanceFadeType_EqualPower = 1
};
	
/*!
    @struct		AudioBalanceFade
    @abstract   this struct is used with kAudioFormatProperty_BalanceFade
    @var        mLeftRightBalance 
					-1 is full left, 0 is center, +1 is full right
    @var        mBackFrontFade 
					-1 is full rear, 0 is center, +1 is full front
    @var        mType 
					an AudioBalanceFadeType constant
    @var        mChannelLayout 
					a pointer to an AudioChannelLayout
*/
struct AudioBalanceFade
{
	Float32						mLeftRightBalance;		// -1 is full left, 0 is center, +1 is full right
	Float32						mBackFrontFade;			// -1 is full rear, 0 is center, +1 is full front
	AudioBalanceFadeType		mType;					// max unity gain, or equal power.
	const AudioChannelLayout*	mChannelLayout;
};
typedef struct AudioBalanceFade AudioBalanceFade;

/*!
    @struct		AudioFormatInfo
    @abstract   this struct is used as a specifier for the kAudioFormatProperty_FormatList property
    @var        mASBD 
					an AudioStreamBasicDescription
    @var        mMagicCookie 
					a pointer to the decompression info for the data described in mASBD
    @var        mMagicCookieSize 
					the size in bytes of mMagicCookie
*/
struct AudioFormatInfo
{
	AudioStreamBasicDescription		mASBD;
	const void*						mMagicCookie;
	UInt32							mMagicCookieSize;
};
typedef struct AudioFormatInfo AudioFormatInfo;

/*!
    @struct		ExtendedAudioFormatInfo
    @abstract   this struct is used as a specifier for the kAudioFormatProperty_FormatList property
    @var        mASBD 
					an AudioStreamBasicDescription
    @var        mMagicCookie 
					a pointer to the decompression info for the data described in mASBD
    @var        mMagicCookieSize 
					the size in bytes of mMagicCookie
	@var  		mClassDescription
					an AudioClassDescription specifying the codec to be used in answering the question.
*/
struct ExtendedAudioFormatInfo
{
	AudioStreamBasicDescription		mASBD;
	const void* __nullable			mMagicCookie;
	UInt32							mMagicCookieSize;
	AudioClassDescription			mClassDescription;
};
typedef struct ExtendedAudioFormatInfo ExtendedAudioFormatInfo;

//=============================================================================
//	Properties - for various format structures.
//=============================================================================

/*!
    @enum		AudioFormatProperty constants
    @abstract   constants for use with AudioFormatGetPropertyInfo and AudioFormatGetProperty.
    @constant   kAudioFormatProperty_FormatInfo
					Retrieves general information about a format. The specifier is a
					magic cookie, or NULL.
					On input, the property value is an AudioStreamBasicDescription which 
					should have at least the mFormatID filled out. On output it will be filled out
					as much as possible given the information known about the format 
					and the contents of the magic cookie (if any is given).
					If multiple formats can be described by the AudioStreamBasicDescription and the associated magic cookie,
					this property will return the base level format.
    @constant   kAudioFormatProperty_FormatIsVBR
					Returns whether or not a format has a variable number of bytes per
					packet. The specifier is an AudioStreamBasicDescription describing
					the format to ask about. The value is a UInt32 where non-zero means
					the format is variable bytes per packet.
    @constant   kAudioFormatProperty_FormatIsExternallyFramed
					Returns whether or not a format requires external framing information,
					i.e. AudioStreamPacketDescriptions. 
					The specifier is an AudioStreamBasicDescription describing
					the format to ask about. The value is a UInt32 where non-zero means
					the format is externally framed. Any format which has variable byte sized packets
					requires AudioStreamPacketDescriptions.
    @constant   kAudioFormatProperty_FormatEmploysDependentPackets
					Returns whether or not a format is capable of combining independently
					decodable packets with dependent packets. The specifier is an
					AudioStreamBasicDescription describing the format to ask about.
					The value is a UInt32 where zero means that all packets in streams
					of the specified format are independently decodable and non-zero means
					that streams of the specified format may include dependent packets.
    @constant   kAudioFormatProperty_FormatIsEncrypted
                    Returns whether or not a format is encrypted. The specifier is a UInt32 format ID.
                    The value is a UInt32 where non-zero means the format is encrypted.
    @constant   kAudioFormatProperty_EncodeFormatIDs
					No specifier needed. Must be set to NULL.
					Returns an array of UInt32 format IDs for formats that are valid output formats 
					for a converter. 
    @constant   kAudioFormatProperty_DecodeFormatIDs
					No specifier needed. Must be set to NULL.
					Returns an array of UInt32 format IDs for formats that are valid input formats 
					for a converter. 
    @constant   kAudioFormatProperty_Encoders
					The specifier is the format that you are interested in, e.g. 'aac '
					Returns an array of AudioClassDescriptions for all installed encoders for the given format 
    @constant   kAudioFormatProperty_Decoders
					The specifier is the format that you are interested in, e.g. 'aac '
					Returns an array of AudioClassDescriptions for all installed decoders for the given format 
    @constant   kAudioFormatProperty_AvailableEncodeBitRates
					The specifier is a UInt32 format ID.
					The property value is an array of AudioValueRange describing all available bit rates.
    @constant   kAudioFormatProperty_AvailableEncodeSampleRates
					The specifier is a UInt32 format ID.
					The property value is an array of AudioValueRange describing all available sample rates.
    @constant   kAudioFormatProperty_AvailableEncodeChannelLayoutTags
					The specifier is an AudioStreamBasicDescription with at least the mFormatID 
					and mChannelsPerFrame fields set.
					The property value is an array of AudioChannelLayoutTags for the format and number of
					channels specified. If mChannelsPerFrame is zero, then all layouts supported by
					the format are returned.
    @constant   kAudioFormatProperty_AvailableEncodeNumberChannels
					The specifier is an AudioStreamBasicDescription with at least the mFormatID field set.
					The property value is an array of UInt32 indicating the number of channels that can be encoded.
					A value of 0xFFFFFFFF indicates that any number of channels may be encoded. 
    @constant   kAudioFormatProperty_AvailableDecodeNumberChannels
					The specifier is an AudioStreamBasicDescription with at least the mFormatID field set.
					The property value is an array of UInt32 indicating the number of channels that can be decoded.
    @constant   kAudioFormatProperty_FormatName
					Returns a name for a given format. The specifier is an
					AudioStreamBasicDescription describing the format to ask about.
					The value is a CFStringRef. The caller is responsible for releasing the
					returned string. For some formats (eg, Linear PCM) you will get back a
					descriptive string (e.g. 16-bit, interleaved, etc...)
    @constant   kAudioFormatProperty_ASBDFromESDS
					Returns an audio stream description for a given ESDS. The specifier is an ESDS. 
					The value is a AudioStreamBasicDescription. If multiple formats can be described 
					by the ESDS this property will return the base level format.
    @constant   kAudioFormatProperty_ChannelLayoutFromESDS
					Returns an audio channel layout for a given ESDS. The specifier is an
					ESDS. The value is a AudioChannelLayout.
    @constant   kAudioFormatProperty_ASBDFromMPEGPacket
					Returns an audio stream description for a given MPEG Packet. The specifier is an MPEG Packet. 
					The value is a AudioStreamBasicDescription.					
    @constant   kAudioFormatProperty_FormatList
					Returns a list of AudioFormatListItem structs describing the audio formats contained within the compressed bit stream
					as described by the magic cookie. The specifier is an AudioFormatInfo struct. The mFormatID member of the 
					ASBD struct must filled in. Formats are returned in order from the most to least 'rich', with 
					channel count taking the highest precedence followed by sample rate. The kAudioFormatProperty_FormatList property 
					is the preferred method for discovering format information of the audio data. If the audio data can only be described
					by a single AudioFormatListItem, this property would be equivalent to using the kAudioFormatProperty_FormatInfo property,
					which should be used by the application as a fallback case, to ensure backward compatibility with existing systems 
					when kAudioFormatProperty_FormatList is not present on the running system.
    @constant   kAudioFormatProperty_OutputFormatList
					Returns a list of AudioFormatListItem structs describing the audio formats which may be obtained by decoding the format
					described by the specifier.
					The specifier is an AudioFormatInfo struct. At a minimum formatID member of the ASBD struct must filled in. Other fields
					may be filled in. If there is no magic cookie, then the number of channels and sample rate should be filled in. 
	@constant	kAudioFormatProperty_FirstPlayableFormatFromList
					The specifier is a list of 1 or more AudioFormatListItem. Generally it is the list of these items returned from kAudioFormatProperty_FormatList. The property value retrieved is an UInt32 that specifies an index into that list. The list that the caller provides is generally sorted with the first item as the best format (most number of channels, highest sample rate), and the returned index represents the first item in that list that can be played by the system.
					Thus, the property is typically used to determine the best playable format for a given (layered) audio stream
	@constant   kAudioFormatProperty_ValidateChannelLayout
					The specifier is an AudioChannelLayout. The property value and size are not used and must be set to NULL.
					This property validates an AudioChannelLayout. This is useful if the layout has come from an untrusted source such as a file.
					It returns noErr if the AudioChannelLayout is OK, kAudio_ParamError if there is a structural problem with the layout,
					or kAudioFormatUnknownFormatError for unrecognized layout tags or channel labels.
	@constant   kAudioFormatProperty_ChannelLayoutForTag
					Returns the channel descriptions for a standard channel layout.
					The specifier is a AudioChannelLayoutTag (the mChannelLayoutTag field 
					of the AudioChannelLayout struct) containing the layout constant. 
					The value is an AudioChannelLayout structure. In typical use a AudioChannelLayout
					can be valid with just a defined AudioChannelLayoutTag (ie, those layouts
					have predefined speaker locations and orderings).
					Returns an error if the tag is kAudioChannelLayoutTag_UseChannelBitmap 
    @constant   kAudioFormatProperty_TagForChannelLayout
					Returns an AudioChannelLayoutTag for a layout, if there is one.
					The specifier is an AudioChannelLayout containing the layout description.
					The value is an AudioChannelLayoutTag.
					This can be used to reduce a layout specified by kAudioChannelLayoutTag_UseChannelDescriptions
					or kAudioChannelLayoutTag_UseChannelBitmap to a known AudioChannelLayoutTag.
    @constant   kAudioFormatProperty_ChannelLayoutForBitmap
					Returns the channel descriptions for a standard channel layout.
					The specifier is a UInt32 (the mChannelBitmap field 
					of the AudioChannelLayout struct) containing the layout bitmap. The value
					is an AudioChannelLayout structure. In some uses, an AudioChannelLayout can be 
					valid with the layoutTag set to "use bitmap" and the bitmap set appropriately.
    @constant   kAudioFormatProperty_BitmapForLayoutTag
					Returns a bitmap for an AudioChannelLayoutTag, if there is one.
					The specifier is a AudioChannelLayoutTag  containing the layout tag.
					The value is an UInt32 bitmap. The bits are as defined in CoreAudioTypes.h.
					To go in the other direction, i.e. get a layout tag for a bitmap, 
					use kAudioFormatProperty_TagForChannelLayout where your layout tag 
					is kAudioChannelLayoutTag_UseChannelBitmap and the bitmap is filled in.
    @constant   kAudioFormatProperty_ChannelLayoutName
					Returns the a name for a particular channel layout. The specifier is
					an AudioChannelLayout containing the layout description. The value
					is a CFStringRef. The caller is responsible for releasing the
					returned string.
    @constant   kAudioFormatProperty_ChannelLayoutSimpleName
					Returns the a simpler name for a channel layout than does kAudioFormatProperty_ChannelLayoutName. 
					It omits the channel labels from the name. The specifier is
					an AudioChannelLayout containing the layout description. The value
					is a CFStringRef. The caller is responsible for releasing the
					returned string.
    @constant   kAudioFormatProperty_ChannelName
					Returns the name for a particular channel. The specifier is an
					AudioChannelDescription which has the mChannelLabel field set. The value
					is a CFStringRef. The caller is responsible for releasing the
					returned string.
    @constant   kAudioFormatProperty_ChannelShortName
					Returns an abbreviated name for a particular channel. The specifier is an
					AudioChannelDescription which has the mChannelLabel field set. The value
					is a CFStringRef. The caller is responsible for releasing the
					returned string.
    @constant   kAudioFormatProperty_MatrixMixMap
					Returns a matrix of scaling coefficients for converting audio from one channel map 
					to another in a standard way, if one is known. Otherwise an error is returned.
					The specifier is an array of two pointers to AudioChannelLayout structures. 
					The first points to the input layout, the second to the output layout.
					The value is a two dimensional array of Float32 where the first dimension (rows) 
					is the input channel and the second dimension (columns) is the output channel. 
    @constant   kAudioFormatProperty_ChannelMap
					Returns an array of SInt32 for reordering input channels.
					The specifier is an array of two pointers to AudioChannelLayout structures. 
					The first points to the input layout, the second to the output layout.
					The length of the output array is equal to the number of output channels.
    @constant   kAudioFormatProperty_NumberOfChannelsForLayout
					This is a general call for parsing a AudioChannelLayout provided as the specifier,
					to determine the number of valid channels that are represented. So, if the
					LayoutTag is specified, it returns the number of channels for that layout. If 
					the bitmap is specified, it returns the number of channels represented by that bitmap.
					If the layout tag is 'kAudioChannelLayoutTag_UseChannelDescriptions' it returns
						the number of channel descriptions.
    @constant   kAudioFormatProperty_AreChannelLayoutsEquivalent
					Returns a UInt32 which is 1 if two layouts are equivalent and 0 if they are not equivalent.
					In order to be equivalent, the layouts must describe the same channels in the same order.
					Whether the layout is represented by a bitmap, channel descriptions or a channel layout tag is not significant.
					The channel coordinates are only significant if the channel label is kAudioChannelLabel_UseCoordinates.
					The specifier is an array of two pointers to AudioChannelLayout structures. 
					The value is a pointer to the UInt32 result.
    @constant   kAudioFormatProperty_ChannelLayoutHash
					Returns a UInt32 which represents the hash of the provided channel layout.
					The specifier is a pointer to an AudioChannelLayout structure.
					The value is a pointer to the UInt32 result.
    @constant   kAudioFormatProperty_TagsForNumberOfChannels
					returns an array of AudioChannelLayoutTags for the number of channels specified.
					The specifier is a UInt32 number of channels. 
    @constant   kAudioFormatProperty_PanningMatrix
					This call will pass in an AudioPanningInfo struct that specifies one of the
					kPanningMode_ constants for the panning algorithm and an AudioChannelLayout
					to describe the destination channel layout. As in kAudioFormatProperty_MatrixMixMap
					the return value is an array of Float32 values of the number of channels
						represented by this specified channel layout. It is presumed that the source 
					being panned is mono (thus for a quad channel layout, 4 Float32 values are returned).
					The intention of this API is to provide support for panning operations that are
					strictly manipulating the respective volumes of the channels. Thus, more
					complex panners (like HRTF, distance filtering etc,) will not be represented
						by this API. The resultant volume scalars can then be applied to a mixer
					or some other processing code to adapt the individual volumes of the mixed
					output.
					The volume values will typically be presented within a 0->1 range (where 1 is unity gain)
					For stereo formats, vector based panning is equivalent to the equal-power pan mode.
    @constant   kAudioFormatProperty_BalanceFade
					get an array of coefficients for applying left/right balance and front/back fade.
					The specifier is an AudioBalanceFade struct. 
					the return value is an array of Float32 values of the number of channels
						represented by this specified channel layout.  
					The volume values will typically be presented within a 0->1 range (where 1 is unity gain)
    @constant   kAudioFormatProperty_ID3TagSize
					Returns a UInt32 indicating the ID3 tag size. 
					The specifier must begin with the ID3 tag header and be at least 10 bytes in length
    @constant   kAudioFormatProperty_ID3TagToDictionary
					Returns a CFDictionary containing key/value pairs for the frames in the ID3 tag
					The specifier is the entire ID3 tag
					Caller must call CFRelease for the returned dictionary
					
*/
CF_ENUM(AudioFormatPropertyID)
{
//=============================================================================
//	The following properties are concerned with the AudioStreamBasicDescription
//=============================================================================
	kAudioFormatProperty_FormatInfo						= 'fmti',
	kAudioFormatProperty_FormatName						= 'fnam',
	kAudioFormatProperty_EncodeFormatIDs				= 'acof',
	kAudioFormatProperty_DecodeFormatIDs				= 'acif',
	kAudioFormatProperty_FormatList						= 'flst',
    kAudioFormatProperty_ASBDFromESDS                   = 'essd',
    kAudioFormatProperty_ChannelLayoutFromESDS          = 'escl',
	kAudioFormatProperty_OutputFormatList				= 'ofls',
	kAudioFormatProperty_FirstPlayableFormatFromList	= 'fpfl',
	kAudioFormatProperty_FormatIsVBR					= 'fvbr',	
	kAudioFormatProperty_FormatIsExternallyFramed		= 'fexf',
	kAudioFormatProperty_FormatEmploysDependentPackets  = 'fdep',
	kAudioFormatProperty_FormatIsEncrypted				= 'cryp',
	kAudioFormatProperty_Encoders						= 'aven',	
	kAudioFormatProperty_Decoders						= 'avde',
	kAudioFormatProperty_AvailableEncodeBitRates		= 'aebr',
	kAudioFormatProperty_AvailableEncodeSampleRates		= 'aesr',
	kAudioFormatProperty_AvailableEncodeChannelLayoutTags = 'aecl',
	kAudioFormatProperty_AvailableEncodeNumberChannels		= 'avnc',
	kAudioFormatProperty_AvailableDecodeNumberChannels		= 'adnc',
	kAudioFormatProperty_ASBDFromMPEGPacket				= 'admp',


//=============================================================================
//	The following properties concern the AudioChannelLayout struct (speaker layouts)
//=============================================================================
	kAudioFormatProperty_BitmapForLayoutTag				= 'bmtg',
	kAudioFormatProperty_MatrixMixMap					= 'mmap',
    kAudioFormatProperty_ChannelMap						= 'chmp',
	kAudioFormatProperty_NumberOfChannelsForLayout		= 'nchm',
	kAudioFormatProperty_AreChannelLayoutsEquivalent	= 'cheq',
    kAudioFormatProperty_ChannelLayoutHash              = 'chha',
	kAudioFormatProperty_ValidateChannelLayout			= 'vacl',
	kAudioFormatProperty_ChannelLayoutForTag			= 'cmpl',
	kAudioFormatProperty_TagForChannelLayout			= 'cmpt',
	kAudioFormatProperty_ChannelLayoutName				= 'lonm',
	kAudioFormatProperty_ChannelLayoutSimpleName		= 'lsnm',
	kAudioFormatProperty_ChannelLayoutForBitmap			= 'cmpb',
	kAudioFormatProperty_ChannelName					= 'cnam',
	kAudioFormatProperty_ChannelShortName				= 'csnm',

	kAudioFormatProperty_TagsForNumberOfChannels		= 'tagc',				
	kAudioFormatProperty_PanningMatrix					= 'panm',
	kAudioFormatProperty_BalanceFade					= 'balf',

//=============================================================================
//	The following properties concern the ID3 Tags
//=============================================================================

	kAudioFormatProperty_ID3TagSize						= 'id3s',
	kAudioFormatProperty_ID3TagToDictionary				= 'id3d'
};

#if TARGET_OS_IPHONE

CF_ENUM(AudioFormatPropertyID) {
	kAudioFormatProperty_HardwareCodecCapabilities		= 'hwcc',
} __attribute__((deprecated));

/*!
	@enum           AudioCodecComponentType
 
	@discussion     Collection of audio codec component types.
					(On macOS these declarations are in AudioToolbox/AudioCodec.h).
 
	@constant		kAudioDecoderComponentType
					A codec that translates data in some other format into linear PCM.
					The component subtype specifies the format ID of the other format.
	@constant		kAudioEncoderComponentType
					A codec that translates linear PCM data into some other format
					The component subtype specifies the format ID of the other format
*/
CF_ENUM(UInt32)
{
	kAudioDecoderComponentType								= 'adec',	
	kAudioEncoderComponentType								= 'aenc',	
};

/*!
	@enum			AudioCodecComponentManufacturer

	@discussion		Audio codec component manufacturer codes. On iPhoneOS, a codec's
					manufacturer can be used to distinguish between hardware and
					software codecs.

	@constant		kAppleSoftwareAudioCodecManufacturer
					Apple software audio codecs.
	@constant		kAppleHardwareAudioCodecManufacturer
					Apple hardware audio codecs.
*/
CF_ENUM(UInt32)
{
	kAppleSoftwareAudioCodecManufacturer					= 'appl',
	kAppleHardwareAudioCodecManufacturer					= 'aphw'
};

#endif


//=============================================================================
//	Routines
//=============================================================================

/*!
    @function	AudioFormatGetPropertyInfo
    @abstract   Retrieve information about the given property
    @param      inPropertyID		an AudioFormatPropertyID constant.
    @param      inSpecifierSize		The size of the specifier data.
    @param      inSpecifier			A specifier is a buffer of data used as an input argument to some of the properties.
    @param      outPropertyDataSize	The size in bytes of the current value of the property. In order to get the property value,
									you will need a buffer of this size.
    @result     returns noErr if successful.
*/
extern OSStatus
AudioFormatGetPropertyInfo(	AudioFormatPropertyID	inPropertyID,
							UInt32					inSpecifierSize,
							const void * __nullable	inSpecifier,
							UInt32 *				outPropertyDataSize)	API_AVAILABLE(macos(10.3), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function	AudioFormatGetProperty
    @abstract   Retrieve the indicated property data
    @param      inPropertyID		an AudioFormatPropertyID constant.
    @param      inSpecifierSize		The size of the specifier data.
    @param      inSpecifier			A specifier is a buffer of data used as an input argument to some of the properties.
    @param      ioPropertyDataSize	on input the size of the outPropertyData buffer. On output the number of bytes written to the buffer.
    @param      outPropertyData		the buffer in which to write the property data. If outPropertyData is NULL and ioPropertyDataSize is
									not, the amount that would have been written will be reported.
    @result     returns noErr if successful.
*/
extern OSStatus
AudioFormatGetProperty(	AudioFormatPropertyID	inPropertyID,
						UInt32					inSpecifierSize,
						const void * __nullable	inSpecifier,
						UInt32 * __nullable		ioPropertyDataSize,
						void * __nullable		outPropertyData)			API_AVAILABLE(macos(10.3), ios(2.0), watchos(2.0), tvos(9.0));


//-----------------------------------------------------------------------------
//  AudioFormat Error Codes
//-----------------------------------------------------------------------------

CF_ENUM(OSStatus)
{
        kAudioFormatUnspecifiedError						= 'what',	// 0x77686174, 2003329396
        kAudioFormatUnsupportedPropertyError 				= 'prop',	// 0x70726F70, 1886547824
        kAudioFormatBadPropertySizeError 					= '!siz',	// 0x2173697A, 561211770
        kAudioFormatBadSpecifierSizeError 					= '!spc',	// 0x21737063, 561213539
        kAudioFormatUnsupportedDataFormatError 				= 'fmt?',	// 0x666D743F, 1718449215
        kAudioFormatUnknownFormatError 						= '!fmt'	// 0x21666D74, 560360820
};


#if defined(__cplusplus)
}
#endif

CF_ASSUME_NONNULL_END

#endif // AudioToolbox_AudioFormat_h
#else
#include <AudioToolboxCore/AudioFormat.h>
#endif
