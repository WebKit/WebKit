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

#include "GStreamerCommon.h"
#include "Logging.h"
#include <glib-object.h>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

namespace WebCore {

InbandTextTrackPrivateGStreamer::InbandTextTrackPrivateGStreamer(gint index, GRefPtr<GstPad> pad)
    : InbandTextTrackPrivate(WebVTT)
    , TrackPrivateBaseGStreamer(this, index, pad)
{
    m_eventProbe = gst_pad_add_probe(m_pad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, [] (GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto* track = static_cast<InbandTextTrackPrivateGStreamer*>(userData);
        switch (GST_EVENT_TYPE(gst_pad_probe_info_get_event(info))) {
        case GST_EVENT_STREAM_START:
            track->streamChanged();
            break;
        default:
            break;
        }
        return GST_PAD_PROBE_OK;
    }, this, nullptr);

    notifyTrackOfStreamChanged();
}

InbandTextTrackPrivateGStreamer::InbandTextTrackPrivateGStreamer(gint index, GRefPtr<GstStream> stream)
    : InbandTextTrackPrivate(WebVTT)
    , TrackPrivateBaseGStreamer(this, index, stream)
{
    m_streamId = gst_stream_get_stream_id(stream.get());
    GST_INFO("Track %d got stream start for stream %s.", m_index, m_streamId.utf8().data());
}

void InbandTextTrackPrivateGStreamer::disconnect()
{
    if (m_pad)
        gst_pad_remove_probe(m_pad.get(), m_eventProbe);

    TrackPrivateBaseGStreamer::disconnect();
}

void InbandTextTrackPrivateGStreamer::handleSample(GRefPtr<GstSample> sample)
{
    {
        LockHolder lock(m_sampleMutex);
        m_pendingSamples.append(sample);
    }

    RefPtr<InbandTextTrackPrivateGStreamer> protectedThis(this);
    m_notifier->notify(MainThreadNotification::NewSample, [protectedThis] {
        protectedThis->notifyTrackOfSample();
    });
}

void InbandTextTrackPrivateGStreamer::streamChanged()
{
    RefPtr<InbandTextTrackPrivateGStreamer> protectedThis(this);
    m_notifier->notify(MainThreadNotification::StreamChanged, [protectedThis] {
        protectedThis->notifyTrackOfStreamChanged();
    });
}

void InbandTextTrackPrivateGStreamer::notifyTrackOfSample()
{
    Vector<GRefPtr<GstSample> > samples;
    {
        LockHolder lock(m_sampleMutex);
        m_pendingSamples.swap(samples);
    }

    for (size_t i = 0; i < samples.size(); ++i) {
        GRefPtr<GstSample> sample = samples[i];
        GstBuffer* buffer = gst_sample_get_buffer(sample.get());
        if (!buffer) {
            GST_WARNING("Track %d got sample with no buffer.", m_index);
            continue;
        }
        auto mappedBuffer = GstMappedBuffer::create(buffer, GST_MAP_READ);
        ASSERT(mappedBuffer);
        if (!mappedBuffer) {
            GST_WARNING("Track %d unable to map buffer.", m_index);
            continue;
        }

        GST_INFO("Track %d parsing sample: %.*s", m_index, static_cast<int>(mappedBuffer->size()),
            reinterpret_cast<char*>(mappedBuffer->data()));
        client()->parseWebVTTCueData(reinterpret_cast<char*>(mappedBuffer->data()), mappedBuffer->size());
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
    GST_INFO("Track %d got stream start for stream %s.", m_index, streamId);
    m_streamId = streamId;
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)
