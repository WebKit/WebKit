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

#include "GPUConnectionToWebProcess.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteAudioTrackProxy.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "RemoteMediaPlayerState.h"
#include "RemoteMediaResource.h"
#include "RemoteMediaResourceIdentifier.h"
#include "RemoteMediaResourceLoader.h"
#include "RemoteMediaResourceManager.h"
#include "RemoteVideoTrackProxy.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/LayoutRect.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/NotImplemented.h>

namespace WebKit {

using namespace WebCore;

RemoteMediaPlayerProxy::RemoteMediaPlayerProxy(RemoteMediaPlayerManagerProxy& manager, MediaPlayerPrivateRemoteIdentifier id, Ref<IPC::Connection>&& connection, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, RemoteMediaPlayerProxyConfiguration&& configuration)
    : m_id(id)
    , m_webProcessConnection(WTFMove(connection))
    , m_manager(manager)
    , m_engineIdentifier(engineIdentifier)
    , m_updateCachedStateMessageTimer(RunLoop::main(), this, &RemoteMediaPlayerProxy::timerFired)
    , m_configuration(configuration)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_manager.logger())
#endif
{
    m_typesRequiringHardwareSupport = m_configuration.mediaContentTypesRequiringHardwareSupport;
    m_player = MediaPlayer::create(*this, m_engineIdentifier);
}

RemoteMediaPlayerProxy::~RemoteMediaPlayerProxy()
{
}

void RemoteMediaPlayerProxy::invalidate()
{
    m_updateCachedStateMessageTimer.stop();
    m_player->invalidate();
    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }
}

void RemoteMediaPlayerProxy::getConfiguration(RemoteMediaPlayerConfiguration& configuration)
{
    configuration.engineDescription = m_player->engineDescription();
    configuration.supportsScanning = m_player->supportsScanning();
    configuration.supportsFullscreen = m_player->supportsFullscreen();
    configuration.supportsPictureInPicture = m_player->supportsPictureInPicture();
    configuration.supportsAcceleratedRendering = m_player->supportsAcceleratedRendering();
    configuration.canPlayToWirelessPlaybackTarget = m_player->canPlayToWirelessPlaybackTarget();
    configuration.shouldIgnoreIntrinsicSize = m_player->shouldIgnoreIntrinsicSize();
}

void RemoteMediaPlayerProxy::load(URL&& url, Optional<SandboxExtension::Handle>&& sandboxExtensionHandle, const ContentType& contentType, const String& keySystem, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&& completionHandler)
{
    RemoteMediaPlayerConfiguration configuration;
    if (sandboxExtensionHandle) {
        m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle.value()));
        if (m_sandboxExtension)
            m_sandboxExtension->consume();
        else
            WTFLogAlways("Unable to create sandbox extension for media url.\n");
    }

    m_player->load(url, contentType, keySystem);
    getConfiguration(configuration);
    completionHandler(WTFMove(configuration));
}

void RemoteMediaPlayerProxy::prepareForPlayback(bool privateMode, WebCore::MediaPlayerEnums::Preload preload, bool preservesPitch, bool prepareForRendering, LayoutRect layoutRect, float videoContentScale, CompletionHandler<void(Optional<LayerHostingContextID>&& contextId)>&& completionHandler)
{
    m_player->setPrivateBrowsingMode(privateMode);
    m_player->setPreload(preload);
    m_player->setPreservesPitch(preservesPitch);
    m_player->prepareForRendering();
    m_videoContentBoxRect = layoutRect;
    m_videoContentScale = videoContentScale;
    m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess();
    completionHandler(m_layerHostingContext->contextID());
}

void RemoteMediaPlayerProxy::cancelLoad()
{
    m_updateCachedStateMessageTimer.stop();
    m_player->cancelLoad();
}

void RemoteMediaPlayerProxy::prepareToPlay()
{
    m_player->prepareToPlay();
}

void RemoteMediaPlayerProxy::play()
{
    if (m_player->movieLoadType() != WebCore::MediaPlayerEnums::MovieLoadType::LiveStream)
        startUpdateCachedStateMessageTimer();
    m_player->play();
    sendCachedState();
}

