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

#ifndef MediaSourcePrivateAVFObjC_h
#define MediaSourcePrivateAVFObjC_h

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#include "MediaSourcePrivate.h"
#include <wtf/Deque.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS AVAsset;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVStreamDataParser;
OBJC_CLASS NSError;
OBJC_CLASS NSObject;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class CDMInstance;
class LegacyCDMSession;
class MediaPlayerPrivateMediaSourceAVFObjC;
class MediaSourcePrivateClient;
class SourceBufferPrivateAVFObjC;
class TimeRanges;
class WebCoreDecompressionSession;

class MediaSourcePrivateAVFObjC final : public MediaSourcePrivate {
public:
    static RefPtr<MediaSourcePrivateAVFObjC> create(MediaPlayerPrivateMediaSourceAVFObjC*, MediaSourcePrivateClient*);
    virtual ~MediaSourcePrivateAVFObjC();

    MediaPlayerPrivateMediaSourceAVFObjC* player() const { return m_player; }
    const Vector<RefPtr<SourceBufferPrivateAVFObjC>>& sourceBuffers() const { return m_sourceBuffers; }
    const Vector<SourceBufferPrivateAVFObjC*>& activeSourceBuffers() const { return m_activeSourceBuffers; }

    AddStatus addSourceBuffer(const ContentType&, RefPtr<SourceBufferPrivate>&) override;
    void durationChanged() override;
    void markEndOfStream(EndOfStreamStatus) override;
    void unmarkEndOfStream() override;
    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState) override;
    void waitForSeekCompleted() override;
    void seekCompleted() override;

    MediaTime duration();
    std::unique_ptr<PlatformTimeRanges> buffered();

    bool hasAudio() const;
    bool hasVideo() const;
    bool hasSelectedVideo() const;

    void willSeek();
    void seekToTime(const MediaTime&);
    MediaTime fastSeekTimeForMediaTime(const MediaTime&, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold);
    FloatSize naturalSize() const;

    void hasSelectedVideoChanged(SourceBufferPrivateAVFObjC&);
    void setVideoLayer(AVSampleBufferDisplayLayer*);
    void setDecompressionSession(WebCoreDecompressionSession*);

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&);
    void cdmInstanceDetached(CDMInstance&);
    void attemptToDecryptWithInstance(CDMInstance&);
    bool waitingForKey() const;
    
    CDMInstance* cdmInstance() const { return m_cdmInstance.get(); }
    void outputObscuredDueToInsufficientExternalProtectionChanged(bool);
#endif

private:
    MediaSourcePrivateAVFObjC(MediaPlayerPrivateMediaSourceAVFObjC*, MediaSourcePrivateClient*);

    void sourceBufferPrivateDidChangeActiveState(SourceBufferPrivateAVFObjC*, bool active);
    void sourceBufferPrivateDidReceiveInitializationSegment(SourceBufferPrivateAVFObjC*);
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void sourceBufferKeyNeeded(SourceBufferPrivateAVFObjC*, Uint8Array*);
#endif
    void monitorSourceBuffers();
    void removeSourceBuffer(SourceBufferPrivate*);

    void setSourceBufferWithSelectedVideo(SourceBufferPrivateAVFObjC*);

    friend class SourceBufferPrivateAVFObjC;

    MediaPlayerPrivateMediaSourceAVFObjC* m_player;
    RefPtr<MediaSourcePrivateClient> m_client;
    Vector<RefPtr<SourceBufferPrivateAVFObjC>> m_sourceBuffers;
    Vector<SourceBufferPrivateAVFObjC*> m_activeSourceBuffers;
    Deque<SourceBufferPrivateAVFObjC*> m_sourceBuffersNeedingSessions;
    SourceBufferPrivateAVFObjC* m_sourceBufferWithSelectedVideo { nullptr };
    bool m_isEnded;
#if ENABLE(ENCRYPTED_MEDIA)
    RefPtr<CDMInstance> m_cdmInstance;
#endif
};

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#endif
