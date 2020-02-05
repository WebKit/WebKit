/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2018 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
#include "MockRealtimeMediaSourceCenter.h"

#if ENABLE(MEDIA_STREAM)

#include "CaptureDevice.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "MockRealtimeAudioSource.h"
#include "MockRealtimeVideoSource.h"
#include "NotImplemented.h"
#include "RealtimeMediaSourceSettings.h"
#include <math.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringView.h>

#if PLATFORM(COCOA)
#include "CoreAudioCaptureSource.h"
#endif

namespace WebCore {

static inline Vector<MockMediaDevice> defaultDevices()
{
    return Vector<MockMediaDevice> {
        MockMediaDevice { "239c24b0-2b15-11e3-8224-0800200c9a66"_s, "Mock audio device 1"_s, MockMicrophoneProperties { 44100 } },
        MockMediaDevice { "239c24b1-2b15-11e3-8224-0800200c9a66"_s, "Mock audio device 2"_s, MockMicrophoneProperties { 48000 } },

        MockMediaDevice { "239c24b2-2b15-11e3-8224-0800200c9a66"_s, "Mock video device 1"_s,
            MockCameraProperties {
                30,
                RealtimeMediaSourceSettings::VideoFacingMode::User, {
                    { { 1280, 720 }, { { 30, 30}, { 27.5, 27.5}, { 25, 25}, { 22.5, 22.5}, { 20, 20}, { 17.5, 17.5}, { 15, 15}, { 12.5, 12.5}, { 10, 10}, { 7.5, 7.5}, { 5, 5} } },
                    { { 640, 480 },  { { 30, 30}, { 27.5, 27.5}, { 25, 25}, { 22.5, 22.5}, { 20, 20}, { 17.5, 17.5}, { 15, 15}, { 12.5, 12.5}, { 10, 10}, { 7.5, 7.5}, { 5, 5} } },
                    { { 112, 112 },  { { 30, 30}, { 27.5, 27.5}, { 25, 25}, { 22.5, 22.5}, { 20, 20}, { 17.5, 17.5}, { 15, 15}, { 12.5, 12.5}, { 10, 10}, { 7.5, 7.5}, { 5, 5} } },
                },
                Color::black,
            } },

        MockMediaDevice { "239c24b3-2b15-11e3-8224-0800200c9a66"_s, "Mock video device 2"_s,
            MockCameraProperties {
                15,
                RealtimeMediaSourceSettings::VideoFacingMode::Environment, {
                    { { 3840, 2160 }, { { 2, 30 } } },
                    { { 1920, 1080 }, { { 2, 30 } } },
                    { { 1280, 720 },  { { 3, 120 } } },
                    { { 960, 540 },   { { 3, 60 } } },
                    { { 640, 480 },   { { 2, 30 } } },
                    { { 352, 288 },   { { 2, 30 } } },
                    { { 320, 240 },   { { 2, 30 } } },
                    { { 160, 120 },   { { 2, 30 } } },
                },
                Color::darkGray,
            } },

        MockMediaDevice { "SCREEN-1"_s, "Mock screen device 1"_s, MockDisplayProperties { CaptureDevice::DeviceType::Screen, Color::lightGray, { 3840, 2160 } } },
        MockMediaDevice { "SCREEN-2"_s, "Mock screen device 2"_s, MockDisplayProperties { CaptureDevice::DeviceType::Screen, Color::yellow, { 1920, 1080 } } },

        MockMediaDevice { "WINDOW-2"_s, "Mock window 1"_s, MockDisplayProperties { CaptureDevice::DeviceType::Screen, SimpleColor { 0xfff1b5 }, { 640, 480 } } },
        MockMediaDevice { "WINDOW-2"_s, "Mock window 2"_s, MockDisplayProperties { CaptureDevice::DeviceType::Screen, SimpleColor { 0xffd0b5 }, { 1280, 600 } } },
    };
}

class MockRealtimeVideoSourceFactory : public VideoCaptureFactory {
public:
    CaptureSourceOrError createVideoCaptureSource(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints) final
    {
        ASSERT(device.type() == CaptureDevice::DeviceType::Camera);
        if (!MockRealtimeMediaSourceCenter::captureDeviceWithPersistentID(CaptureDevice::DeviceType::Camera, device.persistentId()))
            return { "Unable to find mock camera device with given persistentID"_s };

        return MockRealtimeVideoSource::create(String { device.persistentId() }, String { device.label() }, WTFMove(hashSalt), constraints);
    }

private:
#if PLATFORM(IOS_FAMILY)
    void setVideoCapturePageState(bool interrupted, bool pageMuted) final
    {
        if (activeSource())
            activeSource()->setInterrupted(interrupted, pageMuted);
    }
#endif
    CaptureDeviceManager& videoCaptureDeviceManager() final { return MockRealtimeMediaSourceCenter::singleton().videoCaptureDeviceManager(); }
};

class MockRealtimeDisplaySourceFactory : public DisplayCaptureFactory {
public:
    CaptureSourceOrError createDisplayCaptureSource(const CaptureDevice& device, const MediaConstraints* constraints) final
    {
        if (!MockRealtimeMediaSourceCenter::captureDeviceWithPersistentID(device.type(), device.persistentId()))
            return { "Unable to find mock  display device with given persistentID"_s };

        switch (device.type()) {
        case CaptureDevice::DeviceType::Screen:
        case CaptureDevice::DeviceType::Window:
            return MockRealtimeVideoSource::create(String { device.persistentId() }, String { device.label() }, String { }, constraints);
            break;
        case CaptureDevice::DeviceType::Microphone:
        case CaptureDevice::DeviceType::Camera:
        case CaptureDevice::DeviceType::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }

        return { };
    }
private:
    CaptureDeviceManager& displayCaptureDeviceManager() final { return MockRealtimeMediaSourceCenter::singleton().displayCaptureDeviceManager(); }
};

class MockRealtimeAudioSourceFactory : public AudioCaptureFactory {
public:
    CaptureSourceOrError createAudioCaptureSource(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints) final
    {
        ASSERT(device.type() == CaptureDevice::DeviceType::Microphone);
        if (!MockRealtimeMediaSourceCenter::captureDeviceWithPersistentID(CaptureDevice::DeviceType::Microphone, device.persistentId()))
            return { "Unable to find mock microphone device with given persistentID"_s };

        return MockRealtimeAudioSource::create(String { device.persistentId() }, String { device.label() }, WTFMove(hashSalt), constraints);
    }
private:
#if PLATFORM(IOS_FAMILY)
    void setAudioCapturePageState(bool interrupted, bool pageMuted) final { CoreAudioCaptureSourceFactory::singleton().setAudioCapturePageState(interrupted, pageMuted); }
#endif
    CaptureDeviceManager& audioCaptureDeviceManager() final { return MockRealtimeMediaSourceCenter::singleton().audioCaptureDeviceManager(); }
};

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
        return MockRealtimeMediaSourceCenter::audioDevices();
    if (device.isCamera())
        return MockRealtimeMediaSourceCenter::videoDevices();

