#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioFileStream.h>)
/*!
	@file		AudioFileStream.h
	@framework	AudioToolbox.framework
	@copyright	(c) 1985-2015 by Apple, Inc., all rights reserved.

	@brief		API's to parse streamed audio files into packets of audio data.

	@discussion

	AudioFileStream addresses situations where, in a stream of audio data, only a limited window
	of data may be available at any time.

	This case differs significantly enough from the random access file case to warrant a separate
	API rather than overload the AudioFile API with additional semantics. With a random access file,
	one can always assume that a read request for contiguous data that doesn't include EOF will
	always supply all of the data. This makes parsing straightforward and inexpensive. In the
	streaming case such an assumption cannot be made. A request by the parser for data from the
	stream may only be partially satisfied. Any partially satisfied requests must be remembered and
	retried before any other requests are satisfied, otherwise the streamed data might be lost
	forever in the past. So the parser must be able to suspend work at any point and resume parsing
	where it left off.
	
	The client provides data to the parser using AudioFileStreamParseBytes and the parser calls back
	to the client with properties or packets using the AudioFileStream_PropertyListenerProc and
	AudioFileStream_PacketsProc function pointers.
*/

#ifndef AudioToolbox_AudioFileStream_h
#define AudioToolbox_AudioFileStream_h

//=============================================================================
//	Includes
//=============================================================================

#include <AudioToolbox/AudioFile.h>

CF_ASSUME_NONNULL_BEGIN

