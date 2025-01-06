/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "Connection.h"
#include "MediaPlayerPrivateRemote.h"
#include "MessageReceiver.h"
#include "RemoteLegacyCDMSessionIdentifier.h"
#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "RemoteMediaPlayerState.h"
#include "RemoteMediaResourceIdentifier.h"
#include "RemoteVideoFrameProxy.h"
#include "SandboxExtension.h"
#include "ScopedRenderingResourcesRequest.h"
#include <WebCore/Cookie.h>
#include <WebCore/InbandTextTrackPrivate.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/MediaPromiseTypes.h>
#include <WebCore/PlatformMediaResourceLoader.h>
#include <optional>
#include <wtf/LoggerHelper.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMInstanceIdentifier.h"
#include "RemoteCDMInstanceProxy.h"
#endif

#if ENABLE(MEDIA_SOURCE)
#include "RemoteMediaSourceIdentifier.h"
#include "RemoteMediaSourceProxy.h"
#endif

#if PLATFORM(COCOA)
#include "SharedCARingBuffer.h"
#endif

#if USE(AVFOUNDATION)
#include <wtf/RetainPtr.h>
#endif

namespace WTF {
class MachSendRight;
}

namespace WebCore {
class AudioTrackPrivate;
class SecurityOriginData;
class VideoTrackPrivate;

struct FourCC;

class VideoFrame;

#if PLATFORM(COCOA)
class VideoFrameCV;
#endif
}

#if USE(AVFOUNDATION)
typedef struct __CVBuffer* CVPixelBufferRef;
#endif

namespace WebKit {

using LayerHostingContextID = uint32_t;
class LayerHostingContext;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
class MediaPlaybackTargetContextSerialized;
#endif
class RemoteAudioTrackProxy;
class RemoteAudioSourceProviderProxy;
class RemoteMediaPlayerManagerProxy;
class RemoteTextTrackProxy;
class RemoteVideoFrameObjectHeap;
class RemoteVideoTrackProxy;

class RemoteMediaPlayerProxy final
    : public RefCounted<RemoteMediaPlayerProxy>
    , public WebCore::MediaPlayerClient
    , private IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaPlayerProxy);
public:
    USING_CAN_MAKE_WEAKPTR(WebCore::MediaPlayerClient);

    static Ref<RemoteMediaPlayerProxy> create(RemoteMediaPlayerManagerProxy&, WebCore::MediaPlayerIdentifier, WebCore::MediaPlayerClientIdentifier, Ref<IPC::Connection>&&, WebCore::MediaPlayerEnums::MediaEngineIdentifier, RemoteMediaPlayerProxyConfiguration&&, RemoteVideoFrameObjectHeap&, const WebCore::ProcessIdentity&);
    ~RemoteMediaPlayerProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    WebCore::MediaPlayerIdentifier identifier() const { return m_id; }
    void invalidate();

    // Ensure that all previously queued messages to the content process have completed.
    Ref<WebCore::MediaPromise> commitAllTransactions();

#if ENABLE(VIDEO_PRESENTATION_MODE)
    void updateVideoFullscreenInlineImage();
    void setVideoFullscreenMode(WebCore::MediaPlayer::VideoFullscreenMode);
    void videoFullscreenStandbyChanged(bool);
#endif

    void setBufferingPolicy(WebCore::MediaPlayer::BufferingPolicy);

#if PLATFORM(IOS_FAMILY)
    void accessLog(CompletionHandler<void(String)>&&);
    void errorLog(CompletionHandler<void(String)>&&);
#endif

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    void getConfiguration(RemoteMediaPlayerConfiguration&);

    void prepareForPlayback(bool privateMode, WebCore::MediaPlayerEnums::Preload, bool preservesPitch, WebCore::MediaPlayerEnums::PitchCorrectionAlgorithm, bool prepareToPlay, bool prepareForRendering, WebCore::IntSize presentationSize, float videoContentScale, WebCore::DynamicRangeMode);
    void prepareForRendering();

    void load(URL&&, std::optional<SandboxExtension::Handle>&&, const WebCore::ContentType&, const String&, bool, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&&);
