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

#include "config.h"
#include "RemoteMediaPlayerProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "GPUConnectionToWebProcess.h"
#include "LayerHostingContext.h"
#include "Logging.h"
#include "MediaPlayerPrivateRemote.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteAudioSourceProviderProxy.h"
#include "RemoteAudioTrackProxy.h"
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMSessionProxy.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "RemoteMediaResource.h"
#include "RemoteMediaResourceIdentifier.h"
#include "RemoteMediaResourceLoader.h"
#include "RemoteMediaResourceManager.h"
#include "RemoteTextTrackProxy.h"
#include "RemoteVideoFrameObjectHeap.h"
#include "RemoteVideoFrameProxy.h"
#include "RemoteVideoTrackProxy.h"
#include "TextTrackPrivateRemoteConfiguration.h"
#include "TrackPrivateRemoteConfiguration.h"
#include "WebCoreArgumentCoders.h"
#include <JavaScriptCore/Uint8Array.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMFactoryProxy.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetContextSerialized.h"
#endif

#if PLATFORM(COCOA)
#include <WebCore/AudioSourceProviderAVFObjC.h>
#include <WebCore/VideoFrameCV.h>
#endif

#include <wtf/NativePromise.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaPlayerProxy);

Ref<RemoteMediaPlayerProxy> RemoteMediaPlayerProxy::create(RemoteMediaPlayerManagerProxy& manager, MediaPlayerIdentifier identifier, MediaPlayerClientIdentifier clientIdentifier, Ref<IPC::Connection>&& connection, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, RemoteMediaPlayerProxyConfiguration&& configuration, RemoteVideoFrameObjectHeap& videoFrameObjectHeap, const WebCore::ProcessIdentity& resourceOwner)
{
    return adoptRef(*new RemoteMediaPlayerProxy(manager, identifier, clientIdentifier, WTFMove(connection), engineIdentifier, WTFMove(configuration), videoFrameObjectHeap, resourceOwner));
}

RemoteMediaPlayerProxy::RemoteMediaPlayerProxy(RemoteMediaPlayerManagerProxy& manager, MediaPlayerIdentifier identifier, MediaPlayerClientIdentifier clientIdentifier, Ref<IPC::Connection>&& connection, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, RemoteMediaPlayerProxyConfiguration&& configuration, RemoteVideoFrameObjectHeap& videoFrameObjectHeap, const WebCore::ProcessIdentity& resourceOwner)
    : m_id(identifier)
    , m_clientIdentifier(clientIdentifier)
    , m_webProcessConnection(WTFMove(connection))
    , m_manager(manager)
    , m_engineIdentifier(engineIdentifier)
    , m_updateCachedStateMessageTimer(RunLoop::main(), this, &RemoteMediaPlayerProxy::timerFired)
    , m_configuration(configuration)
    , m_renderingResourcesRequest(ScopedRenderingResourcesRequest::acquire())
    , m_videoFrameObjectHeap(videoFrameObjectHeap)
#if !RELEASE_LOG_DISABLED
    , m_logger(manager.logger())
#endif
{
    m_typesRequiringHardwareSupport = m_configuration.mediaContentTypesRequiringHardwareSupport;
    m_renderingCanBeAccelerated = m_configuration.renderingCanBeAccelerated;
    m_playerContentBoxRect = m_configuration.playerContentBoxRect;
    m_player = MediaPlayer::create(*this, m_engineIdentifier);
    m_player->setResourceOwner(resourceOwner);
    m_player->setPresentationSize(m_configuration.presentationSize);
}

RemoteMediaPlayerProxy::~RemoteMediaPlayerProxy()
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourceProxy)
        m_mediaSourceProxy->shutdown();
#endif
    if (m_performTaskAtTimeCompletionHandler)
        m_performTaskAtTimeCompletionHandler(std::nullopt);
    setShouldEnableAudioSourceProvider(false);

    for (auto& request : std::exchange(m_layerHostingContextIDRequests, { }))
        request({ });
}

void RemoteMediaPlayerProxy::invalidate()
{
    m_updateCachedStateMessageTimer.stop();
    m_player->invalidate();
    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }
    m_renderingResourcesRequest = { };
#if USE(AVFOUNDATION)
    m_videoFrameForCurrentTime = nullptr;
#endif
}

