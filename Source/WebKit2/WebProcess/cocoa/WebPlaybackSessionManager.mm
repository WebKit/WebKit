/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebPlaybackSessionManager.h"


#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "Attachment.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebPlaybackSessionManagerMessages.h"
#import "WebPlaybackSessionManagerProxyMessages.h"
#import "WebProcess.h"
#import <WebCore/Color.h>
#import <WebCore/Event.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLMediaElement.h>
#import <WebCore/Settings.h>
#import <WebCore/TimeRanges.h>
#import <mach/mach_port.h>

using namespace WebCore;

namespace WebKit {

static uint64_t nextContextId()
{
    static uint64_t contextId = 0;
    return ++contextId;
}

#pragma mark - WebPlaybackSessionInterfaceContext

WebPlaybackSessionInterfaceContext::WebPlaybackSessionInterfaceContext(WebPlaybackSessionManager& manager, uint64_t contextId)
    : m_manager(&manager)
    , m_contextId(contextId)
{
}

WebPlaybackSessionInterfaceContext::~WebPlaybackSessionInterfaceContext()
{
}

void WebPlaybackSessionInterfaceContext::resetMediaState()
{
    if (m_manager)
        m_manager->resetMediaState(m_contextId);
}

void WebPlaybackSessionInterfaceContext::setDuration(double duration)
{
    if (m_manager)
        m_manager->setDuration(m_contextId, duration);
}

void WebPlaybackSessionInterfaceContext::setCurrentTime(double currentTime, double anchorTime)
{
    if (m_manager)
        m_manager->setCurrentTime(m_contextId, currentTime, anchorTime);
}

void WebPlaybackSessionInterfaceContext::setBufferedTime(double bufferedTime)
{
    if (m_manager)
        m_manager->setBufferedTime(m_contextId, bufferedTime);
}

void WebPlaybackSessionInterfaceContext::setRate(bool isPlaying, float playbackRate)
{
    if (m_manager)
        m_manager->setRate(m_contextId, isPlaying, playbackRate);
}

void WebPlaybackSessionInterfaceContext::setSeekableRanges(const WebCore::TimeRanges& ranges)
{
    if (m_manager)
        m_manager->setSeekableRanges(m_contextId, ranges);
}

void WebPlaybackSessionInterfaceContext::setCanPlayFastReverse(bool value)
{
    if (m_manager)
        m_manager->setCanPlayFastReverse(m_contextId, value);
}

void WebPlaybackSessionInterfaceContext::setAudioMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex)
{
    if (m_manager)
        m_manager->setAudioMediaSelectionOptions(m_contextId, options, selectedIndex);
}

void WebPlaybackSessionInterfaceContext::setLegibleMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex)
{
    if (m_manager)
        m_manager->setLegibleMediaSelectionOptions(m_contextId, options, selectedIndex);
}

void WebPlaybackSessionInterfaceContext::setExternalPlayback(bool enabled, ExternalPlaybackTargetType type, WTF::String localizedDeviceName)
{
    if (m_manager)
        m_manager->setExternalPlayback(m_contextId, enabled, type, localizedDeviceName);
}

void WebPlaybackSessionInterfaceContext::setWirelessVideoPlaybackDisabled(bool disabled)
{
    if (m_manager)
        m_manager->setWirelessVideoPlaybackDisabled(m_contextId, disabled);
}

#pragma mark - WebPlaybackSessionManager

Ref<WebPlaybackSessionManager> WebPlaybackSessionManager::create(WebPage& page)
{
    return adoptRef(*new WebPlaybackSessionManager(page));
}

WebPlaybackSessionManager::WebPlaybackSessionManager(WebPage& page)
    : m_page(&page)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebPlaybackSessionManager::messageReceiverName(), page.pageID(), *this);
}

WebPlaybackSessionManager::~WebPlaybackSessionManager()
{
    for (auto& tuple : m_contextMap.values()) {
        RefPtr<WebPlaybackSessionModelMediaElement> model;
        RefPtr<WebPlaybackSessionInterfaceContext> interface;
        std::tie(model, interface) = tuple;

        model->setWebPlaybackSessionInterface(nullptr);
        model->setMediaElement(nullptr);

        interface->invalidate();
    }

    m_contextMap.clear();
    m_mediaElements.clear();
    m_clientCounts.clear();

    WebProcess::singleton().removeMessageReceiver(Messages::WebPlaybackSessionManager::messageReceiverName(), m_page->pageID());
}

