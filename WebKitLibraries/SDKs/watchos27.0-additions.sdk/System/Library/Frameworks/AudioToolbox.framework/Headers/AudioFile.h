#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioFile.h>)
/*!
	@file		AudioFile.h
	@framework	AudioToolbox.framework
	@copyright	(c) 1985-2015 by Apple, Inc., all rights reserved.
	@abstract	API's to read and write audio files in the filesystem or in memory.
*/

#ifndef AudioToolbox_AudioFile_h
#define AudioToolbox_AudioFile_h

//=============================================================================
//	Includes
//=============================================================================

#include <Availability.h>
#include <CoreAudioTypes/CoreAudioTypes.h>
#include <CoreFoundation/CoreFoundation.h>

CF_ASSUME_NONNULL_BEGIN

#if defined(__cplusplus)
extern "C"
{
#endif

/*!
	@typedef	AudioFileTypeID
	@abstract	Identifier for an audio file type.
*/
typedef UInt32 AudioFileTypeID;

/*!
    Audio File Types
    @abstract   Constants for the built-in audio file types.
    @discussion These constants are used to indicate the type of file to be written, or as a hint to
					what type of file to expect from data provided.
    @constant   kAudioFileAIFFType
					Audio Interchange File Format (AIFF)
    @constant   kAudioFileAIFCType
					Audio Interchange File Format Compressed (AIFF-C)
    @constant   kAudioFileWAVEType
					Microsoft WAVE
    @constant   kAudioFileRF64Type
                    File Format specified in EBU Tech 3306
    @constant   kAudioFileBW64Type
                    File Format specified in ITU-R BS.2088
    @constant   kAudioFileWave64Type
                    Sony Pictures Digital Wave 64
    @constant   kAudioFileSoundDesigner2Type
					Sound Designer II
    @constant   kAudioFileNextType
					NeXT / Sun
    @constant   kAudioFileMP3Type
					MPEG Audio Layer 3 (.mp3)
    @constant   kAudioFileMP2Type
					MPEG Audio Layer 2 (.mp2)
    @constant   kAudioFileMP1Type
					MPEG Audio Layer 1 (.mp1)
    @constant   kAudioFileAC3Type
					AC-3
    @constant   kAudioFileAAC_ADTSType
					Advanced Audio Coding (AAC) Audio Data Transport Stream (ADTS)
    @constant   kAudioFileMPEG4Type
    @constant   kAudioFileM4AType
    @constant   kAudioFileM4BType
    @constant   kAudioFileCAFType
                    CoreAudio File Format
    @constant   kAudioFile3GPType
    @constant   kAudioFile3GP2Type
    @constant   kAudioFileAMRType
    @constant   kAudioFileFLACType
                    Free Lossless Audio Codec
    @constant   kAudioFileLATMInLOASType
                    Low-overhead audio stream with low-overhead audio transport multiplex, per ISO/IEC 14496-3.
                    Support is limited to AudioSyncStream using AudioMuxElement with mux config present.
*/
CF_ENUM(AudioFileTypeID) {
        kAudioFileAIFFType				= 'AIFF',
        kAudioFileAIFCType				= 'AIFC',
        kAudioFileWAVEType				= 'WAVE',
        kAudioFileRF64Type              = 'RF64',
        kAudioFileBW64Type              = 'BW64',
        kAudioFileWave64Type            = 'W64f',
        kAudioFileSoundDesigner2Type	= 'Sd2f',
        kAudioFileNextType				= 'NeXT',
        kAudioFileMP3Type				= 'MPG3',	// mpeg layer 3
        kAudioFileMP2Type				= 'MPG2',	// mpeg layer 2
        kAudioFileMP1Type				= 'MPG1',	// mpeg layer 1
		kAudioFileAC3Type				= 'ac-3',
        kAudioFileAAC_ADTSType			= 'adts',
        kAudioFileMPEG4Type             = 'mp4f',
        kAudioFileM4AType               = 'm4af',
        kAudioFileM4BType               = 'm4bf',
		kAudioFileCAFType				= 'caff',
		kAudioFile3GPType				= '3gpp',
		kAudioFile3GP2Type				= '3gp2',		
		kAudioFileAMRType				= 'amrf',
		kAudioFileFLACType				= 'flac',
		kAudioFileLATMInLOASType		= 'loas'
};

/*!
    AudioFile error codes
    @abstract   These are the error codes returned from the AudioFile API.
    @constant   kAudioFileUnspecifiedError 
		An unspecified error has occurred.
    @constant   kAudioFileUnsupportedFileTypeError 
		The file type is not supported.
    @constant   kAudioFileUnsupportedDataFormatError 
		The data format is not supported by this file type.
    @constant   kAudioFileUnsupportedPropertyError 
		The property is not supported.
    @constant   kAudioFileBadPropertySizeError 
		The size of the property data was not correct.
    @constant   kAudioFilePermissionsError 
		The operation violated the file permissions. For example, trying to write to a file opened with kAudioFileReadPermission.
    @constant   kAudioFileNotOptimizedError 
		There are chunks following the audio data chunk that prevent extending the audio data chunk. 
		The file must be optimized in order to write more audio data.
    @constant   kAudioFileInvalidChunkError 
		The chunk does not exist in the file or is not supported by the file. 
    @constant   kAudioFileDoesNotAllow64BitDataSizeError 
		The a file offset was too large for the file type. AIFF and WAVE have a 32 bit file size limit. 
    @constant   kAudioFileInvalidPacketOffsetError 
		A packet offset was past the end of the file, or not at the end of the file when writing a VBR format, 
		or a corrupt packet size was read when building the packet table. 
    @constant   kAudioFileInvalidPacketDependencyError
		Either the packet dependency info that's necessary for the audio format has not been provided,
		or the provided packet dependency info indicates dependency on a packet that's unavailable.
    @constant   kAudioFileInvalidFileError 
		The file is malformed, or otherwise not a valid instance of an audio file of its type. 
    @constant   kAudioFileOperationNotSupportedError 
		The operation cannot be performed. For example, setting kAudioFilePropertyAudioDataByteCount to increase 
		the size of the audio data in a file is not a supported operation. Write the data instead.
    @constant   kAudioFileNotOpenError 
		The file is closed.
	@constant   kAudioFileEndOfFileError 
		End of file.
	@constant   kAudioFilePositionError 
		Invalid file position.
	@constant   kAudioFileFileNotFoundError 
		File not found.
 */
CF_ENUM(OSStatus) {
        kAudioFileUnspecifiedError						= 'wht?',		// 0x7768743F, 2003334207
        kAudioFileUnsupportedFileTypeError 				= 'typ?',		// 0x7479703F, 1954115647
        kAudioFileUnsupportedDataFormatError 			= 'fmt?',		// 0x666D743F, 1718449215
        kAudioFileUnsupportedPropertyError 				= 'pty?',		// 0x7074793F, 1886681407
        kAudioFileBadPropertySizeError 					= '!siz',		// 0x2173697A,  561211770
        kAudioFilePermissionsError	 					= 'prm?',		// 0x70726D3F, 1886547263
        kAudioFileNotOptimizedError						= 'optm',		// 0x6F70746D, 1869640813
        // file format specific error codes
        kAudioFileInvalidChunkError						= 'chk?',		// 0x63686B3F, 1667787583
        kAudioFileDoesNotAllow64BitDataSizeError		= 'off?',		// 0x6F66663F, 1868981823
        kAudioFileInvalidPacketOffsetError				= 'pck?',		// 0x70636B3F, 1885563711
        kAudioFileInvalidPacketDependencyError			= 'dep?',		// 0x6465703F, 1684369471
        kAudioFileInvalidFileError						= 'dta?',		// 0x6474613F, 1685348671
		kAudioFileOperationNotSupportedError			= 0x6F703F3F, 	// 'op??', integer used because of trigraph

		// general file error codes
		kAudioFileNotOpenError							= -38,
		kAudioFileEndOfFileError						= -39,
		kAudioFilePositionError							= -40,
		kAudioFileFileNotFoundError						= -43
};

/*!
    @enum AudioFileFlags
    @abstract   These are flags that can be used with the CreateURL API call
    @constant   kAudioFileFlags_EraseFile 
		If set, then the CreateURL call will erase the contents of an existing file
		If not set, then the CreateURL call will fail if the file already exists
    @constant   kAudioFileFlags_DontPageAlignAudioData 
		Normally, newly created and optimized files will have padding added in order to page align 
		the data to 4KB boundaries. This makes reading the data more efficient. 
		When disk space is a concern, this flag can be set so that the padding will not be added.
*/
typedef CF_OPTIONS(UInt32, AudioFileFlags) {
	kAudioFileFlags_EraseFile = 1,
	kAudioFileFlags_DontPageAlignAudioData = 2
};

typedef CF_ENUM(SInt8, AudioFilePermissions) {
	kAudioFileReadPermission      = 0x01,
	kAudioFileWritePermission     = 0x02,
	kAudioFileReadWritePermission = 0x03
};

//=============================================================================
//	Types specific to the Audio File API
//=============================================================================

/*!
    @typedef	AudioFileID
    @abstract   An opaque reference to an AudioFile object.
*/
typedef	struct OpaqueAudioFileID	*AudioFileID;
/*!
    @typedef	AudioFilePropertyID
    @abstract   A constant for an AudioFile property.
*/
typedef UInt32			AudioFilePropertyID;

/*!
    enum		AudioFileLoopDirection
    @abstract   These constants describe the playback direction of a looped segment of a file.
    @constant   kAudioFileLoopDirection_NoLooping
					The segment is not looped.
    @constant   kAudioFileLoopDirection_Forward
					play segment forward.
    @constant   kAudioFileLoopDirection_Backward
					play segment backward.
    @constant   kAudioFileLoopDirection_ForwardAndBackward
					play segment forward and backward.
*/
CF_ENUM(UInt32) {
	kAudioFileLoopDirection_NoLooping = 0,
	kAudioFileLoopDirection_Forward = 1,
	kAudioFileLoopDirection_ForwardAndBackward = 2,
	kAudioFileLoopDirection_Backward = 3
};

//=============================================================================
//	Markers, Regions
//=============================================================================


/*!
    @struct		AudioFile_SMPTE_Time
    @abstract   A struct for describing a SMPTE time.
    @var        mHours						The hours.
    @var        mMinutes					The minutes.
    @var        mSeconds					The seconds.
    @var        mFrames						The frames.
    @var        mSubFrameSampleOffset		The sample offset within a frame.
*/
struct AudioFile_SMPTE_Time
{
	SInt8 mHours;
	UInt8 mMinutes;
	UInt8 mSeconds;
	UInt8 mFrames;
	UInt32 mSubFrameSampleOffset;
};
typedef struct AudioFile_SMPTE_Time AudioFile_SMPTE_Time;


/*!
    enum		AudioFileMarkerType
    @abstract   constants for types of markers within a file. Used in the mType field of AudioFileMarker.
    @constant   kAudioFileMarkerType_Generic
    				A generic marker. See CAFFile.h for marker types specific to CAF files.
*/
CF_ENUM(UInt32) {
	kAudioFileMarkerType_Generic			= 0,
};


/*!
    @struct		AudioFileMarker
    @abstract   A marker annotates a position in an audio file with additional information.
    @discussion (description)
    @var        mFramePosition	The frame in the file counting from the start of the audio data.
    @var        mName			The name of this marker.
    @var        mMarkerID		A unique ID for this marker.
    @var        mSMPTETime		The SMPTE time for this marker.
    @var        mType			The marker type.
    @var        mReserved		A reserved field. Set to zero.
    @var        mChannel		The channel number that the marker refers to. Set to zero if marker applies to all channels.
*/
struct AudioFileMarker
{
	Float64 mFramePosition;
	
	CFStringRef	__nullable	mName;
	SInt32					mMarkerID;

	AudioFile_SMPTE_Time	mSMPTETime;
	UInt32					mType;
	UInt16					mReserved;
	UInt16					mChannel;
};
typedef struct AudioFileMarker AudioFileMarker;

/*!
    @struct		AudioFileMarkerList
    @abstract   A list of AudioFileMarker.
    @var        mSMPTE_TimeType
					This defines the SMPTE timing scheme used in the marker list. See CAFFile.h for the values used here.
    @var        mNumberMarkers
					The number of markers in the mMarkers list.
    @var        mMarkers
					A list of AudioFileMarker.
*/
struct AudioFileMarkerList
{
	UInt32				mSMPTE_TimeType;
	UInt32				mNumberMarkers;
	AudioFileMarker		mMarkers[1]; // this is a variable length array of mNumberMarkers elements
};
typedef struct AudioFileMarkerList AudioFileMarkerList;

/*!
    @function	NumBytesToNumAudioFileMarkers
    @abstract   Converts a size in bytes to the number of AudioFileMarkers that can be contained in that number of bytes.
    @discussion This can be used for the kAudioFilePropertyMarkerList property when calculating the number of 
				markers that will be returned. 
    @param      inNumBytes 
					a number of bytes.
    @result     the number of AudioFileMarkers that can be contained in that number of bytes.
*/
#ifdef CF_INLINE
CF_INLINE size_t NumBytesToNumAudioFileMarkers(size_t inNumBytes)
{
	return (inNumBytes<offsetof(AudioFileMarkerList, mMarkers[0]) ? 0 : (inNumBytes - offsetof(AudioFileMarkerList, mMarkers[0])) / sizeof(AudioFileMarker));
}
#else
#define NumBytesToNumAudioFileMarkers(inNumBytes) \
	((inNumBytes)<offsetof(AudioFileMarkerList, mMarkers[0])?0:((inNumBytes) - offsetof(AudioFileMarkerList, mMarkers[0])) / sizeof(AudioFileMarker))
#endif

/*!
    @function	NumAudioFileMarkersToNumBytes
    @abstract   Converts a number of AudioFileMarkers to a size in bytes.
    @discussion This can be used for the kAudioFilePropertyMarkerList property when calculating the size required to 
				contain a number of AudioFileMarkers. 
    @param      inNumMarkers 
					a number of AudioFileMarkers.
    @result     the size in bytes required to contain that number of AudioFileMarkers.
*/
#ifdef CF_INLINE
CF_INLINE size_t NumAudioFileMarkersToNumBytes(size_t inNumMarkers)
{
	return (offsetof(AudioFileMarkerList, mMarkers) + (inNumMarkers) * sizeof(AudioFileMarker));
}
#else
#define NumAudioFileMarkersToNumBytes(inNumMarkers) \
	(offsetof(AudioFileMarkerList, mMarkers) + (inNumMarkers) * sizeof(AudioFileMarker))
#endif

/*!
    @enum		AudioFileRegionFlags
    @abstract   These are flags for an AudioFileRegion that specify a playback direction.
    @discussion One or multiple of these flags can be set. For example, if both kAudioFileRegionFlag_LoopEnable and 
				kAudioFileRegionFlag_PlayForward are set, then the region will play as a forward loop. If only 
				kAudioFileRegionFlag_PlayForward is set, then the region will be played forward once.
    @constant   kAudioFileRegionFlag_LoopEnable
					If this flag is set, the region will be looped. One or both of the following must also be set.
    @constant   kAudioFileRegionFlag_PlayForward
					If this flag is set, the region will be played forward.
    @constant   kAudioFileRegionFlag_PlayBackward
					If this flag is set, the region will be played backward.
*/
typedef CF_OPTIONS(UInt32, AudioFileRegionFlags) {
	kAudioFileRegionFlag_LoopEnable = 1,
	kAudioFileRegionFlag_PlayForward = 2,
	kAudioFileRegionFlag_PlayBackward = 4
};

/*!
    @struct		AudioFileRegion
    @abstract   An AudioFileRegion specifies a segment of audio data.
    @discussion Generally a region consists of at least two markers marking the beginning and end of the segment.
				There may also be other markers defining other meta information such as sync point.
    @var        mRegionID 
					each region must have a unique ID.
    @var        mName 
					The name of the region.
    @var        mFlags 
					AudioFileRegionFlags.
    @var        mNumberMarkers 
					The number of markers in the mMarkers array.
    @var        mMarkers 
					A variable length array of AudioFileMarkers.
*/
struct AudioFileRegion
{
	UInt32					mRegionID;
	CFStringRef				mName;
	AudioFileRegionFlags	mFlags;
	UInt32					mNumberMarkers;
	AudioFileMarker			mMarkers[1]; // this is a variable length array of mNumberMarkers elements
};
typedef struct AudioFileRegion AudioFileRegion;


/*!
    @struct		AudioFileRegionList
    @abstract   A list of the AudioFileRegions in a file.
    @discussion This is the struct used by the kAudioFilePropertyRegionList property.
    @var        mSMPTE_TimeType
					This defines the SMPTE timing scheme used in the file. See CAFFile.h for the values used here.
    @var        mNumberRegions
					The number of regions in the mRegions list.
    @var        mRegions
					A list of AudioFileRegions. Note that AudioFileMarkers are variable length, so this list cannot 
					be accessed as an array. Use the NextAudioFileRegion macro for traversing the list instead.
*/
struct AudioFileRegionList
{
	UInt32				mSMPTE_TimeType;
	UInt32				mNumberRegions;
	AudioFileRegion		mRegions[1]; // this is a variable length array of mNumberRegions elements
};
typedef struct AudioFileRegionList AudioFileRegionList;

/*!
    @function	NextAudioFileRegion
    @abstract   convenience macro for traversing the region list.
    @discussion because AudioFileRegions are variable length, you cannot access them as an array. Use NextAudioFileRegion 
				to walk the list.
    @param      inAFRegionPtr 
					a pointer to the current region.
    @result     a pointer to the region after the current region. This does not protect you from walking off the end of the list, 
				so obey mNumberRegions.
*/
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wcast-align"
#ifdef CF_INLINE
CF_INLINE AudioFileRegion *NextAudioFileRegion(const AudioFileRegion *inAFRegionPtr)
{
	return ((AudioFileRegion*)((char*)inAFRegionPtr + offsetof(AudioFileRegion, mMarkers) + (inAFRegionPtr->mNumberMarkers)*sizeof(AudioFileMarker)));
}
#else
#define NextAudioFileRegion(inAFRegionPtr) \
        ((AudioFileRegion*)((char*)(inAFRegionPtr) + offsetof(AudioFileRegion, mMarkers) + ((inAFRegionPtr)->mNumberMarkers)*sizeof(AudioFileMarker)))
#endif
#pragma clang diagnostic pop

/*!
    @struct		AudioFramePacketTranslation
    @abstract   used for properties kAudioFilePropertyPacketToFrame and kAudioFilePropertyFrameToPacket
    @discussion See description of kAudioFilePropertyPacketToFrame and kAudioFilePropertyFrameToPacket
    @var        mFrame		a frame number.
    @var        mPacket		a packet number.
    @var        mFrameOffsetInPacket		a frame offset in a packet.
*/
struct AudioFramePacketTranslation
{
	SInt64 mFrame;
	SInt64 mPacket;
	UInt32 mFrameOffsetInPacket;
};
typedef struct AudioFramePacketTranslation AudioFramePacketTranslation;


/*!
    @enum		AudioBytePacketTranslation Flags
	
    @abstract   flags for the AudioBytePacketTranslation mFlags field
    @discussion		There is currently only one flag.
					
    @constant   kBytePacketTranslationFlag_IsEstimate
					If the set then the result value is an estimate.
*/
typedef CF_OPTIONS(UInt32, AudioBytePacketTranslationFlags) {
	kBytePacketTranslationFlag_IsEstimate = 1
};

/*!
    @struct		AudioBytePacketTranslation
    @abstract   used for properties kAudioFileByteToPacket and kAudioFilePacketToByte
    @discussion See description of kAudioFileByteToPacket and kAudioFilePacketToByte
    @var        mByte		a byte number.
    @var        mPacket		a packet number.
    @var        mByteOffsetInPacket		a byte offset in a packet.
    @var        mFlags		if kBytePacketTranslationFlag_IsEstimate is set, then the value is an estimate.
*/
struct AudioBytePacketTranslation
{
	SInt64 mByte;
	SInt64 mPacket;
	UInt32 mByteOffsetInPacket;
	AudioBytePacketTranslationFlags mFlags;
};
typedef struct AudioBytePacketTranslation AudioBytePacketTranslation;


/*!
    @struct		AudioFilePacketTableInfo
    @abstract   This contains information about the number of valid frames in a file and where they begin and end.
    @discussion	Some data formats may have packets whose contents are not completely valid, but represent priming or remainder 
				frames that are not meant to be played. For example a file with 100 packets of AAC is nominally 1024 * 100 = 102400 frames
				of data. However the first 2112 frames of that may be priming frames and there may be some 
				number of remainder frames added to pad out to a full packet of 1024 frames. The priming and remainder frames should be 
				discarded. The total number of packets in the file times the frames per packet (or counting each packet's frames 
				individually for a variable frames per packet format) minus mPrimingFrames, minus mRemainderFrames, should 
				equal mNumberValidFrames.
    @var        mNumberValidFrames the number of valid frames in the file.
    @var        mPrimingFrames the number of invalid frames at the beginning of the file.
    @var        mRemainderFrames the number of invalid frames at the end of the file.
*/
struct AudioFilePacketTableInfo
{
        SInt64  mNumberValidFrames;
        SInt32  mPrimingFrames;
        SInt32  mRemainderFrames;
};
typedef struct AudioFilePacketTableInfo AudioFilePacketTableInfo;


/*!
    @struct	AudioPacketRangeByteCountTranslation
    @abstract   used for property kAudioFilePropertyPacketRangeByteCountUpperBound
    @discussion See description of kAudioFilePropertyPacketRangeByteCountUpperBound
    @var        mPacket					a packet number.
    @var        mPacketCount			a packet count.
    @var        mByteCountUpperBound	an upper bound for the total size of the specified packets.
*/
struct AudioPacketRangeByteCountTranslation
{
	SInt64 mPacket;
	SInt64 mPacketCount;
	SInt64 mByteCountUpperBound;
};
typedef struct AudioPacketRangeByteCountTranslation AudioPacketRangeByteCountTranslation;


/*!
    @struct		AudioPacketRollDistanceTranslation
    @abstract   used for property kAudioFilePropertyPacketToRollDistance
    @discussion See descriptions of kAudioFilePropertyPacketToRollDistance and kAudioFilePropertyRestrictsRandomAccess
    @var        mPacket         a packet number
    @var        mRollDistance   a count of packets that must be decoded prior to the packet with the specified number in order to achieve the best practice for the decoding of that packet
*/
struct AudioPacketRollDistanceTranslation
{
	SInt64 mPacket;
	SInt64 mRollDistance;
};
typedef struct AudioPacketRollDistanceTranslation AudioPacketRollDistanceTranslation;


/*!
    @struct		AudioIndependentPacketTranslation
    @abstract   used for property kAudioFilePropertyPreviousIndependentPacket and kAudioFilePropertyNextIndependentPacket
    @discussion See descriptions of kAudioFilePropertyPreviousIndependentPacket and kAudioFilePropertyNextIndependentPacket
    @var        mPacket                         a packet number
    @var        mIndependentlyDecodablePacket   a packet number not equal to mPacket of an independent packet
*/
struct AudioIndependentPacketTranslation
{
	SInt64 mPacket;
	SInt64 mIndependentlyDecodablePacket;
};
typedef struct AudioIndependentPacketTranslation AudioIndependentPacketTranslation;


/*!
    @struct		AudioPacketDependencyInfoTranslation
    @abstract   used for property kAudioFilePropertyPacketToDependencyInfo
    @discussion See descriptions of kAudioFilePropertyPacketToDependencyInfo and kAudioFilePropertyRestrictsRandomAccess
    @var        mPacket                     a packet number
    @var        mIsIndependentlyDecodable   1 means that the specified packet is independently decodable; 0 means it's not
    @var        mNumberPrerollPackets       if the packet is independently decodable, the count of packets that must be decoded after the packet with the specified number in order to refresh the decoder
*/
struct AudioPacketDependencyInfoTranslation
{
	SInt64 mPacket;
	UInt32 mIsIndependentlyDecodable;
	UInt32 mNumberPrerollPackets;
};
typedef struct AudioPacketDependencyInfoTranslation AudioPacketDependencyInfoTranslation;


//=============================================================================
//	Info String Keys
//=============================================================================

// Get key values from the InfoDictionary by making CFStrings from the following constants

#define kAFInfoDictionary_Album							"album"
#define kAFInfoDictionary_ApproximateDurationInSeconds  "approximate duration in seconds"
#define kAFInfoDictionary_Artist                        "artist"
#define kAFInfoDictionary_ChannelLayout					"channel layout"
#define kAFInfoDictionary_Comments						"comments"
#define kAFInfoDictionary_Composer						"composer"
#define kAFInfoDictionary_Copyright						"copyright"
#define kAFInfoDictionary_EncodingApplication           "encoding application"
#define kAFInfoDictionary_Genre							"genre"
#define kAFInfoDictionary_ISRC							"ISRC"					// International Standard Recording Code
#define kAFInfoDictionary_KeySignature					"key signature"
#define kAFInfoDictionary_Lyricist						"lyricist"
#define kAFInfoDictionary_NominalBitRate                "nominal bit rate"
#define kAFInfoDictionary_RecordedDate					"recorded date"
#define kAFInfoDictionary_SourceBitDepth				"source bit depth"
#define kAFInfoDictionary_SourceEncoder					"source encoder"
#define kAFInfoDictionary_SubTitle						"subtitle"
#define kAFInfoDictionary_Tempo							"tempo"
#define kAFInfoDictionary_TimeSignature					"time signature"
#define kAFInfoDictionary_Title							"title"
#define kAFInfoDictionary_TrackNumber                   "track number"
#define kAFInfoDictionary_Year							"year"

//=============================================================================
//	Routines
//=============================================================================

/*!
    @function	AudioFileCreateWithURL
    @abstract   creates a new audio file (or initialises an existing file)
    @discussion	creates a new (or initialises an existing) audio file specified by the URL.
					Upon success, an AudioFileID is returned which can be used for subsequent calls 
					to the AudioFile APIs.
    @param inFileRef		an CFURLRef fully specifying the path of the file to create/initialise
    @param inFileType		an AudioFileTypeID indicating the type of audio file to create.
    @param inFormat			an AudioStreamBasicDescription describing the data format that will be
							added to the audio file.
    @param inFlags			relevant flags for creating/opening the file. 
								if kAudioFileFlags_EraseFile is set, it will erase an existing file
								 if not set, then the Create call will fail if the URL is an existing file
    @param outAudioFile		if successful, an AudioFileID that can be used for subsequent AudioFile calls.
    @result					returns noErr if successful.
*/
extern OSStatus	
AudioFileCreateWithURL (CFURLRef						inFileRef,
                    AudioFileTypeID						inFileType,
                    const AudioStreamBasicDescription	*inFormat,
                    AudioFileFlags						inFlags,
                    AudioFileID	__nullable * __nonnull	outAudioFile)		API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function				AudioFileOpenURL
    @abstract				Open an existing audio file.
    @discussion				Open an existing audio file for reading or reading and writing.
    @param inFileRef		the CFURLRef of an existing audio file.
    @param inPermissions	use the permission constants
    @param inFileTypeHint	For files which have no filename extension and whose type cannot be easily or
							uniquely determined from the data (ADTS,AC3), this hint can be used to indicate the file type. 
							Otherwise you can pass zero for this. The hint is only used on OS versions 10.3.1 or greater.
							For OS versions prior to that, opening files of the above description will fail.
    @param outAudioFile		upon success, an AudioFileID that can be used for subsequent
							AudioFile calls.
    @result					returns noErr if successful.
*/
extern OSStatus	
AudioFileOpenURL (	CFURLRef							inFileRef,
					AudioFilePermissions				inPermissions,
					AudioFileTypeID						inFileTypeHint,
					AudioFileID	__nullable * __nonnull	outAudioFile)					API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @typedef	AudioFile_ReadProc
    @abstract   A callback for reading data. used with AudioFileOpenWithCallbacks or AudioFileInitializeWithCallbacks.
    @discussion a function that will be called when AudioFile needs to read data.
    @param      inClientData	A pointer to the client data as set in the inClientData parameter to AudioFileXXXWithCallbacks.
    @param      inPosition		An offset into the data from which to read.
    @param      requestCount	The number of bytes to read.
    @param      buffer			The buffer in which to put the data read.
    @param      actualCount		The callback should set this to the number of bytes successfully read.
    @result						The callback should return noErr on success, or an appropriate error code on failure.
*/
typedef OSStatus (*AudioFile_ReadProc)(
								void *		inClientData,
								SInt64		inPosition, 
								UInt32		requestCount,
								void *		buffer, 
								UInt32 *	actualCount);

/*!
    @typedef	AudioFile_WriteProc
    @abstract   A callback for writing data. used with AudioFileOpenWithCallbacks or AudioFileInitializeWithCallbacks.
    @discussion a function that will be called when AudioFile needs to write data.
    @param      inClientData	A pointer to the client data as set in the inClientData parameter to AudioFileXXXWithCallbacks.
    @param      inPosition		An offset into the data from which to read.
    @param      requestCount	The number of bytes to write.
    @param      buffer			The buffer containing the data to write.
    @param      actualCount		The callback should set this to the number of bytes successfully written.
    @result						The callback should return noErr on success, or an appropriate error code on failure.
*/
typedef OSStatus (*AudioFile_WriteProc)(
								void * 		inClientData,
								SInt64		inPosition, 
								UInt32		requestCount, 
								const void *buffer, 
								UInt32    * actualCount);
								
/*!
    @typedef	AudioFile_GetSizeProc
    @abstract   A callback for getting the size of the file data. used with AudioFileOpenWithCallbacks or AudioFileInitializeWithCallbacks.
    @discussion a function that will be called when AudioFile needs to determine the size of the file data. This size is for all of the 
				data in the file, not just the audio data.
    @param      inClientData	A pointer to the client data as set in the inClientData parameter to AudioFileXXXWithCallbacks.
    @result						The callback should return the size of the data.
*/
typedef SInt64 (*AudioFile_GetSizeProc)(
								void * 		inClientData);

/*!
    @typedef	AudioFile_SetSizeProc
    @abstract   A callback for setting the size of the file data. used with AudioFileOpenWithCallbacks or AudioFileInitializeWithCallbacks.
    @discussion a function that will be called when AudioFile needs to set the size of the file data. This size is for all of the 
				data in the file, not just the audio data. This will only be called if the file is written to.
    @param      inClientData	A pointer to the client data as set in the inClientData parameter to AudioFileXXXWithCallbacks.
    @result						The callback should return the size of the data.
*/
typedef OSStatus (*AudioFile_SetSizeProc)(
								void *		inClientData,
								SInt64		inSize);

/*!
    @function	AudioFileInitializeWithCallbacks
    @abstract   Wipe clean an existing file. You provide callbacks that the AudioFile API
				will use to get the data.
    @param inClientData		a constant that will be passed to your callbacks.
	@param inReadFunc		a function that will be called when AudioFile needs to read data.
	@param inWriteFunc		a function that will be called when AudioFile needs to write data.
	@param inGetSizeFunc	a function that will be called when AudioFile needs to know the file size.
	@param inSetSizeFunc	a function that will be called when AudioFile needs to set the file size.
	
    @param inFileType 		an AudioFileTypeID indicating the type of audio file to which to initialize the file. 
    @param inFormat 		an AudioStreamBasicDescription describing the data format that will be
							added to the audio file.
    @param inFlags			flags for creating/opening the file. Currently zero.
    @param outAudioFile		upon success, an AudioFileID that can be used for subsequent
							AudioFile calls.
    @result					returns noErr if successful.
*/
extern OSStatus	
AudioFileInitializeWithCallbacks (	
						void *								inClientData, 
						AudioFile_ReadProc					inReadFunc, 
						AudioFile_WriteProc					inWriteFunc, 
						AudioFile_GetSizeProc				inGetSizeFunc,
						AudioFile_SetSizeProc				inSetSizeFunc,
                        AudioFileTypeID						inFileType,
                        const AudioStreamBasicDescription	*inFormat,
                        AudioFileFlags						inFlags,
                        AudioFileID	__nullable * __nonnull	outAudioFile)	API_AVAILABLE(macos(10.3), ios(2.0), watchos(2.0), tvos(9.0));


/*!
    @function	AudioFileOpenWithCallbacks
    @abstract   Open an existing file. You provide callbacks that the AudioFile API
				will use to get the data.
    @param inClientData					a constant that will be passed to your callbacks.
	@param inReadFunc					a function that will be called when AudioFile needs to read data.
	@param inWriteFunc					a function that will be called when AudioFile needs to write data.
	@param inGetSizeFunc				a function that will be called when AudioFile needs to know the total file size.
	@param inSetSizeFunc				a function that will be called when AudioFile needs to set the file size.
	
    @param inFileTypeHint	For files which have no filename extension and whose type cannot be easily or
							uniquely determined from the data (ADTS,AC3), this hint can be used to indicate the file type. 
							Otherwise you can pass zero for this. The hint is only used on OS versions 10.3.1 or greater.
							For OS versions prior to that, opening files of the above description will fail.
    @param outAudioFile		upon success, an AudioFileID that can be used for subsequent
							AudioFile calls.
    @result					returns noErr if successful.
*/
extern OSStatus	
AudioFileOpenWithCallbacks (
				void *								inClientData,
				AudioFile_ReadProc					inReadFunc,
				AudioFile_WriteProc __nullable		inWriteFunc,
				AudioFile_GetSizeProc				inGetSizeFunc,
				AudioFile_SetSizeProc __nullable	inSetSizeFunc,
                AudioFileTypeID						inFileTypeHint,
                AudioFileID	__nullable * __nonnull	outAudioFile)				API_AVAILABLE(macos(10.3), ios(2.0), watchos(2.0), tvos(9.0));
				

/*!
    @function	AudioFileClose
    @abstract   Close an existing audio file.
    @param      inAudioFile		an AudioFileID.
    @result						returns noErr if successful.
*/
extern OSStatus
AudioFileClose	(AudioFileID		inAudioFile)							API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function	AudioFileOptimize
    @abstract   Move the audio data to the end of the file and other internal optimizations of the file structure.
	@discussion			Optimize the file so additional audio data can be appended to 
                        the existing data. Generally, this will place the audio data at 
                        the end of the file so additional writes can be placed to the 
                        file end. This can be a potentially expensive and time-consuming operation 
                        and should not be used during time critical operations. There is 
                        a kAudioFilePropertyIsOptimized property for checking on the optimized state 
                        of the file.
    @param      inAudioFile		an AudioFileID.
    @result						returns noErr if successful.
*/
extern OSStatus	
AudioFileOptimize (AudioFileID  	inAudioFile)							API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function	AudioFileReadBytes
    @abstract   Read bytes of audio data from the audio file. 
				
    @discussion				Returns kAudioFileEndOfFileError when read encounters end of file.
    @param inAudioFile		an AudioFileID.
    @param inUseCache 		true if it is desired to cache the data upon read, else false
    @param inStartingByte	the byte offset of the audio data desired to be returned
    @param ioNumBytes 		on input, the number of bytes to read, on output, the number of
							bytes actually read.
    @param outBuffer 		outBuffer should be a void * to user allocated memory large enough for the requested bytes. 
    @result					returns noErr if successful.
*/
extern OSStatus	
AudioFileReadBytes (	AudioFileID  	inAudioFile,
                        Boolean			inUseCache,
                        SInt64			inStartingByte, 
                        UInt32			*ioNumBytes, 
                        void			*outBuffer)							API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function				AudioFileWriteBytes
    @abstract				Write bytes of audio data to the audio file.
    @param inAudioFile		an AudioFileID.
    @param inUseCache 		true if it is desired to cache the data upon write, else false
    @param inStartingByte	the byte offset where the audio data should be written
    @param ioNumBytes 		on input, the number of bytes to write, on output, the number of
							bytes actually written.
    @param inBuffer 		inBuffer should be a void * containing the bytes to be written 
    @result					returns noErr if successful.
*/
extern OSStatus	
AudioFileWriteBytes (	AudioFileID  	inAudioFile,  
                        Boolean			inUseCache,
                        SInt64			inStartingByte, 
                        UInt32			*ioNumBytes, 
                        const void		*inBuffer)							API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function	AudioFileReadPacketData
    @abstract   Read packets of audio data from the audio file.
    @discussion AudioFileReadPacketData reads as many of the requested number of packets
				as will fit in the buffer size given by ioNumPackets.
				Unlike the deprecated AudioFileReadPackets, ioNumPackets must be initialized.
				If the byte size of the number packets requested is 
				less than the buffer size, ioNumBytes will be reduced.
				If the buffer is too small for the number of packets 
				requested, ioNumPackets and ioNumBytes will be reduced 
				to the number of packets that can be accommodated and their byte size.
				Returns kAudioFileEndOfFileError when read encounters end of file.
				For all uncompressed formats, packets == frames.

    @param inAudioFile				an AudioFileID.
    @param inUseCache 				true if it is desired to cache the data upon read, else false
    @param ioNumBytes				on input the size of outBuffer in bytes. 
									on output, the number of bytes actually returned.
    @param outPacketDescriptions 	An array of packet descriptions describing the packets being returned. 
									The size of the array must be greater or equal to the number of packets requested. 
									On return the packet description will be filled out with the packet offsets and sizes.
									Packet descriptions are ignored for CBR data.   
    @param inStartingPacket 		The packet index of the first packet desired to be returned
    @param ioNumPackets 			on input, the number of packets to read, on output, the number of
									packets actually read.
    @param outBuffer 				outBuffer should be a pointer to user allocated memory.
    @result							returns noErr if successful.
*/
extern OSStatus	
AudioFileReadPacketData (	AudioFileID  					inAudioFile, 
                       		Boolean							inUseCache,
                       		UInt32 *						ioNumBytes,
                       		AudioStreamPacketDescription * __nullable outPacketDescriptions,
                       		SInt64							inStartingPacket, 
                       		UInt32 * 						ioNumPackets,
                       		void * __nullable				outBuffer)			API_AVAILABLE(macos(10.6), ios(2.2), watchos(2.0), tvos(9.0));

/*!
    @function	AudioFileReadPackets
    @abstract   Read packets of audio data from the audio file.
    @discussion AudioFileReadPackets is DEPRECATED. Use AudioFileReadPacketData instead.
				READ THE HEADER DOC FOR AudioFileReadPacketData. It is not a drop-in replacement.
				In particular, for AudioFileReadPacketData ioNumBytes must be initialized to the buffer size.
				AudioFileReadPackets assumes you have allocated your buffer to ioNumPackets times the maximum packet size.
				For many compressed formats this will only use a portion of the buffer since the ratio of the maximum 
				packet size to the typical packet size can be large. Use AudioFileReadPacketData instead.
	
    @param inAudioFile				an AudioFileID.
    @param inUseCache 				true if it is desired to cache the data upon read, else false
    @param outNumBytes				on output, the number of bytes actually returned
    @param outPacketDescriptions 	on output, an array of packet descriptions describing
									the packets being returned. NULL may be passed for this
									parameter. Nothing will be returned for linear pcm data.   
    @param inStartingPacket 		the packet index of the first packet desired to be returned
    @param ioNumPackets 			on input, the number of packets to read, on output, the number of
									packets actually read.
    @param outBuffer 				outBuffer should be a pointer to user allocated memory of size: 
									number of packets requested times file's maximum (or upper bound on)
									packet size.
    @result							returns noErr if successful.
*/
extern OSStatus	
AudioFileReadPackets (	AudioFileID  					inAudioFile, 
                        Boolean							inUseCache,
                        UInt32 *						outNumBytes,
                        AudioStreamPacketDescription * __nullable outPacketDescriptions,
                        SInt64							inStartingPacket, 
                        UInt32 * 						ioNumPackets,
                        void * __nullable				outBuffer)			API_DEPRECATED("no longer supported", macos(10.2, 10.10), ios(2.0, 8.0)) __WATCHOS_PROHIBITED __TVOS_PROHIBITED;


/*!
    @function	AudioFileWritePackets
    @abstract   Write packets of audio data to the audio file.
    @discussion For all uncompressed formats, packets == frames.
    @param inAudioFile				an AudioFileID.
    @param inUseCache 				true if it is desired to cache the data upon write, else false
    @param inNumBytes				the number of bytes being provided for write
    @param inPacketDescriptions 	an array of packet descriptions describing the packets being 
									provided. Not all formats require packet descriptions to be 
									provided. NULL may be passed if no descriptions are required.   
    @param inStartingPacket 		the packet index of where the first packet provided should be placed.
    @param ioNumPackets 			on input, the number of packets to write, on output, the number of
									packets actually written.
    @param inBuffer 				a void * to user allocated memory containing the packets to write.
    @result							returns noErr if successful.
*/
extern OSStatus	
AudioFileWritePackets (	AudioFileID							inAudioFile,  
                        Boolean								inUseCache,
                        UInt32								inNumBytes,
                        const AudioStreamPacketDescription * __nullable inPacketDescriptions,
                        SInt64								inStartingPacket, 
                        UInt32								*ioNumPackets, 
                        const void							*inBuffer)			API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));


