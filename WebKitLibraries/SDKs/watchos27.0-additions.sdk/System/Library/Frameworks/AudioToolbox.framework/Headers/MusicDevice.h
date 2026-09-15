#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/MusicDevice.h>)
/*!
	@file		MusicDevice.h
	@framework	AudioToolbox.framework
	@copyright	(c) 2000-2015 Apple, Inc. All rights reserved.
	@abstract	Additional Audio Unit API for software musical instruments.

	@discussion

	A music device audio unit, commonly referred to as a music instrument, is used to render notes.
	A note is a sound, usually pitched, that is started and stopped with a note number or pitch
	specifier. A note is played on a group (in MIDI this is called a MIDI Channel) and the various
	state values of a group (such as pitch bend, after-touch, etc) is inherited and controlled by
	every playing note on a given group. A note can be individually stopped (which is the common
	case), or stopped with the "All Notes Off" message that is sent to a specific group.

	A music instrument can be multi-timbral - that is, each group can have a particular patch (or
	sound) associated with it, and different groups can have different patches. This is a common
	case for music instruments that implement the General MIDI specification. In this case, the
	music instrument should return the number of available patches at a given time as the value for
	the _InstrumentCount property.

	It is also common for instruments to be mono-timbral - that is, they are only capable of
	producing notes using a single patch/sound and typically only respond to commands on one group.
	In this case, the music instrument should return 0 as the value for the _InstrumentCount
	property.

	Parameters can be defined in Group Scope, and these parameter IDs within the range of 0 < 1024,
	are equivalent to the standard definitions of control in the MIDI specification (up to the ID
	of). Parameters in group scope above 1024 are audio unit defined.

	Notes can be created/started with one of two methods. To stop a note it must be stopped with the
	same API group as was used to start it (MIDI or the extended Start/Stop note API.

	(1) the MIDI Note on event (MusicDeviceMIDIEvent)
		- notes must be stopped with the MIDI note off event (MusicDeviceMIDIEvent)
		The MIDI Note number is used to turn the note off for the specified channel
		
	(2) the extended Note API (MusicDeviceStartNote). This API returns a note instance ID.
		This is unique and can be used with the kAudioUnitScope_Note.
		It is also used to turn the note off with MusicDeviceStopNote
*/

#ifndef AudioUnit_MusicDevice_h
#define AudioUnit_MusicDevice_h

#include <AudioToolbox/AUComponent.h>

CF_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

//=====================================================================================================================
#pragma mark -
#pragma mark Types

/*!
	@typedef MusicDeviceInstrumentID
	@abstract type for instrument identifiers
*/
typedef UInt32                          MusicDeviceInstrumentID;

/*!
	@struct MusicDeviceStdNoteParams
	@abstract convenience struct for specifying a note and velocity
	
	@discussion This struct is the common usage for MusicDeviceStartNote, as most synths that implement this functionality 
				will only allow for the specification of a note number and velocity when starting a new note.
	
	@var  			argCount
			Should be set to 2
	@var  			mPitch
			The pitch of the new note, typically specified using a MIDI note number (and a fractional pitch) within the 
					range of 0 < 128. So 60 is middle C, 60.5 is middle C + 50 cents.
	@var  			mVelocity
			The velocity of the new note - this can be a fractional value - specified as MIDI (within the range of 0 < 128)
*/
struct MusicDeviceStdNoteParams
{
	UInt32							argCount;		/* should be 2*/
	Float32							mPitch;
	Float32							mVelocity;
};
typedef struct MusicDeviceStdNoteParams		MusicDeviceStdNoteParams;

/*!
	@struct NoteParamsControlValue
	@abstract used to describe a control and value
	
	@discussion This struct is used to describe a parameterID (a control in MIDI terminology, though it is not limited to 
					MIDI CC specifications) and the value of this parameter.
	
	@var  			mID
			The parameter ID
	@var  			mValue
			The value of that parameter
*/
struct NoteParamsControlValue
{
	AudioUnitParameterID			mID;
	AudioUnitParameterValue			mValue;
};
typedef struct NoteParamsControlValue		NoteParamsControlValue;

/*!
	@struct MusicDeviceNoteParams
	@abstract Used to hold the value of the inParams parameter for the MusicDeviceStartNote function.
	
	@discussion The generic version of this structure describes an arg count (which is the number of mControls values 
				+ 1 for mPitch and 1 for mVelocity). So, argCount should at least be two. See MusicDeviceStdNoteParams 
				for the common use case, as many audio unit instruments will not respond to control values provided 
				in the start note function
	
	@var  			argCount
			The number of controls + 2 (for mPitch and mVelocity)
	@var  			mPitch
			The pitch of the new note, typically specified using a MIDI note number (and a fractional pitch) within the 
				range of 0 < 128. So 60 is middle C, 60.5 is middle C + 50 cents.
	@var  			mVelocity
			The velocity of the new note - this can be a fractional value - specified as MIDI (within the range of 0 < 128)
	@var  			mControls
			A variable length array with the number of elements: argCount - 2.
*/
struct MusicDeviceNoteParams
{
	UInt32							argCount;
	Float32							mPitch;
	Float32							mVelocity;
	NoteParamsControlValue			mControls[1];				/* arbitrary length */
};
typedef struct MusicDeviceNoteParams    MusicDeviceNoteParams;

