/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L
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
#include "MediaSourceClientGStreamerMSE.h"

#include "AppendPipeline.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "PlaybackPipeline.h"
#include "WebKitMediaSourceGStreamer.h"
#include <gst/gst.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

namespace WebCore {

Ref<MediaSourceClientGStreamerMSE> MediaSourceClientGStreamerMSE::create(MediaPlayerPrivateGStreamerMSE& playerPrivate)
{
    ASSERT(WTF::isMainThread());

    // No return adoptRef(new MediaSourceClientGStreamerMSE(playerPrivate)) because the ownership has already been transferred to MediaPlayerPrivateGStreamerMSE.
    Ref<MediaSourceClientGStreamerMSE> client(adoptRef(*new MediaSourceClientGStreamerMSE(playerPrivate)));
    playerPrivate.setMediaSourceClient(client.get());
    return client;
}

MediaSourceClientGStreamerMSE::MediaSourceClientGStreamerMSE(MediaPlayerPrivateGStreamerMSE& playerPrivate)
    : m_playerPrivate(&playerPrivate)
    , m_duration(MediaTime::invalidTime())
{
    ASSERT(WTF::isMainThread());
}

MediaSourceClientGStreamerMSE::~MediaSourceClientGStreamerMSE()
{
    ASSERT(WTF::isMainThread());
}

MediaSourcePrivate::AddStatus MediaSourceClientGStreamerMSE::addSourceBuffer(RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivate, const ContentType&)
{
    ASSERT(WTF::isMainThread());

    if (!m_playerPrivate)
        return MediaSourcePrivate::AddStatus::NotSupported;

    ASSERT(m_playerPrivate->m_playbackPipeline);
    ASSERT(sourceBufferPrivate);

    RefPtr<AppendPipeline> appendPipeline = adoptRef(new AppendPipeline(*this, *sourceBufferPrivate, *m_playerPrivate));
    GST_TRACE("Adding SourceBuffer to AppendPipeline: this=%p sourceBuffer=%p appendPipeline=%p", this, sourceBufferPrivate.get(), appendPipeline.get());
    m_playerPrivate->m_appendPipelinesMap.add(sourceBufferPrivate, appendPipeline);

    return m_playerPrivate->m_playbackPipeline->addSourceBuffer(sourceBufferPrivate);
}

const MediaTime& MediaSourceClientGStreamerMSE::duration()
{
    ASSERT(WTF::isMainThread());

    return m_duration;
}

void MediaSourceClientGStreamerMSE::durationChanged(const MediaTime& duration)
{
    ASSERT(WTF::isMainThread());

    GST_TRACE("duration: %f", duration.toFloat());
    if (!duration.isValid() || duration.isPositiveInfinite() || duration.isNegativeInfinite())
        return;

    m_duration = duration;
    if (m_playerPrivate)
        m_playerPrivate->durationChanged();
}

void MediaSourceClientGStreamerMSE::abort(RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivate)
{
    ASSERT(WTF::isMainThread());

    GST_DEBUG("aborting");

    if (!m_playerPrivate)
        return;

    RefPtr<AppendPipeline> appendPipeline = m_playerPrivate->m_appendPipelinesMap.get(sourceBufferPrivate);

    ASSERT(appendPipeline);

    appendPipeline->resetParserState();
}

void MediaSourceClientGStreamerMSE::resetParserState(RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivate)
{
    ASSERT(WTF::isMainThread());

    GST_DEBUG("resetting parser state");

    if (!m_playerPrivate)
        return;

    RefPtr<AppendPipeline> appendPipeline = m_playerPrivate->m_appendPipelinesMap.get(sourceBufferPrivate);

    ASSERT(appendPipeline);

    appendPipeline->resetParserState();
}

bool MediaSourceClientGStreamerMSE::append(RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivate, Vector<unsigned char>&& data)
{
    ASSERT(WTF::isMainThread());

    GST_DEBUG("Appending %zu bytes", data.size());

    if (!m_playerPrivate)
        return false;

    RefPtr<AppendPipeline> appendPipeline = m_playerPrivate->m_appendPipelinesMap.get(sourceBufferPrivate);

    ASSERT(appendPipeline);

    // Wrap the whole Vector object in case the data is stored in the inlined buffer.
    auto* bufferData = data.data();
    auto bufferLength = data.size();
    GRefPtr<GstBuffer> buffer = adoptGRef(gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(0), bufferData, bufferLength, 0, bufferLength, new Vector<unsigned char>(WTFMove(data)),
        [](gpointer data)
        {
            delete static_cast<Vector<unsigned char>*>(data);
        }));

    return appendPipeline->pushNewBuffer(WTFMove(buffer)) == GST_FLOW_OK;
}

void MediaSourceClientGStreamerMSE::markEndOfStream(MediaSourcePrivate::EndOfStreamStatus status)
{
    ASSERT(WTF::isMainThread());

    if (!m_playerPrivate)
        return;

    m_playerPrivate->markEndOfStream(status);
}

void MediaSourceClientGStreamerMSE::removedFromMediaSource(RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivate)
{
    ASSERT(WTF::isMainThread());

    if (!m_playerPrivate)
        return;

    ASSERT(m_playerPrivate->m_playbackPipeline);

    // Remove the AppendPipeline from the map. This should cause its destruction since there should be no alive
    // references at this point.
    ASSERT(m_playerPrivate->m_appendPipelinesMap.get(sourceBufferPrivate)->hasOneRef());
    m_playerPrivate->m_appendPipelinesMap.remove(sourceBufferPrivate);

    m_playerPrivate->m_playbackPipeline->removeSourceBuffer(sourceBufferPrivate);
}

void MediaSourceClientGStreamerMSE::flush(AtomicString trackId)
{
    ASSERT(WTF::isMainThread());

    // This is only for on-the-fly reenqueues after appends. When seeking, the seek will do its own flush.
    if (m_playerPrivate && !m_playerPrivate->m_seeking)
        m_playerPrivate->m_playbackPipeline->flush(trackId);
}

void MediaSourceClientGStreamerMSE::enqueueSample(Ref<MediaSample>&& sample)
{
    ASSERT(WTF::isMainThread());

    if (m_playerPrivate)
        m_playerPrivate->m_playbackPipeline->enqueueSample(WTFMove(sample));
}

void MediaSourceClientGStreamerMSE::allSamplesInTrackEnqueued(const AtomicString& trackId)
{
    ASSERT(WTF::isMainThread());

    if (m_playerPrivate)
        m_playerPrivate->m_playbackPipeline->allSamplesInTrackEnqueued(trackId);
}

GRefPtr<WebKitMediaSrc> MediaSourceClientGStreamerMSE::webKitMediaSrc()
{
    ASSERT(WTF::isMainThread());

    if (!m_playerPrivate)
        return nullptr;

    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(m_playerPrivate->m_source.get());

    ASSERT(WEBKIT_IS_MEDIA_SRC(source));

    return source;
}

void MediaSourceClientGStreamerMSE::clearPlayerPrivate()
{
    ASSERT(WTF::isMainThread());

    m_playerPrivate = nullptr;
}

} // namespace WebCore.

#endif // USE(GSTREAMER)
