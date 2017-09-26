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

#pragma once

#if USE(AVFOUNDATION)
// FIXME: Should be USE(COREMEDIA), but this isn't currently defined on Windows.

#include <pal/spi/cf/CoreMediaSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, CoreMedia)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))
#define CMTimeGetSeconds softLink_CoreMedia_CMTimeGetSeconds
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeMake, CMTime, (int64_t value, int32_t timescale), (value, timescale))
#define CMTimeMake softLink_CoreMedia_CMTimeMake
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))
#define CMTimeMakeWithSeconds softLink_CoreMedia_CMTimeMakeWithSeconds

#if PLATFORM(COCOA)
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseCreateWithMasterClock, OSStatus, (CFAllocatorRef allocator, CMClockRef masterClock, CMTimebaseRef *timebaseOut), (allocator, masterClock, timebaseOut))
#define CMTimebaseCreateWithMasterClock softLink_CoreMedia_CMTimebaseCreateWithMasterClock
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseGetTime, CMTime, (CMTimebaseRef timebase), (timebase))
#define CMTimebaseGetTime softLink_CoreMedia_CMTimebaseGetTime
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseSetRate, OSStatus, (CMTimebaseRef timebase, Float64 rate), (timebase, rate))
#define CMTimebaseSetRate softLink_CoreMedia_CMTimebaseSetRate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseSetTime, OSStatus, (CMTimebaseRef timebase, CMTime time), (timebase, time))
#define CMTimebaseSetTime softLink_CoreMedia_CMTimebaseSetTime
#endif // PLATFORM(COCOA)

#if PLATFORM(IOS)
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMAudioClockCreate, OSStatus, (CFAllocatorRef allocator, CMClockRef *clockOut), (allocator, clockOut))
#define CMAudioClockCreate softLink_CoreMedia_CMAudioClockCreate
#endif // PLATFORM(IOS)

#if PLATFORM(MAC)
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMAudioDeviceClockCreate, OSStatus, (CFAllocatorRef allocator, CFStringRef deviceUID, CMClockRef *clockOut), (allocator, deviceUID, clockOut))
#define CMAudioDeviceClockCreate  softLink_CoreMedia_CMAudioDeviceClockCreate
#endif // PLATFORM(MAC)

#endif // USE(AVFOUNDATION)
