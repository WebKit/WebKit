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
#import "PlaybackSessionManager.h"

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "Attachment.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "PlaybackSessionManagerMessages.h"
#import "PlaybackSessionManagerProxyMessages.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/Color.h>
#import <WebCore/ElementInlines.h>
#import <WebCore/Event.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLMediaElement.h>
#import <WebCore/Settings.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/UserGestureIndicator.h>
#import <mach/mach_port.h>
#import <wtf/LoggerHelper.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

#pragma mark - PlaybackSessionInterfaceContext

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionInterfaceContext);

PlaybackSessionInterfaceContext::PlaybackSessionInterfaceContext(PlaybackSessionManager& manager, PlaybackSessionContextIdentifier contextId)
    : m_manager(manager)
    , m_contextId(contextId)
{
}

PlaybackSessionInterfaceContext::~PlaybackSessionInterfaceContext()
{
}

void PlaybackSessionInterfaceContext::durationChanged(double duration)
{
    if (RefPtr manager = m_manager.get())
        manager->durationChanged(m_contextId, duration);
}

void PlaybackSessionInterfaceContext::currentTimeChanged(double currentTime, double anchorTime)
{
    if (RefPtr manager = m_manager.get())
        manager->currentTimeChanged(m_contextId, currentTime, anchorTime);
}

void PlaybackSessionInterfaceContext::bufferedTimeChanged(double bufferedTime)
{
    if (RefPtr manager = m_manager.get())
        manager->bufferedTimeChanged(m_contextId, bufferedTime);
}

void PlaybackSessionInterfaceContext::rateChanged(OptionSet<PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double defaultPlaybackRate)
{
    if (RefPtr manager = m_manager.get())
        manager->rateChanged(m_contextId, playbackState, playbackRate, defaultPlaybackRate);
}

void PlaybackSessionInterfaceContext::playbackStartedTimeChanged(double playbackStartedTime)
{
    if (RefPtr manager = m_manager.get())
        manager->playbackStartedTimeChanged(m_contextId, playbackStartedTime);
}

void PlaybackSessionInterfaceContext::seekableRangesChanged(const WebCore::TimeRanges& ranges, double lastModifiedTime, double liveUpdateInterval)
{
    if (RefPtr manager = m_manager.get())
        manager->seekableRangesChanged(m_contextId, ranges, lastModifiedTime, liveUpdateInterval);
}

void PlaybackSessionInterfaceContext::canPlayFastReverseChanged(bool value)
{
    if (RefPtr manager = m_manager.get())
        manager->canPlayFastReverseChanged(m_contextId, value);
}

void PlaybackSessionInterfaceContext::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    if (RefPtr manager = m_manager.get())
        manager->audioMediaSelectionOptionsChanged(m_contextId, options, selectedIndex);
}

void PlaybackSessionInterfaceContext::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    if (RefPtr manager = m_manager.get())
        manager->legibleMediaSelectionOptionsChanged(m_contextId, options, selectedIndex);
}

void PlaybackSessionInterfaceContext::audioMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    if (RefPtr manager = m_manager.get())
        manager->audioMediaSelectionIndexChanged(m_contextId, selectedIndex);
}

void PlaybackSessionInterfaceContext::legibleMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    if (RefPtr manager = m_manager.get())
        manager->legibleMediaSelectionIndexChanged(m_contextId, selectedIndex);
}

void PlaybackSessionInterfaceContext::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType type, const String& localizedDeviceName)
{
    if (RefPtr manager = m_manager.get())
        manager->externalPlaybackChanged(m_contextId, enabled, type, localizedDeviceName);
}

void PlaybackSessionInterfaceContext::wirelessVideoPlaybackDisabledChanged(bool disabled)
{
    if (RefPtr manager = m_manager.get())
        manager->wirelessVideoPlaybackDisabledChanged(m_contextId, disabled);
}

void PlaybackSessionInterfaceContext::mutedChanged(bool muted)
{
    if (RefPtr manager = m_manager.get())
        manager->mutedChanged(m_contextId, muted);
}

void PlaybackSessionInterfaceContext::isPictureInPictureSupportedChanged(bool supported)
{
    if (RefPtr manager = m_manager.get())
        manager->isPictureInPictureSupportedChanged(m_contextId, supported);
}