/*!
    @function   AudioFileWritePacketsWithDependencies
    @abstract   Write packets of audio data with corresponding packet dependencies to an audio data file.
    @discussion For all uncompressed formats, `packets == frames`.
    @param inAudioFile          The audio file to write to.
    @param inUseCache           Set to `true` if you want to cache the data. Otherwise, set to `false`.
    @param inNumBytes           The number of bytes of audio data being written.
    @param inPacketDescriptions A pointer to an array of packet descriptions for the audio data.
                                Not all formats require packet descriptions. If no packet descriptions
                                are required, for instance, if you are writing CBR data,  pass `NULL`.
    @param inPacketDependencies A pointer to an array of packet dependencies for the audio data.
                                This must not be `NULL`.  To write packets without dependencies,
                                use ``AudioFileWritePackets`` instead.
    @param inStartingPacket     The packet index for the placement of the first provided packet.
    @param ioNumPackets         On input, a pointer to the number of packets to write.
                                On output, a pointer to the number of packets actually written.
    @param inBuffer             A pointer to user-allocated memory containing the new audio data
                                to write to the audio data file.
    @result                     A result code. See Result Codes.
*/
extern OSStatus
AudioFileWritePacketsWithDependencies (
    AudioFileID                                         inAudioFile,
    Boolean                                             inUseCache,
    UInt32                                              inNumBytes,
    const AudioStreamPacketDescription * __nullable     inPacketDescriptions,
    const AudioStreamPacketDependencyDescription *      inPacketDependencies,
    SInt64                                              inStartingPacket,
    UInt32 *                                            ioNumPackets,
    const void *                                        inBuffer)
