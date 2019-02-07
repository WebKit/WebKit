/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include "MediaSourcePrivate.h"
#include <wtf/MediaTime.h>

namespace WebCore {

class MockMediaPlayerMediaSource;
class MockSourceBufferPrivate;

class MockMediaSourcePrivate final : public MediaSourcePrivate {
public:
    static Ref<MockMediaSourcePrivate> create(MockMediaPlayerMediaSource&, MediaSourcePrivateClient&);
    virtual ~MockMediaSourcePrivate();

    const Vector<MockSourceBufferPrivate*>& activeSourceBuffers() const { return m_activeSourceBuffers; }

    bool hasAudio() const;
    bool hasVideo() const;

    MediaTime duration();
    std::unique_ptr<PlatformTimeRanges> buffered();

    MockMediaPlayerMediaSource& player() const { return m_player; }

    void seekToTime(const MediaTime&);
    MediaTime seekToTime(const MediaTime&, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold);

    Optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics();

    void incrementTotalVideoFrames() { ++m_totalVideoFrames; }
    void incrementDroppedFrames() { ++m_droppedVideoFrames; }
    void incrementCorruptedFrames() { ++m_corruptedVideoFrames; }
    void incrementTotalFrameDelayBy(const MediaTime& delay) { m_totalFrameDelay += delay; }

#if !RELEASE_LOG_DISABLED
    const Logger& mediaSourceLogger() const;
    const void* mediaSourceLogIdentifier();
#endif

private:
    MockMediaSourcePrivate(MockMediaPlayerMediaSource&, MediaSourcePrivateClient&);

    // MediaSourcePrivate Overrides
    AddStatus addSourceBuffer(const ContentType&, RefPtr<SourceBufferPrivate>&) override;
    void durationChanged() override;
    void markEndOfStream(EndOfStreamStatus) override;
    void unmarkEndOfStream() override;
    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState) override;
    void waitForSeekCompleted() override;
    void seekCompleted() override;

    void sourceBufferPrivateDidChangeActiveState(MockSourceBufferPrivate*, bool active);
    void removeSourceBuffer(SourceBufferPrivate*);

    friend class MockSourceBufferPrivate;

    MockMediaPlayerMediaSource& m_player;
    Ref<MediaSourcePrivateClient> m_client;
    Vector<RefPtr<MockSourceBufferPrivate>> m_sourceBuffers;
    Vector<MockSourceBufferPrivate*> m_activeSourceBuffers;
    bool m_isEnded { false };

    unsigned m_totalVideoFrames { 0 };
    unsigned m_droppedVideoFrames { 0 };
    unsigned m_corruptedVideoFrames { 0 };
    MediaTime m_totalFrameDelay;
};

}

#endif // ENABLE(MEDIA_SOURCE)
