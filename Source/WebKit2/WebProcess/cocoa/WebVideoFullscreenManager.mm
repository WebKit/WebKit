/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WebVideoFullscreenManager.h"

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "Attachment.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebProcess.h"
#import "WebVideoFullscreenManagerMessages.h"
#import "WebVideoFullscreenManagerProxyMessages.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/Color.h>
#import <WebCore/Event.h>
#import <WebCore/EventNames.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/PlatformCALayer.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderLayerBacking.h>
#import <WebCore/RenderView.h>
#import <WebCore/Settings.h>
#import <WebCore/TimeRanges.h>
#import <mach/mach_port.h>

using namespace WebCore;

namespace WebKit {

static IntRect clientRectForElement(HTMLElement* element)
{
    if (!element)
        return IntRect();

    return element->clientRect();
}

static uint64_t nextContextId()
{
    static uint64_t contextId = 0;
    return ++contextId;
}

#pragma mark - WebVideoFullscreenInterfaceContext

WebVideoFullscreenInterfaceContext::WebVideoFullscreenInterfaceContext(WebVideoFullscreenManager& manager, uint64_t contextId)
    : m_manager(&manager)
    , m_contextId(contextId)
{
}

WebVideoFullscreenInterfaceContext::~WebVideoFullscreenInterfaceContext()
{
}

void WebVideoFullscreenInterfaceContext::setLayerHostingContext(std::unique_ptr<LayerHostingContext>&& context)
{
    m_layerHostingContext = WTFMove(context);
}

void WebVideoFullscreenInterfaceContext::resetMediaState()
{
    if (m_manager)
        m_manager->resetMediaState(m_contextId);
}

void WebVideoFullscreenInterfaceContext::setDuration(double duration)
{
    if (m_manager)
        m_manager->setDuration(m_contextId, duration);
}

void WebVideoFullscreenInterfaceContext::setCurrentTime(double currentTime, double anchorTime)
{
    if (m_manager)
        m_manager->setCurrentTime(m_contextId, currentTime, anchorTime);
}

void WebVideoFullscreenInterfaceContext::setBufferedTime(double bufferedTime)
{
    if (m_manager)
        m_manager->setBufferedTime(m_contextId, bufferedTime);
}

void WebVideoFullscreenInterfaceContext::setRate(bool isPlaying, float playbackRate)
{
    if (m_manager)
        m_manager->setRate(m_contextId, isPlaying, playbackRate);
}

void WebVideoFullscreenInterfaceContext::setVideoDimensions(bool hasVideo, float width, float height)
{
    if (m_manager)
        m_manager->setVideoDimensions(m_contextId, hasVideo, width, height);
}

void WebVideoFullscreenInterfaceContext::setSeekableRanges(const WebCore::TimeRanges& ranges)
{
    if (m_manager)
        m_manager->setSeekableRanges(m_contextId, ranges);
}

void WebVideoFullscreenInterfaceContext::setCanPlayFastReverse(bool value)
{
    if (m_manager)
        m_manager->setCanPlayFastReverse(m_contextId, value);
}

void WebVideoFullscreenInterfaceContext::setAudioMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex)
{
    if (m_manager)
        m_manager->setAudioMediaSelectionOptions(m_contextId, options, selectedIndex);
}

void WebVideoFullscreenInterfaceContext::setLegibleMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex)
{
    if (m_manager)
        m_manager->setLegibleMediaSelectionOptions(m_contextId, options, selectedIndex);
}

void WebVideoFullscreenInterfaceContext::setExternalPlayback(bool enabled, ExternalPlaybackTargetType type, WTF::String localizedDeviceName)
{
    if (m_manager)
        m_manager->setExternalPlayback(m_contextId, enabled, type, localizedDeviceName);
}

void WebVideoFullscreenInterfaceContext::setWirelessVideoPlaybackDisabled(bool disabled)
{
    if (m_manager)
        m_manager->setWirelessVideoPlaybackDisabled(m_contextId, disabled);
}

#pragma mark - WebVideoFullscreenManager

Ref<WebVideoFullscreenManager> WebVideoFullscreenManager::create(PassRefPtr<WebPage> page)
{
    return adoptRef(*new WebVideoFullscreenManager(page));
}

