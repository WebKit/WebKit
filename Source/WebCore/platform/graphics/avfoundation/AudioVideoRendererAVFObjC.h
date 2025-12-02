/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/AudioVideoRenderer.h>
#include <WebCore/PlatformDynamicRangeLimit.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/TrackInfo.h>
#include <WebCore/WebAVSampleBufferListener.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/StdUnorderedMap.h>
#include <wtf/ThreadSafeWeakPtr.h>

OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferRenderSynchronizer;
OBJC_CLASS AVSampleBufferVideoRenderer;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer *CVPixelBufferRef;

namespace WebCore {

class CDMInstanceFairPlayStreamingAVFObjC;
class CDMSessionAVContentKeySession;
class EffectiveRateChangedListener;
class MediaSample;
class NativeImage;
class PixelBufferConformerCV;
class VideoLayerManagerObjC;
class VideoMediaSampleRenderer;

class AudioVideoRendererAVFObjC
    : public AudioVideoRenderer
    , public WebAVSampleBufferListenerClient
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AudioVideoRendererAVFObjC>
    , private LoggerHelper {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(AudioVideoRendererAVFObjC, WEBCORE_EXPORT);
public:
    static Ref<AudioVideoRendererAVFObjC> create(const Logger& logger, uint64_t logIdentifier) { return adoptRef(*new AudioVideoRendererAVFObjC(logger, logIdentifier)); }

    ~AudioVideoRendererAVFObjC();
    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

    void setPreferences(VideoRendererPreferences) final;
    void setHasProtectedVideoContent(bool) final;

    // TracksRendererInterface
    TrackIdentifier addTrack(TrackType) final;
    void removeTrack(TrackIdentifier) final;

    void enqueueSample(TrackIdentifier, Ref<MediaSample>&&, std::optional<MediaTime>) final;
    bool isReadyForMoreSamples(TrackIdentifier) final;
    Ref<RequestPromise> requestMediaDataWhenReady(TrackIdentifier) final;
    void notifyTrackNeedsReenqueuing(TrackIdentifier, Function<void(TrackIdentifier, const MediaTime&)>&&) final;

    bool timeIsProgressing() const final;
    MediaTime currentTime() const final;
    void notifyTimeReachedAndStall(const MediaTime&, Function<void(const MediaTime&)>&&) final;
    void cancelTimeReachedAction() final;
    void performTaskAtTime(const MediaTime&, Function<void(const MediaTime&)>&&) final;
    void setTimeObserver(Seconds, Function<void(const MediaTime&)>&&) final;
    void cancelTimeObserver();

    void flush() final;
    void flushTrack(TrackIdentifier) final;

    void applicationWillResignActive() final;

    void notifyWhenErrorOccurs(Function<void(PlatformMediaError)>&&) final;

    // SynchronizerInterface
    void play(std::optional<MonotonicTime>) final;
    void pause(std::optional<MonotonicTime>) final;
    bool paused() const final;
    void setRate(double) final;
    double effectiveRate() const final;
    void stall() final;
    void prepareToSeek() final;
    Ref<MediaTimePromise> seekTo(const MediaTime&) final;
    void notifyEffectiveRateChanged(Function<void(double)>&&) final;
    bool seeking() const final;

    // AudioInterface
    void setVolume(float) final;
    void setMuted(bool) final;
    void setPreservesPitchAndCorrectionAlgorithm(bool, std::optional<PitchCorrectionAlgorithm>) final;
    void setAudioTimePitchAlgorithm(AVSampleBufferAudioRenderer *, NSString *) const;
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    void setOutputDeviceId(const String&) final;
    void setOutputDeviceIdOnRenderer(AVSampleBufferAudioRenderer *);
#endif

    // VideoInterface
    void setIsVisible(bool);
    void setPresentationSize(const IntSize&) final;
    void setShouldMaintainAspectRatio(bool) final;
    void renderingCanBeAcceleratedChanged(bool) final;
    void contentBoxRectChanged(const LayoutRect&) final;
    void notifyFirstFrameAvailable(Function<void()>&&) final;
    void notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&&) final;
    void notifyWhenRequiresFlushToResume(Function<void()>&&) final;
    void notifyRenderingModeChanged(Function<void()>&&) final;
    void expectMinimumUpcomingPresentationTime(const MediaTime&) final;
    void notifySizeChanged(Function<void(const MediaTime&, FloatSize)>&&) final;
    void setShouldDisableHDR(bool) final;
    void setPlatformDynamicRangeLimit(const PlatformDynamicRangeLimit&) final;
    void setResourceOwner(const ProcessIdentity& resourceOwner) final { m_resourceOwner = resourceOwner; }
    RefPtr<VideoFrame> currentVideoFrame() const final;
    void paintCurrentVideoFrameInContext(GraphicsContext&, const FloatRect&) final;
    RefPtr<NativeImage> currentNativeImage() const final;
    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;
    PlatformLayer* platformVideoLayer() const final;
    void setVideoLayerSizeFenced(const FloatSize&, WTF::MachSendRightAnnotated&&) final;

