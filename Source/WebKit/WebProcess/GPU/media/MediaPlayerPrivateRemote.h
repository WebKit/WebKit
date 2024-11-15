/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "AudioTrackPrivateRemote.h"
#include "LayerHostingContext.h"
#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerManager.h"
#include "RemoteMediaPlayerState.h"
#include "RemoteMediaResourceIdentifier.h"
#include "RemoteMediaResourceProxy.h"
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#include "TextTrackPrivateRemote.h"
#include "VideoTrackPrivateRemote.h"
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/VideoFrameMetadata.h>
#include <wtf/Lock.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/StdUnorderedMap.h>

#if ENABLE(MEDIA_SOURCE)
#include "MediaSourcePrivateRemote.h"
#endif

namespace WTF {
class MachSendRight;
}

namespace WebCore {
struct GenericCueData;
class ISOWebVTTCue;
class SerializedPlatformDataCueValue;
class VideoLayerManager;
class VideoFrame;

#if PLATFORM(COCOA)
class PixelBufferConformerCV;
#endif
}

namespace WebKit {

class RemoteAudioSourceProvider;
class UserData;
struct AudioTrackPrivateRemoteConfiguration;
struct TextTrackPrivateRemoteConfiguration;
struct VideoTrackPrivateRemoteConfiguration;

struct MediaTimeUpdateData {
    MediaTime currentTime;
    bool timeIsProgressing;
    MonotonicTime wallTime;
};

class MediaPlayerPrivateRemote final
    : public WebCore::MediaPlayerPrivateInterface
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaPlayerPrivateRemote, WTF::DestructionThread::Main>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<MediaPlayerPrivateRemote> create(WebCore::MediaPlayer* player, WebCore::MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, WebCore::MediaPlayerIdentifier identifier, RemoteMediaPlayerManager& manager)
    {
        return adoptRef(*new MediaPlayerPrivateRemote(player, remoteEngineIdentifier, identifier, manager));
    }

    MediaPlayerPrivateRemote(WebCore::MediaPlayer*, WebCore::MediaPlayerEnums::MediaEngineIdentifier, WebCore::MediaPlayerIdentifier, RemoteMediaPlayerManager&);
    ~MediaPlayerPrivateRemote();

    constexpr WebCore::MediaPlayerType mediaPlayerType() const final { return WebCore::MediaPlayerType::Remote; }

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    WebCore::MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier() const { return m_remoteEngineIdentifier; }
    std::optional<WebCore::MediaPlayerIdentifier> identifier() const final { return m_id; }
    IPC::Connection& connection() const { return protectedManager()->gpuProcessConnection().connection(); }
    Ref<IPC::Connection> protectedConnection() const { return protectedManager()->gpuProcessConnection().protectedConnection(); }
    RefPtr<WebCore::MediaPlayer> player() const { return m_player.get(); }

    WebCore::MediaPlayer::ReadyState readyState() const final { return m_readyState; }
    void setReadyState(WebCore::MediaPlayer::ReadyState);

    void commitAllTransactions(CompletionHandler<void()>&&);
    void networkStateChanged(RemoteMediaPlayerState&&);
    void readyStateChanged(RemoteMediaPlayerState&&, WebCore::MediaPlayer::ReadyState);
    void volumeChanged(double);
    void muteChanged(bool);
    void seeked(MediaTimeUpdateData&&);
    void timeChanged(RemoteMediaPlayerState&&, MediaTimeUpdateData&&);
    void durationChanged(RemoteMediaPlayerState&&);
    void rateChanged(double, MediaTimeUpdateData&&);
    void playbackStateChanged(bool, MediaTimeUpdateData&&);
    void engineFailedToLoad(int64_t);
    void updateCachedState(RemoteMediaPlayerState&&);
    void updatePlaybackQualityMetrics(WebCore::VideoPlaybackQualityMetrics&&);
    void characteristicChanged(RemoteMediaPlayerState&&);
    void sizeChanged(WebCore::FloatSize);
    void firstVideoFrameAvailable();
    void renderingModeChanged();
#if PLATFORM(COCOA)
    void layerHostingContextIdChanged(std::optional<WebKit::LayerHostingContextID>&&, const WebCore::FloatSize&);
    WebCore::FloatSize videoLayerSize() const final;
    void setVideoLayerSizeFenced(const WebCore::FloatSize&, WTF::MachSendRight&&) final;
#endif