#if ENABLE(MEDIA_SOURCE)
    void loadMediaSource(URL&&, const WebCore::ContentType&, bool webMParserEnabled, RemoteMediaSourceIdentifier, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&&);
#endif
    void cancelLoad();

    void prepareToPlay();

    void play();
    void pause();

    void seekToTarget(const WebCore::SeekTarget&);

    void setVolume(double);
    void setMuted(bool);

    void setPreload(WebCore::MediaPlayerEnums::Preload);
    void setPrivateBrowsingMode(bool);
    void setPreservesPitch(bool);
    void setPitchCorrectionAlgorithm(WebCore::MediaPlayer::PitchCorrectionAlgorithm);

    void setPageIsVisible(bool);
    void setShouldMaintainAspectRatio(bool);
#if ENABLE(VIDEO_PRESENTATION_MODE)
    void setVideoFullscreenGravity(WebCore::MediaPlayerEnums::VideoGravity);
#endif
    void acceleratedRenderingStateChanged(bool);
    void setShouldDisableSleep(bool);
    void setRate(double);
    void didLoadingProgress(CompletionHandler<void(bool)>&&);

    void setPresentationSize(const WebCore::IntSize&);

#if PLATFORM(COCOA)
    void setVideoLayerSizeFenced(const WebCore::FloatSize&, WTF::MachSendRight&&);
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void setWirelessVideoPlaybackDisabled(bool);
    void setShouldPlayToPlaybackTarget(bool);
    void setWirelessPlaybackTarget(MediaPlaybackTargetContextSerialized&&);
    void mediaPlayerCurrentPlaybackTargetIsWirelessChanged(bool) final;
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void setLegacyCDMSession(std::optional<RemoteLegacyCDMSessionIdentifier>&& instanceId);
    void keyAdded();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(RemoteCDMInstanceIdentifier&&);
    void cdmInstanceDetached(RemoteCDMInstanceIdentifier&&);
    void attemptToDecryptWithInstance(RemoteCDMInstanceIdentifier&&);
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
    void setShouldContinueAfterKeyNeeded(bool);
#endif

    void beginSimulatedHDCPError();
    void endSimulatedHDCPError();

    void notifyActiveSourceBuffersChanged();

    void applicationWillResignActive();
    void applicationDidBecomeActive();

    void notifyTrackModeChanged();
    void tracksChanged();

    void audioTrackSetEnabled(WebCore::TrackID, bool);
    void videoTrackSetSelected(WebCore::TrackID, bool);
    void textTrackSetMode(WebCore::TrackID, WebCore::InbandTextTrackPrivate::Mode);

    using PerformTaskAtTimeCompletionHandler = CompletionHandler<void(std::optional<MediaTime>)>;
    void performTaskAtTime(const MediaTime&, PerformTaskAtTimeCompletionHandler&&);
    void isCrossOrigin(WebCore::SecurityOriginData, CompletionHandler<void(std::optional<bool>)>&&);

    void setVideoPlaybackMetricsUpdateInterval(double);

    void setPreferredDynamicRangeMode(WebCore::DynamicRangeMode);

    RefPtr<WebCore::PlatformMediaResource> requestResource(WebCore::ResourceRequest&&, WebCore::PlatformMediaResourceLoader::LoadOptions);
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&);
    void removeResource(RemoteMediaResourceIdentifier);

    RefPtr<WebCore::MediaPlayer> mediaPlayer() { return m_player; }

    void addRemoteAudioTrackProxy(WebCore::AudioTrackPrivate&);
    void addRemoteVideoTrackProxy(WebCore::VideoTrackPrivate&);
    void addRemoteTextTrackProxy(WebCore::InbandTextTrackPrivate&);

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

