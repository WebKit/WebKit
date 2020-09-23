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

#include "LayerHostingContext.h"
#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerManager.h"
#include "RemoteMediaPlayerState.h"
#include "RemoteMediaResourceIdentifier.h"
#include "RemoteMediaResourceProxy.h"
#include "TrackPrivateRemoteIdentifier.h"
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class MachSendRight;
}

namespace WebCore {
struct GenericCueData;
class ISOWebVTTCue;
class SerializedPlatformDataCueValue;
class TextTrackRepresentation;
}

namespace WebKit {

class AudioTrackPrivateRemote;
class TextTrackPrivateRemote;
class UserData;
class VideoTrackPrivateRemote;
struct TextTrackPrivateRemoteConfiguration;
struct TrackPrivateRemoteConfiguration;

class MediaPlayerPrivateRemote final
    : public CanMakeWeakPtr<MediaPlayerPrivateRemote>
    , public WebCore::MediaPlayerPrivateInterface
    , private IPC::MessageReceiver
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static std::unique_ptr<MediaPlayerPrivateRemote> create(WebCore::MediaPlayer* player, WebCore::MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, MediaPlayerPrivateRemoteIdentifier identifier, RemoteMediaPlayerManager& manager)
    {
        return makeUnique<MediaPlayerPrivateRemote>(player, remoteEngineIdentifier, identifier, manager);
    }

    MediaPlayerPrivateRemote(WebCore::MediaPlayer*, WebCore::MediaPlayerEnums::MediaEngineIdentifier, MediaPlayerPrivateRemoteIdentifier, RemoteMediaPlayerManager&);
    ~MediaPlayerPrivateRemote();

    void setConfiguration(RemoteMediaPlayerConfiguration&&, WebCore::SecurityOriginData&&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void invalidate() { m_invalid = true; }
    WebCore::MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier() const { return m_remoteEngineIdentifier; }
    MediaPlayerPrivateRemoteIdentifier itentifier() const { return m_id; }
    IPC::Connection& connection() const { return m_manager.gpuProcessConnection().connection(); }

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
    void characteristicChanged(RemoteMediaPlayerState&&);
    void sizeChanged(WebCore::FloatSize);
    void firstVideoFrameAvailable();
#if PLATFORM(COCOA)
    void setVideoInlineSizeFenced(const WebCore::IntSize&, const WTF::MachSendRight&);
    void setVideoFullscreenFrameFenced(const WebCore::FloatRect&, const WTF::MachSendRight&);
#endif

    void addRemoteAudioTrack(TrackPrivateRemoteIdentifier, TrackPrivateRemoteConfiguration&&);
    void removeRemoteAudioTrack(TrackPrivateRemoteIdentifier);
    void remoteAudioTrackConfigurationChanged(TrackPrivateRemoteIdentifier, TrackPrivateRemoteConfiguration&&);

    void addRemoteVideoTrack(TrackPrivateRemoteIdentifier, TrackPrivateRemoteConfiguration&&);
    void removeRemoteVideoTrack(TrackPrivateRemoteIdentifier);
    void remoteVideoTrackConfigurationChanged(TrackPrivateRemoteIdentifier, TrackPrivateRemoteConfiguration&&);

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

    void requestResource(RemoteMediaResourceIdentifier, WebCore::ResourceRequest&&, WebCore::PlatformMediaResourceLoader::LoadOptions, CompletionHandler<void()>&&);
    void removeResource(RemoteMediaResourceIdentifier);
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&);
    void resourceNotSupported();

    void engineUpdated();