API_AVAILABLE(macos(26.0), ios(26.0), watchos(26.0), tvos(26.0), visionos(26.0));


/*!
    @function	AudioFileCountUserData
    @abstract   Get the number of user data items with a certain ID in the file
    @discussion		"User Data" refers to chunks in AIFF, CAF and WAVE files, or resources 
					in Sound Designer II files, and possibly other things in other files.
					For simplicity, referred to below as "chunks".
    @param      inAudioFile			an AudioFileID.
    @param      inUserDataID		the four char code of the chunk.
    @param      outNumberItems		on output, if successful, number of chunks of this type in the file.
    @result							returns noErr if successful.
*/
extern OSStatus	
AudioFileCountUserData (	AudioFileID			inAudioFile, 
							UInt32				inUserDataID, 
							UInt32				*outNumberItems)			API_AVAILABLE(macos(10.4), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function	AudioFileGetUserDataSize
    @abstract   Get the size of user data in a file
    @param      inAudioFile			an AudioFileID.
    @param      inUserDataID		the four char code of the chunk.
    @param      inIndex				an index specifying which chunk if there are more than one.
    @param      outUserDataSize		on output, if successful, the size of the user data chunk.
    @result							returns noErr if successful.
*/
extern OSStatus	
AudioFileGetUserDataSize (	AudioFileID			inAudioFile, 
							UInt32				inUserDataID, 
							UInt32				inIndex, 
							UInt32				*outUserDataSize)			API_AVAILABLE(macos(10.4), ios(2.0), watchos(2.0), tvos(9.0));

/*!
	@function	AudioFileGetUserDataSize64
	@abstract   Get the 64-bit size of user data in a file
	@param      inAudioFile			an AudioFileID.
	@param      inUserDataID		the four char code of the chunk.
	@param      inIndex				an index specifying which chunk if there are more than one.
	@param      outUserDataSize		on output, if successful, the size of the user data chunk.
	@result							returns noErr if successful.
*/
extern OSStatus
AudioFileGetUserDataSize64 (	AudioFileID			inAudioFile,
								UInt32				inUserDataID,
								UInt32				inIndex,
								UInt64				*outUserDataSize)		API_AVAILABLE(macos(14.0), ios(17.0), watchos(10.0), tvos(17.0));

/*!
    @function	AudioFileGetUserData
    @abstract   Get the data of a chunk in a file.
    @param      inAudioFile			an AudioFileID.
    @param      inUserDataID		the four char code of the chunk.
    @param      inIndex				an index specifying which chunk if there are more than one.
	@param		ioUserDataSize		the size of the buffer on input, size of bytes copied to buffer on output 
    @param      outUserData			a pointer to a buffer in which to copy the chunk data.
    @result							returns noErr if successful.
*/
extern OSStatus	
AudioFileGetUserData (	AudioFileID			inAudioFile, 
						UInt32				inUserDataID, 
						UInt32				inIndex, 
						UInt32				*ioUserDataSize, 
						void				*outUserData)					API_AVAILABLE(macos(10.4), ios(2.0), watchos(2.0), tvos(9.0));

/*!
    @function	AudioFileGetUserDataAtOffset
    @abstract   Get a part of the data of a chunk in a file.
    @param      inAudioFile			an AudioFileID.
    @param      inUserDataID		the four char code of the chunk.
    @param      inIndex				an index specifying which chunk if there are more than one.
    @param      inOffset			offset from the first byte of the chunk to the first byte to get.
    @param      ioUserDataSize		the size of the buffer on input, size of bytes copied to buffer on output
    @param      outUserData			a pointer to a buffer in which to copy the chunk data.
    @result							returns noErr if successful.
*/
extern OSStatus
AudioFileGetUserDataAtOffset (	AudioFileID			inAudioFile,
								UInt32				inUserDataID,
								UInt32				inIndex,
								SInt64				inOffset,
								UInt32				*ioUserDataSize,
								void				*outUserData)			API_AVAILABLE(macos(14.0), ios(17.0), watchos(10.0), tvos(17.0));

/*!
    @function	AudioFileSetUserData
    @abstract   Set the data of a chunk in a file.
    @param      inAudioFile			an AudioFileID.
    @param      inUserDataID		the four char code of the chunk.
    @param      inIndex				an index specifying which chunk if there are more than one.
	@param		inUserDataSize		on input the size of the data to copy, on output, size of bytes copied from the buffer  
    @param      inUserData			a pointer to a buffer from which to copy the chunk data 
									(only the contents of the chunk, not including the chunk header).
    @result							returns noErr if successful.
*/
extern OSStatus	
AudioFileSetUserData ( AudioFileID			inAudioFile, 
						UInt32				inUserDataID, 
						UInt32				inIndex, 
						UInt32				inUserDataSize, 
						const void			*inUserData)					API_AVAILABLE(macos(10.4), ios(2.0), watchos(2.0), tvos(9.0));


/*!
    @function	AudioFileRemoveUserData
    @abstract   Remove a user chunk in a file.
    @param      inAudioFile			an AudioFileID.
    @param      inUserDataID		the four char code of the chunk.
    @param      inIndex				an index specifying which chunk if there are more than one.
    @result							returns noErr if successful.
*/

extern OSStatus	
AudioFileRemoveUserData ( AudioFileID		inAudioFile, 
						UInt32				inUserDataID, 
						UInt32				inIndex)						API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));