WebPlaybackSessionManager::ModelInterfaceTuple WebPlaybackSessionManager::createModelAndInterface(uint64_t contextId)
{
    RefPtr<WebPlaybackSessionModelMediaElement> model = WebPlaybackSessionModelMediaElement::create();
    RefPtr<WebPlaybackSessionInterfaceContext> interface = WebPlaybackSessionInterfaceContext::create(*this, contextId);

    model->setWebPlaybackSessionInterface(interface.get());

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

WebPlaybackSessionManager::ModelInterfaceTuple& WebPlaybackSessionManager::ensureModelAndInterface(uint64_t contextId)
{
    auto addResult = m_contextMap.add(contextId, ModelInterfaceTuple());
    if (addResult.isNewEntry)
        addResult.iterator->value = createModelAndInterface(contextId);
    return addResult.iterator->value;
}

WebCore::WebPlaybackSessionModelMediaElement& WebPlaybackSessionManager::ensureModel(uint64_t contextId)
{
    return *std::get<0>(ensureModelAndInterface(contextId));
}

WebPlaybackSessionInterfaceContext& WebPlaybackSessionManager::ensureInterface(uint64_t contextId)
{
    return *std::get<1>(ensureModelAndInterface(contextId));
}

void WebPlaybackSessionManager::removeContext(uint64_t contextId)
{
    RefPtr<WebPlaybackSessionModelMediaElement> model;
    RefPtr<WebPlaybackSessionInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    RefPtr<HTMLMediaElement> mediaElement = model->mediaElement();
    model->setMediaElement(nullptr);
    model->setWebPlaybackSessionInterface(nullptr);
    interface->invalidate();
    m_mediaElements.remove(mediaElement.get());
    m_contextMap.remove(contextId);
}

void WebPlaybackSessionManager::addClientForContext(uint64_t contextId)
{
    auto addResult = m_clientCounts.add(contextId, 1);
    if (!addResult.isNewEntry)
        addResult.iterator->value++;
}

void WebPlaybackSessionManager::removeClientForContext(uint64_t contextId)
{
    ASSERT(m_clientCounts.contains(contextId));

    int clientCount = m_clientCounts.get(contextId);
    ASSERT(clientCount > 0);
    clientCount--;

    if (clientCount <= 0) {
        m_clientCounts.remove(contextId);
        removeContext(contextId);
        return;
    }

    m_clientCounts.set(contextId, clientCount);
}

void WebPlaybackSessionManager::setUpPlaybackControlsManager(WebCore::HTMLMediaElement& mediaElement)
{
#if PLATFORM(MAC)
    if (m_mediaElements.contains(&mediaElement)) {
        uint64_t contextId = m_mediaElements.get(&mediaElement);
        if (m_controlsManagerContextId == contextId)
            return;

        if (m_controlsManagerContextId)
            removeClientForContext(m_controlsManagerContextId);
        m_controlsManagerContextId = contextId;
    } else {
        auto addResult = m_mediaElements.ensure(&mediaElement, [&] { return nextContextId(); });
        auto contextId = addResult.iterator->value;
        m_controlsManagerContextId = contextId;
        ensureModel(contextId).setMediaElement(&mediaElement);
    }

    addClientForContext(m_controlsManagerContextId);
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetUpPlaybackControlsManagerWithID(m_controlsManagerContextId), m_page->pageID());
#endif
}

void WebPlaybackSessionManager::clearPlaybackControlsManager(WebCore::HTMLMediaElement& mediaElement)
{
#if PLATFORM(MAC)
    if (!m_mediaElements.contains(&mediaElement))
        return;

    uint64_t contextId = m_mediaElements.get(&mediaElement);
    if (m_controlsManagerContextId != contextId)
        return;

    removeClientForContext(m_controlsManagerContextId);
    m_controlsManagerContextId = 0;
    m_page->send(Messages::WebPlaybackSessionManagerProxy::ClearPlaybackControlsManager(), m_page->pageID());
#endif
}

uint64_t WebPlaybackSessionManager::contextIdForMediaElement(WebCore::HTMLMediaElement& mediaElement)
{
    auto addResult = m_mediaElements.ensure(&mediaElement, [&] { return nextContextId(); });
    return addResult.iterator->value;
}

#pragma mark Interface to WebPlaybackSessionInterfaceContext:

void WebPlaybackSessionManager::resetMediaState(uint64_t contextId)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::ResetMediaState(contextId), m_page->pageID());
}