#if defined(__cplusplus)
extern "C"
{
#endif

//=============================================================================
//	AudioFileStream flags
//=============================================================================
#pragma mark flags

/*!
    @enum AudioFileStreamPropertyFlags
    @constant   kAudioFileStreamPropertyFlag_PropertyIsCached 
		This flag is set in a call to AudioFileStream_PropertyListenerProc when the value of the property
		can be obtained at any later time. If this flag is not set, then you should either get the value of 
		the property from within this callback or set the flag kAudioFileStreamPropertyFlag_CacheProperty in order to signal
		to the parser to begin caching the property data. Otherwise the value may not be available in the future.
		
    @constant   kAudioFileStreamPropertyFlag_CacheProperty 
		This flag can be set by a property listener in order to signal to the parser that the client is
		interested in the value of the property and that it should be cached until the full value of the property is available.
*/
typedef CF_OPTIONS(UInt32, AudioFileStreamPropertyFlags) {
	kAudioFileStreamPropertyFlag_PropertyIsCached = 1,
	kAudioFileStreamPropertyFlag_CacheProperty = 2
};

/*!	@enum	AudioFileStreamParseFlags
    @constant   kAudioFileStreamParseFlag_Discontinuity 
		This flag is passed in to AudioFileStreamParseBytes to signal a discontinuity. Any partial packet straddling a buffer
		boundary will be discarded. This is necessary to avoid being called with a corrupt packet. After a discontinuity occurs
		seeking may be approximate in some data formats.
*/
typedef CF_OPTIONS(UInt32, AudioFileStreamParseFlags) {
	kAudioFileStreamParseFlag_Discontinuity = 1
};

/*!	@enum	AudioFileStreamParseFlags
    @constant   kAudioFileStreamSeekFlag_OffsetIsEstimated 
		This flag may be returned from AudioFileStreamSeek if the byte offset is only an estimate, not exact.
*/
typedef CF_OPTIONS(UInt32, AudioFileStreamSeekFlags) {
	kAudioFileStreamSeekFlag_OffsetIsEstimated = 1
};

//=============================================================================
//	AudioFileStream Types
//=============================================================================
#pragma mark types

typedef UInt32 AudioFileStreamPropertyID;
typedef	struct OpaqueAudioFileStreamID	*AudioFileStreamID;

typedef void (*AudioFileStream_PropertyListenerProc)(
											void *							inClientData,
											AudioFileStreamID				inAudioFileStream,
											AudioFileStreamPropertyID		inPropertyID,
											AudioFileStreamPropertyFlags *	ioFlags);

typedef void (*AudioFileStream_PacketsProc)(
											void *										inClientData,
											UInt32										inNumberBytes,
											UInt32										inNumberPackets,
											const void *								inInputData,
											AudioStreamPacketDescription * __nullable	inPacketDescriptions);

//=============================================================================
//	AudioFileStream error codes
//=============================================================================
#pragma mark Error codes
/*!
    @enum AudioFileStream error codes

    @abstract   These are the error codes returned from the AudioFile API.

    @constant   kAudioFileStreamError_UnsupportedFileType 
		The file type is not supported.
    @constant   kAudioFileStreamError_UnsupportedDataFormat 
		The data format is not supported by this file type.
    @constant   kAudioFileStreamError_UnsupportedProperty 
		The property is not supported.
    @constant   kAudioFileStreamError_BadPropertySize 
		The size of the property data was not correct.
    @constant   kAudioFileStreamError_NotOptimized 
		It is not possible to produce output packets because the file's packet table or other defining 
		info is either not present or is after the audio data.
    @constant   kAudioFileStreamError_InvalidPacketOffset 
		A packet offset was less than zero, or past the end of the file,
		or a corrupt packet size was read when building the packet table. 
    @constant   kAudioFileStreamError_InvalidFile 
		The file is malformed, or otherwise not a valid instance of an audio file of its type, or 
		is not recognized as an audio file. 
    @constant   kAudioFileStreamError_ValueUnknown 
		The property value is not present in this file before the audio data.
	@constant	kAudioFileStreamError_DataUnavailable
		The amount of data provided to the parser was insufficient to produce any result.
	@constant	kAudioFileStreamError_IllegalOperation
		An illegal operation was attempted.
    @constant   kAudioFileStreamError_UnspecifiedError 
		An unspecified error has occurred.
		
*/

CF_ENUM(OSStatus)
{
	kAudioFileStreamError_UnsupportedFileType		= 'typ?',
	kAudioFileStreamError_UnsupportedDataFormat		= 'fmt?',
	kAudioFileStreamError_UnsupportedProperty		= 'pty?',
	kAudioFileStreamError_BadPropertySize			= '!siz',
	kAudioFileStreamError_NotOptimized				= 'optm',
	kAudioFileStreamError_InvalidPacketOffset		= 'pck?',
	kAudioFileStreamError_InvalidFile				= 'dta?',
	kAudioFileStreamError_ValueUnknown				= 'unk?',
	kAudioFileStreamError_DataUnavailable			= 'more',
	kAudioFileStreamError_IllegalOperation			= 'nope',
	kAudioFileStreamError_UnspecifiedError			= 'wht?',
	kAudioFileStreamError_DiscontinuityCantRecover	= 'dsc!'
};

//=============================================================================
//	AudioFileStream Properties
//=============================================================================
#pragma mark Properties

/*!
    @enum		AudioFileStream Properties
	
    @abstract   constants for AudioFileStream get property calls
    @discussion		There are currently no settable properties.

					
    @constant   kAudioFileStreamProperty_ReadyToProducePackets
					An UInt32 which is zero until the parser has parsed up to the beginning of the audio data. 
					Once it has reached the audio data, the value of this property becomes one. 
					When this value has become one, all properties that can be known about the stream are known.
					
    @constant   kAudioFileStreamProperty_FileFormat 
					An UInt32 four char code that identifies the format of the file
    @constant   kAudioFileStreamProperty_DataFormat 
					An AudioStreamBasicDescription describing the format of the audio data
    @constant   kAudioFileStreamProperty_FormatList 
					In order to support formats such as AAC SBR where an encoded data stream can be decoded to 
					multiple destination formats, this property returns an array of AudioFormatListItems 
					(see AudioFormat.h) of those formats.
					The default behavior is to return the an AudioFormatListItem that has the same 
					AudioStreamBasicDescription that kAudioFileStreamProperty_DataFormat returns.
    @constant   kAudioFileStreamProperty_MagicCookieData 
					A void * pointing to memory set up by the caller.
					Some file types require that a magic cookie be provided before packets can be written
					to the file, so this property should be set before calling 
					AudioFileWriteBytes()/AudioFileWritePackets() if a magic cookie exists.
    @constant   kAudioFileStreamProperty_AudioDataByteCount 
					a UInt64 that indicates the number of bytes of audio data contained in the file
    @constant   kAudioFileStreamProperty_AudioDataPacketCount 
					a UInt64 that indicates the number of packets of audio data contained in the file
    @constant   kAudioFileStreamProperty_MaximumPacketSize 
					a UInt32 that indicates the maximum size of a packet for the data contained in the file
    @constant   kAudioFileStreamProperty_DataOffset 
					a SInt64 that indicates the byte offset in the file of the audio data.
    @constant   kAudioFileStreamProperty_ChannelLayout 
					An AudioChannelLayout struct.
    @constant   kAudioFileStreamProperty_PacketToFrame 
					pass a AudioFramePacketTranslation with mPacket filled out and get mFrame back. 
					mFrameOffsetInPacket is ignored.
    @constant   kAudioFileStreamProperty_FrameToPacket 
					pass a AudioFramePacketTranslation with mFrame filled out and get mPacket and 
					mFrameOffsetInPacket back.
    @constant   kAudioFileStreamProperty_RestrictsRandomAccess
					A UInt32 indicating whether an Audio File contains packets that cannot be used as random
					access points.
					A value of 0 indicates that any packet can be used as a random access point, i.e. that a
					decoder can start decoding with any packet.
					A value of 1 indicates that some packets cannot be used as random access points, i.e.
					that either kAudioFileStreamProperty_PacketToRollDistance or
					kAudioFileStreamProperty_PacketToDependencyInfo must be employed in order to identify an
					appropriate initial packet for decoding.
	@constant	kAudioFileStreamProperty_PacketToRollDistance
					Pass an AudioPacketRollDistanceTranslation with mPacket filled out and get mRollDistance
					back.
					See AudioFile.h for the declaration of AudioPacketRollDistanceTranslation.
					The roll distance indicates the count of packets that must be decoded prior to the
					packet with the specified number in order to achieve full refresh of the decoder at that
					packet.
					For file formats that do not carry comprehensive information regarding independently
					decodable packets, accurate roll distances may be available only for the range of
					packets either currently or most recently provided to your packets proc.
	@constant   kAudioFileStreamProperty_PreviousIndependentPacket
	@constant	kAudioFileStreamProperty_NextIndependentPacket
					Pass an AudioIndependentPacketTranslation with mPacket filled out and get
					mIndependentlyDecodablePacket back. A value of -1 means that no independent packet is
					present in the stream in the direction of interest. Otherwise, for
					kAudioFileStreamProperty_PreviousIndependentPacket, mIndependentlyDecodablePacket will be
					less than mPacket, and for kAudioFileStreamProperty_NextIndependentPacket,
					mIndependentlyDecodablePacket will be greater than mPacket.
					For file formats that do not carry comprehensive information regarding independently
					decodable packets, independent packets may be identifiable only within the range of
					packets either currently or most recently provided to your packets proc.
	@constant	kAudioFileStreamProperty_PacketToDependencyInfo
					Pass an AudioPacketDependencyInfoTranslation with mPacket filled out and get
					mIsIndependentlyDecodable and mPrerollPacketCount back.
					A value of 0 for mIsIndependentlyDecodable indicates that the specified packet is not
					independently decodable.
					A value of 1 for mIsIndependentlyDecodable indicates that the specified packet is
					independently decodable.
					For independently decodable packets, mPrerollPacketCount indicates the count of packets
					that must be decoded after the packet with the specified number in order to refresh the
					decoder.
					For file formats that do not carry comprehensive information regarding packet
					dependencies, accurate dependency info may be available only for the range of
					packets either currently or most recently provided to your packets proc.
	@constant	kAudioFileStreamProperty_PacketToByte
					pass an AudioBytePacketTranslation struct with mPacket filled out and get mByte back.
					mByteOffsetInPacket is ignored. If the mByte value is an estimate then 
					kBytePacketTranslationFlag_IsEstimate will be set in the mFlags field.
	@constant	kAudioFileStreamProperty_ByteToPacket
					pass an AudioBytePacketTranslation struct with mByte filled out and get mPacket and
					mByteOffsetInPacket back. If the mPacket value is an estimate then 
					kBytePacketTranslationFlag_IsEstimate will be set in the mFlags field.
    @constant   kAudioFileStreamProperty_PacketTableInfo 
					Gets the AudioFilePacketTableInfo struct for the file types that support it.
	@constant	kAudioFileStreamProperty_PacketSizeUpperBound
					a UInt32 for the theoretical maximum packet size in the file.
	@constant	kAudioFileStreamProperty_AverageBytesPerPacket
					a Float64 of giving the average bytes per packet seen. 
					For CBR and files with packet tables, this number will be exact. Otherwise, it is a
					running average of packets parsed.
	@constant	kAudioFileStreamProperty_BitRate
					a UInt32 of the bit rate in bits per second.
    @constant	kAudioFileStreamProperty_InfoDictionary
                    a CFDictionary filled with information about the data contained in the stream.
                    See AudioFile.h for InfoDictionary key strings. Caller is responsible for releasing the CFObject.
*/
CF_ENUM(AudioFileStreamPropertyID)
{
	kAudioFileStreamProperty_ReadyToProducePackets			=	'redy',
	kAudioFileStreamProperty_FileFormat						=	'ffmt',
	kAudioFileStreamProperty_DataFormat						=	'dfmt',
	kAudioFileStreamProperty_FormatList						=	'flst',
	kAudioFileStreamProperty_MagicCookieData				=	'mgic',
	kAudioFileStreamProperty_AudioDataByteCount				=	'bcnt',
	kAudioFileStreamProperty_AudioDataPacketCount			=	'pcnt',
	kAudioFileStreamProperty_MaximumPacketSize				=	'psze',
	kAudioFileStreamProperty_DataOffset						=	'doff',
	kAudioFileStreamProperty_ChannelLayout					=	'cmap',
	kAudioFileStreamProperty_PacketToFrame					=	'pkfr',
	kAudioFileStreamProperty_FrameToPacket					=	'frpk',
	kAudioFileStreamProperty_RestrictsRandomAccess          =   'rrap',
	kAudioFileStreamProperty_PacketToRollDistance           =   'pkrl',
	kAudioFileStreamProperty_PreviousIndependentPacket      =   'pind',
	kAudioFileStreamProperty_NextIndependentPacket          =   'nind',
	kAudioFileStreamProperty_PacketToDependencyInfo         =   'pkdp',
	kAudioFileStreamProperty_PacketToByte					=	'pkby',
	kAudioFileStreamProperty_ByteToPacket					=	'bypk',
	kAudioFileStreamProperty_PacketTableInfo				=	'pnfo',
	kAudioFileStreamProperty_PacketSizeUpperBound  			=	'pkub',
	kAudioFileStreamProperty_AverageBytesPerPacket			=	'abpp',
	kAudioFileStreamProperty_BitRate						=	'brat',
	kAudioFileStreamProperty_InfoDictionary                 = 	'info'
};

//=============================================================================
//	AudioFileStream Functions
//=============================================================================
#pragma mark Functions


/*!
	@function		AudioFileStreamOpen

	@discussion		Create a new audio file stream parser.
					The client provides the parser with data and the parser calls
					callbacks when interesting things are found in the data, such as properties and 
					audio packets.

    @param			inClientData					
						a constant that will be passed to your callbacks.
	@param			inPropertyListenerProc
						Whenever the value of a property is parsed in the data, this function will be called.
						You can then get the value of the property from in the callback. In some cases, due to 
						boundaries in the input data, the property may return kAudioFileStreamError_DataUnavailable.
						When unavailable data is requested from within the property listener, the parser will begin 
						caching the property value and will call the property listener again when the property is
						available. For property values for which kAudioFileStreamPropertyFlag_PropertyIsCached is unset, this 
						will be the only opportunity to get the value of the property, since the data will be 
						disposed upon return of the property listener callback. 
	@param			inPacketsProc
						Whenever packets are parsed in the data, a pointer to the packets is passed to the client 
						using this callback. At times only a single packet may be passed due to boundaries in the 
						input data.
    @param 			inFileTypeHint	
						For files whose type cannot be easily or uniquely determined from the data (ADTS,AC3), 
						this hint can be used to indicate the file type. 
						Otherwise if you do not know the file type, you can pass zero. 
	@param			outAudioFileStream 
						A new file stream ID for use in other AudioFileStream API calls.
*/ 
extern OSStatus	
AudioFileStreamOpen (
							void * __nullable						inClientData,
							AudioFileStream_PropertyListenerProc	inPropertyListenerProc,
							AudioFileStream_PacketsProc				inPacketsProc,
                			AudioFileTypeID							inFileTypeHint,
                			AudioFileStreamID __nullable * __nonnull outAudioFileStream)
																		API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));


