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

#include <gst/video/video.h>

namespace WebCore {

class GStreamerVideoCapturer final : public GStreamerCapturer {
public:
    GStreamerVideoCapturer(GStreamerCaptureDevice);
    GStreamerVideoCapturer(const char* source_factory, CaptureDevice::DeviceType);

    GstElement* createSource() final;
    GstElement* createConverter() final;
    const char* name() final { return "Video"; }

    bool setSize(int width, int height);
    bool setFrameRate(double);
    GstVideoInfo getBestFormat();

    void setPipewireFD(int fd) { m_fd = fd; }
    std::optional<int> pipewireFD() const { return m_fd; }

private:
    std::optional<int> m_fd;
};

} // namespace WebCore
#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
