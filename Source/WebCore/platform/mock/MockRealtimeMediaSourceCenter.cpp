/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2023 Apple Inc.  All rights reserved.
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
#include "DisplayCaptureSourceCocoa.h"
#include "MockAudioSharedUnit.h"
#include "MockRealtimeVideoSourceMac.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "CoreAudioCaptureSourceIOS.h"
#endif

#if USE(GSTREAMER)
#include "MockDisplayCaptureSourceGStreamer.h"
#include "MockRealtimeVideoSourceGStreamer.h"
#endif

namespace WebCore {
class MockDisplayCapturer;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::MockDisplayCapturer> : std::true_type { };
}

namespace WebCore {

static inline Vector<MockMediaDevice> defaultDevices()
{
    return Vector<MockMediaDevice> {
        MockMediaDevice { "239c24b0-2b15-11e3-8224-0800200c9a66"_s, "Mock audio device 1"_s, { }, MockMicrophoneProperties { 44100 } },
        MockMediaDevice { "239c24b1-2b15-11e3-8224-0800200c9a66"_s, "Mock audio device 2"_s, { },  MockMicrophoneProperties { 48000 } },
        MockMediaDevice { "239c24b1-3b15-11e3-8224-0800200c9a66"_s, "Mock audio device 3"_s, { },  MockMicrophoneProperties { 96000 } },

        MockMediaDevice { "239c24b0-2b15-11e3-8224-0800200c9a67"_s, "Mock speaker device 1"_s, { },  MockSpeakerProperties { "239c24b0-2b15-11e3-8224-0800200c9a66"_s, 44100 } },
        MockMediaDevice { "239c24b1-2b15-11e3-8224-0800200c9a67"_s, "Mock speaker device 2"_s, { },  MockSpeakerProperties { "239c24b1-2b15-11e3-8224-0800200c9a66"_s, 48000 } },
        MockMediaDevice { "239c24b2-2b15-11e3-8224-0800200c9a67"_s, "Mock speaker device 3"_s, { },  MockSpeakerProperties { String { }, 48000 } },

        MockMediaDevice { "239c24b2-2b15-11e3-8224-0800200c9a66"_s, "Mock video device 1"_s, { },
            MockCameraProperties {
                30,
                VideoFacingMode::User, {
                    { { 2560, 1440 }, { { 10, 10 }, { 7.5, 7.5 }, { 5, 5 } }, 1, 1, true },
                    { { 1280, 720 }, { { 30, 30 }, { 27.5, 27.5 }, { 25, 25 }, { 22.5, 22.5 }, { 20, 20 }, { 17.5, 17.5 }, { 15, 15 }, { 12.5, 12.5 }, { 10, 10 }, { 7.5, 7.5 }, { 5, 5 } }, 1, 1, true },
                    { { 640, 480 },  { { 30, 30 }, { 27.5, 27.5 }, { 25, 25 }, { 22.5, 22.5 }, { 20, 20 }, { 17.5, 17.5 }, { 15, 15 }, { 12.5, 12.5 }, { 10, 10 }, { 7.5, 7.5 }, { 5, 5 } }, 1, 1, true },
                    { { 112, 112 },  { { 30, 30 }, { 27.5, 27.5 }, { 25, 25 }, { 22.5, 22.5 }, { 20, 20 }, { 17.5, 17.5 }, { 15, 15 }, { 12.5, 12.5 }, { 10, 10 }, { 7.5, 7.5 }, { 5, 5 } }, 1, 1, true },
                },
                Color::black,
                { }, // whiteBalanceModes
                false, // supportsTorch
                false, // background blur enabled
            } },

        MockMediaDevice { "239c24b3-2b15-11e3-8224-0800200c9a66"_s, "Mock video device 2"_s, { },
            MockCameraProperties {
                15,
                VideoFacingMode::Environment, {
                    { { 3840, 2160 }, { { 2, 30 } }, 1, 4, false },
                    { { 1920, 1080 }, { { 2, 30 } }, 1, 4, true },
                    { { 1280, 720 },  { { 3, 120 } }, 1, 4, false },
                    { { 960, 540 },   { { 3, 60 } }, 1, 4, false },
                    { { 640, 480 },   { { 2, 30 } }, 1, 4, false },
                    { { 352, 288 },   { { 2, 30 } }, 1, 4, false },
                    { { 320, 240 },   { { 2, 30 } }, 1, 4, false },
                    { { 160, 120 },   { { 2, 30 } }, 1, 4, false },
                },
                Color::darkGray,
                { MeteringMode::Manual, MeteringMode::SingleShot, MeteringMode::Continuous },
                true,
                true, // background blur enabled
            } },

        MockMediaDevice { "SCREEN-1"_s, "Mock screen device 1"_s, { }, MockDisplayProperties { CaptureDevice::DeviceType::Screen, Color::lightGray, { 1920, 1080 } } },
        MockMediaDevice { "SCREEN-2"_s, "Mock screen device 2"_s, { }, MockDisplayProperties { CaptureDevice::DeviceType::Screen, Color::yellow, { 3840, 2160 } } },

        MockMediaDevice { "WINDOW-1"_s, "Mock window device 1"_s, { }, MockDisplayProperties { CaptureDevice::DeviceType::Window, SRGBA<uint8_t> { 255, 241, 181 }, { 640, 480 } } },
        MockMediaDevice { "WINDOW-2"_s, "Mock window device 2"_s, { }, MockDisplayProperties { CaptureDevice::DeviceType::Window, SRGBA<uint8_t> { 255, 208, 181 }, { 1280, 600 } } },
    };
}

class MockRealtimeVideoSourceFactory : public VideoCaptureFactory {
public:
    CaptureSourceOrError createVideoCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier) final
    {
        ASSERT(device.type() == CaptureDevice::DeviceType::Camera);
        if (!MockRealtimeMediaSourceCenter::captureDeviceWithPersistentID(CaptureDevice::DeviceType::Camera, device.persistentId()))
            return CaptureSourceOrError({ "Unable to find mock camera device with given persistentID"_s, MediaAccessDenialReason::PermissionDenied });

        auto mock = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(device.persistentId());
        ASSERT(mock);
        if (mock->flags.contains(MockMediaDevice::Flag::Invalid))
            return CaptureSourceOrError({ "Invalid mock camera device"_s, MediaAccessDenialReason::PermissionDenied });

        return MockRealtimeVideoSource::create(String { device.persistentId() }, AtomString { device.label() }, WTFMove(hashSalts), constraints, pageIdentifier);
    }

private:
    CaptureDeviceManager& videoCaptureDeviceManager() final { return MockRealtimeMediaSourceCenter::singleton().videoCaptureDeviceManager(); }
};

