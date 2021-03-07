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
#include "SourceBufferPrivate.h"
#include "SourceBufferPrivateClient.h"
#include "WebKitMediaSourceGStreamer.h"
#include <wtf/LoggerHelper.h>

namespace WebCore {

class MediaSourcePrivateGStreamer;

class SourceBufferPrivateGStreamer final : public SourceBufferPrivate {
public:
    static Ref<SourceBufferPrivateGStreamer> create(MediaSourcePrivateGStreamer*, const ContentType&, MediaPlayerPrivateGStreamerMSE&);
    virtual ~SourceBufferPrivateGStreamer() = default;

    void clearMediaSource() { m_mediaSource = nullptr; }

    void append(Vector<unsigned char>&&) final;
    void abort() final;
    void resetParserState() final;
    void removedFromMediaSource() final;
    MediaPlayer::ReadyState readyState() const final;
    void setReadyState(MediaPlayer::ReadyState) final;

    void flush(const AtomString&) final;
    void enqueueSample(Ref<MediaSample>&&, const AtomString&) final;
    void allSamplesInTrackEnqueued(const AtomString&) final;
    bool isReadyForMoreSamples(const AtomString&) final;
    void setActive(bool) final;
    bool isActive() const final;
    void notifyClientWhenReadyForMoreSamples(const AtomString&) final;

    void setReadyForMoreSamples(bool);
    void notifyReadyForMoreSamples();

    void didReceiveInitializationSegment(SourceBufferPrivateClient::InitializationSegment&&, CompletionHandler<void()>&&);
    void didReceiveSample(Ref<MediaSample>&&);
    void didReceiveAllPendingSamples();
    void appendParsingFailed();

    bool isSeeking() const final;
    MediaTime currentMediaTime() const final;
    MediaTime duration() const final;

    ContentType type() const { return m_type; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const override { return "SourceBufferPrivateGStreamer"; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const Logger& sourceBufferLogger() const final { return m_logger; }
    const void* sourceBufferLogIdentifier() final { return logIdentifier(); }
#endif

private:
    SourceBufferPrivateGStreamer(MediaSourcePrivateGStreamer*, const ContentType&, MediaPlayerPrivateGStreamerMSE&);

    MediaSourcePrivateGStreamer* m_mediaSource;
    bool m_isActive { false };
    ContentType m_type;
    MediaPlayerPrivateGStreamerMSE& m_playerPrivate;
    UniqueRef<AppendPipeline> m_appendPipeline;
    bool m_isReadyForMoreSamples = true;
    bool m_notifyWhenReadyForMoreSamples = false;
    AtomString m_trackId;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

}

#endif
