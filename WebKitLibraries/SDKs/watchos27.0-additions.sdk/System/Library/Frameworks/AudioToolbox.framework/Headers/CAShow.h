#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/CAShow.h>)
/*!
	@file		CAShow.h
	@framework	AudioToolbox.framework
	@copyright	(c) 2002-2018 by Apple, Inc., all rights reserved.
	@abstract	Interfaces to log the internal state of various AudioToolbox objects.
*/

#ifndef AudioToolbox_CAShow_h
#define AudioToolbox_CAShow_h

#include <Availability.h>
#include <CoreFoundation/CFBase.h>
#include <os/base.h>
#include <stdio.h>

CF_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

/// Print the internal state of an object to os_log.
OS_EXPORT void CAShow(void *inObject) 
											API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));

/// Print the internal state of an object to the supplied FILE*.
OS_EXPORT void CAShowFile(void *inObject, FILE *inFile)
											API_AVAILABLE(macos(10.2), ios(2.0), watchos(2.0), tvos(9.0));

#ifdef __cplusplus
}
#endif

CF_ASSUME_NONNULL_END

#endif /* AudioToolbox_CAShow_h */
#else
#include <AudioToolboxCore/CAShow.h>
#endif
