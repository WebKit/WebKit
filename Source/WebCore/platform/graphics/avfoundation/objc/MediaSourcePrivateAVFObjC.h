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

#ifndef MediaSourcePrivateAVFObjC_h
#define MediaSourcePrivateAVFObjC_h

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#include "MediaSourcePrivate.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS AVAsset;
OBJC_CLASS AVStreamDataParser;
OBJC_CLASS NSError;
OBJC_CLASS NSObject;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class MediaPlayerPrivateMediaSourceAVFObjC;
class SourceBufferPrivateAVFObjC;
class TimeRanges;

class MediaSourcePrivateAVFObjC FINAL : public MediaSourcePrivate {
public:
    static RefPtr<MediaSourcePrivateAVFObjC> create(MediaPlayerPrivateMediaSourceAVFObjC*);
    virtual ~MediaSourcePrivateAVFObjC();

    MediaPlayerPrivateMediaSourceAVFObjC* player() const { return m_player; }
    const Vector<SourceBufferPrivateAVFObjC*>& activeSourceBuffers() const { return m_activeSourceBuffers; }

    virtual AddStatus addSourceBuffer(const ContentType&, RefPtr<SourceBufferPrivate>&) override;
    virtual double duration() override;
    virtual void setDuration(double) override;
    virtual void markEndOfStream(EndOfStreamStatus) override;
    virtual void unmarkEndOfStream() override;
    virtual MediaPlayer::ReadyState readyState() const override;
    virtual void setReadyState(MediaPlayer::ReadyState) override;

    bool hasAudio() const;
    bool hasVideo() const;

    MediaTime seekToTime(MediaTime, MediaTime negativeThreshold, MediaTime positiveThreshold);
    IntSize naturalSize() const;

private:
    MediaSourcePrivateAVFObjC(MediaPlayerPrivateMediaSourceAVFObjC*);

    void sourceBufferPrivateDidChangeActiveState(SourceBufferPrivateAVFObjC*, bool active);
    void sourceBufferPrivateDidReceiveInitializationSegment(SourceBufferPrivateAVFObjC*);
    void monitorSourceBuffers();
    void removeSourceBuffer(SourceBufferPrivate*);

    friend class SourceBufferPrivateAVFObjC;

    MediaPlayerPrivateMediaSourceAVFObjC* m_player;
    double m_duration;
    Vector<RefPtr<SourceBufferPrivateAVFObjC>> m_sourceBuffers;
    Vector<SourceBufferPrivateAVFObjC*> m_activeSourceBuffers;
    bool m_isEnded;
};

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#endif