#if PLATFORM(COCOA)
class MockDisplayCapturer final
    : public DisplayCaptureSourceCocoa::Capturer
    , public CanMakeWeakPtr<MockDisplayCapturer> {
public:
    MockDisplayCapturer(const CaptureDevice&, PageIdentifier);

    void triggerMockCaptureConfigurationChange();

private:
    bool start() final;
    void stop() final  { m_source->stop(); }
    DisplayCaptureSourceCocoa::DisplayFrameType generateFrame() final;
    DisplaySurfaceType surfaceType() const final { return DisplaySurfaceType::Monitor; }
    void commitConfiguration(const RealtimeMediaSourceSettings&) final;
    CaptureDevice::DeviceType deviceType() const final { return CaptureDevice::DeviceType::Screen; }
    IntSize intrinsicSize() const final;
#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final { return "MockDisplayCapturer"_s; }
#endif
    Ref<MockRealtimeVideoSource> m_source;
    RealtimeMediaSourceSettings m_settings;
};

MockDisplayCapturer::MockDisplayCapturer(const CaptureDevice& device, PageIdentifier pageIdentifier)
    : m_source(MockRealtimeVideoSourceMac::createForMockDisplayCapturer(String { device.persistentId() }, AtomString { device.label() }, MediaDeviceHashSalts { "persistent"_s, "ephemeral"_s }, pageIdentifier))
{
}

bool MockDisplayCapturer::start()
{
    ASSERT(m_settings.frameRate());
    m_source->start();
    return true;
}

void MockDisplayCapturer::commitConfiguration(const RealtimeMediaSourceSettings& settings)
{
    // FIXME: Update m_source width, height and frameRate according settings
    m_settings = settings;
}

