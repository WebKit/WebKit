/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerManager.h"
#include "RemoteMediaPlayerState.h"
#include <WebCore/MediaPlayerPrivate.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class MediaPlayerPrivateRemote final
    : public CanMakeWeakPtr<MediaPlayerPrivateRemote>
    , public MediaPlayerPrivateInterface
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static std::unique_ptr<MediaPlayerPrivateRemote> create(MediaPlayer* player, MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, MediaPlayerPrivateRemoteIdentifier identifier, RemoteMediaPlayerManager& manager, const RemoteMediaPlayerConfiguration& configuration)
    {
        return makeUnique<MediaPlayerPrivateRemote>(player, remoteEngineIdentifier, identifier, manager, configuration);
    }

    explicit MediaPlayerPrivateRemote(MediaPlayer*, MediaPlayerEnums::MediaEngineIdentifier, MediaPlayerPrivateRemoteIdentifier, RemoteMediaPlayerManager&, const RemoteMediaPlayerConfiguration&);
    virtual ~MediaPlayerPrivateRemote();

    void invalidate() { m_invalid = true; }
    MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier() const { return m_remoteEngineIdentifier; }
    MediaPlayerPrivateRemoteIdentifier playerItentifier() const { return m_id; }

    void networkStateChanged(RemoteMediaPlayerState&&);
    void readyStateChanged(RemoteMediaPlayerState&&);
    void volumeChanged(double);
    void muteChanged(bool);
    void timeChanged(RemoteMediaPlayerState&&);
    void durationChanged(RemoteMediaPlayerState&&);
    void rateChanged(double);
    void playbackStateChanged(bool);
    void engineFailedToLoad(long);
    void updateCachedState(RemoteMediaPlayerState&&);
    void characteristicChanged(bool hasAudio, bool hasVideo, WebCore::MediaPlayerEnums::MovieLoadType);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return *m_logger; }
    const char* logClassName() const override { return "MediaPlayerPrivateRemote"; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;
#endif

private:
    void load(const URL&, const ContentType&, const String&) final;
    void prepareForPlayback(bool privateMode, MediaPlayer::Preload, bool preservesPitch, bool prepare) final;

#if ENABLE(MEDIA_SOURCE)
    void load(const String&, MediaSourcePrivateClient*) final;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final;
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

    // Unimplemented
    PlatformLayer* platformLayer() const final;

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    void setVideoFullscreenLayer(PlatformLayer*, WTF::Function<void()>&& completionHandler) final;
    void updateVideoFullscreenInlineImage() final;
    void setVideoFullscreenFrame(FloatRect) final;
    void setVideoFullscreenGravity(MediaPlayer::VideoGravity) final;
    void setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode) final;
    void videoFullscreenStandbyChanged() final;
#endif

#if PLATFORM(IOS_FAMILY)
    NSArray *timedMetadata() const final;
    String accessLog() const final;
    String errorLog() const final;
#endif

    void setBufferingPolicy(MediaPlayer::BufferingPolicy) final;

    bool supportsPictureInPicture() const final;
    bool supportsFullscreen() const final;
    bool supportsScanning() const final;

    bool canSaveMediaData() const final;

    FloatSize naturalSize() const final;

    bool hasVideo() const final;
    bool hasAudio() const final;

    void setVisible(bool) final;

    MediaTime durationMediaTime() const final { return m_cachedState.duration; }
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
    void setClosedCaptionsVisible(bool) final;

    double maxFastForwardRate() const final;
    double minFastReverseRate() const final;

    MediaPlayer::NetworkState networkState() const final { return m_cachedState.networkState; }
    MediaPlayer::ReadyState readyState() const final { return m_cachedState.readyState; }

    std::unique_ptr<PlatformTimeRanges> seekable() const final;

    MediaTime maxMediaTimeSeekable() const final;
    MediaTime minMediaTimeSeekable() const final;
    std::unique_ptr<PlatformTimeRanges> buffered() const final;
    double seekableTimeRangesLastModifiedTime() const final;
    double liveUpdateInterval() const final;

    unsigned long long totalBytes() const final;
    bool didLoadingProgress() const final;

    void setSize(const IntSize&) final;

    void paint(GraphicsContext&, const FloatRect&) final;

    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) final;
    bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject, GC3Denum, GC3Dint, GC3Denum, GC3Denum, GC3Denum, bool, bool) final;
    NativeImagePtr nativeImageForCurrentTime() final;

    void setPreload(MediaPlayer::Preload) final;

    bool hasAvailableVideoFrame() const final;

