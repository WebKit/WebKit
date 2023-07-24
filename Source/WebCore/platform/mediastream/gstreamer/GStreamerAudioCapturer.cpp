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
#include "GStreamerAudioCapturer.h"

#if USE(LIBWEBRTC)
#include "LibWebRTCAudioFormat.h"
#endif

#include <gst/app/gstappsink.h>

namespace WebCore {

#if USE(LIBWEBRTC)
static constexpr size_t s_audioCaptureSampleRate = LibWebRTCAudioFormat::sampleRate;
#else
static constexpr size_t s_audioCaptureSampleRate = 48000;
#endif

GST_DEBUG_CATEGORY(webkit_audio_capturer_debug);
#define GST_CAT_DEFAULT webkit_audio_capturer_debug

static void initializeAudioCapturerDebugCategory()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_audio_capturer_debug, "webkitaudiocapturer", 0, "WebKit Audio Capturer");
    });
}

GStreamerAudioCapturer::GStreamerAudioCapturer(GStreamerCaptureDevice&& device)
    : GStreamerCapturer(WTFMove(device), adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, s_audioCaptureSampleRate, nullptr)))
{
    initializeAudioCapturerDebugCategory();
}

GStreamerAudioCapturer::GStreamerAudioCapturer()
    : GStreamerCapturer("appsrc", adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, s_audioCaptureSampleRate, nullptr)), CaptureDevice::DeviceType::Microphone)
{
    initializeAudioCapturerDebugCategory();
}

void GStreamerAudioCapturer::setSinkAudioCallback(SinkAudioDataCallback&& callback)
{
    if (m_sinkAudioDataCallback.first)
        g_signal_handler_disconnect(sink(), m_sinkAudioDataCallback.first);

    m_sinkAudioDataCallback.second = WTFMove(callback);
    m_sinkAudioDataCallback.first = g_signal_connect_swapped(sink(), "new-sample", G_CALLBACK(+[](GStreamerAudioCapturer* capturer, GstElement* sink) -> GstFlowReturn {
        auto gstSample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));
        auto presentationTime = fromGstClockTime(GST_BUFFER_PTS(gst_sample_get_buffer(gstSample.get())));
        capturer->m_sinkAudioDataCallback.second(WTFMove(gstSample), WTFMove(presentationTime));
        return GST_FLOW_OK;
    }), this);
}

GstElement* GStreamerAudioCapturer::createConverter()
{
    auto* bin = gst_bin_new(nullptr);
    auto* audioconvert = makeGStreamerElement("audioconvert", nullptr);
    auto* audioresample = makeGStreamerElement("audioresample", nullptr);
    gst_bin_add_many(GST_BIN_CAST(bin), audioconvert, audioresample, nullptr);
    gst_element_link(audioconvert, audioresample);

#if USE(GSTREAMER_WEBRTC)
    if (auto* webrtcdsp = makeGStreamerElement("webrtcdsp", nullptr)) {
        g_object_set(webrtcdsp, "echo-cancel", FALSE, "voice-detection", TRUE, nullptr);

        auto* audioconvert2 = makeGStreamerElement("audioconvert", nullptr);
        auto* audioresample2 = makeGStreamerElement("audioresample", nullptr);
        gst_bin_add_many(GST_BIN_CAST(bin), audioconvert2, audioresample2, webrtcdsp, nullptr);
        gst_element_link_many(audioconvert2, audioresample2, webrtcdsp, audioconvert, nullptr);
    }
#endif

    if (auto pad = adoptGRef(gst_bin_find_unlinked_pad(GST_BIN_CAST(bin), GST_PAD_SRC)))
        gst_element_add_pad(GST_ELEMENT_CAST(bin), gst_ghost_pad_new("src", pad.get()));
    if (auto pad = adoptGRef(gst_bin_find_unlinked_pad(GST_BIN_CAST(bin), GST_PAD_SINK)))
        gst_element_add_pad(GST_ELEMENT_CAST(bin), gst_ghost_pad_new("sink", pad.get()));

    return bin;
}

bool GStreamerAudioCapturer::setSampleRate(int sampleRate)
{
    if (sampleRate <= 0) {
        GST_INFO_OBJECT(m_pipeline.get(), "Not forcing sample rate");
        return false;
    }

    GST_INFO_OBJECT(m_pipeline.get(), "Setting SampleRate %d", sampleRate);
    m_caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, sampleRate, nullptr));

    if (!m_capsfilter)
        return false;

    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);
    return true;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
