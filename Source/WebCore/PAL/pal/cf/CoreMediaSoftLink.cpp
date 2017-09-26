/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
// FIXME: Should be USE(COREMEDIA), but this isn't currently defined on Windows.

#include <pal/spi/cf/CoreMediaSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(PAL, CoreMedia)

SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, CoreMedia, CMTimeMake, CMTime, (int64_t value, int32_t timescale), (value, timescale))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))

#if PLATFORM(COCOA)
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, CoreMedia, CMTimebaseCreateWithMasterClock, OSStatus, (CFAllocatorRef allocator, CMClockRef masterClock, CMTimebaseRef *timebaseOut), (allocator, masterClock, timebaseOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, CoreMedia, CMTimebaseGetTime, CMTime, (CMTimebaseRef timebase), (timebase))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, CoreMedia, CMTimebaseSetRate, OSStatus, (CMTimebaseRef timebase, Float64 rate), (timebase, rate))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, CoreMedia, CMTimebaseSetTime, OSStatus, (CMTimebaseRef timebase, CMTime time), (timebase, time))
#endif // PLATFORM(COCOA)

#if PLATFORM(IOS)
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, CoreMedia, CMAudioClockCreate, OSStatus, (CFAllocatorRef allocator, CMClockRef *clockOut), (allocator, clockOut))
#endif // PLATFORM(IOS)

#if PLATFORM(MAC)
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, CoreMedia, CMAudioDeviceClockCreate, OSStatus, (CFAllocatorRef allocator, CFStringRef deviceUID, CMClockRef *clockOut), (allocator, deviceUID, clockOut))
#endif // PLATFORM(MAC)

#endif // USE(AVFOUNDATION)
