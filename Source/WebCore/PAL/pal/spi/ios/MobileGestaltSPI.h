/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import <wtf/Platform.h>

#if PLATFORM(IOS_FAMILY)

#include <CoreFoundation/CoreFoundation.h>

#if USE(APPLE_INTERNAL_SDK)

#include <MobileGestalt.h>

#else

static const CFStringRef kMGQAppleInternalInstallCapability = CFSTR("apple-internal-install");
static const CFStringRef kMGQMainScreenClass = CFSTR("main-screen-class");
static const CFStringRef kMGQMainScreenPitch = CFSTR("main-screen-pitch");
static const CFStringRef kMGQMainScreenScale = CFSTR("main-screen-scale");
static const CFStringRef kMGQiPadCapability = CFSTR("ipad");
static const CFStringRef kMGQDeviceName = CFSTR("DeviceName");
static const CFStringRef kMGQDeviceClassNumber = CFSTR("DeviceClassNumber");
static const CFStringRef kMGQHasExtendedColorDisplay = CFSTR("HasExtendedColorDisplay");

typedef enum {
    MGDeviceClassInvalid = -1,
    /* 0 is intentionally not in this enum */
    MGDeviceClassiPhone  = 1,
    MGDeviceClassiPod    = 2,
    MGDeviceClassiPad    = 3,
    MGDeviceClassAppleTV = 4,
    /* 5 is intentionally not in this enum */
    MGDeviceClassWatch   = 6,
} MGDeviceClass;

typedef enum {
    MGScreenClassPad2          = 4,
    MGScreenClassPad3          = 6,
    MGScreenClassPad4          = 7,
} MGScreenClass;

#endif

WTF_EXTERN_C_BEGIN

CFTypeRef MGCopyAnswer(CFStringRef question, CFDictionaryRef options);

#ifndef MGGetBoolAnswer
bool MGGetBoolAnswer(CFStringRef question);
#endif

#ifndef MGGetSInt32Answer
SInt32 MGGetSInt32Answer(CFStringRef question, SInt32 defaultValue);
#endif

#ifndef MGGetFloat32Answer
Float32 MGGetFloat32Answer(CFStringRef question, Float32 defaultValue);
#endif

WTF_EXTERN_C_END

#endif
