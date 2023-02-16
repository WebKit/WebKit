/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#import "VideoFullscreenManager.h"

#if ENABLE(VIDEO_PRESENTATION_MODE)

#import "Attachment.h"
#import "LayerHostingContext.h"
#import "Logging.h"
#import "PlaybackSessionManager.h"
#import "VideoFullscreenManagerMessages.h"
#import "VideoFullscreenManagerProxyMessages.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/Color.h>
#import <WebCore/Event.h>
#import <WebCore/EventNames.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/PictureInPictureSupport.h>
#import <WebCore/PlatformCALayer.h>
#import <WebCore/Quirks.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderLayerBacking.h>
#import <WebCore/RenderVideo.h>
#import <WebCore/RenderView.h>
#import <WebCore/Settings.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <mach/mach_port.h>
#import <wtf/MachSendRight.h>

namespace WebKit {
using namespace WebCore;

static FloatRect inlineVideoFrame(HTMLVideoElement& element)
{
    auto& document = element.document();
    if (!document.hasLivingRenderTree() || document.activeDOMObjectsAreStopped())
        return { };

    document.updateLayoutIgnorePendingStylesheets();
    auto* renderer = element.renderer();
    if (!renderer)
        return { };

    if (renderer->hasLayer() && renderer->enclosingLayer()->isComposited()) {
        FloatQuad contentsBox = static_cast<FloatRect>(renderer->enclosingLayer()->backing()->contentsBox());
        contentsBox = renderer->localToAbsoluteQuad(contentsBox);
        return element.document().view()->contentsToRootView(contentsBox.boundingBox());
    }

    auto rect = renderer->videoBox();
    rect.moveBy(renderer->absoluteBoundingBoxRect().location());
    return element.document().view()->contentsToRootView(rect);
}

#pragma mark - VideoFullscreenInterfaceContext

VideoFullscreenInterfaceContext::VideoFullscreenInterfaceContext(VideoFullscreenManager& manager, PlaybackSessionContextIdentifier contextId)
    : m_manager(&manager)
    , m_contextId(contextId)
{
}

VideoFullscreenInterfaceContext::~VideoFullscreenInterfaceContext()
{
}

void VideoFullscreenInterfaceContext::setLayerHostingContext(std::unique_ptr<LayerHostingContext>&& context)
{
    m_layerHostingContext = WTFMove(context);
}

void VideoFullscreenInterfaceContext::hasVideoChanged(bool hasVideo)
{
    if (m_manager)
        m_manager->hasVideoChanged(m_contextId, hasVideo);
}

void VideoFullscreenInterfaceContext::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    if (m_manager)
        m_manager->videoDimensionsChanged(m_contextId, videoDimensions);
}

void VideoFullscreenInterfaceContext::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{
    if (m_manager)
        m_manager->setPlayerIdentifier(m_contextId, identifier);
}

#pragma mark - VideoFullscreenManager

Ref<VideoFullscreenManager> VideoFullscreenManager::create(WebPage& page, PlaybackSessionManager& playbackSessionManager)
{
    return adoptRef(*new VideoFullscreenManager(page, playbackSessionManager));
}

VideoFullscreenManager::VideoFullscreenManager(WebPage& page, PlaybackSessionManager& playbackSessionManager)
    : m_page(&page)
    , m_playbackSessionManager(playbackSessionManager)
{
    WebProcess::singleton().addMessageReceiver(Messages::VideoFullscreenManager::messageReceiverName(), page.identifier(), *this);
}

VideoFullscreenManager::~VideoFullscreenManager()
{
    for (auto& [model, interface] : m_contextMap.values()) {
        model->setVideoElement(nullptr);
        model->removeClient(*interface);

        interface->invalidate();
    }

    m_contextMap.clear();
    m_videoElements.clear();
    m_clientCounts.clear();

    if (m_page)
        WebProcess::singleton().removeMessageReceiver(Messages::VideoFullscreenManager::messageReceiverName(), m_page->identifier());
}

void VideoFullscreenManager::invalidate()
{
    ASSERT(m_page);
    WebProcess::singleton().removeMessageReceiver(Messages::VideoFullscreenManager::messageReceiverName(), m_page->identifier());
    m_page = nullptr;
}

