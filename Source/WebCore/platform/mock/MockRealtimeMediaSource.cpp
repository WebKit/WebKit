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

struct MockDeviceInfo {
    const char* id;
    CaptureDevice::DeviceType type;
    const char* name;
    MockRealtimeMediaSource::MockDevice device;
};

static const HashMap<String, MockDeviceInfo>& deviceMap()
{
    static const auto infoMap = makeNeverDestroyed([] {
        static const MockDeviceInfo devices[] = {
            { "239c24b0-2b15-11e3-8224-0800200c9a66", CaptureDevice::DeviceType::Microphone, "Mock audio device 1", MockRealtimeMediaSource::MockDevice::Microphone1 },
            { "239c24b1-2b15-11e3-8224-0800200c9a66", CaptureDevice::DeviceType::Microphone, "Mock audio device 2", MockRealtimeMediaSource::MockDevice::Microphone2 },

            { "239c24b2-2b15-11e3-8224-0800200c9a66", CaptureDevice::DeviceType::Camera, "Mock video device 1", MockRealtimeMediaSource::MockDevice::Camera1 },
            { "239c24b3-2b15-11e3-8224-0800200c9a66", CaptureDevice::DeviceType::Camera, "Mock video device 2", MockRealtimeMediaSource::MockDevice::Camera2 },

            { "SCREEN-1", CaptureDevice::DeviceType::Screen, "Mock screen device 1", MockRealtimeMediaSource::MockDevice::Screen1 },
            { "SCREEN-2", CaptureDevice::DeviceType::Screen, "Mock screen device 2", MockRealtimeMediaSource::MockDevice::Screen2 },
        };

        HashMap<String, MockDeviceInfo> map;
        for (auto& info : devices)
            map.add(ASCIILiteral(info.id), info);
        return map;
    }());

    return infoMap;
}

std::optional<CaptureDevice> MockRealtimeMediaSource::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& id)
{
    ASSERT(!id.isEmpty());

    auto map = deviceMap();
    auto it = map.find(id);
    if (it != map.end() && it->value.type == type) {
        auto device = CaptureDevice(it->value.id, it->value.type, it->value.name);
        device.setEnabled(true);
        return WTFMove(device);
    }

    return std::nullopt;
}

Vector<CaptureDevice>& MockRealtimeMediaSource::audioDevices()
{
    static auto info = makeNeverDestroyed([] {
        Vector<CaptureDevice> vector;

        auto captureDevice = captureDeviceWithPersistentID(CaptureDevice::DeviceType::Microphone, ASCIILiteral("239c24b0-2b15-11e3-8224-0800200c9a66"));
        ASSERT(captureDevice);
        vector.append(WTFMove(captureDevice.value()));

        captureDevice = captureDeviceWithPersistentID(CaptureDevice::DeviceType::Microphone, ASCIILiteral("239c24b1-2b15-11e3-8224-0800200c9a66"));
        ASSERT(captureDevice);
        vector.append(WTFMove(captureDevice.value()));

        return vector;
    }());
    return info;
}

Vector<CaptureDevice>& MockRealtimeMediaSource::videoDevices()
{
    static auto info = makeNeverDestroyed([] {
        Vector<CaptureDevice> vector;

        auto captureDevice = captureDeviceWithPersistentID(CaptureDevice::DeviceType::Camera, ASCIILiteral("239c24b2-2b15-11e3-8224-0800200c9a66"));
        ASSERT(captureDevice);
        vector.append(WTFMove(captureDevice.value()));

        captureDevice = captureDeviceWithPersistentID(CaptureDevice::DeviceType::Camera, ASCIILiteral("239c24b3-2b15-11e3-8224-0800200c9a66"));
        ASSERT(captureDevice);
        vector.append(WTFMove(captureDevice.value()));

        return vector;
    }());
    return info;
}

Vector<CaptureDevice>& MockRealtimeMediaSource::displayDevices()
{
    static auto devices = makeNeverDestroyed([] {
        Vector<CaptureDevice> vector;

        auto captureDevice = captureDeviceWithPersistentID(CaptureDevice::DeviceType::Screen, ASCIILiteral("SCREEN-1"));
        ASSERT(captureDevice);
        vector.append(WTFMove(captureDevice.value()));

        captureDevice = captureDeviceWithPersistentID(CaptureDevice::DeviceType::Screen, ASCIILiteral("SCREEN-2"));
        ASSERT(captureDevice);
        vector.append(WTFMove(captureDevice.value()));

        return vector;
    }());

    return devices;
}

MockRealtimeMediaSource::MockRealtimeMediaSource(const String& id, RealtimeMediaSource::Type type, const String& name)
    : RealtimeMediaSource(id, type, name)
{
    ASSERT(type != RealtimeMediaSource::Type::None);

    auto map = deviceMap();
    auto it = map.find(id);
    ASSERT(it != map.end());

    m_device = it->value.device;
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
