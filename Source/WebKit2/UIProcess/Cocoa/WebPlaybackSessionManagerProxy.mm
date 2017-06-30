/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#import "WebPlaybackSessionManagerProxy.h"

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "WebPageProxy.h"
#import "WebPlaybackSessionManagerMessages.h"
#import "WebPlaybackSessionManagerProxyMessages.h"
#import "WebProcessProxy.h"
#import <WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

#pragma mark - WebPlaybackSessionModelContext

void WebPlaybackSessionModelContext::addClient(WebPlaybackSessionModelClient& client)
{
    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);
}

void WebPlaybackSessionModelContext::removeClient(WebPlaybackSessionModelClient& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
}

void WebPlaybackSessionModelContext::play()
{
    if (m_manager)
        m_manager->play(m_contextId);
}

void WebPlaybackSessionModelContext::pause()
{
    if (m_manager)
        m_manager->pause(m_contextId);
}

void WebPlaybackSessionModelContext::togglePlayState()
{
    if (m_manager)
        m_manager->togglePlayState(m_contextId);
}

void WebPlaybackSessionModelContext::beginScrubbing()
{
    if (m_manager)
        m_manager->beginScrubbing(m_contextId);

    m_isScrubbing = true;
}

void WebPlaybackSessionModelContext::endScrubbing()
{
    if (m_manager)
        m_manager->endScrubbing(m_contextId);

    m_isScrubbing = false;
    m_playbackStartedTimeNeedsUpdate = isPlaying();
}

void WebPlaybackSessionModelContext::seekToTime(double time)
{
    if (m_manager)
        m_manager->seekToTime(m_contextId, time);
}

void WebPlaybackSessionModelContext::fastSeek(double time)
{
    if (m_manager)
        m_manager->fastSeek(m_contextId, time);
}

void WebPlaybackSessionModelContext::beginScanningForward()
{
    if (m_manager)
        m_manager->beginScanningForward(m_contextId);
}

void WebPlaybackSessionModelContext::beginScanningBackward()
{
    if (m_manager)
        m_manager->beginScanningBackward(m_contextId);
}

void WebPlaybackSessionModelContext::endScanning()
{
    if (m_manager)
        m_manager->endScanning(m_contextId);
}

void WebPlaybackSessionModelContext::selectAudioMediaOption(uint64_t optionId)
{
    if (m_manager)
        m_manager->selectAudioMediaOption(m_contextId, optionId);
}

void WebPlaybackSessionModelContext::selectLegibleMediaOption(uint64_t optionId)
{
    if (m_manager)
        m_manager->selectLegibleMediaOption(m_contextId, optionId);
}

void WebPlaybackSessionModelContext::togglePictureInPicture()
{
    if (m_manager)
        m_manager->togglePictureInPicture(m_contextId);
}

void WebPlaybackSessionModelContext::toggleMuted()
{
    if (m_manager)
        m_manager->toggleMuted(m_contextId);
}

void WebPlaybackSessionModelContext::setMuted(bool muted)
{
    if (m_manager)
        m_manager->setMuted(m_contextId, muted);
}

void WebPlaybackSessionModelContext::playbackStartedTimeChanged(double playbackStartedTime)
{
    m_playbackStartedTime = playbackStartedTime;
    m_playbackStartedTimeNeedsUpdate = false;
}

void WebPlaybackSessionModelContext::durationChanged(double duration)
{
    m_duration = duration;
    for (auto* client : m_clients)
        client->durationChanged(duration);
}

void WebPlaybackSessionModelContext::currentTimeChanged(double currentTime)
{
    m_currentTime = currentTime;
    auto anchorTime = [[NSProcessInfo processInfo] systemUptime];
    if (m_playbackStartedTimeNeedsUpdate)
        playbackStartedTimeChanged(currentTime);

    for (auto* client : m_clients)
        client->currentTimeChanged(currentTime, anchorTime);
}

void WebPlaybackSessionModelContext::bufferedTimeChanged(double bufferedTime)
{
    m_bufferedTime = bufferedTime;
    for (auto* client : m_clients)
        client->bufferedTimeChanged(bufferedTime);
}

void WebPlaybackSessionModelContext::rateChanged(bool isPlaying, float playbackRate)
{
    m_isPlaying = isPlaying;
    m_playbackRate = playbackRate;
    for (auto* client : m_clients)
        client->rateChanged(isPlaying, playbackRate);
}

