#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AudioWorkInterval.h>)
/*!
	@file		AudioWorkInterval.h
	@framework	AudioToolbox.framework
	@copyright	(c) 2020 by Apple Inc., all rights reserved.
	@abstract   API to create workgroups.

	Workgroups express that a collection of realtime threads are working in tandem to produce audio
	for a common deadline. Such threads span multiple processes including the audio server and
	client applications. The system uses workgroups to observe the CPU usage of audio realtime
	threads and dynamically balance the competing considerations of power consumption vs. rendering
	capacity.
	
	Workgroups have "work intervals", or "workgroup intervals", which express the duty cycle of the
	main thread in the workgroup. We sometimes use these terms interchangeably with "workgroup".

	Core Audio devices own a workgroup whose lifetime is the same as that of the device. This
	`os_workgroup_t` can be obtained via `kAudioDevicePropertyIOThreadOSWorkgroup`.

	For Audio I/O units which are associated with an audio device, that device-owned workgroup is
	readable as an Audio Unit property. With the C API (AudioUnitGetProperty()), use
	`kAudioOutputUnitProperty_OSWorkgroup`. With the ObjC API (AUAudioUnit), use the osWorkgroup
	property. If an I/O unit's device assignment changes, as a side effect, its workgroup will also
	change.

	An audio app or plug-in may create auxiliary rendering threads. When such a thread has realtime
	priority, it should be associated with a workgroup, as follows:

	1. If an auxiliary realtime thread operates with the same cadence as the Core Audio
	realtime I/O thread, that is, it is triggered by code known to be running on the device's
	I/O thread and has a deadline within the current I/O cycle, that auxiliary thread should be
	joined to the device thread's workgroup, using the join/leave APIs in
	<os/workgroup_object.h>.
	
	2. If an Audio Unit's auxiliary realtime thread operates with the same cadence as the
	requests to render the plug-in, that is, it is triggered by the rendering thread and is
	expected to finish rendering before that render request completes, the plug-in should
	implement a render context observer, and join its auxiliary thread(s) to any workgroup
	passed to that observer. See the Audio Unit properties
	`kAudioUnitProperty_RenderContextObserver` (v2) and `AUAudioUnit.renderContextObserver` (v3).

	3. If an app or plug-in creates realtime threads that operate asynchronously, i.e. at a
	cadence independent of any audio hardware or Audio Unit rendering cycle, then that app or
	plug-in should create its own workgroup interval, using AudioWorkIntervalCreate(). All
	threads processing to this work interval's deadline should join its workgroup, using
	the join/leave APIs in <os/workgroup_object.h>. The "master" thread, the one coordinating
	the activities of all of the threads in the workgroup (if there are multiple), should
	signify the beginning and ending of each work cycle using `os_workgroup_interval_start()` and
	`os_workgroup_interval_finish()`.

	The properties and API described here are available beginning in macOS 10.16 and iOS 14.0.
	Note that they are unavailable from Swift, because these API's pertain exclusively to realtime
	threads, while the Swift runtime is unsafe for use on realtime threads.

	See also: <os/workgroup_object.h>, <os/workgroup_interval.h>.
*/

#ifndef AudioToolbox_AudioWorkInterval_h
#define AudioToolbox_AudioWorkInterval_h

#include <CoreFoundation/CFBase.h>
#include <os/workgroup.h>

#ifdef __cplusplus
extern "C" {
#endif

CF_ASSUME_NONNULL_BEGIN

/*!
	@fn		AudioWorkIntervalCreate
	@brief	Create an OS workgroup interval for use with audio realtime threads.
	
	@param name
		A name for the created work interval.
	@param clock
		The clockid in which interval timestamps are specified, e.g. `OS_CLOCK_MACH_ABSOLUTE_TIME`
		from <os/clock.h>.
	@param attr
		This field is currently not used and should be NULL.
	@return
		A new os_workgroup_interval_t. The client should call `os_workgroup_interval_start()`
		and `os_workgroup_interval_finish()` to notify the system of the beginning and ending
		of each work duty cycle. The caller is responsible for releasing this object when finished
		with it (if not using automatic reference counting).
*/
OS_WORKGROUP_RETURNS_RETAINED
os_workgroup_interval_t AudioWorkIntervalCreate(
	const char* name, os_clockid_t clock, os_workgroup_attr_t _Nullable attr)
	API_AVAILABLE(macos(11.0), ios(14.0), watchos(7.0), tvos(14.0))
	__SWIFT_UNAVAILABLE_MSG("Swift is not supported for use with audio realtime threads");

CF_ASSUME_NONNULL_END

#ifdef __cplusplus
}
#endif

#endif /* AudioToolbox_AudioWorkInterval_h */
#else
#include <AudioToolboxCore/AudioWorkInterval.h>
#endif
