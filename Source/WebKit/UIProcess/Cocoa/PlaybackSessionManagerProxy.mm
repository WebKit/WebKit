/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PlaybackSessionManagerProxy.h"

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "MessageSenderInlines.h"
#import "PlaybackSessionInterfaceLMK.h"
#import "PlaybackSessionManagerMessages.h"
#import "PlaybackSessionManagerProxyMessages.h"
#import "VideoPresentationManagerProxy.h"
#import "VideoReceiverEndpointMessage.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <Foundation/Foundation.h>
#import <WebCore/NullPlaybackSessionInterface.h>
#import <WebCore/PlaybackSessionInterfaceAVKit.h>
#import <WebCore/PlaybackSessionInterfaceAVKitLegacy.h>
#import <WebCore/PlaybackSessionInterfaceMac.h>
#import <WebCore/PlaybackSessionInterfaceTVOS.h>
#if HAVE(PIP_SKIP_PREROLL)
#import <WebCore/VideoPresentationInterfaceMac.h>
#endif
#if ENABLE(LINEAR_MEDIA_PLAYER)
#import <WebCore/VideoPresentationInterfaceIOS.h>
#endif
#import <wtf/LoggerHelper.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/darwin/DispatchExtras.h>

namespace WebKit {
using namespace WebCore;

#pragma mark - PlaybackSessionModelContext

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionModelContext);

PlaybackSessionModelContext::PlaybackSessionModelContext(PlaybackSessionManagerProxy& manager, PlaybackSessionContextIdentifier contextId)
    : m_manager(manager)
    , m_contextId(contextId)
    , m_prefersAutoDimming([[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitPrefersFullScreenDimming"])
{
}

PlaybackSessionModelContext::~PlaybackSessionModelContext()
{
    invalidate();
}

void PlaybackSessionModelContext::addClient(PlaybackSessionModelClient& client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.add(client);
}

void PlaybackSessionModelContext::removeClient(PlaybackSessionModelClient& client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(client);
}

void PlaybackSessionModelContext::sendRemoteCommand(WebCore::PlatformMediaSession::RemoteControlCommandType command, const WebCore::PlatformMediaSession::RemoteCommandArgument& argument)
{
    if (RefPtr manager = m_manager.get())
        manager->sendRemoteCommand(m_contextId, command, argument);
}

void PlaybackSessionModelContext::setVideoReceiverEndpoint(const WebCore::VideoReceiverEndpoint& endpoint)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_videoReceiverEndpoint.get() == endpoint.get())
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (m_manager && m_videoReceiverEndpoint && m_videoReceiverEndpointIdentifier)
        m_manager->setVideoReceiverEndpoint(m_contextId, nullptr, *m_videoReceiverEndpointIdentifier);

    m_videoReceiverEndpoint = endpoint;
    m_videoReceiverEndpointIdentifier = endpoint ? std::make_optional(VideoReceiverEndpointIdentifier::generate()) : std::nullopt;

    if (m_manager && m_videoReceiverEndpoint && m_videoReceiverEndpointIdentifier)
        m_manager->setVideoReceiverEndpoint(m_contextId, m_videoReceiverEndpoint, *m_videoReceiverEndpointIdentifier);
#else
    UNUSED_PARAM(endpoint);
#endif
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void PlaybackSessionModelContext::setSpatialTrackingLabel(const String& label)
{
    if (RefPtr manager = m_manager.get())
        manager->setSpatialTrackingLabel(m_contextId, label);
}
#endif

void PlaybackSessionModelContext::addNowPlayingMetadataObserver(const WebCore::NowPlayingMetadataObserver& nowPlayingInfo)
{
    if (RefPtr manager = m_manager.get())
        manager->addNowPlayingMetadataObserver(m_contextId, nowPlayingInfo);
}

void PlaybackSessionModelContext::removeNowPlayingMetadataObserver(const WebCore::NowPlayingMetadataObserver& nowPlayingInfo)
{
    if (RefPtr manager = m_manager.get())
        manager->removeNowPlayingMetadataObserver(m_contextId, nowPlayingInfo);
}

void PlaybackSessionModelContext::setSoundStageSize(WebCore::AudioSessionSoundStageSize size)
{
    if (m_soundStageSize == size)
        return;

    m_soundStageSize = size;
    if (RefPtr manager = m_manager.get())
        manager->setSoundStageSize(m_contextId, size);
}

void PlaybackSessionModelContext::play()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->play(m_contextId);
}

void PlaybackSessionModelContext::pause()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->pause(m_contextId);
}

void PlaybackSessionModelContext::togglePlayState()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->togglePlayState(m_contextId);
}

void PlaybackSessionModelContext::beginScrubbing()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->beginScrubbing(m_contextId);

    m_isScrubbing = true;
}

void PlaybackSessionModelContext::endScrubbing()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->endScrubbing(m_contextId);

    m_isScrubbing = false;
    m_playbackStartedTimeNeedsUpdate = isPlaying();
}