private:
    RemoteMediaPlayerProxy(RemoteMediaPlayerManagerProxy&, WebCore::MediaPlayerIdentifier, WebCore::MediaPlayerClientIdentifier, Ref<IPC::Connection>&&, WebCore::MediaPlayerEnums::MediaEngineIdentifier, RemoteMediaPlayerProxyConfiguration&&, RemoteVideoFrameObjectHeap&, const WebCore::ProcessIdentity&);

    // MediaPlayerClient
    void mediaPlayerCharacteristicChanged() final;
    void mediaPlayerRenderingModeChanged() final;
    void mediaPlayerNetworkStateChanged() final;
    void mediaPlayerReadyStateChanged() final;
    void mediaPlayerVolumeChanged() final;
    void mediaPlayerMuteChanged() final;
    void mediaPlayerSeeked(const MediaTime&) final;
    void mediaPlayerTimeChanged() final;
    void mediaPlayerDurationChanged() final;
    void mediaPlayerSizeChanged() final;
    void mediaPlayerRateChanged() final;
    void mediaPlayerPlaybackStateChanged() final;
    void mediaPlayerResourceNotSupported() final;
    void mediaPlayerEngineFailedToLoad() final;
    void mediaPlayerActiveSourceBuffersChanged() final;
    void mediaPlayerBufferedTimeRangesChanged() final;
    void mediaPlayerSeekableTimeRangesChanged() final;
    bool mediaPlayerRenderingCanBeAccelerated() final;

    void mediaPlayerDidAddAudioTrack(WebCore::AudioTrackPrivate&) final;
    void mediaPlayerDidRemoveAudioTrack(WebCore::AudioTrackPrivate&) final;
    void mediaPlayerDidAddVideoTrack(WebCore::VideoTrackPrivate&) final;
    void mediaPlayerDidRemoveVideoTrack(WebCore::VideoTrackPrivate&) final;
    void mediaPlayerDidAddTextTrack(WebCore::InbandTextTrackPrivate&) final;
    void mediaPlayerDidRemoveTextTrack(WebCore::InbandTextTrackPrivate&) final;
    String audioOutputDeviceId() const final { return m_configuration.audioOutputDeviceId; }
    String audioOutputDeviceIdOverride() const final { return m_configuration.audioOutputDeviceId; }

    // Not implemented
    void mediaPlayerFirstVideoFrameAvailable() final;

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String&) const final;
    void mediaPlayerKeyNeeded(const WebCore::SharedBuffer&) final;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void mediaPlayerInitializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&) final;
    void mediaPlayerWaitingForKeyChanged() final;
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
    String mediaPlayerMediaKeysStorageDirectory() const final;
#endif

    String mediaPlayerReferrer() const final;
    String mediaPlayerUserAgent() const final;
    bool mediaPlayerIsFullscreen() const final;
    bool mediaPlayerIsFullscreenPermitted() const final;
    bool mediaPlayerIsVideo() const final;
    float mediaPlayerContentsScale() const final;
    bool mediaPlayerPlatformVolumeConfigurationRequired() const final;
    WebCore::CachedResourceLoader* mediaPlayerCachedResourceLoader() final;
    Ref<WebCore::PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() final;
    bool doesHaveAttribute(const AtomString&, AtomString* = nullptr) const final;
    bool mediaPlayerShouldUsePersistentCache() const final;
    const String& mediaPlayerMediaCacheDirectory() const final;
    WebCore::LayoutRect mediaPlayerContentBoxRect() const final;

    void textTrackRepresentationBoundsChanged(const WebCore::IntRect&) final;

#if PLATFORM(COCOA)
    Vector<RefPtr<WebCore::PlatformTextTrack>> outOfBandTrackSources() final;
#endif

#if PLATFORM(IOS_FAMILY)
    String mediaPlayerNetworkInterfaceName() const final;
    void mediaPlayerGetRawCookies(const URL&, WebCore::MediaPlayerClient::GetRawCookiesCallback&&) const final;
#endif

    String mediaPlayerSourceApplicationIdentifier() const final;

    double mediaPlayerRequestedPlaybackRate() const final;
