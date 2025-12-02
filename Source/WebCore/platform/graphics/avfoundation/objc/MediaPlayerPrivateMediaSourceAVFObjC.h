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
#include "HTMLMediaElementIdentifier.h"
#include "MediaPlayerIdentifier.h"
#include "MediaPlayerPrivate.h"
#include "SourceBufferPrivateClient.h"
#include "VideoFrameMetadata.h"
#include <CoreMedia/CMTime.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/NativePromise.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVAsset;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);

typedef struct OpaqueCMTimebase* CMTimebaseRef;
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer *CVPixelBufferRef;
typedef struct __CVBuffer *CVOpenGLTextureRef;
typedef struct OpaqueFigVideoTarget *FigVideoTargetRef;

namespace WebCore {

class AudioTrackPrivate;
class AudioVideoRenderer;
class CDMSessionAVContentKeySession;
class EffectiveRateChangedListener;
class InbandTextTrackPrivate;
class MediaSourcePrivateAVFObjC;
class VideoFrameCV;
class VideoLayerManagerObjC;
class VideoMediaSampleRenderer;
class VideoTrackPrivate;

class MediaPlayerPrivateMediaSourceAVFObjC
    : public CanMakeWeakPtr<MediaPlayerPrivateMediaSourceAVFObjC>
    , public RefCounted<MediaPlayerPrivateMediaSourceAVFObjC>
    , public MediaPlayerPrivateInterface
    , private LoggerHelper
{
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    explicit MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer&);
    virtual ~MediaPlayerPrivateMediaSourceAVFObjC();

    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::AVFObjCMSE; }

    static void registerMediaEngine(MediaEngineRegistrar);

    // MediaPlayer Factory Methods
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters&);

    using TrackIdentifier = AudioVideoRenderer::TrackIdentifier;
    void addAudioTrack(TrackIdentifier);
    void removeAudioTrack(TrackIdentifier);

    void removeAudioTrack(AudioTrackPrivate&);
    void removeVideoTrack(VideoTrackPrivate&);
    void removeTextTrack(InbandTextTrackPrivate&);

    MediaPlayer::NetworkState networkState() const override;
    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

    void seekInternal();
    void startSeek(const MediaTime&);
    void cancelPendingSeek();
    void completeSeek(const MediaTime&);
    void setLoadingProgresssed(bool);
    void setHasAvailableVideoFrame(bool);
    bool hasAvailableVideoFrame() const override;
    void durationChanged();

    void effectiveRateChanged();
    void setNaturalSize(const FloatSize&);
    void characteristicsChanged();

    MediaTime currentTime() const override;
    MediaTime currentOrPendingSeekTime() const final { return currentTime(); }
    bool timeIsProgressing() const final;
    MediaTime clampTimeToSensicalValue(const MediaTime&) const;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    RetainPtr<PlatformLayer> createVideoFullscreenLayer() override;
    void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&& completionHandler) override;
    void setVideoFullscreenFrame(const FloatRect&) override;
#endif

    void setTextTrackRepresentation(TextTrackRepresentation*) override;
    void syncTextTrackBounds() override;

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void setCDMSession(LegacyCDMSession*) override;
    RefPtr<CDMSessionAVContentKeySession> cdmSession() const;
    void keyAdded() final;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&) final;
    void cdmInstanceDetached(CDMInstance&) final;
    void attemptToDecryptWithInstance(CDMInstance&) final;
    bool waitingForKey() const final;
    void waitingForKeyChanged();
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
    void keyNeeded(const SharedBuffer&);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void initializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&);
#endif

    const Vector<ContentType>& mediaContentTypesRequiringHardwareSupport() const;

    void needsVideoLayerChanged();

#if ENABLE(LINEAR_MEDIA_PLAYER)
    void setVideoTarget(const PlatformVideoTarget&) final;
    void maybeUpdateDisplayLayer();
#endif

#if PLATFORM(IOS_FAMILY)
    void sceneIdentifierDidChange() final;
    void applicationWillResignActive() final;
    void applicationDidBecomeActive() final;
