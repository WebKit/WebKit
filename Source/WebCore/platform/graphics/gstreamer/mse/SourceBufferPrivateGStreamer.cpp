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
#include <wtf/NativePromise.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

namespace WebCore {

bool SourceBufferPrivateGStreamer::isContentTypeSupported(const ContentType& type)
{
    const auto& containerType = type.containerType();
    return containerType == "audio/mpeg"_s || containerType.endsWith("mp4"_s) || containerType.endsWith("aac"_s) || containerType.endsWith("webm"_s);
}

Ref<SourceBufferPrivateGStreamer> SourceBufferPrivateGStreamer::create(MediaSourcePrivateGStreamer& mediaSource, const ContentType& contentType, MediaPlayerPrivateGStreamerMSE& playerPrivate)
{
    return adoptRef(*new SourceBufferPrivateGStreamer(mediaSource, contentType, playerPrivate));
}

SourceBufferPrivateGStreamer::SourceBufferPrivateGStreamer(MediaSourcePrivateGStreamer& mediaSource, const ContentType& contentType, MediaPlayerPrivateGStreamerMSE& playerPrivate)
    : SourceBufferPrivate(mediaSource)
    , m_type(contentType)
    , m_playerPrivate(playerPrivate)
    , m_appendPipeline(makeUnique<AppendPipeline>(*this, playerPrivate))
#if !RELEASE_LOG_DISABLED
    , m_logger(mediaSource.logger())
    , m_logIdentifier(mediaSource.nextSourceBufferLogIdentifier())
#endif
{
}

SourceBufferPrivateGStreamer::~SourceBufferPrivateGStreamer()
{
    if (!m_appendPromise)
        return;

    m_appendPromise->reject(PlatformMediaError::BufferRemoved);
    m_appendPromise.reset();
}

Ref<MediaPromise> SourceBufferPrivateGStreamer::appendInternal(Ref<SharedBuffer>&& data)
{
    ASSERT(isMainThread());

    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Appending %zu bytes", data->size());

    ASSERT(!m_appendPromise);
    m_appendPromise.emplace();
    gpointer bufferData = const_cast<void*>(static_cast<const void*>(data->data()));
    auto bufferLength = data->size();
    GRefPtr<GstBuffer> buffer = adoptGRef(gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(0), bufferData, bufferLength, 0, bufferLength, &data.leakRef(),
        [](gpointer data)
        {
            static_cast<SharedBuffer*>(data)->deref();
        }));

    m_appendPipeline->pushNewBuffer(WTFMove(buffer));
    return *m_appendPromise;
}

void SourceBufferPrivateGStreamer::resetParserStateInternal()
{
    ASSERT(isMainThread());
    if (!m_appendPipeline)
        return;

    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "resetting parser state");
    m_appendPipeline->resetParserState();
}

void SourceBufferPrivateGStreamer::removedFromMediaSource()
{
    ASSERT(isMainThread());

    for (auto& [_, track] : tracks())
        track->remove();
    m_hasBeenRemovedFromMediaSource = true;

    m_appendPipeline->stopParser();

    // Release the resources used by the AppendPipeline. This effectively makes the
    // SourceBufferPrivate useless. Ideally the entire instance should be destroyed. For now we
    // explicitely release the AppendPipeline because that's the biggest resource user. In case the
    // process remains alive, GC might kick in later on and release the SourceBufferPrivate.
    m_appendPipeline = nullptr;

    SourceBufferPrivate::removedFromMediaSource();
}

void SourceBufferPrivateGStreamer::flush(TrackID trackId)
{
    ASSERT(isMainThread());

    // This is only for on-the-fly reenqueues after appends. When seeking, the seek will do its own flush.

    RefPtr mediaSource = m_mediaSource.get();
    if (!mediaSource)
        return;

    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];
    if (!downcast<MediaSourcePrivateGStreamer>(mediaSource)->hasAllTracks()) {
        GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Source element has not emitted tracks yet, so we only need to clear the queue. trackId = '%s'", track->stringId().string().utf8().data());
        track->clearQueue();
        return;
    }

    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Source element has emitted tracks, let it handle the flush, which may cause a pipeline flush as well. trackId = '%s'", track->stringId().string().utf8().data());
    webKitMediaSrcFlush(m_playerPrivate.webKitMediaSrc(), track->stringId());
}

void SourceBufferPrivateGStreamer::enqueueSample(Ref<MediaSample>&& sample, TrackID trackId)
{
    ASSERT(isMainThread());

    GRefPtr<GstSample> gstSample = sample->platformSample().sample.gstSample;
    ASSERT(gstSample);
    ASSERT(gst_sample_get_buffer(gstSample.get()));

    GST_TRACE_OBJECT(m_playerPrivate.pipeline(), "enqueing sample trackId=%" PRIu64 " presentationSize=%.0fx%.0f at PTS %" GST_TIME_FORMAT " duration: %" GST_TIME_FORMAT,
        trackId, sample->presentationSize().width(), sample->presentationSize().height(),
        GST_TIME_ARGS(WebCore::toGstClockTime(sample->presentationTime())),
        GST_TIME_ARGS(WebCore::toGstClockTime(sample->duration())));

    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];
    track->enqueueObject(adoptGRef(GST_MINI_OBJECT(gstSample.leakRef())));
}