#if HAVE(PIP_SKIP_PREROLL)
void PlaybackSessionModelContext::skipAd()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->skipAd(m_contextId);
}
#endif

void PlaybackSessionModelContext::seekToTime(double time, double toleranceBefore, double toleranceAfter)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, time, ", toleranceBefore: ", toleranceBefore, ", toleranceAfter: ", toleranceAfter);
    if (RefPtr manager = m_manager.get())
        manager->seekToTime(m_contextId, time, toleranceBefore, toleranceAfter);
}

void PlaybackSessionModelContext::fastSeek(double time)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, time);
    if (RefPtr manager = m_manager.get())
        manager->fastSeek(m_contextId, time);
}

void PlaybackSessionModelContext::beginScanningForward()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->beginScanningForward(m_contextId);
}

void PlaybackSessionModelContext::beginScanningBackward()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->beginScanningBackward(m_contextId);
}

void PlaybackSessionModelContext::endScanning()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->endScanning(m_contextId);
}

void PlaybackSessionModelContext::setDefaultPlaybackRate(double defaultPlaybackRate)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, defaultPlaybackRate);
    if (RefPtr manager = m_manager.get())
        manager->setDefaultPlaybackRate(m_contextId, defaultPlaybackRate);
}

void PlaybackSessionModelContext::setPlaybackRate(double playbackRate)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, playbackRate);
    if (RefPtr manager = m_manager.get())
        manager->setPlaybackRate(m_contextId, playbackRate);
}

void PlaybackSessionModelContext::selectAudioMediaOption(uint64_t index)
{
    if (m_audioMediaSelectedIndex == index)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, index);
    if (RefPtr manager = m_manager.get())
        manager->selectAudioMediaOption(m_contextId, index);
}

void PlaybackSessionModelContext::selectLegibleMediaOption(uint64_t index)
{
    if (m_legibleMediaSelectedIndex == index)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, index);
    if (RefPtr manager = m_manager.get())
        manager->selectLegibleMediaOption(m_contextId, index);
}

void PlaybackSessionModelContext::togglePictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->togglePictureInPicture(m_contextId);
}

void PlaybackSessionModelContext::enterFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->enterFullscreen(m_contextId);
}

void PlaybackSessionModelContext::setPlayerIdentifierForVideoElement()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->setPlayerIdentifierForVideoElement(m_contextId);
}

void PlaybackSessionModelContext::exitFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->exitFullscreen(m_contextId);
}

void PlaybackSessionModelContext::enterInWindowFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->enterInWindow(m_contextId);
}

void PlaybackSessionModelContext::exitInWindowFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->exitInWindow(m_contextId);
}

void PlaybackSessionModelContext::toggleMuted()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->toggleMuted(m_contextId);
}

void PlaybackSessionModelContext::setMuted(bool muted)
{
    if (muted == m_muted)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, muted);
    if (RefPtr manager = m_manager.get())
        manager->setMuted(m_contextId, muted);
}

void PlaybackSessionModelContext::setVolume(double volume)
{
    if (volume == m_volume)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, volume);
    if (RefPtr manager = m_manager.get())
        manager->setVolume(m_contextId, volume);
}

void PlaybackSessionModelContext::setPlayingOnSecondScreen(bool value)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, value);
    if (RefPtr manager = m_manager.get())
        manager->setPlayingOnSecondScreen(m_contextId, value);
}

void PlaybackSessionModelContext::setPrefersAutoDimming(bool value)
{
    if (m_prefersAutoDimming != value) {
        m_prefersAutoDimming = value;
        [[NSUserDefaults standardUserDefaults] setBool:value forKey:@"WebKitPrefersFullScreenDimming"];
    }
}

void PlaybackSessionModelContext::playbackStartedTimeChanged(double playbackStartedTime)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, playbackStartedTime);
    m_playbackStartedTime = playbackStartedTime;
    m_playbackStartedTimeNeedsUpdate = false;
}

void PlaybackSessionModelContext::durationChanged(double duration)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, duration);
    m_duration = duration;
    for (CheckedRef client : m_clients)
        client->durationChanged(duration);
}

void PlaybackSessionModelContext::currentTimeChanged(double currentTime)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, currentTime);
    m_currentTime = currentTime;
    auto anchorTime = [[NSProcessInfo processInfo] systemUptime];
    if (m_playbackStartedTimeNeedsUpdate)
        playbackStartedTimeChanged(currentTime);

    for (CheckedRef client : m_clients)
        client->currentTimeChanged(currentTime, anchorTime);
}

void PlaybackSessionModelContext::bufferedTimeChanged(double bufferedTime)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, bufferedTime);
    m_bufferedTime = bufferedTime;
    for (CheckedRef client : m_clients)
        client->bufferedTimeChanged(bufferedTime);
}

void PlaybackSessionModelContext::rateChanged(OptionSet<WebCore::PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double defaultPlaybackRate)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, playbackRate, ", defaultPlaybackRate", defaultPlaybackRate);
    m_playbackState = playbackState;
    m_playbackRate = playbackRate;
    m_defaultPlaybackRate = defaultPlaybackRate;
    for (CheckedRef client : m_clients)
        client->rateChanged(m_playbackState, m_playbackRate, m_defaultPlaybackRate);
}

