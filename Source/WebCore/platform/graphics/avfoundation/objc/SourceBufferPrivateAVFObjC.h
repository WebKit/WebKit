/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#include "AudioVideoRenderer.h"
#include "MediaSourceConfiguration.h"
#include "ProcessIdentity.h"
#include "SourceBufferParser.h"
#include "SourceBufferPrivate.h"
#include <dispatch/group.h>
#include <wtf/Box.h>
#include <wtf/Deque.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/Observer.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS AVStreamDataParser;
OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS NSData;
OBJC_CLASS NSError;
OBJC_CLASS NSObject;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class AudioVideoRenderer;
class CDMInstance;
class CDMInstanceFairPlayStreamingAVFObjC;
class CDMSessionAVContentKeySession;
class InbandTextTrackPrivate;
class MediaPlayerPrivateMediaSourceAVFObjC;
class MediaSourcePrivateAVFObjC;
class TimeRanges;
class AudioTrackPrivate;
class VideoMediaSampleRenderer;
class VideoTrackPrivate;
class AudioTrackPrivateMediaSourceAVFObjC;
class VideoTrackPrivateMediaSourceAVFObjC;
class SharedBuffer;

struct TrackInfo;

class SourceBufferPrivateAVFObjC final
    : public SourceBufferPrivate
{
public:
    static Ref<SourceBufferPrivateAVFObjC> create(MediaSourcePrivateAVFObjC&, const MediaSourceConfiguration&, Ref<SourceBufferParser>&&, Ref<AudioVideoRenderer>&&);
    virtual ~SourceBufferPrivateAVFObjC();

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::AVFObjC; }

    void didProvideContentKeyRequestInitializationDataForTrackID(Ref<SharedBuffer>&&, TrackID);

    void didProvideContentKeyRequestIdentifierForTrackID(Ref<SharedBuffer>&&, TrackID);

    bool hasSelectedVideo() const;

    void videoTrackDidChangeSelected(TrackID, bool selected);
    void audioTrackDidChangeEnabled(TrackID, bool enabled);

    FloatSize naturalSize();
    void flushAndReenqueueVideo();

    bool needsVideoLayer() const;

#if (ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)) || ENABLE(LEGACY_ENCRYPTED_MEDIA)
    bool waitingForKey() const final { return m_waitingForKey; }
#endif

    // Used by CDMSessionAVContentKeySession
    void flush();

    using TrackIdentifier = TracksRendererManager::TrackIdentifier;
    std::optional<TrackIdentifier> trackIdentifierFor(TrackID) const;

    void setVideoRenderer(bool);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const override { return "SourceBufferPrivateAVFObjC"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const Logger& sourceBufferLogger() const final { return m_logger.get(); }
    uint64_t sourceBufferLogIdentifier() final { return logIdentifier(); }
#endif

    void setResourceOwner(const ProcessIdentity& resourceOwner) { m_resourceOwner = resourceOwner; }

    // Used by detachable MediaSource.
    void setAudioVideoRenderer(AudioVideoRenderer&);

private:
    SourceBufferPrivateAVFObjC(MediaSourcePrivateAVFObjC&, const MediaSourceConfiguration&, Ref<SourceBufferParser>&&, Ref<AudioVideoRenderer>&&);

    void didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&&, TrackID, const String& mediaType);
    bool isMediaSampleAllowed(const MediaSample&) const final;

    // SourceBufferPrivate overrides
    Ref<MediaPromise> appendInternal(Ref<SharedBuffer>&&) final;
    void resetParserStateInternal() final;
    void removedFromMediaSource() final;
    void flush(TrackID) final;
    void enqueueSample(Ref<MediaSample>&&, TrackID) final;
    bool isReadyForMoreSamples(TrackID) final;
    MediaTime timeFudgeFactor() const final;
    void notifyClientWhenReadyForMoreSamples(TrackID) final;
    bool canSetMinimumUpcomingPresentationTime(TrackID) const override;
    void setMinimumUpcomingPresentationTime(TrackID, const MediaTime&) override;
    bool canSwitchToType(const ContentType&) final;

    void configureParser(SourceBufferParser&);
    bool precheckInitializationSegment(const InitializationSegment&) final;
    void processInitializationSegment(std::optional<InitializationSegment>&&) final;
    void processFormatDescriptionForTrackId(Ref<TrackInfo>&&, TrackID) final;
    void updateTrackIds(Vector<std::pair<TrackID, TrackID>>&&) final;

    void enqueueSample(Ref<MediaSampleAVFObjC>&&, TrackID);
    void attachContentKeyToSampleIfNeeded(const MediaSampleAVFObjC&);
    void didBecomeReadyForMoreSamples(TrackID);
    void appendCompleted(bool);
    void destroyRendererTracks();

    bool isEnabledVideoTrackID(TrackID) const;
    bool isTextTrack(TrackID) const;

    bool hasTrackIdentifierFor(TrackID) const;
    void removeTrackID(TrackID);

    RefPtr<MediaPlayerPrivateMediaSourceAVFObjC> player() const;
    bool canEnqueueSample(TrackID, const MediaSampleAVFObjC&);
    bool trackIsBlocked(TrackID) const;

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    void tryToEnqueueBlockedSamples();
#endif

    void setTrackChangeCallbacks(const InitializationSegment&, bool initialized);

    void maybeUpdateNeedsVideoLayer();

    RefPtr<AudioVideoRenderer> protectedRenderer() const;
    void ensureWeakOnDispatcher(Function<void(SourceBufferPrivateAVFObjC&)>&&);
    void callOnMainThreadWithPlayer(Function<void(MediaPlayerPrivateMediaSourceAVFObjC&)>&&);

    StdUnorderedMap<TrackID, RefPtr<VideoTrackPrivate>> m_videoTracks WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    StdUnorderedMap<TrackID, RefPtr<AudioTrackPrivate>> m_audioTracks WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    StdUnorderedMap<TrackID, RefPtr<InbandTextTrackPrivate>> m_textTracks WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    StdUnorderedMap<TrackID, TrackIdentifier> m_trackIdentifiers WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());

    // Detachable MediaSource state records.
    void detach() final;
    StdUnorderedMap<TrackID, bool> m_trackSelectedValues WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    bool m_isDetached WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { false };

    Ref<SourceBufferParser> m_parser WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    Vector<Function<void()>> m_pendingTrackChangeTasks WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    const Ref<WTF::WorkQueue> m_appendQueue;
    const MediaSourceConfiguration m_configuration;

    std::optional<FloatSize> m_cachedSize WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    FloatSize m_currentSize WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    std::atomic<bool> m_waitingForKey { true };
    std::optional<TrackID> m_enabledVideoTrackID WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    std::optional<TrackID> m_protectedTrackID WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    RefPtr<AudioVideoRenderer> m_renderer WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()); // Never null except when detached.
    bool m_isSelectedForVideo  WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { false };
    std::atomic<bool> m_needsVideoLayer { false };

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
    ProcessIdentity m_resourceOwner;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceBufferPrivateAVFObjC)
static bool isType(const WebCore::SourceBufferPrivate& sourceBuffer) { return sourceBuffer.platformType() == WebCore::MediaPlatformType::AVFObjC; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
