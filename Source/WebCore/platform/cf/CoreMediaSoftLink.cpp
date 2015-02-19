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

#include "config.h"

#if USE(AVFOUNDATION)

#include "CoreMediaSPI.h"
#include "SoftLinking.h"
#include <CoreMedia/CoreMedia.h>

namespace WebCore {
SOFT_LINK_FRAMEWORK(CoreMedia)
}

SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMTimeCompare, int32_t, (CMTime time1, CMTime time2), (time1, time2))
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMTimeMake, CMTime, (int64_t value, int32_t timescale), (value, timescale))
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMTimeRangeGetEnd, CMTime, (CMTimeRange range), (range))

#if PLATFORM(COCOA)
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMNotificationCenterGetDefaultLocalCenter, CMNotificationCenterRef, (void), ());
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMNotificationCenterAddListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object, UInt32 flags), (center, listener, callback, notification, object, flags))
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMNotificationCenterRemoveListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object), (center, listener, callback, notification, object))
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMTimebaseGetTime, CMTime, (CMTimebaseRef timebase), (timebase))
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMTimeCopyAsDictionary, CFDictionaryRef, (CMTime time, CFAllocatorRef allocator), (time, allocator))
#endif // PLATFORM(COCOA)

#if PLATFORM(WIN)
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMTimeAdd, CMTime, (CMTime addend1, CMTime addend2), (addend1, addend2))
SOFT_LINK_FUNCTION_SOURCE(CoreMedia, CMTimeMakeFromDictionary, CMTime, (CFDictionaryRef dict), (dict))
#endif // PLATFORM(WIN)

#endif // USE(AVFOUNDATION)