#if ENABLE(VIDEO_PRESENTATION_MODE)
    WebCore::MediaPlayerEnums::VideoFullscreenMode mediaPlayerFullscreenMode() const final;
    bool mediaPlayerIsVideoFullscreenStandby() const final;
#endif
    Vector<String> mediaPlayerPreferredAudioCharacteristics() const final;

    bool mediaPlayerShouldDisableSleep() const final;
    const Vector<WebCore::ContentType>& mediaContentTypesRequiringHardwareSupport() const final;
    bool mediaPlayerShouldCheckHardwareSupport() const final;

    const std::optional<Vector<String>>& allowedMediaContainerTypes() const final { return m_configuration.allowedMediaContainerTypes; };
    const std::optional<Vector<String>>& allowedMediaCodecTypes() const final { return m_configuration.allowedMediaCodecTypes; };
    const std::optional<Vector<WebCore::FourCC>>& allowedMediaVideoCodecIDs() const final { return m_configuration.allowedMediaVideoCodecIDs; };
    const std::optional<Vector<WebCore::FourCC>>& allowedMediaAudioCodecIDs() const final { return m_configuration.allowedMediaAudioCodecIDs; };
    const std::optional<Vector<WebCore::FourCC>>& allowedMediaCaptionFormatTypes() const final { return m_configuration.allowedMediaCaptionFormatTypes; };

    bool mediaPlayerPrefersSandboxedParsing() const final { return m_configuration.prefersSandboxedParsing; }
    bool mediaPlayerShouldDisableHDR() const final { return m_configuration.shouldDisableHDR; }

    WebCore::PlatformVideoTarget mediaPlayerVideoTarget() const final;

    void startUpdateCachedStateMessageTimer();
    void updateCachedState(bool = false);
    void sendCachedState();
    void timerFired();

    void maybeUpdateCachedVideoMetrics();
    void updateCachedVideoMetrics();

    void createAudioSourceProvider();
    void setShouldEnableAudioSourceProvider(bool);

    void playAtHostTime(MonotonicTime);
    void pauseAtHostTime(MonotonicTime);

    void startVideoFrameMetadataGathering();
    void stopVideoFrameMetadataGathering();
#if PLATFORM(COCOA)
    void mediaPlayerOnNewVideoFrameMetadata(WebCore::VideoFrameMetadata&&, RetainPtr<CVPixelBufferRef>&&);
#endif

    void playerContentBoxRectChanged(const WebCore::LayoutRect&);

    void currentTimeChanged(const MediaTime&);

#if PLATFORM(COCOA)
    WebCore::FloatSize mediaPlayerVideoLayerSize() const final { return m_configuration.videoLayerSize; }
    void setVideoLayerSizeIfPossible(const WebCore::FloatSize&);
    void nativeImageForCurrentTime(CompletionHandler<void(std::optional<WTF::MachSendRight>&&, WebCore::DestinationColorSpace)>&&);
    void colorSpace(CompletionHandler<void(WebCore::DestinationColorSpace)>&&);
#endif
    void videoFrameForCurrentTimeIfChanged(CompletionHandler<void(std::optional<RemoteVideoFrameProxy::Properties>&&, bool)>&&);

    void setShouldDisableHDR(bool);
    using LayerHostingContextIDCallback = WebCore::MediaPlayer::LayerHostingContextIDCallback;
    void requestHostingContextID(LayerHostingContextIDCallback&&);
    void setShouldCheckHardwareSupport(bool);
#if HAVE(SPATIAL_TRACKING_LABEL)
    void setDefaultSpatialTrackingLabel(const String&);
    void setSpatialTrackingLabel(const String&);
#endif

    void isInFullscreenOrPictureInPictureChanged(bool);

    void audioOutputDeviceChanged(String&&);

