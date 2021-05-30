/*
 * Copyright (C) 2020, 2021 Metrological Group B.V.
 * Copyright (C) 2020, 2021 Igalia, S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaSourceTrackGStreamer.h"

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

GST_DEBUG_CATEGORY_EXTERN(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

namespace WebCore {

MediaSourceTrackGStreamer::MediaSourceTrackGStreamer(TrackPrivateBaseGStreamer::TrackType type, AtomString trackId, GRefPtr<GstCaps>&& initialCaps)
    : m_type(type)
    , m_trackId(trackId)
    , m_initialCaps(WTFMove(initialCaps))
    , m_queueDataMutex(trackId)
{ }

MediaSourceTrackGStreamer::~MediaSourceTrackGStreamer()
{
    ASSERT(m_isRemoved);
}

Ref<MediaSourceTrackGStreamer> MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType type, AtomString trackId, GRefPtr<GstCaps>&& initialCaps)
{
    return makeRef(*new MediaSourceTrackGStreamer(type, trackId, WTFMove(initialCaps)));
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
    queue->notifyWhenLowLevel(WTFMove(handler));
}

void MediaSourceTrackGStreamer::enqueueObject(GRefPtr<GstMiniObject>&& object)
{
    ASSERT(isMainThread());
    DataMutexLocker queue { m_queueDataMutex };
    queue->enqueueObject(WTFMove(object));
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

}

#endif