Ref<MediaPromise> RemoteMediaPlayerProxy::commitAllTransactions()
{
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return MediaPromise::createAndReject(PlatformMediaError::ClientDisconnected);

    return m_webProcessConnection->sendWithPromisedReply<MediaPromiseConverter>(Messages::MediaPlayerPrivateRemote::CommitAllTransactions { }, m_id);
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
    configuration.supportsPlayAtHostTime = m_player->supportsPlayAtHostTime();
    configuration.supportsPauseAtHostTime = m_player->supportsPauseAtHostTime();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    configuration.canPlayToWirelessPlaybackTarget = m_player->canPlayToWirelessPlaybackTarget();
#endif
    configuration.shouldIgnoreIntrinsicSize = m_player->shouldIgnoreIntrinsicSize();

    m_observingTimeChanges = m_player->setCurrentTimeDidChangeCallback([this, weakThis = WeakPtr { *this }] (auto currentTime) mutable {
        if (!weakThis)
            return;

        currentTimeChanged(currentTime);
    });
}

void RemoteMediaPlayerProxy::load(URL&& url, std::optional<SandboxExtension::Handle>&& sandboxExtensionHandle, const ContentType& contentType, const String& keySystem, bool requiresRemotePlayback, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&& completionHandler)
{
    RemoteMediaPlayerConfiguration configuration;
    if (sandboxExtensionHandle) {
        m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle.value()));
        if (m_sandboxExtension)
            m_sandboxExtension->consume();
        else
            WTFLogAlways("Unable to create sandbox extension for media url.\n");
    }

    m_player->load(url, contentType, keySystem, requiresRemotePlayback);
    getConfiguration(configuration);
    completionHandler(WTFMove(configuration));
}

#if ENABLE(MEDIA_SOURCE)
void RemoteMediaPlayerProxy::loadMediaSource(URL&& url, const WebCore::ContentType& contentType, bool webMParserEnabled, RemoteMediaSourceIdentifier mediaSourceIdentifier, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&& completionHandler)
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());

    RemoteMediaPlayerConfiguration configuration;
    if (!m_manager || !m_manager->gpuConnectionToWebProcess()) {
        completionHandler(WTFMove(configuration));
        return;
    }

    if (m_mediaSourceProxy)
        m_mediaSourceProxy->shutdown();
    m_mediaSourceProxy = adoptRef(*new RemoteMediaSourceProxy(*m_manager->gpuConnectionToWebProcess(), mediaSourceIdentifier, webMParserEnabled, *this));
    m_player->load(url, contentType, *m_mediaSourceProxy);
    getConfiguration(configuration);
    completionHandler(WTFMove(configuration));
}
#endif

void RemoteMediaPlayerProxy::cancelLoad()
{
    m_updateCachedStateMessageTimer.stop();
    m_player->cancelLoad();
}

void RemoteMediaPlayerProxy::prepareForPlayback(bool privateMode, WebCore::MediaPlayerEnums::Preload preload, bool preservesPitch, WebCore::MediaPlayerEnums::PitchCorrectionAlgorithm pitchCorrectionAlgorithm, bool prepareToPlay, bool prepareForRendering, WebCore::IntSize presentationSize, float videoContentScale, WebCore::DynamicRangeMode preferredDynamicRangeMode)
{
    m_player->setPrivateBrowsingMode(privateMode);
    m_player->setPreload(preload);
    m_player->setPreservesPitch(preservesPitch);
    m_player->setPitchCorrectionAlgorithm(pitchCorrectionAlgorithm);
    m_player->setPreferredDynamicRangeMode(preferredDynamicRangeMode);
    m_player->setPresentationSize(presentationSize);
    if (prepareToPlay)
        m_player->prepareToPlay();
    if (prepareForRendering)
        m_player->prepareForRendering();
    m_videoContentScale = videoContentScale;
}

void RemoteMediaPlayerProxy::prepareToPlay()
{
    ALWAYS_LOG(LOGIDENTIFIER);
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
    updateCachedVideoMetrics();
    m_player->pause();
    sendCachedState();
}