//=============================================================================
//	Audio File Properties
//=============================================================================

/*!
    @enum		Audio File Properties
    @abstract   constants for AudioFile get/set property calls
    @constant   kAudioFilePropertyFileFormat 
					An AudioFileTypeID that identifies the format of the file
    @constant   kAudioFilePropertyDataFormat 
					An AudioStreamBasicDescription describing the format of the audio data
    @constant   kAudioFilePropertyFormatList 
					In order to support formats such as AAC SBR where an encoded data stream can be decoded to 
					multiple destination formats, this property returns an array of AudioFormatListItems (see AudioFormat.h) of those formats.
					The default behavior is to return the an AudioFormatListItem that has the same AudioStreamBasicDescription 
					that kAudioFilePropertyDataFormat returns.
    @constant   kAudioFilePropertyIsOptimized 
					A UInt32 indicating whether an Audio File has been optimized.
					Optimized means it is ready to start having sound data written to it. 
					A value of 0 indicates the file needs to be optimized.
					A value of 1 indicates the file is currently optimized.
    @constant   kAudioFilePropertyMagicCookieData 
					A void * pointing to memory set up by the caller.
					Some file types require that a magic cookie be provided before packets can be written
					to the file, so this property should be set before calling 
					AudioFileWriteBytes()/AudioFileWritePackets() if a magic cookie exists.
    @constant   kAudioFilePropertyAudioDataByteCount 
					a UInt64 that indicates the number of bytes of audio data contained in the file
    @constant   kAudioFilePropertyAudioDataPacketCount 
					a UInt64 that indicates the number of packets of audio data contained in the file
    @constant   kAudioFilePropertyMaximumPacketSize 
					a UInt32 that indicates the maximum size of a packet for the data contained in the file
    @constant   kAudioFilePropertyDataOffset 
					a SInt64 that indicates the byte offset in the file of the audio data.
    @constant   kAudioFilePropertyChannelLayout 
					An AudioChannelLayout struct.
    @constant   kAudioFilePropertyDeferSizeUpdates 
					A UInt32. If 1, then updating the files sizes in the header is not done for every write, 
					but deferred until the file is read, optimized or closed. This is more efficient, but less safe
					since, if the application crashes before the size is updated, the file may not be readable.
					The default value is one, it doesn't update the header.
    @constant   kAudioFilePropertyDataFormatName 
					This is deprecated. Use kAudioFormatProperty_FormatName in AudioFormat.h instead.
    @constant   kAudioFilePropertyMarkerList 
					access the list of markers defined in the file. returns an AudioFileMarkerList.
					The CFStringRefs in the returned structs must be released by the client.
					(available in 10.2.4 and later)
    @constant   kAudioFilePropertyRegionList 
					access the list of regions defined in the file. returns an Array of AudioFileRegions.
					The CFStringRefs in the returned structs must be released by the client.
					(available in 10.2.4 and later)
    @constant   kAudioFilePropertyPacketToFrame 
					pass a AudioFramePacketTranslation with mPacket filled out and get mFrame back. mFrameOffsetInPacket is ignored.
    @constant   kAudioFilePropertyFrameToPacket 
					pass a AudioFramePacketTranslation with mFrame filled out and get mPacket and mFrameOffsetInPacket back.
					
    @constant   kAudioFilePropertyRestrictsRandomAccess
					A UInt32 indicating whether an Audio File contains packets that cannot be used as random access points.
					A value of 0 indicates that any packet can be used as a random access point, i.e. that a decoder can start decoding with any packet.
					A value of 1 indicates that some packets cannot be used as random access points, i.e. that kAudioFilePropertyPacketToRollDistance must be employed in order to identify an appropriate initial packet for decoding.
    @constant   kAudioFilePropertyPacketToRollDistance
					Pass an AudioPacketRollDistanceTranslation with mPacket filled out and get mRollDistance back.
					The roll distance indicates the count of packets that must be decoded prior to the packet with the specified number in order to achieve the best practice for the decoding of that packet.
					For file types for which a minimal roll distance is prohibitively expensive to determine per packet, the value returned may be derived from an upper bound for all packet roll distances.
					If the value of kAudioFilePropertyRestrictsRandomAccess is 1, either kAudioFilePropertyPacketToRollDistance
					or kAudioFilePropertyPacketToDependencyInfo must be used in order to identify an appropriate random access point.
					If the value of kAudioFilePropertyRestrictsRandomAccess is 0, kAudioFilePropertyPacketToRollDistance can be used in
					order to identify the best available random access point, which may be prior to the specified packet even if the specified
					packet can be used as a random access point.
    @constant   kAudioFilePropertyPreviousIndependentPacket
    @constant   kAudioFilePropertyNextIndependentPacket
					Pass an AudioIndependentPacketTranslation with mPacket filled out and get mIndependentlyDecodablePacket back.
					A value of -1 means that no independent packet is present in the stream in the direction of interest. Otherwise,
					for kAudioFilePropertyPreviousIndependentPacket, mIndependentlyDecodablePacket will be less than mPacket, and
					for kAudioFilePropertyNextIndependentPacket, mIndependentlyDecodablePacket will be greater than mPacket.
    @constant   kAudioFilePropertyPacketToDependencyInfo
					Pass an AudioPacketDependencyInfoTranslation with mPacket filled out and get mIsIndependentlyDecodable
					and mPrerollPacketCount back.
					A value of 0 for mIsIndependentlyDecodable indicates that the specified packet is not independently decodable.
					A value of 1 for mIsIndependentlyDecodable indicates that the specified packet is independently decodable.
					For independently decodable packets, mPrerollPacketCount indicates the count of packets that must be decoded
					after the packet with the specified number in order to refresh the decoder.
					If the value of kAudioFilePropertyRestrictsRandomAccess is 1, either kAudioFilePropertyPacketToRollDistance or
					kAudioFilePropertyPacketToDependencyInfo must be used in order to identify an appropriate random access point.

	@constant	kAudioFilePropertyPacketToByte
					pass an AudioBytePacketTranslation struct with mPacket filled out and get mByte back.
					mByteOffsetInPacket is ignored. If the mByte value is an estimate then 
					kBytePacketTranslationFlag_IsEstimate will be set in the mFlags field.
	@constant	kAudioFilePropertyByteToPacket
					pass an AudioBytePacketTranslation struct with mByte filled out and get mPacket and
					mByteOffsetInPacket back. If the mPacket value is an estimate then 
					kBytePacketTranslationFlag_IsEstimate will be set in the mFlags field.
					
    @constant   kAudioFilePropertyChunkIDs 
					returns an array of OSType four char codes for each kind of chunk in the file.
    @constant   kAudioFilePropertyInfoDictionary 
					returns a CFDictionary filled with information about the data contained in the file. 
					See dictionary key constants already defined for info string types. 
					AudioFileComponents are free to add keys to the dictionaries that they return for this property...
					caller is responsible for releasing the CFObject
    @constant   kAudioFilePropertyPacketTableInfo 
					Gets or sets an AudioFilePacketTableInfo struct for the file types that support it.
					When setting, the sum of mNumberValidFrames, mPrimingFrames and mRemainderFrames must be the same as the total
					number of frames in all packets. If not you will get a kAudio_ParamError. The best way to ensure this is to get the value of
					the property and make sure the sum of the three values you set has the same sum as the three values you got.
	@constant	kAudioFilePropertyPacketSizeUpperBound
					a UInt32 for the theoretical maximum packet size in the file (without actually scanning
					the whole file to find the largest packet, as may happen with kAudioFilePropertyMaximumPacketSize).
    @constant   kAudioFilePropertyPacketRangeByteCountUpperBound
					Pass an AudioPacketRangeByteCountTranslation with mPacket and mPacketCount filled out
					and get mByteCountUpperBound back. The value of mByteCountUpperBound can be used to allocate a buffer
					for use with AudioFileReadPacketData in order to accommodate the entire packet range.
					May require scanning in order to obtain the requested information, but even if so, no scanning will occur
					beyond the last packet in the specified range.
					For file formats in which packets are directly accessible and stored both contiguously and byte-aligned,
					the returned upper bound will be equal to the total size of the packets in the range. Otherwise the
					upper bound may reflect per-packet storage overhead.
	@constant	kAudioFilePropertyReserveDuration
					The value is a Float64 of the duration in seconds of data that is expected to be written.
					Setting this property before any data has been written reserves space in the file header for a packet table 
					and/or other information so that it can appear before the audio data. Otherwise the packet table may get written at the 
					end of the file, preventing the file from being streamable.
	@constant	kAudioFilePropertyEstimatedDuration
					The value is a Float64 representing an estimated duration in seconds. If duration can be calculated without scanning the entire file,
					or all the audio data packets have been scanned, the value will accurately reflect the duration of the audio data. 
	@constant	kAudioFilePropertyBitRate
					Returns the bit rate for the audio data as a UInt32. For some formats this will be approximate.
	@constant	kAudioFilePropertyID3Tag
					A void * pointing to memory set up by the caller to contain a fully formatted ID3 tag (get/set v2.2, v2.3, or v2.4, v1 get only).
					The ID3 tag is not manipulated in any way either for read or write.
					When setting, this property must be called before calling AudioFileWritePackets.
	@constant	kAudioFilePropertyID3TagOffset
					Returns the offset of a leading ID3v2 tag, if present, and otherwise the offset of a trailing ID3v1 tag, if present, as an SInt64. (get property only)
					Note that for some file formats the offset of a leading ID3v2 tag when present may not be 0.
	@constant	kAudioFilePropertySourceBitDepth
					For encoded data this property returns the bit depth of the source as an SInt32, if known.
					The bit depth is expressed as a negative number if the source was floating point, e.g. -32 for float, -64 for double.
	@constant	kAudioFilePropertyAlbumArtwork
					returns a CFDataRef filled with the Album Art or NULL. 
					The caller is responsible for releasing a non-NULL CFDataRef.
					In order to parse the contents of the data, CGImageSourceCreateWithData may be used.
    @constant	kAudioFilePropertyAudioTrackCount
                    a UInt32 that indicates the number of audio tracks contained in the file. (get property only)
    @constant	kAudioFilePropertyUseAudioTrack
                    a UInt32 that indicates the number of audio tracks contained in the file. (set property only)
 */