bool VideoFullscreenManager::hasVideoPlayingInPictureInPicture() const
{
    return !!m_videoElementInPictureInPicture;
}

VideoFullscreenManager::ModelInterfaceTuple VideoFullscreenManager::createModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto model = VideoFullscreenModelVideoElement::create();
    auto interface = VideoFullscreenInterfaceContext::create(*this, contextId);
    m_playbackSessionManager->addClientForContext(contextId);

    interface->setLayerHostingContext(LayerHostingContext::createForExternalHostingProcess());
    model->addClient(interface.get());

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

VideoFullscreenManager::ModelInterfaceTuple& VideoFullscreenManager::ensureModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_contextMap.add(contextId, ModelInterfaceTuple());
    if (addResult.isNewEntry)
        addResult.iterator->value = createModelAndInterface(contextId);
    return addResult.iterator->value;
}

WebCore::VideoFullscreenModelVideoElement& VideoFullscreenManager::ensureModel(PlaybackSessionContextIdentifier contextId)
{
    return *std::get<0>(ensureModelAndInterface(contextId));
}

VideoFullscreenInterfaceContext& VideoFullscreenManager::ensureInterface(PlaybackSessionContextIdentifier contextId)
{
    return *std::get<1>(ensureModelAndInterface(contextId));
}

void VideoFullscreenManager::removeContext(PlaybackSessionContextIdentifier contextId)
{
    auto [model, interface] = ensureModelAndInterface(contextId);

    m_playbackSessionManager->removeClientForContext(contextId);

    RefPtr<HTMLVideoElement> videoElement = model->videoElement();
    model->setVideoElement(nullptr);
    model->removeClient(*interface);
    interface->invalidate();
    m_videoElements.remove(videoElement.get());
    m_contextMap.remove(contextId);
}

void VideoFullscreenManager::addClientForContext(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_clientCounts.add(contextId, 1);
    if (!addResult.isNewEntry)
        addResult.iterator->value++;
}

void VideoFullscreenManager::removeClientForContext(PlaybackSessionContextIdentifier contextId)
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

bool VideoFullscreenManager::canEnterVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
#if PLATFORM(IOS)
    if (m_currentlyInFullscreen && mode == HTMLMediaElementEnums::VideoFullscreenModeStandard)
        return false;
#endif
    return true;
}

bool VideoFullscreenManager::supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(mode);
#if HAVE(AVKIT)
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    return true;
#else
    return true;
#endif
#else
    return false;
#endif
#else
    return mode == HTMLMediaElementEnums::VideoFullscreenModeStandard || (mode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture && supportsPictureInPicture());
#endif
}

bool VideoFullscreenManager::supportsVideoFullscreenStandby() const
{
#if PLATFORM(IOS_FAMILY)
    return true;
#else
    return false;
#endif
}

