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
#import "VideoReceiverEndpointMessage.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <WebCore/NullPlaybackSessionInterface.h>
#import <WebCore/PlaybackSessionInterfaceAVKit.h>
#import <WebCore/PlaybackSessionInterfaceMac.h>
#import <WebCore/PlaybackSessionInterfaceTVOS.h>
#import <wtf/LoggerHelper.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

#pragma mark - PlaybackSessionModelContext

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionModelContext);

PlaybackSessionModelContext::PlaybackSessionModelContext(PlaybackSessionManagerProxy& manager, PlaybackSessionContextIdentifier contextId)
    : m_manager(manager)
    , m_contextId(contextId)
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

    if (m_manager && m_videoReceiverEndpoint)
        m_manager->uncacheVideoReceiverEndpoint(m_contextId);

    m_videoReceiverEndpoint = endpoint;

    if (m_manager && m_videoReceiverEndpoint)
        m_manager->setVideoReceiverEndpoint(m_contextId, endpoint);
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
    for (auto& client : m_clients)
        client.durationChanged(duration);
}

void PlaybackSessionModelContext::currentTimeChanged(double currentTime)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, currentTime);
    m_currentTime = currentTime;
    auto anchorTime = [[NSProcessInfo processInfo] systemUptime];
    if (m_playbackStartedTimeNeedsUpdate)
        playbackStartedTimeChanged(currentTime);

    for (auto& client : m_clients)
        client.currentTimeChanged(currentTime, anchorTime);
}

void PlaybackSessionModelContext::bufferedTimeChanged(double bufferedTime)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, bufferedTime);
    m_bufferedTime = bufferedTime;
    for (auto& client : m_clients)
        client.bufferedTimeChanged(bufferedTime);
}

void PlaybackSessionModelContext::rateChanged(OptionSet<WebCore::PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double defaultPlaybackRate)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, playbackRate, ", defaultPlaybackRate", defaultPlaybackRate);
    m_playbackState = playbackState;
    m_playbackRate = playbackRate;
    m_defaultPlaybackRate = defaultPlaybackRate;
    for (auto& client : m_clients)
        client.rateChanged(m_playbackState, m_playbackRate, m_defaultPlaybackRate);
}

void PlaybackSessionModelContext::seekableRangesChanged(WebCore::TimeRanges& seekableRanges, double lastModifiedTime, double liveUpdateInterval)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, seekableRanges.ranges());
    m_seekableRanges = seekableRanges;
    m_seekableTimeRangesLastModifiedTime = lastModifiedTime;
    m_liveUpdateInterval = liveUpdateInterval;
    for (auto& client : m_clients)
        client.seekableRangesChanged(seekableRanges, lastModifiedTime, liveUpdateInterval);
}

void PlaybackSessionModelContext::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    m_canPlayFastReverse = canPlayFastReverse;
    for (auto& client : m_clients)
        client.canPlayFastReverseChanged(canPlayFastReverse);
}

void PlaybackSessionModelContext::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& audioMediaSelectionOptions, uint64_t audioMediaSelectedIndex)
{
    m_audioMediaSelectionOptions = audioMediaSelectionOptions;
    m_audioMediaSelectedIndex = audioMediaSelectedIndex;
    for (auto& client : m_clients)
        client.audioMediaSelectionOptionsChanged(audioMediaSelectionOptions, audioMediaSelectedIndex);
}

void PlaybackSessionModelContext::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& legibleMediaSelectionOptions, uint64_t legibleMediaSelectedIndex)
{
    m_legibleMediaSelectionOptions = legibleMediaSelectionOptions;
    m_legibleMediaSelectedIndex = legibleMediaSelectedIndex;

    for (auto& client : m_clients)
        client.legibleMediaSelectionOptionsChanged(legibleMediaSelectionOptions, legibleMediaSelectedIndex);
}

void PlaybackSessionModelContext::audioMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    m_audioMediaSelectedIndex = selectedIndex;

    for (auto& client : m_clients)
        client.audioMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionModelContext::legibleMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    m_legibleMediaSelectedIndex = selectedIndex;

    for (auto& client : m_clients)
        client.legibleMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionModelContext::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType type, const String& localizedName)
{
    m_externalPlaybackEnabled = enabled;
    m_externalPlaybackTargetType = type;
    m_externalPlaybackLocalizedDeviceName = localizedName;

    for (auto& client : m_clients)
        client.externalPlaybackChanged(enabled, type, localizedName);
}