void RemoteMediaPlayerProxy::seekToTarget(const WebCore::SeekTarget& target)
{
    ALWAYS_LOG(LOGIDENTIFIER, target);
    m_player->seekToTarget(target);
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

void RemoteMediaPlayerProxy::setPitchCorrectionAlgorithm(WebCore::MediaPlayer::PitchCorrectionAlgorithm algorithm)
{
    m_player->setPitchCorrectionAlgorithm(algorithm);
}

void RemoteMediaPlayerProxy::prepareForRendering()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_player->prepareForRendering();
}

void RemoteMediaPlayerProxy::setPageIsVisible(bool visible)
{
    ALWAYS_LOG(LOGIDENTIFIER, visible);
    m_player->setPageIsVisible(visible);
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
    ALWAYS_LOG(LOGIDENTIFIER, renderingCanBeAccelerated);
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

void RemoteMediaPlayerProxy::didLoadingProgress(CompletionHandler<void(bool)>&& completionHandler)
{
    m_player->didLoadingProgress(WTFMove(completionHandler));
}

void RemoteMediaPlayerProxy::setPresentationSize(const WebCore::IntSize& size)
{
    if (size == m_configuration.presentationSize)
        return;

    m_configuration.presentationSize = size;
    m_player->setPresentationSize(size);
}

RefPtr<PlatformMediaResource> RemoteMediaPlayerProxy::requestResource(ResourceRequest&& request, PlatformMediaResourceLoader::LoadOptions options)
{
    ASSERT(isMainRunLoop());

    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return nullptr;

    auto& remoteMediaResourceManager = m_manager->gpuConnectionToWebProcess()->remoteMediaResourceManager();
    auto remoteMediaResourceIdentifier = RemoteMediaResourceIdentifier::generate();
    auto remoteMediaResource = RemoteMediaResource::create(remoteMediaResourceManager, *this, remoteMediaResourceIdentifier);
    remoteMediaResourceManager.addMediaResource(remoteMediaResourceIdentifier, remoteMediaResource);

    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RequestResource(remoteMediaResourceIdentifier, request, options), m_id);

    return remoteMediaResource;
}

void RemoteMediaPlayerProxy::sendH2Ping(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    m_webProcessConnection->sendWithAsyncReply(Messages::MediaPlayerPrivateRemote::SendH2Ping(url), WTFMove(completionHandler), m_id);
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
    m_fullscreenMode = mode;
    m_player->setVideoFullscreenMode(mode);
}

