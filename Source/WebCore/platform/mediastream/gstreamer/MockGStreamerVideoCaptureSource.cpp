/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
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

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "MockGStreamerVideoCaptureSource.h"

#include "MediaSampleGStreamer.h"
#include "MockRealtimeVideoSource.h"

#include <gst/app/gstappsrc.h>

namespace WebCore {

class WrappedMockRealtimeVideoSource : public MockRealtimeVideoSource {
public:
    WrappedMockRealtimeVideoSource(const String& deviceID, const String& name)
        : MockRealtimeVideoSource(deviceID, name)
    {
    }

    void updateSampleBuffer()
    {
        int fpsNumerator, fpsDenominator;
        auto imageBuffer = this->imageBuffer();

        if (!imageBuffer)
            return;

        gst_util_double_to_fraction(frameRate(), &fpsNumerator, &fpsDenominator);
        auto data = imageBuffer->toBGRAData();
        auto size = data.size();
        auto image_size = imageBuffer->internalSize();
        auto gstsample = gst_sample_new(gst_buffer_new_wrapped(static_cast<guint8*>(data.releaseBuffer().get()), size),
            adoptGRef(gst_caps_new_simple("video/x-raw",
                "format", G_TYPE_STRING, "BGRA",
                "width", G_TYPE_INT, image_size.width(),
                "height", G_TYPE_INT, image_size.height(),
                "framerate", GST_TYPE_FRACTION, fpsNumerator, fpsDenominator,
                nullptr)).get(),
            nullptr, nullptr);

        auto sample = MediaSampleGStreamer::create(WTFMove(gstsample),
            WebCore::FloatSize(), String());
        videoSampleAvailable(sample);
    }
};

CaptureSourceOrError MockRealtimeVideoSource::create(const String& deviceID,
    const String& name, const MediaConstraints* constraints)
{
    auto source = adoptRef(*new MockGStreamerVideoCaptureSource(deviceID, name));

    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}

void MockGStreamerVideoCaptureSource::startProducingData()
{
    GStreamerVideoCaptureSource::startProducingData();
    m_wrappedSource->start();
}

void MockGStreamerVideoCaptureSource::stopProducingData()
{
    m_wrappedSource->stop();

    GStreamerVideoCaptureSource::stopProducingData();
}

void MockGStreamerVideoCaptureSource::videoSampleAvailable(MediaSample& sample)
{
    auto src = capturer()->source();

    if (src) {
        auto gstsample = static_cast<MediaSampleGStreamer*>(&sample)->platformSample().sample.gstSample;
        gst_app_src_push_sample(GST_APP_SRC(src), gstsample);
    }
}

MockGStreamerVideoCaptureSource::MockGStreamerVideoCaptureSource(const String& deviceID, const String& name)
    : GStreamerVideoCaptureSource(deviceID, name, "appsrc")
    , m_wrappedSource(std::make_unique<WrappedMockRealtimeVideoSource>(deviceID, name))
{
    m_wrappedSource->addObserver(*this);
}

MockGStreamerVideoCaptureSource::~MockGStreamerVideoCaptureSource()
{
    m_wrappedSource->removeObserver(*this);
}

std::optional<std::pair<String, String>> MockGStreamerVideoCaptureSource::applyConstraints(const MediaConstraints& constraints)
{
    m_wrappedSource->applyConstraints(constraints);
    return GStreamerVideoCaptureSource::applyConstraints(constraints);
}

void MockGStreamerVideoCaptureSource::applyConstraints(const MediaConstraints& constraints, SuccessHandler&& successHandler, FailureHandler&& failureHandler)
{
    m_wrappedSource->applyConstraints(constraints, WTFMove(successHandler), WTFMove(failureHandler));
}

const RealtimeMediaSourceSettings& MockGStreamerVideoCaptureSource::settings() const
{
    return m_wrappedSource->settings();
}

const RealtimeMediaSourceCapabilities& MockGStreamerVideoCaptureSource::capabilities() const
{
    m_capabilities = m_wrappedSource->capabilities();
    m_currentSettings = m_wrappedSource->settings();
    return m_capabilities.value();
}

void MockGStreamerVideoCaptureSource::captureFailed()
{
    stop();

    RealtimeMediaSource::captureFailed();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
