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

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "MediaPlayerPrivateRemoteIdentifier.h"
#include "MessageReceiver.h"
#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "RemoteMediaPlayerState.h"
#include "RemoteMediaResourceIdentifier.h"
#include "SandboxExtension.h"
#include "TrackPrivateRemoteIdentifier.h"
#include <WebCore/Cookie.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/PlatformMediaResourceLoader.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class AudioTrackPrivate;
class VideoTrackPrivate;
}

namespace WebKit {

using LayerHostingContextID = uint32_t;
class LayerHostingContext;
class RemoteAudioTrackProxy;
class RemoteMediaPlayerManagerProxy;
class RemoteVideoTrackProxy;

class RemoteMediaPlayerProxy final
    : public CanMakeWeakPtr<RemoteMediaPlayerProxy>
    , public WebCore::MediaPlayerClient
    , private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteMediaPlayerProxy(RemoteMediaPlayerManagerProxy&, MediaPlayerPrivateRemoteIdentifier, Ref<IPC::Connection>&&, WebCore::MediaPlayerEnums::MediaEngineIdentifier, RemoteMediaPlayerProxyConfiguration&&);
    ~RemoteMediaPlayerProxy();

    MediaPlayerPrivateRemoteIdentifier idendifier() const { return m_id; }
    void invalidate();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void getConfiguration(RemoteMediaPlayerConfiguration&);

    void prepareForPlayback(bool privateMode, WebCore::MediaPlayerEnums::Preload, bool preservesPitch, bool prepareForRendering, WebCore::LayoutRect, float videoContentScale, CompletionHandler<void(Optional<LayerHostingContextID>&&)>&&);
    void prepareForRendering();

    void load(URL&&, Optional<SandboxExtension::Handle>&&, const WebCore::ContentType&, const String&, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&&);
    void cancelLoad();

    void prepareToPlay();

    void play();
    void pause();

    void seek(MediaTime&&);
    void seekWithTolerance(MediaTime&&, MediaTime&& negativeTolerance, MediaTime&& positiveTolerance);

    void setVolume(double);
    void setMuted(bool);

    void setPreload(WebCore::MediaPlayerEnums::Preload);
    void setPrivateBrowsingMode(bool);
    void setPreservesPitch(bool);

    void setVisible(bool);
    void setShouldMaintainAspectRatio(bool);
    void setVideoFullscreenFrame(WebCore::FloatRect);
    void setVideoFullscreenGravity(WebCore::MediaPlayerEnums::VideoGravity);
    void acceleratedRenderingStateChanged(bool);
    void setShouldDisableSleep(bool);
    void setRate(double);

    void audioTrackSetEnabled(TrackPrivateRemoteIdentifier, bool);
    void videoTrackSetSelected(TrackPrivateRemoteIdentifier, bool);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void setWirelessVideoPlaybackDisabled(bool);
    void setShouldPlayToPlaybackTarget(bool);
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void keyAdded();
#endif

    void beginSimulatedHDCPError();
    void endSimulatedHDCPError();

    void notifyActiveSourceBuffersChanged();

    void applicationWillResignActive();
    void applicationDidBecomeActive();

    Ref<WebCore::PlatformMediaResource> requestResource(WebCore::ResourceRequest&&, WebCore::PlatformMediaResourceLoader::LoadOptions);
    void removeResource(RemoteMediaResourceIdentifier);

private:
    // MediaPlayerClient
    void mediaPlayerNetworkStateChanged() final;
    void mediaPlayerReadyStateChanged() final;
    void mediaPlayerVolumeChanged() final;
    void mediaPlayerMuteChanged() final;
    void mediaPlayerTimeChanged() final;
    void mediaPlayerDurationChanged() final;
    void mediaPlayerRateChanged() final;
    void mediaPlayerPlaybackStateChanged() final;
    void mediaPlayerEngineFailedToLoad() const final;
    void mediaPlayerBufferedTimeRangesChanged() final;
    void mediaPlayerSeekableTimeRangesChanged() final;
    bool mediaPlayerRenderingCanBeAccelerated() final;

    void mediaPlayerDidAddAudioTrack(WebCore::AudioTrackPrivate&) final;
    void mediaPlayerDidRemoveAudioTrack(WebCore::AudioTrackPrivate&) final;
    void mediaPlayerDidAddVideoTrack(WebCore::VideoTrackPrivate&) final;
    void mediaPlayerDidRemoveVideoTrack(WebCore::VideoTrackPrivate&) final;

