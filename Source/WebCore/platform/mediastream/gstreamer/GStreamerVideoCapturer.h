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

namespace WebCore {

class VideoFrameGStreamer;

class GStreamerVideoCapturer final : public GStreamerCapturer {
    friend class GStreamerVideoCaptureSource;
    friend class MockRealtimeVideoSourceGStreamer;
public:
    GStreamerVideoCapturer(GStreamerCaptureDevice&&);
    GStreamerVideoCapturer(const char* sourceFactory, CaptureDevice::DeviceType);
    ~GStreamerVideoCapturer() = default;

    GstElement* createSource() final;
    GstElement* createConverter() final;
    const char* name() final { return "Video"; }

    using NodeAndFD = std::pair<uint32_t, int>;

    using SinkVideoFrameCallback = Function<void(Ref<VideoFrameGStreamer>&&)>;
    void setSinkVideoFrameCallback(SinkVideoFrameCallback&&);

private:
    bool setSize(int width, int height);
    bool setFrameRate(double);
    void reconfigure();

    GstVideoInfo getBestFormat();

    void setPipewireNodeAndFD(const NodeAndFD& nodeAndFd) { m_nodeAndFd = nodeAndFd; }
    bool isCapturingDisplay() const { return m_nodeAndFd.has_value(); }

    std::optional<NodeAndFD> m_nodeAndFd;
    GRefPtr<GstElement> m_videoSrcMIMETypeFilter;
    std::pair<unsigned long, SinkVideoFrameCallback> m_sinkVideoFrameCallback;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