    void activeSourceBuffersChanged();

#if ENABLE(ENCRYPTED_MEDIA)
    void waitingForKeyChanged();
    void initializationDataEncountered(const String&, IPC::DataReference&&);
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void currentPlaybackTargetIsWirelessChanged(bool);
#endif

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return *m_logger; }
    const char* logClassName() const override { return "MediaPlayerPrivateRemote"; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;
#endif

private:
    void load(const URL&, const WebCore::ContentType&, const String&) final;
    void prepareForPlayback(bool privateMode, WebCore::MediaPlayer::Preload, bool preservesPitch, bool prepare) final;

#if ENABLE(MEDIA_SOURCE)
    void load(const String&, WebCore::MediaSourcePrivateClient*) final;
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

    WebCore::FloatSize naturalSize() const final;

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

    double maxFastForwardRate() const final;
    double minFastReverseRate() const final;

    WebCore::MediaPlayer::NetworkState networkState() const final { return m_cachedState.networkState; }
    WebCore::MediaPlayer::ReadyState readyState() const final { return m_cachedState.readyState; }

    std::unique_ptr<WebCore::PlatformTimeRanges> seekable() const final;

    MediaTime maxMediaTimeSeekable() const final;
    MediaTime minMediaTimeSeekable() const final;
    std::unique_ptr<WebCore::PlatformTimeRanges> buffered() const final;
    double seekableTimeRangesLastModifiedTime() const final;
    double liveUpdateInterval() const final;

    unsigned long long totalBytes() const final;
    bool didLoadingProgress() const final;

    void paint(WebCore::GraphicsContext&, const WebCore::FloatRect&) final;
    void paintCurrentFrameInContext(WebCore::GraphicsContext&, const WebCore::FloatRect&) final;
    bool copyVideoTextureToPlatformTexture(WebCore::GraphicsContextGLOpenGL*, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool) final;
    WebCore::NativeImagePtr nativeImageForCurrentTime() final;

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

    bool hasSingleSecurityOrigin() const final;
    bool didPassCORSAccessCheck() const final;
    Optional<bool> wouldTaintOrigin(const WebCore::SecurityOrigin&) const final;

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
    std::unique_ptr<WebCore::LegacyCDMSession> createSession(const String&, WebCore::LegacyCDMSessionClient*) final;
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

    bool requiresTextTrackRepresentation() const final { return m_requiresTextTrackRepresentation; }
#if PLATFORM(COCOA)
    void setTextTrackRepresentation(WebCore::TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;
#endif
    void tracksChanged() final;

    void beginSimulatedHDCPError() final;
    void endSimulatedHDCPError() final;

    String languageOfPrimaryAudioTrack() const final;

    size_t extraMemoryCost() const final;

    Optional<WebCore::VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;

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

    bool performTaskAtMediaTime(Function<void()>&&, const MediaTime&) final;

    WebCore::MediaPlayer* m_player { nullptr };
    RefPtr<WebCore::PlatformMediaResourceLoader> m_mediaResourceLoader;
    bool m_requiresTextTrackRepresentation { false };
    PlatformLayerContainer m_videoInlineLayer;
    PlatformLayerContainer m_videoFullscreenLayer;
#if PLATFORM(COCOA)
    RetainPtr<PlatformLayer> m_textTrackRepresentationLayer;
#endif
    Optional<LayerHostingContextID> m_fullscreenLayerHostingContextId;
    RemoteMediaPlayerManager& m_manager;
    WebCore::MediaPlayerEnums::MediaEngineIdentifier m_remoteEngineIdentifier;
    MediaPlayerPrivateRemoteIdentifier m_id;
    RemoteMediaPlayerConfiguration m_configuration;

    RemoteMediaPlayerState m_cachedState;
    std::unique_ptr<WebCore::PlatformTimeRanges> m_cachedBufferedTimeRanges;

    HashMap<RemoteMediaResourceIdentifier, RefPtr<WebCore::PlatformMediaResource>> m_mediaResources;
    HashMap<TrackPrivateRemoteIdentifier, Ref<AudioTrackPrivateRemote>> m_audioTracks;
    HashMap<TrackPrivateRemoteIdentifier, Ref<VideoTrackPrivateRemote>> m_videoTracks;
    HashMap<TrackPrivateRemoteIdentifier, Ref<TextTrackPrivateRemote>> m_textTracks;

    WebCore::SecurityOriginData m_documentSecurityOrigin;
    mutable HashMap<WebCore::SecurityOriginData, Optional<bool>> m_wouldTaintOriginCache;

    double m_volume { 1 };
    double m_rate { 1 };
    long m_platformErrorCode { 0 };
    bool m_muted { false };
    bool m_seeking { false };
    bool m_isCurrentPlaybackTargetWireless { false };

#if !RELEASE_LOG_DISABLED
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    bool m_invalid { false };
};

} // namespace WebKit

#endif
