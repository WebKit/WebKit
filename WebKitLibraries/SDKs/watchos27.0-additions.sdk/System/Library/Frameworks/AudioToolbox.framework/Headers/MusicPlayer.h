/*!
	@file		MusicPlayer.h
	@framework	AudioToolbox.framework
	@copyright	(c) 2000-2015 by Apple, Inc., all rights reserved.

	@abstract	API's for Music sequencing and playing services
	
	@discussion
		The objects in this API set include:

			- Music Sequence: a container of music tracks
			- Music Track: a time ordered list of events
			- Music Track Iterator: an object to iterate over events in a track
			- Music Player: an object used to play a sequence
		
		A MusicSequence contains an arbitrary number of tracks (MusicTrack) each of which contains
		time-stamped (in units of beats) events in time-increasing order. There are various types of
		events, defined below, including the expected MIDI events, tempo, and extended events.
		
		A MusicTrack has properties which may be inspected and assigned, including support for
		looping, muting/soloing, and time-stamp interpretation. APIs exist for iterating through the
		events in a MusicTrack, and for performing editing operations on them.
		
		A MusicPlayer is used to play a sequence and provides control of playback rate and setting
		to a particular time.
		
		Each MusicSequence may have an associated AUGraph object, which represents a set of
		AudioUnits and the connections between them.  Then, each MusicTrack of the MusicSequence may
		address its events to a specific AudioUnit within the AUGraph. In such a manner, it's
		possible to automate arbitrary parameters of AudioUnits, and schedule notes to be played to
		MusicDevices (AudioUnit software synthesizers) within an arbitrary audio processing network
		(AUGraph). A MusicSequence or its tracks can also address a CoreMIDI endpoint directly.
*/

#ifndef AudioToolbox_MusicPlayer_h
#define AudioToolbox_MusicPlayer_h

#include <Availability.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/MusicDevice.h>
#include <AudioToolbox/AUGraph.h>

#if __has_include(<CoreMIDI/MIDIServices.h>)
	#include <CoreMIDI/MIDIServices.h>
#else
	typedef UInt32 MIDIEndpointRef;
#endif


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"

CF_ASSUME_NONNULL_BEGIN

