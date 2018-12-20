/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CoreAudioCaptureDevice.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#include "CaptureDeviceManager.h"
#include "Logging.h"
#include <AudioUnit/AudioUnit.h>
#include <CoreMedia/CMSync.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {
using namespace PAL;

static bool getDeviceInfo(uint32_t deviceID, String& persistentID, String& label)
{
    CFStringRef uniqueID;
    AudioObjectPropertyAddress address { kAudioDevicePropertyDeviceUID, kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMaster };
    UInt32 dataSize = sizeof(uniqueID);
    auto err = AudioObjectGetPropertyData(static_cast<UInt32>(deviceID), &address, 0, nullptr, &dataSize, &uniqueID);
    if (err) {
        LOG(Media, "CoreAudioCaptureDevice::getDeviceInfo failed to get device unique id with error %d (%.4s)", (int)err, (char*)&err);
        return false;
    }
    persistentID = uniqueID;
    CFRelease(uniqueID);

    CFStringRef localizedName;
    address = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    dataSize = sizeof(localizedName);
    err = AudioObjectGetPropertyData(static_cast<UInt32>(deviceID), &address, 0, nullptr, &dataSize, &localizedName);
    if (err) {
        LOG(Media, "CoreAudioCaptureDevice::getDeviceInfo failed to get device name with error %d (%.4s)", (int)err, (char*)&err);
        return false;
    }
    label = localizedName;
    CFRelease(localizedName);

    return true;
}

Optional<CoreAudioCaptureDevice> CoreAudioCaptureDevice::create(uint32_t deviceID)
{
    String persistentID;
    String label;
    if (!getDeviceInfo(deviceID, persistentID, label))
        return WTF::nullopt;

    return CoreAudioCaptureDevice(deviceID, persistentID, label);
}

CoreAudioCaptureDevice::CoreAudioCaptureDevice(uint32_t deviceID, const String& persistentID, const String& label)
    : CaptureDevice(persistentID, CaptureDevice::DeviceType::Microphone, label)
    , m_deviceID(deviceID)
{
}

RetainPtr<CMClockRef> CoreAudioCaptureDevice::deviceClock()
{
    if (m_deviceClock)
        return m_deviceClock;

    CMClockRef clock;
    auto err = CMAudioDeviceClockCreate(kCFAllocatorDefault, persistentId().createCFString().get(), &clock);
    if (err) {
        LOG(Media, "CoreAudioCaptureDevice::CMAudioDeviceClockCreate(%p) CMAudioDeviceClockCreate failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return nullptr;
    }

    m_deviceClock = adoptCF(clock);

    return m_deviceClock;
}

bool CoreAudioCaptureDevice::isAlive()
{
    AudioObjectPropertyAddress address = { kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    UInt32 state = 0;
    UInt32 dataSize = sizeof(state);
    if (AudioObjectGetPropertyData(static_cast<UInt32>(m_deviceID), &address, 0, nullptr, &dataSize, &state))
        return false;
    return state;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
