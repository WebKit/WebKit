/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#import "PlaybackSessionManagerMessages.h"
#import "PlaybackSessionManagerProxyMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"

namespace WebKit {
using namespace WebCore;

#pragma mark - PlaybackSessionModelContext

void PlaybackSessionModelContext::addClient(PlaybackSessionModelClient& client)
{
    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);
}

void PlaybackSessionModelContext::removeClient(PlaybackSessionModelClient& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
}

void PlaybackSessionModelContext::play()
{
    if (m_manager)
        m_manager->play(m_contextId);
}

void PlaybackSessionModelContext::pause()
{
    if (m_manager)
        m_manager->pause(m_contextId);
}

void PlaybackSessionModelContext::togglePlayState()
{
    if (m_manager)
        m_manager->togglePlayState(m_contextId);
}

void PlaybackSessionModelContext::beginScrubbing()
{
    if (m_manager)
        m_manager->beginScrubbing(m_contextId);

    m_isScrubbing = true;
}

void PlaybackSessionModelContext::endScrubbing()
{
    if (m_manager)
        m_manager->endScrubbing(m_contextId);

    m_isScrubbing = false;
    m_playbackStartedTimeNeedsUpdate = isPlaying();
}

void PlaybackSessionModelContext::seekToTime(double time, double toleranceBefore, double toleranceAfter)
{
    if (m_manager)
        m_manager->seekToTime(m_contextId, time, toleranceBefore, toleranceAfter);
}

void PlaybackSessionModelContext::fastSeek(double time)
{
    if (m_manager)
        m_manager->fastSeek(m_contextId, time);
}

void PlaybackSessionModelContext::beginScanningForward()
{
    if (m_manager)
        m_manager->beginScanningForward(m_contextId);
}

void PlaybackSessionModelContext::beginScanningBackward()
{
    if (m_manager)
        m_manager->beginScanningBackward(m_contextId);
}

void PlaybackSessionModelContext::endScanning()
{
    if (m_manager)
        m_manager->endScanning(m_contextId);
}

void PlaybackSessionModelContext::selectAudioMediaOption(uint64_t optionId)
{
    if (m_manager)
        m_manager->selectAudioMediaOption(m_contextId, optionId);
}

void PlaybackSessionModelContext::selectLegibleMediaOption(uint64_t optionId)
{
    if (m_manager)
        m_manager->selectLegibleMediaOption(m_contextId, optionId);
}

void PlaybackSessionModelContext::togglePictureInPicture()
{
    if (m_manager)
        m_manager->togglePictureInPicture(m_contextId);
}

void PlaybackSessionModelContext::toggleMuted()
{
    if (m_manager)
        m_manager->toggleMuted(m_contextId);
}

void PlaybackSessionModelContext::setMuted(bool muted)
{
    if (m_manager)
        m_manager->setMuted(m_contextId, muted);
}

void PlaybackSessionModelContext::setVolume(double volume)
{
    if (m_manager)
        m_manager->setVolume(m_contextId, volume);
}

void PlaybackSessionModelContext::setPlayingOnSecondScreen(bool value)
{
    if (m_manager)
        m_manager->setPlayingOnSecondScreen(m_contextId, value);
}

void PlaybackSessionModelContext::playbackStartedTimeChanged(double playbackStartedTime)
{
    m_playbackStartedTime = playbackStartedTime;
    m_playbackStartedTimeNeedsUpdate = false;
}

void PlaybackSessionModelContext::durationChanged(double duration)
{
    m_duration = duration;
    for (auto* client : m_clients)
        client->durationChanged(duration);
}

void PlaybackSessionModelContext::currentTimeChanged(double currentTime)
{
    m_currentTime = currentTime;
    auto anchorTime = [[NSProcessInfo processInfo] systemUptime];
    if (m_playbackStartedTimeNeedsUpdate)
        playbackStartedTimeChanged(currentTime);

    for (auto* client : m_clients)
        client->currentTimeChanged(currentTime, anchorTime);
}

