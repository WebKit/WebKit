/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

const Vector<CaptureDevice>& CoreAudioCaptureDeviceManager::captureDevices()
{
    coreAudioCaptureDevices();
    return m_captureDevices;
}

Optional<CaptureDevice> CoreAudioCaptureDeviceManager::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& deviceID)
{
    ASSERT_UNUSED(type, type == CaptureDevice::DeviceType::Microphone);
    for (auto& device : captureDevices()) {
        if (device.persistentId() == deviceID)
            return device;
    }
    return WTF::nullopt;
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
    AudioObjectPropertyAddress address = { kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMaster };
    return deviceHasStreams(deviceID, address);
}

static bool deviceHasOutputStreams(AudioObjectID deviceID)
{
    AudioObjectPropertyAddress address = { kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    return deviceHasStreams(deviceID, address);
}

static bool isValidCaptureDevice(const CoreAudioCaptureDevice& device)
{
    // Ignore output devices that have input only for echo cancellation.
    AudioObjectPropertyAddress address = { kAudioDevicePropertyTapEnabled, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    if (AudioObjectHasProperty(device.deviceID(), &address)) {
        RELEASE_LOG(WebRTC, "Ignoring output device that have input only for echo cancellation");
        return false;
    }

    // Ignore non-aggregable devices.
    UInt32 dataSize = 0;
    address = { kAudioObjectPropertyCreator, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    CFStringRef name = nullptr;
    dataSize = sizeof(name);
    AudioObjectGetPropertyData(device.deviceID(), &address, 0, nullptr, &dataSize, &name);
    bool isNonAggregable = !name || !String(name).startsWith("com.apple.audio.CoreAudio");
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

    if (device.label().startsWith("VPAUAggregateAudioDevice")) {
        RELEASE_LOG(WebRTC, "Ignoring output VPAUAggregateAudioDevice device");
        return false;
    }

    return true;
}

static inline Optional<CoreAudioCaptureDevice> getDefaultCaptureInputDevice()
{
    AudioObjectPropertyAddress address { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    UInt32 propertySize = sizeof(AudioDeviceID);
    AudioDeviceID deviceID = kAudioDeviceUnknown;
    auto err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &propertySize, &deviceID);

    if (err != noErr || deviceID == kAudioDeviceUnknown)
        return { };
    return CoreAudioCaptureDevice::create(deviceID, CaptureDevice::DeviceType::Microphone, { });
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
                notify |= (properties[i].mSelector == kAudioHardwarePropertyDevices || properties[i].mSelector == kAudioHardwarePropertyDefaultInputDevice);

            if (notify)
                CoreAudioCaptureDeviceManager::singleton().refreshAudioCaptureDevices(NotifyIfDevicesHaveChanged::Notify);
        };

        AudioObjectPropertyAddress address = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
        auto err = AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(), listener);
        if (err)
            LOG_ERROR("CoreAudioCaptureDeviceManager::devices(%p) AudioObjectAddPropertyListener for kAudioHardwarePropertyDevices returned error %d (%.4s)", this, (int)err, (char*)&err);

        address = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
        err = AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(), listener);
        if (err)
            LOG_ERROR("CoreAudioCaptureDeviceManager::devices(%p) AudioObjectAddPropertyListener for kAudioHardwarePropertyDefaultInputDevice returned error %d (%.4s)", this, (int)err, (char*)&err);
    }

    return m_coreAudioCaptureDevices;
}

Optional<CoreAudioCaptureDevice> CoreAudioCaptureDeviceManager::coreAudioDeviceWithUID(const String& deviceID)
{
    for (auto& device : coreAudioCaptureDevices()) {
        if (device.persistentId() == deviceID && device.enabled())
            return device;
    }
    return WTF::nullopt;
}

static inline bool hasDevice(const Vector<CoreAudioCaptureDevice>& devices, uint32_t deviceID, CaptureDevice::DeviceType deviceType)
{
    return std::any_of(devices.begin(), devices.end(), [&deviceID, deviceType](auto& device) {
        return device.deviceID() == deviceID && device.type() == deviceType;
    });
}

