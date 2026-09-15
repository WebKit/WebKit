#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/CAFFile.h>)
/*!
	@file		CAFFile.h
	@framework	AudioToolbox.framework
	@copyright	(c) 2004-2015 by Apple, Inc., all rights reserved.
	@abstract	The data structures contained within a CAF (Core Audio Format) file.
*/

#ifndef AudioToolbox_CAFFile_h
#define AudioToolbox_CAFFile_h

#include <CoreAudioTypes/CoreAudioTypes.h>

#if TARGET_OS_WIN32
#define ATTRIBUTE_PACKED
#pragma pack(push, 1)
#else
#define ATTRIBUTE_PACKED __attribute__((__packed__))
#endif

// In a CAF File all of these types' byte order is big endian.
// When reading or writing these values the program will need to flip byte order to native endian

// CAF File Header
CF_ENUM(UInt32) {
	kCAF_FileType				= 'caff',
	kCAF_FileVersion_Initial	= 1
};

// CAF Chunk Types	
CF_ENUM(UInt32) {
	kCAF_StreamDescriptionChunkID   = 'desc',
	kCAF_AudioDataChunkID			= 'data',
	kCAF_ChannelLayoutChunkID		= 'chan',
	kCAF_FillerChunkID				= 'free',
	kCAF_MarkerChunkID				= 'mark',
	kCAF_RegionChunkID				= 'regn',
	kCAF_InstrumentChunkID			= 'inst',
	kCAF_MagicCookieID				= 'kuki',
	kCAF_InfoStringsChunkID			= 'info',
	kCAF_EditCommentsChunkID		= 'edct',
	kCAF_PacketTableChunkID			= 'pakt',
	kCAF_StringsChunkID				= 'strg',
	kCAF_UUIDChunkID				= 'uuid',
	kCAF_PeakChunkID				= 'peak',
	kCAF_OverviewChunkID			= 'ovvw',
	kCAF_MIDIChunkID				= 'midi',
	kCAF_UMIDChunkID				= 'umid',
	kCAF_FormatListID				= 'ldsc',
	kCAF_iXMLChunkID				= 'iXML'
};


struct CAFFileHeader
{
        UInt32          mFileType;			// 'caff'
		UInt16			mFileVersion;		//initial revision set to 1
		UInt16			mFileFlags;			//initial revision set to 0
} ATTRIBUTE_PACKED;
typedef struct CAFFileHeader CAFFileHeader;


struct CAFChunkHeader
{
        UInt32          mChunkType; // four char code
        SInt64          mChunkSize;  // size in bytes of the chunk data (not including this header).
									// mChunkSize is SInt64 not UInt64 because negative values for 
									// the data size can have a special meaning
} ATTRIBUTE_PACKED;

typedef struct CAFChunkHeader CAFChunkHeader;

struct CAF_UUID_ChunkHeader
{
        CAFChunkHeader   mHeader;
        UInt8            mUUID[16];
} ATTRIBUTE_PACKED;
typedef struct CAF_UUID_ChunkHeader CAF_UUID_ChunkHeader;


// these are the flags if the format ID is 'lpcm'
// <CoreAudioTypes/CoreAudioTypes.h> declares some of the format constants 
// that can be used as Data Formats in a CAF file
typedef CF_OPTIONS(UInt32, CAFFormatFlags)
{
    kCAFLinearPCMFormatFlagIsFloat				= (1L << 0),
    kCAFLinearPCMFormatFlagIsLittleEndian		= (1L << 1)
};

// Every file MUST have this chunk. It MUST be the first chunk in the file
struct CAFAudioDescription
{
        Float64 mSampleRate;
        UInt32  mFormatID;
        CAFFormatFlags  mFormatFlags;
        UInt32  mBytesPerPacket;
        UInt32  mFramesPerPacket;
        UInt32  mChannelsPerFrame;
        UInt32  mBitsPerChannel;
} ATTRIBUTE_PACKED;
typedef struct CAFAudioDescription  CAFAudioDescription;


// 'ldsc'  format list chunk.
// for data formats like AAC SBR which can be decompressed to multiple formats, this chunk contains a list of
// CAFAudioFormatListItem describing those formats. The list is ordered from best to worst by number of channels 
// and sample rate in that order. mChannelLayoutTag is an AudioChannelLayoutTag as defined in CoreAudioTypes.h

struct CAFAudioFormatListItem
{
	CAFAudioDescription mFormat;
	UInt32 				mChannelLayoutTag;
} ATTRIBUTE_PACKED;

// 'chan'  Optional chunk.
// struct AudioChannelLayout  as defined in CoreAudioTypes.h.

// 'free'
// this is a padding chunk for reserving space in the file. content is meaningless.

// 'kuki'
// this is the magic cookie chunk. bag of bytes.

// 'data'    Every file MUST have this chunk.
// actual audio data can be any format as described by the 'asbd' chunk.

