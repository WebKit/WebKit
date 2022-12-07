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

#if PLATFORM(WPE)
#include <wtf/text/StringToIntegerConversion.h>
#endif

GST_DEBUG_CATEGORY_EXTERN(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

namespace WebCore {

bool SourceBufferPrivateGStreamer::isContentTypeSupported(const ContentType& type)
{
    const auto& containerType = type.containerType();
    return containerType == "audio/mpeg"_s || containerType.endsWith("mp4"_s) || containerType.endsWith("aac"_s) || containerType.endsWith("webm"_s);
}

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

    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Appending %zu bytes", data.size());
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
    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "aborting");
    m_appendPipeline->resetParserState();
}

void SourceBufferPrivateGStreamer::resetParserState()
{
    ASSERT(isMainThread());
    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "resetting parser state");
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

    m_appendPipeline->stopParser();
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
        GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Source element has not emitted tracks yet, so we only need to clear the queue. trackId = '%s'", trackId.string().utf8().data());
        MediaSourceTrackGStreamer* track = m_tracks.get(trackId);
        track->clearQueue();
        return;
    }

    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Source element has emitted tracks, let it handle the flush, which may cause a pipeline flush as well. trackId = '%s'", trackId.string().utf8().data());
    webKitMediaSrcFlush(m_playerPrivate.webKitMediaSrc(), trackId);
}

void SourceBufferPrivateGStreamer::enqueueSample(Ref<MediaSample>&& sample, const AtomString& trackId)
{
    ASSERT(isMainThread());

    GRefPtr<GstSample> gstSample = sample->platformSample().sample.gstSample;
    ASSERT(gstSample);
    ASSERT(gst_sample_get_buffer(gstSample.get()));

    GST_TRACE_OBJECT(m_playerPrivate.pipeline(), "enqueing sample trackId=%s presentationSize=%.0fx%.0f at PTS %" GST_TIME_FORMAT " duration: %" GST_TIME_FORMAT,
        trackId.string().utf8().data(), sample->presentationSize().width(), sample->presentationSize().height(),
        GST_TIME_ARGS(WebCore::toGstClockTime(sample->presentationTime())),
        GST_TIME_ARGS(WebCore::toGstClockTime(sample->duration())));

    MediaSourceTrackGStreamer* track = m_tracks.get(trackId);
    track->enqueueObject(adoptGRef(GST_MINI_OBJECT(gstSample.leakRef())));
}

bool SourceBufferPrivateGStreamer::isReadyForMoreSamples(const AtomString& trackId)
{
    ASSERT(isMainThread());
    MediaSourceTrackGStreamer* track = m_tracks.get(trackId);
    bool ret = track->isReadyForMoreSamples();
    GST_TRACE_OBJECT(m_playerPrivate.pipeline(), "isReadyForMoreSamples: %s", boolForPrinting(ret));
    return ret;
}

void SourceBufferPrivateGStreamer::notifyClientWhenReadyForMoreSamples(const AtomString& trackId)
{
    ASSERT(isMainThread());
    MediaSourceTrackGStreamer* track = m_tracks.get(trackId);
    track->notifyWhenReadyForMoreSamples([protector = Ref { *this }, this, trackId]() mutable {
        RunLoop::main().dispatch([protector = WTFMove(protector), this, trackId]() {
            if (!m_hasBeenRemovedFromMediaSource)
                provideMediaData(trackId);
        });
    });
}

