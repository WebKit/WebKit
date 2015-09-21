/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef MediaStreamPrivateAVFObjC_h
#define MediaStreamPrivateAVFObjC_h

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include "MediaPlayer.h"
#include "MediaStreamPrivate.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/MediaTime.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS AVAsset;
OBJC_CLASS AVStreamDataParser;
OBJC_CLASS NSError;
OBJC_CLASS NSObject;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class CDMSession;
class MediaPlayerPrivateMediaStreamAVFObjC;
class MediaStreamPrivate;
class MediaStreamPrivateClient;
class PlatformTimeRanges;
class TimeRanges;

class MediaStreamPrivateAVFObjC final : public MediaStreamPrivate {
public:
    static RefPtr<MediaStreamPrivateAVFObjC> create(MediaPlayerPrivateMediaStreamAVFObjC&, MediaStreamPrivate&);
    virtual ~MediaStreamPrivateAVFObjC();

    MediaPlayerPrivateMediaStreamAVFObjC* player() const { return m_player; }

    virtual MediaStreamPrivateClient* client() const { return m_client.get(); }
    virtual void setClient(MediaStreamPrivateClient* client) { m_client = client; }

    MediaTime duration();
    std::unique_ptr<PlatformTimeRanges> buffered();

    bool hasAudio() const;
    bool hasVideo() const;

    void setReadyState(MediaPlayer::ReadyState);
    FloatSize naturalSize() const;

    MediaPlayer::ReadyState readyState() const;

private:
    MediaStreamPrivateAVFObjC(MediaPlayerPrivateMediaStreamAVFObjC&, MediaStreamPrivate&);

    MediaPlayerPrivateMediaStreamAVFObjC* m_player;
    RefPtr<MediaStreamPrivateClient> m_client;
};
    
}

#endif // ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#endif