void PlaybackSessionModelContext::wirelessVideoPlaybackDisabledChanged(bool wirelessVideoPlaybackDisabled)
{
    m_wirelessVideoPlaybackDisabled = wirelessVideoPlaybackDisabled;
    for (auto& client : m_clients)
        client.wirelessVideoPlaybackDisabledChanged(wirelessVideoPlaybackDisabled);
}

void PlaybackSessionModelContext::mutedChanged(bool muted)
{
    m_muted = muted;
    for (auto& client : m_clients)
        client.mutedChanged(muted);
}

void PlaybackSessionModelContext::volumeChanged(double volume)
{
    m_volume = volume;
    for (auto& client : m_clients)
        client.volumeChanged(volume);
}

void PlaybackSessionModelContext::pictureInPictureSupportedChanged(bool supported)
{
    m_pictureInPictureSupported = supported;
    for (auto& client : m_clients)
        client.isPictureInPictureSupportedChanged(supported);
}

void PlaybackSessionModelContext::pictureInPictureActiveChanged(bool active)
{
    m_pictureInPictureActive = active;
    for (auto& client : m_clients)
        client.pictureInPictureActiveChanged(active);
}

void PlaybackSessionModelContext::isInWindowFullscreenActiveChanged(bool active)
{
    m_isInWindowFullscreenActive = active;
    for (auto& client : m_clients)
        client.isInWindowFullscreenActiveChanged(active);
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void PlaybackSessionModelContext::supportsLinearMediaPlayerChanged(bool supportsLinearMediaPlayer)
{
    if (m_supportsLinearMediaPlayer == supportsLinearMediaPlayer)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, supportsLinearMediaPlayer);
    m_supportsLinearMediaPlayer = supportsLinearMediaPlayer;

    for (auto& client : m_clients)
        client.supportsLinearMediaPlayerChanged(supportsLinearMediaPlayer);

    if (RefPtr manager = m_manager.get())
        manager->updateVideoControlsManager(m_contextId);
}

void PlaybackSessionModelContext::spatialVideoMetadataChanged(const std::optional<WebCore::SpatialVideoMetadata>& metadata)
{
    if (m_spatialVideoMetadata == metadata)
        return;
    if (metadata)
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, *metadata);
    m_spatialVideoMetadata = metadata;

    for (auto& client : m_clients)
        client.spatialVideoMetadataChanged(m_spatialVideoMetadata);
}
#endif

void PlaybackSessionModelContext::invalidate()
{
    setVideoReceiverEndpoint(nullptr);
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
    protectedPage->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::PlaybackSessionManagerProxy::messageReceiverName(), protectedPage->webPageIDInMainFrameProcess(), *this);
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
        page->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::PlaybackSessionManagerProxy::messageReceiverName(), page->webPageIDInMainFrameProcess());
        m_page = nullptr;
    }

    auto contextMap = WTFMove(m_contextMap);
    m_clientCounts.clear();

    for (auto& [model, interface] : contextMap.values()) {
        model->invalidate();
        interface->invalidate();
    }
}

PlaybackSessionManagerProxy::ModelInterfaceTuple PlaybackSessionManagerProxy::createModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    Ref<PlaybackSessionModelContext> model = PlaybackSessionModelContext::create(*this, contextId);

    RefPtr<PlatformPlaybackSessionInterface> interface;
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (RefPtr page = m_page.get(); page->preferences().linearMediaPlayerEnabled()) {
        auto lmkInterface = PlaybackSessionInterfaceLMK::create(model);
        lmkInterface->setSpatialVideoEnabled(page->preferences().spatialVideoEnabled());
        interface = WTFMove(lmkInterface);
    } else
        interface = PlaybackSessionInterfaceAVKit::create(model);
#else
    interface = PlatformPlaybackSessionInterface::create(model);
#endif

    return std::make_tuple(WTFMove(model), interface.releaseNonNull());
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

Ref<PlatformPlaybackSessionInterface> PlaybackSessionManagerProxy::ensureInterface(PlaybackSessionContextIdentifier contextId)
{
    return std::get<1>(ensureModelAndInterface(contextId));
}

