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
    : m_playerPrivate(playerPrivate)
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

    ASSERT(sourceBufferPrivate);

    RefPtr<AppendPipeline> appendPipeline = adoptRef(new AppendPipeline(*this, *sourceBufferPrivate, m_playerPrivate));
    GST_TRACE("Adding SourceBuffer to AppendPipeline: this=%p sourceBuffer=%p appendPipeline=%p", this, sourceBufferPrivate.get(), appendPipeline.get());
    m_playerPrivate.m_appendPipelinesMap.add(sourceBufferPrivate, appendPipeline);

    return MediaSourcePrivate::Ok;
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
    m_playerPrivate.durationChanged();
}

void MediaSourceClientGStreamerMSE::abort(RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivate)
{
    ASSERT(WTF::isMainThread());

    GST_DEBUG("aborting");

    RefPtr<AppendPipeline> appendPipeline = m_playerPrivate.m_appendPipelinesMap.get(sourceBufferPrivate);

    ASSERT(appendPipeline);

    appendPipeline->resetParserState();
}

void MediaSourceClientGStreamerMSE::resetParserState(RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivate)
{
    ASSERT(WTF::isMainThread());

    GST_DEBUG("resetting parser state");

    RefPtr<AppendPipeline> appendPipeline = m_playerPrivate.m_appendPipelinesMap.get(sourceBufferPrivate);

    ASSERT(appendPipeline);

    appendPipeline->resetParserState();
}

void MediaSourceClientGStreamerMSE::append(RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivate, Vector<unsigned char>&& data)
{
    ASSERT(WTF::isMainThread());

    GST_DEBUG("Appending %zu bytes", data.size());

    RefPtr<AppendPipeline> appendPipeline = m_playerPrivate.m_appendPipelinesMap.get(sourceBufferPrivate);

    ASSERT(appendPipeline);

    // Wrap the whole Vector object in case the data is stored in the inlined buffer.
    auto* bufferData = data.data();
    auto bufferLength = data.size();
    GRefPtr<GstBuffer> buffer = adoptGRef(gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(0), bufferData, bufferLength, 0, bufferLength, new Vector<unsigned char>(WTFMove(data)),
        [](gpointer data)
        {
            delete static_cast<Vector<unsigned char>*>(data);
        }));

    appendPipeline->pushNewBuffer(WTFMove(buffer));
}

void MediaSourceClientGStreamerMSE::removedFromMediaSource(RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivate)
{
    ASSERT(WTF::isMainThread());

    // Remove the AppendPipeline from the map. This should cause its destruction since there should be no alive
    // references at this point.
    ASSERT(m_playerPrivate.m_appendPipelinesMap.get(sourceBufferPrivate)->hasOneRef());
    m_playerPrivate.m_appendPipelinesMap.remove(sourceBufferPrivate);

    if (!sourceBufferPrivate->trackId().isNull())
        webKitMediaSrcRemoveStream(WEBKIT_MEDIA_SRC(m_playerPrivate.m_source.get()), sourceBufferPrivate->trackId());
}

void MediaSourceClientGStreamerMSE::flush(AtomString trackId)
{
    ASSERT(WTF::isMainThread());

    // This is only for on-the-fly reenqueues after appends. When seeking, the seek will do its own flush.
    if (!m_playerPrivate.m_seeking)
        webKitMediaSrcFlush(WEBKIT_MEDIA_SRC(m_playerPrivate.m_source.get()), trackId);
}

void MediaSourceClientGStreamerMSE::enqueueSample(Ref<MediaSample>&& sample, AtomString trackId)
{
    ASSERT(WTF::isMainThread());

    GST_TRACE("enqueing sample trackId=%s PTS=%f presentationSize=%.0fx%.0f at %" GST_TIME_FORMAT " duration: %" GST_TIME_FORMAT,
        trackId.string().utf8().data(), sample->presentationTime().toFloat(),
        sample->presentationSize().width(), sample->presentationSize().height(),
        GST_TIME_ARGS(WebCore::toGstClockTime(sample->presentationTime())),
        GST_TIME_ARGS(WebCore::toGstClockTime(sample->duration())));

    GRefPtr<GstSample> gstSample = sample->platformSample().sample.gstSample;
    ASSERT(gstSample);
    ASSERT(gst_sample_get_buffer(gstSample.get()));

    webKitMediaSrcEnqueueSample(WEBKIT_MEDIA_SRC(m_playerPrivate.m_source.get()), trackId, WTFMove(gstSample));
}

bool MediaSourceClientGStreamerMSE::isReadyForMoreSamples(const AtomString& trackId)
{
    return webKitMediaSrcIsReadyForMoreSamples(WEBKIT_MEDIA_SRC(m_playerPrivate.m_source.get()), trackId);
}

void MediaSourceClientGStreamerMSE::notifyClientWhenReadyForMoreSamples(const AtomString& trackId, SourceBufferPrivateClient* sourceBuffer)
{
    webKitMediaSrcNotifyWhenReadyForMoreSamples(WEBKIT_MEDIA_SRC(m_playerPrivate.m_source.get()), trackId, sourceBuffer);
}

void MediaSourceClientGStreamerMSE::allSamplesInTrackEnqueued(const AtomString& trackId)
{
    ASSERT(WTF::isMainThread());

    webKitMediaSrcEndOfStream(WEBKIT_MEDIA_SRC(m_playerPrivate.m_source.get()), trackId);
}

GRefPtr<WebKitMediaSrc> MediaSourceClientGStreamerMSE::webKitMediaSrc()
{
    ASSERT(WTF::isMainThread());

    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(m_playerPrivate.m_source.get());

    ASSERT(WEBKIT_IS_MEDIA_SRC(source));

    return source;
}

} // namespace WebCore.

#endif // USE(GSTREAMER)