    void currentTimeChanged(MediaTimeUpdateData&&);

    void addRemoteAudioTrack(AudioTrackPrivateRemoteConfiguration&&);
    void removeRemoteAudioTrack(WebCore::TrackID);
    void remoteAudioTrackConfigurationChanged(WebCore::TrackID, AudioTrackPrivateRemoteConfiguration&&);

    void addRemoteVideoTrack(VideoTrackPrivateRemoteConfiguration&&);
    void removeRemoteVideoTrack(WebCore::TrackID);
    void remoteVideoTrackConfigurationChanged(WebCore::TrackID, VideoTrackPrivateRemoteConfiguration&&);

    void addRemoteTextTrack(TextTrackPrivateRemoteConfiguration&&);
    void removeRemoteTextTrack(WebCore::TrackID);
    void remoteTextTrackConfigurationChanged(WebCore::TrackID, TextTrackPrivateRemoteConfiguration&&);

    void parseWebVTTFileHeader(WebCore::TrackID, String&&);
    void parseWebVTTCueData(WebCore::TrackID, std::span<const uint8_t>);
    void parseWebVTTCueDataStruct(WebCore::TrackID, WebCore::ISOWebVTTCue&&);

    void addDataCue(WebCore::TrackID, MediaTime&& start, MediaTime&& end, std::span<const uint8_t>);
#if ENABLE(DATACUE_VALUE)
    void addDataCueWithType(WebCore::TrackID, MediaTime&& start, MediaTime&& end, WebCore::SerializedPlatformDataCueValue&&, String&&);
    void updateDataCue(WebCore::TrackID, MediaTime&& start, MediaTime&& end, WebCore::SerializedPlatformDataCueValue&&);
    void removeDataCue(WebCore::TrackID, MediaTime&& start, MediaTime&& end, WebCore::SerializedPlatformDataCueValue&&);
#endif

    void addGenericCue(WebCore::TrackID, WebCore::GenericCueData&&);
    void updateGenericCue(WebCore::TrackID, WebCore::GenericCueData&&);
    void removeGenericCue(WebCore::TrackID, WebCore::GenericCueData&&);

    void requestResource(RemoteMediaResourceIdentifier, WebCore::ResourceRequest&&, WebCore::PlatformMediaResourceLoader::LoadOptions);
    void removeResource(RemoteMediaResourceIdentifier);
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&);
    void resourceNotSupported();

    void activeSourceBuffersChanged();

#if PLATFORM(COCOA)
    bool inVideoFullscreenOrPictureInPicture() const;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void waitingForKeyChanged(bool);
    void initializationDataEncountered(const String&, std::span<const uint8_t>);
#endif

#if ENABLE(MEDIA_SOURCE)
    RefPtr<AudioTrackPrivateRemote> audioTrackPrivateRemote(WebCore::TrackID) const;
    RefPtr<VideoTrackPrivateRemote> videoTrackPrivateRemote(WebCore::TrackID) const;
    RefPtr<TextTrackPrivateRemote> textTrackPrivateRemote(WebCore::TrackID) const;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void currentPlaybackTargetIsWirelessChanged(bool);
#endif

#if PLATFORM(IOS_FAMILY)
    void getRawCookies(const URL&, WebCore::MediaPlayerClient::GetRawCookiesCallback&&) const;
#endif

    WebCore::FloatSize naturalSize() const final;

#if !RELEASE_LOG_DISABLED
    uint64_t mediaPlayerLogIdentifier() const { return logIdentifier(); }
    const Logger& mediaPlayerLogger() const { return logger(); }
#endif

    void requestHostingContextID(LayerHostingContextIDCallback&&) override;
    LayerHostingContextID hostingContextID()const override;
    void setLayerHostingContextID(LayerHostingContextID  inID);

    MediaTime duration() const final;
    MediaTime currentTime() const final;
    MediaTime currentOrPendingSeekTime() const final;

