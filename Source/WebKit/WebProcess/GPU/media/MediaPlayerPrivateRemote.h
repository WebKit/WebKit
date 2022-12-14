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

#if ENABLE(GPU_PROCESS)

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
#include "TrackPrivateRemoteIdentifier.h"
#include "VideoTrackPrivateRemote.h"
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/VideoFrame.h>
#include <WebCore/VideoFrameMetadata.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/WeakPtr.h>

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

class MediaPlayerPrivateRemote final
    : public WebCore::MediaPlayerPrivateInterface
    , public IPC::MessageReceiver
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static std::unique_ptr<MediaPlayerPrivateRemote> create(WebCore::MediaPlayer* player, WebCore::MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, WebCore::MediaPlayerIdentifier identifier, RemoteMediaPlayerManager& manager)
    {
        return makeUnique<MediaPlayerPrivateRemote>(player, remoteEngineIdentifier, identifier, manager);
    }

    MediaPlayerPrivateRemote(WebCore::MediaPlayer*, WebCore::MediaPlayerEnums::MediaEngineIdentifier, WebCore::MediaPlayerIdentifier, RemoteMediaPlayerManager&);
    ~MediaPlayerPrivateRemote();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    WebCore::MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier() const { return m_remoteEngineIdentifier; }
    WebCore::MediaPlayerIdentifier identifier() const final { return m_id; }
    IPC::Connection& connection() const { return m_manager.gpuProcessConnection().connection(); }
    WebCore::MediaPlayer* player() const { return m_player.get(); }

    WebCore::MediaPlayer::ReadyState readyState() const final { return m_cachedState.readyState; }
    void setReadyState(WebCore::MediaPlayer::ReadyState);

    void networkStateChanged(RemoteMediaPlayerState&&);
    void readyStateChanged(RemoteMediaPlayerState&&);
    void volumeChanged(double);
    void muteChanged(bool);
    void timeChanged(RemoteMediaPlayerState&&);
    void durationChanged(RemoteMediaPlayerState&&);
    void rateChanged(double);
    void playbackStateChanged(bool, MediaTime&&, MonotonicTime&&);
    void engineFailedToLoad(int64_t);
    void updateCachedState(RemoteMediaPlayerState&&);
    void characteristicChanged(RemoteMediaPlayerState&&);
    void sizeChanged(WebCore::FloatSize);
    void firstVideoFrameAvailable();
    void renderingModeChanged();
#if PLATFORM(COCOA)
    void layerHostingContextIdChanged(std::optional<WebKit::LayerHostingContextID>&&, const WebCore::IntSize&);
    void setVideoInlineSizeFenced(const WebCore::FloatSize&, const WTF::MachSendRight&);
#endif

    void currentTimeChanged(const MediaTime&, const MonotonicTime&, bool);

    void addRemoteAudioTrack(TrackPrivateRemoteIdentifier, AudioTrackPrivateRemoteConfiguration&&);
    void removeRemoteAudioTrack(TrackPrivateRemoteIdentifier);
    void remoteAudioTrackConfigurationChanged(TrackPrivateRemoteIdentifier, AudioTrackPrivateRemoteConfiguration&&);

    void addRemoteVideoTrack(TrackPrivateRemoteIdentifier, VideoTrackPrivateRemoteConfiguration&&);
    void removeRemoteVideoTrack(TrackPrivateRemoteIdentifier);
    void remoteVideoTrackConfigurationChanged(TrackPrivateRemoteIdentifier, VideoTrackPrivateRemoteConfiguration&&);

    void addRemoteTextTrack(TrackPrivateRemoteIdentifier, TextTrackPrivateRemoteConfiguration&&);
    void removeRemoteTextTrack(TrackPrivateRemoteIdentifier);
    void remoteTextTrackConfigurationChanged(TrackPrivateRemoteIdentifier, TextTrackPrivateRemoteConfiguration&&);

    void parseWebVTTFileHeader(TrackPrivateRemoteIdentifier, String&&);
    void parseWebVTTCueData(TrackPrivateRemoteIdentifier, IPC::DataReference&&);
    void parseWebVTTCueDataStruct(TrackPrivateRemoteIdentifier, WebCore::ISOWebVTTCue&&);

    void addDataCue(TrackPrivateRemoteIdentifier, MediaTime&& start, MediaTime&& end, IPC::DataReference&&);