void PlaybackSessionManagerProxy::addClientForContext(PlaybackSessionContextIdentifier contextId)
{
    m_clientCounts.add(contextId);
}

void PlaybackSessionManagerProxy::removeClientForContext(PlaybackSessionContextIdentifier contextId)
{
    if (!m_clientCounts.remove(contextId))
        return;

    ensureInterface(contextId)->invalidate();
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
    ensureInterface(*m_controlsManagerContextId)->ensureControlsManager();
    addClientForContext(*m_controlsManagerContextId);

    if (RefPtr page = m_page.get())
        page->videoControlsManagerDidChange();
}

void PlaybackSessionManagerProxy::clearPlaybackControlsManager()
{
    if (!m_controlsManagerContextId)
        return;

    removeClientForContext(*m_controlsManagerContextId);
    m_controlsManagerContextId = std::nullopt;
    m_controlsManagerContextIsVideo = false;

    if (RefPtr page = m_page.get())
        page->videoControlsManagerDidChange();
}

void PlaybackSessionManagerProxy::currentTimeChanged(PlaybackSessionContextIdentifier contextId, double currentTime, double hostTime)
{
    ensureModel(contextId)->currentTimeChanged(currentTime);

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr page = m_page.get())
        page->didChangeCurrentTime(contextId);
#endif
}

void PlaybackSessionManagerProxy::bufferedTimeChanged(PlaybackSessionContextIdentifier contextId, double bufferedTime)
{
    ensureModel(contextId)->bufferedTimeChanged(bufferedTime);
}

void PlaybackSessionManagerProxy::seekableRangesVectorChanged(PlaybackSessionContextIdentifier contextId, Vector<std::pair<double, double>> ranges, double lastModifiedTime, double liveUpdateInterval)
{
    Ref<TimeRanges> timeRanges = TimeRanges::create();
    for (const auto& range : ranges) {
        ASSERT(isfinite(range.first));
        ASSERT(!isfinite(range.second) || range.second >= range.first);
        timeRanges->add(range.first, range.second);
    }

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

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr page = m_page.get())
        page->didChangePlaybackRate(contextId);
#endif
}

void PlaybackSessionManagerProxy::pictureInPictureSupportedChanged(PlaybackSessionContextIdentifier contextId, bool supported)
{
    ensureModel(contextId)->pictureInPictureSupportedChanged(supported);
}