private:
    class TimeProgressEstimator final {
    public:
        explicit TimeProgressEstimator(const MediaPlayerPrivateRemote& parent);
        MediaTime currentTime() const;
        MediaTime cachedTime() const;
        bool timeIsProgressing() const;
        void pause();
        void setTime(const MediaTimeUpdateData&);
        void setRate(double);
        Lock& lock() const { return m_lock; };
        MediaTime currentTimeWithLockHeld() const;
        MediaTime cachedTimeWithLockHeld() const;

    private:
        Ref<const MediaPlayerPrivateRemote> protectedParent() const { return m_parent.get().releaseNonNull(); }

        mutable Lock m_lock;
        std::atomic<bool> m_timeIsProgressing { false };
        MediaTime m_cachedMediaTime WTF_GUARDED_BY_LOCK(m_lock);
        MonotonicTime m_cachedMediaTimeQueryTime WTF_GUARDED_BY_LOCK(m_lock);
        double m_rate WTF_GUARDED_BY_LOCK(m_lock) { 1.0 };
        ThreadSafeWeakPtr<const MediaPlayerPrivateRemote> m_parent; // Cannot be null.
    };
    TimeProgressEstimator m_currentTimeEstimator;

    MediaTime currentTimeWithLockHeld() const;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    ASCIILiteral logClassName() const override { return "MediaPlayerPrivateRemote"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif

    void load(const URL&, const WebCore::ContentType&, const String&) final;
    void prepareForPlayback(bool privateMode, WebCore::MediaPlayer::Preload, bool preservesPitch, bool prepareToPlay, bool prepareToRender) final;

#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const WebCore::ContentType&, WebCore::MediaSourcePrivateClient&) final;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(WebCore::MediaStreamPrivate&) final;
#endif
    void cancelLoad() final;

    void play() final;
    void pause() final;

    void prepareToPlay() final;
    long platformErrorCode() const final { return m_platformErrorCode; }

    double rate() const final { return m_rate; }
    void setVolumeDouble(double) final;
    void setMuted(bool) final;
    void setPrivateBrowsingMode(bool) final;
    void setPreservesPitch(bool) final;
    void setPitchCorrectionAlgorithm(WebCore::MediaPlayer::PitchCorrectionAlgorithm) final;

    bool shouldIgnoreIntrinsicSize() final;

    PlatformLayer* platformLayer() const final;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    PlatformLayerContainer createVideoFullscreenLayer() final;
    void setVideoFullscreenLayer(PlatformLayer*, WTF::Function<void()>&& completionHandler) final;
    void updateVideoFullscreenInlineImage() final;
    void setVideoFullscreenFrame(WebCore::FloatRect) final;
    void setVideoFullscreenGravity(WebCore::MediaPlayer::VideoGravity) final;
    void setVideoFullscreenMode(WebCore::MediaPlayer::VideoFullscreenMode) final;
    void videoFullscreenStandbyChanged() final;
#endif

#if PLATFORM(IOS_FAMILY)
    NSArray *timedMetadata() const final;
    String accessLog() const final;
    String errorLog() const final;
#endif

    void setBufferingPolicy(WebCore::MediaPlayer::BufferingPolicy) final;

    bool supportsPictureInPicture() const final;
    bool supportsFullscreen() const final;
    bool supportsScanning() const final;

    bool canSaveMediaData() const final;

    bool hasVideo() const final;
    bool hasAudio() const final;

    void setPageIsVisible(bool) final;

    MediaTime getStartDate() const final;

    void willSeekToTarget(const MediaTime&) final;
    MediaTime pendingSeekTime() const final;
    void seekToTarget(const WebCore::SeekTarget&) final;
    bool seeking() const final;

    MediaTime startTime() const final;

    void setRateDouble(double) final;

    bool paused() const final { return m_cachedState.paused; }
    bool timeIsProgressing() const final;

#if PLATFORM(IOS_FAMILY) || USE(GSTREAMER)
    float volume() const final { return 1; }
#endif

    bool hasClosedCaptions() const final;

    double maxFastForwardRate() const final;
    double minFastReverseRate() const final;

    WebCore::MediaPlayer::NetworkState networkState() const final { return m_cachedState.networkState; }

    MediaTime maxTimeSeekable() const final;
    MediaTime minTimeSeekable() const final;
    const WebCore::PlatformTimeRanges& buffered() const final;
    double seekableTimeRangesLastModifiedTime() const final;
    double liveUpdateInterval() const final;

    unsigned long long totalBytes() const final;
    bool didLoadingProgress() const final;
    void didLoadingProgressAsync(WebCore::MediaPlayer::DidLoadingProgressCompletionHandler&&) const final;

    void setPresentationSize(const WebCore::IntSize&) final;

    void paint(WebCore::GraphicsContext&, const WebCore::FloatRect&) final;
    void paintCurrentFrameInContext(WebCore::GraphicsContext&, const WebCore::FloatRect&) final;
    RefPtr<WebCore::VideoFrame> videoFrameForCurrentTime() final;
    RefPtr<WebCore::NativeImage> nativeImageForCurrentTime() final;
    WebCore::DestinationColorSpace colorSpace() final;
#if PLATFORM(COCOA)
    bool shouldGetNativeImageForCanvasDrawing() const final { return false; }
#endif

    void setPreload(WebCore::MediaPlayer::Preload) final;

    bool hasAvailableVideoFrame() const final;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    String wirelessPlaybackTargetName() const final;
    WebCore::MediaPlayer::WirelessPlaybackTargetType wirelessPlaybackTargetType() const final;

    bool wirelessVideoPlaybackDisabled() const final;
    void setWirelessVideoPlaybackDisabled(bool) final;

    bool canPlayToWirelessPlaybackTarget() const final;
    bool isCurrentPlaybackTargetWireless() const final;
    void setWirelessPlaybackTarget(Ref<WebCore::MediaPlaybackTarget>&&) final;

    void setShouldPlayToPlaybackTarget(bool) final;
#endif

    bool supportsAcceleratedRendering() const final;
    void acceleratedRenderingStateChanged() final;

    void setShouldMaintainAspectRatio(bool) final;

    bool didPassCORSAccessCheck() const final;
    std::optional<bool> isCrossOrigin(const WebCore::SecurityOrigin&) const final;

    WebCore::MediaPlayer::MovieLoadType movieLoadType() const final;

    void prepareForRendering() final;

    MediaTime mediaTimeForTimeValue(const MediaTime& timeValue) const final;

    double maximumDurationToCacheMediaTime() const final;

    unsigned decodedFrameCount() const final;
    unsigned droppedFrameCount() const final;
    unsigned audioDecodedByteCount() const final;
    unsigned videoDecodedByteCount() const final;

    String engineDescription() const final;

#if ENABLE(WEB_AUDIO)
    WebCore::AudioSourceProvider* audioSourceProvider() final;
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<WebCore::LegacyCDMSession> createSession(const String&, WebCore::LegacyCDMSessionClient&) final;
    void setCDM(WebCore::LegacyCDM*) final;
    void setCDMSession(WebCore::LegacyCDMSession*) final;
    void keyAdded() final;
    void mediaPlayerKeyNeeded(std::span<const uint8_t>);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(WebCore::CDMInstance&) final;
    void cdmInstanceDetached(WebCore::CDMInstance&) final;
    void attemptToDecryptWithInstance(WebCore::CDMInstance&) final;
    bool waitingForKey() const final;
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
    void setShouldContinueAfterKeyNeeded(bool) final;
#endif

    void setTextTrackRepresentation(WebCore::TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;

    void tracksChanged() final;

    void beginSimulatedHDCPError() final;
    void endSimulatedHDCPError() final;

    String languageOfPrimaryAudioTrack() const final;

    size_t extraMemoryCost() const final;

    std::optional<WebCore::VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;
    void updateVideoPlaybackMetricsUpdateInterval(const Seconds&);

    void notifyTrackModeChanged() final;

    void notifyActiveSourceBuffersChanged() final;

    void setShouldDisableSleep(bool) final;

    void applicationWillResignActive() final;
    void applicationDidBecomeActive() final;
    void setPreferredDynamicRangeMode(WebCore::DynamicRangeMode) final;

#if USE(AVFOUNDATION)
    AVPlayer *objCAVFoundationAVPlayer() const final { return nullptr; }
#endif

    bool performTaskAtTime(Function<void()>&&, const MediaTime&) final;

    bool supportsPlayAtHostTime() const final { return m_configuration.supportsPlayAtHostTime; }
    bool supportsPauseAtHostTime() const final { return m_configuration.supportsPauseAtHostTime; }
    bool playAtHostTime(const MonotonicTime&) final;
    bool pauseAtHostTime(const MonotonicTime&) final;
    void updateConfiguration(RemoteMediaPlayerConfiguration&&);

    std::optional<WebCore::VideoFrameMetadata> videoFrameMetadata() final;
    void startVideoFrameMetadataGathering() final;
    void stopVideoFrameMetadataGathering() final;

    void playerContentBoxRectChanged(const WebCore::LayoutRect&) final;

    void setShouldDisableHDR(bool) final;

    void setShouldCheckHardwareSupport(bool) final;

#if HAVE(SPATIAL_TRACKING_LABEL)
    const String& defaultSpatialTrackingLabel() const final;
    void setDefaultSpatialTrackingLabel(const String&) final;
    const String& spatialTrackingLabel() const final;
    void setSpatialTrackingLabel(const String&) final;
#endif

    void isInFullscreenOrPictureInPictureChanged(bool) final;

#if ENABLE(LINEAR_MEDIA_PLAYER)
    bool supportsLinearMediaPlayer() const final;
#endif

    void audioOutputDeviceChanged() final;

#if PLATFORM(COCOA)
    void pushVideoFrameMetadata(WebCore::VideoFrameMetadata&&, RemoteVideoFrameProxy::Properties&&);
#endif
    RemoteVideoFrameObjectHeapProxy& videoFrameObjectHeapProxy() const { return protectedManager()->gpuProcessConnection().videoFrameObjectHeapProxy(); }

    Ref<RemoteMediaPlayerManager> protectedManager() const;

    ThreadSafeWeakPtr<WebCore::MediaPlayer> m_player;
#if PLATFORM(COCOA)
    mutable UniqueRef<WebCore::VideoLayerManager> m_videoLayerManager;
#endif
    mutable PlatformLayerContainer m_videoLayer;

    ThreadSafeWeakPtr<RemoteMediaPlayerManager> m_manager; // Cannot be null.
    WebCore::MediaPlayerEnums::MediaEngineIdentifier m_remoteEngineIdentifier;
    WebCore::MediaPlayerIdentifier m_id;
    RemoteMediaPlayerConfiguration m_configuration;

    RemoteMediaPlayerState m_cachedState;
    WebCore::PlatformTimeRanges m_cachedBufferedTimeRanges;

#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
    RefPtr<RemoteAudioSourceProvider> m_audioSourceProvider;
#endif

#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSourcePrivateRemote> m_mediaSourcePrivate;
#endif

    mutable Lock m_lock;
    HashMap<RemoteMediaResourceIdentifier, RefPtr<WebCore::PlatformMediaResource>> m_mediaResources;
    StdUnorderedMap<WebCore::TrackID, Ref<AudioTrackPrivateRemote>> m_audioTracks WTF_GUARDED_BY_LOCK(m_lock);
    StdUnorderedMap<WebCore::TrackID, Ref<VideoTrackPrivateRemote>> m_videoTracks WTF_GUARDED_BY_LOCK(m_lock);
    StdUnorderedMap<WebCore::TrackID, Ref<TextTrackPrivateRemote>> m_textTracks WTF_GUARDED_BY_LOCK(m_lock);

    WebCore::SecurityOriginData m_documentSecurityOrigin;
    mutable HashMap<WebCore::SecurityOriginData, std::optional<bool>> m_isCrossOriginCache;

    WebCore::MediaPlayer::VideoGravity m_videoFullscreenGravity { WebCore::MediaPlayer::VideoGravity::ResizeAspect };
    MonotonicTime m_lastPlaybackQualityMetricsQueryTime;
    Seconds m_videoPlaybackMetricsUpdateInterval;
    WebCore::MediaPlayerEnums::ReadyState m_readyState { WebCore::MediaPlayerEnums::ReadyState::HaveNothing };
    double m_volume { 1 };
    double m_rate { 1 };
    long m_platformErrorCode { 0 };
    bool m_muted { false };
    std::atomic<bool> m_seeking { false };
    bool m_isCurrentPlaybackTargetWireless { false };
    bool m_waitingForKey { false };
    std::optional<bool> m_shouldMaintainAspectRatio;
    std::optional<bool> m_pageIsVisible;
    RefPtr<RemoteVideoFrameProxy> m_videoFrameForCurrentTime;
#if PLATFORM(COCOA)
    RefPtr<RemoteVideoFrameProxy> m_videoFrameGatheredWithVideoFrameMetadata;
#endif

    Vector<LayerHostingContextIDCallback> m_layerHostingContextIDRequests;
    LayerHostingContextID m_layerHostingContextID { 0 };
    std::optional<WebCore::VideoFrameMetadata> m_videoFrameMetadata;
    bool m_isGatheringVideoFrameMetadata { false };
    String m_defaultSpatialTrackingLabel;
    String m_spatialTrackingLabel;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::MediaPlayerPrivateRemote)
static bool isType(const WebCore::MediaPlayerPrivateInterface& player) { return player.mediaPlayerType() == WebCore::MediaPlayerType::Remote; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