WebVideoFullscreenManager::WebVideoFullscreenManager(PassRefPtr<WebPage> page)
    : m_page(page.get())
{
    WebProcess::singleton().addMessageReceiver(Messages::WebVideoFullscreenManager::messageReceiverName(), page->pageID(), *this);
}

WebVideoFullscreenManager::~WebVideoFullscreenManager()
{
    for (auto& tuple : m_contextMap.values()) {
        RefPtr<WebVideoFullscreenModelVideoElement> model;
        RefPtr<WebVideoFullscreenInterfaceContext> interface;
        std::tie(model, interface) = tuple;

        model->setWebVideoFullscreenInterface(nullptr);
        model->setVideoElement(nullptr);

        interface->invalidate();
    }

    m_contextMap.clear();
    m_videoElements.clear();
    m_clientCounts.clear();

    WebProcess::singleton().removeMessageReceiver(Messages::WebVideoFullscreenManager::messageReceiverName(), m_page->pageID());
}

WebVideoFullscreenManager::ModelInterfaceTuple WebVideoFullscreenManager::createModelAndInterface(uint64_t contextId)
{
    RefPtr<WebVideoFullscreenModelVideoElement> model = WebVideoFullscreenModelVideoElement::create();
    RefPtr<WebVideoFullscreenInterfaceContext> interface = WebVideoFullscreenInterfaceContext::create(*this, contextId);

    interface->setLayerHostingContext(LayerHostingContext::createForExternalHostingProcess());
    model->setWebVideoFullscreenInterface(interface.get());

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

WebVideoFullscreenManager::ModelInterfaceTuple& WebVideoFullscreenManager::ensureModelAndInterface(uint64_t contextId)
{
    auto addResult = m_contextMap.add(contextId, ModelInterfaceTuple());
    if (addResult.isNewEntry)
        addResult.iterator->value = createModelAndInterface(contextId);
    return addResult.iterator->value;
}

WebCore::WebVideoFullscreenModelVideoElement& WebVideoFullscreenManager::ensureModel(uint64_t contextId)
{
    return *std::get<0>(ensureModelAndInterface(contextId));
}

WebVideoFullscreenInterfaceContext& WebVideoFullscreenManager::ensureInterface(uint64_t contextId)
{
    return *std::get<1>(ensureModelAndInterface(contextId));
}

void WebVideoFullscreenManager::removeContext(uint64_t contextId)
{
    RefPtr<WebVideoFullscreenModelVideoElement> model;
    RefPtr<WebVideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    RefPtr<HTMLVideoElement> videoElement = model->videoElement();
    model->setVideoElement(nullptr);
    model->setWebVideoFullscreenInterface(nullptr);
    interface->invalidate();
    m_videoElements.remove(videoElement.get());
    m_contextMap.remove(contextId);
}

void WebVideoFullscreenManager::addClientForContext(uint64_t contextId)
{
    auto addResult = m_clientCounts.add(contextId, 1);
    if (!addResult.isNewEntry)
        addResult.iterator->value++;
}

void WebVideoFullscreenManager::removeClientForContext(uint64_t contextId)
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

#pragma mark Interface to ChromeClient:

bool WebVideoFullscreenManager::supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
#if PLATFORM(IOS)
    UNUSED_PARAM(mode);
    return Settings::avKitEnabled();
#elif USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebVideoFullscreenManagerSupportsVideoFullscreenMac.mm>
#else
    return false;
#endif
}

void WebVideoFullscreenManager::enterVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT(mode != HTMLMediaElementEnums::VideoFullscreenModeNone);

    uint64_t contextId;

    auto addResult = m_videoElements.add(&videoElement, 0);
    if (addResult.isNewEntry)
        addResult.iterator->value = nextContextId();
    contextId = addResult.iterator->value;

    RefPtr<WebVideoFullscreenModelVideoElement> model;
    RefPtr<WebVideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);
    addClientForContext(contextId);
    if (!interface->layerHostingContext())
        interface->setLayerHostingContext(LayerHostingContext::createForExternalHostingProcess());

    FloatRect clientRect = clientRectForElement(&videoElement);
    FloatRect videoLayerFrame = FloatRect(0, 0, clientRect.width(), clientRect.height());

    HTMLMediaElementEnums::VideoFullscreenMode oldMode = interface->fullscreenMode();
    interface->setTargetIsFullscreen(true);
    interface->setFullscreenMode(mode);
    model->setVideoElement(&videoElement);
    if (oldMode == HTMLMediaElementEnums::VideoFullscreenModeNone)
        model->setVideoLayerFrame(videoLayerFrame);

    if (interface->isAnimating())
        return;
    interface->setIsAnimating(true);

    bool allowsPictureInPicture = videoElement.mediaSession().allowsPictureInPicture(videoElement);
    
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetupFullscreenWithID(contextId, interface->layerHostingContext()->contextID(), clientRectForElement(&videoElement), m_page->deviceScaleFactor(), interface->fullscreenMode(), allowsPictureInPicture), m_page->pageID());
}

