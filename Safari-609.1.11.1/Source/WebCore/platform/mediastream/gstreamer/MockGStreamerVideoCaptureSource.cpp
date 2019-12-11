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
    static Ref<WrappedMockRealtimeVideoSource> create(String&& deviceID, String&& name, String&& hashSalt)
    {
        return adoptRef(*new WrappedMockRealtimeVideoSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));
    }

    RealtimeMediaSource& asRealtimeMediaSource()
    {
        return *this;
    }

    void updateSampleBuffer()
    {
        auto imageBuffer = this->imageBuffer();
        if (!imageBuffer)
            return;

        int fpsNumerator, fpsDenominator;
        gst_util_double_to_fraction(frameRate(), &fpsNumerator, &fpsDenominator);
        auto imageSize = imageBuffer->internalSize();
        auto caps = adoptGRef(gst_caps_new_simple("video/x-raw",
            "format", G_TYPE_STRING, "BGRA",
            "width", G_TYPE_INT, imageSize.width(),
            "height", G_TYPE_INT, imageSize.height(),
            "framerate", GST_TYPE_FRACTION, fpsNumerator, fpsDenominator, nullptr));
        auto data = imageBuffer->toBGRAData();
        auto size = data.size();
        auto buffer = adoptGRef(gst_buffer_new_wrapped(g_memdup(data.releaseBuffer().get(), size), size));
        auto gstSample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));

        videoSampleAvailable(MediaSampleGStreamer::create(WTFMove(gstSample), FloatSize(), String()));
    }

private:
    WrappedMockRealtimeVideoSource(String&& deviceID, String&& name, String&& hashSalt)
        : MockRealtimeVideoSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt))
    {
    }
};

CaptureSourceOrError MockRealtimeVideoSource::create(String&& deviceID,
    String&& name, String&& hashSalt, const MediaConstraints* constraints)
{
    auto source = adoptRef(*new MockGStreamerVideoCaptureSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));

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

MockGStreamerVideoCaptureSource::MockGStreamerVideoCaptureSource(String&& deviceID, String&& name, String&& hashSalt)
    : GStreamerVideoCaptureSource(String { deviceID }, String { name }, String { hashSalt }, "appsrc")
    , m_wrappedSource(WrappedMockRealtimeVideoSource::create(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)))
{
    m_wrappedSource->addObserver(*this);
}

MockGStreamerVideoCaptureSource::~MockGStreamerVideoCaptureSource()
{
    m_wrappedSource->removeObserver(*this);
}

Optional<RealtimeMediaSource::ApplyConstraintsError> MockGStreamerVideoCaptureSource::applyConstraints(const MediaConstraints& constraints)
{
    m_wrappedSource->applyConstraints(constraints);
    return GStreamerVideoCaptureSource::applyConstraints(constraints);
}

void MockGStreamerVideoCaptureSource::applyConstraints(const MediaConstraints& constraints, ApplyConstraintsHandler&& completionHandler)
{
    m_wrappedSource->applyConstraints(constraints, WTFMove(completionHandler));
}

const RealtimeMediaSourceSettings& MockGStreamerVideoCaptureSource::settings()
{
    return m_wrappedSource->asRealtimeMediaSource().settings();
}

const RealtimeMediaSourceCapabilities& MockGStreamerVideoCaptureSource::capabilities()
{
    m_capabilities = m_wrappedSource->asRealtimeMediaSource().capabilities();
    m_currentSettings = m_wrappedSource->asRealtimeMediaSource().settings();
    return m_capabilities.value();
}

void MockGStreamerVideoCaptureSource::captureFailed()
{
    stop();

    RealtimeMediaSource::captureFailed();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
