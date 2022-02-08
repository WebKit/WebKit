/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "MockRealtimeVideoSourceGStreamer.h"

#include "MediaSampleGStreamer.h"
#include "MockRealtimeMediaSourceCenter.h"
#include "PixelBuffer.h"

namespace WebCore {

CaptureSourceOrError MockRealtimeVideoSource::create(String&& deviceID, String&& name, String&& hashSalt, const MediaConstraints* constraints)
{
#ifndef NDEBUG
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { "No mock camera device"_s };
#endif

    auto source = adoptRef(*new MockRealtimeVideoSourceGStreamer(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));
    if (constraints) {
        if (auto error = source->applyConstraints(*constraints))
            return WTFMove(error.value().badConstraint);
    }

    return CaptureSourceOrError(RealtimeVideoSource::create(WTFMove(source)));
}

CaptureSourceOrError MockDisplayCaptureSourceGStreamer::create(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints)
{
    auto mockSource = adoptRef(*new MockRealtimeVideoSourceGStreamer(String { device.persistentId() }, String { device.label() }, String { hashSalt }));

    if (constraints) {
        if (auto error = mockSource->applyConstraints(*constraints))
            return WTFMove(error.value().badConstraint);
    }

    auto type = device.type() == CaptureDevice::DeviceType::Screen ? RealtimeMediaSource::Type::Screen : RealtimeMediaSource::Type::Window;
    auto source = adoptRef(*new MockDisplayCaptureSourceGStreamer(type, WTFMove(mockSource), WTFMove(hashSalt), device.type()));
    return CaptureSourceOrError(WTFMove(source));
}

MockDisplayCaptureSourceGStreamer::MockDisplayCaptureSourceGStreamer(RealtimeMediaSource::Type type, Ref<MockRealtimeVideoSourceGStreamer>&& source, String&& hashSalt, CaptureDevice::DeviceType deviceType)
    : RealtimeMediaSource(type, source->name().isolatedCopy(), source->persistentID().isolatedCopy(), WTFMove(hashSalt))
    , m_source(WTFMove(source))
    , m_deviceType(deviceType)
{
    m_source->addVideoSampleObserver(*this);
}

MockDisplayCaptureSourceGStreamer::~MockDisplayCaptureSourceGStreamer()
{
    m_source->removeVideoSampleObserver(*this);
}

void MockDisplayCaptureSourceGStreamer::stopProducingData()
{
    m_source->removeVideoSampleObserver(*this);
    m_source->stop();
}

void MockDisplayCaptureSourceGStreamer::requestToEnd(Observer& callingObserver)
{
    RealtimeMediaSource::requestToEnd(callingObserver);
    m_source->removeVideoSampleObserver(*this);
    m_source->requestToEnd(callingObserver);
}

void MockDisplayCaptureSourceGStreamer::setMuted(bool isMuted)
{
    RealtimeMediaSource::setMuted(isMuted);
    m_source->setMuted(isMuted);
}

void MockDisplayCaptureSourceGStreamer::videoSampleAvailable(MediaSample& sample, VideoSampleMetadata metadata)
{
    RealtimeMediaSource::videoSampleAvailable(sample, metadata);
}

const RealtimeMediaSourceCapabilities& MockDisplayCaptureSourceGStreamer::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

        // FIXME: what should these be?
        // Currently mimicking the values for SCREEN-1 in MockRealtimeMediaSourceCenter.cpp::defaultDevices()
        capabilities.setWidth(CapabilityValueOrRange(1, 1920));
        capabilities.setHeight(CapabilityValueOrRange(1, 1080));
        capabilities.setFrameRate(CapabilityValueOrRange(.01, 30.0));

        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& MockDisplayCaptureSourceGStreamer::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setFrameRate(frameRate());

        m_source->ensureIntrinsicSizeMaintainsAspectRatio();
        auto size = m_source->size();
        settings.setWidth(size.width());
        settings.setHeight(size.height());
        settings.setDeviceId(hashedId());

        settings.setLogicalSurface(false);

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsFrameRate(true);
        supportedConstraints.setSupportsWidth(true);
        supportedConstraints.setSupportsHeight(true);
        supportedConstraints.setSupportsDisplaySurface(true);
        supportedConstraints.setSupportsLogicalSurface(true);
        supportedConstraints.setSupportsDeviceId(true);

        settings.setSupportedConstraints(supportedConstraints);

        m_currentSettings = WTFMove(settings);
    }
    return m_currentSettings.value();
}

MockRealtimeVideoSourceGStreamer::MockRealtimeVideoSourceGStreamer(String&& deviceID, String&& name, String&& hashSalt)
    : MockRealtimeVideoSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt))
{
    ensureGStreamerInitialized();
}

void MockRealtimeVideoSourceGStreamer::updateSampleBuffer()
{
    auto imageBuffer = this->imageBuffer();
    if (!imageBuffer)
        return;

    auto pixelBuffer = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }, { { }, imageBuffer->truncatedLogicalSize() });
    if (!pixelBuffer)
        return;

    std::optional<VideoSampleMetadata> metadata;
    metadata->captureTime = MonotonicTime::now().secondsSinceEpoch();
    auto sample = MediaSampleGStreamer::createImageSample(WTFMove(*pixelBuffer), size(), frameRate(), sampleRotation(), false, WTFMove(metadata));
    sample->offsetTimestampsBy(MediaTime::createWithDouble((elapsedTime() + 100_ms).seconds()));
    dispatchMediaSampleToObservers(sample.get(), { });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
