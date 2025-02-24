/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include "MediaPlayerPrivate.h"
#include "SourceBufferPrivateClient.h"
#include "VideoFrameMetadata.h"
#include <CoreMedia/CMTime.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVAsset;
OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferRenderSynchronizer;
OBJC_CLASS AVSampleBufferVideoRenderer;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);

typedef struct OpaqueCMTimebase* CMTimebaseRef;
typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct __CVBuffer *CVOpenGLTextureRef;
typedef struct OpaqueFigVideoTarget *FigVideoTargetRef;

namespace WebCore {

class AudioTrackPrivate;
class CDMSessionAVContentKeySession;
class EffectiveRateChangedListener;
class InbandTextTrackPrivate;
class MediaSourcePrivateAVFObjC;
class PixelBufferConformerCV;
class VideoLayerManagerObjC;
class VideoMediaSampleRenderer;
class VideoTrackPrivate;

class MediaPlayerPrivateMediaSourceAVFObjC final
    : public CanMakeWeakPtr<MediaPlayerPrivateMediaSourceAVFObjC>
    , public RefCounted<MediaPlayerPrivateMediaSourceAVFObjC>
    , public MediaPlayerPrivateInterface
    , private LoggerHelper
{
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    explicit MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer*);
    virtual ~MediaPlayerPrivateMediaSourceAVFObjC();

    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::AVFObjCMSE; }

    static void registerMediaEngine(MediaEngineRegistrar);

    // MediaPlayer Factory Methods
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters&);

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    void addAudioRenderer(AVSampleBufferAudioRenderer*);
    void removeAudioRenderer(AVSampleBufferAudioRenderer*);
ALLOW_NEW_API_WITHOUT_GUARDS_END

    void removeAudioTrack(AudioTrackPrivate&);
    void removeVideoTrack(VideoTrackPrivate&);
    void removeTextTrack(InbandTextTrackPrivate&);

    MediaPlayer::NetworkState networkState() const final;
    MediaPlayer::ReadyState readyState() const final;
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

    void seekInternal();
    void maybeCompleteSeek();
    void setLoadingProgresssed(bool flag) { m_loadingProgressed = flag; }
    void setHasAvailableVideoFrame(bool);
    bool hasAvailableVideoFrame() const final;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    void setHasAvailableAudioSample(AVSampleBufferAudioRenderer*, bool);
ALLOW_NEW_API_WITHOUT_GUARDS_END
    bool allRenderersHaveAvailableSamples() const { return m_allRenderersHaveAvailableSamples; }
    void updateAllRenderersHaveAvailableSamples();
    void durationChanged();

    void effectiveRateChanged();
    void sizeWillChangeAtTime(const MediaTime&, const FloatSize&);
    void setNaturalSize(const FloatSize&);
    void flushPendingSizeChanges();
    void characteristicsChanged();

    MediaTime currentTime() const final;
    bool timeIsProgressing() const final;
    bool hasVideoRenderer() const;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    RetainPtr<PlatformLayer> createVideoFullscreenLayer() final;
    void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&& completionHandler) final;
    void setVideoFullscreenFrame(FloatRect) final;
#endif

    void setTextTrackRepresentation(TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void setCDMSession(LegacyCDMSession*) final;
    CDMSessionAVContentKeySession* cdmSession() const;
    void keyAdded() final;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&) final;
    void cdmInstanceDetached(CDMInstance&) final;
    void attemptToDecryptWithInstance(CDMInstance&) final;
    bool waitingForKey() const final;
    void waitingForKeyChanged();
#endif

    void outputObscuredDueToInsufficientExternalProtectionChanged(bool);
    void beginSimulatedHDCPError() final { outputObscuredDueToInsufficientExternalProtectionChanged(true); }
    void endSimulatedHDCPError() final { outputObscuredDueToInsufficientExternalProtectionChanged(false); }

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
    void keyNeeded(const SharedBuffer&);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void initializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&);
#endif

    const Vector<ContentType>& mediaContentTypesRequiringHardwareSupport() const;

    void needsVideoLayerChanged();
    void setNeedsPlaceholderImage(bool);

#if ENABLE(LINEAR_MEDIA_PLAYER)
    void setVideoTarget(const PlatformVideoTarget&) final;