void WebPlaybackSessionManager::setDuration(uint64_t contextId, double duration)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetDuration(contextId, duration), m_page->pageID());
}

void WebPlaybackSessionManager::setCurrentTime(uint64_t contextId, double currentTime, double anchorTime)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetCurrentTime(contextId, currentTime, anchorTime), m_page->pageID());
}

void WebPlaybackSessionManager::setBufferedTime(uint64_t contextId, double bufferedTime)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetBufferedTime(contextId, bufferedTime), m_page->pageID());
}

void WebPlaybackSessionManager::setRate(uint64_t contextId, bool isPlaying, float playbackRate)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetRate(contextId, isPlaying, playbackRate), m_page->pageID());
}

void WebPlaybackSessionManager::setSeekableRanges(uint64_t contextId, const WebCore::TimeRanges& timeRanges)
{
    Vector<std::pair<double, double>> rangesVector;

    for (unsigned i = 0; i < timeRanges.length(); i++) {
        ExceptionCode exceptionCode;
        double start = timeRanges.start(i, exceptionCode);
        double end = timeRanges.end(i, exceptionCode);
        rangesVector.append(std::pair<double, double>(start, end));
    }

    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetSeekableRangesVector(contextId, WTFMove(rangesVector)), m_page->pageID());
}

void WebPlaybackSessionManager::setCanPlayFastReverse(uint64_t contextId, bool value)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetCanPlayFastReverse(contextId, value), m_page->pageID());
}

void WebPlaybackSessionManager::setAudioMediaSelectionOptions(uint64_t contextId, const Vector<String>& options, uint64_t selectedIndex)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetAudioMediaSelectionOptions(contextId, options, selectedIndex), m_page->pageID());
}

void WebPlaybackSessionManager::setLegibleMediaSelectionOptions(uint64_t contextId, const Vector<String>& options, uint64_t selectedIndex)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetLegibleMediaSelectionOptions(contextId, options, selectedIndex), m_page->pageID());
}

void WebPlaybackSessionManager::setExternalPlayback(uint64_t contextId, bool enabled, WebPlaybackSessionInterface::ExternalPlaybackTargetType targetType, String localizedDeviceName)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetExternalPlaybackProperties(contextId, enabled, static_cast<uint32_t>(targetType), localizedDeviceName), m_page->pageID());
}

void WebPlaybackSessionManager::setWirelessVideoPlaybackDisabled(uint64_t contextId, bool disabled)
{
    m_page->send(Messages::WebPlaybackSessionManagerProxy::SetWirelessVideoPlaybackDisabled(contextId, disabled));
}

#pragma mark Messages from WebPlaybackSessionManagerProxy:

void WebPlaybackSessionManager::play(uint64_t contextId)
{
    ensureModel(contextId).play();
}

void WebPlaybackSessionManager::pause(uint64_t contextId)
{
    ensureModel(contextId).pause();
}

void WebPlaybackSessionManager::togglePlayState(uint64_t contextId)
{
    ensureModel(contextId).togglePlayState();
}

void WebPlaybackSessionManager::beginScrubbing(uint64_t contextId)
{
    ensureModel(contextId).beginScrubbing();
}

void WebPlaybackSessionManager::endScrubbing(uint64_t contextId)
{
    ensureModel(contextId).endScrubbing();
}

void WebPlaybackSessionManager::seekToTime(uint64_t contextId, double time)
{
    ensureModel(contextId).seekToTime(time);
}

void WebPlaybackSessionManager::fastSeek(uint64_t contextId, double time)
{
    ensureModel(contextId).fastSeek(time);
}

void WebPlaybackSessionManager::beginScanningForward(uint64_t contextId)
{
    ensureModel(contextId).beginScanningForward();
}

void WebPlaybackSessionManager::beginScanningBackward(uint64_t contextId)
{
    ensureModel(contextId).beginScanningBackward();
}

void WebPlaybackSessionManager::endScanning(uint64_t contextId)
{
    ensureModel(contextId).endScanning();
}

void WebPlaybackSessionManager::selectAudioMediaOption(uint64_t contextId, uint64_t index)
{
    ensureModel(contextId).selectAudioMediaOption(index);
}

void WebPlaybackSessionManager::selectLegibleMediaOption(uint64_t contextId, uint64_t index)
{
    ensureModel(contextId).selectLegibleMediaOption(index);
}

} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
