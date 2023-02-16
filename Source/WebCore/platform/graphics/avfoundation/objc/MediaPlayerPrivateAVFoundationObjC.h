/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#include "MediaPlayerPrivateAVFoundation.h"
#include "VideoFrameMetadata.h"
#include <CoreMedia/CMTime.h>
#include <wtf/Function.h>
#include <wtf/Observer.h>
#include <wtf/RobinHoodHashMap.h>

OBJC_CLASS AVAssetImageGenerator;
OBJC_CLASS AVAssetTrack;
OBJC_CLASS AVAssetResourceLoadingRequest;
OBJC_CLASS AVMediaSelectionGroup;
OBJC_CLASS AVOutputContext;
OBJC_CLASS AVPlayerItem;
OBJC_CLASS AVPlayerItemLegibleOutput;
OBJC_CLASS AVPlayerItemMetadataCollector;
OBJC_CLASS AVPlayerItemMetadataOutput;
OBJC_CLASS AVPlayerItemVideoOutput;
OBJC_CLASS AVPlayerLayer;
OBJC_CLASS AVURLAsset;
OBJC_CLASS NSArray;
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS WebCoreAVFLoaderDelegate;
OBJC_CLASS WebCoreAVFMovieObserver;
OBJC_CLASS WebCoreAVFPullDelegate;

typedef struct CGImage *CGImageRef;
typedef struct __CVBuffer *CVPixelBufferRef;
typedef NSString *AVMediaCharacteristic;
typedef double NSTimeInterval;

namespace WebCore {

class AudioSourceProviderAVFObjC;
class AudioTrackPrivateAVFObjC;
class CDMInstanceFairPlayStreamingAVFObjC;
class CDMSessionAVFoundationObjC;
class ImageRotationSessionVT;
class InbandChapterTrackPrivateAVFObjC;
class InbandMetadataTextTrackPrivateAVF;
class MediaPlaybackTarget;
class MediaSelectionGroupAVFObjC;
class PixelBufferConformerCV;
class QueuedVideoOutput;
class SharedBuffer;
class VideoLayerManagerObjC;
class VideoTrackPrivateAVFObjC;
class WebCoreAVFResourceLoader;

class MediaPlayerPrivateAVFoundationObjC final : public MediaPlayerPrivateAVFoundation {
public:
    explicit MediaPlayerPrivateAVFoundationObjC(MediaPlayer*);
    virtual ~MediaPlayerPrivateAVFoundationObjC();

    static void registerMediaEngine(MediaEngineRegistrar);

    void setAsset(RetainPtr<id>&&);
    void didEnd() final;
    void metadataLoaded() final;

    void processCue(NSArray *, NSArray *, const MediaTime&);
    void flushCues();

    bool shouldWaitForLoadingOfResource(AVAssetResourceLoadingRequest *);
    void didCancelLoadingRequest(AVAssetResourceLoadingRequest *);
    void didStopLoadingRequest(AVAssetResourceLoadingRequest *);

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RetainPtr<AVAssetResourceLoadingRequest> takeRequestForKeyURI(const String&);
#endif

    void playerItemStatusDidChange(int);
    void playbackLikelyToKeepUpWillChange();
    void playbackLikelyToKeepUpDidChange(bool);
    void playbackBufferEmptyWillChange();
    void playbackBufferEmptyDidChange(bool);
    void playbackBufferFullWillChange();
    void playbackBufferFullDidChange(bool);
    void loadedTimeRangesDidChange(RetainPtr<NSArray>&&);
    void seekableTimeRangesDidChange(RetainPtr<NSArray>&&, NSTimeInterval, NSTimeInterval);
    void tracksDidChange(const RetainPtr<NSArray>&);
    void hasEnabledAudioDidChange(bool);
    void hasEnabledVideoDidChange(bool);
    void presentationSizeDidChange(FloatSize);
    void durationDidChange(const MediaTime&);
    void rateDidChange(double);
    void timeControlStatusDidChange(int);
    void metadataGroupDidArrive(const RetainPtr<NSArray>&, const MediaTime&);
    void metadataDidArrive(const RetainPtr<NSArray>&, const MediaTime&);
    void firstFrameAvailableDidChange(bool);
    void trackEnabledDidChange(bool);
    void canPlayFastReverseDidChange(bool);
    void canPlayFastForwardDidChange(bool);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void playbackTargetIsWirelessDidChange();
#endif

