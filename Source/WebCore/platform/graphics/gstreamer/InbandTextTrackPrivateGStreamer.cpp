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

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "InbandTextTrackPrivateGStreamer.h"

#include <wtf/Lock.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_text_track_debug);
#define GST_CAT_DEFAULT webkit_text_track_debug

static void ensureTextTrackDebugCategoryInitialized()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_text_track_debug, "webkittexttrack", 0, "WebKit Text Track");
    });
}

InbandTextTrackPrivateGStreamer::InbandTextTrackPrivateGStreamer(unsigned index, GRefPtr<GstPad>&& pad, bool shouldHandleStreamStartEvent)
    : InbandTextTrackPrivate(CueFormat::WebVTT)
    , TrackPrivateBaseGStreamer(TrackPrivateBaseGStreamer::TrackType::Text, this, index, WTFMove(pad), shouldHandleStreamStartEvent)
    , m_kind(Kind::Subtitles)
{
    ensureTextTrackDebugCategoryInitialized();
    installUpdateConfigurationHandlers();
}

InbandTextTrackPrivateGStreamer::InbandTextTrackPrivateGStreamer(unsigned index, GstStream* stream)
    : InbandTextTrackPrivate(CueFormat::WebVTT)
    , TrackPrivateBaseGStreamer(TrackPrivateBaseGStreamer::TrackType::Text, this, index, stream)
{
    ensureTextTrackDebugCategoryInitialized();
    installUpdateConfigurationHandlers();

    GST_INFO("Track %d got stream start for stream %s.", m_index, m_stringId.string().utf8().data());

    GST_DEBUG("Stream %" GST_PTR_FORMAT, m_stream.get());
    auto caps = adoptGRef(gst_stream_get_caps(m_stream.get()));
    m_kind = doCapsHaveType(caps.get(), "closedcaption/"_s) ? Kind::Captions : Kind::Subtitles;
}

void InbandTextTrackPrivateGStreamer::tagsChanged(GRefPtr<GstTagList>&& tags)
{
    if (!tags)
        return;

    if (!updateTrackIDFromTags(tags))
        return;

    GST_DEBUG_OBJECT(objectForLogging(), "Text track ID set from container-specific-track-id tag %" G_GUINT64_FORMAT, *m_trackID);
    notifyClients([trackID = *m_trackID](auto& client) {
        client.idChanged(trackID);
    });
}

void InbandTextTrackPrivateGStreamer::handleSample(GRefPtr<GstSample>&& sample)
{
    {
        Locker locker { m_sampleMutex };
        m_pendingSamples.append(WTFMove(sample));
    }

    RefPtr<InbandTextTrackPrivateGStreamer> protectedThis(this);
    m_notifier->notify(MainThreadNotification::NewSample, [protectedThis] {
        protectedThis->notifyTrackOfSample();
    });
}

void InbandTextTrackPrivateGStreamer::notifyTrackOfSample()
{
    Vector<GRefPtr<GstSample>> samples;
    {
        Locker locker { m_sampleMutex };
        m_pendingSamples.swap(samples);
    }

    for (auto& sample : samples) {
        GstBuffer* buffer = gst_sample_get_buffer(sample.get());
        if (!buffer) {
            GST_WARNING("Track %d got sample with no buffer.", m_index);
            continue;
        }
        GstMappedBuffer mappedBuffer(buffer, GST_MAP_READ);
        ASSERT(mappedBuffer);
        if (!mappedBuffer) {
            GST_WARNING("Track %d unable to map buffer.", m_index);
            continue;
        }

        GST_INFO("Track %d parsing sample: %.*s", m_index, static_cast<int>(mappedBuffer.size()),
            reinterpret_cast<char*>(mappedBuffer.data()));
        ASSERT(isMainThread());
        ASSERT(!hasClients() || hasOneClient());
        notifyMainThreadClient([&](auto& client) {
            downcast<InbandTextTrackPrivateClient>(client).parseWebVTTCueData(std::span { mappedBuffer.data(), mappedBuffer.size() });
        });
    }
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
