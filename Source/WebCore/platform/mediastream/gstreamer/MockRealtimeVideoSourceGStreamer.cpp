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

#include "GStreamerCaptureDeviceManager.h"
#include "MockRealtimeMediaSourceCenter.h"
#include "PixelBuffer.h"
#include "VideoFrameGStreamer.h"
#include <gst/app/gstappsrc.h>

namespace WebCore {

CaptureSourceOrError MockRealtimeVideoSource::create(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
#ifndef NDEBUG
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return CaptureSourceOrError({ "No mock camera device"_s , MediaAccessDenialReason::PermissionDenied });
#endif

    Ref<RealtimeMediaSource> source = adoptRef(*new MockRealtimeVideoSourceGStreamer(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts), pageIdentifier));
    if (constraints) {
        if (auto error = source->applyConstraints(*constraints))
            return CaptureSourceOrError(CaptureSourceError { error->invalidConstraint });
    }

    return source;
}

MockRealtimeVideoSourceGStreamer::MockRealtimeVideoSourceGStreamer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts, PageIdentifier pageIdentifier)
    : MockRealtimeVideoSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts), pageIdentifier)
{
    ensureGStreamerInitialized();
    auto& singleton = GStreamerVideoCaptureDeviceManager::singleton();
    auto device = singleton.gstreamerDeviceWithUID(this->captureDevice().persistentId());
    ASSERT(device);
    if (!device)
        return;

    device->setIsMockDevice(true);
    m_capturer = adoptRef(*new GStreamerVideoCapturer(WTFMove(*device)));
    m_capturer->addObserver(*this);
    m_capturer->setupPipeline();
    m_capturer->setSinkVideoFrameCallback([this](auto&& videoFrame) {
        if (!isProducingData() || muted())
            return;
        dispatchVideoFrameToObservers(WTFMove(videoFrame), { });
    });
    singleton.registerCapturer(m_capturer);
}

MockRealtimeVideoSourceGStreamer::~MockRealtimeVideoSourceGStreamer()
{
    m_capturer->stop();
    m_capturer->removeObserver(*this);

    auto& singleton = GStreamerVideoCaptureDeviceManager::singleton();
    singleton.unregisterCapturer(*m_capturer);
}

void MockRealtimeVideoSourceGStreamer::startProducingData()
{
    if (deviceType() == CaptureDevice::DeviceType::Camera)
        m_capturer->setSize(size());

    m_capturer->setFrameRate(frameRate());
    m_capturer->start();
    MockRealtimeVideoSource::startProducingData();
}

void MockRealtimeVideoSourceGStreamer::stopProducingData()
{
    m_capturer->stop();
    MockRealtimeVideoSource::stopProducingData();
}

void MockRealtimeVideoSourceGStreamer::captureEnded()
{
    // NOTE: We could call captureFailed() like in the mock audio source, but that would trigger new
    // test failures. For some reason we want 'ended' MediaStreamTrack notifications only for audio
    // devices removal.
}

void MockRealtimeVideoSourceGStreamer::updateSampleBuffer()
{
    RefPtr imageBuffer = this->imageBufferInternal();
    if (!imageBuffer)
        return;

    auto pixelBuffer = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }, { { }, imageBuffer->truncatedLogicalSize() });
    if (!pixelBuffer)
        return;

    VideoFrameTimeMetadata metadata;
    metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();
    auto presentationTime = MediaTime::createWithDouble((elapsedTime()).seconds());
    auto videoFrame = VideoFrameGStreamer::createFromPixelBuffer(pixelBuffer.releaseNonNull(), VideoFrameGStreamer::CanvasContentType::Canvas2D, videoFrameRotation(), presentationTime, m_capturer->size(), frameRate(), false, WTFMove(metadata));
    if (!videoFrame)
        return;

    // Mock GstDevice is an appsrc, see webkitMockDeviceCreateElement().
    ASSERT(GST_IS_APP_SRC(m_capturer->source()));
    gst_app_src_push_sample(GST_APP_SRC_CAST(m_capturer->source()), videoFrame->sample());
}

void MockRealtimeVideoSourceGStreamer::setSizeFrameRateAndZoom(const VideoPresetConstraints& constraints)
{
    MockRealtimeVideoSource::setSizeFrameRateAndZoom(constraints);

    if (!constraints.width || !constraints.height)
        return;

    m_capturer->setSize({ *constraints.width, *constraints.height });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