#if defined(__cplusplus)
extern "C"
{
#endif

/*!
	enum MusicEventType
	@abstract Music event types, including both MIDI and "extended" protocol
	
	@constant kMusicEventType_NULL
		
	@constant kMusicEventType_ExtendedNote
		Note with variable number of arguments (non-MIDI).
	@constant kMusicEventType_ExtendedTempo
		Tempo change in BPM.
	@constant kMusicEventType_User
		User defined data.
	@constant kMusicEventType_Meta
		Standard MIDI File meta-event.
	@constant kMusicEventType_MIDINoteMessage
		MIDI note-on with duration (for note-off).
	@constant kMusicEventType_MIDIChannelMessage
		MIDI channel message (other than note-on/off).
	@constant kMusicEventType_MIDIRawData
		For MIDI system exclusive data.
	@constant kMusicEventType_Parameter
		General purpose AudioUnit parameter, added in macOS 10.2.
	@constant kMusicEventType_AUPreset
		An AudioUnit's user preset CFDictionaryRef (the ClassInfo property), added 10.3.
*/
CF_ENUM(UInt32)
{
	kMusicEventType_NULL					= 0,
	kMusicEventType_ExtendedNote			= 1,
	kMusicEventType_ExtendedTempo			= 3,
	kMusicEventType_User					= 4,
	kMusicEventType_Meta					= 5,
	kMusicEventType_MIDINoteMessage			= 6,
	kMusicEventType_MIDIChannelMessage		= 7,
	kMusicEventType_MIDIRawData				= 8,
	kMusicEventType_Parameter				= 9,
	kMusicEventType_AUPreset				= 10
};
typedef UInt32		MusicEventType;

/*!
	@enum MusicSequenceLoadFlags
	@abstract Flags used to customise loading behaviour
 	@constant	kMusicSequenceLoadSMF_PreserveTracks
			If this flag is set the resultant Sequence will contain:
			a tempo track
			a track for each track found in the SMF
 			This is the default behavior
	@constant	kMusicSequenceLoadSMF_ChannelsToTracks
			If this flag is set the resultant Sequence will contain:
			a tempo track
			1 track for each MIDI Channel that is found in the SMF
			1 track for SysEx or MetaEvents - this will be the last track 
			in the sequence after the LoadSMFWithFlags calls
*/
typedef CF_OPTIONS(UInt32, MusicSequenceLoadFlags)
{
	kMusicSequenceLoadSMF_PreserveTracks	= 0,
	kMusicSequenceLoadSMF_ChannelsToTracks 	= (1 << 0)
};

/*!
	@enum MusicSequenceType
	@abstract	A sequence type
	@discussion Different sequence types to describe the basic mode of operation of a sequence's time line
				You cannot change a music sequence's type to samples/seconds if there are tempo events
				The type will also define how the sequence is saved to a MIDI file:
					Beats - normal midi file
					Seconds - midi file with SMPTE time
					Samples - cannot be saved to a midi file
	@constant	kMusicSequenceType_Beats
					The default/normal type of a sequence.
					Tempo track defines the number of beats per second and can have multiple tempo events
	@constant	kMusicSequenceType_Seconds
					A music sequence with a single 60bpm tempo event
	@constant	kMusicSequenceType_Samples
					A music sequence with a single tempo event that represents the audio sample rate
*/
typedef CF_ENUM(UInt32, MusicSequenceType) {
	kMusicSequenceType_Beats		= 'beat',
	kMusicSequenceType_Seconds		= 'secs',
	kMusicSequenceType_Samples		= 'samp'
};

/*!
	@enum MusicSequenceFileTypeID
	@abstract	describes different types of files that can be parsed by a music sequence
 	@constant	kMusicSequenceFile_AnyType
 					let the system read iMelody files and read and write MIDI files (and any future types)
	@constant	kMusicSequenceFile_MIDIType
					read and write MIDI files
	@constant	kMusicSequenceFile_iMelodyType
					read iMelody files
*/
typedef CF_ENUM(UInt32, MusicSequenceFileTypeID) {
	kMusicSequenceFile_AnyType			= 0,
	kMusicSequenceFile_MIDIType			= 'midi',
	kMusicSequenceFile_iMelodyType		= 'imel'
};

/*!
	@enum MusicSequenceFileFlags
	@abstract	controls the behaviour of the create file calls
	@constant	kMusicSequenceFileFlags_Default
 					Does not overwrite existing files.  Attempts to save over an existing file
 					will return kAudio_FilePermissionError
	@constant	kMusicSequenceFileFlags_EraseFile
					Erase an existing file when creating a new file
*/
typedef CF_OPTIONS(UInt32, MusicSequenceFileFlags) {
	kMusicSequenceFileFlags_Default	  = 0,
	kMusicSequenceFileFlags_EraseFile = 1
};


/*!
	@typedef	MusicTimeStamp
	@abstract	The type used to refer to time values in a music sequence
*/
typedef Float64		MusicTimeStamp;

/*!
	@struct		kMusicTimeStamp_EndOfTrack
	@abstract	used to signal end of track
	@discussion Pass this value in to indicate a time passed the last event in the track.
				In this way, it's possible to perform edits on tracks which include all events
				starting at some particular time up to and including the last event
*/
#define kMusicTimeStamp_EndOfTrack			DBL_MAX

/*!
	@struct		MIDINoteMessage
	@discussion	The parameters to specify a MIDI note
*/
typedef struct MIDINoteMessage
{
	UInt8		channel;
	UInt8		note;
	UInt8		velocity;
	UInt8		releaseVelocity;	// was "reserved". 0 is the correct value when you don't know.
	Float32		duration;
} MIDINoteMessage;

/*!
	@struct		MIDIChannelMessage
	@discussion	The parameters to specify a MIDI channel message
*/
typedef struct MIDIChannelMessage
{
	UInt8		status;		// contains message and channel
	
	// message specific data
	UInt8		data1;		
	UInt8		data2;
	UInt8		reserved;
} MIDIChannelMessage;

/*!
	@struct		MIDIRawData
	@discussion	Generally used to represent a MIDI SysEx message
*/
typedef struct MIDIRawData
{
	UInt32		length;
	UInt8		data[1];
} MIDIRawData;

/*!
	@struct		MIDIMetaEvent
	@discussion	The parameters to specify a MIDI meta event
*/
typedef struct MIDIMetaEvent
{
	UInt8		metaEventType;
	UInt8		unused1;
	UInt8		unused2;
	UInt8		unused3;
	UInt32		dataLength;
	UInt8		data[1];
} MIDIMetaEvent;

/*!
	@struct		MusicEventUserData
	@discussion	Provides a general struct for specifying a user defined event. 
	@var  		length
					the size in bytes of the data
	@var  		data
					size bytes of user defined event data
*/
typedef struct MusicEventUserData
{
	UInt32		length;
	UInt8		data[1];
} MusicEventUserData;

/*!
	@struct		ExtendedNoteOnEvent
	@discussion	The parameters to specify an extended note on event
*/
typedef struct ExtendedNoteOnEvent
{
	MusicDeviceInstrumentID		instrumentID;
	MusicDeviceGroupID			groupID;
	Float32						duration;
	MusicDeviceNoteParams		extendedParams;
} ExtendedNoteOnEvent;

/*!
	@struct		ParameterEvent
	@discussion	The parameters to specify a parameter event to an audio unit.
*/
typedef struct ParameterEvent
{
	AudioUnitParameterID		parameterID;
	AudioUnitScope				scope;
    AudioUnitElement			element;
	AudioUnitParameterValue		value;
} ParameterEvent;

/*!
	@struct		ExtendedTempoEvent
	@discussion	specifies the value for a tempo in beats per minute
*/
typedef struct ExtendedTempoEvent
{
	Float64		bpm;
} ExtendedTempoEvent;

/*!
	@struct		AUPresetEvent
	@discussion	The parameters to specify a preset for an audio unit.
*/
typedef struct AUPresetEvent
{
	AudioUnitScope				scope;
    AudioUnitElement			element;
	CFPropertyListRef 			preset;
} AUPresetEvent;


/*!
	@struct		CABarBeatTime
	@abstract	A display representation of a musical time in beats.
	
	A clock's internal representation of musical time is in beats based on the
	beginning of the timeline. Normally, such times should be displayed to the user
	in terms of bars, beats, and subbeats (sometimes called "units" or "parts per
	quarter" [PPQ]). This data structure is such a display representation.

	By convention, bar 1 is the beginning of the sequence. Beat 1 is the first beat
	of the measure. In 4/4 time, beat will have a value from 1 to 4. Music
	applications often use beat divisions such as 480 and 960.

	@var  	bar
				A measure number.
	@var  	beat
				A beat number (1..n).
	@var  	subbeat
				The numerator of the fractional number of beats.
	@var  	subbeatDivisor
				The denominator of the fractional number of beats.
	@var  	reserved
				Must be 0.
*/
struct CABarBeatTime {
	SInt32				bar;
	UInt16				beat;
	UInt16				subbeat;
	UInt16				subbeatDivisor;
	UInt16				reserved;
};
typedef struct CABarBeatTime CABarBeatTime;

typedef struct OpaqueMusicPlayer		*MusicPlayer;
typedef struct OpaqueMusicSequence		*MusicSequence;
typedef struct OpaqueMusicTrack			*MusicTrack;
typedef struct OpaqueMusicEventIterator *MusicEventIterator;

/*!
	@typedef MusicSequenceUserCallback
	@discussion See MusicSequenceSetUserCallback
*/
typedef void (*MusicSequenceUserCallback)(
											void * __nullable			inClientData,
											MusicSequence				inSequence,
											MusicTrack					inTrack,
											MusicTimeStamp				inEventTime,
											const MusicEventUserData *	inEventData,
											MusicTimeStamp				inStartSliceBeat,
											MusicTimeStamp				inEndSliceBeat) CA_REALTIME_API;

/*!
	enum MusicPlayerErrors
	@constant	kAudioToolboxErr_InvalidSequenceType
	@constant	kAudioToolboxErr_TrackIndexError
	@constant	kAudioToolboxErr_TrackNotFound
	@constant	kAudioToolboxErr_EndOfTrack
	@constant	kAudioToolboxErr_StartOfTrack
    @constant	kAudioToolboxErr_IllegalTrackDestination
    @constant	kAudioToolboxErr_NoSequence
	@constant	kAudioToolboxErr_InvalidEventType
	@constant	kAudioToolboxErr_InvalidPlayerState
	@constant	kAudioToolboxErr_CannotDoInCurrentContext
*/
CF_ENUM(OSStatus)
{
	kAudioToolboxErr_InvalidSequenceType		= -10846,
	kAudioToolboxErr_TrackIndexError 			= -10859,
	kAudioToolboxErr_TrackNotFound				= -10858,
	kAudioToolboxErr_EndOfTrack					= -10857,
	kAudioToolboxErr_StartOfTrack				= -10856,
    kAudioToolboxErr_IllegalTrackDestination	= -10855,
    kAudioToolboxErr_NoSequence 				= -10854,
	kAudioToolboxErr_InvalidEventType			= -10853,
	kAudioToolboxErr_InvalidPlayerState			= -10852,
	kAudioToolboxErr_CannotDoInCurrentContext	= -10863,
	kAudioToolboxError_NoTrackDestination		= -66720
};

/*!
	enum MusicTrackProperties
	@discussion Property values are always get and set by reference
	
	@constant	kSequenceTrackProperty_LoopInfo
		read/write	- MusicTrackLoopInfo
		The default looping behaviour is off (track plays once)
		Looping is set by specifying the length of the loop. It loops from
			(TrackLength - loop length) to Track Length
			If numLoops is set to zero, it will loop forever.
		To turn looping off, you set this with loop length equal to zero.

	@constant	kSequenceTrackProperty_OffsetTime
		read/write	- MusicTimeStamp
		offset's the track's start time to the specified beat. By default this value is zero.
		
	@constant	kSequenceTrackProperty_MuteStatus
		read/write	- Boolean
		mute state of a track. By default this value is false (not muted)
		
	@constant	kSequenceTrackProperty_SoloStatus
		read/write	- Boolean
		solo state of a track. By default this value is false (not soloed)

	@constant	kSequenceTrackProperty_AutomatedParameters
		read/write	- UInt32
		Determines whether a track is used for automating parameters.
		If set to != 0 the track is used to automate parameter events to an AUNode.
		The track can only contain parameter events and these events are interpreted 
		as points in the automation curve
		
    @constant	kSequenceTrackProperty_TrackLength
		read/write	- MusicTimeStamp
		The time of the last event in the track plus any additional time that is allowed for fading out of ending notes
		or round a loop point to musical bar, etc.
		
		If this is not set, the track length will always be adjusted to the end of the last active event in a track and 
		is adjusted dynamically as events are added or removed.
		
		The property will return the max of the user set track length, or the calculated length
	 	
    @constant	kSequenceTrackProperty_TimeResolution
		read only	- SInt16 (only valid on the Tempo track)
		
		This retrieves the time resolution value for a sequence. This time resolution can be the resolution that
		was contained within the midi file used to construct a sequence. If you want to keep a time resolution
		when writing a new file, you can retrieve this value and then specify it when creating a new file from a sequence.
		
		It has no direct baring on the rendering or notion of time of the sequence itself (just its representation in file
		formats such as MIDI files). By default this is set to either:
			480 if the sequence is created manually
			some_value based on what was in a MIDI file if the sequence was created from a MIDI file		
*/
CF_ENUM(UInt32)
{
	kSequenceTrackProperty_LoopInfo = 0,
	kSequenceTrackProperty_OffsetTime = 1,
	kSequenceTrackProperty_MuteStatus = 2,
	kSequenceTrackProperty_SoloStatus = 3,
	// added in 10.2
	kSequenceTrackProperty_AutomatedParameters = 4,
	// added in 10.3
	kSequenceTrackProperty_TrackLength = 5,
	// added in 10.5
	kSequenceTrackProperty_TimeResolution = 6
};


/*!
	@struct		MusicTrackLoopInfo
	@discussion	Used to control the looping behaviour of a track
*/
typedef struct MusicTrackLoopInfo
{
	MusicTimeStamp		loopDuration;
	SInt32				numberOfLoops;
} MusicTrackLoopInfo;


//=====================================================================================================================
#pragma mark -

//=====================================================================================================================
#pragma mark Music Player
/*! 
	@functiongroup Music Player
*/

/*!
	@function	NewMusicPlayer
	@abstract	Create a new music player
	@discussion	A music player is used to play a sequence back. This call is used to create a player
				When a sequence is to be played by a player, it can play to either an AUGraph, a MIDI Destination or a
				mixture/combination of both.
	@param		outPlayer	the newly created player
*/
extern OSStatus
NewMusicPlayer(			MusicPlayer	__nullable * __nonnull outPlayer)			API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	DisposeMusicPlayer
	@abstract	Dispose a music player
	@param		inPlayer	the player to dispose
*/
extern OSStatus
DisposeMusicPlayer(		MusicPlayer		inPlayer)								API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));