void RemoteMediaPlayerProxy::videoFullscreenStandbyChanged(bool standby)
{
    m_videoFullscreenStandby = standby;
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
    updateCachedState(true);
    m_cachedState.networkState = m_player->networkState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::NetworkStateChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerReadyStateChanged()
{
    auto newReadyState = m_player->readyState();
    ALWAYS_LOG(LOGIDENTIFIER, newReadyState);
    updateCachedVideoMetrics();
    updateCachedState(true);
    m_cachedState.networkState = m_player->networkState();
    m_cachedState.duration = m_player->duration();

    m_cachedState.movieLoadType = m_player->movieLoadType();
    m_cachedState.minTimeSeekable = m_player->minTimeSeekable();
    m_cachedState.maxTimeSeekable = m_player->maxTimeSeekable();
    m_cachedState.startDate = m_player->getStartDate();
    m_cachedState.startTime = m_player->startTime();
    m_cachedState.naturalSize = m_player->naturalSize();
    m_cachedState.maxFastForwardRate = m_player->maxFastForwardRate();
    m_cachedState.minFastReverseRate = m_player->minFastReverseRate();
    m_cachedState.seekableTimeRangesLastModifiedTime = m_player->seekableTimeRangesLastModifiedTime();
    m_cachedState.liveUpdateInterval = m_player->liveUpdateInterval();
    m_cachedState.hasAvailableVideoFrame = m_player->hasAvailableVideoFrame();
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    m_cachedState.wirelessVideoPlaybackDisabled = m_player->wirelessVideoPlaybackDisabled();
#endif
    m_cachedState.canSaveMediaData = m_player->canSaveMediaData();
    m_cachedState.didPassCORSAccessCheck = m_player->didPassCORSAccessCheck();
    m_cachedState.documentIsCrossOrigin = m_player->isCrossOrigin(m_configuration.documentSecurityOrigin.securityOrigin());
    m_cachedState.videoConfiguration = m_player->videoPlaybackConfiguration();

    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ReadyStateChanged(m_cachedState, newReadyState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerVolumeChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::VolumeChanged(m_player->volume()), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerMuteChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::MuteChanged(m_player->muted()), m_id);
}

static MediaTimeUpdateData timeUpdateData(const MediaPlayer& player, MediaTime time)
{
    return {
        time,
        player.timeIsProgressing(),
        MonotonicTime::now()
    };
}

void RemoteMediaPlayerProxy::mediaPlayerSeeked(const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, time);
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::Seeked(timeUpdateData(*m_player, time)), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerTimeChanged()
{
    updateCachedState(true);
    m_cachedState.duration = m_player->duration();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::TimeChanged(m_cachedState, timeUpdateData(*m_player, m_player->currentTime())), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerDurationChanged()
{
    updateCachedState(true);
    m_cachedState.duration = m_player->duration();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::DurationChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerRateChanged()
{
    updateCachedVideoMetrics();
    sendCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RateChanged(m_player->effectiveRate(), timeUpdateData(*m_player, m_player->currentTime())), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerEngineFailedToLoad()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::EngineFailedToLoad(m_player->platformErrorCode()), m_id);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
String RemoteMediaPlayerProxy::mediaPlayerMediaKeysStorageDirectory() const
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return emptyString();

    return m_manager->gpuConnectionToWebProcess()->mediaKeysStorageDirectory();
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

void RemoteMediaPlayerProxy::mediaPlayerGetRawCookies(const URL& url, WebCore::MediaPlayerClient::GetRawCookiesCallback&& completionHandler) const
{
    m_webProcessConnection->sendWithAsyncReply(Messages::MediaPlayerPrivateRemote::GetRawCookies(url), WTFMove(completionHandler), m_id);
}
#endif

const String& RemoteMediaPlayerProxy::mediaPlayerMediaCacheDirectory() const
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return emptyString();

    return m_manager->gpuConnectionToWebProcess()->mediaCacheDirectory();
}

LayoutRect RemoteMediaPlayerProxy::mediaPlayerContentBoxRect() const
{
    return m_playerContentBoxRect;
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

void RemoteMediaPlayerProxy::mediaPlayerPlaybackStateChanged()
{
    m_cachedState.paused = m_player->paused();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::PlaybackStateChanged(m_cachedState.paused, timeUpdateData(*m_player, m_player->currentTime())), m_id);
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
    updateCachedVideoMetrics();
    updateCachedState();
    m_cachedState.hasAudio = m_player->hasAudio();
    m_cachedState.hasVideo = m_player->hasVideo();
    m_cachedState.hasClosedCaptions = m_player->hasClosedCaptions();
    m_cachedState.languageOfPrimaryAudioTrack = m_player->languageOfPrimaryAudioTrack();

    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::CharacteristicChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerVideoPlaybackConfigurationChanged()
{
    updateCachedVideoMetrics();
    updateCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::VideoPlaybackConfigurationChanged(m_cachedState.videoConfiguration), m_id);
}

bool RemoteMediaPlayerProxy::mediaPlayerRenderingCanBeAccelerated()
{
    return m_renderingCanBeAccelerated;
}

#if !PLATFORM(COCOA)
void RemoteMediaPlayerProxy::mediaPlayerRenderingModeChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RenderingModeChanged(), m_id);
}

void RemoteMediaPlayerProxy::requestHostingContextID(LayerHostingContextIDCallback&& completionHandler)
{
    completionHandler({ });
}
#endif

void RemoteMediaPlayerProxy::addRemoteAudioTrackProxy(WebCore::AudioTrackPrivate& track)
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return;

#if !RELEASE_LOG_DISABLED
    track.setLogger(mediaPlayerLogger(), mediaPlayerLogIdentifier());
#endif

    for (auto& audioTrack : m_audioTracks) {
        if (audioTrack.get() == track)
            return;
        if (audioTrack->id() == track.id()) {
            audioTrack = RemoteAudioTrackProxy::create(*m_manager->gpuConnectionToWebProcess(), track, m_id);
            return;
        }
    }

    m_audioTracks.append(RemoteAudioTrackProxy::create(*m_manager->gpuConnectionToWebProcess(), track, m_id));
}