#if ENABLE(DATACUE_VALUE)
    void addDataCueWithType(TrackPrivateRemoteIdentifier, MediaTime&& start, MediaTime&& end, WebCore::SerializedPlatformDataCueValue&&, String&&);
    void updateDataCue(TrackPrivateRemoteIdentifier, MediaTime&& start, MediaTime&& end, WebCore::SerializedPlatformDataCueValue&&);
    void removeDataCue(TrackPrivateRemoteIdentifier, MediaTime&& start, MediaTime&& end, WebCore::SerializedPlatformDataCueValue&&);
#endif

    void addGenericCue(TrackPrivateRemoteIdentifier, WebCore::GenericCueData&&);
    void updateGenericCue(TrackPrivateRemoteIdentifier, WebCore::GenericCueData&&);
    void removeGenericCue(TrackPrivateRemoteIdentifier, WebCore::GenericCueData&&);

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
    void initializationDataEncountered(const String&, IPC::DataReference&&);
#endif

#if ENABLE(MEDIA_SOURCE)
    RefPtr<AudioTrackPrivateRemote> audioTrackPrivateRemote(TrackPrivateRemoteIdentifier identifier) const { return m_audioTracks.get(identifier); }
    RefPtr<VideoTrackPrivateRemote> videoTrackPrivateRemote(TrackPrivateRemoteIdentifier identifier) const { return m_videoTracks.get(identifier); }
    RefPtr<TextTrackPrivateRemote> textTrackPrivateRemote(TrackPrivateRemoteIdentifier identifier) const { return m_textTracks.get(identifier); }
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void currentPlaybackTargetIsWirelessChanged(bool);
#endif

#if PLATFORM(IOS_FAMILY)
    void getRawCookies(const URL&, WebCore::MediaPlayerClient::GetRawCookiesCallback&&) const;
#endif

    WebCore::FloatSize naturalSize() const final;

#if !RELEASE_LOG_DISABLED
    const void* mediaPlayerLogIdentifier() { return logIdentifier(); }
    const Logger& mediaPlayerLogger() { return logger(); }
#endif