void PlaybackSessionInterfaceContext::volumeChanged(double volume)
{
    if (RefPtr manager = m_manager.get())
        manager->volumeChanged(m_contextId, volume);
}

void PlaybackSessionInterfaceContext::isInWindowFullscreenActiveChanged(bool isInWindow)
{
    if (RefPtr manager = m_manager.get())
        manager->isInWindowFullscreenActiveChanged(m_contextId, isInWindow);
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void PlaybackSessionInterfaceContext::spatialVideoMetadataChanged(const std::optional<WebCore::SpatialVideoMetadata>& metadata)
{
    if (m_manager)
        m_manager->spatialVideoMetadataChanged(m_contextId, metadata);
}
#endif

#pragma mark - PlaybackSessionManager

Ref<PlaybackSessionManager> PlaybackSessionManager::create(WebPage& page)
{
    return adoptRef(*new PlaybackSessionManager(page));
}

PlaybackSessionManager::PlaybackSessionManager(WebPage& page)
    : m_page(&page)
#if !RELEASE_LOG_DISABLED
    , m_logger(page.logger())
    , m_logIdentifier(page.logIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
    WebProcess::singleton().addMessageReceiver(Messages::PlaybackSessionManager::messageReceiverName(), page.identifier(), *this);
}

PlaybackSessionManager::~PlaybackSessionManager()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    for (auto [model, interface] : m_contextMap.values()) {
        model->removeClient(interface);
        model->setMediaElement(nullptr);

        interface->invalidate();
    }

    m_contextMap.clear();
    m_mediaElements.clear();
    m_clientCounts.clear();

    if (m_page)
        WebProcess::singleton().removeMessageReceiver(Messages::PlaybackSessionManager::messageReceiverName(), m_page->identifier());
}

void PlaybackSessionManager::invalidate()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    ASSERT(m_page);
    WebProcess::singleton().removeMessageReceiver(Messages::PlaybackSessionManager::messageReceiverName(), m_page->identifier());
    m_page = nullptr;
}

PlaybackSessionManager::ModelInterfaceTuple PlaybackSessionManager::createModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto model = PlaybackSessionModelMediaElement::create();
    auto interface = PlaybackSessionInterfaceContext::create(*this, contextId);
    model->addClient(interface.get());

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

const PlaybackSessionManager::ModelInterfaceTuple& PlaybackSessionManager::ensureModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_contextMap.ensure(contextId, [&] {
        return createModelAndInterface(contextId);
    });
    return addResult.iterator->value;
}

Ref<WebCore::PlaybackSessionModelMediaElement> PlaybackSessionManager::ensureModel(PlaybackSessionContextIdentifier contextId)
{
    return std::get<0>(ensureModelAndInterface(contextId));
}

Ref<PlaybackSessionInterfaceContext> PlaybackSessionManager::ensureInterface(PlaybackSessionContextIdentifier contextId)
{
    return std::get<1>(ensureModelAndInterface(contextId));
}

void PlaybackSessionManager::removeContext(PlaybackSessionContextIdentifier contextId)
{
    auto it = m_contextMap.find(contextId);
    if (it == m_contextMap.end())
        return;
    auto [model, interface] = it->value;

    model->removeClient(interface);
    interface->invalidate();
    m_contextMap.remove(it);

    RefPtr mediaElement = model->mediaElement();
    ASSERT(mediaElement);
    if (!mediaElement)
        return;

    model->setMediaElement(nullptr);
    m_mediaElements.remove(*mediaElement);
}

void PlaybackSessionManager::addClientForContext(PlaybackSessionContextIdentifier contextId)
{
    m_clientCounts.add(contextId);
}

void PlaybackSessionManager::removeClientForContext(PlaybackSessionContextIdentifier contextId)
{
    ASSERT(m_clientCounts.contains(contextId));
    if (m_clientCounts.remove(contextId))
        removeContext(contextId);
}

