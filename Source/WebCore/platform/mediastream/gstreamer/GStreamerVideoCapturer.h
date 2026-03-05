/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro  <alex@igalia.com>
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

#pragma once

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GStreamerCapturer.h"
#include "IntSize.h"

namespace WebCore {

class VideoFrameGStreamer;

class GStreamerVideoCapturer final : public GStreamerCapturer {
    friend class GStreamerVideoCaptureSource;
    friend class MockRealtimeVideoSourceGStreamer;
public:
    GStreamerVideoCapturer(GStreamerCaptureDevice&&);
    GStreamerVideoCapturer(const PipeWireCaptureDevice&);
    ~GStreamerVideoCapturer() = default;

    void tearDown(bool disconnectSignals) final;
    void setupPipeline() final;
    GstElement* createConverter() final;
    ASCIILiteral name() final { return "Video"_s; }

    using SinkVideoFrameCallback = Function<void(Ref<VideoFrameGStreamer>&&)>;
    void setSinkVideoFrameCallback(SinkVideoFrameCallback&&);

private:
    void handleSample(GRefPtr<GstSample>&&);
    bool setSize(const IntSize&);
    const IntSize& size() const { return m_size; }

    bool setFrameRate(double);
    void reconfigure();

    bool isCapturingDisplay() const;

    GRefPtr<GstElement> m_videoSrcMIMETypeFilter;

    std::pair<GStreamerCapturer::SinkSignalsHolder, SinkVideoFrameCallback> m_sinkVideoFrameCallback;
    IntSize m_size;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