    void outputObscuredDueToInsufficientExternalProtectionChanged(bool);

    MediaTime currentMediaTime() const final;
    void outputMediaDataWillChange();
    void processChapterTracks();

private:
#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&) final;
    void cdmInstanceDetached(CDMInstance&) final;
    void attemptToDecryptWithInstance(CDMInstance&) final;
    void setWaitingForKey(bool);
    bool waitingForKey() const final { return m_waitingForKey; }
#endif

    float currentTime() const final;

    // engine support
    class Factory;
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types);
    static MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters&);
    static bool supportsKeySystem(const String& keySystem, const String& mimeType);
    static HashSet<SecurityOriginData> originsInMediaCache(const String&);
    static void clearMediaCache(const String&, WallTime modifiedSince);
    static void clearMediaCacheForOrigins(const String&, const HashSet<SecurityOriginData>&);

    void setBufferingPolicy(MediaPlayer::BufferingPolicy) final;

    void tracksChanged() final;

    void cancelLoad() final;

    void beginSimulatedHDCPError() final { outputObscuredDueToInsufficientExternalProtectionChanged(true); }
    void endSimulatedHDCPError() final { outputObscuredDueToInsufficientExternalProtectionChanged(false); }

#if ENABLE(AVF_CAPTIONS)
    void notifyTrackModeChanged() final;
    void synchronizeTextTrackState() final;
#endif

    void platformSetVisible(bool) final;
    void platformPlay() final;
    void platformPause() final;
    bool platformPaused() const final;
    void setVolume(float) final;
    void setMuted(bool) final;
    void paint(GraphicsContext&, const FloatRect&) final;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) final;
    PlatformLayer* platformLayer() const final;
#if ENABLE(VIDEO_PRESENTATION_MODE)
    RetainPtr<PlatformLayer> createVideoFullscreenLayer() final;
    void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&& completionHandler) final;
    void updateVideoFullscreenInlineImage() final;
    void setVideoFullscreenFrame(FloatRect) final;
    void setVideoFullscreenGravity(MediaPlayer::VideoGravity) final;
    void setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode) final;
    void videoFullscreenStandbyChanged() final;
#endif
    void setPlayerRate(double, std::optional<MonotonicTime>&& = std::nullopt);

#if PLATFORM(IOS_FAMILY)
    NSArray *timedMetadata() const final;
    String accessLog() const final;
    String errorLog() const final;
