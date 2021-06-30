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
 * (INCLUDING NEGLIGEPAL::NCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
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

static bool getDeviceInfo(uint32_t deviceID, CaptureDevice::DeviceType type, String& persistentID, String& label)
{
    CFStringRef uniqueID;
    AudioObjectPropertyAddress address {
        kAudioDevicePropertyDeviceUID,
        kAudioDevicePropertyScopeInput,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    UInt32 dataSize = sizeof(uniqueID);
    auto err = AudioObjectGetPropertyData(static_cast<UInt32>(deviceID), &address, 0, nullptr, &dataSize, &uniqueID);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureDevice::getDeviceInfo failed to get device unique id with error %d (%.4s)", (int)err, (char*)&err);
        return false;
    }
    persistentID = uniqueID;
    CFRelease(uniqueID);

    CFStringRef localizedName = nullptr;
    AudioObjectPropertyScope scope = type == CaptureDevice::DeviceType::Microphone ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    address = {
        kAudioDevicePropertyDataSource,
        scope,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    uint32_t sourceID;
    dataSize = sizeof(sourceID);
    err = AudioObjectGetPropertyData(static_cast<UInt32>(deviceID), &address, 0, nullptr, &dataSize, &sourceID);
    if (!err) {
        AudioValueTranslation translation = { &deviceID, sizeof(deviceID), &localizedName, sizeof(localizedName) };
        address = {
            kAudioDevicePropertyDataSourceNameForIDCFString,
            scope,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
            kAudioObjectPropertyElementMain
#else
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            kAudioObjectPropertyElementMaster
            ALLOW_DEPRECATED_DECLARATIONS_END
#endif
        };
        dataSize = sizeof(translation);
        err = AudioObjectGetPropertyData(static_cast<UInt32>(deviceID), &address, 0, nullptr, &dataSize, &translation);
    }

    if (err || !localizedName || !CFStringGetLength(localizedName)) {
        address = {
            kAudioObjectPropertyName,
            kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
            kAudioObjectPropertyElementMain
#else
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            kAudioObjectPropertyElementMaster
            ALLOW_DEPRECATED_DECLARATIONS_END
#endif
        };
        dataSize = sizeof(localizedName);
        err = AudioObjectGetPropertyData(static_cast<UInt32>(deviceID), &address, 0, nullptr, &dataSize, &localizedName);
    }

    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureDevice::getDeviceInfo failed to get device name with error %d (%.4s)", (int)err, (char*)&err);
        return false;
    }

    label = localizedName;
    CFRelease(localizedName);

    return true;
}

std::optional<CoreAudioCaptureDevice> CoreAudioCaptureDevice::create(uint32_t deviceID, DeviceType type, const String& groupID)
{
    ASSERT(type == CaptureDevice::DeviceType::Microphone || type == CaptureDevice::DeviceType::Speaker);
    String persistentID;
    String label;
    if (!getDeviceInfo(deviceID, type, persistentID, label))
        return std::nullopt;

    return CoreAudioCaptureDevice(deviceID, persistentID, type, label, groupID.isNull() ? persistentID : groupID);
}

CoreAudioCaptureDevice::CoreAudioCaptureDevice(uint32_t deviceID, const String& persistentID, DeviceType deviceType, const String& label, const String& groupID)
    : CaptureDevice(persistentID, deviceType, label, groupID)
    , m_deviceID(deviceID)
{
    AudioObjectPropertyAddress address {
        kAudioDevicePropertyDeviceIsAlive,
        kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    UInt32 state = 0;
    UInt32 dataSize = sizeof(state);
    auto err = AudioObjectGetPropertyData(static_cast<UInt32>(m_deviceID), &address, 0, nullptr, &dataSize, &state);
    if (err)
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureDevice::CoreAudioCaptureDevice(%p) failed to get \"is alive\" with error %d (%.4s)", this, (int)err, (char*)&err);
    setEnabled(!err && state);

    UInt32 property = deviceType == CaptureDevice::DeviceType::Microphone ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice;
    address = {
        property,
        kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    AudioDeviceID defaultID = kAudioDeviceUnknown;
    dataSize = sizeof(defaultID);
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &dataSize, &defaultID);
    if (err)
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureDevice::CoreAudioCaptureDevice(%p) failed to get \"is default\" with error %d (%.4s)", this, (int)err, (char*)&err);
    setIsDefault(!err && defaultID == m_deviceID);
}

Vector<AudioDeviceID> CoreAudioCaptureDevice::relatedAudioDeviceIDs(AudioDeviceID deviceID)
{
    UInt32 size = 0;
    AudioObjectPropertyAddress property = {
        kAudioDevicePropertyRelatedDevices,
        kAudioDevicePropertyScopeOutput,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };
    OSStatus error = AudioObjectGetPropertyDataSize(deviceID, &property, 0, 0, &size);
    if (error || !size)
        return { };

    Vector<AudioDeviceID> devices(size / sizeof(AudioDeviceID));
    error = AudioObjectGetPropertyData(deviceID, &property, 0, nullptr, &size, devices.data());
    if (error)
        return { };
    return devices;
}

RetainPtr<CMClockRef> CoreAudioCaptureDevice::deviceClock()
{
    if (m_deviceClock)
        return m_deviceClock;

    CMClockRef clock;
    auto err = PAL::CMAudioDeviceClockCreate(kCFAllocatorDefault, persistentId().createCFString().get(), &clock);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureDevice::CMAudioDeviceClockCreate(%p) CMAudioDeviceClockCreate failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return nullptr;
    }

    m_deviceClock = adoptCF(clock);

    return m_deviceClock;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