void PlaybackSessionModelContext::seekableRangesChanged(const WebCore::PlatformTimeRanges& seekableRanges, double lastModifiedTime, double liveUpdateInterval)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, seekableRanges);
    m_seekableRanges = seekableRanges;
    m_seekableTimeRangesLastModifiedTime = lastModifiedTime;
    m_liveUpdateInterval = liveUpdateInterval;
    for (CheckedRef client : m_clients)
        client->seekableRangesChanged(seekableRanges, lastModifiedTime, liveUpdateInterval);
}

void PlaybackSessionModelContext::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    m_canPlayFastReverse = canPlayFastReverse;
    for (CheckedRef client : m_clients)
        client->canPlayFastReverseChanged(canPlayFastReverse);
}

void PlaybackSessionModelContext::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& audioMediaSelectionOptions, uint64_t audioMediaSelectedIndex)
{
    m_audioMediaSelectionOptions = audioMediaSelectionOptions;
    m_audioMediaSelectedIndex = audioMediaSelectedIndex;
    for (CheckedRef client : m_clients)
        client->audioMediaSelectionOptionsChanged(audioMediaSelectionOptions, audioMediaSelectedIndex);
}

void PlaybackSessionModelContext::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& legibleMediaSelectionOptions, uint64_t legibleMediaSelectedIndex)
{
    m_legibleMediaSelectionOptions = legibleMediaSelectionOptions;
    m_legibleMediaSelectedIndex = legibleMediaSelectedIndex;

    for (CheckedRef client : m_clients)
        client->legibleMediaSelectionOptionsChanged(legibleMediaSelectionOptions, legibleMediaSelectedIndex);
}

void PlaybackSessionModelContext::audioMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    m_audioMediaSelectedIndex = selectedIndex;

    for (CheckedRef client : m_clients)
        client->audioMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionModelContext::legibleMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    m_legibleMediaSelectedIndex = selectedIndex;

    for (CheckedRef client : m_clients)
        client->legibleMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionModelContext::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType type, const String& localizedName)
{
    m_externalPlaybackEnabled = enabled;
    m_externalPlaybackTargetType = type;
    m_externalPlaybackLocalizedDeviceName = localizedName;

    for (CheckedRef client : m_clients)
        client->externalPlaybackChanged(enabled, type, localizedName);
}

void PlaybackSessionModelContext::wirelessVideoPlaybackDisabledChanged(bool wirelessVideoPlaybackDisabled)
{
    m_wirelessVideoPlaybackDisabled = wirelessVideoPlaybackDisabled;
    for (CheckedRef client : m_clients)
        client->wirelessVideoPlaybackDisabledChanged(wirelessVideoPlaybackDisabled);
}

void PlaybackSessionModelContext::mutedChanged(bool muted)
{
    m_muted = muted;
    for (CheckedRef client : m_clients)
        client->mutedChanged(muted);
}

void PlaybackSessionModelContext::volumeChanged(double volume)
{
    m_volume = volume;
    for (CheckedRef client : m_clients)
        client->volumeChanged(volume);
}

void PlaybackSessionModelContext::pictureInPictureSupportedChanged(bool supported)
{
    m_pictureInPictureSupported = supported;
    for (CheckedRef client : m_clients)
        client->isPictureInPictureSupportedChanged(supported);
}

void PlaybackSessionModelContext::pictureInPictureActiveChanged(bool active)
{
    m_pictureInPictureActive = active;
    for (CheckedRef client : m_clients)
        client->pictureInPictureActiveChanged(active);
}

void PlaybackSessionModelContext::isInWindowFullscreenActiveChanged(bool active)
{
    m_isInWindowFullscreenActive = active;
    for (CheckedRef client : m_clients)
        client->isInWindowFullscreenActiveChanged(active);
}

#if HAVE(PIP_SKIP_PREROLL)
void PlaybackSessionModelContext::canSkipAdChanged(bool value)
{
    for (CheckedRef client : m_clients)
        client->canSkipAdChanged(value);
}
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
void PlaybackSessionModelContext::supportsLinearMediaPlayerChanged(bool supportsLinearMediaPlayer)
{
    if (m_supportsLinearMediaPlayer == supportsLinearMediaPlayer)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, supportsLinearMediaPlayer);
    m_supportsLinearMediaPlayer = supportsLinearMediaPlayer;

    for (CheckedRef client : m_clients)
        client->supportsLinearMediaPlayerChanged(supportsLinearMediaPlayer);

    if (RefPtr manager = m_manager.get())
        manager->updateVideoControlsManager(m_contextId);
}
#endif

void PlaybackSessionModelContext::immersiveVideoMetadataChanged(const std::optional<WebCore::ImmersiveVideoMetadata>& metadata)
{
    if (m_immersiveVideoMetadata == metadata)
        return;
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, metadata);
    m_immersiveVideoMetadata = metadata;

    for (CheckedRef client : m_clients)
        client->immersiveVideoMetadataChanged(m_immersiveVideoMetadata);
}