// if mChunkSize is < 0 then this is the last chunk in the file and the actual length 
// should be determined from the file size. 
// The motivation for this is to allow writing the files without seeking to update size fields after every 
// write in order to keep the file legal.
// The program can put a -1 in the mChunkSize field and 
// update it only once at the end of recording. 
// If the program were to crash during recording then the file is still well defined.

// 'pakt' Required if either/or mBytesPerPacket or mFramesPerPacket in the Format Description are zero
// For formats that are packetized and have variable sized packets. 
// The table is stored as an array of one or two variable length integers. 
// (a) size in bytes of the data of a given packet.
// (b) number of frames in a given packet.
// These sizes are encoded as variable length integers

// The packet description entries are either one or two values depending on the format.
// There are three possibilities
// (1)
// If the format has variable bytes per packets (desc.mBytesPerPacket == 0) and constant frames per packet
// (desc.mFramesPerPacket != 0) then the packet table contains single entries representing the bytes in a given packet
// (2)
// If the format is a constant bit rate (desc.mBytesPerPacket != 0) but variable frames per packet
// (desc.mFramesPerPacket == 0) then the packet table entries contains single entries 
// representing the number of frames in a given packet
// (3)
// If the format has variable frames per packet (asbd.mFramesPerPacket == 0) and variable bytes per packet
// (desc.mBytesPerPacket == 0) then the packet table entries are a duple of two values. The first value
// is the number of bytes in a given packet, the second value is the number of frames in a given packet

struct CAFPacketTableHeader
{
        SInt64  mNumberPackets;
        SInt64  mNumberValidFrames;
        SInt32  mPrimingFrames;
        SInt32  mRemainderFrames;
		
		UInt8   mPacketDescriptions[1]; // this is a variable length array of mNumberPackets elements
} ATTRIBUTE_PACKED;
typedef struct CAFPacketTableHeader CAFPacketTableHeader;

struct CAFDataChunk
{
		UInt32 mEditCount;
		UInt8 mData[1]; // this is a variable length data field based off the size of the data chunk
} ATTRIBUTE_PACKED;
typedef struct CAFDataChunk CAFDataChunk;

// markers types
CF_ENUM(UInt32) {
	kCAFMarkerType_Generic					= 0,
	kCAFMarkerType_ProgramStart				= 'pbeg',
	kCAFMarkerType_ProgramEnd				= 'pend',
	kCAFMarkerType_TrackStart				= 'tbeg',
	kCAFMarkerType_TrackEnd					= 'tend',
	kCAFMarkerType_Index					= 'indx',
	kCAFMarkerType_RegionStart				= 'rbeg',
	kCAFMarkerType_RegionEnd				= 'rend',
	kCAFMarkerType_RegionSyncPoint			= 'rsyc',
	kCAFMarkerType_SelectionStart			= 'sbeg',
	kCAFMarkerType_SelectionEnd				= 'send',
	kCAFMarkerType_EditSourceBegin			= 'cbeg',
	kCAFMarkerType_EditSourceEnd			= 'cend',
	kCAFMarkerType_EditDestinationBegin		= 'dbeg',
	kCAFMarkerType_EditDestinationEnd		= 'dend',
	kCAFMarkerType_SustainLoopStart			= 'slbg',
	kCAFMarkerType_SustainLoopEnd			= 'slen',
	kCAFMarkerType_ReleaseLoopStart			= 'rlbg',
	kCAFMarkerType_ReleaseLoopEnd			= 'rlen',
	kCAFMarkerType_SavedPlayPosition		= 'sply',
	kCAFMarkerType_Tempo					= 'tmpo',
	kCAFMarkerType_TimeSignature			= 'tsig',
	kCAFMarkerType_KeySignature				= 'ksig'
};

CF_ENUM(UInt32)
{
    kCAF_SMPTE_TimeTypeNone		 = 0,
    kCAF_SMPTE_TimeType24        = 1,
    kCAF_SMPTE_TimeType25        = 2,
    kCAF_SMPTE_TimeType30Drop    = 3,
    kCAF_SMPTE_TimeType30        = 4,
    kCAF_SMPTE_TimeType2997      = 5,
    kCAF_SMPTE_TimeType2997Drop  = 6,
    kCAF_SMPTE_TimeType60        = 7,
    kCAF_SMPTE_TimeType5994      = 8,
    kCAF_SMPTE_TimeType60Drop    = 9,
    kCAF_SMPTE_TimeType5994Drop  = 10,
    kCAF_SMPTE_TimeType50        = 11,
    kCAF_SMPTE_TimeType2398      = 12
};

struct CAF_SMPTE_Time
{
	SInt8 mHours;
	SInt8 mMinutes;
	SInt8 mSeconds;
	SInt8 mFrames;
	UInt32 mSubFrameSampleOffset;
} ATTRIBUTE_PACKED;
typedef struct CAF_SMPTE_Time CAF_SMPTE_Time;

struct CAFMarker
{
        UInt32               mType;
        Float64              mFramePosition;
        UInt32               mMarkerID;
        CAF_SMPTE_Time       mSMPTETime;
        UInt32               mChannel;
} ATTRIBUTE_PACKED;
typedef struct CAFMarker CAFMarker;

