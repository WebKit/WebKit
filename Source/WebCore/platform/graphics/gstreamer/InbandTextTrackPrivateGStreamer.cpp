/*
 * Copyright (C) 2013 Cable Television Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)

#include "InbandTextTrackPrivateGStreamer.h"

#include "GStreamerUtilities.h"
#include "Logging.h"
#include <glib-object.h>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

namespace WebCore {

static GstPadProbeReturn textTrackPrivateEventCallback(GstPad*, GstPadProbeInfo* info, InbandTextTrackPrivateGStreamer* track)
{
    GstEvent* event = gst_pad_probe_info_get_event(info);
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_STREAM_START:
        track->streamChanged();
        break;
    default:
        break;
    }
    return GST_PAD_PROBE_OK;
}

InbandTextTrackPrivateGStreamer::InbandTextTrackPrivateGStreamer(gint index, GRefPtr<GstPad> pad)
    : InbandTextTrackPrivate(WebVTT), TrackPrivateBaseGStreamer(this, index, pad)
{
    m_eventProbe = gst_pad_add_probe(m_pad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
        reinterpret_cast<GstPadProbeCallback>(textTrackPrivateEventCallback), this, 0);

    notifyTrackOfStreamChanged();
}

void InbandTextTrackPrivateGStreamer::disconnect()
{
    if (!m_pad)
        return;

    gst_pad_remove_probe(m_pad.get(), m_eventProbe);

    m_streamTimerHandler.cancel();

    TrackPrivateBaseGStreamer::disconnect();
}

void InbandTextTrackPrivateGStreamer::handleSample(GRefPtr<GstSample> sample)
{
    m_sampleTimerHandler.cancel();
    {
        MutexLocker lock(m_sampleMutex);
        m_pendingSamples.append(sample);
    }
    m_sampleTimerHandler.schedule("[WebKit] InbandTextTrackPrivateGStreamer::notifyTrackOfSample", std::function<void()>(std::bind(&InbandTextTrackPrivateGStreamer::notifyTrackOfSample, this)));
}

void InbandTextTrackPrivateGStreamer::streamChanged()
{
    m_streamTimerHandler.schedule("[WebKit] InbandTextTrackPrivateGStreamer::notifyTrackOfStreamChanged", std::function<void()>(std::bind(&InbandTextTrackPrivateGStreamer::notifyTrackOfStreamChanged, this)));
}

void InbandTextTrackPrivateGStreamer::notifyTrackOfSample()
{
    Vector<GRefPtr<GstSample> > samples;
    {
        MutexLocker lock(m_sampleMutex);
        m_pendingSamples.swap(samples);
    }

    for (size_t i = 0; i < samples.size(); ++i) {
        GRefPtr<GstSample> sample = samples[i];
        GstBuffer* buffer = gst_sample_get_buffer(sample.get());
        if (!buffer) {
            WARN_MEDIA_MESSAGE("Track %d got sample with no buffer.", m_index);
            continue;
        }
        GstMapInfo info;
        gboolean ret = gst_buffer_map(buffer, &info, GST_MAP_READ);
        ASSERT(ret);
        if (!ret) {
            WARN_MEDIA_MESSAGE("Track %d unable to map buffer.", m_index);
            continue;
        }

        INFO_MEDIA_MESSAGE("Track %d parsing sample: %.*s", m_index, static_cast<int>(info.size),
            reinterpret_cast<char*>(info.data));
        client()->parseWebVTTCueData(this, reinterpret_cast<char*>(info.data), info.size);
        gst_buffer_unmap(buffer, &info);
    }
}

void InbandTextTrackPrivateGStreamer::notifyTrackOfStreamChanged()
{
    GRefPtr<GstEvent> event = adoptGRef(gst_pad_get_sticky_event(m_pad.get(),
        GST_EVENT_STREAM_START, 0));
    if (!event)
        return;

    const gchar* streamId;
    gst_event_parse_stream_start(event.get(), &streamId);
    INFO_MEDIA_MESSAGE("Track %d got stream start for stream %s.", m_index, streamId);
    m_streamId = streamId;
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)