void WebVideoFullscreenManager::exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement& videoElement)
{
    ASSERT(m_videoElements.contains(&videoElement));

    uint64_t contextId = m_videoElements.get(&videoElement);
    auto& interface = ensureInterface(contextId);

    interface.setTargetIsFullscreen(false);

    if (interface.isAnimating())
        return;

    interface.setIsAnimating(true);
    m_page->send(Messages::WebVideoFullscreenManagerProxy::ExitFullscreen(contextId, clientRectForElement(&videoElement)), m_page->pageID());
}

void WebVideoFullscreenManager::setUpVideoControlsManager(WebCore::HTMLVideoElement& videoElement)
{
#if PLATFORM(MAC)
    if (m_videoElements.contains(&videoElement)) {
        uint64_t contextId = m_videoElements.get(&videoElement);
        if (m_controlsManagerContextId == contextId)
            return;

        if (m_controlsManagerContextId)
            removeClientForContext(m_controlsManagerContextId);
        m_controlsManagerContextId = contextId;
    } else {
        auto addResult = m_videoElements.ensure(&videoElement, [&] { return nextContextId(); });
        auto contextId = addResult.iterator->value;
        m_controlsManagerContextId = contextId;
        ensureModel(contextId).setVideoElement(&videoElement);
    }

    addClientForContext(m_controlsManagerContextId);
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetUpVideoControlsManagerWithID(m_controlsManagerContextId), m_page->pageID());
#endif
}

void WebVideoFullscreenManager::exitVideoFullscreenToModeWithoutAnimation(WebCore::HTMLVideoElement& videoElement, WebCore::HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    ASSERT(m_videoElements.contains(&videoElement));

    uint64_t contextId = m_videoElements.get(&videoElement);
    auto& interface = ensureInterface(contextId);

    interface.setTargetIsFullscreen(false);

    m_page->send(Messages::WebVideoFullscreenManagerProxy::ExitFullscreenWithoutAnimationToMode(contextId, targetMode), m_page->pageID());
#else
    UNUSED_PARAM(videoElement);
    UNUSED_PARAM(targetMode);
#endif
}

#pragma mark Interface to WebVideoFullscreenInterfaceContext:

void WebVideoFullscreenManager::resetMediaState(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::ResetMediaState(contextId), m_page->pageID());
}
    
void WebVideoFullscreenManager::setDuration(uint64_t contextId, double duration)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetDuration(contextId, duration), m_page->pageID());
}

void WebVideoFullscreenManager::setCurrentTime(uint64_t contextId, double currentTime, double anchorTime)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetCurrentTime(contextId, currentTime, anchorTime), m_page->pageID());
}

void WebVideoFullscreenManager::setBufferedTime(uint64_t contextId, double bufferedTime)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetBufferedTime(contextId, bufferedTime), m_page->pageID());
}

void WebVideoFullscreenManager::setRate(uint64_t contextId, bool isPlaying, float playbackRate)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetRate(contextId, isPlaying, playbackRate), m_page->pageID());
}

void WebVideoFullscreenManager::setVideoDimensions(uint64_t contextId, bool hasVideo, float width, float height)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetVideoDimensions(contextId, hasVideo, width, height), m_page->pageID());
}
    
void WebVideoFullscreenManager::setSeekableRanges(uint64_t contextId, const WebCore::TimeRanges& timeRanges)
{
    Vector<std::pair<double, double>> rangesVector;
    
    for (unsigned i = 0; i < timeRanges.length(); i++) {
        ExceptionCode exceptionCode;
        double start = timeRanges.start(i, exceptionCode);
        double end = timeRanges.end(i, exceptionCode);
        rangesVector.append(std::pair<double, double>(start, end));
    }

    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetSeekableRangesVector(contextId, WTFMove(rangesVector)), m_page->pageID());
}

