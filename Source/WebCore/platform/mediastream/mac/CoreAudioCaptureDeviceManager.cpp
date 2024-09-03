/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "CoreAudioCaptureDeviceManager.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#include "CoreAudioCaptureDevice.h"
#include "CoreAudioCaptureSource.h"
#include "Logging.h"
#include "RealtimeMediaSourceCenter.h"
#include <AudioUnit/AudioUnit.h>
#include <CoreMedia/CMSync.h>
#include <pal/spi/cf/CoreAudioSPI.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

CoreAudioCaptureDeviceManager& CoreAudioCaptureDeviceManager::singleton()
{
    static NeverDestroyed<CoreAudioCaptureDeviceManager> manager;
    return manager;
}

CoreAudioCaptureDeviceManager::~CoreAudioCaptureDeviceManager() = default;

const Vector<CaptureDevice>& CoreAudioCaptureDeviceManager::captureDevices()
{
    coreAudioCaptureDevices();
    return m_captureDevices;
}

std::optional<CaptureDevice> CoreAudioCaptureDeviceManager::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& deviceID)
{
    ASSERT_UNUSED(type, type == CaptureDevice::DeviceType::Microphone);
    for (auto& device : captureDevices()) {
        if (device.persistentId() == deviceID)
            return device;
    }
    return std::nullopt;
}

static bool deviceHasStreams(AudioObjectID deviceID, const AudioObjectPropertyAddress& address)
{
    UInt32 dataSize = 0;
    auto err = AudioObjectGetPropertyDataSize(deviceID, &address, 0, nullptr, &dataSize);
    if (err || !dataSize)
        return false;

    auto bufferList = std::unique_ptr<AudioBufferList>((AudioBufferList*) ::operator new (dataSize));
    memset(bufferList.get(), 0, dataSize);
    err = AudioObjectGetPropertyData(deviceID, &address, 0, nullptr, &dataSize, bufferList.get());

    return !err && bufferList->mNumberBuffers;
}

static bool deviceHasInputStreams(AudioObjectID deviceID)
{
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain
    };
    return deviceHasStreams(deviceID, address);
}

static bool deviceHasOutputStreams(AudioObjectID deviceID)
{
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };
    return deviceHasStreams(deviceID, address);
}

static bool isValidCaptureDevice(const CoreAudioCaptureDevice& device, bool filterTapEnabledDevices)
{
    if (filterTapEnabledDevices) {
        // Ignore output devices that have input only for echo cancellation.
        AudioObjectPropertyAddress address = {
#if HAVE(AUDIO_DEVICE_PROPERTY_REFERENCE_STREAM_ENABLED)
            kAudioDevicePropertyReferenceStreamEnabled,
#else
            kAudioDevicePropertyTapEnabled,
#endif
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMain
        };
        if (AudioObjectHasProperty(device.deviceID(), &address)) {
            RELEASE_LOG(WebRTC, "Ignoring output device that have input only for echo cancellation");
            return false;
        }
    }

    // Ignore non-aggregable devices.
    UInt32 dataSize = 0;
    AudioObjectPropertyAddress address = {
        kAudioObjectPropertyCreator,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef name = nullptr;
    dataSize = sizeof(name);
    AudioObjectGetPropertyData(device.deviceID(), &address, 0, nullptr, &dataSize, &name);
    bool isNonAggregable = !name || !String(name).startsWith("com.apple.audio.CoreAudio"_s);
    if (name)
        CFRelease(name);
    if (isNonAggregable) {
        RELEASE_LOG(WebRTC, "Ignoring output device that is non aggregable");
        return false;
    }

    // Ignore unnamed devices and aggregate devices created by VPIO.
    if (device.label().isEmpty()) {
        RELEASE_LOG(WebRTC, "Ignoring output device that is unnamed");
        return false;
    }

    if (device.label().startsWith("VPAUAggregateAudioDevice"_s)) {
        RELEASE_LOG(WebRTC, "Ignoring output VPAUAggregateAudioDevice device");
        return false;
    }

    if (device.label().contains("WebexMediaAudioDevice"_s)) {
        RELEASE_LOG(WebRTC, "Ignoring webex audio device");
        return false;
    }

    return true;
}

void CoreAudioCaptureDeviceManager::scheduleUpdateCaptureDevices()
{
    if (m_wasRefreshAudioCaptureDevicesScheduled)
        return;

    m_wasRefreshAudioCaptureDevicesScheduled = true;
    callOnMainThread([this] {
        refreshAudioCaptureDevices(NotifyIfDevicesHaveChanged::Notify);
        m_wasRefreshAudioCaptureDevicesScheduled = false;
    });
}

Vector<CoreAudioCaptureDevice>& CoreAudioCaptureDeviceManager::coreAudioCaptureDevices()
{
    static bool initialized;
    if (!initialized) {
        initialized = true;
        refreshAudioCaptureDevices(NotifyIfDevicesHaveChanged::DoNotNotify);

        auto listener = ^(UInt32 count, const AudioObjectPropertyAddress properties[]) {
            bool notify = false;
            for (UInt32 i = 0; i < count; ++i)
                notify |= (properties[i].mSelector == kAudioHardwarePropertyDevices || properties[i].mSelector == kAudioHardwarePropertyDefaultInputDevice || properties[i].mSelector == kAudioHardwarePropertyDefaultOutputDevice);

            if (notify)
                CoreAudioCaptureDeviceManager::singleton().scheduleUpdateCaptureDevices();
        };

        AudioObjectPropertyAddress address = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        auto err = AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(), listener);
        if (err)
            LOG_ERROR("CoreAudioCaptureDeviceManager::devices(%p) AudioObjectAddPropertyListener for kAudioHardwarePropertyDevices returned error %d (%.4s)", this, (int)err, (char*)&err);

        address = {
            kAudioHardwarePropertyDefaultInputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        err = AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(), listener);
        if (err)
            LOG_ERROR("CoreAudioCaptureDeviceManager::devices(%p) AudioObjectAddPropertyListener for kAudioHardwarePropertyDefaultInputDevice returned error %d (%.4s)", this, (int)err, (char*)&err);

        address = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        err = AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(), listener);
        if (err)
            LOG_ERROR("CoreAudioCaptureDeviceManager::devices(%p) AudioObjectAddPropertyListener for kAudioHardwarePropertyDefaultOutputDevice returned error %d (%.4s)", this, (int)err, (char*)&err);
    }

    return m_coreAudioCaptureDevices;
}