#endif

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const override { return "MediaPlayerPrivateMediaSourceAVFObjC"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    uint64_t mediaPlayerLogIdentifier() { return logIdentifier(); }
    const Logger& mediaPlayerLogger() { return logger(); }
#endif

    bool supportsLimitedMatroska() const;

private:
    // MediaPlayerPrivateInterface
    void load(const String& url) override;
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) override;
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override;
#endif
    void cancelLoad() override;

    void prepareToPlay() override;
    PlatformLayer* platformLayer() const override;

    bool supportsPictureInPicture() const override { return true; }
    bool supportsFullscreen() const override { return true; }

    void play() override;
    void playInternal(std::optional<MonotonicTime>&& = std::nullopt);

    void pause() override;
    void pauseInternal(std::optional<MonotonicTime>&& = std::nullopt);

    bool paused() const override;

    void setVolume(float volume) override;
    void setMuted(bool) override;

    bool supportsScanning() const override;

    FloatSize naturalSize() const override;

    bool hasVideo() const override;
    bool hasAudio() const override;

    void setPageIsVisible(bool) final;

    MediaTime duration() const override;
    MediaTime startTime() const override;
    MediaTime initialTime() const override;

    void seekToTarget(const SeekTarget&) final;
    bool seeking() const final;
    void setRateDouble(double) override;
    double rate() const override;
    double effectiveRate() const override;

    void setPreservesPitch(bool) override;

    MediaTime maxTimeSeekable() const override;
    MediaTime minTimeSeekable() const override;
    const PlatformTimeRanges& buffered() const override;

    bool didLoadingProgress() const override;

    RefPtr<NativeImage> nativeImageForCurrentTime() override;
    bool updateLastVideoFrame();
    bool updateLastImage();
    void maybePurgeLastImage();
    void paint(GraphicsContext&, const FloatRect&) override;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) override;
    RefPtr<VideoFrame> videoFrameForCurrentTime() final;
    DestinationColorSpace colorSpace() final;

    bool supportsAcceleratedRendering() const override;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    void acceleratedRenderingStateChanged() override;
    void notifyActiveSourceBuffersChanged() override;

    void setPresentationSize(const IntSize&) final;

    Ref<AudioVideoRenderer> audioVideoRenderer() const;

    RefPtr<MediaSourcePrivateAVFObjC> protectedMediaSourcePrivate() const;

    // NOTE: Because the only way for MSE to recieve data is through an ArrayBuffer provided by
    // javascript running in the page, the video will, by necessity, always be CORS correct and
    // in the page's origin.
    bool didPassCORSAccessCheck() const override { return true; }

    MediaPlayer::MovieLoadType movieLoadType() const override;

    String engineDescription() const override;

    String languageOfPrimaryAudioTrack() const override;

    size_t extraMemoryCost() const override;

    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() override;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool isCurrentPlaybackTargetWireless() const override;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) override;
    void setShouldPlayToPlaybackTarget(bool) override;
    bool wirelessVideoPlaybackDisabled() const override { return false; }
#endif

    bool performTaskAtTime(Function<void(const MediaTime&)>&&, const MediaTime&) final;
    void audioOutputDeviceChanged() final;

    bool shouldBePlaying() const;
    bool setCurrentTimeDidChangeCallback(MediaPlayer::CurrentTimeDidChangeCallback&&) final;

    bool supportsPlayAtHostTime() const final { return true; }
    bool supportsPauseAtHostTime() const final { return true; }
    bool playAtHostTime(const MonotonicTime&) final;
    bool pauseAtHostTime(const MonotonicTime&) final;

    void startVideoFrameMetadataGathering() final;
    void stopVideoFrameMetadataGathering() final;
    std::optional<VideoFrameMetadata> videoFrameMetadata() final;

    void setResourceOwner(const ProcessIdentity&) final;

    void checkNewVideoFrameMetadata(MediaTime, double);

    void setShouldDisableHDR(bool) final;
    void setPlatformDynamicRangeLimit(PlatformDynamicRangeLimit) final;
    void playerContentBoxRectChanged(const LayoutRect&) final;
    void setShouldMaintainAspectRatio(bool) final;

#if HAVE(SPATIAL_TRACKING_LABEL)
    String defaultSpatialTrackingLabel() const final;
    void setDefaultSpatialTrackingLabel(const String&) final;
    String spatialTrackingLabel() const final;
    void setSpatialTrackingLabel(const String&) final;
    void updateSpatialTrackingLabel();
#endif

    void isInFullscreenOrPictureInPictureChanged(bool) final;

    void readyStateFromMediaSourceChanged() final;
    void updateStateFromReadyState();
    void mediaSourceHasRetrievedAllData() final;

    bool supportsProgressMonitoring() const final { return false; }

#if ENABLE(LINEAR_MEDIA_PLAYER)
    bool supportsLinearMediaPlayer() const final { return true; }