void VideoFullscreenManager::enterVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode, bool standby)
{
    ASSERT(m_page);
    ASSERT(standby || mode != HTMLMediaElementEnums::VideoFullscreenModeNone);

#if PLATFORM(IOS)
    auto allowLayeredFullscreenVideos = videoElement.document().quirks().allowLayeredFullscreenVideos();
    if (m_currentlyInFullscreen
        && mode == HTMLMediaElementEnums::VideoFullscreenModeStandard
        && !allowLayeredFullscreenVideos) {
        LOG(Fullscreen, "VideoFullscreenManager::enterVideoFullscreenForVideoElement(%p) already in fullscreen, aborting", this);
        ASSERT_NOT_REACHED();
        return;
    }
#endif

    LOG(Fullscreen, "VideoFullscreenManager::enterVideoFullscreenForVideoElement(%p)", this);

    auto contextId = m_playbackSessionManager->contextIdForMediaElement(videoElement);
    auto addResult = m_videoElements.add(&videoElement, contextId);
    UNUSED_PARAM(addResult);
    ASSERT(addResult.iterator->value == contextId);

    auto [model, interface] = ensureModelAndInterface(contextId);
    HTMLMediaElementEnums::VideoFullscreenMode oldMode = interface->fullscreenMode();

    if (oldMode == HTMLMediaElementEnums::VideoFullscreenModeNone)
        addClientForContext(contextId);

    if (!interface->layerHostingContext())
        interface->setLayerHostingContext(LayerHostingContext::createForExternalHostingProcess());

    auto videoRect = inlineVideoFrame(videoElement);
    FloatRect videoLayerFrame = FloatRect(0, 0, videoRect.width(), videoRect.height());

#if PLATFORM(IOS)
    if (allowLayeredFullscreenVideos)
        interface->setTargetIsFullscreen(mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
    else
#endif
        setCurrentlyInFullscreen(*interface, mode != HTMLMediaElementEnums::VideoFullscreenModeNone);

    if (mode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        m_videoElementInPictureInPicture = videoElement;

    interface->setFullscreenMode(mode);
    interface->setFullscreenStandby(standby);
    model->setVideoElement(&videoElement);
    if (oldMode == HTMLMediaElementEnums::VideoFullscreenModeNone && mode != HTMLMediaElementEnums::VideoFullscreenModeNone)
        model->setVideoLayerFrame(videoLayerFrame);

    if (interface->animationState() != VideoFullscreenInterfaceContext::AnimationType::None)
        return;
    interface->setAnimationState(VideoFullscreenInterfaceContext::AnimationType::IntoFullscreen);

    bool allowsPictureInPicture = videoElement.webkitSupportsPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);

    if (!interface->layerHostingContext()->rootLayer()) {
        auto videoLayer = model->createVideoFullscreenLayer();
        [videoLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        [videoLayer setName:@"Web Video Fullscreen Layer"];
        [videoLayer setPosition:CGPointMake(0, 0)];
        [videoLayer setBackgroundColor:cachedCGColor(WebCore::Color::transparentBlack).get()];

        // Set a scale factor here to make convertRect:toLayer:nil take scale factor into account. <rdar://problem/18316542>.
        // This scale factor is inverted in the hosting process.
        float hostingScaleFactor = m_page->deviceScaleFactor();
        [videoLayer setTransform:CATransform3DMakeScale(hostingScaleFactor, hostingScaleFactor, 1)];

        interface->layerHostingContext()->setRootLayer(videoLayer.get());
    }

    m_page->send(Messages::VideoFullscreenManagerProxy::SetupFullscreenWithID(contextId, interface->layerHostingContext()->contextID(), videoRect, FloatSize(videoElement.videoWidth(), videoElement.videoHeight()), m_page->deviceScaleFactor(), interface->fullscreenMode(), allowsPictureInPicture, standby, videoElement.document().quirks().blocksReturnToFullscreenFromPictureInPictureQuirk()));

    if (auto player = videoElement.player()) {
        if (auto identifier = player->identifier())
            setPlayerIdentifier(contextId, identifier);
    }
}

void VideoFullscreenManager::exitVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, CompletionHandler<void(bool)>&& completionHandler)
{
    LOG(Fullscreen, "VideoFullscreenManager::exitVideoFullscreenForVideoElement(%p)", this);
    ASSERT(m_page);
    ASSERT(m_videoElements.contains(&videoElement));

    auto contextId = m_videoElements.get(&videoElement);
    auto& interface = ensureInterface(contextId);
    if (interface.animationState() != VideoFullscreenInterfaceContext::AnimationType::None) {
        completionHandler(false);
        return;
    }

    m_page->sendWithAsyncReply(Messages::VideoFullscreenManagerProxy::ExitFullscreen(contextId, inlineVideoFrame(videoElement)), [protectedThis = Ref { *this }, this, contextId, videoElementPtr = &videoElement, completionHandler = WTFMove(completionHandler)](auto success) mutable {
        if (!success) {
            completionHandler(false);
            return;
        }

        if (m_videoElementInPictureInPicture == videoElementPtr)
            m_videoElementInPictureInPicture = nullptr;

        auto& interface = ensureInterface(contextId);
        protectedThis->setCurrentlyInFullscreen(interface, false);
        interface.setAnimationState(VideoFullscreenInterfaceContext::AnimationType::FromFullscreen);
        completionHandler(true);
    });
}

void VideoFullscreenManager::exitVideoFullscreenToModeWithoutAnimation(HTMLVideoElement& videoElement, WebCore::HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
    LOG(Fullscreen, "VideoFullscreenManager::exitVideoFullscreenToModeWithoutAnimation(%p)", this);

    ASSERT(m_page);
    ASSERT(m_videoElements.contains(&videoElement));

    if (m_videoElementInPictureInPicture == &videoElement)
        m_videoElementInPictureInPicture = nullptr;

    auto contextId = m_videoElements.get(&videoElement);
    if (!contextId.isValid()) {
        // We have somehow managed to be asked to exit video fullscreen
        // for a video element which was either never in fullscreen or
        // has already been removed. Bail.
        ASSERT_NOT_REACHED();
        return;
    }
    auto& interface = ensureInterface(contextId);

    setCurrentlyInFullscreen(interface, false);

    // didCleanupFullscreen() will call removeClientForContext() on Mac
#if PLATFORM(IOS_FAMILY)
    removeClientForContext(contextId);
#endif

    m_page->send(Messages::VideoFullscreenManagerProxy::ExitFullscreenWithoutAnimationToMode(contextId, targetMode));
}

#pragma mark Interface to VideoFullscreenInterfaceContext:

void VideoFullscreenManager::hasVideoChanged(PlaybackSessionContextIdentifier contextId, bool hasVideo)
{
    if (m_page)
        m_page->send(Messages::VideoFullscreenManagerProxy::SetHasVideo(contextId, hasVideo));
}

void VideoFullscreenManager::videoDimensionsChanged(PlaybackSessionContextIdentifier contextId, const FloatSize& videoDimensions)
{
    if (m_page)
        m_page->send(Messages::VideoFullscreenManagerProxy::SetVideoDimensions(contextId, videoDimensions));
}

void VideoFullscreenManager::setPlayerIdentifier(PlaybackSessionContextIdentifier contextIdentifier, std::optional<MediaPlayerIdentifier> playerIdentifier)
{
    if (m_page)
        m_page->send(Messages::VideoFullscreenManagerProxy::SetPlayerIdentifier(contextIdentifier, playerIdentifier));
}

#pragma mark Messages from VideoFullscreenManagerProxy:

void VideoFullscreenManager::requestFullscreenMode(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    ensureModel(contextId).requestFullscreenMode(mode, finishedWithMedia);
}

void VideoFullscreenManager::fullscreenModeChanged(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    model->fullscreenModeChanged(videoFullscreenMode);
    interface->setFullscreenMode(videoFullscreenMode);
}

void VideoFullscreenManager::requestUpdateInlineRect(PlaybackSessionContextIdentifier contextId)
{
    if (!m_page)
        return;

    auto& model = ensureModel(contextId);
    auto inlineRect = inlineVideoFrame(*model.videoElement());
    m_page->send(Messages::VideoFullscreenManagerProxy::SetInlineRect(contextId, inlineRect, inlineRect != IntRect(0, 0, 0, 0)));
}

void VideoFullscreenManager::requestVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    auto [model, interface] = ensureModelAndInterface(contextId);

    CALayer* videoLayer = interface->layerHostingContext()->rootLayer();

    model->setVideoFullscreenLayer(videoLayer, [protectedThis = Ref { *this }, this, contextId] () mutable {
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), this, contextId] {
            if (protectedThis->m_page)
                m_page->send(Messages::VideoFullscreenManagerProxy::SetHasVideoContentLayer(contextId, true));
        });
    });
}