void RemoteMediaPlayerProxy::audioTrackSetEnabled(TrackID trackId, bool enabled)
{
    for (auto& track : m_audioTracks) {
        if (track->id() == trackId) {
            track->setEnabled(enabled);
            return;
        }
    }
}

void RemoteMediaPlayerProxy::addRemoteVideoTrackProxy(WebCore::VideoTrackPrivate& track)
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return;

#if !RELEASE_LOG_DISABLED
    track.setLogger(mediaPlayerLogger(), mediaPlayerLogIdentifier());
#endif

    for (auto& videoTrack : m_videoTracks) {
        if (videoTrack.get() == track)
            return;
        if (videoTrack->id() == track.id()) {
            videoTrack = RemoteVideoTrackProxy::create(*m_manager->gpuConnectionToWebProcess(), track, m_id);
            return;
        }
    }

    m_videoTracks.append(RemoteVideoTrackProxy::create(*m_manager->gpuConnectionToWebProcess(), track, m_id));
}

void RemoteMediaPlayerProxy::videoTrackSetSelected(TrackID trackId, bool selected)
{
    for (auto& track : m_videoTracks) {
        if (track->id() == trackId) {
            track->setSelected(selected);
            return;
        }
    }
}

void RemoteMediaPlayerProxy::addRemoteTextTrackProxy(WebCore::InbandTextTrackPrivate& track)
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return;

#if !RELEASE_LOG_DISABLED
    track.setLogger(mediaPlayerLogger(), mediaPlayerLogIdentifier());
#endif

    for (auto& textTrack : m_textTracks) {
        if (textTrack.get() == track)
            return;
        if (textTrack->id() == track.id()) {
            textTrack = RemoteTextTrackProxy::create(*m_manager->gpuConnectionToWebProcess(), track, m_id);
            return;
        }
    }

    m_textTracks.append(RemoteTextTrackProxy::create(*m_manager->gpuConnectionToWebProcess(), track, m_id));
}

void RemoteMediaPlayerProxy::textTrackSetMode(TrackID trackId, WebCore::InbandTextTrackPrivate::Mode mode)
{
    for (auto& track : m_textTracks) {
        if (track->id() == trackId) {
            track->setMode(mode);
            return;
        }
    }
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddAudioTrack(WebCore::AudioTrackPrivate& track)
{
    addRemoteAudioTrackProxy(track);
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveAudioTrack(WebCore::AudioTrackPrivate& track)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoveRemoteAudioTrack(track.id()), m_id);
    m_audioTracks.removeFirstMatching([&track] (auto& current) {
        return track.id() == current->id();
    });
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddVideoTrack(WebCore::VideoTrackPrivate& track)
{
    addRemoteVideoTrackProxy(track);
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveVideoTrack(WebCore::VideoTrackPrivate& track)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoveRemoteVideoTrack(track.id()), m_id);
    m_videoTracks.removeFirstMatching([&track] (auto& current) {
        return track.id() == current->id();
    });
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddTextTrack(WebCore::InbandTextTrackPrivate& track)
{
    addRemoteTextTrackProxy(track);
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveTextTrack(WebCore::InbandTextTrackPrivate& track)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RemoveRemoteTextTrack(track.id()), m_id);
    m_textTracks.removeFirstMatching([&track] (auto& current) {
        return track.id() == current->id();
    });
}

void RemoteMediaPlayerProxy::textTrackRepresentationBoundsChanged(const IntRect&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerResourceNotSupported()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ResourceNotSupported(), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerSizeChanged()
{
    m_cachedState.naturalSize = m_player->naturalSize();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::SizeChanged(m_cachedState.naturalSize), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerActiveSourceBuffersChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::ActiveSourceBuffersChanged(), m_id);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RefPtr<ArrayBuffer> RemoteMediaPlayerProxy::mediaPlayerCachedKeyForKeyId(const String& keyId) const
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return nullptr;

    if (!m_legacySession)
        return nullptr;

    if (auto cdmSession = m_manager->gpuConnectionToWebProcess()->legacyCdmFactoryProxy().getSession(*m_legacySession))
        return cdmSession->getCachedKeyForKeyId(keyId);
    return nullptr;
}