/*!
	@function	MusicPlayerSetSequence
	@abstract	Set the sequence for the player to play
	@discussion A Sequence cannot be set on a player while it is playing. Setting a sequence
				will overide the currently set sequence.
	@param		inPlayer	the player
	@param		inSequence	the sequence for the player to play
*/
extern OSStatus
MusicPlayerSetSequence(	MusicPlayer 	inPlayer,
						MusicSequence __nullable inSequence)					API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicPlayerGetSequence
	@abstract	Get the sequence attached to a player
	@discussion If the player does not have a sequence set, this will return the _NoSequence error
	@param		inPlayer	the player
	@param		outSequence	the sequence currently set on the player
 
*/
extern OSStatus
MusicPlayerGetSequence(	MusicPlayer 	inPlayer,
						MusicSequence __nullable * __nonnull outSequence)		API_AVAILABLE(macos(10.3), ios(5.0), watchos(2.0), tvos(9.0));
								
/*!
	@function	MusicPlayerSetTime
	@abstract	Set the current time on the player
	@discussion The Get and Set Time calls take a specification of time as beats. This positions the player
				to the specified time based on the currently set sequence. No range checking on the time value
				is done. This can be set on a playing player (in which case playing will be resumed from the
				new time).
	@param		inPlayer	the player
	@param		inTime		the new time value
*/
extern OSStatus
MusicPlayerSetTime(		MusicPlayer 	inPlayer,
						MusicTimeStamp 	inTime)									API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicPlayerGetTime
	@abstract	Get the current time of the player
	@discussion The Get and Set Time calls take a specification of time as beats. This retrieves the player's
				current time. If it is playing this time is the time of the player at the time the call was made. 
	@param		inPlayer	the player
	@param		outTime		the current time value
*/
extern OSStatus
MusicPlayerGetTime(		MusicPlayer 	inPlayer,
						MusicTimeStamp	*outTime)
							CA_REALTIME_API
							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicPlayerGetHostTimeForBeats
	@abstract	Returns the host time that will be (or was) played at the specified beat.
	@discussion This call is only valid if the player is playing and will return an error if the player is not playing
				or if the starting position of the player (its "starting beat") was after the specified beat.
				For general translation of beats to time in a sequence, see the MusicSequence calls for beat<->seconds.
				
				The call uses the player's sequence's tempo map to translate a beat time from the starting time and beat
				of the player.
	@param		inPlayer	the player
	@param		inBeats		the specified beat-time value
	@param		outHostTime the corresponding host time
*/
extern OSStatus
MusicPlayerGetHostTimeForBeats(	MusicPlayer 	inPlayer,
								MusicTimeStamp	inBeats,
								UInt64 *		outHostTime)
									CA_REALTIME_API
									API_AVAILABLE(macos(10.2), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicPlayerGetBeatsForHostTime
	@abstract	Returns the beat that will be (or was) played at the specified host time. 
	@discussion This call is only valid if the player is playing and will return an error if the player is not playing
				or if the starting time of the player was after the specified host time.
				For general translation of beats to time in a sequence, see the MusicSequence calls for beat<->seconds.
				
				The call uses the player's sequence's tempo map to retrieve a beat time from the starting and specified host time. 
				
	@param		inPlayer	the player
	@param		inHostTime	the specified host time value
	@param		outBeats	the corresponding beat time
*/
extern OSStatus
MusicPlayerGetBeatsForHostTime(	MusicPlayer 	inPlayer,
								UInt64			inHostTime,
								MusicTimeStamp *outBeats)
									CA_REALTIME_API
									API_AVAILABLE(macos(10.2), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicPlayerPreroll
	@abstract	Prepare the player for playing
	@discussion Allows the player to prepare its state so that starting is has a lower latency. If a player is started without
				being prerolled, the player will pre-roll itself and then start.
	@param		inPlayer	the player
*/
extern OSStatus
MusicPlayerPreroll(		MusicPlayer 	inPlayer)								API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicPlayerStart
	@abstract	Start the player
	@discussion If the player has not been prerolled, it will pre-roll itself and then start.
	@param		inPlayer	the player
*/
extern OSStatus
MusicPlayerStart(		MusicPlayer 	inPlayer)								API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicPlayerStop
	@abstract	Stop the player
	@param		inPlayer	the player
*/
extern OSStatus
MusicPlayerStop(		MusicPlayer 	inPlayer)								API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

// 
/*!
	@function	MusicPlayerIsPlaying
	@abstract	Returns the playing state of the player. "Is it playing?"
	@discussion This call returns a non-zero value in outIsPlaying if the player has been
				started and not stopped. It may have "played" past the events of the attached
				MusicSequence, but it is still considered to be playing (and its time value increasing)
				until it is explicitly stopped
	@param		inPlayer		the player
	@param		outIsPlaying	false if not, true (non-zero) if is playing
*/
extern OSStatus
MusicPlayerIsPlaying(	MusicPlayer 	inPlayer,
						Boolean *		outIsPlaying)
							CA_REALTIME_API
							API_AVAILABLE(macos(10.2), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicPlayerSetPlayRateScalar
	@abstract	Scale the playback rate of the player
	@param		inPlayer	the player
	@param		inScaleRate	a scalar that will be applied to the playback rate. If 2, playback is twice as fast, if
				0.5 it is half as fast. As a scalar, the value must be greater than zero.
*/
extern OSStatus
MusicPlayerSetPlayRateScalar(	MusicPlayer		inPlayer,
								Float64			inScaleRate)
									CA_REALTIME_API
									API_AVAILABLE(macos(10.3), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicPlayerGetPlayRateScalar
	@abstract	Get the playback rate scalar of the player
	@param		inPlayer		the player
	@param		outScaleRate	the current scalar being applied to the player. Default value is 1.0
*/
extern OSStatus
MusicPlayerGetPlayRateScalar(	MusicPlayer		inPlayer,
								Float64 *		outScaleRate)
									CA_REALTIME_API
									API_AVAILABLE(macos(10.3), ios(5.0), watchos(2.0), tvos(9.0));


//=====================================================================================================================
#pragma mark -

//=====================================================================================================================
#pragma mark Music Sequence
/*! 
	@functiongroup Music Sequence
*/
/*!
	@function	NewMusicSequence
	@abstract	Create a new empty sequence
	@discussion	A new music sequence will only have a tempo track (with a default tempo of 120 bpm), 
				and the default type is beat based.

				When a sequence is to be played by a player, it can play to either an AUGraph, a MIDI Destination or a
				mixture/combination of both. See MusicSequenceSetAUGraph and MusicSequenceSetMIDIEndpoint for the generic
				destination assignments. Specific tracks can also be assigned nodes of a graph or a MIDI endpoint as targets
				for the events that they contain; see MusicTrackSetDestNode and MusicTrackSetDestMIDIEndpoint.
				
	@param		outSequence		the new sequence
*/
extern OSStatus
NewMusicSequence(	MusicSequence __nullable * __nonnull outSequence)			API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	DisposeMusicSequence
	@abstract	Dispose the sequence
	@discussion	 A sequence cannot be disposed while a MusicPlayer has it.
	@param		inSequence		the sequence
*/
extern OSStatus
DisposeMusicSequence(		MusicSequence		inSequence)						API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceNewTrack
	@abstract	Add a new (empty) track to the sequence
	@param		inSequence		the sequence
	@param		outTrack		the new track (it is always appended to any existing tracks)
*/
extern OSStatus
MusicSequenceNewTrack(		MusicSequence 		inSequence,
							MusicTrack __nullable * __nonnull outTrack)			API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));
													
/*!
	@function	MusicSequenceDisposeTrack
	@abstract	Remove and dispose a track from a sequence
	@param		inSequence		the sequence
	@param		inTrack			the track to remove and dispose
*/
extern OSStatus
MusicSequenceDisposeTrack(	MusicSequence 		inSequence,
							MusicTrack 			inTrack)						API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceGetTrackCount
	@abstract	The number of tracks in a sequence. 
				The track count and accessors exclude the tempo track (which is treated as a special case)
	@param		inSequence			the sequence
	@param		outNumberOfTracks	the number of tracks
*/
extern OSStatus
MusicSequenceGetTrackCount(	MusicSequence 		inSequence,
							UInt32 				*outNumberOfTracks)
								CA_REALTIME_API
								API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));
										