#if USE(NATIVE_FULLSCREEN_VIDEO)
    void enterFullscreen() final;
    void exitFullscreen() final;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    String wirelessPlaybackTargetName() const final;
    MediaPlayer::WirelessPlaybackTargetType wirelessPlaybackTargetType() const final;

    bool wirelessVideoPlaybackDisabled() const final;
    void setWirelessVideoPlaybackDisabled(bool) final;

    bool canPlayToWirelessPlaybackTarget() const final;
    bool isCurrentPlaybackTargetWireless() const final;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) final;

    void setShouldPlayToPlaybackTarget(bool) final;
#endif

#if USE(NATIVE_FULLSCREEN_VIDEO)
    bool canEnterFullscreen() const final;
#endif

    bool supportsAcceleratedRendering() const final;
    void acceleratedRenderingStateChanged() final;

    bool shouldMaintainAspectRatio() const final;
    void setShouldMaintainAspectRatio(bool) final;

    bool hasSingleSecurityOrigin() const final;
    bool didPassCORSAccessCheck() const final;
    Optional<bool> wouldTaintOrigin(const SecurityOrigin&) const final;

    MediaPlayer::MovieLoadType movieLoadType() const final;

    void prepareForRendering() final;

    MediaTime mediaTimeForTimeValue(const MediaTime& timeValue) const final;

    double maximumDurationToCacheMediaTime() const final;

    unsigned decodedFrameCount() const final;
    unsigned droppedFrameCount() const final;
    unsigned audioDecodedByteCount() const final;
    unsigned videoDecodedByteCount() const final;

    String engineDescription() const final;

#if ENABLE(WEB_AUDIO)
    AudioSourceProvider* audioSourceProvider() final;
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    std::unique_ptr<LegacyCDMSession> createSession(const String&, LegacyCDMSessionClient*) final;
    void setCDMSession(LegacyCDMSession*) final;
    void keyAdded() final;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&) final;
    void cdmInstanceDetached(CDMInstance&) final;
    void attemptToDecryptWithInstance(CDMInstance&) final;
    bool waitingForKey() const final;
#endif

#if ENABLE(VIDEO_TRACK)
    bool requiresTextTrackRepresentation() const final;
    void setTextTrackRepresentation(TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;
    void tracksChanged() final;
#endif

#if USE(GSTREAMER)
    void simulateAudioInterruption() final;
#endif

    void beginSimulatedHDCPError() final;
    void endSimulatedHDCPError() final;

    String languageOfPrimaryAudioTrack() const final;

    size_t extraMemoryCost() const final;

    unsigned long long fileSize() const final;

    bool ended() const final;

    Optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;

#if ENABLE(AVF_CAPTIONS)
    void notifyTrackModeChanged() final;
#endif

    void notifyActiveSourceBuffersChanged() final;

    void setShouldDisableSleep(bool) final;

    void applicationWillResignActive() final;
    void applicationDidBecomeActive() final;

#if USE(AVFOUNDATION)
    AVPlayer *objCAVFoundationAVPlayer() const final { return nullptr; }
#endif

    bool performTaskAtMediaTime(WTF::Function<void()>&&, MediaTime) final;

    bool shouldIgnoreIntrinsicSize() final;

    MediaPlayer* m_player { nullptr };
    RemoteMediaPlayerManager m_manager;
    MediaPlayerEnums::MediaEngineIdentifier m_remoteEngineIdentifier;
    MediaPlayerPrivateRemoteIdentifier m_id;
    RemoteMediaPlayerConfiguration m_configuration;

    RemoteMediaPlayerState m_cachedState;
    std::unique_ptr<PlatformTimeRanges> m_cachedBufferedTimeRanges;

    double m_volume { 1 };
    double m_rate { 1 };
    long m_platformErrorCode { 0 };
    MediaPlayerEnums::MovieLoadType m_movieLoadType { MediaPlayerEnums::MovieLoadType::Unknown };
    bool m_hasAudio { false };
    bool m_hasVideo { false };
    bool m_muted { false };
    bool m_seeking { false };

#if !RELEASE_LOG_DISABLED
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    bool m_invalid { false };
};

} // namespace WebKit

#endif