bool SourceBufferPrivateGStreamer::isReadyForMoreSamples(TrackID trackId)
{
    ASSERT(isMainThread());
    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];
    bool ret = track->isReadyForMoreSamples();
    GST_TRACE_OBJECT(m_playerPrivate.pipeline(), "isReadyForMoreSamples: %s", boolForPrinting(ret));
    return ret;
}

void SourceBufferPrivateGStreamer::notifyClientWhenReadyForMoreSamples(TrackID trackId)
{
    ASSERT(isMainThread());
    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];
    track->notifyWhenReadyForMoreSamples([protectedThis = Ref { *this }, this, trackId]() mutable {
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), this, trackId]() {
            if (!m_hasBeenRemovedFromMediaSource)
                provideMediaData(trackId);
        });
    });
}

void SourceBufferPrivateGStreamer::allSamplesInTrackEnqueued(TrackID trackId)
{
    ASSERT(isMainThread());
    ASSERT(m_tracks.contains(trackId));
    auto track = m_tracks[trackId];
    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Enqueueing EOS for track '%s'", track->stringId().string().utf8().data());
    track->enqueueObject(adoptGRef(GST_MINI_OBJECT(gst_event_new_eos())));
}

bool SourceBufferPrivateGStreamer::precheckInitializationSegment(const InitializationSegment& segment)
{
    for (auto& trackInfo : segment.videoTracks) {
        auto* videoTrackInfo = static_cast<VideoTrackPrivateGStreamer*>(trackInfo.track.get());
        GRefPtr<GstCaps> initialCaps = videoTrackInfo->initialCaps();
        ASSERT(initialCaps);
        if (!m_tracks.contains(trackInfo.track->id()))
            m_tracks.try_emplace(trackInfo.track->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Video, trackInfo.track->id(), videoTrackInfo->stringId(), WTFMove(initialCaps)));
    }
    for (auto& trackInfo : segment.audioTracks) {
        auto* audioTrackInfo = static_cast<AudioTrackPrivateGStreamer*>(trackInfo.track.get());
        GRefPtr<GstCaps> initialCaps = audioTrackInfo->initialCaps();
        ASSERT(initialCaps);
        if (!m_tracks.contains(trackInfo.track->id()))
            m_tracks.try_emplace(trackInfo.track->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Audio, trackInfo.track->id(), audioTrackInfo->stringId(), WTFMove(initialCaps)));
    }
    for (auto& trackInfo : segment.textTracks) {
        auto* textTrackInfo = static_cast<InbandTextTrackPrivateGStreamer*>(trackInfo.track.get());
        GRefPtr<GstCaps> initialCaps = textTrackInfo->initialCaps();
        ASSERT(initialCaps);
        if (!m_tracks.contains(trackInfo.track->id()))
            m_tracks.try_emplace(trackInfo.track->id(), MediaSourceTrackGStreamer::create(TrackPrivateBaseGStreamer::TrackType::Text, trackInfo.track->id(), textTrackInfo->stringId(), WTFMove(initialCaps)));
    }

    return true;
}

void SourceBufferPrivateGStreamer::processInitializationSegment(std::optional<InitializationSegment>&& segment)
{
    if (RefPtr mediaSource = m_mediaSource.get(); mediaSource && segment)
        static_cast<MediaSourcePrivateGStreamer*>(mediaSource.get())->startPlaybackIfHasAllTracks();
}

void SourceBufferPrivateGStreamer::didReceiveAllPendingSamples()
{
    // TODO: didReceiveAllPendingSamples is called even when an error occurred.
    if (m_appendPromise) {
        m_appendPromise->resolve();
        m_appendPromise.reset();
    }
}

void SourceBufferPrivateGStreamer::appendParsingFailed()
{
    if (m_appendPromise) {
        m_appendPromise->reject(PlatformMediaError::ParsingError);
        m_appendPromise.reset();
    }
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
                auto key = keyvalue[0].trim(deprecatedIsSpaceOrNewline).convertToLowercaseWithoutLocale();
                auto value = keyvalue[1].trim(deprecatedIsSpaceOrNewline).convertToLowercaseWithoutLocale();
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
    // element have to be used. Otherwise, the track limits are accumulative. If everything is specified
    // but there's no track (eg: because we're processing an init segment that we don't know yet which
    // kind of track(s) is going to generate) we assume that the 3 kind of tracks might appear (audio,
    // video, text) and use all the accumulated limits at once to make room for any possible outcome.
    do {
        bool hasVideo = false;
        bool hasAudio = false;
        bool hasText = false;
        size_t bufferSize = 0;

        for (auto& [_, track] : m_tracks) {
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

        if (hasVideo || m_tracks.empty()) {
            if (maxBufferSizeVideo)
                bufferSize += maxBufferSizeVideo;
            else
                break;
        }
        if (hasAudio || m_tracks.empty()) {
            if (maxBufferSizeAudio)
                bufferSize += maxBufferSizeAudio;
            else
                break;
        }
        if (hasText || m_tracks.empty()) {
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

size_t SourceBufferPrivateGStreamer::platformEvictionThreshold() const
{
    static size_t evictionThreshold = 0;
    static std::once_flag once;
    std::call_once(once, []() {
        auto stringView = StringView::fromLatin1(std::getenv("MSE_BUFFER_SAMPLES_EVICTION_THRESHOLD"));
        if (!stringView.isEmpty())
            evictionThreshold = parseInteger<size_t>(stringView, 10).value_or(0);
    });
    return evictionThreshold;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)