/*!
	@function	MusicSequenceGetIndTrack
	@abstract	Get a track at the specified index
	@discussion Index is zero based. It will return kAudio_ParamError if index is not in the range: 0 < TrackCount
				The track count and accessors exclude the tempo track (which is treated as a special case)
	@param		inSequence		the sequence
	@param		inTrackIndex	the index
	@param		outTrack		the track at that index
*/
extern OSStatus
MusicSequenceGetIndTrack(	MusicSequence 						inSequence,
							UInt32 								inTrackIndex,
							MusicTrack __nullable * __nonnull	outTrack)
								CA_REALTIME_API
								API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceGetTrackIndex
	@abstract	Get the index for a specific track
	@discussion Index is zero based. It will return an error if the track is not a member of the sequence.
				The track count and accessors exclude the tempo track (which is treated as a special case)
	@param		inSequence		the sequence
	@param		inTrack			the track
	@param		outTrackIndex	the index of the track
*/
extern OSStatus
MusicSequenceGetTrackIndex(	MusicSequence 		inSequence,
							MusicTrack 			inTrack,
							UInt32				*outTrackIndex)
								CA_REALTIME_API
								API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceGetTempoTrack
	@abstract	Get the tempo track of the sequence
	@discussion	Each sequence has a single tempo track. All tempo events are placed into this tempo track (as well
				as other appropriate events (time sig for instance from a MIDI file). The tempo track, once retrieved
				can be edited and iterated upon as any other track. Non-tempo events in a tempo track are ignored.
	@param		inSequence		the sequence
	@param		outTrack		the tempo track of the sequence
*/
extern OSStatus
MusicSequenceGetTempoTrack(	MusicSequence						inSequence,
							MusicTrack __nullable * __nonnull	outTrack)
								CA_REALTIME_API
								API_AVAILABLE(macos(10.1), ios(5.0), watchos(2.0), tvos(9.0));


/*!
	@function	MusicSequenceSetAUGraph
	@abstract	Set the graph to be associated with the sequence
	@discussion	A sequence can be associated with an AUGraph and this graph will be used to render the events as 
				controlled by the sequence when it is played. By default, all of the tracks of a sequence will
				find the first AUNode that is an instance of an Apple MusicDevice audio unit (see MusicSequenceGetAUGraph).
				Specific nodes of the graph can be targeted for different tracks (see MusicTrackSetDestNode).  To render a
 				multi-track GM MIDI sequence on iOS, create a custom graph with a MIDISynth audio unit as the MusicDevice.
 				If inGraph is set to NULL, the sequence will reset to use the default graph.
	@param		inSequence		the sequence
	@param		inGraph			the graph
*/
extern OSStatus
MusicSequenceSetAUGraph(	MusicSequence 	   inSequence,
							AUGraph __nullable inGraph)							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));


/*!
	@function	MusicSequenceGetAUGraph
	@abstract	Gets the graph currently associated with a sequence
	@discussion	By default if no graph is assigned to a sequence then the sequence will create a default graph. 
				This default graph contains a MusicDevice and a DynamicsProcessor and all tracks will be targeted
				to the MusicDevice.  On macOS, this MusicDevice is an instance of a software synthesizer that is 
				compatible with the GM and GS MIDI standards.  On iOS, it is an instance of a monotimbral software 
				synthesizer designed to render events from a single MIDI channel.  To render multi-track GM MIDI
 				sequences on iOS, create a custom graph with a MIDISynth audio unit as the MusicDevice.
				
				This call will thus either return the graph as set by the user, or this default graph.
	@param		inSequence		the sequence
	@param		outGraph		the graph
*/
extern OSStatus
MusicSequenceGetAUGraph(	MusicSequence 					inSequence,
							AUGraph __nullable * __nonnull	outGraph)			API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceSetMIDIEndpoint
	@abstract	Makes the target of all of the tracks in the sequence a MIDI endpoint
	@discussion	This is a convenience function, and is equivalent to iterating through all of the tracks in a sequence
				and targeting each track to the MIDI endpoint
				
	@param		inSequence		the sequence
	@param		inEndpoint		the MIDI endpoint
*/
extern OSStatus
MusicSequenceSetMIDIEndpoint(	MusicSequence 	inSequence,
								MIDIEndpointRef	inEndpoint)						API_AVAILABLE(macos(10.1), ios(5.0), tvos(12.0)) __WATCHOS_PROHIBITED;
	