    // Not implemented
    void mediaPlayerResourceNotSupported() final;
    void mediaPlayerSizeChanged() final;
    void mediaPlayerEngineUpdated() final;
    void mediaPlayerFirstVideoFrameAvailable() final;
    void mediaPlayerCharacteristicChanged() final;
    void mediaPlayerRenderingModeChanged() final;
    void mediaPlayerActiveSourceBuffersChanged() final;

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String&) const final;
    bool mediaPlayerKeyNeeded(Uint8Array*) final;
    String mediaPlayerMediaKeysStorageDirectory() const final;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void mediaPlayerInitializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&) final;
    void mediaPlayerWaitingForKeyChanged() final;
#endif

    void mediaPlayerCurrentPlaybackTargetIsWirelessChanged(bool) final;

    String mediaPlayerReferrer() const final;
    String mediaPlayerUserAgent() const final;
    void mediaPlayerEnterFullscreen() final;
    void mediaPlayerExitFullscreen() final;
    bool mediaPlayerIsFullscreen() const final;
    bool mediaPlayerIsFullscreenPermitted() const final;
    bool mediaPlayerIsVideo() const final;
    WebCore::LayoutRect mediaPlayerContentBoxRect() const final;
    float mediaPlayerContentsScale() const final;
    void mediaPlayerPause() final;
    void mediaPlayerPlay() final;
    bool mediaPlayerPlatformVolumeConfigurationRequired() const final;
    WebCore::CachedResourceLoader* mediaPlayerCachedResourceLoader() final;
    RefPtr<WebCore::PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() final;
    bool doesHaveAttribute(const AtomString&, AtomString* = nullptr) const final;
    bool mediaPlayerShouldUsePersistentCache() const final;
    const String& mediaPlayerMediaCacheDirectory() const final;

    void mediaPlayerDidAddTextTrack(WebCore::InbandTextTrackPrivate&) final;
    void mediaPlayerDidRemoveTextTrack(WebCore::InbandTextTrackPrivate&) final;

    void textTrackRepresentationBoundsChanged(const WebCore::IntRect&) final;

#if ENABLE(VIDEO_TRACK) && ENABLE(AVF_CAPTIONS)
    Vector<RefPtr<WebCore::PlatformTextTrack>> outOfBandTrackSources() final;
#endif

#if PLATFORM(IOS_FAMILY)
    String mediaPlayerNetworkInterfaceName() const final;
    bool mediaPlayerGetRawCookies(const URL&, Vector<WebCore::Cookie>&) const final;
#endif

    String mediaPlayerSourceApplicationIdentifier() const final;

    double mediaPlayerRequestedPlaybackRate() const final;
    WebCore::MediaPlayerEnums::VideoFullscreenMode mediaPlayerFullscreenMode() const final;
    bool mediaPlayerIsVideoFullscreenStandby() const final;
    Vector<String> mediaPlayerPreferredAudioCharacteristics() const final;

    bool mediaPlayerShouldDisableSleep() const final;
    const Vector<WebCore::ContentType>& mediaContentTypesRequiringHardwareSupport() const final;
    bool mediaPlayerShouldCheckHardwareSupport() const final;

    void startUpdateCachedStateMessageTimer();
    void updateCachedState();
    void sendCachedState();
    void timerFired();

#if !RELEASE_LOG_DISABLED
    const Logger& mediaPlayerLogger() final { return m_logger; }
    const void* mediaPlayerLogIdentifier() { return reinterpret_cast<const void*>(m_configuration.logIdentifier); }
#endif

    HashMap<WebCore::AudioTrackPrivate*, Ref<RemoteAudioTrackProxy>> m_audioTracks;
    HashMap<WebCore::VideoTrackPrivate*, Ref<RemoteVideoTrackProxy>> m_videoTracks;
    MediaPlayerPrivateRemoteIdentifier m_id;
    RefPtr<SandboxExtension> m_sandboxExtension;
    Ref<IPC::Connection> m_webProcessConnection;
    RefPtr<WebCore::MediaPlayer> m_player;
    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    RemoteMediaPlayerManagerProxy& m_manager;
    WebCore::MediaPlayerEnums::MediaEngineIdentifier m_engineIdentifier;
    Vector<WebCore::ContentType> m_typesRequiringHardwareSupport;
    RunLoop::Timer<RemoteMediaPlayerProxy> m_updateCachedStateMessageTimer;
    RemoteMediaPlayerState m_cachedState;
    RemoteMediaPlayerProxyConfiguration m_configuration;
    bool m_seekableChanged { true };
    bool m_bufferedChanged { true };
    bool m_renderingCanBeAccelerated { true };
    WebCore::LayoutRect m_videoContentBoxRect;
    float m_videoContentScale { 1.0 };

#if !RELEASE_LOG_DISABLED
    const Logger& m_logger;
#endif
};

} // namespace WebKit

#endif
