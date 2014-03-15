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

#ifndef MockSourceBufferPrivate_h
#define MockSourceBufferPrivate_h

#if ENABLE(MEDIA_SOURCE)

#include "SourceBufferPrivate.h"
#include <wtf/HashMap.h>
#include <wtf/MediaTime.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class AudioTrackPrivate;
class InbandTextTrackPrivate;
class MockInitializationBox;
class MockMediaSourcePrivate;
class MockSampleBox;
class TimeRanges;
class VideoTrackPrivate;

class MockSourceBufferPrivate : public SourceBufferPrivate {
public:
    static RefPtr<MockSourceBufferPrivate> create(MockMediaSourcePrivate*);
    virtual ~MockSourceBufferPrivate();

    void clearMediaSource() { m_mediaSource = nullptr; }

    bool hasVideo() const;
    bool hasAudio() const;

    void seekToTime(const MediaTime&);
    MediaTime fastSeekTimeForMediaTime(const MediaTime&, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold);

private:
    explicit MockSourceBufferPrivate(MockMediaSourcePrivate*);

    // SourceBufferPrivate overrides
    virtual void setClient(SourceBufferPrivateClient*) override;
    virtual AppendResult append(const unsigned char* data, unsigned length) override;
    virtual void abort() override;
    virtual void removedFromMediaSource() override;
    virtual MediaPlayer::ReadyState readyState() const override;
    virtual void setReadyState(MediaPlayer::ReadyState) override;
    virtual void evictCodedFrames() override;
    virtual bool isFull() override;

    virtual void flushAndEnqueueNonDisplayingSamples(Vector<RefPtr<MediaSample>>, AtomicString) override { }
    virtual void enqueueSample(PassRefPtr<MediaSample>, AtomicString) override;
    virtual bool isReadyForMoreSamples(AtomicString) override { return true; }
    virtual void setActive(bool) override;

    void didReceiveInitializationSegment(const MockInitializationBox&);
    void didReceiveSample(const MockSampleBox&);

    MockMediaSourcePrivate* m_mediaSource;
    SourceBufferPrivateClient* m_client;

    Vector<char> m_inputBuffer;
};

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#endif

