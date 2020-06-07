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

#include "DataReference.h"
#include "GPUConnectionToWebProcess.h"
#include "LayerHostingContext.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteAudioTrackProxy.h"
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMSessionProxy.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "RemoteMediaPlayerState.h"
#include "RemoteMediaResource.h"
#include "RemoteMediaResourceIdentifier.h"
#include "RemoteMediaResourceLoader.h"
#include "RemoteMediaResourceManager.h"
#include "RemoteTextTrackProxy.h"
#include "RemoteVideoTrackProxy.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/LayoutRect.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/NotImplemented.h>

#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMFactoryProxy.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include <WebCore/MediaPlaybackTargetCocoa.h>
#include <WebCore/MediaPlaybackTargetMock.h>
#endif

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
    if (m_performTaskAtMediaTimeCompletionHandler)
        m_performTaskAtMediaTimeCompletionHandler(WTF::nullopt);
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
    auto maxDuration = m_player->maximumDurationToCacheMediaTime();
    configuration.maximumDurationToCacheMediaTime = maxDuration ? maxDuration : 0.2;
    configuration.supportsScanning = m_player->supportsScanning();
    configuration.supportsFullscreen = m_player->supportsFullscreen();
    configuration.supportsPictureInPicture = m_player->supportsPictureInPicture();
    configuration.supportsAcceleratedRendering = m_player->supportsAcceleratedRendering();
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    configuration.canPlayToWirelessPlaybackTarget = m_player->canPlayToWirelessPlaybackTarget();
#endif
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

void RemoteMediaPlayerProxy::seek(const MediaTime& time)
{
    m_player->seek(time);
}

void RemoteMediaPlayerProxy::seekWithTolerance(const MediaTime& time, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
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

#if ENABLE(VIDEO_PRESENTATION_MODE)
void RemoteMediaPlayerProxy::setVideoFullscreenGravity(WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    m_player->setVideoFullscreenGravity(gravity);
}
#endif

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
#if ENABLE(VIDEO_PRESENTATION_MODE)
void RemoteMediaPlayerProxy::updateVideoFullscreenInlineImage()
{
    m_player->updateVideoFullscreenInlineImage();
}

void RemoteMediaPlayerProxy::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode mode)
{
    m_player->setVideoFullscreenMode(mode);

}

void RemoteMediaPlayerProxy::videoFullscreenStandbyChanged()
{
    m_player->videoFullscreenStandbyChanged();
}
#endif

void RemoteMediaPlayerProxy::setBufferingPolicy(MediaPlayer::BufferingPolicy policy)
{
    m_player->setBufferingPolicy(policy);
}

#if PLATFORM(IOS_FAMILY)
void RemoteMediaPlayerProxy::accessLog(CompletionHandler<void(String)>&& completionHandler)
{
    completionHandler(m_player->accessLog());
}

void RemoteMediaPlayerProxy::errorLog(CompletionHandler<void(String)>&& completionHandler)
{
    completionHandler(m_player->errorLog());
}
#endif

void RemoteMediaPlayerProxy::mediaPlayerNetworkStateChanged()
{
    updateCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::NetworkStateChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerReadyStateChanged()
{
    updateCachedState();
    m_cachedState.canSaveMediaData = m_player->canSaveMediaData();
    m_cachedState.startDate = m_player->getStartDate();
    m_cachedState.startTime = m_player->startTime();
    m_cachedState.naturalSize = m_player->naturalSize();
    m_cachedState.maxFastForwardRate = m_player->maxFastForwardRate();
    m_cachedState.minFastReverseRate = m_player->minFastReverseRate();
    m_cachedState.hasAvailableVideoFrame = m_player->hasAvailableVideoFrame();
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    m_cachedState.wirelessVideoPlaybackDisabled = m_player->wirelessVideoPlaybackDisabled();
#endif
    m_cachedState.hasSingleSecurityOrigin = m_player->hasSingleSecurityOrigin();
    m_cachedState.didPassCORSAccessCheck = m_player->didPassCORSAccessCheck();
    m_cachedState.wouldTaintDocumentSecurityOrigin = m_player->wouldTaintOrigin(m_configuration.documentSecurityOrigin.securityOrigin());

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
    m_cachedState.minTimeSeekable = m_player->minTimeSeekable();
    m_cachedState.maxTimeSeekable = m_player->maxTimeSeekable();
    m_cachedState.seekableTimeRangesLastModifiedTime = m_player->seekableTimeRangesLastModifiedTime();
    m_cachedState.liveUpdateInterval = m_player->liveUpdateInterval();

    if (!m_updateCachedStateMessageTimer.isActive())
        sendCachedState();
}

void RemoteMediaPlayerProxy::mediaPlayerCharacteristicChanged()
{
    updateCachedState();
    m_cachedState.hasClosedCaptions = m_player->hasClosedCaptions();
    m_cachedState.languageOfPrimaryAudioTrack = m_player->languageOfPrimaryAudioTrack();

    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::CharacteristicChanged(m_cachedState), m_id);
}