void VideoFullscreenManager::returnVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    RefPtr<VideoFullscreenModelVideoElement> model;
    RefPtr<VideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    model->waitForPreparedForInlineThen([protectedThis = Ref { *this }, this, contextId, model] () mutable { // need this for return video layer
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), this, contextId, model] () mutable {
            model->setVideoFullscreenLayer(nil, [protectedThis = WTFMove(protectedThis), this, contextId] () mutable {
                RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), this, contextId] {
                    if (protectedThis->m_page)
                        m_page->send(Messages::VideoFullscreenManagerProxy::SetHasVideoContentLayer(contextId, false));
                });
            });
        });
    });
}

#if !PLATFORM(IOS_FAMILY)
void VideoFullscreenManager::didSetupFullscreen(PlaybackSessionContextIdentifier contextId)
{
    LOG(Fullscreen, "VideoFullscreenManager::didSetupFullscreen(%p, %x)", this, contextId);

    ASSERT(m_page);
    auto [model, interface] = ensureModelAndInterface(contextId);
    CALayer* videoLayer = interface->layerHostingContext()->rootLayer();

    model->setVideoFullscreenLayer(videoLayer, [protectedThis = Ref { *this }, this, contextId] () mutable {
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), this, contextId] {
            if (protectedThis->m_page)
                m_page->send(Messages::VideoFullscreenManagerProxy::EnterFullscreen(contextId));
        });
    });
}
#endif