void RemoteMediaPlayerProxy::mediaPlayerKeyNeeded(const SharedBuffer& message)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::MediaPlayerKeyNeeded(message.span()), m_id);
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::mediaPlayerInitializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::InitializationDataEncountered(initDataType, std::span<const uint8_t>(static_cast<uint8_t*>(initData->data()), initData->byteLength())), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerWaitingForKeyChanged()
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::WaitingForKeyChanged(m_player->waitingForKey()), m_id);
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
    sendCachedState();
}

void RemoteMediaPlayerProxy::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    m_player->setShouldPlayToPlaybackTarget(shouldPlay);
}

void RemoteMediaPlayerProxy::setWirelessPlaybackTarget(MediaPlaybackTargetContextSerialized&& targetContext)
{
    WTF::switchOn(targetContext.platformContext(), [&](WebCore::MediaPlaybackTargetContextMock&& context) {
        m_player->setWirelessPlaybackTarget(MediaPlaybackTargetMock::create(WTFMove(context)));
    }, [&](WebCore::MediaPlaybackTargetContextCocoa&& context) {
        m_player->setWirelessPlaybackTarget(MediaPlaybackTargetCocoa::create(WTFMove(context)));
    });
}
#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

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

Ref<PlatformMediaResourceLoader> RemoteMediaPlayerProxy::mediaPlayerCreateResourceLoader()
{
    return adoptRef(*new RemoteMediaResourceLoader(*this));
}

bool RemoteMediaPlayerProxy::doesHaveAttribute(const AtomString&, AtomString*) const
{
    notImplemented();
    return false;
}

#if PLATFORM(COCOA)
Vector<RefPtr<PlatformTextTrack>> RemoteMediaPlayerProxy::outOfBandTrackSources()
{
    return WTF::map(m_configuration.outOfBandTrackData, [](auto& data) -> RefPtr<PlatformTextTrack> {
        return PlatformTextTrack::create(WTFMove(data));
    });
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
    return m_fullscreenMode;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsVideoFullscreenStandby() const
{
    return m_videoFullscreenStandby;
}
#endif

bool RemoteMediaPlayerProxy::mediaPlayerShouldDisableSleep() const
{
    notImplemented();
    return false;
}

bool RemoteMediaPlayerProxy::mediaPlayerShouldCheckHardwareSupport() const
{
    return m_shouldCheckHardwareSupport;
}

WebCore::PlatformVideoTarget RemoteMediaPlayerProxy::mediaPlayerVideoTarget() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_manager)
        return m_manager->videoTargetForMediaElementIdentifier(m_clientIdentifier);
#endif
    return nullptr;
}

void RemoteMediaPlayerProxy::startUpdateCachedStateMessageTimer()
{
    static const Seconds lessFrequentTimeupdateEventFrequency { 2000_ms };
    static const Seconds moreFrequentTimeupdateEventFrequency { 250_ms };

    if (m_updateCachedStateMessageTimer.isActive())
        return;

    auto frequency = m_observingTimeChanges ? lessFrequentTimeupdateEventFrequency : moreFrequentTimeupdateEventFrequency;
    m_updateCachedStateMessageTimer.startRepeating(frequency);
}

void RemoteMediaPlayerProxy::timerFired()
{
    sendCachedState();
}

void RemoteMediaPlayerProxy::currentTimeChanged(const MediaTime& mediaTime)
{
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::CurrentTimeChanged(timeUpdateData(*m_player, mediaTime)), m_id);
}

void RemoteMediaPlayerProxy::videoFrameForCurrentTimeIfChanged(CompletionHandler<void(std::optional<RemoteVideoFrameProxy::Properties>&&, bool)>&& completionHandler)
{
    std::optional<RemoteVideoFrameProxy::Properties> result;
    bool changed = false;
    RefPtr<WebCore::VideoFrame> videoFrame;
    if (m_player)
        videoFrame = m_player->videoFrameForCurrentTime();
    if (m_videoFrameForCurrentTime != videoFrame) {
        m_videoFrameForCurrentTime = videoFrame;
        changed = true;
        if (videoFrame)
            result = m_videoFrameObjectHeap->add(videoFrame.releaseNonNull());
    }
    completionHandler(WTFMove(result), changed);
}

void RemoteMediaPlayerProxy::setShouldDisableHDR(bool shouldDisable)
{
    if (m_configuration.shouldDisableHDR == shouldDisable)
        return;

    m_configuration.shouldDisableHDR = shouldDisable;
    if (m_player)
        m_player->setShouldDisableHDR(shouldDisable);
}