/*!
	@function		AudioFileStreamParseBytes

	@discussion		This call is the means for streams to supply data to the parser. 
					Data is expected to be passed in sequentially from the beginning of the file, without gaps.
					In the course of parsing, the client's property and/or packets callbacks may be called.
					At the end of the stream, this function must be called once with null data pointer and zero
					data byte size to flush any remaining packets out of the parser.

	@param			inAudioFileStream 
						The file stream ID
	@param			inDataByteSize 
						The number of bytes passed in for parsing. Must be zero when flushing the parser.
	@param			inData 
						The data passed in to be parsed. Must be null when flushing the parser.
	@param			inFlags 
						If there is a data discontinuity, then kAudioFileStreamParseFlag_Discontinuity should be set true. 
*/
extern OSStatus
AudioFileStreamParseBytes(	
								AudioFileStreamID				inAudioFileStream,
								UInt32							inDataByteSize,
								const void * __nullable			inData,
								AudioFileStreamParseFlags		inFlags)
																		API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
	@function		AudioFileStreamSeek

	@discussion		This call is used to seek in the data stream. The client passes in a packet 
					offset to seek to and the parser passes back a byte offset from which to
					get the data to satisfy that request. The data passed to the next call to 
					AudioFileParseBytes will be assumed to be from that byte offset.
					For file formats which do not contain packet tables the byte offset may 
					be an estimate. If so, the flag kAudioFileStreamSeekFlag_OffsetIsEstimated will be true.

	@param			inAudioFileStream 
						The file stream ID
	@param			inPacketOffset 
						The offset from the beginning of the file of the packet to which to seek.
	@param			outDataByteOffset 
						The byte offset of the data from the file's data offset returned. 
						You need to add the value of kAudioFileStreamProperty_DataOffset to get an absolute byte offset in the file.
	@param			ioFlags
						If outDataByteOffset is an estimate, then kAudioFileStreamSeekFlag_OffsetIsEstimated will be set on output.
						There are currently no flags defined for passing into this call.