void RemoteMediaPlayerProxy::pause()
{
    m_updateCachedStateMessageTimer.stop();
    m_player->pause();
    sendCachedState();
}

void RemoteMediaPlayerProxy::seek(MediaTime&& time)
{
    m_player->seek(time);
}

void RemoteMediaPlayerProxy::seekWithTolerance(MediaTime&& time, MediaTime&& negativeTolerance, MediaTime&& positiveTolerance)
{
    m_player->seekWithTolerance(time, negativeTolerance, positiveTolerance);
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

void RemoteMediaPlayerProxy::prepareForRendering()
{
    m_player->prepareForRendering();
}

void RemoteMediaPlayerProxy::setVisible(bool visible)
{
    m_player->setVisible(visible);
}

void RemoteMediaPlayerProxy::setShouldMaintainAspectRatio(bool maintainRatio)
{
    m_player->setShouldMaintainAspectRatio(maintainRatio);
}

void RemoteMediaPlayerProxy::setVideoFullscreenFrame(WebCore::FloatRect rect)
{
    m_player->setVideoFullscreenFrame(rect);
}

void RemoteMediaPlayerProxy::setVideoFullscreenGravity(WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    m_player->setVideoFullscreenGravity(gravity);
}

void RemoteMediaPlayerProxy::acceleratedRenderingStateChanged(bool renderingCanBeAccelerated)
{
    m_renderingCanBeAccelerated = renderingCanBeAccelerated;
    m_player->acceleratedRenderingStateChanged();
}

void RemoteMediaPlayerProxy::setShouldDisableSleep(bool disable)
{
    m_player->setShouldDisableSleep(disable);
}

void RemoteMediaPlayerProxy::setRate(double rate)
{
    m_player->setRate(rate);
}

Ref<PlatformMediaResource> RemoteMediaPlayerProxy::requestResource(ResourceRequest&& request, PlatformMediaResourceLoader::LoadOptions options)
{
    auto& remoteMediaResourceManager = m_manager.gpuConnectionToWebProcess().remoteMediaResourceManager();
    auto remoteMediaResourceIdentifier = RemoteMediaResourceIdentifier::generate();
    auto remoteMediaResource = RemoteMediaResource::create(remoteMediaResourceManager, *this, remoteMediaResourceIdentifier);
    remoteMediaResourceManager.addMediaResource(remoteMediaResourceIdentifier, remoteMediaResource);

    m_webProcessConnection->sendWithAsyncReply(Messages::MediaPlayerPrivateRemote::RequestResource(remoteMediaResourceIdentifier, request, options), [remoteMediaResource = remoteMediaResource.copyRef()]() {
        remoteMediaResource->setReady(true);
    }, m_id);

    return remoteMediaResource;
}

void RemoteMediaPlayerProxy::removeResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoveResource(remoteMediaResourceIdentifier), m_id);
}