void RemoteMediaPlayerProxy::updateCachedState(bool forceCurrentTimeUpdate)
{
    if (!m_observingTimeChanges || forceCurrentTimeUpdate)
        currentTimeChanged(m_player->currentTime());

    m_cachedState.paused = m_player->paused();
    maybeUpdateCachedVideoMetrics();
    if (m_bufferedChanged) {
        m_bufferedChanged = false;
        if (m_engineIdentifier != MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE
            && m_engineIdentifier != MediaPlayerEnums::MediaEngineIdentifier::MockMSE) {
            m_cachedState.bufferedRanges = m_player->buffered();
        }
    }
}

void RemoteMediaPlayerProxy::sendCachedState()
{
    updateCachedState();
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::UpdateCachedState(m_cachedState), m_id);
    m_cachedState.bufferedRanges = std::nullopt;
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::setLegacyCDMSession(std::optional<RemoteLegacyCDMSessionIdentifier>&& instanceId)
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return;

    if (m_legacySession == instanceId)
        return;

    if (m_legacySession) {
        if (auto cdmSession = m_manager->gpuConnectionToWebProcess()->legacyCdmFactoryProxy().getSession(*m_legacySession)) {
            m_player->setCDMSession(nullptr);
            cdmSession->setPlayer(nullptr);
        }
    }

    m_legacySession = instanceId;

    if (m_legacySession) {
        if (auto cdmSession = m_manager->gpuConnectionToWebProcess()->legacyCdmFactoryProxy().getSession(*m_legacySession)) {
            m_player->setCDMSession(cdmSession->session());
            cdmSession->setPlayer(*this);
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
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return;

    if (auto* instanceProxy = m_manager->gpuConnectionToWebProcess()->cdmFactoryProxy().getInstance(instanceId))
        m_player->cdmInstanceAttached(instanceProxy->instance());
}

void RemoteMediaPlayerProxy::cdmInstanceDetached(RemoteCDMInstanceIdentifier&& instanceId)
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return;

    if (auto* instanceProxy = m_manager->gpuConnectionToWebProcess()->cdmFactoryProxy().getInstance(instanceId))
        m_player->cdmInstanceDetached(instanceProxy->instance());
}

void RemoteMediaPlayerProxy::attemptToDecryptWithInstance(RemoteCDMInstanceIdentifier&& instanceId)
{
    ASSERT(m_manager && m_manager->gpuConnectionToWebProcess());
    if (!m_manager || !m_manager->gpuConnectionToWebProcess())
        return;

    if (auto* instanceProxy = m_manager->gpuConnectionToWebProcess()->cdmFactoryProxy().getInstance(instanceId))
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
    m_player->notifyTrackModeChanged();
}

void RemoteMediaPlayerProxy::tracksChanged()
{
    m_player->tracksChanged();
}

void RemoteMediaPlayerProxy::performTaskAtTime(const MediaTime& taskTime, PerformTaskAtTimeCompletionHandler&& completionHandler)
{
    if (m_performTaskAtTimeCompletionHandler) {
        // A media player is only expected to track one pending task-at-time at once (e.g. see
        // MediaPlayerPrivateAVFoundationObjC::performTaskAtMediaTime), so cancel the existing
        // CompletionHandler.
        auto handler = WTFMove(m_performTaskAtTimeCompletionHandler);
        handler(std::nullopt);
    }

    auto currentTime = m_player->currentTime();
    if (taskTime <= currentTime) {
        completionHandler(currentTime);
        return;
    }

    m_performTaskAtTimeCompletionHandler = WTFMove(completionHandler);
    m_player->performTaskAtTime([this, weakThis = WeakPtr { *this }]() mutable {
        if (!weakThis || !m_performTaskAtTimeCompletionHandler)
            return;

        auto completionHandler = WTFMove(m_performTaskAtTimeCompletionHandler);
        completionHandler(m_player->currentTime());
    }, taskTime);
}

void RemoteMediaPlayerProxy::isCrossOrigin(WebCore::SecurityOriginData originData, CompletionHandler<void(std::optional<bool>)>&& completionHandler)
{
    completionHandler(m_player->isCrossOrigin(originData.securityOrigin()));
}