bool RemoteMediaPlayerProxy::mediaPlayerRenderingCanBeAccelerated()
{
    return m_renderingCanBeAccelerated;
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddAudioTrack(WebCore::AudioTrackPrivate& track)
{
#if !RELEASE_LOG_DISABLED
    track.setLogger(mediaPlayerLogger(), mediaPlayerLogIdentifier());
#endif
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
#if !RELEASE_LOG_DISABLED
    track.setLogger(mediaPlayerLogger(), mediaPlayerLogIdentifier());
#endif
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

void RemoteMediaPlayerProxy::mediaPlayerDidAddTextTrack(WebCore::InbandTextTrackPrivate& track)
{
#if !RELEASE_LOG_DISABLED
    track.setLogger(mediaPlayerLogger(), mediaPlayerLogIdentifier());
#endif
    m_textTracks.set(&track, RemoteTextTrackProxy::create(*this, TrackPrivateRemoteIdentifier::generate(), m_webProcessConnection.copyRef(), track));
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveTextTrack(WebCore::InbandTextTrackPrivate& track)
{
    ASSERT(m_textTracks.contains(&track));
    m_textTracks.remove(&track);
}

void RemoteMediaPlayerProxy::textTrackRepresentationBoundsChanged(const IntRect&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::textTrackSetMode(TrackPrivateRemoteIdentifier trackID, WebCore::InbandTextTrackPrivate::Mode mode)
{
    for (auto& track : m_textTracks.values()) {
        if (track->identifier() == trackID) {
            track->setMode(mode);
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

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

void RemoteMediaPlayerProxy::mediaPlayerActiveSourceBuffersChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ActiveSourceBuffersChanged(), m_id);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RefPtr<ArrayBuffer> RemoteMediaPlayerProxy::mediaPlayerCachedKeyForKeyId(const String& keyId) const
{
    if (auto cdmSession = m_manager.gpuConnectionToWebProcess().legacyCdmFactoryProxy().getSession(m_legacySession))
        return cdmSession->getCachedKeyForKeyId(keyId);
    return nullptr;
}

void RemoteMediaPlayerProxy::mediaPlayerKeyNeeded(Uint8Array* message)
{
    IPC::DataReference messageReference;
    if (message)
        messageReference = { message->data(), message->byteLength() };
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::MediaPlayerKeyNeeded(WTFMove(messageReference)), m_id);
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::mediaPlayerInitializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::InitializationDataEncountered(initDataType, IPC::DataReference(reinterpret_cast<uint8_t*>(initData->data()), initData->byteLength())), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerWaitingForKeyChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::WaitingForKeyChanged(), m_id);
}
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void RemoteMediaPlayerProxy::mediaPlayerCurrentPlaybackTargetIsWirelessChanged(bool isCurrentPlaybackTargetWireless)
{
    m_cachedState.wirelessPlaybackTargetName = m_player->wirelessPlaybackTargetName();
    m_cachedState.wirelessPlaybackTargetType = m_player->wirelessPlaybackTargetType();
    sendCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::CurrentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless), m_id);
}

void RemoteMediaPlayerProxy::setWirelessVideoPlaybackDisabled(bool disabled)
{
    m_player->setWirelessVideoPlaybackDisabled(disabled);
    m_cachedState.wirelessVideoPlaybackDisabled = m_player->wirelessVideoPlaybackDisabled();
}

void RemoteMediaPlayerProxy::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    m_player->setShouldPlayToPlaybackTarget(shouldPlay);
}

void RemoteMediaPlayerProxy::setWirelessPlaybackTarget(const WebCore::MediaPlaybackTargetContext& targetContext)
{
#if !PLATFORM(IOS_FAMILY)
    switch (targetContext.type()) {
    case MediaPlaybackTargetContext::AVOutputContextType:
        m_player->setWirelessPlaybackTarget(WebCore::MediaPlaybackTargetCocoa::create(targetContext.avOutputContext()));
        break;
    case MediaPlaybackTargetContext::MockType:
        m_player->setWirelessPlaybackTarget(WebCore::MediaPlaybackTargetMock::create(targetContext.mockDeviceName(), targetContext.mockState()));
        break;
    case MediaPlaybackTargetContext::None:
        ASSERT_NOT_REACHED();
        break;
    }
#else
    UNUSED_PARAM(targetContext);
#endif
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

float RemoteMediaPlayerProxy::mediaPlayerContentsScale() const
{
    return m_videoContentScale;
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

#if ENABLE(AVF_CAPTIONS)
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

#if ENABLE(VIDEO_PRESENTATION_MODE)
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
#endif

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
    m_cachedState.hasAudio = m_player->hasAudio();
    m_cachedState.hasVideo = m_player->hasVideo();

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
void RemoteMediaPlayerProxy::setLegacyCDMSession(RemoteLegacyCDMSessionIdentifier&& instanceId)
{
    if (m_legacySession == instanceId)
        return;

    if (m_legacySession) {
        if (auto cdmSession = m_manager.gpuConnectionToWebProcess().legacyCdmFactoryProxy().getSession(m_legacySession)) {
            m_player->setCDMSession(nullptr);
            cdmSession->setPlayer(nullptr);
        }
    }

    m_legacySession = instanceId;

    if (m_legacySession) {
        if (auto cdmSession = m_manager.gpuConnectionToWebProcess().legacyCdmFactoryProxy().getSession(m_legacySession)) {
            m_player->setCDMSession(cdmSession->session());
            cdmSession->setPlayer(makeWeakPtr(this));
        }
    }
}

void RemoteMediaPlayerProxy::keyAdded()
{
    m_player->keyAdded();
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::cdmInstanceAttached(RemoteCDMInstanceIdentifier&& instanceId)
{
    if (auto* instanceProxy = m_manager.gpuConnectionToWebProcess().cdmFactoryProxy().getInstance(instanceId))
        m_player->cdmInstanceAttached(instanceProxy->instance());
}

void RemoteMediaPlayerProxy::cdmInstanceDetached(RemoteCDMInstanceIdentifier&& instanceId)
{
    if (auto* instanceProxy = m_manager.gpuConnectionToWebProcess().cdmFactoryProxy().getInstance(instanceId))
        m_player->cdmInstanceDetached(instanceProxy->instance());
}

void RemoteMediaPlayerProxy::attemptToDecryptWithInstance(RemoteCDMInstanceIdentifier&& instanceId)
{
    if (auto* instanceProxy = m_manager.gpuConnectionToWebProcess().cdmFactoryProxy().getInstance(instanceId))
        m_player->attemptToDecryptWithInstance(instanceProxy->instance());
}
#endif


#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::setShouldContinueAfterKeyNeeded(bool should)
{
    m_player->setShouldContinueAfterKeyNeeded(should);
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

void RemoteMediaPlayerProxy::notifyTrackModeChanged()
{
#if ENABLE(AVF_CAPTIONS)
    m_player->notifyTrackModeChanged();
#endif
}

void RemoteMediaPlayerProxy::tracksChanged()
{
    m_player->tracksChanged();
}

void RemoteMediaPlayerProxy::syncTextTrackBounds()
{
    m_player->syncTextTrackBounds();
}

void RemoteMediaPlayerProxy::performTaskAtMediaTime(const MediaTime& taskTime, WallTime messageTime, CompletionHandler<void(Optional<MediaTime>)>&& completionHandler)
{
    if (m_performTaskAtMediaTimeCompletionHandler) {
        // A media player is only expected to track one pending task-at-time at once (e.g. see
        // MediaPlayerPrivateAVFoundationObjC::performTaskAtMediaTime), so cancel the existing
        // CompletionHandler.
        auto handler = WTFMove(m_performTaskAtMediaTimeCompletionHandler);
        handler(WTF::nullopt);
    }

    auto transmissionTime = MediaTime::createWithDouble((WallTime::now() - messageTime).value(), 1);
    auto adjustedTaskTime = taskTime - transmissionTime;
    auto currentTime = m_player->currentTime();
    if (adjustedTaskTime <= currentTime) {
        completionHandler(currentTime);
        return;
    }

    m_performTaskAtMediaTimeCompletionHandler = WTFMove(completionHandler);
    m_player->performTaskAtMediaTime([this, weakThis = makeWeakPtr(this)]() mutable {
        if (!weakThis || !m_performTaskAtMediaTimeCompletionHandler)
            return;

        auto completionHandler = WTFMove(m_performTaskAtMediaTimeCompletionHandler);
        completionHandler(m_player->currentTime());
    }, adjustedTaskTime);
}

void RemoteMediaPlayerProxy::wouldTaintOrigin(struct WebCore::SecurityOriginData originData, CompletionHandler<void(Optional<bool>)>&& completionHandler)
{
    completionHandler(m_player->wouldTaintOrigin(originData.securityOrigin()));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
