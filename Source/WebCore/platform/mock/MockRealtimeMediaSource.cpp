/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MockRealtimeMediaSource.h"

#if ENABLE(MEDIA_STREAM)

#include "CaptureDevice.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "NotImplemented.h"
#include "RealtimeMediaSourceSettings.h"
#include <math.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringView.h>

namespace WebCore {

static inline Vector<MockMediaDevice> defaultDevices()
{
    return Vector<MockMediaDevice> {
        MockMediaDevice { "239c24b0-2b15-11e3-8224-0800200c9a66"_s, "Mock audio device 1"_s, MockMicrophoneProperties { 44100 } },
        MockMediaDevice { "239c24b1-2b15-11e3-8224-0800200c9a66"_s, "Mock audio device 2"_s, MockMicrophoneProperties { 48000 } },

        MockMediaDevice { "239c24b2-2b15-11e3-8224-0800200c9a66"_s, "Mock video device 1"_s, MockCameraProperties { 30, RealtimeMediaSourceSettings::VideoFacingMode::User, Color::black } },
        MockMediaDevice { "239c24b3-2b15-11e3-8224-0800200c9a66"_s, "Mock video device 2"_s, MockCameraProperties { 15, RealtimeMediaSourceSettings::VideoFacingMode::Environment, Color::darkGray } },

        MockMediaDevice { "SCREEN-1"_s, "Mock screen device 1"_s, MockDisplayProperties { 30, Color::lightGray } },
        MockMediaDevice { "SCREEN-2"_s, "Mock screen device 2"_s, MockDisplayProperties { 10, Color::yellow } }
    };
}

static Vector<MockMediaDevice>& devices()
{
    static auto devices = makeNeverDestroyed([] {
        return defaultDevices();
    }());
    return devices;
}

static HashMap<String, MockMediaDevice>& deviceMap()
{
    static auto map = makeNeverDestroyed([] {
        HashMap<String, MockMediaDevice> map;
        for (auto& device : devices())
            map.add(device.persistentId, device);

        return map;
    }());
    return map;
}

static inline Vector<CaptureDevice>& deviceListForDevice(const MockMediaDevice& device)
{
    if (device.isMicrophone())
        return MockRealtimeAudioSource::audioDevices();
    if (device.isCamera())
        return MockRealtimeAudioSource::videoDevices();

    ASSERT(device.isDisplay());
    return MockRealtimeAudioSource::displayDevices();
}

void MockRealtimeMediaSource::createCaptureDevice(const MockMediaDevice& device)
{
    deviceListForDevice(device).append(captureDeviceWithPersistentID(device.type(), device.persistentId).value());
}

void MockRealtimeMediaSource::resetDevices()
{
    setDevices(defaultDevices());
}

void MockRealtimeMediaSource::setDevices(Vector<MockMediaDevice>&& newMockDevices)
{
    audioDevices().clear();
    videoDevices().clear();
    displayDevices().clear();

    auto& mockDevices = devices();
    mockDevices = WTFMove(newMockDevices);

    auto& map = deviceMap();
    map.clear();

    for (const auto& device : mockDevices) {
        map.add(device.persistentId, device);
        createCaptureDevice(device);
    }
}

void MockRealtimeMediaSource::addDevice(const MockMediaDevice& device)
{
    devices().append(device);
    deviceMap().set(device.persistentId, device);
    createCaptureDevice(device);
}

void MockRealtimeMediaSource::removeDevice(const String& persistentId)
{
    auto& map = deviceMap();
    auto iterator = map.find(persistentId);
    if (iterator == map.end())
        return;

    devices().removeFirstMatching([&persistentId](const auto& device) {
        return device.persistentId == persistentId;
    });

    deviceListForDevice(iterator->value).removeFirstMatching([&persistentId](const auto& device) {
        return device.persistentId() == persistentId;
    });

    map.remove(iterator);
}

std::optional<CaptureDevice> MockRealtimeMediaSource::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& id)
{
    ASSERT(!id.isEmpty());

    auto& map = deviceMap();
    auto iterator = map.find(id);
    if (iterator == map.end() || iterator->value.type() != type)
        return std::nullopt;

    CaptureDevice device { iterator->value.persistentId, type, iterator->value.label };
    device.setEnabled(true);
    return WTFMove(device);
}

Vector<CaptureDevice>& MockRealtimeMediaSource::audioDevices()
{
    static auto audioDevices = makeNeverDestroyed([] {
        Vector<CaptureDevice> audioDevices;
        for (const auto& device : devices()) {
            if (device.type() == CaptureDevice::DeviceType::Microphone)
                audioDevices.append(captureDeviceWithPersistentID(CaptureDevice::DeviceType::Microphone, device.persistentId).value());
        }
        return audioDevices;
    }());
    return audioDevices;
}

Vector<CaptureDevice>& MockRealtimeMediaSource::videoDevices()
{
    static auto videoDevices = makeNeverDestroyed([] {
        Vector<CaptureDevice> videoDevices;
        for (const auto& device : devices()) {
            if (device.type() == CaptureDevice::DeviceType::Camera)
                videoDevices.append(captureDeviceWithPersistentID(CaptureDevice::DeviceType::Camera, device.persistentId).value());
        }
        return videoDevices;
    }());
    return videoDevices;
}

Vector<CaptureDevice>& MockRealtimeMediaSource::displayDevices()
{
    static auto displayDevices = makeNeverDestroyed([] {
        Vector<CaptureDevice> displayDevices;
        for (const auto& device : devices()) {
            if (device.type() == CaptureDevice::DeviceType::Screen)
                displayDevices.append(captureDeviceWithPersistentID(CaptureDevice::DeviceType::Screen, device.persistentId).value());
        }
        return displayDevices;
    }());

    return displayDevices;
}

MockRealtimeMediaSource::MockRealtimeMediaSource(const String& id, RealtimeMediaSource::Type type, const String& name)
    : RealtimeMediaSource(id, type, name)
    , m_device(deviceMap().get(id))
{
    ASSERT(type != RealtimeMediaSource::Type::None);
    setPersistentID(String(id));
}

void MockRealtimeMediaSource::initializeCapabilities()
{
    m_capabilities = std::make_unique<RealtimeMediaSourceCapabilities>(supportedConstraints());
    m_capabilities->setDeviceId(id());
    initializeCapabilities(*m_capabilities.get());
}

const RealtimeMediaSourceCapabilities& MockRealtimeMediaSource::capabilities() const
{
    if (!m_capabilities)
        const_cast<MockRealtimeMediaSource&>(*this).initializeCapabilities();
    return *m_capabilities;
}

void MockRealtimeMediaSource::initializeSettings()
{
    if (m_currentSettings.deviceId().isEmpty()) {
        m_currentSettings.setSupportedConstraints(supportedConstraints());
        m_currentSettings.setDeviceId(id());
        m_currentSettings.setLabel(name());
    }

    updateSettings(m_currentSettings);
}

const RealtimeMediaSourceSettings& MockRealtimeMediaSource::settings() const
{
    const_cast<MockRealtimeMediaSource&>(*this).initializeSettings();
    return m_currentSettings;
}

RealtimeMediaSourceSupportedConstraints& MockRealtimeMediaSource::supportedConstraints()
{
    if (m_supportedConstraints.supportsDeviceId())
        return m_supportedConstraints;

    m_supportedConstraints.setSupportsDeviceId(true);
    initializeSupportedConstraints(m_supportedConstraints);

    return m_supportedConstraints;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