void PlaybackSessionModelContext::bufferedTimeChanged(double bufferedTime)
{
    m_bufferedTime = bufferedTime;
    for (auto* client : m_clients)
        client->bufferedTimeChanged(bufferedTime);
}

void PlaybackSessionModelContext::rateChanged(bool isPlaying, float playbackRate)
{
    m_isPlaying = isPlaying;
    m_playbackRate = playbackRate;
    for (auto* client : m_clients)
        client->rateChanged(isPlaying, playbackRate);
}

void PlaybackSessionModelContext::seekableRangesChanged(WebCore::TimeRanges& seekableRanges, double lastModifiedTime, double liveUpdateInterval)
{
    m_seekableRanges = seekableRanges;
    m_seekableTimeRangesLastModifiedTime = lastModifiedTime;
    m_liveUpdateInterval = liveUpdateInterval;
    for (auto* client : m_clients)
        client->seekableRangesChanged(seekableRanges, lastModifiedTime, liveUpdateInterval);
}

void PlaybackSessionModelContext::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    m_canPlayFastReverse = canPlayFastReverse;
    for (auto* client : m_clients)
        client->canPlayFastReverseChanged(canPlayFastReverse);
}

void PlaybackSessionModelContext::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& audioMediaSelectionOptions, uint64_t audioMediaSelectedIndex)
{
    m_audioMediaSelectionOptions = audioMediaSelectionOptions;
    m_audioMediaSelectedIndex = audioMediaSelectedIndex;
    for (auto* client : m_clients)
        client->audioMediaSelectionOptionsChanged(audioMediaSelectionOptions, audioMediaSelectedIndex);
}

void PlaybackSessionModelContext::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& legibleMediaSelectionOptions, uint64_t legibleMediaSelectedIndex)
{
    m_legibleMediaSelectionOptions = legibleMediaSelectionOptions;
    m_legibleMediaSelectedIndex = legibleMediaSelectedIndex;

    for (auto* client : m_clients)
        client->legibleMediaSelectionOptionsChanged(legibleMediaSelectionOptions, legibleMediaSelectedIndex);
}

void PlaybackSessionModelContext::audioMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    m_audioMediaSelectedIndex = selectedIndex;

    for (auto* client : m_clients)
        client->audioMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionModelContext::legibleMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    m_legibleMediaSelectedIndex = selectedIndex;

    for (auto* client : m_clients)
        client->legibleMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionModelContext::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType type, const String& localizedName)
{
    m_externalPlaybackEnabled = enabled;
    m_externalPlaybackTargetType = type;
    m_externalPlaybackLocalizedDeviceName = localizedName;

    for (auto* client : m_clients)
        client->externalPlaybackChanged(enabled, type, localizedName);
}

void PlaybackSessionModelContext::wirelessVideoPlaybackDisabledChanged(bool wirelessVideoPlaybackDisabled)
{
    m_wirelessVideoPlaybackDisabled = wirelessVideoPlaybackDisabled;
    for (auto* client : m_clients)
        client->wirelessVideoPlaybackDisabledChanged(wirelessVideoPlaybackDisabled);
}

void PlaybackSessionModelContext::mutedChanged(bool muted)
{
    m_muted = muted;
    for (auto* client : m_clients)
        client->mutedChanged(muted);
}

void PlaybackSessionModelContext::volumeChanged(double volume)
{
    m_volume = volume;
    for (auto* client : m_clients)
        client->volumeChanged(volume);
}

void PlaybackSessionModelContext::pictureInPictureSupportedChanged(bool supported)
{
    m_pictureInPictureSupported = supported;
    for (auto* client : m_clients)
        client->isPictureInPictureSupportedChanged(supported);
}

void PlaybackSessionModelContext::pictureInPictureActiveChanged(bool active)
{
    m_pictureInPictureActive = active;
    for (auto* client : m_clients)
        client->pictureInPictureActiveChanged(active);
}