    // VideoFullscreenInterface
    void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&&) final;
    void setVideoFullscreenFrame(const FloatRect&) final;
    void setTextTrackRepresentation(TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;
    Ref<GenericPromise> setVideoTarget(const PlatformVideoTarget&) final;
    void isInFullscreenOrPictureInPictureChanged(bool) final;

private:
    WEBCORE_EXPORT AudioVideoRendererAVFObjC(const Logger&, uint64_t);

    MediaTime clampTimeToLastSeekTime(const MediaTime&) const;
    void maybeCompleteSeek();
    bool shouldBePlaying() const;
    bool allRenderersHaveAvailableSamples() const { return m_allRenderersHaveAvailableSamples; }
    void updateAllRenderersHaveAvailableSamples();
    void setHasAvailableVideoFrame(bool);
    void setHasAvailableAudioSample(TrackIdentifier, bool);

    std::optional<TrackType> typeOf(TrackIdentifier) const;

    void addAudioRenderer(TrackIdentifier);
    void removeAudioRenderer(TrackIdentifier);
    void destroyAudioRenderers();
    void destroyAudioRenderer(RetainPtr<AVSampleBufferAudioRenderer>);
    RetainPtr<AVSampleBufferAudioRenderer> audioRendererFor(TrackIdentifier) const;
    void applyOnAudioRenderers(NOESCAPE Function<void(AVSampleBufferAudioRenderer *)>&&) const;

    Ref<GenericPromise> updateDisplayLayerIfNeeded();
    bool shouldEnsureLayerOrVideoRenderer() const;
    WebSampleBufferVideoRendering *layerOrVideoRenderer() const;
    Ref<GenericPromise> ensureLayerOrVideoRenderer();
    void ensureLayer();
    void destroyLayer();
    void ensureVideoRenderer();
    void destroyVideoRenderer();
    void destroyExpiringVideoRenderersIfNeeded();
    Ref<GenericPromise> setVideoRenderer(WebSampleBufferVideoRendering *);
    void configureHasAvailableVideoFrameCallbackIfNeeded();
    void configureLayerOrVideoRenderer(WebSampleBufferVideoRendering *);
    Ref<GenericPromise> stageVideoRenderer(WebSampleBufferVideoRendering *);
    void destroyVideoTrack();
    void removeRendererFromSynchronizerIfNeeded(id);

    enum class AcceleratedVideoMode: uint8_t {
        Layer = 0,
        VideoRenderer,
    };
    AcceleratedVideoMode acceleratedVideoMode() const;

    void notifyError(PlatformMediaError);
    // WebAVSampleBufferListenerClient
    void audioRendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *) final;
    void audioRendererWasAutomaticallyFlushed(AVSampleBufferAudioRenderer *, const CMTime&) final;