/*!
	enum	MusicNoteEvent
	@discussion This is used to signify that the patch used to start a note (its sound) is defined by the current 
					selection for the group ID; this is the normal usage in MIDI as any notes started on a given channel 
					(group ID) use the sound (patch) defined for that channel. See MusicDeviceStartNote
	
	@constant	kMusicNoteEvent_UseGroupInstrument
			Use the patch (instrument number) assigned to the new notes group ID
	@constant	kMusicNoteEvent_Unused
			The instrument ID is not specified
	
*/
enum {
	kMusicNoteEvent_UseGroupInstrument  = 0xFFFFFFFF,
	kMusicNoteEvent_Unused				= 0xFFFFFFFF
};

/*!
	@typedef		MusicDeviceGroupID
	@discussion The type used to specify which group (channel number in MIDI) is used with a given command (new note, 
				control or parameter value change)
*/
typedef UInt32                          MusicDeviceGroupID;

/*!
	@typedef		NoteInstanceID
	@discussion The type used to hold an unique identifier returned by MusicDeviceStartNote that is used to then address 
				that note (typically to turn the note off). An ID must be used for notes, because notes can be specified 
				by fractional pitches, and so using the MIDI note number is not sufficient to identify the note to turn 
				it off (or to apply polyphonic after touch). 
*/
typedef UInt32                          NoteInstanceID;

/*!
	@typedef		MusicDeviceComponent
	@discussion	The unique type of a MusicDevice audio unit (which is an AudioComponentInstance)
*/
typedef AudioComponentInstance          MusicDeviceComponent;

/*!
	@struct			MIDIEventList
	@abstract Forward declaration of MIDIEventList found in <CoreMIDI/MIDIServices.h>
*/
typedef struct MIDIEventList MIDIEventList;

//=====================================================================================================================
#pragma mark -
#pragma mark Functions

/*!
	@function	MusicDeviceMIDIEvent
	@abstract	Used to sent MIDI channel messages to an audio unit
	
	@discussion	This is the API used to send MIDI channel messages to an audio unit. The status and data parameters 
				are used exactly as described by the MIDI specification, including the combination of channel and 
				command in the status byte. All events sent via MusicDeviceMIDIEventList will be delivered to the
				audio unit in the MIDI protocol returned by kAudioUnitProperty_AudioUnitMIDIProtocol.
	
	@param			inUnit
				The audio unit
	@param			inStatus
				The MIDI status byte
	@param			inData1
				The first MIDI data byte (value is in the range 0 < 128)
	@param			inData2
				The second MIDI data byte (value is in the range 0 < 128). If the MIDI status byte only has one 
					data byte, this should be set to zero.
	@param			inOffsetSampleFrame
				If you are scheduling the MIDI Event from the audio unit's render thread, then you can supply a 
					sample offset that the audio unit may apply when applying that event in its next audio unit render. 
					This allows you to schedule to the sample, the time when a MIDI command is applied and is particularly 
					important when starting new notes. If you are not scheduling in the audio unit's render thread, 
					then you should set this value to 0

	@result			noErr, or an audio unit error code
*/
extern OSStatus
MusicDeviceMIDIEvent(	MusicDeviceComponent	inUnit,
						UInt32					inStatus,
						UInt32					inData1,
						UInt32					inData2,
						UInt32					inOffsetSampleFrame)
							CA_REALTIME_API
							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicDeviceSysEx
	@abstract	used to send any non-channel MIDI event to an audio unit
	
	@discussion	This is used to send any non-channel MIDI event to an audio unit. In practise this is a System Exclusive 
					(SysEx) MIDI message
	
	@param			inUnit
				The audio unit
	@param			inData
				The complete MIDI SysEx message including the F0 and F7 start and termination bytes
	@param			inLength
				The size, in bytes, of the data

	@result			noErr, or an audio unit error code
