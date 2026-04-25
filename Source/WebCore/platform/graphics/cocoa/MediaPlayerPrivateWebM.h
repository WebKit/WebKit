/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Platform.h>
#if ENABLE(COCOA_WEBM_PLAYER)

#include <WebCore/AudioVideoRenderer.h>
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/TimeRanges.h>
#include <WebCore/VideoFrameMetadata.h>
#include "SourceBufferParserWebM.h"
#include "WebMResourceClient.h"
#include <wtf/HashFunctions.h>
#include <wtf/LoggerHelper.h>
#include <wtf/NativePromise.h>
#include <wtf/StdUnorderedMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

OBJC_PROTOCOL(WebSampleBufferVideoRendering);

typedef struct CF_BRIDGED_TYPE(id) __CVBuffer *CVPixelBufferRef;

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class AudioTrackPrivateWebM;
class FragmentedSharedBuffer;
class MediaDescription;
class MediaPlaybackTarget;
class MediaSample;
class MediaSampleAVFObjC;
class PixelBufferConformerCV;
class ResourceError;
class SharedBuffer;
class TextTrackRepresentation;
class TrackBuffer;
class VideoFrame;
class VideoFrameCV;
class VideoTrackPrivateWebM;

class MediaPlayerPrivateWebM
    : public MediaPlayerPrivateInterface
    , public WebMResourceClientParent
    , private LoggerHelper
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaPlayerPrivateWebM, WTF::DestructionThread::Main> {
    WTF_MAKE_TZONE_ALLOCATED(MediaPlayerPrivateWebM);
public:
    static Ref<MediaPlayerPrivateWebM> create(MediaPlayer&);
    ~MediaPlayerPrivateWebM();

    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::CocoaWebM; }

    static void registerMediaEngine(MediaEngineRegistrar);

    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

private:
    explicit MediaPlayerPrivateWebM(MediaPlayer&);

    void setPreload(MediaPlayer::Preload) final;
    void doPreload();
    void load(const URL&, const LoadOptions&) final;
    bool NODELETE needsResourceClient() const;
    bool createResourceClientIfNeeded();

#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) final;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final;
#endif
    
    // WebMResourceClientParent
    friend class WebMResourceClient;
    void dataLengthReceived(size_t) final;
    void dataReceived(const SharedBuffer&) final;
    void loadFailed(const ResourceError&) final;
    void loadFinished() final;

    void cancelLoad() final;

    PlatformLayer* platformLayer() const final;

    bool supportsPictureInPicture() const final { return true; }
    bool supportsFullscreen() const final { return true; }

    void prepareToPlay() final;
    void play() final;
    void pause() final;
    bool paused() const final;
    bool timeIsProgressing() const final;
    void playInternal(std::optional<MonotonicTime> = std::nullopt);

    bool supportsPlayAtHostTime() const final { return true; }
    bool supportsPauseAtHostTime() const final { return true; }
    bool playAtHostTime(const MonotonicTime&) final;
    bool pauseAtHostTime(const MonotonicTime&) final;

    FloatSize naturalSize() const final;

    bool performTaskAtTime(Function<void(const MediaTime&)>&&, const MediaTime&) final;
    void audioOutputDeviceChanged() final;

    bool hasVideo() const final { return m_hasVideo.load(std::memory_order_relaxed); }
    bool hasAudio() const final { return m_hasAudio.load(std::memory_order_relaxed); }

    void setPageIsVisible(bool) final;

    MediaTime timeFudgeFactor() const { return { 1, 10 }; }
    MediaTime currentTime() const final;
    MediaTime duration() const final;
    MediaTime startTime() const final { return MediaTime::zeroTime(); }
    MediaTime initialTime() const final { return MediaTime::zeroTime(); }

    void setRateDouble(double) final;
    double rate() const final;
    double effectiveRate() const final;

    void setVolume(float) final;
    void setMuted(bool) final;

    MediaPlayer::NetworkState networkState() const final { return m_networkState; }
    MediaPlayer::ReadyState readyState() const final { return m_readyState; }

    MediaTime maxTimeSeekable() const final { return duration(); }
    MediaTime minTimeSeekable() const final { return startTime(); }
    const PlatformTimeRanges& buffered() const final;

    void setBufferedRanges(PlatformTimeRanges);
    void updateBufferedFromTrackBuffers(bool);
    void updateDurationFromTrackBuffers();

    void setLoadingProgresssed(bool);
    bool didLoadingProgress() const final;

    RefPtr<NativeImage> nativeImageForCurrentTime() final;
    bool updateLastVideoFrame();
    bool updateLastImage();
    void paint(GraphicsContext&, const FloatRect&) final;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) final;
    RefPtr<VideoFrame> videoFrameForCurrentTime() final;
    DestinationColorSpace colorSpace() final;
    Ref<BitmapImagePromise> bitmapImageForCurrentTime() final;

    void setNaturalSize(FloatSize);
    void effectiveRateChanged();
    void setHasAudio(bool);
    void setHasVideo(bool);
    void setHasAvailableVideoFrame(bool);
    bool hasAvailableVideoFrame() const final;
    void setDuration(MediaTime);
    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);
    void characteristicsChanged();
    void errorOccurred();

    void setPreservesPitch(bool) final;
    void setPresentationSize(const IntSize&) final;
    bool supportsAcceleratedRendering() const final { return true; }
    void acceleratedRenderingStateChanged() final;

    RetainPtr<PlatformLayer> createVideoFullscreenLayer() final;
    void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&& completionHandler) final;
    void setVideoFullscreenFrame(const FloatRect&) final;

    void setTextTrackRepresentation(TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;
        
    String engineDescription() const final;
    MediaPlayer::MovieLoadType movieLoadType() const final { return MediaPlayer::MovieLoadType::Download; }
        
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool isCurrentPlaybackTargetWireless() const final;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) final;
    void setShouldPlayToPlaybackTarget(bool) final;
    bool wirelessVideoPlaybackDisabled() const final { return false; }
