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

#include "config.h"
#include "RemoteMediaPlayerProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteMediaPlayerManagerProxy.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/NotImplemented.h>

namespace WebKit {

using namespace WebCore;

RemoteMediaPlayerProxy::RemoteMediaPlayerProxy(RemoteMediaPlayerManagerProxy& manager, MediaPlayerPrivateRemoteIdentifier id, Ref<IPC::Connection>&& connection, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier)
    : m_id(id)
    , m_webProcessConnection(WTFMove(connection))
    , m_manager(manager)
    , m_engineIdentifier(engineIdentifier)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_manager.logger())
#endif
{
    m_player = MediaPlayer::create(*this, m_engineIdentifier);
}

void RemoteMediaPlayerProxy::invalidate()
{
    m_player->invalidate();
}

void RemoteMediaPlayerProxy::load(const URL& url, const ContentType& contentType, const String& keySystem)
{
    m_player->load(url, contentType, keySystem);
}

void RemoteMediaPlayerProxy::prepareForPlayback(bool privateMode, WebCore::MediaPlayerEnums::Preload preload, bool preservesPitch, bool prepareForRendering)
{
    m_player->setPrivateBrowsingMode(privateMode);
    m_player->setPreload(preload);
    m_player->setPreservesPitch(preservesPitch);
    m_player->prepareForRendering();
}

void RemoteMediaPlayerProxy::cancelLoad()
{
    m_player->cancelLoad();
}

void RemoteMediaPlayerProxy::prepareToPlay()
{
    m_player->prepareToPlay();
}

void RemoteMediaPlayerProxy::play()
{
    m_player->play();
}

void RemoteMediaPlayerProxy::pause()
{
    m_player->pause();
}

void RemoteMediaPlayerProxy::setVolume(double volume)
{
    m_player->setVolume(volume);
}

void RemoteMediaPlayerProxy::setMuted(bool muted)
{
    m_player->setMuted(muted);
}

void RemoteMediaPlayerProxy::setPreload(WebCore::MediaPlayerEnums::Preload preload)
{
    m_player->setPreload(preload);
}

void RemoteMediaPlayerProxy::setPrivateBrowsingMode(bool privateMode)
{
    m_player->setPrivateBrowsingMode(privateMode);
}

void RemoteMediaPlayerProxy::setPreservesPitch(bool preservesPitch)
{
    m_player->setPreservesPitch(preservesPitch);
}