void PlaybackSessionManager::setUpPlaybackControlsManager(WebCore::HTMLMediaElement& mediaElement)
{
    auto contextId = mediaElement.identifier();
    auto result = m_mediaElements.add(mediaElement);
    if (result.isNewEntry) {
        ensureModel(contextId)->setMediaElement(&mediaElement);
#if !RELEASE_LOG_DISABLED
        sendLogIdentifierForMediaElement(mediaElement);
#endif
    }

    if (m_controlsManagerContextId == contextId)
        return;

    auto previousContextId = m_controlsManagerContextId;
    m_controlsManagerContextId = contextId;
    if (previousContextId)
        removeClientForContext(*previousContextId);

    addClientForContext(*m_controlsManagerContextId);

    m_page->videoControlsManagerDidChange();
    m_page->send(Messages::PlaybackSessionManagerProxy::SetUpPlaybackControlsManagerWithID(*m_controlsManagerContextId, mediaElement.isVideo()));
}

void PlaybackSessionManager::clearPlaybackControlsManager()
{
    if (!m_controlsManagerContextId)
        return;

    removeClientForContext(*m_controlsManagerContextId);
    m_controlsManagerContextId = std::nullopt;

    m_page->videoControlsManagerDidChange();
    m_page->send(Messages::PlaybackSessionManagerProxy::ClearPlaybackControlsManager());
}

void PlaybackSessionManager::mediaEngineChanged(HTMLMediaElement& mediaElement)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    RefPtr player = mediaElement.protectedPlayer();
    bool supportsLinearMediaPlayer = player && player->supportsLinearMediaPlayer();
    Ref { *m_page }->send(Messages::PlaybackSessionManagerProxy::SupportsLinearMediaPlayerChanged(mediaElement.identifier(), supportsLinearMediaPlayer));
#else
    UNUSED_PARAM(mediaElement);
#endif

    // FIXME: mediaEngineChanged is called whenever an HTMLMediaElement's media engine changes, but
    // that element's identifier may not match m_controlsManagerContextId. That means that (a) the
    // current playback controls element's PlaybackSessionModel is notified whenever *any* element
    // changes its media engine, and (b) mediaElement's PlaybackSessionModel is *never* notified if
    // it's not the current playback controls element.

    if (!m_controlsManagerContextId)
        return;

    auto it = m_contextMap.find(*m_controlsManagerContextId);
    if (it == m_contextMap.end())
        return;

    std::get<0>(it->value)->mediaEngineChanged();
}

PlaybackSessionContextIdentifier PlaybackSessionManager::contextIdForMediaElement(WebCore::HTMLMediaElement& mediaElement)
{
    auto contextId = mediaElement.identifier();
    ensureModel(contextId)->setMediaElement(&mediaElement);
    return contextId;
}

WebCore::HTMLMediaElement* PlaybackSessionManager::currentPlaybackControlsElement() const
{
    if (!m_controlsManagerContextId)
        return nullptr;

    auto iter = m_contextMap.find(*m_controlsManagerContextId);
    if (iter == m_contextMap.end())
        return nullptr;

    return std::get<0>(iter->value)->mediaElement();
}

#pragma mark Interface to PlaybackSessionInterfaceContext:

void PlaybackSessionManager::durationChanged(PlaybackSessionContextIdentifier contextId, double duration)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::DurationChanged(contextId, duration));
}

void PlaybackSessionManager::currentTimeChanged(PlaybackSessionContextIdentifier contextId, double currentTime, double anchorTime)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::CurrentTimeChanged(contextId, currentTime, anchorTime));
}

void PlaybackSessionManager::bufferedTimeChanged(PlaybackSessionContextIdentifier contextId, double bufferedTime)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::BufferedTimeChanged(contextId, bufferedTime));
}

void PlaybackSessionManager::playbackStartedTimeChanged(PlaybackSessionContextIdentifier contextId, double playbackStartedTime)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::PlaybackStartedTimeChanged(contextId, playbackStartedTime));
}

void PlaybackSessionManager::rateChanged(PlaybackSessionContextIdentifier contextId, OptionSet<PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double defaultPlaybackRate)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::RateChanged(contextId, playbackState, playbackRate, defaultPlaybackRate));
}