void PlaybackSessionModelContext::invalidate()
{
    setVideoReceiverEndpoint(nullptr);
}

void PlaybackSessionModelContext::swapVideoReceiverEndpointsWith(PlaybackSessionModelContext& otherModel)
{
    std::swap(m_videoReceiverEndpoint, otherModel.m_videoReceiverEndpoint);
    std::swap(m_videoReceiverEndpointIdentifier, otherModel.m_videoReceiverEndpointIdentifier);
}

#if !RELEASE_LOG_DISABLED
const Logger* PlaybackSessionModelContext::loggerPtr() const
{
    return m_manager ? &m_manager->logger() : nullptr;
}

WTFLogChannel& PlaybackSessionModelContext::logChannel() const
{
    return WebKit2LogMedia;
}
#endif

#pragma mark - PlaybackSessionManagerProxy

Ref<PlaybackSessionManagerProxy> PlaybackSessionManagerProxy::create(WebPageProxy& page)
{
    return adoptRef(*new PlaybackSessionManagerProxy(page));
}

PlaybackSessionManagerProxy::PlaybackSessionManagerProxy(WebPageProxy& page)
    : m_page(page)
#if !RELEASE_LOG_DISABLED
    , m_logger(page.logger())
    , m_logIdentifier(page.logIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);

    RefPtr protectedPage = m_page.get();
    protect(protectedPage->legacyMainFrameProcess())->addMessageReceiver(Messages::PlaybackSessionManagerProxy::messageReceiverName(), protectedPage->webPageIDInMainFrameProcess(), *this);
}

PlaybackSessionManagerProxy::~PlaybackSessionManagerProxy()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (!m_page)
        return;
    invalidate();
}

void PlaybackSessionManagerProxy::invalidate()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr page = m_page.get()) {
        protect(page->legacyMainFrameProcess())->removeMessageReceiver(Messages::PlaybackSessionManagerProxy::messageReceiverName(), page->webPageIDInMainFrameProcess());
        m_page = nullptr;
    }

    auto contextMap = WTF::move(m_contextMap);
    m_clientCounts.clear();

    for (auto& [model, interface] : contextMap.values()) {
        model->invalidate();
        interface->invalidate();
    }
}

template <typename Message>
void PlaybackSessionManagerProxy::sendToWebProcess(PlaybackSessionContextIdentifier contextId, Message&& message)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    RefPtr process = WebProcessProxy::processForIdentifier(contextId.processIdentifier());
    if (!process)
        return;
    process->send(std::forward<Message>(message), page->webPageIDInProcess(*process));
}

static Ref<PlatformPlaybackSessionInterface> playbackSessionInterface(WebPageProxy& page, PlaybackSessionModel& model)
{
#if HAVE(AVKIT_CONTENT_SOURCE)
    if (page.preferences().isAVKitContentSourceEnabled())
        return PlaybackSessionInterfaceAVKit::create(model);
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (page.preferences().linearMediaPlayerEnabled())
        return PlaybackSessionInterfaceLMK::create(model);
#endif

#if PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(VISION)
    return PlaybackSessionInterfaceAVKitLegacy::create(model);
#elif PLATFORM(APPLETV)
    return PlaybackSessionInterfaceTVOS::create(model);
#else
    return PlatformPlaybackSessionInterface::create(model);
#endif
}

PlaybackSessionManagerProxy::ModelInterfaceTuple PlaybackSessionManagerProxy::createModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    Ref page = *m_page;
    Ref model = PlaybackSessionModelContext::create(*this, contextId);
    Ref interface = playbackSessionInterface(page.get(), model.get());

    return std::make_tuple(WTF::move(model), WTF::move(interface));
}

const PlaybackSessionManagerProxy::ModelInterfaceTuple& PlaybackSessionManagerProxy::ensureModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_contextMap.ensure(contextId, [&] {
        return createModelAndInterface(contextId);
    });
    return addResult.iterator->value;
}

Ref<PlaybackSessionModelContext> PlaybackSessionManagerProxy::ensureModel(PlaybackSessionContextIdentifier contextId)
{
    return std::get<0>(ensureModelAndInterface(contextId));
}

PlatformPlaybackSessionInterface& PlaybackSessionManagerProxy::ensureInterface(PlaybackSessionContextIdentifier contextId)
{
    return std::get<1>(ensureModelAndInterface(contextId)).get();
}

Ref<PlatformPlaybackSessionInterface> PlaybackSessionManagerProxy::ensureProtectedInterface(PlaybackSessionContextIdentifier contextId)
{
    return ensureInterface(contextId);
}

void PlaybackSessionManagerProxy::addClientForContext(PlaybackSessionContextIdentifier contextId)
{
    m_clientCounts.add(contextId);
}

void PlaybackSessionManagerProxy::removeClientForContext(PlaybackSessionContextIdentifier contextId)
{
    if (!m_clientCounts.remove(contextId))
        return;

    ensureProtectedInterface(contextId)->invalidate();
    m_contextMap.remove(contextId);
}