#endif

    bool supportsAcceleratedRendering() const final { return true; }
    MediaTime mediaTimeForTimeValue(const MediaTime&) const final;

    void createAVPlayer() final;
    void createAVPlayerItem() final;
    virtual void createAVPlayerLayer();
    void createAVAssetForURL(const URL&) final;
    void createAVAssetForURL(const URL&, RetainPtr<NSMutableDictionary>);
    MediaPlayerPrivateAVFoundation::ItemStatus playerItemStatus() const final;
    MediaPlayerPrivateAVFoundation::AssetStatus assetStatus() const final;
    long assetErrorCode() const final;

    double seekableTimeRangesLastModifiedTime() const final;
    double liveUpdateInterval() const final;

    void checkPlayability();
    void setRateDouble(double) final;
    double rate() const final;
    double effectiveRate() const final;
    void setPreservesPitch(bool) final;
    void setPitchCorrectionAlgorithm(MediaPlayer::PitchCorrectionAlgorithm) final;
    void seekToTime(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance) final;
    unsigned long long totalBytes() const final;
    std::unique_ptr<PlatformTimeRanges> platformBufferedTimeRanges() const final;
    MediaTime platformMinTimeSeekable() const final;
    MediaTime platformMaxTimeSeekable() const final;
    MediaTime platformDuration() const final;
    MediaTime platformMaxTimeLoaded() const final;
    void beginLoadingMetadata() final;
    void sizeChanged() final;
    void resolvedURLChanged() final;

    bool hasAvailableVideoFrame() const final;

    void createContextVideoRenderer() final;
    void destroyContextVideoRenderer() final;

    void createVideoLayer() final;
    void destroyVideoLayer() final;

    bool hasContextRenderer() const final;
    bool hasLayerRenderer() const final;


    enum class ShouldAnimate : bool { No, Yes };
    void updateVideoLayerGravity() final { updateVideoLayerGravity(ShouldAnimate::No); }
    void updateVideoLayerGravity(ShouldAnimate);

    bool didPassCORSAccessCheck() const final;
    std::optional<bool> isCrossOrigin(const SecurityOrigin&) const final;

    MediaTime getStartDate() const final;

    bool requiresTextTrackRepresentation() const final;
    void setTextTrackRepresentation(TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;

    void setAVPlayerItem(AVPlayerItem *);

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    AudioSourceProvider* audioSourceProvider() final;
#endif

    void createImageGenerator();
    void destroyImageGenerator();
    RetainPtr<CGImageRef> createImageForTimeInRect(float, const FloatRect&);
    void paintWithImageGenerator(GraphicsContext&, const FloatRect&);

    enum class UpdateType { UpdateSynchronously, UpdateAsynchronously };
    void updateLastImage(UpdateType type = UpdateType::UpdateAsynchronously);

    void createVideoOutput();
    void destroyVideoOutput();
    bool updateLastPixelBuffer();
    bool videoOutputHasAvailableFrame();
    void paintWithVideoOutput(GraphicsContext&, const FloatRect&);
    RefPtr<VideoFrame> videoFrameForCurrentTime() final;
    RefPtr<NativeImage> nativeImageForCurrentTime() final;
    DestinationColorSpace colorSpace() final;
    void waitForVideoOutputMediaDataWillChange();

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void keyAdded() final;
    std::unique_ptr<LegacyCDMSession> createSession(const String& keySystem, LegacyCDMSessionClient&) final;
#endif

    String languageOfPrimaryAudioTrack() const final;

    void processMediaSelectionOptions();
    bool hasLoadedMediaSelectionGroups();

    AVMediaSelectionGroup *safeMediaSelectionGroupForLegibleMedia();
    AVMediaSelectionGroup *safeMediaSelectionGroupForAudibleMedia();
    AVMediaSelectionGroup *safeMediaSelectionGroupForVisualMedia();

    AVAssetTrack* firstEnabledAudibleTrack() const;
    AVAssetTrack* firstEnabledVisibleTrack() const;
    AVAssetTrack* firstEnabledTrack(AVMediaCharacteristic) const;

#if ENABLE(DATACUE_VALUE)
    void processMetadataTrack();
#endif

    void setCurrentTextTrack(InbandTextTrackPrivateAVF*) final;
    InbandTextTrackPrivateAVF* currentTextTrack() const final { return m_currentTextTrack; }

    void updateAudioTracks();
    void updateVideoTracks();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool hasVideo() const final;
    bool hasAudio() const final;
    bool isCurrentPlaybackTargetWireless() const final;
    String wirelessPlaybackTargetName() const final;
    MediaPlayer::WirelessPlaybackTargetType wirelessPlaybackTargetType() const final;
    bool wirelessVideoPlaybackDisabled() const final;
    void setWirelessVideoPlaybackDisabled(bool) final;
    bool canPlayToWirelessPlaybackTarget() const final { return true; }
    void updateDisableExternalPlayback();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && PLATFORM(MAC)
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) final;
    void setShouldPlayToPlaybackTarget(bool) final;
#endif

    double maxFastForwardRate() const final { return m_cachedCanPlayFastForward ? std::numeric_limits<double>::infinity() : 2.0; }
    double minFastReverseRate() const final { return m_cachedCanPlayFastReverse ? -std::numeric_limits<double>::infinity() : 0.0; }

    Vector<String> preferredAudioCharacteristics() const;

    void setShouldDisableSleep(bool) final;
    void updateRotationSession();

    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "MediaPlayerPrivateAVFoundationObjC"; }
