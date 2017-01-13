/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#if (PLATFORM(IOS) && HAVE(AVKIT)) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "Attachment.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebPlaybackSessionManager.h"
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
#import <WebCore/RenderVideo.h>
#import <WebCore/RenderView.h>
#import <WebCore/Settings.h>
#import <WebCore/TimeRanges.h>
#import <mach/mach_port.h>

using namespace WebCore;

namespace WebKit {

static IntRect inlineVideoFrame(HTMLVideoElement& element)
{
    element.document().updateLayoutIgnorePendingStylesheets();
    auto* renderer = element.renderer();
    if (!renderer)
        return { };
    auto rect = renderer->videoBox();
    rect.moveBy(renderer->absoluteBoundingBoxRect().location());
    return element.document().view()->contentsToRootView(rect);
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

void WebVideoFullscreenInterfaceContext::hasVideoChanged(bool hasVideo)
{
    if (m_manager)
        m_manager->hasVideoChanged(m_contextId, hasVideo);
}

void WebVideoFullscreenInterfaceContext::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    if (m_manager)
        m_manager->videoDimensionsChanged(m_contextId, videoDimensions);
}

#pragma mark - WebVideoFullscreenManager

Ref<WebVideoFullscreenManager> WebVideoFullscreenManager::create(WebPage& page, WebPlaybackSessionManager& playbackSessionManager)
{
    return adoptRef(*new WebVideoFullscreenManager(page, playbackSessionManager));
}

WebVideoFullscreenManager::WebVideoFullscreenManager(WebPage& page, WebPlaybackSessionManager& playbackSessionManager)
    : m_page(&page)
    , m_playbackSessionManager(playbackSessionManager)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebVideoFullscreenManager::messageReceiverName(), page.pageID(), *this);
}

WebVideoFullscreenManager::~WebVideoFullscreenManager()
{
    for (auto& tuple : m_contextMap.values()) {
        RefPtr<WebVideoFullscreenModelVideoElement> model;
        RefPtr<WebVideoFullscreenInterfaceContext> interface;
        std::tie(model, interface) = tuple;

        model->setVideoElement(nullptr);
        model->removeClient(*interface);

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
    m_playbackSessionManager->addClientForContext(contextId);

    interface->setLayerHostingContext(LayerHostingContext::createForExternalHostingProcess());
    model->addClient(*interface);

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

    m_playbackSessionManager->removeClientForContext(contextId);

    RefPtr<HTMLVideoElement> videoElement = model->videoElement();
    model->setVideoElement(nullptr);
    model->removeClient(*interface);
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
#else
    return mode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture && supportsPictureInPicture();
#endif
}

void WebVideoFullscreenManager::enterVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT(mode != HTMLMediaElementEnums::VideoFullscreenModeNone);

    uint64_t contextId = m_playbackSessionManager->contextIdForMediaElement(videoElement);
    auto addResult = m_videoElements.add(&videoElement, contextId);
    UNUSED_PARAM(addResult);
    ASSERT(addResult.iterator->value == contextId);

    RefPtr<WebVideoFullscreenModelVideoElement> model;
    RefPtr<WebVideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);
    addClientForContext(contextId);
    if (!interface->layerHostingContext())
        interface->setLayerHostingContext(LayerHostingContext::createForExternalHostingProcess());

    auto videoRect = inlineVideoFrame(videoElement);
    FloatRect videoLayerFrame = FloatRect(0, 0, videoRect.width(), videoRect.height());

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
    
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetupFullscreenWithID(contextId, interface->layerHostingContext()->contextID(), videoRect, m_page->deviceScaleFactor(), interface->fullscreenMode(), allowsPictureInPicture), m_page->pageID());
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
    m_page->send(Messages::WebVideoFullscreenManagerProxy::ExitFullscreen(contextId, inlineVideoFrame(videoElement)), m_page->pageID());
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

void WebVideoFullscreenManager::hasVideoChanged(uint64_t contextId, bool hasVideo)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetHasVideo(contextId, hasVideo), m_page->pageID());
}

void WebVideoFullscreenManager::videoDimensionsChanged(uint64_t contextId, const FloatSize& videoDimensions)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetVideoDimensions(contextId, videoDimensions), m_page->pageID());
}

#pragma mark Messages from WebVideoFullscreenManagerProxy:

void WebVideoFullscreenManager::requestFullscreenMode(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    ensureModel(contextId).requestFullscreenMode(mode, finishedWithMedia);
}

void WebVideoFullscreenManager::fullscreenModeChanged(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode)
{
    ensureModel(contextId).fullscreenModeChanged(videoFullscreenMode);
}

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

    RefPtr<WebVideoFullscreenManager> strongThis(this);
    
    model->setVideoFullscreenLayer(videoLayer, [strongThis, this, contextId] {
        dispatch_async(dispatch_get_main_queue(), [strongThis, this, contextId] {
            if (!strongThis->m_page)
                return;

            m_page->send(Messages::WebVideoFullscreenManagerProxy::EnterFullscreen(contextId), strongThis->m_page->pageID());
        });
    });
    
    [CATransaction commit];
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
    RefPtr<WebVideoFullscreenManager> strongThis(this);
    
    model->waitForPreparedForInlineThen([strongThis, this, contextId, interface, model] {
        dispatch_async(dispatch_get_main_queue(), [strongThis, this, contextId, interface, model] {
            model->setVideoFullscreenLayer(nil, [strongThis, this, contextId, interface] {
                dispatch_async(dispatch_get_main_queue(), [strongThis, this, contextId, interface] {
                    if (interface->layerHostingContext()) {
                        interface->layerHostingContext()->setRootLayer(nullptr);
                        interface->setLayerHostingContext(nullptr);
                    }
                    if (strongThis->m_page)
                        strongThis->m_page->send(Messages::WebVideoFullscreenManagerProxy::CleanupFullscreen(contextId), strongThis->m_page->pageID());
                });
            });
        });
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
    m_page->send(Messages::WebVideoFullscreenManagerProxy::PreparedToReturnToInline(contextId, true, inlineVideoFrame(*model.videoElement())), m_page->pageID());
}
    
void WebVideoFullscreenManager::setVideoLayerFrameFenced(uint64_t contextId, WebCore::FloatRect bounds, IPC::Attachment fencePort)
{
    RefPtr<WebVideoFullscreenModelVideoElement> model;
    RefPtr<WebVideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    if (std::isnan(bounds.x()) || std::isnan(bounds.y()) || std::isnan(bounds.width()) || std::isnan(bounds.height())) {
        auto videoRect = inlineVideoFrame(*model->videoElement());
        bounds = FloatRect(0, 0, videoRect.width(), videoRect.height());
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
