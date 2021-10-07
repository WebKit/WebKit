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

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#include "SourceBufferParser.h"
#include "SourceBufferPrivate.h"
#include <dispatch/group.h>
#include <wtf/Box.h>
#include <wtf/CancellableTask.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/WTFSemaphore.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>
#include <wtf/threads/BinarySemaphore.h>

OBJC_CLASS AVStreamDataParser;
OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS NSData;
OBJC_CLASS NSError;
OBJC_CLASS NSObject;
OBJC_CLASS WebAVSampleBufferErrorListener;

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class CDMInstance;
class CDMInstanceFairPlayStreamingAVFObjC;
class CDMSessionMediaSourceAVFObjC;
class MediaPlayerPrivateMediaSourceAVFObjC;
class MediaSourcePrivateAVFObjC;
class TimeRanges;
class AudioTrackPrivate;
class VideoTrackPrivate;
class AudioTrackPrivateMediaSourceAVFObjC;
class VideoTrackPrivateMediaSourceAVFObjC;
class WebCoreDecompressionSession;
class SharedBuffer;

class SourceBufferPrivateAVFObjCErrorClient {
public:
    virtual ~SourceBufferPrivateAVFObjCErrorClient() = default;
    virtual void layerDidReceiveError(AVSampleBufferDisplayLayer *, NSError *, bool& shouldIgnore) = 0;
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    virtual void rendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *, bool& shouldIgnore) = 0;
    ALLOW_NEW_API_WITHOUT_GUARDS_END
};

