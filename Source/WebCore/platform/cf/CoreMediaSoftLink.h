/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CoreMediaSoftLink_h
#define CoreMediaSoftLink_h

#if USE(AVFOUNDATION)

#include "CoreMediaSPI.h"
#include "SoftLinking.h"
#include <CoreMedia/CoreMedia.h>

SOFT_LINK_FUNCTION_HEADER(CMTimeCompare, int32_t, (CMTime time1, CMTime time2), (time1, time2))
#define CMTimeCompare softLink_CMTimeCompare
SOFT_LINK_FUNCTION_HEADER(CMTimeGetSeconds, Float64, (CMTime time), (time))
#define CMTimeGetSeconds softLink_CMTimeGetSeconds
SOFT_LINK_FUNCTION_HEADER(CMTimeMake, CMTime, (int64_t value, int32_t timescale), (value, timescale))
#define CMTimeMake softLink_CMTimeMake
SOFT_LINK_FUNCTION_HEADER(CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))
#define CMTimeMakeWithSeconds softLink_CMTimeMakeWithSeconds
SOFT_LINK_FUNCTION_HEADER(CMTimeRangeGetEnd, CMTime, (CMTimeRange range), (range))
#define CMTimeRangeGetEnd softLink_CMTimeRangeGetEnd

#if PLATFORM(COCOA)

SOFT_LINK_FUNCTION_HEADER(CMNotificationCenterGetDefaultLocalCenter, CMNotificationCenterRef, (void), ());
#define CMNotificationCenterGetDefaultLocalCenter softLink_CMNotificationCenterGetDefaultLocalCenter
SOFT_LINK_FUNCTION_HEADER(CMNotificationCenterAddListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object, UInt32 flags), (center, listener, callback, notification, object, flags))
#define CMNotificationCenterAddListener softLink_CMNotificationCenterAddListener
SOFT_LINK_FUNCTION_HEADER(CMNotificationCenterRemoveListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object), (center, listener, callback, notification, object))
#define CMNotificationCenterRemoveListener softLink_CMNotificationCenterRemoveListener
SOFT_LINK_FUNCTION_HEADER(CMTimebaseGetTime, CMTime, (CMTimebaseRef timebase), (timebase))
#define CMTimebaseGetTime softLink_CMTimebaseGetTime
SOFT_LINK_FUNCTION_HEADER(CMTimeCopyAsDictionary, CFDictionaryRef, (CMTime time, CFAllocatorRef allocator), (time, allocator))
#define CMTimeCopyAsDictionary softLink_CMTimeCopyAsDictionary

#endif // PLATFORM(COCOA)

#if PLATFORM(WIN)

SOFT_LINK_FUNCTION_HEADER(CMTimeAdd, CMTime, (CMTime addend1, CMTime addend2), (addend1, addend2))
#define CMTimeAdd softLink_CMTimeAdd
SOFT_LINK_FUNCTION_HEADER(CMTimeMakeFromDictionary, CMTime, (CFDictionaryRef dict), (dict))
#define CMTimeMakeFromDictionary softLink_CMTimeMakeFromDictionary

#endif // PLATFORM(WIN)

#endif // USE(AVFOUNDATION)

#endif // CoreMediaSoftLink_h