void VideoFullscreenManager::willExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    LOG(Fullscreen, "VideoFullscreenManager::willExitFullscreen(%p, %x)", this, contextId);

    auto [model, interface] = ensureModelAndInterface(contextId);

    RefPtr<HTMLVideoElement> videoElement = model->videoElement();
    if (!videoElement)
        return;

    RunLoop::main().dispatch([protectedThis = Ref { *this }, videoElement = WTFMove(videoElement), contextId] {
        videoElement->willExitFullscreen();
        if (protectedThis->m_page)
            protectedThis->m_page->send(Messages::VideoFullscreenManagerProxy::PreparedToExitFullscreen(contextId));
    });
}

void VideoFullscreenManager::didEnterFullscreen(PlaybackSessionContextIdentifier contextId, std::optional<WebCore::FloatSize> size)
{
    LOG(Fullscreen, "VideoFullscreenManager::didEnterFullscreen(%p, %x)", this, contextId);

    auto [model, interface] = ensureModelAndInterface(contextId);

    interface->setAnimationState(VideoFullscreenInterfaceContext::AnimationType::None);
    interface->setIsFullscreen(false);

    RefPtr<HTMLVideoElement> videoElement = model->videoElement();
    if (!videoElement)
        return;

    videoElement->didEnterFullscreenOrPictureInPicture(valueOrDefault(size));

    if (interface->targetIsFullscreen() || interface->fullscreenStandby())
        return;

    // exit fullscreen now if it was previously requested during an animation.
    RunLoop::main().dispatch([protectedThis = Ref { *this }, videoElement] {
        if (protectedThis->m_page)
            protectedThis->exitVideoFullscreenForVideoElement(*videoElement, [](bool) { });
    });
}

void VideoFullscreenManager::failedToEnterFullscreen(PlaybackSessionContextIdentifier contextId)
{
    LOG(Fullscreen, "VideoFullscreenManager::failedToEnterFullscreen(%p, %x)", this, contextId);

#if PLATFORM(IOS_FAMILY)
    RefPtr<VideoFullscreenModelVideoElement> model;
    RefPtr<VideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

    RunLoop::main().dispatch([protectedThis = Ref { *this }, contextId, interface] {
        if (protectedThis->m_page)
            protectedThis->m_page->send(Messages::VideoFullscreenManagerProxy::CleanupFullscreen(contextId));
    });
#endif
}

void VideoFullscreenManager::didExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    LOG(Fullscreen, "VideoFullscreenManager::didExitFullscreen(%p, %x)", this, contextId);

    RefPtr<VideoFullscreenModelVideoElement> model;
    RefPtr<VideoFullscreenInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

#if PLATFORM(IOS_FAMILY)
    RunLoop::main().dispatch([protectedThis = Ref { *this }, contextId, interface] {
        if (protectedThis->m_page)
            protectedThis->m_page->send(Messages::VideoFullscreenManagerProxy::CleanupFullscreen(contextId));
    });