private:

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    const char* logClassName() const override { return "MediaPlayerPrivateRemote"; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;

    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    void load(const URL&, const WebCore::ContentType&, const String&) final;
    void prepareForPlayback(bool privateMode, WebCore::MediaPlayer::Preload, bool preservesPitch, bool prepare) final;

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

    MediaTime durationMediaTime() const final;
    MediaTime currentMediaTime() const final;

    MediaTime getStartDate() const final;

    void seek(const MediaTime&) final;
    void seekWithTolerance(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance) final;

    bool seeking() const final { return m_seeking; }

    MediaTime startTime() const final;

    void setRateDouble(double) final;

    bool paused() const final { return m_cachedState.paused; }

#if PLATFORM(IOS_FAMILY) || USE(GSTREAMER)
    float volume() const final { return 1; }
#endif

    bool hasClosedCaptions() const final;

    double maxFastForwardRate() const final;
    double minFastReverseRate() const final;

    WebCore::MediaPlayer::NetworkState networkState() const final { return m_cachedState.networkState; }

    MediaTime maxMediaTimeSeekable() const final;
    MediaTime minMediaTimeSeekable() const final;
    std::unique_ptr<WebCore::PlatformTimeRanges> buffered() const final;
    double seekableTimeRangesLastModifiedTime() const final;
    double liveUpdateInterval() const final;

    unsigned long long totalBytes() const final;
    bool didLoadingProgress() const final;
    void didLoadingProgressAsync(WebCore::MediaPlayer::DidLoadingProgressCompletionHandler&&) const final;

    void setPresentationSize(const WebCore::IntSize&) final;

    void paint(WebCore::GraphicsContext&, const WebCore::FloatRect&) final;
    void paintCurrentFrameInContext(WebCore::GraphicsContext&, const WebCore::FloatRect&) final;
#if !USE(AVFOUNDATION)
    bool copyVideoTextureToPlatformTexture(WebCore::GraphicsContextGL*, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool) final;
#endif
#if PLATFORM(COCOA) && !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    void willBeAskedToPaintGL() final;
#endif
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
    void checkAcceleratedRenderingState();

    void setShouldMaintainAspectRatio(bool) final;

    bool hasSingleSecurityOrigin() const final;
    bool didPassCORSAccessCheck() const final;
    std::optional<bool> wouldTaintOrigin(const WebCore::SecurityOrigin&) const final;

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
    std::unique_ptr<WebCore::LegacyCDMSession> createSession(const String&, WebCore::LegacyCDMSessionClient&) final;
    void setCDM(WebCore::LegacyCDM*) final;
    void setCDMSession(WebCore::LegacyCDMSession*) final;
    void keyAdded() final;
    void mediaPlayerKeyNeeded(IPC::DataReference&&);
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

    bool requiresTextTrackRepresentation() const final;
    void setTextTrackRepresentation(WebCore::TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;

    void tracksChanged() final;

    void beginSimulatedHDCPError() final;
    void endSimulatedHDCPError() final;

    String languageOfPrimaryAudioTrack() const final;

    size_t extraMemoryCost() const final;

    std::optional<WebCore::VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;
    void updateVideoPlaybackMetricsUpdateInterval(const Seconds&);

#if ENABLE(AVF_CAPTIONS)
    void notifyTrackModeChanged() final;
#endif

    void notifyActiveSourceBuffersChanged() final;

    void setShouldDisableSleep(bool) final;

    void applicationWillResignActive() final;
    void applicationDidBecomeActive() final;
    void setPreferredDynamicRangeMode(WebCore::DynamicRangeMode) final;

#if USE(AVFOUNDATION)
    AVPlayer *objCAVFoundationAVPlayer() const final { return nullptr; }
#endif

    bool performTaskAtMediaTime(Function<void()>&&, const MediaTime&) final;

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

#if PLATFORM(COCOA)
    void pushVideoFrameMetadata(WebCore::VideoFrameMetadata&&, RemoteVideoFrameProxy::Properties&&);
#endif
    RemoteVideoFrameObjectHeapProxy& videoFrameObjectHeapProxy() const { return m_manager.gpuProcessConnection().videoFrameObjectHeapProxy(); }

    WeakPtr<WebCore::MediaPlayer> m_player;
    Ref<WebCore::PlatformMediaResourceLoader> m_mediaResourceLoader;
#if PLATFORM(COCOA)
    UniqueRef<WebCore::VideoLayerManager> m_videoLayerManager;
#endif
    PlatformLayerContainer m_videoLayer;

    RemoteMediaPlayerManager& m_manager;
    WebCore::MediaPlayerEnums::MediaEngineIdentifier m_remoteEngineIdentifier;
    WebCore::MediaPlayerIdentifier m_id;
    RemoteMediaPlayerConfiguration m_configuration;

    RemoteMediaPlayerState m_cachedState;
    std::unique_ptr<WebCore::PlatformTimeRanges> m_cachedBufferedTimeRanges;

#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
    RefPtr<RemoteAudioSourceProvider> m_audioSourceProvider;
#endif

#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSourcePrivateRemote> m_mediaSourcePrivate;
#endif

    HashMap<RemoteMediaResourceIdentifier, RefPtr<WebCore::PlatformMediaResource>> m_mediaResources;
    HashMap<TrackPrivateRemoteIdentifier, Ref<AudioTrackPrivateRemote>> m_audioTracks;
    HashMap<TrackPrivateRemoteIdentifier, Ref<VideoTrackPrivateRemote>> m_videoTracks;
    HashMap<TrackPrivateRemoteIdentifier, Ref<TextTrackPrivateRemote>> m_textTracks;

    WebCore::SecurityOriginData m_documentSecurityOrigin;
    mutable HashMap<WebCore::SecurityOriginData, std::optional<bool>> m_wouldTaintOriginCache;

    MediaTime m_cachedMediaTime;
    MonotonicTime m_cachedMediaTimeQueryTime;

    WebCore::MediaPlayer::VideoGravity m_videoFullscreenGravity { WebCore::MediaPlayer::VideoGravity::ResizeAspect };
    MonotonicTime m_lastPlaybackQualityMetricsQueryTime;
    Seconds m_videoPlaybackMetricsUpdateInterval;
    double m_volume { 1 };
    double m_rate { 1 };
    long m_platformErrorCode { 0 };
    bool m_muted { false };
    bool m_seeking { false };
    bool m_isCurrentPlaybackTargetWireless { false };
    bool m_waitingForKey { false };
    bool m_timeIsProgressing { false };
    bool m_renderingCanBeAccelerated { false };
    std::optional<bool> m_shouldMaintainAspectRatio;
    std::optional<bool> m_pageIsVisible;
    RefPtr<RemoteVideoFrameProxy> m_videoFrameForCurrentTime;
#if PLATFORM(COCOA)
    RefPtr<RemoteVideoFrameProxy> m_videoFrameGatheredWithVideoFrameMetadata;
#endif
    std::optional<WebCore::VideoFrameMetadata> m_videoFrameMetadata;
    bool m_isGatheringVideoFrameMetadata { false };
#if PLATFORM(COCOA) && !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    bool m_hasBeenAskedToPaintGL { false };
#endif
};

} // namespace WebKit

#endif
