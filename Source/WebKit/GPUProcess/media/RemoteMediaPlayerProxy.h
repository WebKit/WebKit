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

#include "MediaPlayerPrivateRemoteIdentifier.h"
#include <wtf/LoggerHelper.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace IPC {
class Connection;
}

namespace WebCore {
class MediaPlayer;
}

namespace WebKit {

class RemoteMediaPlayerManagerProxy;

class RemoteMediaPlayerProxy
    : public MediaPlayerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteMediaPlayerProxy(RemoteMediaPlayerManagerProxy&, MediaPlayerPrivateRemoteIdentifier, Ref<IPC::Connection>&&, MediaPlayerEnums::MediaEngineIdentifier);
    virtual ~RemoteMediaPlayerProxy()
    {
    }

    void invalidate();

    void prepareForPlayback(bool privateMode, WebCore::MediaPlayerEnums::Preload, bool preservesPitch, bool prepareForRendering);

    void load(const URL&, const ContentType&, const String&);
    void cancelLoad();

    void prepareToPlay();

    void play();
    void pause();

    void setVolume(double);
    void setMuted(bool);

    void setPreload(WebCore::MediaPlayerEnums::Preload);
    void setPrivateBrowsingMode(bool);
    void setPreservesPitch(bool);

private:
    // MediaPlayerClient
    void mediaPlayerNetworkStateChanged() final;
    void mediaPlayerReadyStateChanged() final;
    void mediaPlayerVolumeChanged() final;
    void mediaPlayerMuteChanged() final;
    void mediaPlayerTimeChanged() final;
    void mediaPlayerDurationChanged() final;
    void mediaPlayerRateChanged() final;

    // Not implemented
    void mediaPlayerPlaybackStateChanged() final;
    void mediaPlayerResourceNotSupported() final;
    void mediaPlayerSizeChanged() final;
    void mediaPlayerEngineUpdated() final;
    void mediaPlayerFirstVideoFrameAvailable() final;
    void mediaPlayerCharacteristicChanged() final;
    bool mediaPlayerRenderingCanBeAccelerated() final;
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

    void mediaPlayerCurrentPlaybackTargetIsWirelessChanged() final;

    String mediaPlayerReferrer() const final;
    String mediaPlayerUserAgent() const final;
    void mediaPlayerEnterFullscreen() final;
    void mediaPlayerExitFullscreen() final;
    bool mediaPlayerIsFullscreen() const final;
    bool mediaPlayerIsFullscreenPermitted() const final;
    bool mediaPlayerIsVideo() const final;
    LayoutRect mediaPlayerContentBoxRect() const final;
    float mediaPlayerContentsScale() const final;
    void mediaPlayerPause() final;
    void mediaPlayerPlay() final;
    bool mediaPlayerPlatformVolumeConfigurationRequired() const final;
    CachedResourceLoader* mediaPlayerCachedResourceLoader() final;
    RefPtr<PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() final;
    bool doesHaveAttribute(const AtomString&, AtomString* = nullptr) const final;
    bool mediaPlayerShouldUsePersistentCache() const final;
    const String& mediaPlayerMediaCacheDirectory() const final;

    void mediaPlayerDidAddAudioTrack(AudioTrackPrivate&) final;
    void mediaPlayerDidAddTextTrack(InbandTextTrackPrivate&) final;
    void mediaPlayerDidAddVideoTrack(VideoTrackPrivate&) final;
    void mediaPlayerDidRemoveAudioTrack(AudioTrackPrivate&) final;
    void mediaPlayerDidRemoveTextTrack(InbandTextTrackPrivate&) final;
    void mediaPlayerDidRemoveVideoTrack(VideoTrackPrivate&) final;

    void textTrackRepresentationBoundsChanged(const IntRect&) final;

#if ENABLE(VIDEO_TRACK) && ENABLE(AVF_CAPTIONS)
    Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources() final;
#endif

#if PLATFORM(IOS_FAMILY)
    String mediaPlayerNetworkInterfaceName() const final;
    bool mediaPlayerGetRawCookies(const URL&, Vector<Cookie>&) const final;
#endif

    void mediaPlayerHandlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType) final;

    String mediaPlayerSourceApplicationIdentifier() const final;

    bool mediaPlayerIsInMediaDocument() const final;
    void mediaPlayerEngineFailedToLoad() const final;

    double mediaPlayerRequestedPlaybackRate() const final;
    MediaPlayerEnums::VideoFullscreenMode mediaPlayerFullscreenMode() const final;
    bool mediaPlayerIsVideoFullscreenStandby() const final;
    Vector<String> mediaPlayerPreferredAudioCharacteristics() const final;

    bool mediaPlayerShouldDisableSleep() const final;
    const Vector<WebCore::ContentType>& mediaContentTypesRequiringHardwareSupport() const final;
    bool mediaPlayerShouldCheckHardwareSupport() const final;

#if !RELEASE_LOG_DISABLED
    const Logger& mediaPlayerLogger() final { return m_logger; }
#endif

    MediaPlayerPrivateRemoteIdentifier m_id;
    Ref<IPC::Connection> m_webProcessConnection;
    RefPtr<MediaPlayer> m_player;
    RemoteMediaPlayerManagerProxy& m_manager;
    MediaPlayerEnums::MediaEngineIdentifier m_engineIdentifier;
    Vector<WebCore::ContentType> m_typesRequiringHardwareSupport;

#if !RELEASE_LOG_DISABLED
    const Logger& m_logger;
#endif
};

}

#endif