#endif

    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;

    void enqueueSample(Ref<MediaSample>&&, TrackID);
    enum class NeedsFlush: bool {
        No = 0,
        Yes
    };
    void reenqueSamples(TrackID, NeedsFlush = NeedsFlush::Yes);
    void reenqueueMediaForTime(const MediaTime&);
    void reenqueueMediaForTime(TrackBuffer&, TrackID, const MediaTime&, NeedsFlush = NeedsFlush::Yes);
    void notifyClientWhenReadyForMoreSamples(TrackID);

    bool isReadyForMoreSamples(TrackID);
    void didBecomeReadyForMoreSamples(TrackID);
    void appendCompleted(bool);
    void provideMediaData(TrackID);
    void provideMediaData(TrackBuffer&, TrackID);

    void trackDidChangeSelected(VideoTrackPrivate&, bool);
    void trackDidChangeEnabled(AudioTrackPrivate&, bool);

    using InitializationSegment = SourceBufferParserWebM::InitializationSegment;
    void didParseInitializationData(InitializationSegment&&);
    void didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&&, TrackID, const String& mediaType);
    void didUpdateFormatDescriptionForTrackId(Ref<TrackInfo>&&, TrackID);

    void flush();
    void flushTrack(TrackID);
    void flushVideoIfNeeded();

    void addTrackBuffer(TrackID, RefPtr<MediaDescription>&&);

    void clearTracks(); // Called from destructor (main thread) or running queue

    void startVideoFrameMetadataGathering() final;
    void stopVideoFrameMetadataGathering() final;
    std::optional<VideoFrameMetadata> videoFrameMetadata() final;
    void setResourceOwner(const ProcessIdentity&) final;

    void checkNewVideoFrameMetadata(const MediaTime& presentationTime, double displayTime);

    void setShouldDisableHDR(bool) final;
    void setPlatformDynamicRangeLimit(PlatformDynamicRangeLimit) final;
    void playerContentBoxRectChanged(const LayoutRect&) final;
    void setShouldMaintainAspectRatio(bool) final;
    bool m_shouldMaintainAspectRatio WTF_GUARDED_BY_CAPABILITY(mainThread) { true };

#if HAVE(SPATIAL_TRACKING_LABEL)
    String defaultSpatialTrackingLabel() const final;
    void setDefaultSpatialTrackingLabel(const String&) final;
    String spatialTrackingLabel() const final;
    void setSpatialTrackingLabel(const String&) final;
    void updateSpatialTrackingLabel();
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
    void setVideoTarget(const PlatformVideoTarget&) final;
