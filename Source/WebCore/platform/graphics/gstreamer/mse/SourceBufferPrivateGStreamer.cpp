/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Metrological Group B.V.
 * Copyright (C) 2015, 2016 Igalia, S.L
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
#include "SourceBufferPrivateGStreamer.h"

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "AppendPipeline.h"
#include "ContentType.h"
#include "GStreamerCommon.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "MediaSample.h"
#include "MediaSourcePrivateGStreamer.h"
#include "NotImplemented.h"
#include "PlaybackPipeline.h"
#include "WebKitMediaSourceGStreamer.h"

namespace WebCore {

Ref<SourceBufferPrivateGStreamer> SourceBufferPrivateGStreamer::create(MediaSourcePrivateGStreamer* mediaSource, const ContentType& contentType, MediaPlayerPrivateGStreamerMSE& playerPrivate)
{
    return adoptRef(*new SourceBufferPrivateGStreamer(mediaSource, contentType, playerPrivate));
}

SourceBufferPrivateGStreamer::SourceBufferPrivateGStreamer(MediaSourcePrivateGStreamer* mediaSource, const ContentType& contentType, MediaPlayerPrivateGStreamerMSE& playerPrivate)
    : SourceBufferPrivate()
    , m_mediaSource(mediaSource)
    , m_type(contentType)
    , m_playerPrivate(playerPrivate)
    , m_appendPipeline(makeUniqueRef<AppendPipeline>(*this, playerPrivate))
#if !RELEASE_LOG_DISABLED
    , m_logger(mediaSource->logger())
    , m_logIdentifier(mediaSource->nextSourceBufferLogIdentifier())
#endif
{
}

void SourceBufferPrivateGStreamer::append(Vector<unsigned char>&& data)
{
    ASSERT(isMainThread());
    ASSERT(m_mediaSource);
    ASSERT(m_client);

    GST_DEBUG("Appending %zu bytes", data.size());
    // Wrap the whole Vector object in case the data is stored in the inlined buffer.
    auto* bufferData = data.data();
    auto bufferLength = data.size();
    GRefPtr<GstBuffer> buffer = adoptGRef(gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(0), bufferData, bufferLength, 0, bufferLength, new Vector<unsigned char>(WTFMove(data)),
        [](gpointer data)
        {
            delete static_cast<Vector<unsigned char>*>(data);
        }));

    m_appendPipeline->pushNewBuffer(WTFMove(buffer));
}

void SourceBufferPrivateGStreamer::abort()
{
    ASSERT(isMainThread());
    GST_DEBUG("aborting");
    m_appendPipeline->resetParserState();
}

void SourceBufferPrivateGStreamer::resetParserState()
{
    ASSERT(isMainThread());
    GST_DEBUG("resetting parser state");
    m_appendPipeline->resetParserState();
}

void SourceBufferPrivateGStreamer::removedFromMediaSource()
{
    ASSERT(isMainThread());
    clearTrackBuffers();
    m_mediaSource->removeSourceBuffer(this);
    m_playerPrivate.playbackPipeline()->removeSourceBuffer(this);
    // After this only SourceBuffer should hold a reference to us, which will be destroyed eventually (when JS
    // GC releases the last reference). Until then SourceBuffer is in "removed" state and won't use SourceBufferPrivate.
    ASSERT(refCount() == 1);
}

MediaPlayer::ReadyState SourceBufferPrivateGStreamer::readyState() const
{
    return m_mediaSource->readyState();
}

void SourceBufferPrivateGStreamer::setReadyState(MediaPlayer::ReadyState state)
{
    m_mediaSource->setReadyState(state);
}

void SourceBufferPrivateGStreamer::flush(const AtomString& trackId)
{
    ASSERT(isMainThread());

    // This is only for on-the-fly reenqueues after appends. When seeking, the seek will do its own flush.
    if (!m_playerPrivate.seeking())
        m_playerPrivate.playbackPipeline()->flush(trackId);
}

void SourceBufferPrivateGStreamer::enqueueSample(Ref<MediaSample>&& sample, const AtomString&)
{
    ASSERT(isMainThread());
    m_notifyWhenReadyForMoreSamples = false;

    m_playerPrivate.playbackPipeline()->enqueueSample(WTFMove(sample));
}

void SourceBufferPrivateGStreamer::allSamplesInTrackEnqueued(const AtomString& trackId)
{
    ASSERT(isMainThread());
    m_playerPrivate.playbackPipeline()->allSamplesInTrackEnqueued(trackId);
}

bool SourceBufferPrivateGStreamer::isReadyForMoreSamples(const AtomString&)
{
    return m_isReadyForMoreSamples;
}

void SourceBufferPrivateGStreamer::setReadyForMoreSamples(bool isReady)
{
    ASSERT(WTF::isMainThread());
    m_isReadyForMoreSamples = isReady;
}

void SourceBufferPrivateGStreamer::notifyReadyForMoreSamples()
{
    ASSERT(WTF::isMainThread());
    setReadyForMoreSamples(true);
    if (m_notifyWhenReadyForMoreSamples)
        provideMediaData(m_trackId);
}

void SourceBufferPrivateGStreamer::setActive(bool isActive)
{
    m_isActive = isActive;
    m_mediaSource->sourceBufferPrivateDidChangeActiveState(this, isActive);
}

bool SourceBufferPrivateGStreamer::isActive() const
{
    return m_isActive;
}

void SourceBufferPrivateGStreamer::notifyClientWhenReadyForMoreSamples(const AtomString& trackId)
{
    ASSERT(WTF::isMainThread());
    m_notifyWhenReadyForMoreSamples = true;
    m_trackId = trackId;
}

void SourceBufferPrivateGStreamer::didReceiveInitializationSegment(SourceBufferPrivateClient::InitializationSegment&& initializationSegment, CompletionHandler<void()>&& completionHandler)
{
    SourceBufferPrivate::didReceiveInitializationSegment(WTFMove(initializationSegment), WTFMove(completionHandler));
}

void SourceBufferPrivateGStreamer::didReceiveSample(Ref<MediaSample>&& sample)
{
    SourceBufferPrivate::didReceiveSample(WTFMove(sample));
}

void SourceBufferPrivateGStreamer::didReceiveAllPendingSamples()
{
    SourceBufferPrivate::appendCompleted(true, m_mediaSource ? m_mediaSource->isEnded() : true);
}

void SourceBufferPrivateGStreamer::appendParsingFailed()
{
    SourceBufferPrivate::appendCompleted(false, m_mediaSource ? m_mediaSource->isEnded() : true);
}

bool SourceBufferPrivateGStreamer::isSeeking() const
{
    return m_mediaSource && m_mediaSource->isSeeking();
}

MediaTime SourceBufferPrivateGStreamer::currentMediaTime() const
{
    if (!m_mediaSource)
        return { };

    return m_mediaSource->currentMediaTime();
}

MediaTime SourceBufferPrivateGStreamer::duration() const
{
    if (!m_mediaSource)
        return { };

    return m_mediaSource->duration();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateGStreamer::logChannel() const
{
    return LogMediaSource;
}
#endif

}
#endif
