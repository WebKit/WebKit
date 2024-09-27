/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016, 2017 Metrological Group B.V.
 * Copyright (C) 2015, 2016, 2017 Igalia, S.L
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

#pragma once

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "ContentType.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "MediaSourceTrackGStreamer.h"
#include "SourceBufferPrivate.h"
#include "SourceBufferPrivateClient.h"
#include "TrackPrivateBaseGStreamer.h"
#include "WebKitMediaSourceGStreamer.h"
#include <optional>
#include <wtf/LoggerHelper.h>
#include <wtf/StdUnorderedMap.h>

namespace WebCore {

using TrackID = uint64_t;

class AppendPipeline;
class MediaSourcePrivateGStreamer;

class SourceBufferPrivateGStreamer final : public SourceBufferPrivate, public CanMakeWeakPtr<SourceBufferPrivateGStreamer> {
public:
    static bool isContentTypeSupported(const ContentType&);
    static Ref<SourceBufferPrivateGStreamer> create(MediaSourcePrivateGStreamer&, const ContentType&);
    ~SourceBufferPrivateGStreamer();

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::GStreamer; }

    Ref<MediaPromise> appendInternal(Ref<SharedBuffer>&&) final;
    void resetParserStateInternal() final;
    void removedFromMediaSource() final;

    void flush(TrackID) final;
    void enqueueSample(Ref<MediaSample>&&, TrackID) final;
    void allSamplesInTrackEnqueued(TrackID) final;
    bool isReadyForMoreSamples(TrackID) final;

    bool precheckInitializationSegment(const InitializationSegment&) final;
    void processInitializationSegment(std::optional<InitializationSegment>&&) final;

    void didReceiveAllPendingSamples();
    void appendParsingFailed();

    auto& tracks() const { return m_tracks; }

    ContentType type() const { return m_type; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const override { return "SourceBufferPrivateGStreamer"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const Logger& sourceBufferLogger() const final { return m_logger; }
    uint64_t sourceBufferLogIdentifier() final { return logIdentifier(); }
#endif

    size_t platformMaximumBufferSize() const override;
    size_t platformEvictionThreshold() const final;

private:
    friend class AppendPipeline;

    SourceBufferPrivateGStreamer(MediaSourcePrivateGStreamer&, const ContentType&);
    RefPtr<MediaPlayerPrivateGStreamerMSE> player() const;

    void notifyClientWhenReadyForMoreSamples(TrackID) override;

    void detach() final;

    bool m_hasBeenRemovedFromMediaSource { false };
    ContentType m_type;
    std::unique_ptr<AppendPipeline> m_appendPipeline;
    StdUnorderedMap<TrackID, RefPtr<MediaSourceTrackGStreamer>> m_tracks;
    std::optional<MediaPromise::Producer> m_appendPromise;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceBufferPrivateGStreamer)
static bool isType(const WebCore::SourceBufferPrivate& sourceBuffer) { return sourceBuffer.platformType() == WebCore::MediaPlatformType::GStreamer; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