void PlaybackSessionManagerProxy::isInWindowFullscreenActiveChanged(PlaybackSessionContextIdentifier contextId, bool active)
{
    ensureModel(contextId)->isInWindowFullscreenActiveChanged(active);
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void PlaybackSessionManagerProxy::supportsLinearMediaPlayerChanged(PlaybackSessionContextIdentifier contextId, bool supportsLinearMediaPlayer)
{
    ensureModel(contextId)->supportsLinearMediaPlayerChanged(supportsLinearMediaPlayer);
}

void PlaybackSessionManagerProxy::spatialVideoMetadataChanged(PlaybackSessionContextIdentifier contextId, const std::optional<WebCore::SpatialVideoMetadata>& metadata)
{
    ensureModel(contextId)->spatialVideoMetadataChanged(metadata);
}

#endif

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
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::Play(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::pause(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::Pause(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::togglePlayState(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::TogglePlayState(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::beginScrubbing(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::BeginScrubbing(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::endScrubbing(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::EndScrubbing(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::seekToTime(PlaybackSessionContextIdentifier contextId, double time, double toleranceBefore, double toleranceAfter)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SeekToTime(contextId, time, toleranceBefore, toleranceAfter), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::fastSeek(PlaybackSessionContextIdentifier contextId, double time)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::FastSeek(contextId, time), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::beginScanningForward(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::BeginScanningForward(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::beginScanningBackward(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::BeginScanningBackward(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::endScanning(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::EndScanning(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::setDefaultPlaybackRate(PlaybackSessionContextIdentifier contextId, double defaultPlaybackRate)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SetDefaultPlaybackRate(contextId, defaultPlaybackRate), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::setPlaybackRate(PlaybackSessionContextIdentifier contextId, double playbackRate)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SetPlaybackRate(contextId, playbackRate), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::selectAudioMediaOption(PlaybackSessionContextIdentifier contextId, uint64_t index)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SelectAudioMediaOption(contextId, index), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::selectLegibleMediaOption(PlaybackSessionContextIdentifier contextId, uint64_t index)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SelectLegibleMediaOption(contextId, index), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::togglePictureInPicture(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::TogglePictureInPicture(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::enterFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::EnterFullscreen(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::exitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::ExitFullscreen(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::enterInWindow(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::EnterInWindow(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::exitInWindow(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::ExitInWindow(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::toggleMuted(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::ToggleMuted(contextId), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::setMuted(PlaybackSessionContextIdentifier contextId, bool muted)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SetMuted(contextId, muted), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::setVolume(PlaybackSessionContextIdentifier contextId, double volume)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SetVolume(contextId, volume), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::setPlayingOnSecondScreen(PlaybackSessionContextIdentifier contextId, bool value)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SetPlayingOnSecondScreen(contextId, value), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::sendRemoteCommand(PlaybackSessionContextIdentifier contextId, WebCore::PlatformMediaSession::RemoteControlCommandType command, const WebCore::PlatformMediaSession::RemoteCommandArgument& argument)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SendRemoteCommand(contextId, command, argument), page->webPageIDInMainFrameProcess());
}

void PlaybackSessionManagerProxy::setVideoReceiverEndpoint(PlaybackSessionContextIdentifier contextId, const WebCore::VideoReceiverEndpoint& endpoint)
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

    Ref process = page->protectedLegacyMainFrameProcess();
    WebCore::ProcessIdentifier processIdentifier = process->coreProcessIdentifier();

    Ref gpuProcess = process->processPool().ensureProtectedGPUProcess();
    RefPtr connection = gpuProcess->protectedConnection();
    if (!connection)
        return;

    OSObjectPtr xpcConnection = connection->xpcConnection();
    if (!xpcConnection)
        return;

    VideoReceiverEndpointMessage endpointMessage(WTFMove(processIdentifier), contextId, WTFMove(playerIdentifier), endpoint);
    xpc_connection_send_message(xpcConnection.get(), endpointMessage.encode().get());
#else
    UNUSED_PARAM(contextId);
    UNUSED_PARAM(endpoint);
#endif
}

void PlaybackSessionManagerProxy::uncacheVideoReceiverEndpoint(PlaybackSessionContextIdentifier contextId)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    RefPtr page = m_page.get();
    if (!page)
        return;

    Ref process = page->protectedLegacyMainFrameProcess();
    WebCore::ProcessIdentifier processIdentifier = process->coreProcessIdentifier();

    Ref gpuProcess = process->processPool().ensureProtectedGPUProcess();
    RefPtr connection = gpuProcess->protectedConnection();
    if (!connection)
        return;

    OSObjectPtr xpcConnection = connection->xpcConnection();
    if (!xpcConnection)
        return;

    VideoReceiverEndpointMessage endpointMessage(WTFMove(processIdentifier), contextId, std::nullopt, nullptr);
    xpc_connection_send_message(xpcConnection.get(), endpointMessage.encode().get());
#else
    UNUSED_PARAM(contextId);
#endif
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void PlaybackSessionManagerProxy::setSpatialTrackingLabel(PlaybackSessionContextIdentifier contextId, const String& label)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SetSpatialTrackingLabel(contextId, label), page->webPageIDInMainFrameProcess());
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
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::SetSoundStageSize(contextId, size), page->webPageIDInMainFrameProcess());
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
        page->protectedLegacyMainFrameProcess()->send(Messages::PlaybackSessionManager::HandleControlledElementIDRequest(*m_controlsManagerContextId), page->webPageIDInMainFrameProcess());
}

RefPtr<PlatformPlaybackSessionInterface> PlaybackSessionManagerProxy::controlsManagerInterface()
{
    if (!m_controlsManagerContextId)
        return nullptr;

    return ensureInterface(*m_controlsManagerContextId);
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

std::optional<SharedPreferencesForWebProcess> PlaybackSessionManagerProxy::sharedPreferencesForWebProcess() const
{
    if (!m_page)
        return std::nullopt;

    // FIXME: Remove SUPPRESS_UNCOUNTED_ARG once https://github.com/llvm/llvm-project/pull/111198 lands.
    SUPPRESS_UNCOUNTED_ARG return m_page->legacyMainFrameProcess().sharedPreferencesForWebProcess();
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