std::optional<CoreAudioCaptureDevice> CoreAudioCaptureDeviceManager::coreAudioDeviceWithUID(const String& deviceID)
{
    for (auto& device : coreAudioCaptureDevices()) {
        if (device.persistentId() == deviceID && device.enabled())
            return device;
    }
    return std::nullopt;
}

static inline bool hasDevice(const Vector<CoreAudioCaptureDevice>& devices, uint32_t deviceID, CaptureDevice::DeviceType deviceType)
{
    return std::any_of(devices.begin(), devices.end(), [&deviceID, deviceType](auto& device) {
        return device.deviceID() == deviceID && device.type() == deviceType;
    });
}

static inline Vector<CoreAudioCaptureDevice> computeAudioDeviceList(bool filterTapEnabledDevices)
{
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 dataSize = 0;
    auto err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &dataSize);
    if (err) {
        RELEASE_LOG(WebRTC, "computeAudioDeviceList failed to get size of device list %d (%.4s)", (int)err, (char*)&err);
        return { };
    }

    size_t deviceCount = dataSize / sizeof(AudioObjectID);
    Vector<AudioObjectID> deviceIDs(deviceCount);
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &dataSize, deviceIDs.data());
    if (err) {
        RELEASE_LOG(WebRTC, "computeAudioDeviceList failed to get device list %d (%.4s)", (int)err, (char*)&err);
        return { };
    }

    Vector<CoreAudioCaptureDevice> audioDevices;

    // Microphones
    for (size_t i = 0; i < deviceCount; i++) {
        AudioObjectID deviceID = deviceIDs[i];

        if (!deviceHasInputStreams(deviceID) || hasDevice(audioDevices, deviceID, CaptureDevice::DeviceType::Microphone))
            continue;

        auto microphoneDevice = CoreAudioCaptureDevice::create(deviceID, CaptureDevice::DeviceType::Microphone, { });
        if (microphoneDevice && isValidCaptureDevice(microphoneDevice.value(), filterTapEnabledDevices))
            audioDevices.append(WTFMove(microphoneDevice.value()));
    }

    // Speakers
    for (size_t i = 0; i < deviceCount; i++) {
        AudioObjectID deviceID = deviceIDs[i];

        if (!deviceHasOutputStreams(deviceID) || hasDevice(audioDevices, deviceID, CaptureDevice::DeviceType::Speaker))
            continue;

        String groupID;
        for (auto relatedDeviceID : CoreAudioCaptureDevice::relatedAudioDeviceIDs(deviceID)) {
            for (auto& device : audioDevices) {
                if (device.deviceID() == relatedDeviceID && device.type() == CaptureDevice::DeviceType::Microphone) {
                    groupID = device.persistentId();
                    break;
                }
            }
        }

        auto device = CoreAudioCaptureDevice::create(deviceID, CaptureDevice::DeviceType::Speaker, groupID);
        if (device) {
            // If there is no groupID, relate devices if the label is matching.
            if (groupID.isNull()) {
                for (auto& existingDevice : audioDevices) {
                    if (existingDevice.label() == device->label() && existingDevice.type() == CaptureDevice::DeviceType::Microphone) {
                        device->setGroupId(existingDevice.persistentId());
                        break;
                    }
                }
            }
            audioDevices.append(WTFMove(device.value()));
        }
    }
    return audioDevices;
}

void CoreAudioCaptureDeviceManager::refreshAudioCaptureDevices(NotifyIfDevicesHaveChanged notify)
{
    ASSERT(isMainThread());

    auto audioDevices = computeAudioDeviceList(m_filterTapEnabledDevices);
    bool haveDeviceChanges = audioDevices.size() != m_coreAudioCaptureDevices.size();
    if (!haveDeviceChanges) {
        for (size_t cptr = 0; cptr < audioDevices.size(); ++cptr) {
            auto& oldDevice = m_coreAudioCaptureDevices[cptr];
            auto& newDevice = audioDevices[cptr];
            if (newDevice.type() != oldDevice.type() || newDevice.deviceID() != oldDevice.deviceID() || newDevice.isDefault() != oldDevice.isDefault() || newDevice.enabled() != oldDevice.enabled() || newDevice.isDefault() != oldDevice.isDefault())
                haveDeviceChanges = true;
        }
    }
    if (!haveDeviceChanges)
        return;

    std::sort(audioDevices.begin(), audioDevices.end(), [] (auto& first, auto& second) -> bool {
        return first.isDefault() && !second.isDefault();
    });
    m_coreAudioCaptureDevices = WTFMove(audioDevices);

    m_captureDevices.clear();
    m_speakerDevices.clear();
    for (auto& device : m_coreAudioCaptureDevices) {
        if (device.type() == CaptureDevice::DeviceType::Microphone)
            m_captureDevices.append(device);
        else
            m_speakerDevices.append(device);
    }

    if (notify == NotifyIfDevicesHaveChanged::Notify)
        deviceChanged();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
