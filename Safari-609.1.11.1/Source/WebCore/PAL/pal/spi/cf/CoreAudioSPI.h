/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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

#if PLATFORM(COCOA)

#if USE(APPLE_INTERNAL_SDK)
#include <CoreAudio/AudioHardwarePriv.h>
#else

#if PLATFORM(MAC)
#include <CoreAudio/AudioHardware.h>

CF_ENUM(AudioObjectPropertySelector)
{
    kAudioDevicePropertyTapEnabled = 'tapd',
};

#else

WTF_EXTERN_C_BEGIN

typedef UInt32 AudioObjectPropertySelector;
typedef UInt32 AudioObjectPropertyScope;
typedef UInt32 AudioObjectPropertyElement;

struct AudioObjectPropertyAddress {
    AudioObjectPropertySelector mSelector;
    AudioObjectPropertyScope mScope;
    AudioObjectPropertyElement mElement;
};
typedef struct AudioObjectPropertyAddress AudioObjectPropertyAddress;

CF_ENUM(AudioObjectPropertyScope)
{
    kAudioObjectPropertyScopeGlobal = 'glob'
};

CF_ENUM(AudioObjectPropertySelector)
{
    kAudioHardwarePropertyDefaultInputDevice = 'dIn ',
    kAudioDevicePropertyTapEnabled = 'tapd',
};

CF_ENUM(int)
{
    kAudioObjectSystemObject    = 1
};

typedef UInt32  AudioObjectID;
typedef AudioObjectID AudioDeviceID;

extern Boolean AudioObjectHasProperty(AudioObjectID inObjectID, const AudioObjectPropertyAddress* __nullable inAddress);
extern OSStatus AudioObjectGetPropertyData(AudioObjectID inObjectID, const AudioObjectPropertyAddress* __nullable inAddress, UInt32                              inQualifierDataSize, const void* __nullable inQualifierData, UInt32* __nullable ioDataSize, void* __nullable outData);

WTF_EXTERN_C_END

#endif

#endif

WTF_EXTERN_C_BEGIN

extern OSStatus AudioDeviceDuck(AudioDeviceID inDevice, Float32 inDuckedLevel, const AudioTimeStamp* __nullable inStartTime, Float32 inRampDuration);

WTF_EXTERN_C_END

#endif // PLATFORM(COCOA)