// MediaPlayerClient
void RemoteMediaPlayerProxy::mediaPlayerNetworkStateChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::NetworkStateChanged(m_id, m_player->networkState()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerReadyStateChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::ReadyStateChanged(m_id, m_player->readyState()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerVolumeChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::VolumeChanged(m_id, m_player->volume()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerMuteChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::MuteChanged(m_id, m_player->muted()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerTimeChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::TimeChanged(m_id, m_player->currentTime()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerDurationChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::DurationChanged(m_id, m_player->duration()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerRateChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::RateChanged(m_id, m_player->rate()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerEngineFailedToLoad() const
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::EngineFailedToLoad(m_id, m_player->platformErrorCode()), 0);
}

// FIXME: Unimplemented
void RemoteMediaPlayerProxy::mediaPlayerPlaybackStateChanged()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerSawUnsupportedTracks()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerResourceNotSupported()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerRepaint()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerSizeChanged()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerEngineUpdated()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerFirstVideoFrameAvailable()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerCharacteristicChanged()
{
    notImplemented();
}


bool RemoteMediaPlayerProxy::mediaPlayerRenderingCanBeAccelerated()
{
    notImplemented();
    return false;
}

void RemoteMediaPlayerProxy::mediaPlayerRenderingModeChanged()
{
    notImplemented();
}

bool RemoteMediaPlayerProxy::mediaPlayerAcceleratedCompositingEnabled()
{
    notImplemented();
    return false;
}

void RemoteMediaPlayerProxy::mediaPlayerActiveSourceBuffersChanged()
{
    notImplemented();
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RefPtr<ArrayBuffer> RemoteMediaPlayerProxy::mediaPlayerCachedKeyForKeyId(const String&) const
{
    notImplemented();
    return nullptr;
}

bool RemoteMediaPlayerProxy::mediaPlayerKeyNeeded(Uint8Array*)
{
    notImplemented();
    return false;
}

String RemoteMediaPlayerProxy::mediaPlayerMediaKeysStorageDirectory() const
{
    notImplemented();
    return emptyString();
}

#endif

#if ENABLE(ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::mediaPlayerInitializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerWaitingForKeyChanged()
{
    notImplemented();
}
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void RemoteMediaPlayerProxy::mediaPlayerCurrentPlaybackTargetIsWirelessChanged() { };
#endif

String RemoteMediaPlayerProxy::mediaPlayerReferrer() const
{
    notImplemented();
    return String();
}

String RemoteMediaPlayerProxy::mediaPlayerUserAgent() const
{
    notImplemented();
    return String();
}

void RemoteMediaPlayerProxy::mediaPlayerEnterFullscreen()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerExitFullscreen()
{
    notImplemented();
}

bool RemoteMediaPlayerProxy::mediaPlayerIsFullscreen() const
{
    return false;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsFullscreenPermitted() const
{
    notImplemented();
    return false;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsVideo() const
{
    notImplemented();
    return false;
}

LayoutRect RemoteMediaPlayerProxy::mediaPlayerContentBoxRect() const
{
    notImplemented();
    return LayoutRect();
}

float RemoteMediaPlayerProxy::mediaPlayerContentsScale() const
{
    notImplemented();
    return 1;
}

void RemoteMediaPlayerProxy::mediaPlayerSetSize(const IntSize&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerPause()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerPlay()
{
    notImplemented();
}

bool RemoteMediaPlayerProxy::mediaPlayerPlatformVolumeConfigurationRequired() const
{
    notImplemented();
    return false;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsPaused() const
{
    notImplemented();
    return true;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsLooping() const
{
    notImplemented();
    return false;
}

CachedResourceLoader* RemoteMediaPlayerProxy::mediaPlayerCachedResourceLoader()
{
    notImplemented();
    return nullptr;
}

RefPtr<PlatformMediaResourceLoader> RemoteMediaPlayerProxy::mediaPlayerCreateResourceLoader()
{
    notImplemented();
    return nullptr;
}

bool RemoteMediaPlayerProxy::doesHaveAttribute(const AtomString&, AtomString*) const
{
    notImplemented();
    return false;
}

bool RemoteMediaPlayerProxy::mediaPlayerShouldUsePersistentCache() const
{
    notImplemented();
    return true;
}

const String& RemoteMediaPlayerProxy::mediaPlayerMediaCacheDirectory() const
{
    notImplemented();
    return emptyString();
}

#if ENABLE(VIDEO_TRACK)
void RemoteMediaPlayerProxy::mediaPlayerDidAddAudioTrack(AudioTrackPrivate&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddTextTrack(InbandTextTrackPrivate&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddVideoTrack(VideoTrackPrivate&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveAudioTrack(AudioTrackPrivate&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveTextTrack(InbandTextTrackPrivate&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveVideoTrack(VideoTrackPrivate&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::textTrackRepresentationBoundsChanged(const IntRect&)
{
    notImplemented();
}

#endif

#if ENABLE(VIDEO_TRACK) && ENABLE(AVF_CAPTIONS)
Vector<RefPtr<PlatformTextTrack>> RemoteMediaPlayerProxy::outOfBandTrackSources()
{
    notImplemented();
    return { };
}

#endif

#if PLATFORM(IOS_FAMILY)
String RemoteMediaPlayerProxy::mediaPlayerNetworkInterfaceName() const
{
    notImplemented();
    return String();
}

bool RemoteMediaPlayerProxy::mediaPlayerGetRawCookies(const URL&, Vector<Cookie>&) const
{
    notImplemented();
    return false;
}

#endif

void RemoteMediaPlayerProxy::mediaPlayerHandlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType)
{
    notImplemented();
}

String RemoteMediaPlayerProxy::mediaPlayerSourceApplicationIdentifier() const
{
    notImplemented();
    return emptyString();
}

bool RemoteMediaPlayerProxy::mediaPlayerIsInMediaDocument() const
{
    notImplemented();
    return false;
}

double RemoteMediaPlayerProxy::mediaPlayerRequestedPlaybackRate() const
{
    notImplemented();
    return 0;
}

MediaPlayerEnums::VideoFullscreenMode RemoteMediaPlayerProxy::mediaPlayerFullscreenMode() const
{
    notImplemented();
    return MediaPlayerEnums::VideoFullscreenModeNone;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsVideoFullscreenStandby() const
{
    notImplemented();
    return false;
}

Vector<String> RemoteMediaPlayerProxy::mediaPlayerPreferredAudioCharacteristics() const
{
    notImplemented();
    return Vector<String>();
}

bool RemoteMediaPlayerProxy::mediaPlayerShouldDisableSleep() const
{
    notImplemented();
    return false;
}

const Vector<WebCore::ContentType>& RemoteMediaPlayerProxy::mediaContentTypesRequiringHardwareSupport() const
{
    notImplemented();
    return m_typesRequiringHardwareSupport;
}

bool RemoteMediaPlayerProxy::mediaPlayerShouldCheckHardwareSupport() const
{
    notImplemented();
    return false;
}

} // namespace WebKit

#undef MESSAGE_CHECK_CONTEXTID

#endif // ENABLE(GPU_PROCESS)
