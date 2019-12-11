/*
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreMedia/CoreMedia.h>

#if PLATFORM(COCOA)

#if USE(APPLE_INTERNAL_SDK)
#include <CoreMedia/CMNotificationCenter.h>
#else
typedef struct opaqueCMNotificationCenter *CMNotificationCenterRef;
typedef void (*CMNotificationCallback)(CMNotificationCenterRef inCenter, const void *inListener, CFStringRef inNotificationName, const void *inNotifyingObject, CFTypeRef inNotificationPayload);
#endif

WTF_EXTERN_C_BEGIN
CMNotificationCenterRef CMNotificationCenterGetDefaultLocalCenter(void);
OSStatus CMNotificationCenterAddListener(CMNotificationCenterRef inCenter, const void *inListener, CMNotificationCallback inCallBack, CFStringRef inNotificationName, const void *inObjectToObserve, UInt32 inFlags);
OSStatus CMNotificationCenterRemoveListener(CMNotificationCenterRef inCenter, const void *inListener, CMNotificationCallback inCallBack, CFStringRef inNotificationName, const void *inObject);
WTF_EXTERN_C_END

#endif // PLATFORM(COCOA)

#if PLATFORM(WIN)

typedef struct OpaqueCMBlockBuffer* CMBlockBufferRef;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer* CMSampleBufferRef;

#ifndef CMSAMPLEBUFFER_H
WTF_EXTERN_C_BEGIN
#pragma pack(push, 4)
typedef struct {
    CMTime duration;
    CMTime presentationTimeStamp;
    CMTime decodeTimeStamp;
} CMSampleTimingInfo;
#pragma pack(pop)
WTF_EXTERN_C_END
#endif
#endif // PLATFORM(WIN)