/*!
	@function	MusicSequenceSetSequenceType
	@abstract	Set the sequence type (the default is beats)
	@discussion
				These two calls allow you to get and set a MusicSequence type; specifying
					kMusicSequenceType_Beats		= 'beat',
					kMusicSequenceType_Seconds		= 'secs',
					kMusicSequenceType_Samples		= 'samp'

				The sequence type can be set to beats at any time. The sequence type can only be set to 
				seconds or samples if there are NO tempo events already in the sequence.
				
				For beats - it can have as many tempo events as you want
				For Samples and Seconds - you should add a single tempo event after setting the type
					Samples - the tempo is the desired sample rate - e.g. 44100 and each "beat" in the sequence will be
						interpreted as a sample count at that sample rate (so beat == 44100 is a second)
					Seconds - the tempo should be set to 60 - a beat is a second.

				Beats is the default (and is the behaviour on pre 10.5 systems)

				A meta event of interest for Seconds based MIDI files is the SMPTE Offset meta event - stored in the tempo track.
				The sequence doesn't do anything with this event (except store/write it)	
	@param		inSequence	the sequence
	@param		inType		the sequence type
*/
extern OSStatus
MusicSequenceSetSequenceType(	MusicSequence		inSequence,
							MusicSequenceType		inType)						API_AVAILABLE(macos(10.5), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceGetSequenceType
	@abstract	Get the sequence type
	@discussion	See SetSequence for a full description
	@param		inSequence		the sequence
	@param		outType			the type
*/
extern OSStatus
MusicSequenceGetSequenceType(	MusicSequence		inSequence,
							MusicSequenceType * 	outType)					API_AVAILABLE(macos(10.5), ios(5.0), watchos(2.0), tvos(9.0));


/*!
	@function	MusicSequenceFileLoad
	@abstract	Load the data contained within the referenced file to the sequence
	@discussion	This function will parse the file referenced by the URL and add the events to the sequence.
	@param		inSequence		the sequence
	@param		inFileRef		a file:// URL that references a file
	@param		inFileTypeHint	provides a hint to the sequence on the file type being imported. Can be zero in many cases.
	@param		inFlags			flags that can control how the data is parsed in the file and laid out in the tracks
								that will be created and added to the sequence in this operation
*/
extern OSStatus
MusicSequenceFileLoad(MusicSequence					inSequence,
						CFURLRef					inFileRef,
						MusicSequenceFileTypeID		inFileTypeHint,
						MusicSequenceLoadFlags		inFlags)					API_AVAILABLE(macos(10.5), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceFileLoadData
	@abstract	Load the data to the sequence
	@discussion	This function will parse the data and add the events to the sequence. The data provided needs to 
				be of a particular file type as specified by the fileTypeHint.
	@param		inSequence		the sequence
	@param		inData			the contents of a valid file loaded into a CFData object
	@param		inFileTypeHint	provides a hint to the sequence on the file type being imported. Can be zero in many cases.
	@param		inFlags			flags that can control how the data is parsed in the file and laid out in the tracks
								that will be created and added to the sequence in this operation
*/
extern OSStatus
MusicSequenceFileLoadData(MusicSequence				inSequence,
						CFDataRef					inData,
						MusicSequenceFileTypeID		inFileTypeHint,
						MusicSequenceLoadFlags		inFlags)					API_AVAILABLE(macos(10.5), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceSetSMPTEResolution
	@abstract	Helper function to establish the SMPTE based MIDI file resolution for the specified ticks
	@discussion SMPTE resolution helpers for dealing with the interpretation and creation of
				tick values for standard MIDI files (see MusicSequenceFileCreate)
	@param		fps		the frames per second
	@param		ticks	the ticks per quarter note
	@result		the resolution that can be used when creating a MIDI file derived from the two parameters
*/
CF_INLINE SInt16 
MusicSequenceSetSMPTEResolution (SignedByte fps, Byte ticks) 
{
	SInt8 res8 = fps < 0 ? fps : -fps;
	SInt16 res = (SInt16) (res8 << 8);
	res += ticks;
	return res;
}

/*!
	@function	MusicSequenceGetSMPTEResolution
	@abstract	Helper function to get the fps and ticks from their representation in a SMPTE based MIDI file
	@discussion SMPTE resolution helpers for dealing with the interpretation and creation of
				tick values for standard MIDI files (see MusicSequenceFileCreate)
	@param		inRes	the resolution from a MIDI file
	@param		fps		the frames per second
	@param		ticks	the ticks per quarter note
*/
CF_INLINE void 
MusicSequenceGetSMPTEResolution (SInt16 inRes, SignedByte * fps, Byte * ticks) 
{
	*fps = (SignedByte)((0xFF00 & inRes) >> 8);
	*ticks = 0x7F & inRes;			
}

/*!
	@function	MusicSequenceFileCreate
	@abstract	Create a file from a sequence
	@discussion	This function can be (and is most commonly) used to create a MIDI file from the events in a sequence.
				Only MIDI based events are used when creating the MIDI file. MIDI files are normally beat based, but
				can also have a SMPTE (or real-time rather than beat time) representation.
				
				inResolution is relationship between "tick" and quarter note for saving to Standard MIDI File
					- pass in zero to use default - this will be the value that is currently set on the tempo track
					- see the comments for the set track property's time resolution
				
				The different Sequence types determine the kinds of files that can be created:

				Beats
					When saving a MIDI file, it saves a beats (PPQ) based axis

				Seconds
					When saving a MIDI file, it will save it as a SMPTE resolution - so you should specify this resolution
					when creating the MIDI file. 
					If zero is specified, 25 fps and 40 ticks/frame is used (a time scale of a millisecond)
			
				Samples
					You cannot save to a MIDI file with this sequence type

				The complete meaning of the 16-bit "division" field in a MIDI File's MThd chunk.

				If it is positive, then a tick represents 1/D quarter notes.

				If it negative:

				bits 14-8 are a signed 7-bit number representing the SMPTE format:
					-24, -25, -29 (drop), -30
				bits 7-0 represents the number of ticks per SMPTE frame
					typical values: 4, 10, 80, 100

				You can obtain millisecond resolution by specifying 25 frames/sec and 40 divisions/frame.

				30 fps with 80 bits (ticks) per frame: 0xE250  ((char)0xE2 == -30)

	@param		inSequence		the sequence
	@param		inFileRef		the location of the file to create
	@param		inFileType		the type of file to create
	@param		inFlags			flags to control the file creation
	@param		inResolution	the resolution (depending on file type and sequence type)
*/
extern OSStatus
MusicSequenceFileCreate (MusicSequence				inSequence,
						CFURLRef					inFileRef,
						MusicSequenceFileTypeID		inFileType,
						MusicSequenceFileFlags		inFlags,
						SInt16						inResolution)				API_AVAILABLE(macos(10.5), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceFileCreateData
	@abstract	Create a data object from a sequence
	@discussion	The same basic parameters apply to this as with the MusicSequenceFileCreate function. The difference
				being that that function will create a file on disk, whereas this one will create a CFData object
				that is a file in memory. The CFData object should be released by the caller.
	@param		inSequence		the sequence
	@param		inFileType		the type of file to create
	@param		inFlags			flags to control the file creation
	@param		inResolution	the resolution (depending on file type and sequence type)
	@param		outData			the resulting data object
*/
extern OSStatus
MusicSequenceFileCreateData (MusicSequence					inSequence,
						MusicSequenceFileTypeID				inFileType,
						MusicSequenceFileFlags				inFlags,
						SInt16								inResolution,
						CFDataRef __nullable * __nonnull	outData)			API_AVAILABLE(macos(10.5), ios(5.0), watchos(2.0), tvos(9.0));


/*!
	@function	MusicSequenceReverse
	@abstract	Reverse in time all events in a sequence, including the tempo events
	@param		inSequence		the sequence
*/
extern OSStatus
MusicSequenceReverse(		MusicSequence	inSequence)							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceGetSecondsForBeats
	@abstract	Returns a seconds value that would correspond to the supplied beats
	@discussion	Uses the sequence's tempo events 
	@param		inSequence		the sequence
	@param		inBeats			the beats
	@param		outSeconds		the seconds (time from 0 beat)
*/
extern OSStatus
MusicSequenceGetSecondsForBeats(	MusicSequence		inSequence,
									MusicTimeStamp		inBeats,
									Float64 *			outSeconds)
										CA_REALTIME_API
										API_AVAILABLE(macos(10.2), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceGetBeatsForSeconds
	@abstract	Returns a beat value that would correspond to the supplied seconds from zero.
	@discussion	Uses the sequence's tempo events 
	@param		inSequence		the sequence
	@param		inSeconds		the seconds
	@param		outBeats		the corresponding beat
*/
extern OSStatus
MusicSequenceGetBeatsForSeconds(	MusicSequence		inSequence,
									Float64				inSeconds,
									MusicTimeStamp *	outBeats)
										CA_REALTIME_API
										API_AVAILABLE(macos(10.2), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceSetUserCallback
	@abstract	Establish a user callback for a sequence
	@discussion	This call is used to register (or remove if inCallback is NULL) a callback
				that the MusicSequence will call for ANY UserEvents that are added to any of the
				tracks of the sequence.
				
				If there is a callback registered, then UserEvents will be chased when
				MusicPlayerSetTime is called. In that case the inStartSliceBeat and inEndSliceBeat
				will both be the same value and will be the beat that the player is chasing too.
				
				In normal cases, where the sequence data is being scheduled for playback, the
				following will apply:
					inStartSliceBeat <= inEventTime < inEndSliceBeat
				
				The only exception to this is if the track that owns the MusicEvent is looping.
				In this case the start beat will still be less than the end beat (so your callback
				can still determine that it is playing, and what beats are currently being scheduled),
				however, the inEventTime will be the original time-stamped time of the user event. 
	@param		inSequence		the sequence
	@param		inCallback		the callback
	@param		inClientData	client (user supplied) data provided back to the callback when it is called by the sequence
*/
extern OSStatus
MusicSequenceSetUserCallback(	MusicSequence							inSequence,
								MusicSequenceUserCallback __nullable	inCallback,
								void * __nullable						inClientData)
									CA_REALTIME_API
									API_AVAILABLE(macos(10.3), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceBeatsToBarBeatTime
	@abstract	Convenience function to format a sequence's beat time to its bar-beat time
	@discussion	The sequence's tempo track Time Sig events are used to
				to calculate the bar-beat representation. If there are no Time Sig events added to the sequence
				4/4 is assumed. A Time Sig event is a MIDI Meta Event as specified for MIDI files.
	@param		inSequence		the sequence
	@param		inBeats			the beat which should be represented by the bar-beat
	@param		inSubbeatDivisor	The denominator of the fractional number of beats.
	@param		outBarBeatTime	the formatted bar/beat time
*/
extern OSStatus
MusicSequenceBeatsToBarBeatTime(MusicSequence				inSequence,
								MusicTimeStamp				inBeats,
								UInt32						inSubbeatDivisor,
								CABarBeatTime *				outBarBeatTime)
									CA_REALTIME_API
									API_AVAILABLE(macos(10.5), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceBarBeatTimeToBeats
	@abstract	Convenience function to format a bar-beat time to a sequence's beat time
	@discussion	The sequence's tempo track Time Sig events are used to
				to calculate the bar-beat representation. If there are no Time Sig events added to the sequence
				4/4 is assumed. A Time Sig event is a MIDI Meta Event as specified for MIDI files.
	@param		inSequence		the sequence
	@param		inBarBeatTime	the bar-beat time
	@param		outBeats		the sequence's beat time for that bar-beat time
*/
extern OSStatus
MusicSequenceBarBeatTimeToBeats(MusicSequence				inSequence,
								const CABarBeatTime *		inBarBeatTime,
								MusicTimeStamp *			outBeats)
									CA_REALTIME_API
									API_AVAILABLE(macos(10.5), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicSequenceGetInfoDictionary
	@abstract	Returns a dictionary containing meta-data derived from a sequence
	@discussion	The dictionary can contain one or more of the kAFInfoDictionary_* 
				keys specified in <AudioToolbox/AudioFile.h>
				
				The caller should release the returned dictionary. If the call fails it will return NULL

	@param		inSequence		the sequence
	@result		a CFDictionary or NULL if the call fails.
*/

CF_IMPLICIT_BRIDGING_ENABLED

extern CFDictionaryRef
MusicSequenceGetInfoDictionary(	MusicSequence				inSequence)			API_AVAILABLE(macos(10.5), ios(5.0), watchos(2.0), tvos(9.0));

CF_IMPLICIT_BRIDGING_DISABLED

//=====================================================================================================================
#pragma mark -

//=====================================================================================================================
#pragma mark Music Track
/*! 
	@functiongroup Music Track
*/

/*!
	@function	MusicTrackGetSequence
	@abstract	Gets the sequence which the track is a member of
	@param		inTrack		the track
	@param		outSequence the track's sequence
*/
extern OSStatus
MusicTrackGetSequence(	MusicTrack 			inTrack,
						MusicSequence __nullable * __nonnull outSequence)
							CA_REALTIME_API
							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackSetDestNode
	@abstract	Sets the track's target to the specified AUNode
	@discussion	The node must be a member of the graph that the track's sequence is using. When played, the track
				will send all of its events to that node.
	@param		inTrack		the track
	@param		inNode		the new node
*/
extern OSStatus
MusicTrackSetDestNode(	MusicTrack 			inTrack,
						AUNode				inNode)								API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackSetDestMIDIEndpoint
	@abstract	Sets the track's target to the specified MIDI endpoint
	@discussion	When played, the track will send all of its events to the specified MIDI Endpoint.
	@param		inTrack		the track
	@param		inEndpoint	the new MIDI endpoint
*/
extern OSStatus
MusicTrackSetDestMIDIEndpoint(	MusicTrack			inTrack,
								MIDIEndpointRef		inEndpoint)					API_AVAILABLE(macos(10.1), ios(5.0), tvos(12.0)) __WATCHOS_PROHIBITED;
	
/*!
	@function	MusicTrackGetDestNode
	@abstract	Gets the track's target if it is an AUNode
	@discussion	Returns kAudioToolboxErr_IllegalTrackDestination if the track's target is a MIDIEndpointRef 
				and NOT an AUNode
	@param		inTrack		the track
	@param		outNode		the node target for the track
*/
extern OSStatus
MusicTrackGetDestNode(			MusicTrack 			inTrack,
								AUNode *			outNode)					API_AVAILABLE(macos(10.1), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackGetDestMIDIEndpoint
	@abstract	Gets the track's target if it is a MIDI Endpoint
	@discussion	Returns kAudioToolboxErr_IllegalTrackDestination if the track's target is an AUNode 
				and NOT a MIDI Endpoint
	@param		inTrack		the track
	@param		outEndpoint	the MIDI Endpoint target for the track
*/
extern OSStatus
MusicTrackGetDestMIDIEndpoint(	MusicTrack			inTrack,
								MIDIEndpointRef	*	outEndpoint)				API_AVAILABLE(macos(10.1), ios(5.0), tvos(12.0)) __WATCHOS_PROHIBITED;
	
/*!
	@function	MusicTrackSetProperty
	@abstract	Sets the specified property value
	@discussion Property values are always get and set by reference
	@param		inTrack			the track
	@param		inPropertyID	the property ID
	@param		inData			the new property value
	@param		inLength		the size of the property value being set
*/
extern OSStatus
MusicTrackSetProperty(	MusicTrack 			inTrack,
						UInt32 				inPropertyID,
						void				*inData,
						UInt32				inLength)							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackGetProperty
	@abstract	Gets the specified property value
	@discussion	If outData is NULL, then the size of the data will be passed back in ioLength
				This allows the client to allocate a buffer of the correct size (useful for variable
				length properties -- currently all properties have fixed size)
				Property values are always get and set by reference
	@param		inTrack			the track
	@param		inPropertyID	the property ID
	@param		outData			if not NULL, points to data of size ioLength
	@param		ioLength		on input the available size of outData, on output the size of the valid data that outData
								will then point too.
*/
extern OSStatus
MusicTrackGetProperty(	MusicTrack 			inTrack,
						UInt32 				inPropertyID,
						void				*outData,
						UInt32				*ioLength)							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Editing operations on sequence tracks

/*!
	@function	MusicTrackMoveEvents
	@abstract	Move events in a track
	@discussion	Moves all of the events in the specified time range by the moveTime. MoveTime maybe negative to 
				move events backwards (towards zero).
				
				All time ranges are [starttime < endtime]
				
	@param		inTrack			the track
	@param		inStartTime		the start time for the range of events
	@param		inEndTime		the end time up to which will form the range of the events to move
	@param		inMoveTime		amount of beats to move the selected events.
*/
extern OSStatus
MusicTrackMoveEvents(	MusicTrack 			inTrack,
						MusicTimeStamp		inStartTime,
						MusicTimeStamp		inEndTime,
						MusicTimeStamp		inMoveTime)							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackClear
	@abstract	Removes all events within the specified range
	@discussion	All time ranges are [starttime < endtime]
	@param		inTrack		the track
	@param		inStartTime	the start time for the range of events
	@param		inEndTime	the end time up to which will form the range of the events to clear
*/
extern OSStatus
MusicTrackClear(		MusicTrack 			inTrack,
						MusicTimeStamp		inStartTime,
						MusicTimeStamp		inEndTime)							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackCut
	@abstract	Removes all the events within the specified range
	@discussion	Events that fall past the specified range will be moved back by the specified range time.
				
				All time ranges are [starttime < endtime]
				
	@param		inTrack		the track
	@param		inStartTime	the start time for the range of events
	@param		inEndTime	the end time up to which will form the range of the events to cut out
*/
extern OSStatus
MusicTrackCut(			MusicTrack 			inTrack,
						MusicTimeStamp		inStartTime,
						MusicTimeStamp		inEndTime)							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackCopyInsert
	@abstract	Copies events from one track and inserts them into another
	@discussion	Copies all of the events with the specified time range of the source track. It then inserts
				those events into the destination track. All events at and after inDestInsertTime in inDestTrack 
				are moved forward by the range's duration
				
				All time ranges are [starttime < endtime]
				
	@param		inSourceTrack		the source track
	@param		inSourceStartTime	the start time for the range of events
	@param		inSourceEndTime		the end time up to which will form the range of the events to copy from the source track
	@param		inDestTrack			the destination track to copy too
	@param		inDestInsertTime	the time at which the copied events will be inserted.
*/
extern OSStatus
MusicTrackCopyInsert(	MusicTrack 			inSourceTrack,
						MusicTimeStamp		inSourceStartTime,
						MusicTimeStamp		inSourceEndTime,
						MusicTrack 			inDestTrack,
						MusicTimeStamp		inDestInsertTime)					API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackMerge
	@abstract	Copies events from one track and merges them into another
	@discussion	Copies all of the events with the specified time range of the source track. It then merges
				those events into the destination track starting at inDestInsertTime.
				
				All time ranges are [starttime < endtime]
				
	@param		inSourceTrack		the source track
	@param		inSourceStartTime	the start time for the range of events
	@param		inSourceEndTime		the end time up to which will form the range of the events to copy from the source track
	@param		inDestTrack			the destination track to copy too
	@param		inDestInsertTime	the time at which the copied events will be merged.
*/
extern OSStatus
MusicTrackMerge(		MusicTrack 			inSourceTrack,
						MusicTimeStamp		inSourceStartTime,
						MusicTimeStamp		inSourceEndTime,
						MusicTrack 			inDestTrack,
						MusicTimeStamp		inDestInsertTime)					API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));


//=====================================================================================================================
#pragma mark -

//=====================================================================================================================
#pragma mark Music Events
/*! 
	@functiongroup Music Events
*/
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// adding time-stamped events to the track

/*!
	@function	MusicTrackNewMIDINoteEvent
	@abstract	Adds a MIDINoteMessage event to a track
	@discussion	The event is added at the specified time stamp. The time stamp should not be less than zero.
	@param		inTrack			the track
	@param		inTimeStamp		the time stamp
	@param		inMessage		the event
*/
extern OSStatus
MusicTrackNewMIDINoteEvent(			MusicTrack 					inTrack,
									MusicTimeStamp				inTimeStamp,
									const MIDINoteMessage *		inMessage)		API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackNewMIDIChannelEvent
	@abstract	Adds a MIDIChannelMessage event to a track
	@discussion	The event is added at the specified time stamp. The time stamp should not be less than zero.
	@param		inTrack			the track
	@param		inTimeStamp		the time stamp
	@param		inMessage		the event
*/
extern OSStatus
MusicTrackNewMIDIChannelEvent(		MusicTrack 					inTrack,
									MusicTimeStamp				inTimeStamp,
									const MIDIChannelMessage *	inMessage)		API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackNewMIDIRawDataEvent
	@abstract	Adds a MIDIRawData event to a track
	@discussion	The event is added at the specified time stamp. The time stamp should not be less than zero.
	@param		inTrack			the track
	@param		inTimeStamp		the time stamp
	@param		inRawData		the event
*/
extern OSStatus
MusicTrackNewMIDIRawDataEvent(		MusicTrack 					inTrack,
									MusicTimeStamp				inTimeStamp,
									const MIDIRawData *			inRawData)		API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackNewExtendedNoteEvent
	@abstract	Adds a ExtendedNoteOnEvent to a track
	@discussion	The event is added at the specified time stamp. The time stamp should not be less than zero.
	@param		inTrack			the track
	@param		inTimeStamp		the time stamp
	@param		inInfo			the event
*/
extern OSStatus
MusicTrackNewExtendedNoteEvent(		MusicTrack 					inTrack,
									MusicTimeStamp				inTimeStamp,
									const ExtendedNoteOnEvent	*inInfo)		API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));
										
/*!
	@function	MusicTrackNewParameterEvent
	@abstract	Adds a ParameterEvent to a track
	@discussion	The event is added at the specified time stamp. The time stamp should not be less than zero.
	@param		inTrack			the track
	@param		inTimeStamp		the time stamp
	@param		inInfo			the event
*/
extern OSStatus
MusicTrackNewParameterEvent(		MusicTrack 					inTrack,
									MusicTimeStamp				inTimeStamp,
									const ParameterEvent *		inInfo)			API_AVAILABLE(macos(10.2), ios(5.0), watchos(2.0), tvos(9.0));
										
/*!
	@function	MusicTrackNewExtendedTempoEvent
	@abstract	Adds a tempo event to a track
	@discussion	The event is added at the specified time stamp. The time stamp should not be less than zero.
	@param		inTrack			the track
	@param		inTimeStamp		the time stamp
	@param		inBPM			the event
*/
extern OSStatus
MusicTrackNewExtendedTempoEvent(	MusicTrack 					inTrack,
									MusicTimeStamp				inTimeStamp,
									Float64						inBPM)			API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));
										
/*!
	@function	MusicTrackNewMetaEvent
	@abstract	Adds a MIDIMetaEvent to a track
	@discussion	The event is added at the specified time stamp. The time stamp should not be less than zero.
	@param		inTrack			the track
	@param		inTimeStamp		the time stamp
	@param		inMetaEvent		the event
*/
extern OSStatus
MusicTrackNewMetaEvent(				MusicTrack 					inTrack,
									MusicTimeStamp				inTimeStamp,
									const MIDIMetaEvent *		inMetaEvent)	API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));
										
/*!
	@function	MusicEventUserData
	@abstract	Adds a MusicEventUserData event to a track
	@discussion	The event is added at the specified time stamp. The time stamp should not be less than zero.
	@param		inTrack			the track
	@param		inTimeStamp		the time stamp
	@param		inUserData		the event
*/
extern OSStatus
MusicTrackNewUserEvent(				MusicTrack 					inTrack,
									MusicTimeStamp				inTimeStamp,
									const MusicEventUserData *	inUserData)		API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicTrackNewAUPresetEvent
	@abstract	Adds a AUPresetEvent to a track
	@discussion	The event is added at the specified time stamp. The time stamp should not be less than zero.
	@param		inTrack			the track
	@param		inTimeStamp		the time stamp
	@param		inPresetEvent	the event
*/
extern OSStatus
MusicTrackNewAUPresetEvent(			MusicTrack 					inTrack,
									MusicTimeStamp			 	inTimeStamp,
									const AUPresetEvent *		inPresetEvent)	API_AVAILABLE(macos(10.3), ios(5.0), watchos(2.0), tvos(9.0));



//=====================================================================================================================
#pragma mark -

//=====================================================================================================================
#pragma mark Music Event Iterator
/*! 
	@functiongroup Music Event Iterator
*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// actual event representation and manipulation within a track....
//
// Here we need to be very careful to be able to deal with both SMF type of MIDI events, and
// also be upwardly compatible with an extended MPEG4-SA like paradigm!
//
// The solution is to hide the internal event representation from the client
// and allow access to the events through accessor functions.  The user, in this way, can
// examine and create standard events, or any user-defined event.


/*!
	@function	NewMusicEventIterator
	@abstract	Creates an iterator to iterator over a track's events
	@discussion	The iterator should be considered invalid if a track is edited. In that case you should create a new
				iterator and seek it to the desired position.
				
	@param		inTrack			the track upon which to iterate
	@param		outIterator		the new iterator
*/
extern OSStatus
NewMusicEventIterator(		MusicTrack 									inTrack,
							MusicEventIterator __nullable * __nonnull	outIterator)
							CA_REALTIME_API
							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));
													