void RemoteMediaPlayerProxy::setVideoPlaybackMetricsUpdateInterval(double interval)
{
    static const Seconds metricsAdvanceUpdate = 0.25_s;
    ALWAYS_LOG(LOGIDENTIFIER, interval);

    updateCachedVideoMetrics();
    m_videoPlaybackMetricsUpdateInterval = Seconds(interval);
    m_nextPlaybackQualityMetricsUpdateTime = MonotonicTime::now() + Seconds(interval) - metricsAdvanceUpdate;
}

void RemoteMediaPlayerProxy::maybeUpdateCachedVideoMetrics()
{
    if (m_cachedState.paused || !m_videoPlaybackMetricsUpdateInterval || MonotonicTime::now() < m_nextPlaybackQualityMetricsUpdateTime || m_hasPlaybackMetricsUpdatePending)
        return;

    updateCachedVideoMetrics();
}

void RemoteMediaPlayerProxy::updateCachedVideoMetrics()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_nextPlaybackQualityMetricsUpdateTime = MonotonicTime::now() + m_videoPlaybackMetricsUpdateInterval;
    if (m_hasPlaybackMetricsUpdatePending)
        return;
    m_hasPlaybackMetricsUpdatePending = true;
    m_player->asyncVideoPlaybackQualityMetrics()->whenSettled(RunLoop::current(), [weakThis = WeakPtr { *this }, this](auto&& result) {
        if (!weakThis)
            return;
        if (result) {
            m_cachedState.videoMetrics = *result;
            m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::UpdatePlaybackQualityMetrics(WTFMove(*result)), m_id);
        } else
            m_cachedState.videoMetrics.reset();
        m_hasPlaybackMetricsUpdatePending = false;
    });
}

void RemoteMediaPlayerProxy::setPreferredDynamicRangeMode(DynamicRangeMode mode)
{
    if (m_player)
        m_player->setPreferredDynamicRangeMode(mode);
}

void RemoteMediaPlayerProxy::createAudioSourceProvider()
{
#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
    if (!m_player)
        return;

    auto* provider = dynamicDowncast<AudioSourceProviderAVFObjC>(m_player->audioSourceProvider());
    if (!provider)
        return;

    m_remoteAudioSourceProvider = RemoteAudioSourceProviderProxy::create(m_id, m_webProcessConnection.copyRef(), *provider);
#endif
}

void RemoteMediaPlayerProxy::setShouldEnableAudioSourceProvider(bool shouldEnable)
{
#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
    if (auto* provider = m_player->audioSourceProvider())
        provider->setClient(shouldEnable ? m_remoteAudioSourceProvider.get() : nullptr);
#endif
}

void RemoteMediaPlayerProxy::playAtHostTime(MonotonicTime time)
{
    if (m_player)
        m_player->playAtHostTime(time);
}

void RemoteMediaPlayerProxy::pauseAtHostTime(MonotonicTime time)
{
    if (m_player)
        m_player->pauseAtHostTime(time);
}

void RemoteMediaPlayerProxy::startVideoFrameMetadataGathering()
{
    if (m_player)
        m_player->startVideoFrameMetadataGathering();
}

void RemoteMediaPlayerProxy::stopVideoFrameMetadataGathering()
{
    if (m_player)
        m_player->stopVideoFrameMetadataGathering();
}

void RemoteMediaPlayerProxy::playerContentBoxRectChanged(const WebCore::LayoutRect& contentRect)
{
    if (m_playerContentBoxRect == contentRect)
        return;

    m_playerContentBoxRect = contentRect;

    if (m_player)
        m_player->playerContentBoxRectChanged(contentRect);
}

void RemoteMediaPlayerProxy::setShouldCheckHardwareSupport(bool value)
{
    m_player->setShouldCheckHardwareSupport(value);
    m_shouldCheckHardwareSupport = value;
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void RemoteMediaPlayerProxy::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    m_player->setDefaultSpatialTrackingLabel(defaultSpatialTrackingLabel);
}

void RemoteMediaPlayerProxy::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    m_player->setSpatialTrackingLabel(spatialTrackingLabel);
}
#endif

void RemoteMediaPlayerProxy::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
    m_player->setInFullscreenOrPictureInPicture(isInFullscreenOrPictureInPicture);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RemoteMediaPlayerProxy::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