class SourceBufferPrivateAVFObjC final
    : public SourceBufferPrivate
    , public CanMakeWeakPtr<SourceBufferPrivateAVFObjC>
{
public:
    static Ref<SourceBufferPrivateAVFObjC> create(MediaSourcePrivateAVFObjC*, Ref<SourceBufferParser>&&);
    virtual ~SourceBufferPrivateAVFObjC();

    void clearMediaSource() { m_mediaSource = nullptr; }

    void willProvideContentKeyRequestInitializationDataForTrackID(uint64_t trackID);
    void didProvideContentKeyRequestInitializationDataForTrackID(Ref<Uint8Array>&&, uint64_t trackID, Box<BinarySemaphore>);

    bool hasSelectedVideo() const;

    void trackDidChangeSelected(VideoTrackPrivate&, bool selected);
    void trackDidChangeEnabled(AudioTrackPrivate&, bool enabled);

    void willSeek();
    FloatSize naturalSize();

    uint64_t protectedTrackID() const { return m_protectedTrackID; }
    AVStreamDataParser* streamDataParser() const;
    void setCDMSession(CDMSessionMediaSourceAVFObjC*);
    void setCDMInstance(CDMInstance*);
    void attemptToDecrypt();
    bool waitingForKey() const { return m_waitingForKey; }

    void flush();
#if PLATFORM(IOS_FAMILY)
    void flushIfNeeded();
#endif

    void registerForErrorNotifications(SourceBufferPrivateAVFObjCErrorClient*);
    void unregisterForErrorNotifications(SourceBufferPrivateAVFObjCErrorClient*);
    void layerDidReceiveError(AVSampleBufferDisplayLayer *, NSError *);
    void rendererWasAutomaticallyFlushed(AVSampleBufferAudioRenderer *, const CMTime&);
    void outputObscuredDueToInsufficientExternalProtectionChanged(bool);
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    void rendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *);
    ALLOW_NEW_API_WITHOUT_GUARDS_END

    void setVideoLayer(AVSampleBufferDisplayLayer*);
    void setDecompressionSession(WebCoreDecompressionSession*);

    void bufferWasConsumed();
    
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    Uint8Array* initData() { return m_initData.get(); }
#endif

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const override { return "SourceBufferPrivateAVFObjC"; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const Logger& sourceBufferLogger() const final { return m_logger.get(); }
    const void* sourceBufferLogIdentifier() final { return logIdentifier(); }
#endif

private:
    explicit SourceBufferPrivateAVFObjC(MediaSourcePrivateAVFObjC*, Ref<SourceBufferParser>&&);

    using InitializationSegment = SourceBufferPrivateClient::InitializationSegment;
    void didParseInitializationData(InitializationSegment&&);
    void didEncounterErrorDuringParsing(int32_t);
    void didProvideMediaDataForTrackId(Ref<MediaSample>&&, uint64_t trackId, const String& mediaType);

    // SourceBufferPrivate overrides
    void append(Ref<SharedBuffer>&&) final;
    void abort() final;
    void resetParserState() final;
    void removedFromMediaSource() final;
    MediaPlayer::ReadyState readyState() const final;
    void setReadyState(MediaPlayer::ReadyState) final;
    void flush(const AtomString& trackID) final;
    void enqueueSample(Ref<MediaSample>&&, const AtomString& trackID) final;
    bool isReadyForMoreSamples(const AtomString& trackID) final;
    MediaTime timeFudgeFactor() const final;
    void setActive(bool) final;
    bool isActive() const final;
    void notifyClientWhenReadyForMoreSamples(const AtomString& trackID) final;
    bool canSetMinimumUpcomingPresentationTime(const AtomString&) const override;
    void setMinimumUpcomingPresentationTime(const AtomString&, const MediaTime&) override;
    void clearMinimumUpcomingPresentationTime(const AtomString&) override;
    bool canSwitchToType(const ContentType&) final;
    bool isSeeking() const final;
    MediaTime currentMediaTime() const final;
    MediaTime duration() const final;

    void didBecomeReadyForMoreSamples(uint64_t trackID);
    void appendCompleted();
    void destroyStreamDataParser();
    void destroyRenderers();
    void clearTracks();

    void flushVideo();
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    void flushAudio(AVSampleBufferAudioRenderer *);
    ALLOW_NEW_API_WITHOUT_GUARDS_END

    MediaPlayerPrivateMediaSourceAVFObjC* player() const;

    Vector<RefPtr<VideoTrackPrivate>> m_videoTracks;
    Vector<RefPtr<AudioTrackPrivate>> m_audioTracks;
    Vector<SourceBufferPrivateAVFObjCErrorClient*> m_errorClients;

    WeakPtrFactory<SourceBufferPrivateAVFObjC> m_appendWeakFactory;

    Ref<SourceBufferParser> m_parser;
    bool m_processingInitializationSegment { false };
    bool m_hasPendingAppendCompletedCallback { false };
    Vector<Function<void()>> m_pendingTrackChangeCallbacks;
    Vector<std::pair<uint64_t, Ref<MediaSample>>> m_mediaSamples;

    RetainPtr<AVSampleBufferDisplayLayer> m_displayLayer;
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    HashMap<uint64_t, RetainPtr<AVSampleBufferAudioRenderer>, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_audioRenderers;
    ALLOW_NEW_API_WITHOUT_GUARDS_END
    RetainPtr<WebAVSampleBufferErrorListener> m_errorListener;
#if PLATFORM(IOS_FAMILY)
    bool m_displayLayerWasInterrupted { false };
#endif
    RetainPtr<NSError> m_hdcpError;
    Box<BinarySemaphore> m_hasSessionSemaphore;
    Box<Semaphore> m_abortSemaphore;
    const Ref<WTF::WorkQueue> m_appendQueue;
    RefPtr<WebCoreDecompressionSession> m_decompressionSession;

    MediaSourcePrivateAVFObjC* m_mediaSource;
    bool m_isActive { false };
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<Uint8Array> m_initData;
    WeakPtr<CDMSessionMediaSourceAVFObjC> m_session { nullptr };
#endif
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    RefPtr<CDMInstanceFairPlayStreamingAVFObjC> m_cdmInstance;
    Vector<Ref<SharedBuffer>> m_keyIDs;
#endif

    std::optional<FloatSize> m_cachedSize;
    FloatSize m_currentSize;
    bool m_parsingSucceeded { true };
    bool m_waitingForKey { true };
    uint64_t m_enabledVideoTrackID { notFound };
    uint64_t m_protectedTrackID { notFound };
    uint64_t m_mapID;
    uint32_t m_abortCalled { 0 };

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