#pragma mark Messages from PlaybackSessionManager

void PlaybackSessionManagerProxy::setUpPlaybackControlsManagerWithID(PlaybackSessionContextIdentifier contextId, bool isVideo)
{
    if (m_controlsManagerContextId == contextId)
        return;

    if (m_controlsManagerContextId)
        removeClientForContext(*m_controlsManagerContextId);

    m_controlsManagerContextId = contextId;
    m_controlsManagerContextIsVideo = isVideo;
    ensureProtectedInterface(*m_controlsManagerContextId)->ensureControlsManager();
    addClientForContext(*m_controlsManagerContextId);

    if (RefPtr page = m_page.get())
        page->videoControlsManagerDidChange();
}

void PlaybackSessionManagerProxy::clearPlaybackControlsManager()
{
    if (!m_controlsManagerContextId)
        return;

#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (RefPtr page = m_page.get()) {
        if (RefPtr videoPresentationManager = page->videoPresentationManager()) {
            if (RefPtr controlsManagerInterface = videoPresentationManager->controlsManagerInterface())
                controlsManagerInterface->cleanupExternalPlayback();
        }
    }
#endif

    removeClientForContext(*m_controlsManagerContextId);
    m_controlsManagerContextId = std::nullopt;
    m_controlsManagerContextIsVideo = false;

    if (RefPtr page = m_page.get())
        page->videoControlsManagerDidChange();
}

void PlaybackSessionManagerProxy::swapFullscreenModes(PlaybackSessionContextIdentifier firstContextId, PlaybackSessionContextIdentifier secondContextId)
{
    auto firstModelInterface = ensureModelAndInterface(firstContextId);
    auto secondModelInterface = ensureModelAndInterface(secondContextId);

    ALWAYS_LOG(LOGIDENTIFIER, "swapping from media element ", firstContextId.loggingString(), " to media element ", secondContextId.loggingString());

    Ref firstInterface = WTF::move(get<1>(firstModelInterface));
    Ref secondInterface = WTF::move(get<1>(secondModelInterface));
    firstInterface->swapFullscreenModesWith(secondInterface);

    Ref firstModel = WTF::move(get<0>(firstModelInterface));
    Ref secondModel = WTF::move(get<0>(secondModelInterface));
    firstModel->swapVideoReceiverEndpointsWith(secondModel);

    swapVideoReceiverEndpoints(firstContextId, secondContextId);

    if (RefPtr page = m_page.get()) {
        if (RefPtr videoPresentationManager = page->videoPresentationManager())
            videoPresentationManager->swapFullscreenModes(firstContextId, secondContextId);
    }
}

void PlaybackSessionManagerProxy::currentTimeChanged(PlaybackSessionContextIdentifier contextId, double currentTime, double hostTime)
{
    ensureModel(contextId)->currentTimeChanged(currentTime);
}

void PlaybackSessionManagerProxy::bufferedTimeChanged(PlaybackSessionContextIdentifier contextId, double bufferedTime)
{
    ensureModel(contextId)->bufferedTimeChanged(bufferedTime);
}

void PlaybackSessionManagerProxy::seekableRangesVectorChanged(PlaybackSessionContextIdentifier contextId, const WebCore::PlatformTimeRanges& timeRanges, double lastModifiedTime, double liveUpdateInterval)
{
    ensureModel(contextId)->seekableRangesChanged(timeRanges, lastModifiedTime, liveUpdateInterval);
}

void PlaybackSessionManagerProxy::canPlayFastReverseChanged(PlaybackSessionContextIdentifier contextId, bool value)
{
    ensureModel(contextId)->canPlayFastReverseChanged(value);
}

void PlaybackSessionManagerProxy::audioMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier contextId, Vector<MediaSelectionOption> options, uint64_t selectedIndex)
{
    ensureModel(contextId)->audioMediaSelectionOptionsChanged(options, selectedIndex);
}

void PlaybackSessionManagerProxy::legibleMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier contextId, Vector<MediaSelectionOption> options, uint64_t selectedIndex)
{
    ensureModel(contextId)->legibleMediaSelectionOptionsChanged(options, selectedIndex);
}