struct CAFMarkerChunk
{
        UInt32          mSMPTE_TimeType;
        UInt32          mNumberMarkers;
        CAFMarker       mMarkers[1]; // this is a variable length array of mNumberMarkers elements
} ATTRIBUTE_PACKED;
typedef struct CAFMarkerChunk CAFMarkerChunk;

#define kCAFMarkerChunkHdrSize		offsetof(CAFMarkerChunk, mMarkers)

typedef CF_OPTIONS(UInt32, CAFRegionFlags) {
	kCAFRegionFlag_LoopEnable = 1,
	kCAFRegionFlag_PlayForward = 2,
	kCAFRegionFlag_PlayBackward = 4
};

struct CAFRegion
{
        UInt32          mRegionID;               
        CAFRegionFlags  mFlags;
        UInt32          mNumberMarkers;
        CAFMarker       mMarkers[1]; // this is a variable length array of mNumberMarkers elements
} ATTRIBUTE_PACKED;
typedef struct CAFRegion CAFRegion;

/* because AudioFileRegions are variable length, you cannot access them as an array. Use NextAudioFileRegion to walk the list. */
#define NextCAFRegion(inCAFRegionPtr) \
        ((CAFRegion*)((char*)(inCAFRegionPtr) + offsetof(CAFRegion, mMarkers) + ((inCAFRegionPtr)->mNumberMarkers)*sizeof(CAFMarker)))


struct CAFRegionChunk
{
        UInt32          mSMPTE_TimeType;
        UInt32          mNumberRegions;
        CAFRegion       mRegions[1];  // this is a variable length array of mNumberRegions elements
} ATTRIBUTE_PACKED;
typedef struct CAFRegionChunk CAFRegionChunk;

#define kCAFRegionChunkHdrSize		offsetof(CAFRegionChunk, mRegions)

struct CAFInstrumentChunk
{
        Float32         mBaseNote;
        UInt8           mMIDILowNote;
        UInt8           mMIDIHighNote;
        UInt8           mMIDILowVelocity;
        UInt8           mMIDIHighVelocity;
        Float32         mdBGain;
        UInt32          mStartRegionID;               
        UInt32          mSustainRegionID;               
        UInt32          mReleaseRegionID;               
        UInt32          mInstrumentID;
} ATTRIBUTE_PACKED;
typedef struct CAFInstrumentChunk CAFInstrumentChunk;



struct CAFStringID {
        UInt32         mStringID;
        SInt64         mStringStartByteOffset;
} ATTRIBUTE_PACKED;
typedef struct CAFStringID CAFStringID;

struct CAFStrings
{
        UInt32         mNumEntries;
		CAFStringID    mStringsIDs[1]; // this is a variable length array of mNumEntries elements
// this struct is only fictionally described due to the variable length fields
//		UInt8          mStrings[ variable num elements ]; // null terminated UTF8 strings
} ATTRIBUTE_PACKED;
typedef struct CAFStrings CAFStrings;


struct CAFInfoStrings
{
        UInt32		mNumEntries;

// These are only fictionally defined in the struct due to the variable length fields.
//      struct {
//              UInt8   mKey[ variable num elements ];			// null terminated UTF8 string
//              UInt8   mValue[ variable num elements ];		// null terminated UTF8 string
//      } mStrings[ variable num elements ];
} ATTRIBUTE_PACKED;
typedef struct CAFInfoStrings CAFInfoStrings;


struct CAFPositionPeak
{
        Float32       mValue;
        UInt64        mFrameNumber;
} ATTRIBUTE_PACKED;
typedef struct CAFPositionPeak CAFPositionPeak;

struct CAFPeakChunk
{
	UInt32 mEditCount;
	CAFPositionPeak mPeaks[1]; // this is a variable length array of peak elements (calculated from the size of the chunk)
} ATTRIBUTE_PACKED;
typedef struct CAFPeakChunk CAFPeakChunk;


struct CAFOverviewSample
{
        SInt16       mMinValue;
        SInt16       mMaxValue;
} ATTRIBUTE_PACKED;
typedef struct CAFOverviewSample CAFOverviewSample;

struct CAFOverviewChunk
{
        UInt32                   mEditCount;
        UInt32                   mNumFramesPerOVWSample;
        CAFOverviewSample        mData[1]; // data is of variable size, calculated from the sizeo of the chunk.
} ATTRIBUTE_PACKED;
typedef struct CAFOverviewChunk CAFOverviewChunk;

struct CAFUMIDChunk
{
	UInt8 mBytes[64];
} ATTRIBUTE_PACKED;
typedef struct CAFUMIDChunk CAFUMIDChunk;

#if TARGET_OS_WIN32
#pragma pack(pop)
#endif
////////////////////////////////////////////////////////////////////////////////////////////////

#endif // AudioToolbox_CAFFile_h
#else
#include <AudioToolboxCore/CAFFile.h>
#endif
