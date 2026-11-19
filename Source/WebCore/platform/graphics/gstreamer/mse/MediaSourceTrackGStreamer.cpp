/*
 * Copyright (C) 2020, 2021 Metrological Group B.V.
 * Copyright (C) 2020, 2021 Igalia, S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaSourceTrackGStreamer.h"

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

GST_DEBUG_CATEGORY_STATIC(webkit_mse_track_debug);
#define GST_CAT_DEFAULT webkit_mse_track_debug

namespace WebCore {

MediaSourceTrackGStreamer::MediaSourceTrackGStreamer(GStreamerTrackType type, TrackID trackId, GRefPtr<GstCaps>&& initialCaps)
    : m_type(type)
    , m_id(trackId)
    , m_initialCaps(WTF::move(initialCaps))
    , m_queueDataMutex(trackId)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_mse_track_debug, "webkitmsetrack", 0, "WebKit MSE Track");
    });
}

MediaSourceTrackGStreamer::~MediaSourceTrackGStreamer()
{
    ASSERT(m_isRemoved);
}

Ref<MediaSourceTrackGStreamer> MediaSourceTrackGStreamer::create(GStreamerTrackType type, TrackID trackId, GRefPtr<GstCaps>&& initialCaps)
{
    return adoptRef(*new MediaSourceTrackGStreamer(type, trackId, WTF::move(initialCaps)));
}

bool MediaSourceTrackGStreamer::isReadyForMoreSamples()
{
    ASSERT(isMainThread());
    DataMutexLocker queue { m_queueDataMutex };
    return !queue->isFull();
}

void MediaSourceTrackGStreamer::notifyWhenReadyForMoreSamples(TrackQueue::LowLevelHandler&& handler)
{
    ASSERT(isMainThread());
    DataMutexLocker queue { m_queueDataMutex };
    queue->notifyWhenLowLevel(WTF::move(handler));
}

void MediaSourceTrackGStreamer::enqueueObject(GRefPtr<GstMiniObject>&& object)
{
    ASSERT(isMainThread());
    DataMutexLocker queue { m_queueDataMutex };
    queue->enqueueObject(WTF::move(object));
}

void MediaSourceTrackGStreamer::clearQueue()
{
    ASSERT(isMainThread());
    DataMutexLocker queue { m_queueDataMutex };
    queue->clear();
}

void MediaSourceTrackGStreamer::remove()
{
    ASSERT(isMainThread());
    m_isRemoved = true;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)