#endif

    AVPlayer *objCAVFoundationAVPlayer() const final { return m_avPlayer.get(); }

    bool performTaskAtMediaTime(Function<void()>&&, const MediaTime&) final;
    void setShouldObserveTimeControlStatus(bool);

    void setPreferredDynamicRangeMode(DynamicRangeMode) final;
    void audioOutputDeviceChanged() final;

    void currentMediaTimeDidChange(MediaTime&&) const;
    bool setCurrentTimeDidChangeCallback(MediaPlayer::CurrentTimeDidChangeCallback&&) final;

    bool currentMediaTimeIsBuffered() const;

    bool supportsPlayAtHostTime() const final { return true; }
    bool supportsPauseAtHostTime() const final { return true; }
    bool playAtHostTime(const MonotonicTime&) final;
    bool pauseAtHostTime(const MonotonicTime&) final;
    bool haveBeenAskedToPaint() const { return m_haveBeenAskedToPaint; }

    void startVideoFrameMetadataGathering() final;
    void stopVideoFrameMetadataGathering() final;
    std::optional<VideoFrameMetadata> videoFrameMetadata() final { return std::exchange(m_videoFrameMetadata, { }); }
    void setResourceOwner(const ProcessIdentity& resourceOwner) final { m_resourceOwner = resourceOwner; }

    void checkNewVideoFrameMetadata();

    void setShouldDisableHDR(bool) final;

    std::optional<bool> allTracksArePlayable() const;
    bool containsDisabledTracks() const;
    bool trackIsPlayable(AVAssetTrack*) const;

    RetainPtr<AVURLAsset> m_avAsset;
    RetainPtr<AVPlayer> m_avPlayer;
    RetainPtr<AVPlayerItem> m_avPlayerItem;
    RetainPtr<AVPlayerLayer> m_videoLayer;
    std::unique_ptr<VideoLayerManagerObjC> m_videoLayerManager;
    MediaPlayer::VideoGravity m_videoFullscreenGravity { MediaPlayer::VideoGravity::ResizeAspect };
    RetainPtr<WebCoreAVFMovieObserver> m_objcObserver;
    RetainPtr<id> m_timeObserver;
    mutable String m_languageOfPrimaryAudioTrack;
    bool m_videoFrameHasDrawn { false };
    bool m_haveCheckedPlayability { false };
    bool m_createAssetPending { false };

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    RefPtr<AudioSourceProviderAVFObjC> m_provider;
#endif

    RetainPtr<AVAssetImageGenerator> m_imageGenerator;
    RefPtr<QueuedVideoOutput> m_videoOutput;
    RetainPtr<WebCoreAVFPullDelegate> m_videoOutputDelegate;
    RetainPtr<CVPixelBufferRef> m_lastPixelBuffer;
    RefPtr<NativeImage> m_lastImage;
    std::unique_ptr<ImageRotationSessionVT> m_imageRotationSession;
    std::unique_ptr<PixelBufferConformerCV> m_pixelBufferConformer;

    friend class WebCoreAVFResourceLoader;
    HashMap<RetainPtr<CFTypeRef>, RefPtr<WebCoreAVFResourceLoader>> m_resourceLoaderMap;
    RetainPtr<WebCoreAVFLoaderDelegate> m_loaderDelegate;
    MemoryCompactRobinHoodHashMap<String, RetainPtr<AVAssetResourceLoadingRequest>> m_keyURIToRequestMap;
    MemoryCompactRobinHoodHashMap<String, RetainPtr<AVAssetResourceLoadingRequest>> m_sessionIDToRequestMap;

    RetainPtr<AVPlayerItemLegibleOutput> m_legibleOutput;

    Vector<RefPtr<AudioTrackPrivateAVFObjC>> m_audioTracks;
    Vector<RefPtr<VideoTrackPrivateAVFObjC>> m_videoTracks;
    RefPtr<MediaSelectionGroupAVFObjC> m_audibleGroup;
    RefPtr<MediaSelectionGroupAVFObjC> m_visualGroup;

    InbandTextTrackPrivateAVF* m_currentTextTrack { nullptr };

#if ENABLE(DATACUE_VALUE)
    RefPtr<InbandMetadataTextTrackPrivateAVF> m_metadataTrack;