/*!
	@function	DisposeMusicEventIterator
	@abstract	Dispose an iterator
	@param		inIterator		the iterator
*/
extern OSStatus
DisposeMusicEventIterator(			MusicEventIterator	inIterator)
							CA_REALTIME_API
							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicEventIteratorSeek
	@abstract	Move the iterator to an event at the specified time
	@discussion If there is no event at the specified time, the iterator will point to the first event after
				that time.
				By specifying kMusicTimeStamp_EndOfTrack you will position the iterator to the end of track
				(which is pointing to the space just AFTER the last event). You can use MusicEventIteratorPreviousEvent 
				to backup to the last event.
				By specifying 0, you will position the iterator at the first event
	@param		inIterator		the iterator
	@param		inTimeStamp		the time stamp to seek too
*/
extern OSStatus
MusicEventIteratorSeek(				MusicEventIterator 	inIterator,
									MusicTimeStamp 		inTimeStamp)
										CA_REALTIME_API
										API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicEventIteratorNextEvent
	@abstract	Move the iterator to the next event
	@discussion If the iterator was at the last event, then it will move past the last event and will no longer point
				to an event. You can use check MusicEventIteratorHasCurrentEvent to see if there is an event at the 
				iterator's current position. See also MusicEventIteratorHasNextEvent.
				
				Typically this call is used to move the iterator forwards through the track's events.
	@param		inIterator		the iterator