*/
extern OSStatus
AudioFileStreamSeek(	
								AudioFileStreamID				inAudioFileStream,
								SInt64							inPacketOffset,
								SInt64 *						outDataByteOffset,
								AudioFileStreamSeekFlags *		ioFlags)
																		API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
	@function		AudioFileStreamGetPropertyInfo
 
	@discussion		Retrieve the info about the given property. The outSize argument
					will return the size in bytes of the current value of the property.
 
	@param			inAudioFileStream 
						The file stream ID
	@param			inPropertyID
						Property ID whose value should be read
	@param			outPropertyDataSize
						Size in bytes of the property
	@param			outWritable
						whether the property is writable
 
	@result			an OSStatus return code
*/
extern OSStatus
AudioFileStreamGetPropertyInfo(	
								AudioFileStreamID				inAudioFileStream,
								AudioFileStreamPropertyID		inPropertyID,
								UInt32 * __nullable				outPropertyDataSize,
								Boolean * __nullable			outWritable)
																		API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));


/*!
	@function		AudioFileStreamGetProperty
 
	@discussion		Retrieve the indicated property data. 
 
	@param			inAudioFileStream 
						The file stream ID
	@param			inPropertyID
						Property ID whose value should be read
	@param			ioPropertyDataSize
						On input, the size of the buffer pointed to by outPropertyData. On output, 
						the number of bytes written.
	@param			outPropertyData
						Pointer to the property data buffer

	@result			an OSStatus return code
*/
extern OSStatus
AudioFileStreamGetProperty(	
							AudioFileStreamID					inAudioFileStream,
							AudioFileStreamPropertyID			inPropertyID,
							UInt32 *							ioPropertyDataSize,
							void *								outPropertyData)		
																		API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
	@function		AudioFileStreamSetProperty
 
	@discussion		Set the value of the property. There are currently no settable properties.
 
	@param			inAudioFileStream 
						The file stream ID
	@param			inPropertyID
						Property ID whose value should be set
	@param			inPropertyDataSize
						Size in bytes of the property data
	@param			inPropertyData
						Pointer to the property data buffer

	@result			an OSStatus return code
*/
extern OSStatus
AudioFileStreamSetProperty(	
							AudioFileStreamID					inAudioFileStream,
							AudioFileStreamPropertyID			inPropertyID,
							UInt32								inPropertyDataSize,
							const void *						inPropertyData)		
																		API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));

/*!
	@function		AudioFileStreamClose
 
	@discussion		Close and deallocate the file stream object.

	@param			inAudioFileStream 
						The file stream ID
*/
extern OSStatus
AudioFileStreamClose(			AudioFileStreamID				inAudioFileStream)										
																		API_AVAILABLE(macos(10.5), ios(2.0), watchos(2.0), tvos(9.0));


#if defined(__cplusplus)
}
#endif

CF_ASSUME_NONNULL_END

#endif // AudioToolbox_AudioFileStream_h

#else
#include <AudioToolboxCore/AudioFileStream.h>
#endif
