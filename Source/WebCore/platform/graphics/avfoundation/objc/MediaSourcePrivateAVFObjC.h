/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#include "MediaSourcePrivate.h"
#include "MediaSourcePrivateClient.h"
#include <wtf/Deque.h>
#include <wtf/LoggerHelper.h>
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

class MediaSourcePrivateAVFObjC final
    : public MediaSourcePrivate
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<MediaSourcePrivateAVFObjC> create(MediaPlayerPrivateMediaSourceAVFObjC&, MediaSourcePrivateClient&);
    virtual ~MediaSourcePrivateAVFObjC();

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::AVFObjC; }

    MediaPlayerPrivateMediaSourceAVFObjC* player() const { return m_player.get(); }

    AddStatus addSourceBuffer(const ContentType&, bool webMParserEnabled, RefPtr<SourceBufferPrivate>&) final;
    void durationChanged(const MediaTime&) final;
    void markEndOfStream(EndOfStreamStatus) final;

    MediaPlayer::ReadyState mediaPlayerReadyState() const final;
    void setMediaPlayerReadyState(MediaPlayer::ReadyState) final;

    bool hasSelectedVideo() const;

    MediaTime currentMediaTime() const final;
    void willSeek();

    FloatSize naturalSize() const;

    void hasSelectedVideoChanged(SourceBufferPrivateAVFObjC&);
    void setVideoLayer(AVSampleBufferDisplayLayer*);
    void setDecompressionSession(WebCoreDecompressionSession*);

    void flushActiveSourceBuffersIfNeeded();

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&);
    void cdmInstanceDetached(CDMInstance&);
    void attemptToDecryptWithInstance(CDMInstance&);
    bool waitingForKey() const;

    CDMInstance* cdmInstance() const { return m_cdmInstance.get(); }
    void outputObscuredDueToInsufficientExternalProtectionChanged(bool);
#endif

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const final { return "MediaSourcePrivateAVFObjC"; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    const void* nextSourceBufferLogIdentifier() { return childLogIdentifier(m_logIdentifier, ++m_nextSourceBufferID); }
#endif

    using RendererType = MediaSourcePrivateClient::RendererType;
    void failedToCreateRenderer(RendererType);
    bool needsVideoLayer() const;

private:
    MediaSourcePrivateAVFObjC(MediaPlayerPrivateMediaSourceAVFObjC&, MediaSourcePrivateClient&);

    void notifyActiveSourceBuffersChanged() final;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void sourceBufferKeyNeeded(SourceBufferPrivateAVFObjC*, const SharedBuffer&);
#endif
    void removeSourceBuffer(SourceBufferPrivate&) final;

    void setSourceBufferWithSelectedVideo(SourceBufferPrivateAVFObjC*);

    friend class SourceBufferPrivateAVFObjC;

    WeakPtr<MediaPlayerPrivateMediaSourceAVFObjC> m_player;
    Deque<SourceBufferPrivateAVFObjC*> m_sourceBuffersNeedingSessions;
    SourceBufferPrivateAVFObjC* m_sourceBufferWithSelectedVideo { nullptr };
#if ENABLE(ENCRYPTED_MEDIA)
    RefPtr<CDMInstance> m_cdmInstance;
#endif
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
    uint64_t m_nextSourceBufferID { 0 };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaSourcePrivateAVFObjC)
static bool isType(const WebCore::MediaSourcePrivate& mediaSource) { return mediaSource.platformType() == WebCore::MediaPlatformType::AVFObjC; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