CF_ENUM(AudioFilePropertyID)
{
	kAudioFilePropertyFileFormat			=	'ffmt',
	kAudioFilePropertyDataFormat			=	'dfmt',
	kAudioFilePropertyIsOptimized			=	'optm',
	kAudioFilePropertyMagicCookieData		=	'mgic',
	kAudioFilePropertyAudioDataByteCount	=	'bcnt',
	kAudioFilePropertyAudioDataPacketCount	=	'pcnt',
	kAudioFilePropertyMaximumPacketSize		=	'psze',
	kAudioFilePropertyDataOffset			=	'doff',
	kAudioFilePropertyChannelLayout			=	'cmap',
	kAudioFilePropertyDeferSizeUpdates		=	'dszu',
	kAudioFilePropertyDataFormatName		=	'fnme',
	kAudioFilePropertyMarkerList			=	'mkls',
	kAudioFilePropertyRegionList			=	'rgls',
	kAudioFilePropertyPacketToFrame			=	'pkfr',
	kAudioFilePropertyFrameToPacket			=	'frpk',
	kAudioFilePropertyRestrictsRandomAccess	=	'rrap',
	kAudioFilePropertyPacketToRollDistance	=	'pkrl',
	kAudioFilePropertyPreviousIndependentPacket	= 'pind',
	kAudioFilePropertyNextIndependentPacket	=	'nind',
	kAudioFilePropertyPacketToDependencyInfo =	'pkdp',
	kAudioFilePropertyPacketToByte			=	'pkby',
	kAudioFilePropertyByteToPacket			=	'bypk',
	kAudioFilePropertyChunkIDs				=	'chid',
	kAudioFilePropertyInfoDictionary        =	'info',
	kAudioFilePropertyPacketTableInfo		=	'pnfo',
	kAudioFilePropertyFormatList			=	'flst',
	kAudioFilePropertyPacketSizeUpperBound  =	'pkub',
	kAudioFilePropertyPacketRangeByteCountUpperBound = 'prub',
	kAudioFilePropertyReserveDuration		=	'rsrv',
	kAudioFilePropertyEstimatedDuration		=	'edur',
	kAudioFilePropertyBitRate				=	'brat',
	kAudioFilePropertyID3Tag				=	'id3t',
	kAudioFilePropertyID3TagOffset			=	'id3o',
	kAudioFilePropertySourceBitDepth		=	'sbtd',
	kAudioFilePropertyAlbumArtwork			=	'aart',
    kAudioFilePropertyAudioTrackCount       =   'atct',
	kAudioFilePropertyUseAudioTrack			=	'uatk'
};