void PlaybackSessionManagerProxy::audioMediaSelectionIndexChanged(PlaybackSessionContextIdentifier contextId, uint64_t selectedIndex)
{
    ensureModel(contextId)->audioMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionManagerProxy::legibleMediaSelectionIndexChanged(PlaybackSessionContextIdentifier contextId, uint64_t selectedIndex)
{
    ensureModel(contextId)->legibleMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionManagerProxy::externalPlaybackPropertiesChanged(PlaybackSessionContextIdentifier contextId, bool enabled, WebCore::PlaybackSessionModel::ExternalPlaybackTargetType targetType, String localizedDeviceName)
{
    ensureModel(contextId)->externalPlaybackChanged(enabled, targetType, localizedDeviceName);
}

void PlaybackSessionManagerProxy::wirelessVideoPlaybackDisabledChanged(PlaybackSessionContextIdentifier contextId, bool disabled)
{
    ensureModel(contextId)->wirelessVideoPlaybackDisabledChanged(disabled);
}

void PlaybackSessionManagerProxy::mutedChanged(PlaybackSessionContextIdentifier contextId, bool muted)
{
    ensureModel(contextId)->mutedChanged(muted);
}

void PlaybackSessionManagerProxy::volumeChanged(PlaybackSessionContextIdentifier contextId, double volume)
{
    ensureModel(contextId)->volumeChanged(volume);
}

void PlaybackSessionManagerProxy::durationChanged(PlaybackSessionContextIdentifier contextId, double duration)
{
    ensureModel(contextId)->durationChanged(duration);
}

void PlaybackSessionManagerProxy::playbackStartedTimeChanged(PlaybackSessionContextIdentifier contextId, double playbackStartedTime)
{
    ensureModel(contextId)->playbackStartedTimeChanged(playbackStartedTime);
}

void PlaybackSessionManagerProxy::rateChanged(PlaybackSessionContextIdentifier contextId, OptionSet<WebCore::PlaybackSessionModel::PlaybackState> playbackState, double rate, double defaultPlaybackRate)
{
    ensureModel(contextId)->rateChanged(playbackState, rate, defaultPlaybackRate);
}

void PlaybackSessionManagerProxy::pictureInPictureSupportedChanged(PlaybackSessionContextIdentifier contextId, bool supported)
{
    ensureModel(contextId)->pictureInPictureSupportedChanged(supported);
}

void PlaybackSessionManagerProxy::isInWindowFullscreenActiveChanged(PlaybackSessionContextIdentifier contextId, bool active)
{
    ensureModel(contextId)->isInWindowFullscreenActiveChanged(active);
}

#if HAVE(PIP_SKIP_PREROLL)
void PlaybackSessionManagerProxy::canSkipAdChanged(PlaybackSessionContextIdentifier contextId, bool value)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr videoPresentationManager = page->videoPresentationManager();
    if (!videoPresentationManager)
        return;

    RefPtr interface = videoPresentationManager->controlsManagerInterface();
    if (!interface)
        return;

    interface->canSkipAdChanged(value);
}
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
void PlaybackSessionManagerProxy::supportsLinearMediaPlayerChanged(PlaybackSessionContextIdentifier contextId, bool supportsLinearMediaPlayer)
{
    ensureModel(contextId)->supportsLinearMediaPlayerChanged(supportsLinearMediaPlayer);
}
#endif

void PlaybackSessionManagerProxy::immersiveVideoMetadataChanged(PlaybackSessionContextIdentifier contextId, const std::optional<WebCore::ImmersiveVideoMetadata>& metadata)
{
    ensureModel(contextId)->immersiveVideoMetadataChanged(metadata);
}

void PlaybackSessionManagerProxy::handleControlledElementIDResponse(PlaybackSessionContextIdentifier contextId, String identifier) const
{
#if PLATFORM(MAC)
    if (RefPtr page = m_page.get(); contextId == m_controlsManagerContextId)
        page->handleControlledElementIDResponse(identifier);
#else
    UNUSED_PARAM(contextId);
    UNUSED_PARAM(identifier);
#endif
}


#pragma mark Messages to PlaybackSessionManager

void PlaybackSessionManagerProxy::play(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::Play(contextId.object()));
}

void PlaybackSessionManagerProxy::pause(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::Pause(contextId.object()));
}

void PlaybackSessionManagerProxy::togglePlayState(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::TogglePlayState(contextId.object()));
}

void PlaybackSessionManagerProxy::beginScrubbing(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::BeginScrubbing(contextId.object()));
}

void PlaybackSessionManagerProxy::endScrubbing(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::EndScrubbing(contextId.object()));
}

void PlaybackSessionManagerProxy::seekToTime(PlaybackSessionContextIdentifier contextId, double time, double toleranceBefore, double toleranceAfter)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SeekToTime(contextId.object(), time, toleranceBefore, toleranceAfter));
}

void PlaybackSessionManagerProxy::fastSeek(PlaybackSessionContextIdentifier contextId, double time)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::FastSeek(contextId.object(), time));
}

void PlaybackSessionManagerProxy::beginScanningForward(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::BeginScanningForward(contextId.object()));
}

void PlaybackSessionManagerProxy::beginScanningBackward(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::BeginScanningBackward(contextId.object()));
}

void PlaybackSessionManagerProxy::endScanning(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::EndScanning(contextId.object()));
}

void PlaybackSessionManagerProxy::setDefaultPlaybackRate(PlaybackSessionContextIdentifier contextId, double defaultPlaybackRate)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SetDefaultPlaybackRate(contextId.object(), defaultPlaybackRate));
}

void PlaybackSessionManagerProxy::setPlaybackRate(PlaybackSessionContextIdentifier contextId, double playbackRate)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SetPlaybackRate(contextId.object(), playbackRate));
}

