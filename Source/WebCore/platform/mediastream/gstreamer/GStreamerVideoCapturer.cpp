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

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "GStreamerVideoCapturer.h"

GST_DEBUG_CATEGORY(webkit_video_capturer_debug);
#define GST_CAT_DEFAULT webkit_video_capturer_debug

namespace WebCore {

static void initializeDebugCategory()
{
    ensureGStreamerInitialized();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_capturer_debug, "webkitvideocapturer", 0, "WebKit Video Capturer");
    });
}

GStreamerVideoCapturer::GStreamerVideoCapturer(GStreamerCaptureDevice device)
    : GStreamerCapturer(device, adoptGRef(gst_caps_new_empty_simple("video/x-raw")))
{
    initializeDebugCategory();
}

GStreamerVideoCapturer::GStreamerVideoCapturer(const char* sourceFactory, CaptureDevice::DeviceType deviceType)
    : GStreamerCapturer(sourceFactory, adoptGRef(gst_caps_new_empty_simple("video/x-raw")), deviceType)
{
    initializeDebugCategory();
}

GstElement* GStreamerVideoCapturer::createSource()
{
    auto* src = GStreamerCapturer::createSource();
    if (m_fd)
        g_object_set(m_src.get(), "fd", *m_fd, nullptr);
    return src;
}

GstElement* GStreamerVideoCapturer::createConverter()
{
    // https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/issues/97#note_56575
    return makeGStreamerBin("videoscale ! videoconvert ! videorate drop-only=1 average-period=1", true);
}

GstVideoInfo GStreamerVideoCapturer::getBestFormat()
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_fixate(gst_device_get_caps(m_device.get())));
    GstVideoInfo info;
    gst_video_info_from_caps(&info, caps.get());

    return info;
}

bool GStreamerVideoCapturer::setSize(int width, int height)
{
    if (m_fd.has_value()) {
        // Pipewiresrc doesn't seem to support caps re-negotiation and framerate configuration properly.
        GST_FIXME_OBJECT(m_pipeline.get(), "Resizing disabled on display capture source");
        return true;
    }

    if (!width || !height)
        return false;

    auto videoResolution = getVideoResolutionFromCaps(m_caps.get());
    if (videoResolution && videoResolution->width() == width && videoResolution->height() == height) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Size has not changed");
        return true;
    }

    GST_INFO_OBJECT(m_pipeline.get(), "Setting size to %dx%d", width, height);
    m_caps = adoptGRef(gst_caps_copy(m_caps.get()));
    gst_caps_set_simple(m_caps.get(), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, nullptr);

    if (!m_capsfilter)
        return false;

    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);
    return true;
}

bool GStreamerVideoCapturer::setFrameRate(double frameRate)
{
    if (m_fd.has_value()) {
        // Pipewiresrc doesn't seem to support caps re-negotiation and framerate configuration properly.
        GST_FIXME_OBJECT(m_pipeline.get(), "Framerate override disabled on display capture source");
        return true;
    }

    int numerator, denominator;

    gst_util_double_to_fraction(frameRate, &numerator, &denominator);

    if (numerator < -G_MAXINT) {
        GST_INFO_OBJECT(m_pipeline.get(), "Framerate %f not allowed", frameRate);
        return false;
    }

    if (!numerator) {
        GST_INFO_OBJECT(m_pipeline.get(), "Do not force variable framerate");
        return false;
    }

    m_caps = adoptGRef(gst_caps_copy(m_caps.get()));
    gst_caps_set_simple(m_caps.get(), "framerate", GST_TYPE_FRACTION, numerator, denominator, nullptr);

    if (!m_capsfilter)
        return false;

    GST_INFO_OBJECT(m_pipeline.get(), "Setting framerate to %f fps", frameRate);
    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);

    return true;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