void CoreAudioCaptureDeviceManager::refreshAudioCaptureDevices(NotifyIfDevicesHaveChanged notify)
{
    ASSERT(isMainThread());

    AudioObjectPropertyAddress address = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    UInt32 dataSize = 0;
    auto err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &dataSize);
    if (err) {
        LOG(Media, "CoreAudioCaptureDeviceManager::refreshAudioCaptureDevices(%p) failed to get size of device list %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }

    size_t deviceCount = dataSize / sizeof(AudioObjectID);
    Vector<AudioObjectID> deviceIDs(deviceCount);
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &dataSize, deviceIDs.data());
    if (err) {
        LOG(Media, "CoreAudioCaptureDeviceManager::refreshAudioCaptureDevices(%p) failed to get device list %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }

    auto defaultInputDevice = getDefaultCaptureInputDevice();
    bool haveDeviceChanges = false;
    if (defaultInputDevice && !m_coreAudioCaptureDevices.isEmpty() && m_coreAudioCaptureDevices.first().deviceID() != defaultInputDevice->deviceID()) {
        m_coreAudioCaptureDevices = Vector<CoreAudioCaptureDevice>::from(WTFMove(*defaultInputDevice));
        haveDeviceChanges = true;
    }

    // Microphones
    for (size_t i = 0; i < deviceCount; i++) {
        AudioObjectID deviceID = deviceIDs[i];

        if (!deviceHasInputStreams(deviceID) || hasDevice(m_coreAudioCaptureDevices, deviceID, CaptureDevice::DeviceType::Microphone))
            continue;

        auto microphoneDevice = CoreAudioCaptureDevice::create(deviceID, CaptureDevice::DeviceType::Microphone, { });
        if (microphoneDevice && isValidCaptureDevice(microphoneDevice.value())) {
            m_coreAudioCaptureDevices.append(WTFMove(microphoneDevice.value()));
            haveDeviceChanges = true;
        }
    }

    // Speakers
    for (size_t i = 0; i < deviceCount; i++) {
        AudioObjectID deviceID = deviceIDs[i];

        if (!deviceHasOutputStreams(deviceID) || hasDevice(m_coreAudioCaptureDevices, deviceID, CaptureDevice::DeviceType::Speaker))
            continue;

        String groupID;
        for (auto relatedDeviceID : CoreAudioCaptureDevice::relatedAudioDeviceIDs(deviceID)) {
            for (auto& device : m_coreAudioCaptureDevices) {
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
                for (auto& existingDevice : m_coreAudioCaptureDevices) {
                    if (existingDevice.label() == device->label() && existingDevice.type() == CaptureDevice::DeviceType::Microphone) {
                        device->setGroupId(existingDevice.persistentId());
                        break;
                    }
                }
            }
            m_coreAudioCaptureDevices.append(WTFMove(device.value()));
            haveDeviceChanges = true;
        }
    }

    for (auto& device : m_coreAudioCaptureDevices) {
        bool isAlive = device.isAlive();
        if (device.enabled() != isAlive) {
            device.setEnabled(isAlive);
            haveDeviceChanges = true;
        }
    }

    if (!haveDeviceChanges)
        return;

    m_captureDevices.clear();
    m_speakerDevices.clear();
    for (auto& device : m_coreAudioCaptureDevices) {
        CaptureDevice captureDevice { device.persistentId(), device.type(), device.label(), device.groupId() };
        captureDevice.setEnabled(device.enabled());
        if (device.type() == CaptureDevice::DeviceType::Microphone)
            m_captureDevices.append(WTFMove(captureDevice));
        else
            m_speakerDevices.append(WTFMove(captureDevice));
    }

    if (notify == NotifyIfDevicesHaveChanged::Notify) {
        deviceChanged();
        CoreAudioCaptureSourceFactory::singleton().devicesChanged(m_captureDevices);
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