#pragma mark - PlaybackSessionManagerProxy

Ref<PlaybackSessionManagerProxy> PlaybackSessionManagerProxy::create(WebPageProxy& page)
{
    return adoptRef(*new PlaybackSessionManagerProxy(page));
}

PlaybackSessionManagerProxy::PlaybackSessionManagerProxy(WebPageProxy& page)
    : m_page(&page)
{
    m_page->process().addMessageReceiver(Messages::PlaybackSessionManagerProxy::messageReceiverName(), m_page->webPageID(), *this);
}

PlaybackSessionManagerProxy::~PlaybackSessionManagerProxy()
{
    if (!m_page)
        return;
    invalidate();
}

void PlaybackSessionManagerProxy::invalidate()
{
    m_page->process().removeMessageReceiver(Messages::PlaybackSessionManagerProxy::messageReceiverName(), m_page->webPageID());
    m_page = nullptr;

    auto contextMap = WTFMove(m_contextMap);
    m_clientCounts.clear();

    for (auto& [model, interface] : contextMap.values())
        interface->invalidate();
}

PlaybackSessionManagerProxy::ModelInterfaceTuple PlaybackSessionManagerProxy::createModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    Ref<PlaybackSessionModelContext> model = PlaybackSessionModelContext::create(*this, contextId);
    Ref<PlatformPlaybackSessionInterface> interface = PlatformPlaybackSessionInterface::create(model);

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

PlaybackSessionManagerProxy::ModelInterfaceTuple& PlaybackSessionManagerProxy::ensureModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_contextMap.add(contextId, ModelInterfaceTuple());
    if (addResult.isNewEntry)
        addResult.iterator->value = createModelAndInterface(contextId);
    return addResult.iterator->value;
}

PlaybackSessionModelContext& PlaybackSessionManagerProxy::ensureModel(PlaybackSessionContextIdentifier contextId)
{
    return *std::get<0>(ensureModelAndInterface(contextId));
}

PlatformPlaybackSessionInterface& PlaybackSessionManagerProxy::ensureInterface(PlaybackSessionContextIdentifier contextId)
{
    return *std::get<1>(ensureModelAndInterface(contextId));
}

void PlaybackSessionManagerProxy::addClientForContext(PlaybackSessionContextIdentifier contextId)
{
    m_clientCounts.add(contextId);
}

void PlaybackSessionManagerProxy::removeClientForContext(PlaybackSessionContextIdentifier contextId)
{
    if (!m_clientCounts.remove(contextId))
        return;

    ensureInterface(contextId).invalidate();
    m_contextMap.remove(contextId);
}

#pragma mark Messages from PlaybackSessionManager

void PlaybackSessionManagerProxy::setUpPlaybackControlsManagerWithID(PlaybackSessionContextIdentifier contextId)
{
    if (m_controlsManagerContextId == contextId)
        return;

    if (m_controlsManagerContextId)
        removeClientForContext(m_controlsManagerContextId);

    m_controlsManagerContextId = contextId;
    ensureInterface(m_controlsManagerContextId).ensureControlsManager();
    addClientForContext(m_controlsManagerContextId);

    m_page->videoControlsManagerDidChange();
}

void PlaybackSessionManagerProxy::clearPlaybackControlsManager()
{
    if (!m_controlsManagerContextId)
        return;

    removeClientForContext(m_controlsManagerContextId);
    m_controlsManagerContextId = { };
    m_page->videoControlsManagerDidChange();
}

void PlaybackSessionManagerProxy::currentTimeChanged(PlaybackSessionContextIdentifier contextId, double currentTime, double hostTime)
{
    ensureModel(contextId).currentTimeChanged(currentTime);
}

void PlaybackSessionManagerProxy::bufferedTimeChanged(PlaybackSessionContextIdentifier contextId, double bufferedTime)
{
    ensureModel(contextId).bufferedTimeChanged(bufferedTime);
}