    ASSERT(device.isDisplay());
    return MockRealtimeMediaSourceCenter::displayDevices();
}

MockRealtimeMediaSourceCenter& MockRealtimeMediaSourceCenter::singleton()
{
    static NeverDestroyed<MockRealtimeMediaSourceCenter> center;
    return center;
}

void MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(bool enabled)
{
    MockRealtimeMediaSourceCenter& mock = singleton();

    if (mock.m_isEnabled == enabled)
        return;

    mock.m_isEnabled = enabled;
    RealtimeMediaSourceCenter& center = RealtimeMediaSourceCenter::singleton();

    if (mock.m_isEnabled) {
        if (mock.m_isMockAudioCaptureEnabled)
            center.setAudioCaptureFactory(mock.audioCaptureFactory());
        if (mock.m_isMockVideoCaptureEnabled)
            center.setVideoCaptureFactory(mock.videoCaptureFactory());
        if (mock.m_isMockDisplayCaptureEnabled)
            center.setDisplayCaptureFactory(mock.displayCaptureFactory());
        return;
    }

    if (mock.m_isMockAudioCaptureEnabled)
        center.unsetAudioCaptureFactory(mock.audioCaptureFactory());
    if (mock.m_isMockVideoCaptureEnabled)
        center.unsetVideoCaptureFactory(mock.videoCaptureFactory());
    if (mock.m_isMockDisplayCaptureEnabled)
        center.unsetDisplayCaptureFactory(mock.displayCaptureFactory());
}

