/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef MockMediaSourcePrivate_h
#define MockMediaSourcePrivate_h

#if ENABLE(MEDIA_SOURCE)

#include "MediaSourcePrivate.h"
#include <wtf/MediaTime.h>

namespace WebCore {

class MockMediaPlayerMediaSource;
class MockSourceBufferPrivate;
class TimeRanges;

class MockMediaSourcePrivate final : public MediaSourcePrivate {
public:
    static RefPtr<MockMediaSourcePrivate> create(MockMediaPlayerMediaSource*, MediaSourcePrivateClient*);
    virtual ~MockMediaSourcePrivate();

    const Vector<MockSourceBufferPrivate*>& activeSourceBuffers() const { return m_activeSourceBuffers; }

    bool hasAudio() const;
    bool hasVideo() const;

    MediaTime duration();
    std::unique_ptr<PlatformTimeRanges> buffered();

    MockMediaPlayerMediaSource* player() const { return m_player; }

    void seekToTime(const MediaTime&);
    MediaTime seekToTime(const MediaTime&, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold);

    unsigned long totalVideoFrames() const { return m_totalVideoFrames; }
    unsigned long droppedVideoFrames() const  { return m_droppedVideoFrames; }
    unsigned long corruptedVideoFrames() const { return m_corruptedVideoFrames; }
    MediaTime totalFrameDelay() const { return m_totalFrameDelay; }

    void incrementTotalVideoFrames() { ++m_totalVideoFrames; }
    void incrementDroppedFrames() { ++m_droppedVideoFrames; }
    void incrementCorruptedFrames() { ++m_corruptedVideoFrames; }
    void incrementTotalFrameDelayBy(const MediaTime& delay) { m_totalFrameDelay += delay; }

private:
    MockMediaSourcePrivate(MockMediaPlayerMediaSource*, MediaSourcePrivateClient*);

    // MediaSourcePrivate Overrides
    virtual AddStatus addSourceBuffer(const ContentType&, RefPtr<SourceBufferPrivate>&) override;
    virtual void durationChanged() override;
    virtual void markEndOfStream(EndOfStreamStatus) override;
    virtual void unmarkEndOfStream() override;
    virtual MediaPlayer::ReadyState readyState() const override;
    virtual void setReadyState(MediaPlayer::ReadyState) override;
    virtual void waitForSeekCompleted() override;
    virtual void seekCompleted() override;

    void sourceBufferPrivateDidChangeActiveState(MockSourceBufferPrivate*, bool active);
    void removeSourceBuffer(SourceBufferPrivate*);

    friend class MockSourceBufferPrivate;

    MockMediaPlayerMediaSource* m_player;
    RefPtr<MediaSourcePrivateClient> m_client;
    Vector<RefPtr<MockSourceBufferPrivate>> m_sourceBuffers;
    Vector<MockSourceBufferPrivate*> m_activeSourceBuffers;
    bool m_isEnded;

    unsigned long m_totalVideoFrames;
    unsigned long m_droppedVideoFrames;
    unsigned long m_corruptedVideoFrames;
    MediaTime m_totalFrameDelay;
};

}

#endif // ENABLE(MEDIA_SOURCE)

#endif // MockMediaSourcePrivate_h

