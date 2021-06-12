/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "SourceBufferPrivate.h"

namespace WebCore {

class AudioTrackPrivate;
class InbandTextTrackPrivate;
class MockInitializationBox;
class MockMediaSourcePrivate;
class MockSampleBox;
class TimeRanges;
class VideoTrackPrivate;

class MockSourceBufferPrivate final : public SourceBufferPrivate {
public:
    static Ref<MockSourceBufferPrivate> create(MockMediaSourcePrivate*);
    virtual ~MockSourceBufferPrivate();

    void clearMediaSource() { m_mediaSource = nullptr; }

private:
    explicit MockSourceBufferPrivate(MockMediaSourcePrivate*);

    // SourceBufferPrivate overrides
    void append(Vector<uint8_t>&&) final;
    void abort() final;
    void resetParserState() final;
    void removedFromMediaSource() final;
    MediaPlayer::ReadyState readyState() const final;
    void setReadyState(MediaPlayer::ReadyState) final;
    bool canSetMinimumUpcomingPresentationTime(const AtomString&) const final;
    void setMinimumUpcomingPresentationTime(const AtomString&, const MediaTime&) final;
    void clearMinimumUpcomingPresentationTime(const AtomString&) final;
    bool canSwitchToType(const ContentType&) final;
    bool isSeeking() const final;
    MediaTime currentMediaTime() const final;
    MediaTime duration() const final;

    void flush(const AtomString&) final { m_enqueuedSamples.clear(); m_minimumUpcomingPresentationTime = MediaTime::invalidTime(); }
    void enqueueSample(Ref<MediaSample>&&, const AtomString&) final;
    bool isReadyForMoreSamples(const AtomString&) final { return !m_maxQueueDepth || m_enqueuedSamples.size() < m_maxQueueDepth.value(); }
    void setActive(bool) final;
    bool isActive() const final;

    void enqueuedSamplesForTrackID(const AtomString&, CompletionHandler<void(Vector<String>&&)>&&) final;
    MediaTime minimumUpcomingPresentationTimeForTrackID(const AtomString&) final;
    void setMaximumQueueDepthForTrackID(const AtomString&, uint64_t) final;

    void didReceiveInitializationSegment(const MockInitializationBox&);
    void didReceiveSample(const MockSampleBox&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const override { return "MockSourceBufferPrivate"; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    const Logger& sourceBufferLogger() const final { return m_logger.get(); }
    const void* sourceBufferLogIdentifier() final { return logIdentifier(); }
#endif

    MockMediaSourcePrivate* m_mediaSource;
    bool m_isActive { false };
    MediaTime m_minimumUpcomingPresentationTime;
    Vector<String> m_enqueuedSamples;
    std::optional<uint64_t> m_maxQueueDepth;
    Vector<uint8_t> m_inputBuffer;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