/*!
    @function	AudioFileGetPropertyInfo
    @abstract   Get information about the size of a property of an AudioFile  and whether it can be set.
    @param      inAudioFile			an AudioFileID.
    @param      inPropertyID		an AudioFileProperty constant.
    @param      outDataSize			the size in bytes of the current value of the property. In order to get the property value, 
									you will need a buffer of this size.
    @param      isWritable			will be set to 1 if writable, or 0 if read only.
    @result							returns noErr if successful.
*/
extern OSStatus
AudioFileGetPropertyInfo(		AudioFileID				inAudioFile,
                                AudioFilePropertyID		inPropertyID,
                                UInt32 * __nullable		outDataSize,
                                UInt32 * __nullable		isWritable)			API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));
                                
/*!
    @function	AudioFileGetProperty
    @abstract   Copies the value for a property of an AudioFile into a buffer.
    @param      inAudioFile			an AudioFileID.
    @param      inPropertyID		an AudioFileProperty constant.
    @param      ioDataSize			on input the size of the outPropertyData buffer. On output the number of bytes written to the buffer.
    @param      outPropertyData		the buffer in which to write the property data.
    @result							returns noErr if successful.
*/
extern OSStatus
AudioFileGetProperty(	AudioFileID				inAudioFile,
                        AudioFilePropertyID		inPropertyID,
                        UInt32					*ioDataSize,
                        void					*outPropertyData)			API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));
                        