void SourceBufferPrivateGStreamer::allSamplesInTrackEnqueued(const AtomString& trackId)
{
    ASSERT(isMainThread());
    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Enqueueing EOS for track '%s'", trackId.string().utf8().data());
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

void SourceBufferPrivateGStreamer::didReceiveInitializationSegment(SourceBufferPrivateClient::InitializationSegment&& initializationSegment, CompletionHandler<void(SourceBufferPrivateClient::ReceiveResult)>&& completionHandler)
{
    m_hasReceivedInitializationSegment = true;
    for (auto& trackInfo : initializationSegment.videoTracks) {
        GRefPtr<GstCaps> initialCaps = static_cast<VideoTrackPrivateGStreamer*>(trackInfo.track.get())->initialCaps();
        ASSERT(initialCaps);
        if (!m_tracks.contains(trackInfo.track->id()))
            m_tracks.add(trackInfo.track->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Video, trackInfo.track->id(), WTFMove(initialCaps)));
    }
    for (auto& trackInfo : initializationSegment.audioTracks) {
        GRefPtr<GstCaps> initialCaps = static_cast<AudioTrackPrivateGStreamer*>(trackInfo.track.get())->initialCaps();
        ASSERT(initialCaps);
        if (!m_tracks.contains(trackInfo.track->id()))
            m_tracks.add(trackInfo.track->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Audio, trackInfo.track->id(), WTFMove(initialCaps)));
    }
    for (auto& trackInfo : initializationSegment.textTracks) {
        GRefPtr<GstCaps> initialCaps = static_cast<InbandTextTrackPrivateGStreamer*>(trackInfo.track.get())->initialCaps();
        ASSERT(initialCaps);
        if (!m_tracks.contains(trackInfo.track->id()))
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

size_t SourceBufferPrivateGStreamer::platformMaximumBufferSize() const
{
#if PLATFORM(WPE)
    if (!m_client)
        return 0;

    static size_t maxBufferSizeVideo = 0;
    static size_t maxBufferSizeAudio = 0;
    static size_t maxBufferSizeText = 0;

    static std::once_flag once;
    std::call_once(once, []() {
        // Syntax: Case insensitive, full type (audio, video, text), compact type (a, v, t),
        //         wildcard (*), unit multipliers (M=Mb, K=Kb, <empty>=bytes).
        // Examples: MSE_MAX_BUFFER_SIZE='V:50M,audio:12k,TeXT:500K'
        //           MSE_MAX_BUFFER_SIZE='*:100M'
        //           MSE_MAX_BUFFER_SIZE='video:90M,T:100000'

        auto s = String::fromLatin1(std::getenv("MSE_MAX_BUFFER_SIZE"));
        if (!s.isEmpty()) {
            Vector<String> entries = s.split(',');
            for (const String& entry : entries) {
                Vector<String> keyvalue = entry.split(':');
                if (keyvalue.size() != 2)
                    continue;
                String key = keyvalue[0].stripWhiteSpace().convertToLowercaseWithoutLocale();
                String value = keyvalue[1].stripWhiteSpace().convertToLowercaseWithoutLocale();
                size_t units = 1;
                if (value.endsWith('k'))
                    units = 1024;
                else if (value.endsWith('m'))
                    units = 1024 * 1024;
                if (units != 1)
                    value = value.left(value.length()-1);
                auto parsedSize = parseInteger<size_t>(value);
                if (!parsedSize)
                    continue;
                size_t size = *parsedSize;

                if (key == "a"_s || key == "audio"_s || key == "*"_s)
                    maxBufferSizeAudio = size * units;
                if (key == "v"_s || key == "video"_s || key == "*"_s)
                    maxBufferSizeVideo = size * units;
                if (key == "t"_s || key == "text"_s || key == "*"_s)
                    maxBufferSizeText = size * units;
            }
        }
    });

    // If any track type size isn't specified, we consider that it has no limit and the values from the
    // element have to be used. Otherwise, the track limits are accumulative.
    do {
        bool hasVideo = false;
        bool hasAudio = false;
        bool hasText = false;
        size_t bufferSize = 0;

        for (auto track : m_tracks.values()) {
            switch (track->type()) {
            case TrackPrivateBaseGStreamer::Video:
                hasVideo = true;
                break;
            case TrackPrivateBaseGStreamer::Audio:
                hasAudio = true;
                break;
            case TrackPrivateBaseGStreamer::Text:
                hasText = true;
                break;
            default:
                break;
            }
        }

        if (hasVideo) {
            if (maxBufferSizeVideo)
                bufferSize += maxBufferSizeVideo;
            else
                break;
        }
        if (hasAudio) {
            if (maxBufferSizeAudio)
                bufferSize += maxBufferSizeAudio;
            else
                break;
        }
        if (hasText) {
            if (maxBufferSizeText)
                bufferSize += maxBufferSizeText;
            else
                break;
        }
        if (bufferSize)
            return bufferSize;
    } while (false);
#endif

    return 0;
}

}
#endif