#endif

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const final { return "MediaPlayerPrivateMediaSourceAVFObjC"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    uint64_t mediaPlayerLogIdentifier() { return logIdentifier(); }
    const Logger& mediaPlayerLogger() { return logger(); }
#endif

    enum SeekState {
        Seeking,
        WaitingForAvailableFame,
        SeekCompleted,
    };

    bool supportsLimitedMatroska() const { return m_loadOptions.supportsLimitedMatroska; }

private:
    // MediaPlayerPrivateInterface
    void load(const String& url) final;
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) final;
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final;
#endif
    void cancelLoad() final;

    void prepareToPlay() final;
    PlatformLayer* platformLayer() const final;

    bool supportsPictureInPicture() const final { return true; }
    bool supportsFullscreen() const final { return true; }

    void play() final;
    void playInternal(std::optional<MonotonicTime>&& = std::nullopt);

    void pause() final;
    void pauseInternal(std::optional<MonotonicTime>&& = std::nullopt);

    bool paused() const final;

    void setVolume(float) final;
    void setMuted(bool) final;

    bool supportsScanning() const final;

    FloatSize naturalSize() const final;

    bool hasVideo() const final;
    bool hasAudio() const final;

    void setPageIsVisible(bool) final;

    MediaTime duration() const final;
    MediaTime startTime() const final;
    MediaTime initialTime() const final;

    void seekToTarget(const SeekTarget&) final;
    bool seeking() const final;
    void setRateDouble(double) final;
    double rate() const final;
    double effectiveRate() const final;

    void setPreservesPitch(bool) final;

    MediaTime maxTimeSeekable() const final;
    MediaTime minTimeSeekable() const final;
    const PlatformTimeRanges& buffered() const final;

    bool didLoadingProgress() const final;

    RefPtr<NativeImage> nativeImageForCurrentTime() final;
    bool updateLastPixelBuffer();
    bool updateLastImage();
    void maybePurgeLastImage();
    void paint(GraphicsContext&, const FloatRect&) final;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) final;
    RefPtr<VideoFrame> videoFrameForCurrentTime() final;
    DestinationColorSpace colorSpace() final;

    bool supportsAcceleratedRendering() const final;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    void acceleratedRenderingStateChanged() final;
    void notifyActiveSourceBuffersChanged() final;

    void setPresentationSize(const IntSize&) final;
    void setVideoLayerSizeFenced(const FloatSize&, WTF::MachSendRight&&) final;

    void updateDisplayLayer();
    RefPtr<VideoMediaSampleRenderer> layerOrVideoRenderer() const;

    RefPtr<MediaSourcePrivateAVFObjC> protectedMediaSourcePrivate() const;

    // NOTE: Because the only way for MSE to recieve data is through an ArrayBuffer provided by
    // javascript running in the page, the video will, by necessity, always be CORS correct and
    // in the page's origin.
    bool didPassCORSAccessCheck() const final { return true; }

    MediaPlayer::MovieLoadType movieLoadType() const final;

    void prepareForRendering() final;

    String engineDescription() const final;

    String languageOfPrimaryAudioTrack() const final;

    size_t extraMemoryCost() const final;

    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool isCurrentPlaybackTargetWireless() const final;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) final;
    void setShouldPlayToPlaybackTarget(bool) final;
    bool wirelessVideoPlaybackDisabled() const final { return false; }
#endif

    bool performTaskAtTime(Function<void()>&&, const MediaTime&) final;
    void audioOutputDeviceChanged() final;

    void ensureLayer();
    void destroyLayer();
    void ensureVideoRenderer();
    void destroyVideoRenderer();

    bool isUsingRenderlessMediaSampleRenderer() const;
    void ensureRenderlessVideoMediaSampleRenderer();
    MediaPlayerEnums::NeedsRenderingModeChanged destroyRenderlessVideoMediaSampleRenderer();

    bool shouldEnsureLayerOrVideoRenderer() const;
    void ensureLayerOrVideoRenderer(MediaPlayerEnums::NeedsRenderingModeChanged);
    void destroyLayerOrVideoRenderer();
    Ref<VideoMediaSampleRenderer> createVideoMediaSampleRendererForRendererer(WebSampleBufferVideoRendering *);
    void configureLayerOrVideoRenderer(WebSampleBufferVideoRendering *);

    bool shouldBePlaying() const;
    void setSynchronizerRate(double, std::optional<MonotonicTime>&& = std::nullopt);

    bool setCurrentTimeDidChangeCallback(MediaPlayer::CurrentTimeDidChangeCallback&&) final;

    bool supportsPlayAtHostTime() const final { return true; }
    bool supportsPauseAtHostTime() const final { return true; }
    bool playAtHostTime(const MonotonicTime&) final;
    bool pauseAtHostTime(const MonotonicTime&) final;

    void startVideoFrameMetadataGathering() final;
    void stopVideoFrameMetadataGathering() final;
    std::optional<VideoFrameMetadata> videoFrameMetadata() final { return std::exchange(m_videoFrameMetadata, { }); }
    void setResourceOwner(const ProcessIdentity& resourceOwner) final { m_resourceOwner = resourceOwner; }

    void checkNewVideoFrameMetadata(MediaTime, double);
    MediaTime clampTimeToSensicalValue(const MediaTime&) const;

    void setShouldDisableHDR(bool) final;
    void playerContentBoxRectChanged(const LayoutRect&) final;
    void setShouldMaintainAspectRatio(bool) final;

#if HAVE(SPATIAL_TRACKING_LABEL)
    const String& defaultSpatialTrackingLabel() const final;
    void setDefaultSpatialTrackingLabel(const String&) final;
    const String& spatialTrackingLabel() const final;
    void setSpatialTrackingLabel(const String&) final;
    void updateSpatialTrackingLabel();