void PlaybackSessionManagerProxy::selectAudioMediaOption(PlaybackSessionContextIdentifier contextId, uint64_t index)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SelectAudioMediaOption(contextId.object(), index));
}

void PlaybackSessionManagerProxy::selectLegibleMediaOption(PlaybackSessionContextIdentifier contextId, uint64_t index)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SelectLegibleMediaOption(contextId.object(), index));
}

void PlaybackSessionManagerProxy::togglePictureInPicture(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::TogglePictureInPicture(contextId.object()));
}

void PlaybackSessionManagerProxy::enterFullscreen(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::EnterFullscreen(contextId.object()));
}

void PlaybackSessionManagerProxy::setPlayerIdentifierForVideoElement(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SetPlayerIdentifierForVideoElement(contextId.object()));
}

void PlaybackSessionManagerProxy::exitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::ExitFullscreen(contextId.object()));
}

void PlaybackSessionManagerProxy::enterInWindow(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::EnterInWindow(contextId.object()));
}

void PlaybackSessionManagerProxy::exitInWindow(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::ExitInWindow(contextId.object()));
}

void PlaybackSessionManagerProxy::toggleMuted(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::ToggleMuted(contextId.object()));
}

void PlaybackSessionManagerProxy::setMuted(PlaybackSessionContextIdentifier contextId, bool muted)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SetMuted(contextId.object(), muted));
}

void PlaybackSessionManagerProxy::setVolume(PlaybackSessionContextIdentifier contextId, double volume)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SetVolume(contextId.object(), volume));
}

void PlaybackSessionManagerProxy::setPlayingOnSecondScreen(PlaybackSessionContextIdentifier contextId, bool value)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SetPlayingOnSecondScreen(contextId.object(), value));
}

#if HAVE(PIP_SKIP_PREROLL)
void PlaybackSessionManagerProxy::skipAd(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SkipAd(contextId.object()));
}
#endif

void PlaybackSessionManagerProxy::sendRemoteCommand(PlaybackSessionContextIdentifier contextId, WebCore::PlatformMediaSession::RemoteControlCommandType command, const WebCore::PlatformMediaSession::RemoteCommandArgument& argument)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SendRemoteCommand(contextId.object(), command, argument));
}

void PlaybackSessionManagerProxy::setVideoReceiverEndpoint(PlaybackSessionContextIdentifier contextId, const WebCore::VideoReceiverEndpoint& endpoint, WebCore::VideoReceiverEndpointIdentifier endpointIdentifier)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    auto it = m_contextMap.find(contextId);
    if (it == m_contextMap.end()) {
        ALWAYS_LOG(LOGIDENTIFIER, "no context, ", contextId.loggingString());
        return;
    }

    Ref interface = std::get<1>(it->value);
    if (!interface->playerIdentifier()) {
        ALWAYS_LOG(LOGIDENTIFIER, "no player identifier");
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER);
    WebCore::MediaPlayerIdentifier playerIdentifier = *interface->playerIdentifier();

    RefPtr page = m_page.get();
    if (!page) {
        ALWAYS_LOG(LOGIDENTIFIER, "no page");
        return;
    }

    RefPtr process = WebProcessProxy::processForIdentifier(contextId.processIdentifier());
    if (!process) {
        ALWAYS_LOG(LOGIDENTIFIER, "no process");
        return;
    }
    WebCore::ProcessIdentifier processIdentifier = process->coreProcessIdentifier();

    Ref gpuProcess = process->processPool().ensureProtectedGPUProcess();
    Ref connection = gpuProcess->connection();
    OSObjectPtr<xpc_connection_t> xpcConnection = connection->xpcConnection();
    if (!xpcConnection)
        return;

    VideoReceiverEndpointMessage endpointMessage(WTF::move(processIdentifier), contextId.object(), WTF::move(playerIdentifier), endpoint, endpointIdentifier);
    xpc_connection_send_message_with_reply(xpcConnection.get(), endpointMessage.encode().get(), mainDispatchQueueSingleton(), ^(xpc_object_t reply) {
        RefPtr videoPresentationManager = page->videoPresentationManager();
        if (!videoPresentationManager)
            return;

        RefPtr controlsManagerInterface = videoPresentationManager->controlsManagerInterface();
        if (!controlsManagerInterface)
            return;

        controlsManagerInterface->didSetVideoReceiverEndpoint();
    });
#else
    UNUSED_PARAM(contextId);
    UNUSED_PARAM(endpoint);
#endif
}