/*!
    @function	AudioFileSetProperty
    @abstract   Sets the value for a property of an AudioFile .
    @param      inAudioFile			an AudioFileID.
    @param      inPropertyID		an AudioFileProperty constant.
    @param      inDataSize			the size of the property data.
    @param      inPropertyData		the buffer containing the property data.
    @result							returns noErr if successful.
*/
extern OSStatus
AudioFileSetProperty(	AudioFileID				inAudioFile,
                        AudioFilePropertyID		inPropertyID,
                        UInt32					inDataSize,
                        const void				*inPropertyData)			API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));



//=============================================================================
//	Audio File Global Info Properties
//=============================================================================

/*!
    @enum		Audio File Global Info Properties
    @abstract   constants for AudioFileGetGlobalInfo properties
    @constant   kAudioFileGlobalInfo_ReadableTypes
					No specifier needed. Must be set to NULL.
					Returns an array of UInt32 containing the file types 
					(i.e. AIFF, WAVE, etc) that can be opened for reading.
    @constant   kAudioFileGlobalInfo_WritableTypes
					No specifier needed. Must be set to NULL.
					Returns an array of UInt32 containing the file types 
					(i.e. AIFF, WAVE, etc) that can be opened for writing.
    @constant   kAudioFileGlobalInfo_FileTypeName
					Specifier is a pointer to a AudioFileTypeID containing a file type.
					Returns a CFString containing the name for the file type. 
    @constant   kAudioFileGlobalInfo_AvailableFormatIDs
					Specifier is a pointer to a AudioFileTypeID containing a file type.
					Returns a array of format IDs for formats that can be read. 
    @constant   kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat
					Specifier is a pointer to a AudioFileTypeAndFormatID struct defined below.
					Returns an array of AudioStreamBasicDescriptions which have all of the 
					formats for a particular file type and format ID. The AudioStreamBasicDescriptions
					have the following fields filled in: mFormatID, mFormatFlags, mBitsPerChannel
					writing new files.
					
					
    @constant   kAudioFileGlobalInfo_AllExtensions
					No specifier needed. Must be set to NULL.
					Returns a CFArray of CFStrings containing all file extensions 
					that are recognized. The array be used when creating an NSOpenPanel.

    @constant   kAudioFileGlobalInfo_AllHFSTypeCodes
					No specifier needed. Must be set to NULL.
					Returns an array of HFSTypeCode's containing all HFSTypeCodes
					that are recognized.

    @constant   kAudioFileGlobalInfo_AllUTIs
					No specifier needed. Must be set to NULL.
					Returns a CFArray of CFString of all Universal Type Identifiers
					that are recognized by AudioFile. 
					The caller is responsible for releasing the CFArray.

    @constant   kAudioFileGlobalInfo_AllMIMETypes
					No specifier needed. Must be set to NULL.
					Returns a CFArray of CFString of all MIME types
					that are recognized by AudioFile. 
					The caller is responsible for releasing the CFArray.


    @constant   kAudioFileGlobalInfo_ExtensionsForType
					Specifier is a pointer to a AudioFileTypeID containing a file type.
					Returns a CFArray of CFStrings containing the file extensions 
					that are recognized for this file type. 

    @constant   kAudioFileGlobalInfo_HFSTypeCodesForType
					Specifier is a pointer to an AudioFileTypeID.
					Returns an array of HFSTypeCodes corresponding to that file type.
					The first type in the array is the preferred one for use when

    @constant   kAudioFileGlobalInfo_UTIsForType
					Specifier is a pointer to an AudioFileTypeID.
					Returns a CFArray of CFString of all Universal Type Identifiers
					that are recognized by the file type. 
					The caller is responsible for releasing the CFArray.

    @constant   kAudioFileGlobalInfo_MIMETypesForType
					Specifier is a pointer to an AudioFileTypeID.
					Returns a CFArray of CFString of all MIME types
					that are recognized by the file type. 
					The caller is responsible for releasing the CFArray.

	these are inverses of the above:

    @constant   kAudioFileGlobalInfo_TypesForExtension
					Specifier is a CFStringRef containing a file extension.
					Returns an array of all AudioFileTypeIDs that support the extension. 
	
    @constant   kAudioFileGlobalInfo_TypesForHFSTypeCode
					Specifier is an HFSTypeCode.
					Returns an array of all AudioFileTypeIDs that support the HFSTypeCode. 

    @constant   kAudioFileGlobalInfo_TypesForUTI
					Specifier is a CFStringRef containing a Universal Type Identifier.
					Returns an array of all AudioFileTypeIDs that support the UTI. 

    @constant   kAudioFileGlobalInfo_TypesForMIMEType
					Specifier is a CFStringRef containing a MIME Type.
					Returns an array of all AudioFileTypeIDs that support the MIME type. 

*/
CF_ENUM(AudioFilePropertyID)
{
	kAudioFileGlobalInfo_ReadableTypes							= 'afrf',
	kAudioFileGlobalInfo_WritableTypes							= 'afwf',
	kAudioFileGlobalInfo_FileTypeName							= 'ftnm',
	kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat	= 'sdid',
	kAudioFileGlobalInfo_AvailableFormatIDs						= 'fmid',

	kAudioFileGlobalInfo_AllExtensions							= 'alxt',
	kAudioFileGlobalInfo_AllHFSTypeCodes						= 'ahfs',
	kAudioFileGlobalInfo_AllUTIs								= 'auti',
	kAudioFileGlobalInfo_AllMIMETypes							= 'amim',

	kAudioFileGlobalInfo_ExtensionsForType						= 'fext',
	kAudioFileGlobalInfo_HFSTypeCodesForType					= 'fhfs',
	kAudioFileGlobalInfo_UTIsForType							= 'futi',
	kAudioFileGlobalInfo_MIMETypesForType						= 'fmim',
	
	kAudioFileGlobalInfo_TypesForMIMEType						= 'tmim',
	kAudioFileGlobalInfo_TypesForUTI							= 'tuti',
	kAudioFileGlobalInfo_TypesForHFSTypeCode					= 'thfs',
	kAudioFileGlobalInfo_TypesForExtension						= 'text'
};