#endif

    friend class MediaSourcePrivateAVFObjC;
    void bufferedChanged();
    void stall();
    void timeChanged();

    void setLayerRequiresFlush();
    void flush();
    void flushVideoIfNeeded();
    void reenqueueMediaForTime(const MediaTime&);

    // Remote layer support
    WebCore::HostingContext hostingContext() const final;
    void setVideoLayerSizeFenced(const WebCore::FloatSize&, WTF::MachSendRightAnnotated&&) final;
    std::optional<MediaPlayerIdentifier> identifier() const final { return m_playerIdentifier; }

    static Ref<AudioVideoRenderer> createRenderer(LoggerHelper&, HTMLMediaElementIdentifier, MediaPlayerIdentifier);

    const ThreadSafeWeakPtr<MediaPlayer> m_player;
    RefPtr<MediaSourcePrivateAVFObjC> m_mediaSourcePrivate; // set on load, immutable after.

    struct AudioTrackProperties {
        bool hasAudibleSample { false };
    };
    HashMap<TrackIdentifier, AudioTrackProperties> m_audioTracksMap WTF_GUARDED_BY_CAPABILITY(mainThread);
    RefPtr<VideoFrame> m_lastVideoFrame WTF_GUARDED_BY_CAPABILITY(mainThread);
    RefPtr<NativeImage> m_lastImage WTF_GUARDED_BY_CAPABILITY(mainThread);

    // Seeking
    Timer m_seekTimer WTF_GUARDED_BY_CAPABILITY(mainThread);
    bool m_seeking  WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    std::optional<SeekTarget> m_pendingSeek WTF_GUARDED_BY_CAPABILITY(mainThread);
    const Ref<NativePromiseRequest> m_rendererSeekRequest WTF_GUARDED_BY_CAPABILITY(mainThread);

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    ThreadSafeWeakPtr<CDMSessionAVContentKeySession> m_session;
#endif
    MediaPlayer::NetworkState m_networkState WTF_GUARDED_BY_CAPABILITY(mainThread);
    MediaPlayer::ReadyState m_readyState WTF_GUARDED_BY_CAPABILITY(mainThread) { MediaPlayer::ReadyState::HaveNothing };
    bool m_readyStateIsWaitingForAvailableFrame WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    MediaTime m_duration WTF_GUARDED_BY_CAPABILITY(mainThread) { MediaTime::invalidTime() };
    MediaTime m_lastSeekTime WTF_GUARDED_BY_CAPABILITY(mainThread);
    FloatSize m_naturalSize; WTF_GUARDED_BY_CAPABILITY(mainThread)
    double m_rate WTF_GUARDED_BY_CAPABILITY(mainThread) { 1 };
    mutable bool m_loadingProgressed WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    bool m_hasAvailableVideoFrame WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    bool m_allRenderersHaveAvailableSamples WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    bool m_visible WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    RetainPtr<CVOpenGLTextureRef> m_lastTexture WTF_GUARDED_BY_CAPABILITY(mainThread);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    RefPtr<MediaPlaybackTarget> m_playbackTarget WTF_GUARDED_BY_CAPABILITY(mainThread);
    bool m_shouldPlayToTarget WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
#endif
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
    bool m_isGatheringVideoFrameMetadata WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    std::optional<VideoFrameMetadata> m_videoFrameMetadata WTF_GUARDED_BY_CAPABILITY(mainThread);
    uint64_t m_lastConvertedSampleCount WTF_GUARDED_BY_CAPABILITY(mainThread) { 0 };
    ProcessIdentity m_resourceOwner;
    LoadOptions m_loadOptions WTF_GUARDED_BY_CAPABILITY(mainThread);
#if HAVE(SPATIAL_TRACKING_LABEL)
    String m_defaultSpatialTrackingLabel;
    String m_spatialTrackingLabel;
#endif

    bool m_layerRequiresFlush WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
#if PLATFORM(IOS_FAMILY)
    bool m_applicationIsActive WTF_GUARDED_BY_CAPABILITY(mainThread) { true };
#endif

    const MediaPlayerIdentifier m_playerIdentifier;
    const Ref<AudioVideoRenderer> m_renderer;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaPlayerPrivateMediaSourceAVFObjC)
static bool isType(const WebCore::MediaPlayerPrivateInterface& player) { return player.mediaPlayerType() == WebCore::MediaPlayerType::AVFObjCMSE; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