DisplayCaptureSourceCocoa::DisplayFrameType MockDisplayCapturer::generateFrame()
{
    if (auto* imageBuffer = m_source->imageBuffer())
        return imageBuffer->copyNativeImage();
    return { };
}

IntSize MockDisplayCapturer::intrinsicSize() const
{
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(m_source->persistentID());
    ASSERT(device);
    if (!device)
        return { };

    ASSERT(device->isDisplay());
    if (!device->isDisplay())
        return { };

    auto& properties = std::get<MockDisplayProperties>(device->properties);
    return properties.defaultSize;
}

void MockDisplayCapturer::triggerMockCaptureConfigurationChange()
{
    auto deviceId = m_source->persistentID();
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceId.startsWith("WINDOW"_s) ? "WINDOW-2"_s : "SCREEN-2"_s);
    ASSERT(device);
    if (!device)
        return;

    bool isStarted = m_source->isProducingData();
    auto pageIdentifier = m_source->pageIdentifier();
    m_source = MockRealtimeVideoSourceMac::createForMockDisplayCapturer(String { device->persistentId }, AtomString { device->label }, MediaDeviceHashSalts { "persistent"_s, "ephemeral"_s }, pageIdentifier);
    if (isStarted)
        m_source->start();

    configurationChanged();
}
#endif // PLATFORM(COCOA)

class MockRealtimeDisplaySourceFactory : public DisplayCaptureFactory {
public:
    static MockRealtimeDisplaySourceFactory& singleton();

    CaptureSourceOrError createDisplayCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier) final
    {
        if (!MockRealtimeMediaSourceCenter::captureDeviceWithPersistentID(device.type(), device.persistentId()))
            return CaptureSourceOrError({ "Unable to find mock display device with given persistentID"_s, MediaAccessDenialReason::PermissionDenied });

        switch (device.type()) {
        case CaptureDevice::DeviceType::Screen:
        case CaptureDevice::DeviceType::Window: {
#if PLATFORM(COCOA)
            auto capturer = makeUniqueRef<MockDisplayCapturer>(device, pageIdentifier);
            m_capturer = capturer.get();
            return DisplayCaptureSourceCocoa::create(UniqueRef<DisplayCaptureSourceCocoa::Capturer>(WTFMove(capturer)), device, WTFMove(hashSalts), constraints, pageIdentifier);
#elif USE(GSTREAMER)
            return MockDisplayCaptureSourceGStreamer::create(device, WTFMove(hashSalts), constraints, pageIdentifier);
#else
            return MockRealtimeVideoSource::create(String { device.persistentId() }, AtomString { device.label() }, WTFMove(hashSalts), constraints, pageIdentifier);
#endif
            break;
        }
        case CaptureDevice::DeviceType::Microphone:
        case CaptureDevice::DeviceType::Speaker:
        case CaptureDevice::DeviceType::Camera:
        case CaptureDevice::DeviceType::SystemAudio:
        case CaptureDevice::DeviceType::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }

        return { };
    }

#if PLATFORM(COCOA)
    WeakPtr<MockDisplayCapturer> latestCapturer() { return m_capturer; }
#endif

private:
    DisplayCaptureManager& displayCaptureDeviceManager() final { return MockRealtimeMediaSourceCenter::singleton().displayCaptureDeviceManager(); }
#if PLATFORM(COCOA)
    WeakPtr<MockDisplayCapturer> m_capturer;
#endif
};

class MockRealtimeAudioSourceFactory final : public AudioCaptureFactory
{
public:
    CaptureSourceOrError createAudioCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier) final
    {
        ASSERT(device.type() == CaptureDevice::DeviceType::Microphone || device.type() == CaptureDevice::DeviceType::Speaker);
        if (!MockRealtimeMediaSourceCenter::captureDeviceWithPersistentID(device.type(), device.persistentId()))
            return CaptureSourceOrError({ "Unable to find mock microphone or speaker device with given persistentID"_s, MediaAccessDenialReason::PermissionDenied });

        auto mock = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(device.persistentId());
        ASSERT(mock);
        if (mock->flags.contains(MockMediaDevice::Flag::Invalid))
            return CaptureSourceOrError({ "Invalid mock microphone or speaker device"_s, MediaAccessDenialReason::PermissionDenied });

        return MockRealtimeAudioSource::create(String { device.persistentId() }, AtomString { device.label() }, WTFMove(hashSalts), constraints, pageIdentifier);
    }