#endif

    void isInFullscreenOrPictureInPictureChanged(bool) final;

    void setDecompressionSessionPreferences(bool preferDecompressionSession, bool canFallbackToDecompressionSession) final
    {
        m_preferDecompressionSession = preferDecompressionSession;
        m_canFallbackToDecompressionSession = canFallbackToDecompressionSession;
    }

#if ENABLE(LINEAR_MEDIA_PLAYER)
    bool supportsLinearMediaPlayer() const final { return true; }
#endif

    friend class MediaSourcePrivateAVFObjC;
    void bufferedChanged();

    enum class AcceleratedVideoMode: uint8_t {
        Layer = 0,
        StagedVideoRenderer,
        VideoRenderer,
        StagedLayer
    };

    AcceleratedVideoMode acceleratedVideoMode() const;
    bool canUseDecompressionSession() const;
    bool isUsingDecompressionSession() const;
    bool willUseDecompressionSessionIfNeeded() const;

    std::optional<SeekTarget> m_pendingSeek;

    ThreadSafeWeakPtr<MediaPlayer> m_player;
    WeakPtrFactory<MediaPlayerPrivateMediaSourceAVFObjC> m_sizeChangeObserverWeakPtrFactory;
    RefPtr<MediaSourcePrivateAVFObjC> m_mediaSourcePrivate;
    RetainPtr<AVAsset> m_asset;
    RefPtr<VideoMediaSampleRenderer> m_sampleBufferDisplayLayer;
    RefPtr<VideoMediaSampleRenderer> m_sampleBufferVideoRenderer;

    struct AudioRendererProperties {
        bool hasAudibleSample { false };
    };
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    UncheckedKeyHashMap<RetainPtr<CFTypeRef>, AudioRendererProperties> m_sampleBufferAudioRendererMap;
    RetainPtr<AVSampleBufferRenderSynchronizer> m_synchronizer;
ALLOW_NEW_API_WITHOUT_GUARDS_END
    mutable MediaPlayer::CurrentTimeDidChangeCallback m_currentTimeDidChangeCallback;
    RetainPtr<id> m_timeChangedObserver;
    RetainPtr<id> m_timeJumpedObserver;
    RetainPtr<id> m_gapObserver;
    RetainPtr<id> m_performTaskObserver;
    RetainPtr<CVPixelBufferRef> m_lastPixelBuffer;
    MediaTime m_lastPixelBufferPresentationTimeStamp;
    RefPtr<NativeImage> m_lastImage;
    std::unique_ptr<PixelBufferConformerCV> m_rgbConformer;
    Deque<RetainPtr<id>> m_sizeChangeObservers;
    Timer m_seekTimer;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    WeakPtr<CDMSessionAVContentKeySession> m_session;
#endif
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    bool m_readyStateIsWaitingForAvailableFrame { false };
    MediaTime m_duration { MediaTime::invalidTime() };
    MediaTime m_lastSeekTime;
    FloatSize m_naturalSize;
    double m_rate { 1 };
    bool m_isPlaying { false };
    bool m_isSynchronizerSeeking { false };
    SeekState m_seekState { SeekCompleted };
    mutable bool m_loadingProgressed { false };
    bool m_hasAvailableVideoFrame { false };
    bool m_allRenderersHaveAvailableSamples { false };
    bool m_visible { false };
    bool m_flushingActiveSourceBuffersDueToVisibilityChange { false };
    RetainPtr<CVOpenGLTextureRef> m_lastTexture;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
    bool m_shouldPlayToTarget { false };
#endif
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
    std::unique_ptr<VideoLayerManagerObjC> m_videoLayerManager;
    const Ref<EffectiveRateChangedListener> m_effectiveRateChangedListener;
    uint64_t m_sampleCount { 0 };
    RetainPtr<id> m_videoFrameMetadataGatheringObserver;
    bool m_isGatheringVideoFrameMetadata { false };
    std::optional<VideoFrameMetadata> m_videoFrameMetadata;
    uint64_t m_lastConvertedSampleCount { 0 };
    ProcessIdentity m_resourceOwner;
    bool m_shouldMaintainAspectRatio { true };
    bool m_needsPlaceholderImage { false };
    bool m_preferDecompressionSession { false };
    bool m_canFallbackToDecompressionSession { false };
    LoadOptions m_loadOptions;
#if HAVE(SPATIAL_TRACKING_LABEL)
    String m_defaultSpatialTrackingLabel;
    String m_spatialTrackingLabel;
#endif
#if ENABLE(LINEAR_MEDIA_PLAYER)
    bool m_usingLinearMediaPlayer { false };
    RetainPtr<FigVideoTargetRef> m_videoTarget;
#endif
};

String convertEnumerationToString(MediaPlayerPrivateMediaSourceAVFObjC::SeekState);

}

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::MediaPlayerPrivateMediaSourceAVFObjC::SeekState> {
    static String toString(const WebCore::MediaPlayerPrivateMediaSourceAVFObjC::SeekState state)
    {
        return convertEnumerationToString(state);
    }
};

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaPlayerPrivateMediaSourceAVFObjC)
static bool isType(const WebCore::MediaPlayerPrivateInterface& player) { return player.mediaPlayerType() == WebCore::MediaPlayerType::AVFObjCMSE; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