#else
    model->waitForPreparedForInlineThen([protectedThis = Ref { *this }, contextId, interface, model]() mutable {
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), contextId, interface, model] () mutable {
            model->setVideoFullscreenLayer(nil, [protectedThis = WTFMove(protectedThis), contextId, interface] () mutable {
                RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), contextId, interface] {
                    if (interface->layerHostingContext()) {
                        interface->layerHostingContext()->setRootLayer(nullptr);
                        interface->setLayerHostingContext(nullptr);
                    }
                    if (protectedThis->m_page)
                        protectedThis->m_page->send(Messages::VideoFullscreenManagerProxy::CleanupFullscreen(contextId));
                });
            });
        });
    });
#endif
}

void VideoFullscreenManager::didCleanupFullscreen(PlaybackSessionContextIdentifier contextId)
{
    LOG(Fullscreen, "VideoFullscreenManager::didCleanupFullscreen(%p, %x)", this, contextId);

    auto [model, interface] = ensureModelAndInterface(contextId);

    if (interface->layerHostingContext()) {
        interface->layerHostingContext()->setRootLayer(nullptr);
        interface->setLayerHostingContext(nullptr);
    }

    interface->setAnimationState(VideoFullscreenInterfaceContext::AnimationType::None);
    interface->setIsFullscreen(false);
    HTMLMediaElementEnums::VideoFullscreenMode mode = interface->fullscreenMode();
    bool standby = interface->fullscreenStandby();
    bool targetIsFullscreen = interface->targetIsFullscreen();

    model->setVideoFullscreenLayer(nil);
    RefPtr<HTMLVideoElement> videoElement = model->videoElement();
    if (videoElement)
        videoElement->didExitFullscreenOrPictureInPicture();

    interface->setFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
    interface->setFullscreenStandby(false);
    removeClientForContext(contextId);

    if (!videoElement || !targetIsFullscreen || mode == HTMLMediaElementEnums::VideoFullscreenModeNone) {
        setCurrentlyInFullscreen(*interface, false);
        return;
    }

    RunLoop::main().dispatch([protectedThis = Ref { *this }, videoElement, mode, standby] {
        if (protectedThis->m_page)
            protectedThis->enterVideoFullscreenForVideoElement(*videoElement, mode, standby);
    });
}

void VideoFullscreenManager::setVideoLayerGravityEnum(PlaybackSessionContextIdentifier contextId, unsigned gravity)
{
    ensureModel(contextId).setVideoLayerGravity((MediaPlayerEnums::VideoGravity)gravity);
}

void VideoFullscreenManager::fullscreenMayReturnToInline(PlaybackSessionContextIdentifier contextId, bool isPageVisible)
{
    if (!m_page)
        return;

    auto& model = ensureModel(contextId);

    if (!isPageVisible)
        model.videoElement()->scrollIntoViewIfNotVisible(false);
    m_page->send(Messages::VideoFullscreenManagerProxy::PreparedToReturnToInline(contextId, true, inlineVideoFrame(*model.videoElement())));
}

void VideoFullscreenManager::requestRouteSharingPolicyAndContextUID(PlaybackSessionContextIdentifier contextId, CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&& reply)
{
    ensureModel(contextId).requestRouteSharingPolicyAndContextUID(WTFMove(reply));
}

void VideoFullscreenManager::setCurrentlyInFullscreen(VideoFullscreenInterfaceContext& interface, bool currentlyInFullscreen)
{
    interface.setTargetIsFullscreen(currentlyInFullscreen);
    m_currentlyInFullscreen = currentlyInFullscreen;
}

void VideoFullscreenManager::setVideoLayerFrameFenced(PlaybackSessionContextIdentifier contextId, WebCore::FloatRect bounds, const WTF::MachSendRight& machSendRight)
{
    LOG(Fullscreen, "VideoFullscreenManager::setVideoLayerFrameFenced(%p, %x)", this, contextId);

    auto [model, interface] = ensureModelAndInterface(contextId);

    if (std::isnan(bounds.x()) || std::isnan(bounds.y()) || std::isnan(bounds.width()) || std::isnan(bounds.height())) {
        auto videoRect = inlineVideoFrame(*model->videoElement());
        bounds = FloatRect(0, 0, videoRect.width(), videoRect.height());
    }

    if (auto* context = interface->layerHostingContext())
        context->setFencePort(machSendRight.sendRight());
    model->setVideoLayerFrame(bounds);
}

} // namespace WebKit

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