void WebVideoFullscreenManager::setCanPlayFastReverse(uint64_t contextId, bool value)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetCanPlayFastReverse(contextId, value), m_page->pageID());
}

void WebVideoFullscreenManager::setAudioMediaSelectionOptions(uint64_t contextId, const Vector<String>& options, uint64_t selectedIndex)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetAudioMediaSelectionOptions(contextId, options, selectedIndex), m_page->pageID());
}

void WebVideoFullscreenManager::setLegibleMediaSelectionOptions(uint64_t contextId, const Vector<String>& options, uint64_t selectedIndex)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetLegibleMediaSelectionOptions(contextId, options, selectedIndex), m_page->pageID());
}

void WebVideoFullscreenManager::setExternalPlayback(uint64_t contextId, bool enabled, WebVideoFullscreenInterface::ExternalPlaybackTargetType targetType, String localizedDeviceName)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetExternalPlaybackProperties(contextId, enabled, static_cast<uint32_t>(targetType), localizedDeviceName), m_page->pageID());
}

void WebVideoFullscreenManager::setWirelessVideoPlaybackDisabled(uint64_t contextId, bool disabled)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetWirelessVideoPlaybackDisabled(contextId, disabled));
}

#pragma mark Messages from WebVideoFullscreenManagerProxy:

void WebVideoFullscreenManager::play(uint64_t contextId)
{
    ensureModel(contextId).play();
}

void WebVideoFullscreenManager::pause(uint64_t contextId)
{
    ensureModel(contextId).pause();
}

void WebVideoFullscreenManager::togglePlayState(uint64_t contextId)
{
    ensureModel(contextId).togglePlayState();
}

void WebVideoFullscreenManager::beginScrubbing(uint64_t contextId)
{
    ensureModel(contextId).beginScrubbing();
}

void WebVideoFullscreenManager::endScrubbing(uint64_t contextId)
{
    ensureModel(contextId).endScrubbing();
}

void WebVideoFullscreenManager::seekToTime(uint64_t contextId, double time)
{
    ensureModel(contextId).seekToTime(time);
}

void WebVideoFullscreenManager::fastSeek(uint64_t contextId, double time)
{
    ensureModel(contextId).fastSeek(time);
}

void WebVideoFullscreenManager::beginScanningForward(uint64_t contextId)
{
    ensureModel(contextId).beginScanningForward();
}

void WebVideoFullscreenManager::beginScanningBackward(uint64_t contextId)
{
    ensureModel(contextId).beginScanningBackward();
}

void WebVideoFullscreenManager::endScanning(uint64_t contextId)
{
    ensureModel(contextId).endScanning();
}

void WebVideoFullscreenManager::requestFullscreenMode(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ensureModel(contextId).requestFullscreenMode(mode);
}

void WebVideoFullscreenManager::selectAudioMediaOption(uint64_t contextId, uint64_t index)
{
    ensureModel(contextId).selectAudioMediaOption(index);
}

void WebVideoFullscreenManager::selectLegibleMediaOption(uint64_t contextId, uint64_t index)
{
    ensureModel(contextId).selectLegibleMediaOption(index);
}

void WebVideoFullscreenManager::fullscreenModeChanged(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode)
{
    ensureModel(contextId).fullscreenModeChanged(videoFullscreenMode);
}

#pragma mark Messages from WebVideoFullscreenManager:

void WebVideoFullscreenManager::didSetupFullscreen(uint64_t contextId)
{
    PlatformLayer* videoLayer = [CALayer layer];
#ifndef NDEBUG
    [videoLayer setName:@"Web video fullscreen manager layer"];
#endif

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [videoLayer setPosition:CGPointMake(0, 0)];
    [videoLayer setBackgroundColor:cachedCGColor(WebCore::Color::transparent)];

    // Set a scale factor here to make convertRect:toLayer:nil take scale factor into account. <rdar://problem/18316542>.
    // This scale factor is inverted in the hosting process.
    float hostingScaleFactor = m_page->deviceScaleFactor();
    [videoLayer setTransform:CATransform3DMakeScale(hostingScaleFactor, hostingScaleFactor, 1)];

    RefPtr<WebVideoFullscreenModelVideoElement> model;
    RefPtr<WebVideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    interface->layerHostingContext()->setRootLayer(videoLayer);
    model->setVideoFullscreenLayer(videoLayer);

    [CATransaction commit];

    RefPtr<WebVideoFullscreenManager> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this, contextId] {
        m_page->send(Messages::WebVideoFullscreenManagerProxy::EnterFullscreen(contextId), m_page->pageID());
    });
}
    