bool MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled()
{
    return singleton().m_isEnabled;
}

static void createCaptureDevice(const MockMediaDevice& device)
{
    deviceListForDevice(device).append(MockRealtimeMediaSourceCenter::captureDeviceWithPersistentID(device.type(), device.persistentId).value());
}

void MockRealtimeMediaSourceCenter::resetDevices()
{
    setDevices(defaultDevices());
    RealtimeMediaSourceCenter::singleton().captureDevicesChanged();
}

void MockRealtimeMediaSourceCenter::setDevices(Vector<MockMediaDevice>&& newMockDevices)
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
    RealtimeMediaSourceCenter::singleton().captureDevicesChanged();
}

void MockRealtimeMediaSourceCenter::addDevice(const MockMediaDevice& device)
{
    devices().append(device);
    deviceMap().set(device.persistentId, device);
    createCaptureDevice(device);
    RealtimeMediaSourceCenter::singleton().captureDevicesChanged();
}

void MockRealtimeMediaSourceCenter::removeDevice(const String& persistentId)
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
    RealtimeMediaSourceCenter::singleton().captureDevicesChanged();
}

Optional<MockMediaDevice> MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(const String& id)
{
    ASSERT(!id.isEmpty());

    auto& map = deviceMap();
    auto iterator = map.find(id);
    if (iterator == map.end())
        return WTF::nullopt;

    return iterator->value;
}

Optional<CaptureDevice> MockRealtimeMediaSourceCenter::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& id)
{
    ASSERT(!id.isEmpty());

    auto& map = deviceMap();
    auto iterator = map.find(id);
    if (iterator == map.end() || iterator->value.type() != type)
        return WTF::nullopt;

    CaptureDevice device { iterator->value.persistentId, type, iterator->value.label };
    device.setEnabled(true);
    return device;
}

Vector<CaptureDevice>& MockRealtimeMediaSourceCenter::audioDevices()
{
    static auto audioDevices = makeNeverDestroyed([] {
        Vector<CaptureDevice> audioDevices;
        for (const auto& device : devices()) {
            if (device.isMicrophone())
                audioDevices.append(captureDeviceWithPersistentID(CaptureDevice::DeviceType::Microphone, device.persistentId).value());
        }
        return audioDevices;
    }());

    return audioDevices;
}

Vector<CaptureDevice>& MockRealtimeMediaSourceCenter::videoDevices()
{
    static auto videoDevices = makeNeverDestroyed([] {
        Vector<CaptureDevice> videoDevices;
        for (const auto& device : devices()) {
            if (device.isCamera())
                videoDevices.append(captureDeviceWithPersistentID(CaptureDevice::DeviceType::Camera, device.persistentId).value());
        }
        return videoDevices;
    }());

    return videoDevices;
}

Vector<CaptureDevice>& MockRealtimeMediaSourceCenter::displayDevices()
{
    static auto displayDevices = makeNeverDestroyed([] {
        Vector<CaptureDevice> displayDevices;
        for (const auto& device : devices()) {
            if (device.isDisplay())
                displayDevices.append(captureDeviceWithPersistentID(CaptureDevice::DeviceType::Screen, device.persistentId).value());
        }
        return displayDevices;
    }());

    return displayDevices;
}

AudioCaptureFactory& MockRealtimeMediaSourceCenter::audioCaptureFactory()
{
    static NeverDestroyed<MockRealtimeAudioSourceFactory> factory;
    return factory.get();
}

VideoCaptureFactory& MockRealtimeMediaSourceCenter::videoCaptureFactory()
{
    static NeverDestroyed<MockRealtimeVideoSourceFactory> factory;
    return factory.get();
}

DisplayCaptureFactory& MockRealtimeMediaSourceCenter::displayCaptureFactory()
{
    static NeverDestroyed<MockRealtimeDisplaySourceFactory> factory;
    return factory.get();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