/*!
    @struct		AudioFileTypeAndFormatID
    @abstract   This is used as a specifier for kAudioFileGlobalInfo_AvailableStreamDescriptions
    @discussion This struct is used to specify a desired audio file type and data format ID  so
				that a list of stream descriptions of available formats can be obtained.
    @var        mFileType
					a four char code for the file type such as kAudioFileAIFFType, kAudioFileCAFType, etc.
    @var        mFormatID
					a four char code for the format ID such as kAudioFormatLinearPCM, kAudioFormatMPEG4AAC, etc.
*/
struct AudioFileTypeAndFormatID
{
	AudioFileTypeID  mFileType;
	UInt32			mFormatID;
};
typedef struct AudioFileTypeAndFormatID AudioFileTypeAndFormatID;


/*!
    @function	AudioFileGetGlobalInfoSize
    @abstract   Get the size of a global property.
    @param      inPropertyID		an AudioFileGlobalInfo property constant.
    @param      inSpecifierSize		The size of the specifier data.
    @param      inSpecifier			A specifier is a buffer of data used as an input argument to some of the global info properties.
    @param      outDataSize			the size in bytes of the current value of the property. In order to get the property value, 
									you will need a buffer of this size.
    @result							returns noErr if successful.
*/
extern OSStatus
AudioFileGetGlobalInfoSize(		AudioFilePropertyID		inPropertyID,
                                UInt32					inSpecifierSize,
                                void * __nullable		inSpecifier,
                                UInt32					*outDataSize)		API_AVAILABLE(macos(10.3), ios(2.0), watchos(2.0), tvos(9.0));
                                
/*!
    @function	AudioFileGetGlobalInfo
    @abstract   Copies the value for a global property into a buffer.
    @param      inPropertyID		an AudioFileGlobalInfo property constant.
    @param      inSpecifierSize		The size of the specifier data.
    @param      inSpecifier			A specifier is a buffer of data used as an input argument to some of the global info properties.
    @param      ioDataSize			on input the size of the outPropertyData buffer. On output the number of bytes written to the buffer.
    @param      outPropertyData		the buffer in which to write the property data.
    @result							returns noErr if successful.
*/
extern OSStatus
AudioFileGetGlobalInfo(			AudioFilePropertyID		inPropertyID,
								UInt32					inSpecifierSize,
                                void * __nullable		inSpecifier,
                    		    UInt32					*ioDataSize,
                    		    void					*outPropertyData)	API_AVAILABLE(macos(10.3), ios(2.0), watchos(2.0), tvos(9.0));

#pragma mark - Deprecated
	
#if !TARGET_OS_IPHONE
struct FSRef;
/*!
    @function	AudioFileCreate
    @abstract   creates a new audio file
    @discussion	creates a new audio file located in the parent directory 
                      provided. Upon success, an AudioFileID is returned which can
                      be used for subsequent calls to the AudioFile APIs.
    @param inParentRef		an FSRef to the directory where  the new file should be created.
    @param inFileName		a CFStringRef containing the name of the file to be created.
    @param inFileType		an AudioFileTypeID indicating the type of audio file to create.
    @param inFormat			an AudioStreamBasicDescription describing the data format that will be
							added to the audio file.
    @param inFlags			relevant flags for creating/opening the file. 
    @param outNewFileRef	if successful, the FSRef of the newly created file.
    @param outAudioFile		if successful, an AudioFileID that can be used for subsequent AudioFile calls.
    @result					returns noErr if successful.
	@deprecated				in macOS 10.6, see AudioFileCreateWithURL
*/
extern OSStatus	
AudioFileCreate (	const struct FSRef					*inParentRef, 
                    CFStringRef							inFileName,
                    AudioFileTypeID						inFileType,
                    const AudioStreamBasicDescription	*inFormat,
                    AudioFileFlags						inFlags,
                    struct FSRef						*outNewFileRef,
                    AudioFileID	__nullable * __nonnull	outAudioFile)		API_DEPRECATED("no longer supported", macos(10.2, 10.6)) API_UNAVAILABLE(ios, watchos, tvos);

/*!
    @function				AudioFileInitialize
    @abstract				Write over an existing audio file.
    @discussion				Use AudioFileInitialize to wipe clean an existing audio file
							and prepare it to be populated with new data.
    @param inFileRef		the FSRef of an existing audio file.
    @param inFileType		an AudioFileTypeID indicating the type of audio file to initialize the file to. 
    @param inFormat			an AudioStreamBasicDescription describing the data format that will be
							added to the audio file.
    @param inFlags			flags for creating/opening the file. Currently zero.
    @param outAudioFile		upon success, an AudioFileID that can be used for subsequent
							AudioFile calls.
    @result					returns noErr if successful.
	@deprecated				in macOS 10.6, see AudioFileCreateWithURL
*/
extern OSStatus	
AudioFileInitialize (	const struct FSRef					*inFileRef,
                        AudioFileTypeID						inFileType,
                        const AudioStreamBasicDescription	*inFormat,
                        AudioFileFlags						inFlags,
                        AudioFileID	__nullable * __nonnull	outAudioFile)	API_DEPRECATED("no longer supported", macos(10.2, 10.6)) API_UNAVAILABLE(ios, watchos, tvos);

/*!
    @function				AudioFileOpen
    @abstract				Open an existing audio file.
    @discussion				Open an existing audio file for reading or reading and writing.
    @param inFileRef		the FSRef of an existing audio file.
    @param inPermissions	use the permission constants
    @param inFileTypeHint	For files which have no filename extension and whose type cannot be easily or
							uniquely determined from the data (ADTS,AC3), this hint can be used to indicate the file type. 
							Otherwise you can pass zero for this. The hint is only used on OS versions 10.3.1 or greater.
							For OS versions prior to that, opening files of the above description will fail.
    @param outAudioFile		upon success, an AudioFileID that can be used for subsequent
							AudioFile calls.
    @result					returns noErr if successful.
	@deprecated				in macOS 10.6, see AudioFileOpenURL
*/
extern OSStatus	
AudioFileOpen (	const struct FSRef		*inFileRef,
                AudioFilePermissions	inPermissions,
                AudioFileTypeID			inFileTypeHint,
                AudioFileID	__nullable * __nonnull	outAudioFile)			API_DEPRECATED("no longer supported", macos(10.2, 10.6)) API_UNAVAILABLE(ios, watchos, tvos);

#endif
	
#if defined(__cplusplus)
}
#endif

CF_ASSUME_NONNULL_END

#endif // AudioToolbox_AudioFile_h
#else
#include <AudioToolboxCore/AudioFile.h>
#endif
