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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

namespace WebCore {

class MockMediaPlayerMediaSource;
class MockSourceBufferPrivate;
class TimeRanges;

class MockMediaSourcePrivate FINAL : public MediaSourcePrivate {
public:
    static RefPtr<MockMediaSourcePrivate> create(MockMediaPlayerMediaSource*);
    virtual ~MockMediaSourcePrivate();

    const Vector<MockSourceBufferPrivate*>& activeSourceBuffers() const { return m_activeSourceBuffers; }

    bool hasAudio() const;
    bool hasVideo() const;

    MockMediaPlayerMediaSource* player() const { return m_player; }

private:
    MockMediaSourcePrivate(MockMediaPlayerMediaSource*);

    // MediaSourcePrivate Overrides
    virtual AddStatus addSourceBuffer(const ContentType&, RefPtr<SourceBufferPrivate>&) OVERRIDE;
    virtual double duration() OVERRIDE;
    virtual void setDuration(double) OVERRIDE;
    virtual void markEndOfStream(EndOfStreamStatus) OVERRIDE;
    virtual void unmarkEndOfStream() OVERRIDE;
    virtual MediaPlayer::ReadyState readyState() const OVERRIDE;
    virtual void setReadyState(MediaPlayer::ReadyState) OVERRIDE;

    void sourceBufferPrivateDidChangeActiveState(MockSourceBufferPrivate*, bool active);
    void removeSourceBuffer(SourceBufferPrivate*);

    friend class MockSourceBufferPrivate;

    MockMediaPlayerMediaSource* m_player;
    double m_duration;
    Vector<RefPtr<MockSourceBufferPrivate>> m_sourceBuffers;
    Vector<MockSourceBufferPrivate*> m_activeSourceBuffers;
    bool m_isEnded;
};

}

#endif // ENABLE(MEDIA_SOURCE)

#endif // MockMediaSourcePrivate_h