void WebVideoFullscreenManager::didEnterFullscreen(uint64_t contextId)
{
    RefPtr<WebVideoFullscreenModelVideoElement> model;
    RefPtr<WebVideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    interface->setIsAnimating(false);
    interface->setIsFullscreen(false);

    if (interface->targetIsFullscreen())
        return;

    RefPtr<HTMLVideoElement> videoElement = model->videoElement();
    if (!videoElement)
        return;

    // exit fullscreen now if it was previously requested during an animation.
    RefPtr<WebVideoFullscreenManager> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, videoElement] {
        strongThis->exitVideoFullscreenForVideoElement(*videoElement);
    });
}

void WebVideoFullscreenManager::didExitFullscreen(uint64_t contextId)
{
    RefPtr<WebVideoFullscreenModelVideoElement> model;
    RefPtr<WebVideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    model->setVideoFullscreenLayer(nil);

    RefPtr<WebVideoFullscreenManager> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, contextId, interface] {
        if (interface->layerHostingContext()) {
            interface->layerHostingContext()->setRootLayer(nullptr);
            interface->setLayerHostingContext(nullptr);
        }
        if (strongThis->m_page)
            strongThis->m_page->send(Messages::WebVideoFullscreenManagerProxy::CleanupFullscreen(contextId), strongThis->m_page->pageID());
    });
}
    
void WebVideoFullscreenManager::didCleanupFullscreen(uint64_t contextId)
{
    RefPtr<WebVideoFullscreenModelVideoElement> model;
    RefPtr<WebVideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    interface->setIsAnimating(false);
    interface->setIsFullscreen(false);
    HTMLMediaElementEnums::VideoFullscreenMode mode = interface->fullscreenMode();
    bool targetIsFullscreen = interface->targetIsFullscreen();

    model->setVideoFullscreenLayer(nil);
    RefPtr<HTMLVideoElement> videoElement = model->videoElement();

    interface->setFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
    removeClientForContext(contextId);

    if (!videoElement || !targetIsFullscreen)
        return;

    RefPtr<WebVideoFullscreenManager> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, videoElement, mode] {
        strongThis->enterVideoFullscreenForVideoElement(*videoElement, mode);
    });
}
    
void WebVideoFullscreenManager::setVideoLayerGravityEnum(uint64_t contextId, unsigned gravity)
{
    ensureModel(contextId).setVideoLayerGravity((WebVideoFullscreenModel::VideoGravity)gravity);
}
    
void WebVideoFullscreenManager::fullscreenMayReturnToInline(uint64_t contextId, bool isPageVisible)
{
    auto& model = ensureModel(contextId);

    if (!isPageVisible)
        model.videoElement()->scrollIntoViewIfNotVisible(false);
    m_page->send(Messages::WebVideoFullscreenManagerProxy::PreparedToReturnToInline(contextId, true, clientRectForElement(model.videoElement())), m_page->pageID());
}
    
void WebVideoFullscreenManager::setVideoLayerFrameFenced(uint64_t contextId, WebCore::FloatRect bounds, IPC::Attachment fencePort)
{
    RefPtr<WebVideoFullscreenModelVideoElement> model;
    RefPtr<WebVideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    if (std::isnan(bounds.x()) || std::isnan(bounds.y()) || std::isnan(bounds.width()) || std::isnan(bounds.height())) {
        FloatRect clientRect = clientRectForElement(model->videoElement());
        bounds = FloatRect(0, 0, clientRect.width(), clientRect.height());
    }
    
    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    if (interface->layerHostingContext())
        interface->layerHostingContext()->setFencePort(fencePort.port());
    model->setVideoLayerFrame(bounds);
    mach_port_deallocate(mach_task_self(), fencePort.port());
    [CATransaction commit];
}

} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