#if HAVE(SPATIAL_TRACKING_LABEL)
    void setSpatialTrackingInfo(bool prefersSpatialAudioExperience, SoundStageSize, const String& sceneIdentifier, const String& defaultLabel, const String& label) final;
    void updateSpatialTrackingLabel();
#endif

#if HAVE(AVCONTENTKEYSESSION)
#if ENABLE(ENCRYPTED_MEDIA)
    void setCDMInstance(CDMInstance*) final;
    Ref<MediaPromise> setInitData(Ref<SharedBuffer>) final;
    void attemptToDecrypt() final;
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<SharedBuffer> initData() const final { return m_initData; }
    void setCDMSession(LegacyCDMSession*) final;
#endif
#endif

    void setSynchronizerRate(float, std::optional<MonotonicTime>);
    bool updateLastPixelBuffer();
    void maybePurgeLastPixelBuffer();
    void setNeedsPlaceholderImage(bool);

    bool isEnabledVideoTrackId(TrackIdentifier) const;
    bool hasSelectedVideo() const;
    void flushVideo();
    void flushAudio();
    void flushAudioTrack(TrackIdentifier);
    void notifyRequiresFlushToResume();

    void cancelSeekingPromiseIfNeeded();

    RefPtr<VideoMediaSampleRenderer> protectedVideoRenderer() const;
    bool canUseDecompressionSession() const;
    bool isUsingDecompressionSession() const;
    bool willUseDecompressionSessionIfNeeded() const;

    void sizeWillChangeAtTime(const MediaTime&, const FloatSize&);
    void flushPendingSizeChanges();

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    void tryToEnqueueBlockedSamples();
    bool canEnqueueSample(TrackIdentifier, const MediaSample&);
    void attachContentKeyToSampleIfNeeded(const MediaSample&);
