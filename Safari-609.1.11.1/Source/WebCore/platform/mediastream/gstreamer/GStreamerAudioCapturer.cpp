/*
 * Copyright (C) 2018 Metrological Group B.V.
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

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GStreamerAudioCapturer.h"

#include "LibWebRTCAudioFormat.h"

#include <gst/app/gstappsink.h>

namespace WebCore {

GStreamerAudioCapturer::GStreamerAudioCapturer(GStreamerCaptureDevice device)
    : GStreamerCapturer(device, adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, LibWebRTCAudioFormat::sampleRate, nullptr)))
{
}

GStreamerAudioCapturer::GStreamerAudioCapturer()
    : GStreamerCapturer("appsrc", adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, LibWebRTCAudioFormat::sampleRate, nullptr)))
{
}

GstElement* GStreamerAudioCapturer::createConverter()
{
    auto converter = gst_parse_bin_from_description("audioconvert ! audioresample", TRUE, nullptr);

    ASSERT(converter);

    return converter;
}

bool GStreamerAudioCapturer::setSampleRate(int sampleRate)
{

    if (sampleRate <= 0) {
        GST_INFO_OBJECT(m_pipeline.get(), "Not forcing sample rate");

        return false;
    }

    GST_INFO_OBJECT(m_pipeline.get(), "Setting SampleRate %d", sampleRate);
    m_caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, sampleRate, nullptr));

    if (!m_capsfilter.get())
        return false;

    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);

    return true;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