*/
extern OSStatus
MusicEventIteratorNextEvent(		MusicEventIterator 	inIterator)
	CA_REALTIME_API
	API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicEventIteratorPreviousEvent
	@abstract	Move the iterator to the previous event
	@discussion If the iterator was at the first event, then it will leave the iterator unchanged and return an error. 
				See also MusicEventIteratorHasPreviousEvent

				Typically this call is used to move the iterator backwards through the track's events.
	@param		inIterator		the iterator
*/
extern OSStatus
MusicEventIteratorPreviousEvent(	MusicEventIterator 	inIterator)
	CA_REALTIME_API
	API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicEventIteratorGetEventInfo
	@abstract	Retrieves the event data at the iterator.
	@discussion Retrieves the event and other information from the iterator's current position.
	
				If you do not want specific information (eg, the time stamp) pass in NULL for that parameter.
				
	@param		inIterator		the iterator
	@param		outTimeStamp	the time stamp of the event
	@param		outEventType	one of kMusicEventType_XXX that indicates what kind of event type the iterator
								is currently pointing too
	@param		outEventData	a reference to the event data. The type of data is described by the eventType. This data
								is read only and should not be edited in place.
	@param		outEventDataSize	the size of the data referenced by outEventData
*/
extern OSStatus
MusicEventIteratorGetEventInfo(		MusicEventIterator 		inIterator,
									MusicTimeStamp *		outTimeStamp,
									MusicEventType *		outEventType,
									const void * __nullable * __nonnull outEventData,
									UInt32 *				outEventDataSize)
										CA_REALTIME_API
										API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));
	