void PlaybackSessionManagerProxy::seekableRangesVectorChanged(PlaybackSessionContextIdentifier contextId, Vector<std::pair<double, double>> ranges, double lastModifiedTime, double liveUpdateInterval)
{
    Ref<TimeRanges> timeRanges = TimeRanges::create();
    for (const auto& range : ranges) {
        ASSERT(isfinite(range.first));
        ASSERT(isfinite(range.second));
        ASSERT(range.second >= range.first);
        timeRanges->add(range.first, range.second);
    }

    ensureModel(contextId).seekableRangesChanged(timeRanges, lastModifiedTime, liveUpdateInterval);
}

void PlaybackSessionManagerProxy::canPlayFastReverseChanged(PlaybackSessionContextIdentifier contextId, bool value)
{
    ensureModel(contextId).canPlayFastReverseChanged(value);
}

void PlaybackSessionManagerProxy::audioMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier contextId, Vector<MediaSelectionOption> options, uint64_t selectedIndex)
{
    ensureModel(contextId).audioMediaSelectionOptionsChanged(options, selectedIndex);
}

void PlaybackSessionManagerProxy::legibleMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier contextId, Vector<MediaSelectionOption> options, uint64_t selectedIndex)
{
    ensureModel(contextId).legibleMediaSelectionOptionsChanged(options, selectedIndex);
}

void PlaybackSessionManagerProxy::audioMediaSelectionIndexChanged(PlaybackSessionContextIdentifier contextId, uint64_t selectedIndex)
{
    ensureModel(contextId).audioMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionManagerProxy::legibleMediaSelectionIndexChanged(PlaybackSessionContextIdentifier contextId, uint64_t selectedIndex)
{
    ensureModel(contextId).legibleMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionManagerProxy::externalPlaybackPropertiesChanged(PlaybackSessionContextIdentifier contextId, bool enabled, uint32_t targetType, String localizedDeviceName)
{
    PlaybackSessionModel::ExternalPlaybackTargetType type = static_cast<PlaybackSessionModel::ExternalPlaybackTargetType>(targetType);
    ASSERT(type == PlaybackSessionModel::TargetTypeAirPlay || type == PlaybackSessionModel::TargetTypeTVOut || type == PlaybackSessionModel::TargetTypeNone);

    ensureModel(contextId).externalPlaybackChanged(enabled, type, localizedDeviceName);
}

void PlaybackSessionManagerProxy::wirelessVideoPlaybackDisabledChanged(PlaybackSessionContextIdentifier contextId, bool disabled)
{
    ensureModel(contextId).wirelessVideoPlaybackDisabledChanged(disabled);
}

void PlaybackSessionManagerProxy::mutedChanged(PlaybackSessionContextIdentifier contextId, bool muted)
{
    ensureModel(contextId).mutedChanged(muted);
}

void PlaybackSessionManagerProxy::volumeChanged(PlaybackSessionContextIdentifier contextId, double volume)
{
    ensureModel(contextId).volumeChanged(volume);
}

void PlaybackSessionManagerProxy::durationChanged(PlaybackSessionContextIdentifier contextId, double duration)
{
    ensureModel(contextId).durationChanged(duration);
}

void PlaybackSessionManagerProxy::playbackStartedTimeChanged(PlaybackSessionContextIdentifier contextId, double playbackStartedTime)
{
    ensureModel(contextId).playbackStartedTimeChanged(playbackStartedTime);
}

void PlaybackSessionManagerProxy::rateChanged(PlaybackSessionContextIdentifier contextId, bool isPlaying, double rate)
{
    ensureModel(contextId).rateChanged(isPlaying, rate);
}

void PlaybackSessionManagerProxy::pictureInPictureSupportedChanged(PlaybackSessionContextIdentifier contextId, bool supported)
{
    ensureModel(contextId).pictureInPictureSupportedChanged(supported);
}

void PlaybackSessionManagerProxy::handleControlledElementIDResponse(PlaybackSessionContextIdentifier contextId, String identifier) const
{
#if PLATFORM(MAC)
    if (contextId == m_controlsManagerContextId)
        m_page->handleControlledElementIDResponse(identifier);
#else
    UNUSED_PARAM(contextId);
    UNUSED_PARAM(identifier);
#endif
}


#pragma mark Messages to PlaybackSessionManager

void PlaybackSessionManagerProxy::play(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::Play(contextId));
}

void PlaybackSessionManagerProxy::pause(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::Pause(contextId));
}