void PlaybackSessionManager::seekableRangesChanged(PlaybackSessionContextIdentifier contextId, const WebCore::TimeRanges& timeRanges, double lastModifiedTime, double liveUpdateInterval)
{
    Vector<std::pair<double, double>> rangesVector;
    for (unsigned i = 0; i < timeRanges.length(); i++) {
        double start = timeRanges.ranges().start(i).toDouble();
        double end = timeRanges.ranges().end(i).toDouble();
        rangesVector.append({ start, end });
    }
    m_page->send(Messages::PlaybackSessionManagerProxy::SeekableRangesVectorChanged(contextId, WTFMove(rangesVector), lastModifiedTime, liveUpdateInterval));
}

void PlaybackSessionManager::canPlayFastReverseChanged(PlaybackSessionContextIdentifier contextId, bool value)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::CanPlayFastReverseChanged(contextId, value));
}

void PlaybackSessionManager::audioMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier contextId, const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::AudioMediaSelectionOptionsChanged(contextId, options, selectedIndex));
}

void PlaybackSessionManager::legibleMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier contextId, const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::LegibleMediaSelectionOptionsChanged(contextId, options, selectedIndex));
}

void PlaybackSessionManager::externalPlaybackChanged(PlaybackSessionContextIdentifier contextId, bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType targetType, String localizedDeviceName)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::ExternalPlaybackPropertiesChanged(contextId, enabled, targetType, localizedDeviceName));
}

void PlaybackSessionManager::audioMediaSelectionIndexChanged(PlaybackSessionContextIdentifier contextId, uint64_t selectedIndex)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::AudioMediaSelectionIndexChanged(contextId, selectedIndex));
}

void PlaybackSessionManager::legibleMediaSelectionIndexChanged(PlaybackSessionContextIdentifier contextId, uint64_t selectedIndex)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::LegibleMediaSelectionIndexChanged(contextId, selectedIndex));
}

void PlaybackSessionManager::wirelessVideoPlaybackDisabledChanged(PlaybackSessionContextIdentifier contextId, bool disabled)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::WirelessVideoPlaybackDisabledChanged(contextId, disabled));
}

void PlaybackSessionManager::mutedChanged(PlaybackSessionContextIdentifier contextId, bool muted)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::MutedChanged(contextId, muted));
}

void PlaybackSessionManager::volumeChanged(PlaybackSessionContextIdentifier contextId, double volume)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::VolumeChanged(contextId, volume));
}

void PlaybackSessionManager::isPictureInPictureSupportedChanged(PlaybackSessionContextIdentifier contextId, bool supported)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::PictureInPictureSupportedChanged(contextId, supported));
}

void PlaybackSessionManager::isInWindowFullscreenActiveChanged(PlaybackSessionContextIdentifier contextId, bool inWindow)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::IsInWindowFullscreenActiveChanged(contextId, inWindow));
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void PlaybackSessionManager::spatialVideoMetadataChanged(PlaybackSessionContextIdentifier contextId, const std::optional<WebCore::SpatialVideoMetadata>& metadata)
{
    m_page->send(Messages::PlaybackSessionManagerProxy::SpatialVideoMetadataChanged(contextId, metadata));
}
#endif

#pragma mark Messages from PlaybackSessionManagerProxy:

void PlaybackSessionManager::play(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->play();
}

void PlaybackSessionManager::pause(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->pause();
}

void PlaybackSessionManager::togglePlayState(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->togglePlayState();
}

void PlaybackSessionManager::beginScrubbing(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->beginScrubbing();
}

void PlaybackSessionManager::endScrubbing(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->endScrubbing();
}

void PlaybackSessionManager::seekToTime(PlaybackSessionContextIdentifier contextId, double time, double toleranceBefore, double toleranceAfter)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->seekToTime(time, toleranceBefore, toleranceAfter);
}

void PlaybackSessionManager::fastSeek(PlaybackSessionContextIdentifier contextId, double time)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->fastSeek(time);
}

void PlaybackSessionManager::beginScanningForward(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->beginScanningForward();
}

void PlaybackSessionManager::beginScanningBackward(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->beginScanningBackward();
}

void PlaybackSessionManager::endScanning(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->endScanning();
}

void PlaybackSessionManager::setDefaultPlaybackRate(PlaybackSessionContextIdentifier contextId, float defaultPlaybackRate)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->setDefaultPlaybackRate(defaultPlaybackRate);
}