/*!
	@function	MusicEventIteratorSetEventInfo
	@abstract	Changes the type or value of an event
	@discussion Allows you to change either the event type, or the values of the event data, that the iterator is 
				currently pointing too. You cannot change the event's time (to do that you should use 
				MusicEventIteratorSetEventTime).
				
	@param		inIterator		the iterator
	@param		inEventType		the new (or existing) type of the event you are changing
	@param		inEventData		the new event data. The size and type of this event data must match the inEventType
*/
extern OSStatus
MusicEventIteratorSetEventInfo(		MusicEventIterator 		inIterator,
									MusicEventType			inEventType,
									const void *			inEventData)		API_AVAILABLE(macos(10.2), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicEventIteratorSetEventTime
	@abstract	Set a new time for an event
	@discussion The iterator will still be pointing to the same event, but as the event will have moved, 
				it may or may not have a next or previous event now (depending of course on the time
				you moved it to).
				
	@param		inIterator		the iterator
	@param		inTimeStamp		the new time stamp of the event
*/
extern OSStatus
MusicEventIteratorSetEventTime(		MusicEventIterator 		inIterator,
									MusicTimeStamp			inTimeStamp)		API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicEventIteratorDeleteEvent
	@abstract	Deletes the event pointed to by the iterator
	@discussion The iterator will reference the next event after the event has been deleted.
				
	@param		inIterator		the iterator
*/
extern OSStatus
MusicEventIteratorDeleteEvent(		MusicEventIterator	 	inIterator)			API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicEventIteratorHasPreviousEvent
	@abstract	Does the track have an event previous to the event the iterator is pointing to?
	@discussion To use the iterator going backwards through a track:
					iter = New Iterator (points at first event)
					MusicEventIteratorSeek (iter, kMusicTimeStamp_EndOfTrack) // will point it past the last event
					bool hasPreviousEvent;
					MusicEventIteratorHasPreviousEvent (iter, &hasPreviousEvent)
					while (hasPreviousEvent) {
						MusicEventIteratorPreviousEvent (iter)
						// 	do work... MusicEventIteratorGetEventInfo (iter, ...
						
						MusicEventIteratorHasPreviousEvent (iter, &hasPreviousEvent);
					}				
	@param		inIterator		the iterator
	@param		outHasPrevEvent	true if there is a previous event, false if not
*/
extern OSStatus
MusicEventIteratorHasPreviousEvent(	MusicEventIterator 	inIterator,
									Boolean	*			outHasPrevEvent)
										CA_REALTIME_API
										API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicEventIteratorHasNextEvent
	@abstract	Does the track have an event past the event the iterator is pointing too?
	@discussion To use the iterator going forwards through a track:
					iter = New Iterator (points at first event)
					bool hasCurrentEvent;
					MusicEventIteratorHasCurrentEvent(iter, &hasCurrentEvent);
					while (hasCurrentEvent) {
						// do work... MusicEventIteratorGetEventInfo (iter, ...
						
						MusicEventIteratorNextEvent (iter)
						MusicEventIteratorHasCurrentEvent(iter, &hasCurrentEvent);
					}
				
	@param		inIterator		the iterator
	@param		outHasNextEvent	true if there is a next event, false if not
*/
extern OSStatus
MusicEventIteratorHasNextEvent(		MusicEventIterator	inIterator,
									Boolean	*			outHasNextEvent)
										CA_REALTIME_API
										API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicEventIteratorHasCurrentEvent
	@abstract	Is there an event at the iterator's current position?
	@param		inIterator		the iterator
	@param		outHasCurEvent	true if there is an event, false if not
*/
extern OSStatus
MusicEventIteratorHasCurrentEvent(	MusicEventIterator	inIterator,
									Boolean	*			outHasCurEvent)
										CA_REALTIME_API
										API_AVAILABLE(macos(10.2), ios(5.0), watchos(2.0), tvos(9.0));



//=====================================================================================================================
#pragma mark -

#pragma mark -- Deprecated
// standard MIDI files (SMF, and RMF)

// MusicSequenceLoadSMF() also intelligently parses an RMID file to extract SMF part
#if !TARGET_RT_64_BIT
extern OSStatus
MusicSequenceLoadSMFData(	MusicSequence	inSequence,
							CFDataRef		inData)					API_DEPRECATED("no longer supported", macos(10.2, 10.5)) API_UNAVAILABLE(ios, watchos, tvos);
#endif // !TARGET_RT_64_BIT


// passing a value of zero for the flags makes this call equivalent to MusicSequenceLoadSMFData
// a kAudio_ParamError is returned if the sequence has ANY data in it and the flags value is != 0
// This will create a sequence with the first tracks containing MIDI Channel data
// IF the MIDI file had Meta events or SysEx data, then the last track in the sequence
// will contain that data.
extern OSStatus
MusicSequenceLoadSMFWithFlags(		MusicSequence			inSequence,
									const struct FSRef *			inFileRef,
									MusicSequenceLoadFlags	inFlags)	API_DEPRECATED("no longer supported", macos(10.3, 10.5)) API_UNAVAILABLE(ios, watchos, tvos);

extern OSStatus
MusicSequenceLoadSMFDataWithFlags(	MusicSequence	inSequence,
									CFDataRef				inData,
									MusicSequenceLoadFlags	inFlags)	API_DEPRECATED("no longer supported", macos(10.3, 10.5)) API_UNAVAILABLE(ios, watchos, tvos);
// inFlags must be zero
extern OSStatus
MusicSequenceSaveMIDIFile(	MusicSequence	inSequence,
							const struct FSRef *inParentDirectory,
							CFStringRef		inFileName,
							UInt16			inResolution,
							UInt32			inFlags)				API_DEPRECATED("no longer supported", macos(10.4, 10.5)) API_UNAVAILABLE(ios, watchos, tvos);

extern OSStatus
MusicSequenceSaveSMFData(	MusicSequence	inSequence,
							CFDataRef __nullable * __nonnull outData,
							UInt16			inResolution)			API_DEPRECATED("no longer supported", macos(10.2, 10.5)) API_UNAVAILABLE(ios, watchos, tvos);

extern OSStatus
NewMusicTrackFrom(		MusicTrack			inSourceTrack,
						MusicTimeStamp		inSourceStartTime,
						MusicTimeStamp		inSourceEndTime,
						MusicTrack __nullable * __nonnull outNewTrack)	API_DEPRECATED("no longer supported", macos(10.0, 10.6)) API_UNAVAILABLE(ios, watchos, tvos);

#if !TARGET_OS_IPHONE
enum {
	kMusicEventType_ExtendedControl			= 2
};

typedef struct ExtendedControlEvent
{
	MusicDeviceGroupID			groupID;
	AudioUnitParameterID		controlID;
	AudioUnitParameterValue		value;
} ExtendedControlEvent;

extern OSStatus
MusicTrackNewExtendedControlEvent(	MusicTrack 					inTrack,
									MusicTimeStamp				inTimeStamp,
									const ExtendedControlEvent	*inInfo)		
																	API_DEPRECATED("no longer supported", macos(10.0, 10.7)) API_UNAVAILABLE(ios, watchos, tvos);
#endif

#if defined(__cplusplus)
}
#endif

CF_ASSUME_NONNULL_END

#pragma clang diagnostic pop

#endif // AudioToolbox_MusicPlayer_h