#if !RELEASE_LOG_DISABLED
    const Logger& mediaPlayerLogger() final { return m_logger; }
    Ref<const Logger> protectedMediaPlayerLogger() const { return m_logger; }
    uint64_t mediaPlayerLogIdentifier() { return m_configuration.logIdentifier; }
    const Logger& logger() { return mediaPlayerLogger(); }
    uint64_t logIdentifier() { return mediaPlayerLogIdentifier(); }
    ASCIILiteral logClassName() const { return "RemoteMediaPlayerProxy"_s; }
    WTFLogChannel& logChannel() const;
#endif

    RefPtr<WebCore::MediaPlayer> protectedPlayer() const { return m_player; }
#if ENABLE(MEDIA_SOURCE)
    RefPtr<RemoteMediaSourceProxy> protectedMediaSourceProxy() const { return m_mediaSourceProxy; }
#endif

    Ref<IPC::Connection> protectedConnection() const { return m_webProcessConnection; }
    Ref<RemoteVideoFrameObjectHeap> protectedVideoFrameObjectHeap() const;

    Vector<Ref<RemoteAudioTrackProxy>> m_audioTracks;
    Vector<Ref<RemoteVideoTrackProxy>> m_videoTracks;
    Vector<Ref<RemoteTextTrackProxy>> m_textTracks;

    WebCore::MediaPlayerIdentifier m_id;
    WebCore::MediaPlayerClientIdentifier m_clientIdentifier;
    RefPtr<SandboxExtension> m_sandboxExtension;
    Ref<IPC::Connection> m_webProcessConnection;
    RefPtr<WebCore::MediaPlayer> m_player;
    Vector<LayerHostingContextIDCallback> m_layerHostingContextIDRequests;
    std::unique_ptr<LayerHostingContext> m_inlineLayerHostingContext;
#if ENABLE(VIDEO_PRESENTATION_MODE)
    std::unique_ptr<LayerHostingContext> m_fullscreenLayerHostingContext;
#endif
    WeakPtr<RemoteMediaPlayerManagerProxy> m_manager;
    WebCore::MediaPlayerEnums::MediaEngineIdentifier m_engineIdentifier;
    Vector<WebCore::ContentType> m_typesRequiringHardwareSupport;
    RunLoop::Timer m_updateCachedStateMessageTimer;
    RemoteMediaPlayerState m_cachedState;
    RemoteMediaPlayerProxyConfiguration m_configuration;
    PerformTaskAtTimeCompletionHandler m_performTaskAtTimeCompletionHandler;
#if ENABLE(MEDIA_SOURCE)
    RefPtr<RemoteMediaSourceProxy> m_mediaSourceProxy;
#endif

    Seconds m_videoPlaybackMetricsUpdateInterval;
    MonotonicTime m_nextPlaybackQualityMetricsUpdateTime;
    bool m_hasPlaybackMetricsUpdatePending { false };

    float m_videoContentScale { 1.0 };
    WebCore::LayoutRect m_playerContentBoxRect;

    bool m_bufferedChanged { true };
    bool m_renderingCanBeAccelerated { false };
    WebCore::MediaPlayer::VideoFullscreenMode m_fullscreenMode { WebCore::MediaPlayer::VideoFullscreenModeNone };
    bool m_videoFullscreenStandby { false };

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
    bool m_shouldContinueAfterKeyNeeded { false };
    std::optional<RemoteLegacyCDMSessionIdentifier> m_legacySession;
#endif

#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
    RefPtr<RemoteAudioSourceProviderProxy> m_remoteAudioSourceProvider;
#endif
    ScopedRenderingResourcesRequest m_renderingResourcesRequest;

    bool m_observingTimeChanges { false };
    Ref<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;
    RefPtr<WebCore::VideoFrame> m_videoFrameForCurrentTime;
    bool m_shouldCheckHardwareSupport { false };
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
#endif
};

} // namespace WebKit

namespace IPC {

// default CompletionHandler value should the process crash.
template<> struct AsyncReplyError<WTF::MediaTime> {
    static WTF::MediaTime create() { return WTF::MediaTime::invalidTime(); };
};

} // namespace IPC

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