void PlaybackSessionManagerProxy::togglePlayState(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::TogglePlayState(contextId));
}

void PlaybackSessionManagerProxy::beginScrubbing(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::BeginScrubbing(contextId));
}

void PlaybackSessionManagerProxy::endScrubbing(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::EndScrubbing(contextId));
}

void PlaybackSessionManagerProxy::seekToTime(PlaybackSessionContextIdentifier contextId, double time, double toleranceBefore, double toleranceAfter)
{
    m_page->send(Messages::PlaybackSessionManager::SeekToTime(contextId, time, toleranceBefore, toleranceAfter));
}

void PlaybackSessionManagerProxy::fastSeek(PlaybackSessionContextIdentifier contextId, double time)
{
    m_page->send(Messages::PlaybackSessionManager::FastSeek(contextId, time));
}

void PlaybackSessionManagerProxy::beginScanningForward(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::BeginScanningForward(contextId));
}

void PlaybackSessionManagerProxy::beginScanningBackward(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::BeginScanningBackward(contextId));
}

void PlaybackSessionManagerProxy::endScanning(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::EndScanning(contextId));
}

void PlaybackSessionManagerProxy::selectAudioMediaOption(PlaybackSessionContextIdentifier contextId, uint64_t index)
{
    m_page->send(Messages::PlaybackSessionManager::SelectAudioMediaOption(contextId, index));
}

void PlaybackSessionManagerProxy::selectLegibleMediaOption(PlaybackSessionContextIdentifier contextId, uint64_t index)
{
    m_page->send(Messages::PlaybackSessionManager::SelectLegibleMediaOption(contextId, index));
}

void PlaybackSessionManagerProxy::togglePictureInPicture(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::TogglePictureInPicture(contextId));
}

void PlaybackSessionManagerProxy::toggleMuted(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::PlaybackSessionManager::ToggleMuted(contextId));
}

void PlaybackSessionManagerProxy::setMuted(PlaybackSessionContextIdentifier contextId, bool muted)
{
    m_page->send(Messages::PlaybackSessionManager::SetMuted(contextId, muted));
}

void PlaybackSessionManagerProxy::setVolume(PlaybackSessionContextIdentifier contextId, double volume)
{
    m_page->send(Messages::PlaybackSessionManager::SetVolume(contextId, volume));
}

void PlaybackSessionManagerProxy::setPlayingOnSecondScreen(PlaybackSessionContextIdentifier contextId, bool value)
{
    if (m_page)
        m_page->send(Messages::PlaybackSessionManager::SetPlayingOnSecondScreen(contextId, value));
}

bool PlaybackSessionManagerProxy::wirelessVideoPlaybackDisabled()
{
    auto it = m_contextMap.find(m_controlsManagerContextId);
    if (it == m_contextMap.end())
        return true;

    return std::get<0>(it->value)->wirelessVideoPlaybackDisabled();
}

void PlaybackSessionManagerProxy::requestControlledElementID()
{
    if (m_controlsManagerContextId)
        m_page->send(Messages::PlaybackSessionManager::HandleControlledElementIDRequest(m_controlsManagerContextId));
}

PlatformPlaybackSessionInterface* PlaybackSessionManagerProxy::controlsManagerInterface()
{
    if (!m_controlsManagerContextId)
        return nullptr;

    auto& interface = ensureInterface(m_controlsManagerContextId);
    return &interface;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
