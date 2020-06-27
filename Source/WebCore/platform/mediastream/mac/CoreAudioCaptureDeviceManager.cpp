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
    return m_devices;
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

static bool deviceHasInputStreams(AudioObjectID deviceID)
{
    UInt32 dataSize = 0;
    AudioObjectPropertyAddress address = { kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMaster };
    auto err = AudioObjectGetPropertyDataSize(deviceID, &address, 0, nullptr, &dataSize);
    if (err || !dataSize)
        return false;

    auto bufferList = std::unique_ptr<AudioBufferList>((AudioBufferList*) ::operator new (dataSize));
    memset(bufferList.get(), 0, dataSize);
    err = AudioObjectGetPropertyData(deviceID, &address, 0, nullptr, &dataSize, bufferList.get());

    return !err && bufferList->mNumberBuffers;
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
    return CoreAudioCaptureDevice::create(deviceID);
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

    for (size_t i = 0; i < deviceCount; i++) {
        AudioObjectID deviceID = deviceIDs[i];
        if (!deviceHasInputStreams(deviceID))
            continue;

        if (std::any_of(m_coreAudioCaptureDevices.begin(), m_coreAudioCaptureDevices.end(), [deviceID](auto& device) { return device.deviceID() == deviceID; }))
            continue;

        auto device = CoreAudioCaptureDevice::create(deviceID);
        if (!device || !isValidCaptureDevice(device.value()))
            continue;

        m_coreAudioCaptureDevices.append(WTFMove(device.value()));
        haveDeviceChanges = true;
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

    m_devices = Vector<CaptureDevice>();

    for (auto &device : m_coreAudioCaptureDevices) {
        CaptureDevice captureDevice(device.persistentId(), CaptureDevice::DeviceType::Microphone, device.label());
        captureDevice.setEnabled(device.enabled());
        m_devices.append(captureDevice);
    }

    if (notify == NotifyIfDevicesHaveChanged::Notify) {
        deviceChanged();
        CoreAudioCaptureSourceFactory::singleton().devicesChanged(m_devices);
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
