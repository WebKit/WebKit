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
#include "AudioTrackPrivateGStreamer.h"
#include "ContentType.h"
#include "GStreamerCommon.h"
#include "InbandTextTrackPrivate.h"
#include "InbandTextTrackPrivateGStreamer.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "MediaSample.h"
#include "MediaSourcePrivateGStreamer.h"
#include "MediaSourceTrackGStreamer.h"
#include "NotImplemented.h"
#include "VideoTrackPrivateGStreamer.h"
#include "WebKitMediaSourceGStreamer.h"

GST_DEBUG_CATEGORY_EXTERN(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

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

    for (auto& track : m_tracks.values())
        track->remove();
    m_hasBeenRemovedFromMediaSource = true;
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

    if (!m_playerPrivate.hasAllTracks()) {
        // Source element has not emitted tracks yet, so we only need to clear the queue.
        MediaSourceTrackGStreamer* track = m_tracks.get(trackId);
        track->clearQueue();
        return;
    }

    // Source element has emitted tracks, let it handle the flush, which may cause a pipeline flush as well.
    webKitMediaSrcFlush(m_playerPrivate.webKitMediaSrc(), trackId);
}

void SourceBufferPrivateGStreamer::enqueueSample(Ref<MediaSample>&& sample, const AtomString& trackId)
{
    ASSERT(isMainThread());

    GST_TRACE("enqueing sample trackId=%s PTS=%s presentationSize=%.0fx%.0f at %" GST_TIME_FORMAT " duration: %" GST_TIME_FORMAT,
        trackId.string().utf8().data(), sample->presentationTime().toString().utf8().data(),
        sample->presentationSize().width(), sample->presentationSize().height(),
        GST_TIME_ARGS(WebCore::toGstClockTime(sample->presentationTime())),
        GST_TIME_ARGS(WebCore::toGstClockTime(sample->duration())));

    GRefPtr<GstSample> gstSample = sample->platformSample().sample.gstSample;
    ASSERT(gstSample);
    ASSERT(gst_sample_get_buffer(gstSample.get()));

    MediaSourceTrackGStreamer* track = m_tracks.get(trackId);
    track->enqueueObject(adoptGRef(GST_MINI_OBJECT(gstSample.leakRef())));
}

bool SourceBufferPrivateGStreamer::isReadyForMoreSamples(const AtomString& trackId)
{
    ASSERT(isMainThread());
    MediaSourceTrackGStreamer* track = m_tracks.get(trackId);
    bool ret = track->isReadyForMoreSamples();
    GST_TRACE("SourceBufferPrivate(%p) - isReadyForMoreSamples: %s", this, boolForPrinting(ret));
    return ret;
}

void SourceBufferPrivateGStreamer::notifyClientWhenReadyForMoreSamples(const AtomString& trackId)
{
    ASSERT(isMainThread());
    MediaSourceTrackGStreamer* track = m_tracks.get(trackId);
    track->notifyWhenReadyForMoreSamples([protector = makeRef(*this), this, trackId]() mutable {
        RunLoop::main().dispatch([protector = WTFMove(protector), this, trackId]() {
            if (!m_hasBeenRemovedFromMediaSource)
                provideMediaData(trackId);
        });
    });
}

void SourceBufferPrivateGStreamer::allSamplesInTrackEnqueued(const AtomString& trackId)
{
    ASSERT(isMainThread());
    GST_DEBUG("Enqueueing EOS for track '%s'", trackId.string().utf8().data());
    MediaSourceTrackGStreamer* track = m_tracks.get(trackId);
    track->enqueueObject(adoptGRef(GST_MINI_OBJECT(gst_event_new_eos())));
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

void SourceBufferPrivateGStreamer::didReceiveInitializationSegment(SourceBufferPrivateClient::InitializationSegment&& initializationSegment, CompletionHandler<void()>&& completionHandler)
{
    m_hasReceivedInitializationSegment = true;
    for (auto& trackInfo : initializationSegment.videoTracks) {
        GRefPtr<GstCaps> initialCaps = static_cast<VideoTrackPrivateGStreamer*>(trackInfo.track.get())->initialCaps();
        ASSERT(initialCaps);
        m_tracks.add(trackInfo.track->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Video, trackInfo.track->id(), WTFMove(initialCaps)));
    }
    for (auto& trackInfo : initializationSegment.audioTracks) {
        GRefPtr<GstCaps> initialCaps = static_cast<AudioTrackPrivateGStreamer*>(trackInfo.track.get())->initialCaps();
        ASSERT(initialCaps);
        m_tracks.add(trackInfo.track->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Audio, trackInfo.track->id(), WTFMove(initialCaps)));
    }
    for (auto& trackInfo : initializationSegment.textTracks) {
        GRefPtr<GstCaps> initialCaps = static_cast<InbandTextTrackPrivateGStreamer*>(trackInfo.track.get())->initialCaps();
        ASSERT(initialCaps);
        m_tracks.add(trackInfo.track->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Text, trackInfo.track->id(), WTFMove(initialCaps)));
    }

    m_mediaSource->startPlaybackIfHasAllTracks();

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