*/
extern OSStatus
MusicDeviceSysEx(		MusicDeviceComponent	inUnit,
						const UInt8 *			inData,
						UInt32					inLength)
							CA_REALTIME_API
							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicDeviceMIDIEventList
	@abstract	Used to send MIDI messages to an audio unit
    
    @discussion This API is suitable for sending Universal MIDI Packet (UMP) MIDI messages to an audio unit. The message must be
					a full non-SysEx event, a partial SysEx event, or a complete SysEx event. Running status is not allowed. MIDI 1.0 in
					universal packets (MIDI-1UP) and MIDI 2.0 messages are allowed. All events sent via MusicDeviceMIDIEventList will
					be delivered to the audio unit in the MIDI protocol returned by kAudioUnitProperty_AudioUnitMIDIProtocol.

					This is bridged to the v2 API property kAudioUnitProperty_MIDIOutputCallback.
    
	@param				inUnit
					The audio unit
	@param				inOffsetSampleFrame
					If you are scheduling the MIDIEventList from the audio unit's render thread, then you can supply a
					sample offset that the audio unit may apply within its next audio unit render.
					This allows you to schedule to the sample, the time when a MIDI command is applied and is particularly
					important when starting new notes. If you are not scheduling in the audio unit's render thread,
					then you should set this value to 0
 
					inOffsetSampleFrame should serve as the base offset for each packet's timestamp i.e.
					sampleOffset = inOffsetSampleFrame + evtList.packet[0].timeStamp
 
	@param        evtList
					The MIDIEventList to be sent

    @result            noErr, or an audio unit error code
*/
extern OSStatus
MusicDeviceMIDIEventList(   MusicDeviceComponent			inUnit,
							UInt32							inOffsetSampleFrame,
							const struct MIDIEventList *	evtList)
								CA_REALTIME_API
								API_AVAILABLE(macos(12), ios(15.0), tvos(15.0));

/*!
	@function	MusicDeviceStartNote
	@abstract	used to start a note
	
	@discussion	This function is used to start a note.  The caller must provide a NoteInstanceID to receive a 
					token that is then used to stop the note. The MusicDeviceStopNote call should be used to stop 
					notes started with this API. The token can also be used to address individual notes on the 
					kAudioUnitScope_Note if the audio unit supports it. The instrumentID is no longer used and the 
					kMusicNoteEvent_Unused constant should be specified (this takes the current patch for the 
					specifed group as the sound to use for the note).
	
			The Audio unit must provide an unique ID for the note instance ID. This ID must be non-zero and not 
					0xFFFFFFFF (any other UInt32 value is valid).
			
			Not all Music Device audio units implement the semantics of this API (though it is strongly recommended 
					that they do). A host application shoudl query the kMusicDeviceProperty_SupportsStartStopNote to 
					check that this is supported.
			
	@param			inUnit
				The audio unit
	@param			inInstrument
				The instrumentID is no longer used and the kMusicNoteEvent_Unused constant should be specified (this takes 
					the current patch for the specifed group as the sound to use for the note)
	@param			inGroupID
				The group ID that this note will be attached too. As with MIDI, all notes sounding on a groupID can be 
					controlled through the various parameters (such as pitch bend, etc) that can be specified on the Group 
					Scope
	@param			outNoteInstanceID
				A pointer to receive the token that is used to identify the note. This parameter must be specified
	@param			inOffsetSampleFrame
				If you are scheduling the MIDI Event from the audio unit's render thread, then you can supply a sample offset 
					that the audio unit may apply when starting the note in its next audio unit render. This allows you to 
					schedule to the sample and is particularly important when starting new notes. If you are not scheduling 
					in the audio unit's render thread, then you should set this value to 0
	@param			inParams
				The parameters to be used when starting the note - pitch and velocity must be specified
	
	@result			noErr, or an audio unit error code
*/
extern OSStatus
MusicDeviceStartNote(	MusicDeviceComponent				inUnit,
						MusicDeviceInstrumentID				inInstrument,
						MusicDeviceGroupID					inGroupID,
						NoteInstanceID *					outNoteInstanceID,
						UInt32								inOffsetSampleFrame,
						const MusicDeviceNoteParams *	 	inParams)
							CA_REALTIME_API
							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));

/*!
	@function	MusicDeviceStopNote
	@abstract	used to stop notes started with the MusicDeviceStartNote call

	@discussion	This call is used to stop notes that have been started with the MusicDeviceStartNote call; both the group ID 
					that the note was started on and the noteInstanceID should be specified.
	
	@param			inUnit
				The audio unit
	@param			inGroupID
				the group ID
	@param			inNoteInstanceID
				the note instance ID
	@param			inOffsetSampleFrame
				the sample offset within the next buffer rendered that the note should be turned off at

	@result			noErr, or an audio unit error code
*/
extern OSStatus
MusicDeviceStopNote(	MusicDeviceComponent	inUnit,
						MusicDeviceGroupID		inGroupID,
						NoteInstanceID			inNoteInstanceID,
						UInt32					inOffsetSampleFrame)
							CA_REALTIME_API
							API_AVAILABLE(macos(10.0), ios(5.0), watchos(2.0), tvos(9.0));