#endif

    MemoryCompactRobinHoodHashMap<String, RefPtr<InbandChapterTrackPrivateAVFObjC>> m_chapterTracks;

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && PLATFORM(MAC)
    RetainPtr<AVOutputContext> m_outputContext;
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    WeakPtr<CDMSessionAVFoundationObjC> m_session;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    bool m_waitingForKey { false };
#endif

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    RefPtr<CDMInstanceFairPlayStreamingAVFObjC> m_cdmInstance;
#endif

    RetainPtr<AVPlayerItemMetadataCollector> m_metadataCollector;
    RetainPtr<AVPlayerItemMetadataOutput> m_metadataOutput;
    RetainPtr<id> m_currentTimeObserver;

    mutable RetainPtr<NSArray> m_cachedSeekableRanges;
    mutable RetainPtr<NSArray> m_cachedLoadedRanges;
    RetainPtr<NSArray> m_cachedTracks;
    RetainPtr<NSArray> m_currentMetaData;
    FloatSize m_cachedPresentationSize;
    MediaTime m_cachedDuration;
    mutable MediaPlayer::CurrentTimeDidChangeCallback m_currentTimeDidChangeCallback;
    mutable MediaTime m_cachedCurrentMediaTime { -1, 1, 0 };
    mutable MediaTime m_lastPeriodicObserverMediaTime;
    mutable Markable<WallTime> m_wallClockAtCachedCurrentTime;
    mutable int m_timeControlStatusAtCachedCurrentTime { 0 };
    mutable double m_requestedRateAtCachedCurrentTime { 0 };
    RefPtr<SharedBuffer> m_keyID;
    double m_cachedRate { 0 };
    bool m_requestedPlaying { false };
    double m_requestedRate { 1.0 };
    int m_cachedTimeControlStatus { 0 };
    mutable long long m_cachedTotalBytes { 0 };
    unsigned m_pendingStatusChanges { 0 };
    int m_cachedItemStatus;
    int m_runLoopNestingLevel { 0 };
    MediaPlayer::BufferingPolicy m_bufferingPolicy { MediaPlayer::BufferingPolicy::Default };
    bool m_cachedLikelyToKeepUp { false };
    bool m_cachedBufferEmpty { false };
    bool m_cachedBufferFull { false };
    bool m_cachedHasEnabledAudio { false };
    bool m_cachedHasEnabledVideo { false };
    bool m_cachedIsReadyForDisplay { false };
    bool m_haveBeenAskedToCreateLayer { false };
    bool m_cachedCanPlayFastForward { false };
    bool m_cachedCanPlayFastReverse { false };
    mutable bool m_cachedAssetIsLoaded { false };
    mutable bool m_cachedTracksAreLoaded { false };
    mutable std::optional<bool> m_cachedAssetIsPlayable;
    mutable std::optional<bool> m_cachedTracksArePlayable;
    bool m_muted { false };
    bool m_shouldObserveTimeControlStatus { false };
    mutable std::optional<bool> m_tracksArePlayable;
    bool m_automaticallyWaitsToMinimizeStalling { false };
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    mutable bool m_allowsWirelessVideoPlayback { true };
    bool m_shouldPlayToPlaybackTarget { false };
#endif
    bool m_haveProcessedChapterTracks { false };
    bool m_waitForVideoOutputMediaDataWillChangeTimedOut { false };
    bool m_haveBeenAskedToPaint { false };
    uint64_t m_sampleCount { 0 };
    RetainPtr<id> m_videoFrameMetadataGatheringObserver;
    bool m_isGatheringVideoFrameMetadata { false };
    std::optional<VideoFrameMetadata> m_videoFrameMetadata;
    mutable std::optional<NSTimeInterval> m_cachedSeekableTimeRangesLastModifiedTime;
    mutable std::optional<NSTimeInterval> m_cachedLiveUpdateInterval;
    std::unique_ptr<Observer<void()>> m_currentImageChangedObserver;
    std::unique_ptr<Observer<void()>> m_waitForVideoOutputMediaDataWillChangeObserver;
    ProcessIdentity m_resourceOwner;
};

}

#endif