private:
    CaptureDeviceManager& audioCaptureDeviceManager() final { return MockRealtimeMediaSourceCenter::singleton().audioCaptureDeviceManager(); }
    const Vector<CaptureDevice>& speakerDevices() const final { return MockRealtimeMediaSourceCenter::speakerDevices(); }
};

static Vector<MockMediaDevice>& devices()
{
    static NeverDestroyed devices = defaultDevices();
    return devices;
}

static HashMap<String, MockMediaDevice>& deviceMap()
{
    static NeverDestroyed map = [] {
        HashMap<String, MockMediaDevice> map;
        for (auto& device : devices())
            map.add(device.persistentId, device);
        return map;
    }();
    return map;
}

static inline Vector<CaptureDevice>& deviceListForDevice(const MockMediaDevice& device)
{
    if (device.isMicrophone())
        return MockRealtimeMediaSourceCenter::microphoneDevices();
    if (device.isSpeaker())
        return MockRealtimeMediaSourceCenter::speakerDevices();
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

static CaptureDevice toCaptureDevice(const MockMediaDevice& device)
{
    auto captureDevice = device.captureDevice();
    captureDevice.setEnabled(true);
    captureDevice.setIsMockDevice(true);

    return captureDevice;
}

static void createMockDevice(const MockMediaDevice& device, bool isDefault)
{
    if (isDefault)
        deviceListForDevice(device).insert(0, toCaptureDevice(device));
    else
        deviceListForDevice(device).append(toCaptureDevice(device));
}

void MockRealtimeMediaSourceCenter::resetDevices()
{
    setDevices(defaultDevices());
    RealtimeMediaSourceCenter::singleton().captureDevicesChanged();
}

void MockRealtimeMediaSourceCenter::setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted)
{
    MockRealtimeVideoSource::setIsInterrupted(isCameraInterrupted);
    MockRealtimeAudioSource::setIsInterrupted(isMicrophoneInterrupted);
}

void MockRealtimeMediaSourceCenter::triggerMockCaptureConfigurationChange(bool forMicrophone, bool forDisplay)
{
#if PLATFORM(COCOA)
    if (forMicrophone) {
        auto devices = audioCaptureDeviceManager().captureDevices();
        if (devices.size() > 1) {
            MockAudioSharedUnit::increaseBufferSize();
            MockAudioSharedUnit::singleton().handleNewCurrentMicrophoneDevice(WTFMove(devices[1]));
        }
    }
    if (forDisplay) {
        if (auto capturer = MockRealtimeDisplaySourceFactory::singleton().latestCapturer())
            capturer->triggerMockCaptureConfigurationChange();
    }
#else
    UNUSED_PARAM(forMicrophone);
    UNUSED_PARAM(forDisplay);
#endif
}

void MockRealtimeMediaSourceCenter::setDevices(Vector<MockMediaDevice>&& newMockDevices)
{
    microphoneDevices().clear();
    speakerDevices().clear();
    videoDevices().clear();
    displayDevices().clear();

    auto& mockDevices = devices();
    for (auto& device : mockDevices) {
        auto& persistentId = device.persistentId;
        if (!newMockDevices.containsIf([&persistentId](auto& newDevice) -> bool {
            return newDevice.persistentId == persistentId;
        }))
            RealtimeMediaSourceCenter::singleton().captureDeviceWillBeRemoved(persistentId);
    }
    mockDevices = WTFMove(newMockDevices);

    auto& map = deviceMap();
    map.clear();

    for (const auto& device : mockDevices) {
        map.add(device.persistentId, device);
        createMockDevice(device, false);
    }
    RealtimeMediaSourceCenter::singleton().captureDevicesChanged();
}

static bool shouldBeDefaultDevice(const MockMediaDevice& device)
{
    auto* cameraProperties = device.cameraProperties();
    return cameraProperties && cameraProperties->facingMode == VideoFacingMode::Unknown;
}