/*!
	@enum	Music Device range
	@constant	kMusicDeviceRange
				delineates the start of the selector ranges for music devices
				
	@constant	kMusicDeviceMIDIEventSelect
	@constant	kMusicDeviceSysExSelect
	@constant	kMusicDevicePrepareInstrumentSelect
	@constant	kMusicDeviceReleaseInstrumentSelect
	@constant	kMusicDeviceStartNoteSelect
	@constant	kMusicDeviceStopNoteSelect
    @constant	kMusicDeviceMIDIEventListSelect
*/
enum {
		kMusicDeviceRange						= 0x0100,
		kMusicDeviceMIDIEventSelect				= 0x0101,
		kMusicDeviceSysExSelect					= 0x0102,
		kMusicDevicePrepareInstrumentSelect		= 0x0103,
		kMusicDeviceReleaseInstrumentSelect		= 0x0104,
		kMusicDeviceStartNoteSelect				= 0x0105,
		kMusicDeviceStopNoteSelect				= 0x0106,
		kMusicDeviceMIDIEventListSelect			= 0x0107
};

//=====================================================================================================================
#pragma mark -
#pragma mark Fast-dispatch function prototypes

/*!
	@typedef		MusicDeviceMIDIEventProc
	@discussion		This proc can be exported through the FastDispatch property or is used as the prototype for
					an audio component dispatch for this selector. 
					
					The arguments are the same as are provided to the corresponding API call
	
	@param			self
					For a component manager component, this is the component instance storage pointer

	@result			noErr, or an audio unit error code
*/
typedef OSStatus
(*MusicDeviceMIDIEventProc)(	void *					self,
								UInt32					inStatus,
								UInt32					inData1,
								UInt32					inData2,
								UInt32					inOffsetSampleFrame) CA_REALTIME_API;

/*!
	@typedef		MusicDeviceSysExProc
	@discussion		This proc can be exported through the FastDispatch property or is used as the prototype for
					an audio component dispatch for this selector. 
					
					The arguments are the same as are provided to the corresponding API call
	
	@param			self
					For a component manager component, this is the component instance storage pointer

	@result			noErr, or an audio unit error code
*/
typedef OSStatus
(*MusicDeviceSysExProc)(	void *						self,
							const UInt8 *				inData,
							UInt32						inLength) CA_REALTIME_API;

/*!
	@typedef		MusicDeviceStartNoteProc
	@discussion		This proc can be exported through the FastDispatch property or is used as the prototype for
					an audio component dispatch for this selector. 
					
					The arguments are the same as are provided to the corresponding API call
	
	@param			self
					For a component manager component, this is the component instance storage pointer
	
	@result			noErr, or an audio unit error code
*/
typedef OSStatus
(*MusicDeviceStartNoteProc)(	void *					self,
						MusicDeviceInstrumentID			inInstrument,
						MusicDeviceGroupID				inGroupID,
						NoteInstanceID *				outNoteInstanceID,
						UInt32							inOffsetSampleFrame,
						const MusicDeviceNoteParams *	inParams) CA_REALTIME_API;

/*!
	@typedef		MusicDeviceStopNoteProc
	@discussion		This proc can be exported through the FastDispatch property or is used as the prototype for
					an audio component dispatch for this selector. 
					
					The arguments are the same as are provided to the corresponding API call
	
	@param			self
					For a component manager component, this is the component instance storage pointer
	
	@result			noErr, or an audio unit error code
*/
typedef OSStatus
(*MusicDeviceStopNoteProc)(	void *						self,
						MusicDeviceGroupID				inGroupID,
						NoteInstanceID					inNoteInstanceID,
						UInt32							inOffsetSampleFrame) CA_REALTIME_API;


//=====================================================================================================================
#pragma mark -
#pragma mark Deprecated

/*
	The notion of instruments (separate voices assigned to different control groups) is a deprecated concept.
	
	Going forward, multitimbral synths are implemented using Part Scopes.
	
	Thus, the Prepare and Release Instrument API calls are deprecated (see also MusicDeviceStartNote)

*/
extern OSStatus
MusicDevicePrepareInstrument(	MusicDeviceComponent		inUnit,
								MusicDeviceInstrumentID		inInstrument)		
										API_DEPRECATED("no longer supported", macos(10.0, 10.5)) API_UNAVAILABLE(ios, watchos, tvos);


extern OSStatus
MusicDeviceReleaseInstrument(	MusicDeviceComponent		inUnit,
								MusicDeviceInstrumentID		inInstrument)		
										API_DEPRECATED("no longer supported", macos(10.0, 10.5)) API_UNAVAILABLE(ios, watchos, tvos);

	
#ifdef __cplusplus
}
#endif

CF_ASSUME_NONNULL_END

#endif /* AudioUnit_MusicDevice_h */

#else
#include <AudioToolboxCore/MusicDevice.h>
#endif