#endif

    // Logger
    const Logger& logger() const final { return m_logger.get(); }
    Ref<const Logger> protectedLogger() const { return logger(); }
    ASCIILiteral logClassName() const final { return "AudioVideoRendererAVFObjC"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    enum SeekState {
        Preparing,
        RequiresFlush,
        Seeking,
        WaitingForAvailableFame,
        SeekCompleted,
    };
    struct AudioTrackProperties {
        bool hasAudibleSample { false };
        std::unique_ptr<RequestPromise::AutoRejectProducer> requestPromise;
        Function<void(TrackIdentifier, const MediaTime&)> callbackForReenqueuing;
    };
    AudioTrackProperties& audioTrackPropertiesFor(TrackIdentifier);

    String toString(TrackIdentifier) const;
    String toString(SeekState) const;

    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
    const UniqueRef<VideoLayerManagerObjC> m_videoLayerManager;
    const RetainPtr<AVSampleBufferRenderSynchronizer> m_synchronizer;
    const Ref<WebAVSampleBufferListener> m_listener;

    Function<void(PlatformMediaError)> m_errorCallback;
    Function<void()> m_firstFrameAvailableCallback;
    Function<void(const MediaTime&, double)> m_hasAvailableVideoFrameCallback;
    Function<void()> m_notifyWhenRequiresFlushToResume;
    Function<void()> m_renderingModeChangedCallback;
    Function<void(const MediaTime&, FloatSize)> m_sizeChangedCallback;

    RetainPtr<id> m_currentTimeObserver;
    RetainPtr<id> m_performTaskObserver;
    RetainPtr<id> m_timeChangedObserver;
    Function<void(const MediaTime&)> m_currentTimeDidChangeCallback;

    bool m_isPlaying { false };
    double m_rate { 1 };
    RetainPtr<CVPixelBufferRef> m_lastPixelBuffer;
    bool m_needsPlaceholderImage { false };

    float m_volume { 1 };
    bool m_muted { false };
    bool m_preservesPitch { true };
    std::optional<PitchCorrectionAlgorithm> m_pitchCorrectionAlgorithm;
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    String m_audioOutputDeviceId;
#endif

    // Seek Logic
    MediaTime m_lastSeekTime;
    SeekState m_seekState { SeekCompleted };
    std::optional<MediaTimePromise::Producer> m_seekPromise;
    RetainPtr<id> m_timeJumpedObserver;
    bool m_isSynchronizerSeeking { false };
    bool m_hasAvailableVideoFrame { false };
    bool m_allRenderersHaveAvailableSamples { false };

    HashMap<TrackIdentifier, AudioTrackProperties> m_audioTracksMap;
    std::optional<RequestPromise::AutoRejectProducer> m_requestVideoPromise;
    bool m_readyToRequestVideoData { true };
    bool m_readyToRequestAudioData { true };

    HashMap<TrackIdentifier, TrackType> m_trackTypes;
    HashMap<TrackIdentifier, RetainPtr<AVSampleBufferAudioRenderer>> m_audioRenderers;
    RetainPtr<AVSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;
    RetainPtr<AVSampleBufferVideoRenderer> m_sampleBufferVideoRenderer;
    RefPtr<VideoMediaSampleRenderer> m_videoRenderer;
    Vector<RetainPtr<AVSampleBufferVideoRenderer>> m_expiringSampleBufferVideoRenderers;
    enum class SampleBufferLayerState : uint8_t {
        AddedToSynchronizer,
        PendingRemovalFromSynchronizer,
        RemovedFromSynchronizer
    };
    SampleBufferLayerState m_sampleBufferDisplayLayerState { SampleBufferLayerState::RemovedFromSynchronizer };
    bool m_renderingCanBeAccelerated { false };
    bool m_visible { false };
    IntSize m_presentationSize;
    bool m_shouldMaintainAspectRatio { true };
    std::optional<TrackIdentifier> m_enabledVideoTrackId;
    std::optional<FloatSize> m_cachedSize;
    Deque<RetainPtr<id>> m_sizeChangeObservers;
    bool m_shouldDisableHDR { false };
    PlatformDynamicRangeLimit m_dynamicRangeLimit { PlatformDynamicRangeLimit::initialValueForVideos() };
    ProcessIdentity m_resourceOwner;
    VideoRendererPreferences m_preferences;
    bool m_hasProtectedVideoContent { false };
    struct RendererConfiguration {
        bool canUseDecompressionSession { false };
        bool isProtected { false };
        bool hasVideoTrack { false };
        bool operator==(const RendererConfiguration&) const = default;
    };
    RendererConfiguration m_previousRendererConfiguration;

    // Video Frame metadata gathering
    RetainPtr<id> m_videoFrameMetadataGatheringObserver;
    MonotonicTime m_startupTime;

    RefPtr<EffectiveRateChangedListener> m_effectiveRateChangedListener;

    mutable std::unique_ptr<PixelBufferConformerCV> m_rgbConformer;

#if HAVE(SPATIAL_TRACKING_LABEL)
    bool m_prefersSpatialAudioExperience { false };
    SoundStageSize m_soundStage { SoundStageSize::Auto };
    String m_sceneIdentifier;
    String m_defaultSpatialTrackingLabel;
    String m_spatialTrackingLabel;
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
    RetainPtr<FigVideoTargetRef> m_videoTarget;
#endif
#if HAVE(AVCONTENTKEYSESSION)
#if ENABLE(ENCRYPTED_MEDIA)
    RefPtr<CDMInstanceFairPlayStreamingAVFObjC> m_cdmInstance;
    const Ref<Observer<void()>> m_keyStatusesChangedObserver;
    using KeyIDs = Vector<Ref<SharedBuffer>>;
    KeyIDs m_keyIDs;
    using TrackKeyIdsMap = HashMap<TrackIdentifier, KeyIDs>;
    TrackKeyIdsMap m_currentTrackIds;
    Deque<std::pair<TrackIdentifier, Ref<MediaSample>>> m_blockedSamples;
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<SharedBuffer> m_initData;
    ThreadSafeWeakPtr<CDMSessionAVContentKeySession> m_session;
#endif
#endif
};

} // namespace WebCore