void WebPlaybackSessionModelContext::seekableRangesChanged(WebCore::TimeRanges& seekableRanges, double lastModifiedTime, double liveUpdateInterval)
{
    m_seekableRanges = seekableRanges;
    m_seekableTimeRangesLastModifiedTime = lastModifiedTime;
    m_liveUpdateInterval = liveUpdateInterval;
    for (auto* client : m_clients)
        client->seekableRangesChanged(seekableRanges, lastModifiedTime, liveUpdateInterval);
}

void WebPlaybackSessionModelContext::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    m_canPlayFastReverse = canPlayFastReverse;
    for (auto* client : m_clients)
        client->canPlayFastReverseChanged(canPlayFastReverse);
}

void WebPlaybackSessionModelContext::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& audioMediaSelectionOptions, uint64_t audioMediaSelectedIndex)
{
    m_audioMediaSelectionOptions = audioMediaSelectionOptions;
    m_audioMediaSelectedIndex = audioMediaSelectedIndex;
    for (auto* client : m_clients)
        client->audioMediaSelectionOptionsChanged(audioMediaSelectionOptions, audioMediaSelectedIndex);
}

void WebPlaybackSessionModelContext::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& legibleMediaSelectionOptions, uint64_t legibleMediaSelectedIndex)
{
    m_legibleMediaSelectionOptions = legibleMediaSelectionOptions;
    m_legibleMediaSelectedIndex = legibleMediaSelectedIndex;

    for (auto* client : m_clients)
        client->legibleMediaSelectionOptionsChanged(legibleMediaSelectionOptions, legibleMediaSelectedIndex);
}

void WebPlaybackSessionModelContext::audioMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    m_audioMediaSelectedIndex = selectedIndex;

    for (auto* client : m_clients)
        client->audioMediaSelectionIndexChanged(selectedIndex);
}

void WebPlaybackSessionModelContext::legibleMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    m_legibleMediaSelectedIndex = selectedIndex;

    for (auto* client : m_clients)
        client->legibleMediaSelectionIndexChanged(selectedIndex);
}

void WebPlaybackSessionModelContext::externalPlaybackChanged(bool enabled, WebPlaybackSessionModel::ExternalPlaybackTargetType type, const String& localizedName)
{
    m_externalPlaybackEnabled = enabled;
    m_externalPlaybackTargetType = type;
    m_externalPlaybackLocalizedDeviceName = localizedName;

    for (auto* client : m_clients)
        client->externalPlaybackChanged(enabled, type, localizedName);
}

void WebPlaybackSessionModelContext::wirelessVideoPlaybackDisabledChanged(bool wirelessVideoPlaybackDisabled)
{
    m_wirelessVideoPlaybackDisabled = wirelessVideoPlaybackDisabled;
    for (auto* client : m_clients)
        client->wirelessVideoPlaybackDisabledChanged(wirelessVideoPlaybackDisabled);
}

void WebPlaybackSessionModelContext::mutedChanged(bool muted)
{
    m_muted = muted;
    for (auto* client : m_clients)
        client->mutedChanged(muted);
}

#pragma mark - WebPlaybackSessionManagerProxy

RefPtr<WebPlaybackSessionManagerProxy> WebPlaybackSessionManagerProxy::create(WebPageProxy& page)
{
    return adoptRef(new WebPlaybackSessionManagerProxy(page));
}

WebPlaybackSessionManagerProxy::WebPlaybackSessionManagerProxy(WebPageProxy& page)
    : m_page(&page)
{
    m_page->process().addMessageReceiver(Messages::WebPlaybackSessionManagerProxy::messageReceiverName(), m_page->pageID(), *this);
}

WebPlaybackSessionManagerProxy::~WebPlaybackSessionManagerProxy()
{
    if (!m_page)
        return;
    invalidate();
}

void WebPlaybackSessionManagerProxy::invalidate()
{
    m_page->process().removeMessageReceiver(Messages::WebPlaybackSessionManagerProxy::messageReceiverName(), m_page->pageID());
    m_page = nullptr;

    for (auto& tuple : m_contextMap.values()) {
        RefPtr<WebPlaybackSessionModelContext> model;
        RefPtr<PlatformWebPlaybackSessionInterface> interface;
        std::tie(model, interface) = tuple;

        interface->invalidate();
    }

    m_contextMap.clear();
    m_clientCounts.clear();
}