// MediaPlayerClient
void RemoteMediaPlayerProxy::mediaPlayerNetworkStateChanged()
{
    updateCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::NetworkStateChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerReadyStateChanged()
{
    updateCachedState();
    m_cachedState.naturalSize = m_player->naturalSize();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ReadyStateChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerVolumeChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::VolumeChanged(m_player->volume()), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerMuteChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::MuteChanged(m_player->muted()), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerTimeChanged()
{
    updateCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::TimeChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerDurationChanged()
{
    updateCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::DurationChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerRateChanged()
{
    sendCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RateChanged(m_player->rate()), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerEngineFailedToLoad() const
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::EngineFailedToLoad(m_player->platformErrorCode()), m_id);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
String RemoteMediaPlayerProxy::mediaPlayerMediaKeysStorageDirectory() const
{
    return m_manager.gpuConnectionToWebProcess().mediaKeysStorageDirectory();
}
#endif

String RemoteMediaPlayerProxy::mediaPlayerReferrer() const
{
    return m_configuration.referrer;
}

String RemoteMediaPlayerProxy::mediaPlayerUserAgent() const
{
    return m_configuration.userAgent;
}

String RemoteMediaPlayerProxy::mediaPlayerSourceApplicationIdentifier() const
{
    return m_configuration.sourceApplicationIdentifier;
}

#if PLATFORM(IOS_FAMILY)
String RemoteMediaPlayerProxy::mediaPlayerNetworkInterfaceName() const
{
    return m_configuration.networkInterfaceName;
}
#endif

const String& RemoteMediaPlayerProxy::mediaPlayerMediaCacheDirectory() const
{
    return m_manager.gpuConnectionToWebProcess().mediaCacheDirectory();
}

const Vector<WebCore::ContentType>& RemoteMediaPlayerProxy::mediaContentTypesRequiringHardwareSupport() const
{
    return m_typesRequiringHardwareSupport;
}

Vector<String> RemoteMediaPlayerProxy::mediaPlayerPreferredAudioCharacteristics() const
{
    return m_configuration.preferredAudioCharacteristics;
}

bool RemoteMediaPlayerProxy::mediaPlayerShouldUsePersistentCache() const
{
    return m_configuration.shouldUsePersistentCache;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsVideo() const
{
    return m_configuration.isVideo;
}

// FIXME: Unimplemented
void RemoteMediaPlayerProxy::mediaPlayerPlaybackStateChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::PlaybackStateChanged(m_player->paused()), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerBufferedTimeRangesChanged()
{
    m_bufferedChanged = true;
}

void RemoteMediaPlayerProxy::mediaPlayerSeekableTimeRangesChanged()
{
    m_seekableChanged = true;
}

void RemoteMediaPlayerProxy::mediaPlayerCharacteristicChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::CharacteristicChanged(m_player->hasAudio(), m_player->hasVideo(), m_player->movieLoadType()), m_id);
}

bool RemoteMediaPlayerProxy::mediaPlayerRenderingCanBeAccelerated()
{
    return m_renderingCanBeAccelerated;
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddAudioTrack(WebCore::AudioTrackPrivate& track)
{
    m_audioTracks.set(&track, RemoteAudioTrackProxy::create(*this, TrackPrivateRemoteIdentifier::generate(), m_webProcessConnection.copyRef(), track));
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveAudioTrack(WebCore::AudioTrackPrivate& track)
{
    ASSERT(m_audioTracks.contains(&track));
    m_audioTracks.remove(&track);
}

void RemoteMediaPlayerProxy::audioTrackSetEnabled(TrackPrivateRemoteIdentifier trackID, bool enabled)
{
    for (auto& track : m_audioTracks.values()) {
        if (track->identifier() == trackID) {
            track->setEnabled(enabled);
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddVideoTrack(WebCore::VideoTrackPrivate& track)
{
    m_videoTracks.set(&track, RemoteVideoTrackProxy::create(*this, TrackPrivateRemoteIdentifier::generate(), m_webProcessConnection.copyRef(), track));
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveVideoTrack(WebCore::VideoTrackPrivate& track)
{
    ASSERT(m_videoTracks.contains(&track));
    m_videoTracks.remove(&track);
}

void RemoteMediaPlayerProxy::videoTrackSetSelected(TrackPrivateRemoteIdentifier trackID, bool selected)
{
    for (auto& track : m_videoTracks.values()) {
        if (track->identifier() == trackID) {
            track->setSelected(selected);
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

// FIXME: Unimplemented
void RemoteMediaPlayerProxy::mediaPlayerResourceNotSupported()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ResourceNotSupported(), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerSizeChanged()
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerEngineUpdated()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::EngineUpdated(), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerFirstVideoFrameAvailable()
{
    m_layerHostingContext->setRootLayer(m_player->platformLayer());
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::FirstVideoFrameAvailable(), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerRenderingModeChanged()
{
    m_layerHostingContext->setRootLayer(m_player->platformLayer());
}

void RemoteMediaPlayerProxy::mediaPlayerActiveSourceBuffersChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ActiveSourceBuffersChanged(), m_id);
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
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::mediaPlayerInitializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerWaitingForKeyChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::WaitingForKeyChanged(), m_id);
}
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void RemoteMediaPlayerProxy::mediaPlayerCurrentPlaybackTargetIsWirelessChanged(bool isCurrentPlaybackTargetWireless)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::CurrentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless), m_id);
}

void RemoteMediaPlayerProxy::setWirelessVideoPlaybackDisabled(bool disabled)
{
    m_player->setWirelessVideoPlaybackDisabled(disabled);
}

void RemoteMediaPlayerProxy::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    m_player->setShouldPlayToPlaybackTarget(shouldPlay);
}
#endif

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

LayoutRect RemoteMediaPlayerProxy::mediaPlayerContentBoxRect() const
{
    return m_videoContentBoxRect;
}

float RemoteMediaPlayerProxy::mediaPlayerContentsScale() const
{
    return m_videoContentScale;
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

CachedResourceLoader* RemoteMediaPlayerProxy::mediaPlayerCachedResourceLoader()
{
    notImplemented();
    return nullptr;
}

RefPtr<PlatformMediaResourceLoader> RemoteMediaPlayerProxy::mediaPlayerCreateResourceLoader()
{
    return adoptRef(*new RemoteMediaResourceLoader(*this));
}

bool RemoteMediaPlayerProxy::doesHaveAttribute(const AtomString&, AtomString*) const
{
    notImplemented();
    return false;
}

#if ENABLE(VIDEO_TRACK)
void RemoteMediaPlayerProxy::mediaPlayerDidAddTextTrack(InbandTextTrackPrivate&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveTextTrack(InbandTextTrackPrivate&)
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
bool RemoteMediaPlayerProxy::mediaPlayerGetRawCookies(const URL&, Vector<Cookie>&) const
{
    notImplemented();
    return false;
}
#endif

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

bool RemoteMediaPlayerProxy::mediaPlayerShouldDisableSleep() const
{
    notImplemented();
    return false;
}

bool RemoteMediaPlayerProxy::mediaPlayerShouldCheckHardwareSupport() const
{
    notImplemented();
    return false;
}

void RemoteMediaPlayerProxy::startUpdateCachedStateMessageTimer()
{
    static const Seconds maxTimeupdateEventFrequency { 100_ms };

    if (m_updateCachedStateMessageTimer.isActive())
        return;

    m_updateCachedStateMessageTimer.startRepeating(maxTimeupdateEventFrequency);
}

void RemoteMediaPlayerProxy::timerFired()
{
    sendCachedState();
}

void RemoteMediaPlayerProxy::updateCachedState()
{
    m_cachedState.currentTime = m_player->currentTime();
    m_cachedState.duration = m_player->duration();
    m_cachedState.networkState = m_player->networkState();
    m_cachedState.readyState = m_player->readyState();
    m_cachedState.paused = m_player->paused();
    m_cachedState.loadingProgressed = m_player->didLoadingProgress();

    if (m_seekableChanged) {
        m_seekableChanged = false;
        m_cachedState.minTimeSeekable = m_player->minTimeSeekable();
        m_cachedState.maxTimeSeekable = m_player->maxTimeSeekable();
    }

    if (m_bufferedChanged) {
        m_bufferedChanged = false;
        m_cachedState.bufferedRanges = *m_player->buffered();
    }
}

void RemoteMediaPlayerProxy::sendCachedState()
{
    updateCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::UpdateCachedState(m_cachedState), m_id);
    m_cachedState.bufferedRanges.clear();
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::keyAdded()
{
    m_player->keyAdded();
}
#endif

void RemoteMediaPlayerProxy::beginSimulatedHDCPError()
{
    m_player->beginSimulatedHDCPError();
}

void RemoteMediaPlayerProxy::endSimulatedHDCPError()
{
    m_player->endSimulatedHDCPError();
}

void RemoteMediaPlayerProxy::notifyActiveSourceBuffersChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ActiveSourceBuffersChanged(), m_id);
}

void RemoteMediaPlayerProxy::applicationWillResignActive()
{
    m_player->applicationWillResignActive();
}

void RemoteMediaPlayerProxy::applicationDidBecomeActive()
{
    m_player->applicationDidBecomeActive();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