void PlaybackSessionManagerProxy::swapVideoReceiverEndpoints(PlaybackSessionContextIdentifier firstContextId, PlaybackSessionContextIdentifier secondContextId)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    RefPtr page = m_page.get();
    if (!page) {
        ALWAYS_LOG(LOGIDENTIFIER, "no page");
        return;
    }

    RefPtr process = WebProcessProxy::processForIdentifier(firstContextId.processIdentifier());
    if (!process) {
        ALWAYS_LOG(LOGIDENTIFIER, "no process");
        return;
    }
    WebCore::ProcessIdentifier processIdentifier = process->coreProcessIdentifier();

    Ref gpuProcess = process->processPool().ensureProtectedGPUProcess();
    Ref connection = gpuProcess->connection();
    OSObjectPtr<xpc_connection_t> xpcConnection = connection->xpcConnection();
    if (!xpcConnection)
        return;

    Ref firstInterface = ensureInterface(firstContextId);
    Ref secondInterface = ensureInterface(secondContextId);

    VideoReceiverSwapEndpointsMessage endpointMessage(WTF::move(processIdentifier), firstContextId.object(), firstInterface->playerIdentifier(), secondContextId.object(), secondInterface->playerIdentifier());
    xpc_connection_send_message(xpcConnection.get(), endpointMessage.encode().get());
#else
    UNUSED_PARAM(firstContextId);
    UNUSED_PARAM(secondContextId);
#endif
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void PlaybackSessionManagerProxy::setSpatialTrackingLabel(PlaybackSessionContextIdentifier contextId, const String& label)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SetSpatialTrackingLabel(contextId.object(), label));
}
#endif

void PlaybackSessionManagerProxy::addNowPlayingMetadataObserver(PlaybackSessionContextIdentifier, const WebCore::NowPlayingMetadataObserver& nowPlayingInfo)
{
    if (RefPtr page = m_page.get())
        page->addNowPlayingMetadataObserver(nowPlayingInfo);
}

void PlaybackSessionManagerProxy::removeNowPlayingMetadataObserver(PlaybackSessionContextIdentifier, const WebCore::NowPlayingMetadataObserver& nowPlayingInfo)
{
    if (RefPtr page = m_page.get())
        page->removeNowPlayingMetadataObserver(nowPlayingInfo);
}

void PlaybackSessionManagerProxy::setSoundStageSize(PlaybackSessionContextIdentifier contextId, WebCore::AudioSessionSoundStageSize size)
{
    sendToWebProcess(contextId, Messages::PlaybackSessionManager::SetSoundStageSize(contextId.object(), size));
}

bool PlaybackSessionManagerProxy::prefersAutoDimming() const
{
    if (!m_controlsManagerContextId)
        return false;

    auto it = m_contextMap.find(*m_controlsManagerContextId);
    if (it == m_contextMap.end())
        return false;

    Ref model = std::get<0>(it->value);
    return model->prefersAutoDimming();
}

void PlaybackSessionManagerProxy::setPrefersAutoDimming(bool prefersAutoDimming)
{
    if (!m_controlsManagerContextId)
        return;

    auto it = m_contextMap.find(*m_controlsManagerContextId);
    if (it == m_contextMap.end())
        return;

    Ref model = std::get<0>(it->value);
    model->setPrefersAutoDimming(prefersAutoDimming);
}

bool PlaybackSessionManagerProxy::wirelessVideoPlaybackDisabled()
{
    if (!m_controlsManagerContextId)
        return true;

    auto it = m_contextMap.find(*m_controlsManagerContextId);
    if (it == m_contextMap.end())
        return true;

    return std::get<0>(it->value)->wirelessVideoPlaybackDisabled();
}

void PlaybackSessionManagerProxy::requestControlledElementID()
{
    if (RefPtr page = m_page.get(); m_controlsManagerContextId)
        protect(page->legacyMainFrameProcess())->send(Messages::PlaybackSessionManager::HandleControlledElementIDRequest(m_controlsManagerContextId->object()), page->webPageIDInMainFrameProcess());
}

PlatformPlaybackSessionInterface* PlaybackSessionManagerProxy::controlsManagerInterface()
{
    if (!m_controlsManagerContextId)
        return nullptr;

    return &ensureInterface(*m_controlsManagerContextId);
}

bool PlaybackSessionManagerProxy::isPaused(PlaybackSessionContextIdentifier identifier) const
{
    auto iterator = m_contextMap.find(identifier);
    if (iterator == m_contextMap.end())
        return false;

    Ref model = std::get<0>(iterator->value);
    return !model->isPlaying() && !model->isStalled();
}

void PlaybackSessionManagerProxy::updateVideoControlsManager(PlaybackSessionContextIdentifier identifier)
{
    if (m_controlsManagerContextId != identifier)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (RefPtr page = m_page.get())
        page->videoControlsManagerDidChange();
}

std::optional<SharedPreferencesForWebProcess> PlaybackSessionManagerProxy::sharedPreferencesForWebProcess(IPC::Connection& connection) const
{
    return WebProcessProxy::fromConnection(connection)->sharedPreferencesForWebProcess();
}

#if !RELEASE_LOG_DISABLED
void PlaybackSessionManagerProxy::setLogIdentifier(PlaybackSessionContextIdentifier identifier, uint64_t logIdentifier)
{
    Ref model = ensureModel(identifier);
    model->setLogIdentifier(logIdentifier);
}

WTFLogChannel& PlaybackSessionManagerProxy::logChannel() const
{
    return WebKit2LogMedia;
}
#endif

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