void PlaybackSessionManager::setPlaybackRate(PlaybackSessionContextIdentifier contextId, float playbackRate)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->setPlaybackRate(playbackRate);
}

void PlaybackSessionManager::selectAudioMediaOption(PlaybackSessionContextIdentifier contextId, uint64_t index)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->selectAudioMediaOption(index);
}

void PlaybackSessionManager::selectLegibleMediaOption(PlaybackSessionContextIdentifier contextId, uint64_t index)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    Ref model = ensureModel(contextId);
    model->selectLegibleMediaOption(index);

    // Selecting a text track may not result in a call to legibleMediaSelectionIndexChanged(), so just
    // artificially trigger it here:
    legibleMediaSelectionIndexChanged(contextId, model->legibleMediaSelectedIndex());
}

void PlaybackSessionManager::handleControlledElementIDRequest(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr element = ensureModel(contextId)->mediaElement())
        m_page->send(Messages::PlaybackSessionManagerProxy::HandleControlledElementIDResponse(contextId, element->getIdAttribute()));
}

void PlaybackSessionManager::togglePictureInPicture(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->togglePictureInPicture();
}

void PlaybackSessionManager::enterFullscreen(PlaybackSessionContextIdentifier contextId)
{
    ensureModel(contextId)->enterFullscreen();
}

void PlaybackSessionManager::exitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    ensureModel(contextId)->exitFullscreen();
}

void PlaybackSessionManager::enterInWindow(PlaybackSessionContextIdentifier contextId)
{
    ensureModel(contextId)->enterInWindowFullscreen();
}

void PlaybackSessionManager::exitInWindow(PlaybackSessionContextIdentifier contextId)
{
    ensureModel(contextId)->exitInWindowFullscreen();
}

void PlaybackSessionManager::toggleMuted(PlaybackSessionContextIdentifier contextId)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->toggleMuted();
}

void PlaybackSessionManager::setMuted(PlaybackSessionContextIdentifier contextId, bool muted)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->setMuted(muted);
}

void PlaybackSessionManager::setVolume(PlaybackSessionContextIdentifier contextId, double volume)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->setVolume(volume);
}

void PlaybackSessionManager::setPlayingOnSecondScreen(PlaybackSessionContextIdentifier contextId, bool value)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->setPlayingOnSecondScreen(value);
}

void PlaybackSessionManager::sendRemoteCommand(PlaybackSessionContextIdentifier contextId, WebCore::PlatformMediaSession::RemoteControlCommandType command, const WebCore::PlatformMediaSession::RemoteCommandArgument& argument)
{
    UserGestureIndicator indicator(IsProcessingUserGesture::Yes);
    ensureModel(contextId)->sendRemoteCommand(command, argument);
}

void PlaybackSessionManager::setSoundStageSize(PlaybackSessionContextIdentifier contextId, WebCore::AudioSessionSoundStageSize size)
{
    ensureModel(contextId)->setSoundStageSize(size);
    auto maxSize = size;
    forEachModel([&] (auto& model) {
        if (model.soundStageSize() > maxSize)
            maxSize = model.soundStageSize();
    });
    AudioSession::sharedSession().setSoundStageSize(maxSize);
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void PlaybackSessionManager::setSpatialTrackingLabel(PlaybackSessionContextIdentifier contextId, const String& label)
{
    ensureModel(contextId)->setSpatialTrackingLabel(label);
}
#endif

void PlaybackSessionManager::forEachModel(Function<void(PlaybackSessionModel&)>&& callback)
{
    for (auto [model, interface] : m_contextMap.values()) {
        UNUSED_PARAM(interface);
        callback(model);
    }
}

#if !RELEASE_LOG_DISABLED
void PlaybackSessionManager::sendLogIdentifierForMediaElement(HTMLMediaElement& mediaElement)
{
    auto contextId = contextIdForMediaElement(mediaElement);
    m_page->send(Messages::PlaybackSessionManagerProxy::SetLogIdentifier(contextId, reinterpret_cast<uint64_t>(mediaElement.logIdentifier())));
}

WTFLogChannel& PlaybackSessionManager::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