#endif

#if PLATFORM(IOS_FAMILY)
    void sceneIdentifierDidChange() final;
    void applicationWillResignActive() final;
    void applicationDidBecomeActive() final;
#endif

    void isInFullscreenOrPictureInPictureChanged(bool) final;

#if ENABLE(LINEAR_MEDIA_PLAYER)
    bool supportsLinearMediaPlayer() const final { return true; }
#endif

    using TrackIdentifier = TracksRendererManager::TrackIdentifier;
    TrackIdentifier NODELETE trackIdentifierFor(TrackID) const;
    std::optional<TrackIdentifier> NODELETE maybeTrackIdentifierFor(TrackID) const;

    void setLayerRequiresFlush();
    void NODELETE setAllTracksForReenqueuing();
    void NODELETE setTrackForReenqueuing(TrackID);

    // Remote layer support
    WebCore::HostingContext hostingContext() const final;
    void setVideoLayerSizeFenced(const WebCore::FloatSize&, WTF::MachSendRightAnnotated&&) final;
    std::optional<MediaPlayerIdentifier> identifier() const final { return m_playerIdentifier; }

    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const final { return "MediaPlayerPrivateWebM"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    friend class MediaPlayerFactoryWebM;
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    void maybeFinishLoading();
    void readyToProcessData();

    void monitorReadyState();

    static Ref<AudioVideoRenderer> createRenderer(LoggerHelper&, HTMLMediaElementIdentifier, MediaPlayerIdentifier);

    URL m_assetURL WTF_GUARDED_BY_CAPABILITY(mainThread);
    std::atomic<MediaPlayer::Preload> m_preload { MediaPlayer::Preload::Auto };
    ThreadSafeWeakPtr<MediaPlayer> m_player;
    RefPtr<VideoFrame> m_lastVideoFrame WTF_GUARDED_BY_CAPABILITY(mainThread);
    RefPtr<NativeImage> m_lastImage WTF_GUARDED_BY_CAPABILITY(mainThread);
    RefPtr<WebMResourceClient> m_resourceClient WTF_GUARDED_BY_CAPABILITY(mainThread);
    bool m_needsResourceClient WTF_GUARDED_BY_CAPABILITY(mainThread) { true };

    Vector<RefPtr<VideoTrackPrivateWebM>> m_videoTracks WTF_GUARDED_BY_CAPABILITY(runningQueue()); // or in destructor
    Vector<RefPtr<AudioTrackPrivateWebM>> m_audioTracks WTF_GUARDED_BY_CAPABILITY(runningQueue()); // or in destructor
    StdUnorderedMap<TrackID, TrackIdentifier> m_trackIdentifiers WTF_GUARDED_BY_CAPABILITY(runningQueue());
    StdUnorderedMap<TrackID, UniqueRef<TrackBuffer>> m_trackBufferMap WTF_GUARDED_BY_CAPABILITY(runningQueue());
    StdUnorderedMap<TrackID, bool> m_readyForMoreSamplesMap WTF_GUARDED_BY_CAPABILITY(runningQueue());
    StdUnorderedMap<TrackID, bool> m_requestReadyForMoreSamplesSetMap WTF_GUARDED_BY_CAPABILITY(runningQueue());
    PlatformTimeRanges m_buffered WTF_GUARDED_BY_CAPABILITY(runningQueue());
    PlatformTimeRanges m_bufferedMainThread WTF_GUARDED_BY_CAPABILITY(mainThread);

    const Ref<SourceBufferParserWebM> m_parser;
    const Ref<WTF::WorkQueue> m_appendQueue;

    std::atomic<MediaPlayer::NetworkState> m_networkState { MediaPlayer::NetworkState::Empty };
    std::atomic<MediaPlayer::ReadyState> m_readyState { MediaPlayer::ReadyState::HaveNothing };

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    RefPtr<MediaPlaybackTarget> m_playbackTarget WTF_GUARDED_BY_CAPABILITY(mainThread);
    bool m_shouldPlayToTarget WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
#endif
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;

    bool m_isGatheringVideoFrameMetadata WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    std::optional<VideoFrameMetadata> m_videoFrameMetadata WTF_GUARDED_BY_CAPABILITY(mainThread);
    uint64_t m_lastConvertedSampleCount WTF_GUARDED_BY_CAPABILITY(mainThread) { 0 };

    FloatSize m_naturalSize WTF_GUARDED_BY_CAPABILITY(mainThread);
    MediaTime m_duration WTF_GUARDED_BY_CAPABILITY(runningQueue()) { MediaTime::indefiniteTime() };
    MediaTime m_durationMainThread WTF_GUARDED_BY_CAPABILITY(mainThread) { MediaTime::indefiniteTime() };
    double m_rate WTF_GUARDED_BY_CAPABILITY(mainThread) { 1 };

    bool NODELETE isEnabledVideoTrackID(TrackID) const;
    bool NODELETE hasSelectedVideo() const;
    std::optional<TrackID> m_enabledVideoTrackID WTF_GUARDED_BY_CAPABILITY(runningQueue());
    std::atomic<uint32_t> m_abortCalled { 0 };
    size_t m_contentLength WTF_GUARDED_BY_CAPABILITY(mainThread) { 0 };
    size_t m_contentReceived WTF_GUARDED_BY_CAPABILITY(mainThread) { 0 };
    uint32_t m_pendingAppends WTF_GUARDED_BY_CAPABILITY(runningQueue()) { 0 };
    bool m_layerRequiresFlush WTF_GUARDED_BY_CAPABILITY(runningQueue()) { false };
#if PLATFORM(IOS_FAMILY)
    std::atomic<bool> m_applicationIsActive { true };
#endif
    std::atomic<bool> m_hasAudio { false };
    std::atomic<bool> m_hasVideo { false };
    bool m_hasAvailableVideoFrame WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    bool m_readyStateIsWaitingForAvailableFrame WTF_GUARDED_BY_CAPABILITY(mainThread) { true };
    bool m_visible WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    mutable std::atomic<bool> m_loadingProgressed { false };
    bool m_loadFinished WTF_GUARDED_BY_CAPABILITY(runningQueue()) { false };
    std::atomic<bool> m_errored { false };
    std::atomic<bool> m_processingInitializationSegment { false };

    // Seek logic support
    void seekToTarget(const SeekTarget&) final;
    bool seeking() const final;
    void seekInternal();
    void cancelPendingSeek(); // Called from destructor or running queue
    void completeSeek(const MediaTime&);
    Ref<GenericPromise> waitForTimeBuffered(const MediaTime&);
    void resolveWaitForTimeBufferedPromiseIfPossible();
    bool shouldBePlaying() const;

    // WorkQueue on which the player is running.
    WorkQueue& runningQueue() const { return m_runningQueue.get(); }
    void ensureOnRunningQueue(Function<void()>&&);
    MediaTime NODELETE durationOnRunningQueue() const;

    Timer m_seekTimer WTF_GUARDED_BY_CAPABILITY(mainThread);
    MediaTime m_lastSeekTime WTF_GUARDED_BY_CAPABILITY(runningQueue());
    std::optional<SeekTarget> m_pendingSeek WTF_GUARDED_BY_CAPABILITY(mainThread);
    std::atomic<bool> m_hasPendingSeek { false };
    std::optional<GenericPromise::AutoRejectProducer> m_waitForTimeBufferedPromise WTF_GUARDED_BY_CAPABILITY(runningQueue());
    Ref<NativePromiseRequest> m_rendererSeekRequest;
    std::atomic<bool> m_seeking { false };
#if HAVE(SPATIAL_TRACKING_LABEL)
    String m_defaultSpatialTrackingLabel WTF_GUARDED_BY_CAPABILITY(mainThread);
    String m_spatialTrackingLabel WTF_GUARDED_BY_CAPABILITY(mainThread);
#endif
    const MediaPlayerIdentifier m_playerIdentifier;
    const Ref<AudioVideoRenderer> m_renderer;
    const Ref<WorkQueue> m_runningQueue;
};

} // namespace WebCore

#endif // ENABLE(COCOA_WEBM_PLAYER)