void MockRealtimeMediaSourceCenter::addDevice(const MockMediaDevice& device)
{
    bool isDefault = shouldBeDefaultDevice(device);
    if (isDefault)
        devices().insert(0, device);
    else
        devices().append(device);
    deviceMap().set(device.persistentId, device);
    createMockDevice(device, isDefault);
    RealtimeMediaSourceCenter::singleton().captureDevicesChanged();
}

void MockRealtimeMediaSourceCenter::removeDevice(const String& persistentId)
{
    auto& map = deviceMap();
    auto iterator = map.find(persistentId);
    if (iterator == map.end())
        return;

    RealtimeMediaSourceCenter::singleton().captureDeviceWillBeRemoved(persistentId);
    devices().removeFirstMatching([&persistentId](const auto& device) {
        return device.persistentId == persistentId;
    });

    deviceListForDevice(iterator->value).removeFirstMatching([&persistentId](const auto& device) {
        return device.persistentId() == persistentId;
    });

    map.remove(iterator);
    RealtimeMediaSourceCenter::singleton().captureDevicesChanged();
}

void MockRealtimeMediaSourceCenter::setDeviceIsEphemeral(const String& persistentId, bool isEphemeral)
{
    ASSERT(!persistentId.isEmpty());

    auto& map = deviceMap();
    auto iterator = map.find(persistentId);
    if (iterator == map.end())
        return;

    MockMediaDevice device = iterator->value;
    if (isEphemeral)
        device.flags.add(MockMediaDevice::Flag::Ephemeral);
    else
        device.flags.remove(MockMediaDevice::Flag::Ephemeral);

    removeDevice(persistentId);
    addDevice(device);
}

std::optional<MockMediaDevice> MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(const String& id)
{
    ASSERT(!id.isEmpty());

    auto& map = deviceMap();
    auto iterator = map.find(id);
    if (iterator == map.end())
        return std::nullopt;

    return iterator->value;
}

std::optional<CaptureDevice> MockRealtimeMediaSourceCenter::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& id)
{
    ASSERT(!id.isEmpty());

    auto& map = deviceMap();
    auto iterator = map.find(id);
    if (iterator == map.end() || iterator->value.type() != type)
        return std::nullopt;

    return toCaptureDevice(iterator->value);
}

Vector<CaptureDevice>& MockRealtimeMediaSourceCenter::microphoneDevices()
{
    static NeverDestroyed microphoneDevices = [] {
        Vector<CaptureDevice> microphoneDevices;
        for (const auto& device : devices()) {
            if (device.isMicrophone())
                microphoneDevices.append(toCaptureDevice(device));
        }
        return microphoneDevices;
    }();
    return microphoneDevices;
}

Vector<CaptureDevice>& MockRealtimeMediaSourceCenter::speakerDevices()
{
    static NeverDestroyed speakerDevices = [] {
        Vector<CaptureDevice> speakerDevices;
        for (const auto& device : devices()) {
            if (device.isSpeaker())
                speakerDevices.append(toCaptureDevice(device));
        }
        return speakerDevices;
    }();
    return speakerDevices;
}

Vector<CaptureDevice>& MockRealtimeMediaSourceCenter::videoDevices()
{
    static NeverDestroyed videoDevices = [] {
        Vector<CaptureDevice> videoDevices;
        for (const auto& device : devices()) {
            if (device.isCamera())
                videoDevices.append(toCaptureDevice(device));
        }
        return videoDevices;
    }();
    return videoDevices;
}

Vector<CaptureDevice>& MockRealtimeMediaSourceCenter::displayDevices()
{
    static NeverDestroyed displayDevices = [] {
        Vector<CaptureDevice> displayDevices;
        for (const auto& device : devices()) {
            if (device.isDisplay())
                displayDevices.append(toCaptureDevice(device));
        }
        return displayDevices;
    }();
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
    return MockRealtimeDisplaySourceFactory::singleton();
}

MockRealtimeDisplaySourceFactory& MockRealtimeDisplaySourceFactory::singleton()
{
    static NeverDestroyed<MockRealtimeDisplaySourceFactory> factory;
    return factory.get();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