WebPlaybackSessionManagerProxy::ModelInterfaceTuple WebPlaybackSessionManagerProxy::createModelAndInterface(uint64_t contextId)
{
    Ref<WebPlaybackSessionModelContext> model = WebPlaybackSessionModelContext::create(*this, contextId);
    Ref<PlatformWebPlaybackSessionInterface> interface = PlatformWebPlaybackSessionInterface::create(model);

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

WebPlaybackSessionManagerProxy::ModelInterfaceTuple& WebPlaybackSessionManagerProxy::ensureModelAndInterface(uint64_t contextId)
{
    auto addResult = m_contextMap.add(contextId, ModelInterfaceTuple());
    if (addResult.isNewEntry)
        addResult.iterator->value = createModelAndInterface(contextId);
    return addResult.iterator->value;
}

WebPlaybackSessionModelContext& WebPlaybackSessionManagerProxy::ensureModel(uint64_t contextId)
{
    return *std::get<0>(ensureModelAndInterface(contextId));
}

PlatformWebPlaybackSessionInterface& WebPlaybackSessionManagerProxy::ensureInterface(uint64_t contextId)
{
    return *std::get<1>(ensureModelAndInterface(contextId));
}

void WebPlaybackSessionManagerProxy::addClientForContext(uint64_t contextId)
{
    m_clientCounts.add(contextId);
}

void WebPlaybackSessionManagerProxy::removeClientForContext(uint64_t contextId)
{
    if (!m_clientCounts.remove(contextId))
        return;

    ensureInterface(contextId).invalidate();
    m_contextMap.remove(contextId);
}

#pragma mark Messages from WebPlaybackSessionManager

void WebPlaybackSessionManagerProxy::setUpPlaybackControlsManagerWithID(uint64_t contextId)
{
#if PLATFORM(MAC)
    if (m_controlsManagerContextId == contextId)
        return;

    if (m_controlsManagerContextId)
        removeClientForContext(m_controlsManagerContextId);

    m_controlsManagerContextId = contextId;
    ensureInterface(m_controlsManagerContextId).ensureControlsManager();
    addClientForContext(m_controlsManagerContextId);

    m_page->videoControlsManagerDidChange();
#else
    UNUSED_PARAM(contextId);
#endif
}

void WebPlaybackSessionManagerProxy::clearPlaybackControlsManager()
{
#if PLATFORM(MAC)
    if (!m_controlsManagerContextId)
        return;

    removeClientForContext(m_controlsManagerContextId);
    m_controlsManagerContextId = 0;
    m_page->videoControlsManagerDidChange();
#endif
}

void WebPlaybackSessionManagerProxy::resetMediaState(uint64_t contextId)
{
    ensureInterface(contextId).resetMediaState();
}

void WebPlaybackSessionManagerProxy::currentTimeChanged(uint64_t contextId, double currentTime, double hostTime)
{
    ensureModel(contextId).currentTimeChanged(currentTime);
}

void WebPlaybackSessionManagerProxy::bufferedTimeChanged(uint64_t contextId, double bufferedTime)
{
    ensureModel(contextId).bufferedTimeChanged(bufferedTime);
}

void WebPlaybackSessionManagerProxy::seekableRangesVectorChanged(uint64_t contextId, Vector<std::pair<double, double>> ranges, double lastModifiedTime, double liveUpdateInterval)
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

void WebPlaybackSessionManagerProxy::canPlayFastReverseChanged(uint64_t contextId, bool value)
{
    ensureModel(contextId).canPlayFastReverseChanged(value);
}

void WebPlaybackSessionManagerProxy::audioMediaSelectionOptionsChanged(uint64_t contextId, Vector<MediaSelectionOption> options, uint64_t selectedIndex)
{
    ensureModel(contextId).audioMediaSelectionOptionsChanged(options, selectedIndex);
}

void WebPlaybackSessionManagerProxy::legibleMediaSelectionOptionsChanged(uint64_t contextId, Vector<MediaSelectionOption> options, uint64_t selectedIndex)
{
    ensureModel(contextId).legibleMediaSelectionOptionsChanged(options, selectedIndex);
}

void WebPlaybackSessionManagerProxy::audioMediaSelectionIndexChanged(uint64_t contextId, uint64_t selectedIndex)
{
    ensureModel(contextId).audioMediaSelectionIndexChanged(selectedIndex);
}

void WebPlaybackSessionManagerProxy::legibleMediaSelectionIndexChanged(uint64_t contextId, uint64_t selectedIndex)
{
    ensureModel(contextId).legibleMediaSelectionIndexChanged(selectedIndex);
}

void WebPlaybackSessionManagerProxy::externalPlaybackPropertiesChanged(uint64_t contextId, bool enabled, uint32_t targetType, String localizedDeviceName)
{
    WebPlaybackSessionModel::ExternalPlaybackTargetType type = static_cast<WebPlaybackSessionModel::ExternalPlaybackTargetType>(targetType);
    ASSERT(type == WebPlaybackSessionModel::TargetTypeAirPlay || type == WebPlaybackSessionModel::TargetTypeTVOut || type == WebPlaybackSessionModel::TargetTypeNone);

    ensureModel(contextId).externalPlaybackChanged(enabled, type, localizedDeviceName);
}

void WebPlaybackSessionManagerProxy::wirelessVideoPlaybackDisabledChanged(uint64_t contextId, bool disabled)
{
    ensureModel(contextId).wirelessVideoPlaybackDisabledChanged(disabled);
}

void WebPlaybackSessionManagerProxy::mutedChanged(uint64_t contextId, bool muted)
{
    ensureModel(contextId).mutedChanged(muted);
}

void WebPlaybackSessionManagerProxy::durationChanged(uint64_t contextId, double duration)
{
    ensureModel(contextId).durationChanged(duration);
}

void WebPlaybackSessionManagerProxy::playbackStartedTimeChanged(uint64_t contextId, double playbackStartedTime)
{
    ensureModel(contextId).playbackStartedTimeChanged(playbackStartedTime);
}

void WebPlaybackSessionManagerProxy::rateChanged(uint64_t contextId, bool isPlaying, double rate)
{
    ensureModel(contextId).rateChanged(isPlaying, rate);
}


void WebPlaybackSessionManagerProxy::handleControlledElementIDResponse(uint64_t contextId, String identifier) const
{
#if PLATFORM(MAC)
    if (contextId == m_controlsManagerContextId)
        m_page->handleControlledElementIDResponse(identifier);
#else
    UNUSED_PARAM(contextId);
    UNUSED_PARAM(identifier);
#endif
}


#pragma mark Messages to WebPlaybackSessionManager

void WebPlaybackSessionManagerProxy::play(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::Play(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::pause(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::Pause(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::togglePlayState(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::TogglePlayState(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::beginScrubbing(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::BeginScrubbing(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::endScrubbing(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::EndScrubbing(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::seekToTime(uint64_t contextId, double time)
{
    m_page->send(Messages::WebPlaybackSessionManager::SeekToTime(contextId, time), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::fastSeek(uint64_t contextId, double time)
{
    m_page->send(Messages::WebPlaybackSessionManager::FastSeek(contextId, time), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::beginScanningForward(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::BeginScanningForward(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::beginScanningBackward(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::BeginScanningBackward(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::endScanning(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::EndScanning(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::selectAudioMediaOption(uint64_t contextId, uint64_t index)
{
    m_page->send(Messages::WebPlaybackSessionManager::SelectAudioMediaOption(contextId, index), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::selectLegibleMediaOption(uint64_t contextId, uint64_t index)
{
    m_page->send(Messages::WebPlaybackSessionManager::SelectLegibleMediaOption(contextId, index), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::togglePictureInPicture(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::TogglePictureInPicture(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::toggleMuted(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManager::ToggleMuted(contextId), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::setMuted(uint64_t contextId, bool muted)
{
    m_page->send(Messages::WebPlaybackSessionManager::SetMuted(contextId, muted), m_page->pageID());
}

void WebPlaybackSessionManagerProxy::requestControlledElementID()
{
    if (m_controlsManagerContextId)
        m_page->send(Messages::WebPlaybackSessionManager::HandleControlledElementIDRequest(m_controlsManagerContextId), m_page->pageID());
}

PlatformWebPlaybackSessionInterface* WebPlaybackSessionManagerProxy::controlsManagerInterface()
{
    if (!m_controlsManagerContextId)
        return nullptr;

    auto& interface = ensureInterface(m_controlsManagerContextId);
    return &interface;
}

} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
