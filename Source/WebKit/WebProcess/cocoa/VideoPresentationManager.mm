/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#import "VideoPresentationManager.h"

#if ENABLE(VIDEO_PRESENTATION_MODE)

#import "Attachment.h"
#import "LayerHostingContext.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "PlaybackSessionManager.h"
#import "TextTrackRepresentationCocoa.h"
#import "VideoPresentationManagerMessages.h"
#import "VideoPresentationManagerProxyMessages.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/Chrome.h>
#import <WebCore/Color.h>
#import <WebCore/Event.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/LocalFrameView.h>
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
#import <wtf/LoggerHelper.h>
#import <wtf/MachSendRight.h>

namespace WebKit {
using namespace WebCore;

static FloatRect inlineVideoFrame(HTMLVideoElement& element)
{
    Ref document = element.document();
    if (!document->hasLivingRenderTree() || document->activeDOMObjectsAreStopped())
        return { };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);
    auto* renderer = element.renderer();
    if (!renderer)
        return { };

    if (renderer->hasLayer() && renderer->enclosingLayer()->isComposited()) {
        FloatQuad contentsBox = static_cast<FloatRect>(renderer->enclosingLayer()->backing()->contentsBox());
        contentsBox = renderer->localToAbsoluteQuad(contentsBox);
        return document->view()->contentsToRootView(contentsBox.boundingBox());
    }

    auto rect = renderer->videoBox();
    rect.moveBy(renderer->absoluteBoundingBoxRect().location());
    return document->view()->contentsToRootView(rect);
}

#pragma mark - VideoPresentationInterfaceContext

VideoPresentationInterfaceContext::VideoPresentationInterfaceContext(VideoPresentationManager& manager, PlaybackSessionContextIdentifier contextId)
    : m_manager(manager)
    , m_contextId(contextId)
{
}

VideoPresentationInterfaceContext::~VideoPresentationInterfaceContext()
{
}

void VideoPresentationInterfaceContext::setLayerHostingContext(std::unique_ptr<LayerHostingContext>&& context)
{
    m_layerHostingContext = WTFMove(context);
}

void VideoPresentationInterfaceContext::setRootLayer(RetainPtr<CALayer> layer)
{
    m_rootLayer = layer;
    if (m_layerHostingContext)
        m_layerHostingContext->setRootLayer(layer.get());
}

void VideoPresentationInterfaceContext::hasVideoChanged(bool hasVideo)
{
    if (m_manager)
        m_manager->hasVideoChanged(m_contextId, hasVideo);
}

void VideoPresentationInterfaceContext::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    if (m_manager)
        m_manager->videoDimensionsChanged(m_contextId, videoDimensions);
}

void VideoPresentationInterfaceContext::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{
    if (m_manager)
        m_manager->setPlayerIdentifier(m_contextId, identifier);
}

#pragma mark - VideoPresentationManager

Ref<VideoPresentationManager> VideoPresentationManager::create(WebPage& page, PlaybackSessionManager& playbackSessionManager)
{
    return adoptRef(*new VideoPresentationManager(page, playbackSessionManager));
}

VideoPresentationManager::VideoPresentationManager(WebPage& page, PlaybackSessionManager& playbackSessionManager)
    : m_page(&page)
    , m_playbackSessionManager(playbackSessionManager)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    WebProcess::singleton().addMessageReceiver(Messages::VideoPresentationManager::messageReceiverName(), page.identifier(), *this);
}

VideoPresentationManager::~VideoPresentationManager()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    for (auto& [model, interface] : m_contextMap.values()) {
        model->setVideoElement(nullptr);
        model->removeClient(*interface);
    }

    m_contextMap.clear();
    m_videoElements.clear();
    m_clientCounts.clear();

    if (m_page)
        WebProcess::singleton().removeMessageReceiver(Messages::VideoPresentationManager::messageReceiverName(), m_page->identifier());
}

void VideoPresentationManager::invalidate()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    ASSERT(m_page);
    WebProcess::singleton().removeMessageReceiver(Messages::VideoPresentationManager::messageReceiverName(), m_page->identifier());
    m_page = nullptr;
}

bool VideoPresentationManager::hasVideoPlayingInPictureInPicture() const
{
    return !!m_videoElementInPictureInPicture;
}

VideoPresentationManager::ModelInterfaceTuple VideoPresentationManager::createModelAndInterface(PlaybackSessionContextIdentifier contextId, bool createlayerHostingContext)
{
    auto model = VideoPresentationModelVideoElement::create();
    auto interface = VideoPresentationInterfaceContext::create(*this, contextId);
    m_playbackSessionManager->addClientForContext(contextId);

    if (createlayerHostingContext)
        interface->setLayerHostingContext(LayerHostingContext::createForExternalHostingProcess());

    model->addClient(interface.get());

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

VideoPresentationManager::ModelInterfaceTuple& VideoPresentationManager::ensureModelAndInterface(PlaybackSessionContextIdentifier contextId, bool createlayerHostingContext)
{
    auto addResult = m_contextMap.add(contextId, ModelInterfaceTuple());
    if (addResult.isNewEntry)
        addResult.iterator->value = createModelAndInterface(contextId, createlayerHostingContext);
    return addResult.iterator->value;
}

WebCore::VideoPresentationModelVideoElement& VideoPresentationManager::ensureModel(PlaybackSessionContextIdentifier contextId)
{
    return *std::get<0>(ensureModelAndInterface(contextId));
}

VideoPresentationInterfaceContext& VideoPresentationManager::ensureInterface(PlaybackSessionContextIdentifier contextId)
{
    return *std::get<1>(ensureModelAndInterface(contextId));
}

void VideoPresentationManager::removeContext(PlaybackSessionContextIdentifier contextId)
{
    m_playbackSessionManager->removeClientForContext(contextId);

    auto [model, interface] = m_contextMap.get(contextId);
    ASSERT(model);
    ASSERT(interface);
    if (!model || !interface)
        return;

    model->removeClient(*interface);
    m_contextMap.remove(contextId);

    RefPtr videoElement = model->videoElement();
    ASSERT(videoElement);
    if (!videoElement)
        return;

    model->setVideoElement(nullptr);
    m_videoElements.remove(*videoElement);
}

void VideoPresentationManager::addClientForContext(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_clientCounts.add(contextId, 1);
    if (!addResult.isNewEntry)
        addResult.iterator->value++;
}

void VideoPresentationManager::removeClientForContext(PlaybackSessionContextIdentifier contextId)
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

bool VideoPresentationManager::canEnterVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    if (m_currentlyInFullscreen && mode == HTMLMediaElementEnums::VideoFullscreenModeStandard)
        return false;
#endif
    return true;
}

bool VideoPresentationManager::supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(mode);
#if HAVE(AVKIT)
#if PLATFORM(VISION)
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

bool VideoPresentationManager::supportsVideoFullscreenStandby() const
{
#if PLATFORM(IOS_FAMILY)
    return true;
#else
    return false;
#endif
}

void VideoPresentationManager::setupRemoteLayerHosting(HTMLVideoElement& videoElement)
{
    auto contextId = m_playbackSessionManager->contextIdForMediaElement(videoElement);
    auto addResult = m_videoElements.add(videoElement, contextId);
    if (addResult.isNewEntry)
        m_playbackSessionManager->sendLogIdentifierForMediaElement(videoElement);
    ASSERT(addResult.iterator->value == contextId);

    bool blockMediaLayerRehosting = videoElement.document().settings().blockMediaLayerRehostingInWebContentProcess() && videoElement.document().page() &&  videoElement.document().page()->chrome().client().isUsingUISideCompositing();
    INFO_LOG(LOGIDENTIFIER, "Block Media layer rehosting = ", blockMediaLayerRehosting);
    if (blockMediaLayerRehosting) {
        auto representationFactory = [] (TextTrackRepresentationClient& client, HTMLMediaElement& mediaElement) {
            auto textTrackRepresentation = makeUnique<WebKit::WebTextTrackRepresentationCocoa>(client, mediaElement);
            return textTrackRepresentation;
        };
        WebCore::TextTrackRepresentationCocoa::representationFactory() = WTFMove(representationFactory);
    }

    auto [model, interface] = ensureModelAndInterface(contextId, !blockMediaLayerRehosting);
    model->setVideoElement(&videoElement);

    addClientForContext(contextId);
}

void VideoPresentationManager::willRemoveLayerForID(PlaybackSessionContextIdentifier contextId)
{
    removeClientForContext(contextId);
}

void VideoPresentationManager::enterVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode, bool standby)
{
    ASSERT(m_page);
    ASSERT(standby || mode != HTMLMediaElementEnums::VideoFullscreenModeNone);

#if PLATFORM(IOS) || PLATFORM(VISION)
    auto allowLayeredFullscreenVideos = videoElement.document().quirks().allowLayeredFullscreenVideos();
    if (m_currentlyInFullscreen
        && mode == HTMLMediaElementEnums::VideoFullscreenModeStandard
        && !allowLayeredFullscreenVideos) {
        ERROR_LOG(LOGIDENTIFIER, "already in fullscreen, aborting");
        ASSERT_NOT_REACHED();
        return;
    }
#endif

    INFO_LOG(LOGIDENTIFIER);

    auto contextId = m_playbackSessionManager->contextIdForMediaElement(videoElement);
    auto addResult = m_videoElements.add(videoElement, contextId);
    if (addResult.isNewEntry)
        m_playbackSessionManager->sendLogIdentifierForMediaElement(videoElement);
    ASSERT(addResult.iterator->value == contextId);

    auto [model, interface] = ensureModelAndInterface(contextId);
    HTMLMediaElementEnums::VideoFullscreenMode oldMode = interface->fullscreenMode();

    if (oldMode == HTMLMediaElementEnums::VideoFullscreenModeNone)
        addClientForContext(contextId);

    auto videoRect = inlineVideoFrame(videoElement);
    FloatRect videoLayerFrame = FloatRect(0, 0, videoRect.width(), videoRect.height());

    FloatSize initialSize = videoElement.videoLayerSize();

#if PLATFORM(IOS) || PLATFORM(VISION)
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

    if (interface->animationState() != VideoPresentationInterfaceContext::AnimationType::None)
        return;
    interface->setAnimationState(VideoPresentationInterfaceContext::AnimationType::IntoFullscreen);

    bool allowsPictureInPicture = videoElement.webkitSupportsPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);

    if (!interface->rootLayer()) {
        auto videoLayer = model->createVideoFullscreenLayer();
        [videoLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        [videoLayer setName:@"Web Video Fullscreen Layer"];
        [videoLayer setAnchorPoint:CGPointMake(0, 0)];
        [videoLayer setPosition:CGPointMake(0, 0)];
        [videoLayer setBackgroundColor:cachedCGColor(WebCore::Color::transparentBlack).get()];
        interface->setRootLayer(videoLayer.get());
    }

    auto setupFullscreen = [protectedThis = Ref { *this }, page = WeakPtr { m_page }, contextId = contextId, initialSize = initialSize, videoRect = videoRect, videoElement = WeakPtr { videoElement }, allowsPictureInPicture = allowsPictureInPicture, standby = standby, fullscreenMode = interface->fullscreenMode()] (LayerHostingContextID contextID, const FloatSize& size) {
        if (!page || !videoElement)
            return;
        page->send(Messages::VideoPresentationManagerProxy::SetupFullscreenWithID(contextId, contextID, videoRect, initialSize, size, page->deviceScaleFactor(), fullscreenMode, allowsPictureInPicture, standby, videoElement->document().quirks().blocksReturnToFullscreenFromPictureInPictureQuirk()));

        if (auto player = videoElement->player()) {
            if (auto identifier = player->identifier())
                protectedThis->setPlayerIdentifier(contextId, identifier);
        }
    };

    LayerHostingContextID contextID = 0;
    bool blockMediaLayerRehosting = videoElement.document().settings().blockMediaLayerRehostingInWebContentProcess() && videoElement.document().page() &&  videoElement.document().page()->chrome().client().isUsingUISideCompositing();
    if (blockMediaLayerRehosting) {
        contextID = videoElement.layerHostingContextID();
        if (!contextID) {
            m_setupFullscreenHandler = WTFMove(setupFullscreen);
            videoElement.requestHostingContextID([this, protectedThis = Ref { *this }, videoElement = Ref { videoElement }] (auto contextID) {
                if (!contextID || !m_setupFullscreenHandler)
                    return;
                m_setupFullscreenHandler(contextID, FloatSize(videoElement->videoWidth(), videoElement->videoHeight()));
            });
            return;
        }
    } else
        contextID = interface->layerHostingContext()->contextID();

    setupFullscreen(contextID, FloatSize(videoElement.videoWidth(), videoElement.videoHeight()));
}

void VideoPresentationManager::exitVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, CompletionHandler<void(bool)>&& completionHandler)
{
    INFO_LOG(LOGIDENTIFIER);
    LOG(Fullscreen, "VideoPresentationManager::exitVideoFullscreenForVideoElement(%p)", this);

    ASSERT(m_page);

    if (!m_videoElements.contains(videoElement)) {
        completionHandler(false);
        return;
    }

    auto contextId = m_videoElements.get(videoElement);
    auto& interface = ensureInterface(contextId);
    if (interface.animationState() != VideoPresentationInterfaceContext::AnimationType::None) {
        completionHandler(false);
        return;
    }

    m_page->sendWithAsyncReply(Messages::VideoPresentationManagerProxy::ExitFullscreen(contextId, inlineVideoFrame(videoElement)), [protectedThis = Ref { *this }, this, contextId, videoElementPtr = &videoElement, completionHandler = WTFMove(completionHandler)](auto success) mutable {
        if (!success) {
            completionHandler(false);
            return;
        }

        if (m_videoElementInPictureInPicture == videoElementPtr)
            m_videoElementInPictureInPicture = nullptr;

        auto& interface = ensureInterface(contextId);
        protectedThis->setCurrentlyInFullscreen(interface, false);
        interface.setAnimationState(VideoPresentationInterfaceContext::AnimationType::FromFullscreen);
        completionHandler(true);
    });
}

void VideoPresentationManager::exitVideoFullscreenToModeWithoutAnimation(HTMLVideoElement& videoElement, WebCore::HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
    INFO_LOG(LOGIDENTIFIER);

    ASSERT(m_page);

    if (!m_videoElements.contains(videoElement))
        return;

    if (m_videoElementInPictureInPicture == &videoElement)
        m_videoElementInPictureInPicture = nullptr;

    auto contextId = m_videoElements.get(videoElement);
    if (!contextId.isValid()) {
        // We have somehow managed to be asked to exit video fullscreen
        // for a video element which was either never in fullscreen or
        // has already been removed. Bail.
        ASSERT_NOT_REACHED();
        return;
    }
    auto& interface = ensureInterface(contextId);

    setCurrentlyInFullscreen(interface, false);

    m_page->send(Messages::VideoPresentationManagerProxy::ExitFullscreenWithoutAnimationToMode(contextId, targetMode));
}

#pragma mark Interface to VideoPresentationInterfaceContext:

void VideoPresentationManager::hasVideoChanged(PlaybackSessionContextIdentifier contextId, bool hasVideo)
{
    if (m_page)
        m_page->send(Messages::VideoPresentationManagerProxy::SetHasVideo(contextId, hasVideo));
}

void VideoPresentationManager::videoDimensionsChanged(PlaybackSessionContextIdentifier contextId, const FloatSize& videoDimensions)
{
    if (m_setupFullscreenHandler) {
        auto [model, interface] = ensureModelAndInterface(contextId);
        RefPtr videoElement = model->videoElement();
        auto layerHostingContextID = videoElement ? videoElement->layerHostingContextID() : 0;
        if (layerHostingContextID) {
            m_setupFullscreenHandler(layerHostingContextID, videoDimensions);
            m_setupFullscreenHandler = nullptr;
        }
    }
    if (m_page)
        m_page->send(Messages::VideoPresentationManagerProxy::SetVideoDimensions(contextId, videoDimensions));
}

void VideoPresentationManager::setPlayerIdentifier(PlaybackSessionContextIdentifier contextIdentifier, std::optional<MediaPlayerIdentifier> playerIdentifier)
{
    if (m_page)
        m_page->send(Messages::VideoPresentationManagerProxy::SetPlayerIdentifier(contextIdentifier, playerIdentifier));
}

#pragma mark Messages from VideoPresentationManagerProxy:

void VideoPresentationManager::requestFullscreenMode(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    ensureModel(contextId).requestFullscreenMode(mode, finishedWithMedia);
}

void VideoPresentationManager::fullscreenModeChanged(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    model->fullscreenModeChanged(videoFullscreenMode);
    interface->setFullscreenMode(videoFullscreenMode);
}

void VideoPresentationManager::requestUpdateInlineRect(PlaybackSessionContextIdentifier contextId)
{
    if (!m_page)
        return;

    Ref model = ensureModel(contextId);
    RefPtr videoElement = model->videoElement();
    auto inlineRect = inlineVideoFrame(*videoElement);
    RefPtr { m_page.get() }->send(Messages::VideoPresentationManagerProxy::SetInlineRect(contextId, inlineRect, inlineRect != IntRect(0, 0, 0, 0)));
}

void VideoPresentationManager::requestVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    INFO_LOG(LOGIDENTIFIER, model->logIdentifier());

    auto videoLayer = interface->rootLayer();

    model->setVideoFullscreenLayer(videoLayer.get(), [protectedThis = Ref { *this }, contextId] () mutable {
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), contextId] {
            if (RefPtr page = protectedThis->m_page.get())
                page->send(Messages::VideoPresentationManagerProxy::SetHasVideoContentLayer(contextId, true));
        });
    });
}

void VideoPresentationManager::returnVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    RefPtr<VideoPresentationModelVideoElement> model;
    RefPtr<VideoPresentationInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);
    INFO_LOG(LOGIDENTIFIER, model->logIdentifier());

    model->waitForPreparedForInlineThen([protectedThis = Ref { *this }, contextId, model] () mutable { // need this for return video layer
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), contextId, model] () mutable {
            model->setVideoFullscreenLayer(nil, [protectedThis = WTFMove(protectedThis), contextId] () mutable {
                RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), contextId] {
                    if (RefPtr page = protectedThis->m_page.get())
                        page->send(Messages::VideoPresentationManagerProxy::SetHasVideoContentLayer(contextId, false));
                });
            });
        });
    });
}

#if !PLATFORM(IOS_FAMILY)
void VideoPresentationManager::didSetupFullscreen(PlaybackSessionContextIdentifier contextId)
{
    ASSERT(m_page);
    auto [model, interface] = ensureModelAndInterface(contextId);
    INFO_LOG(LOGIDENTIFIER, model->logIdentifier());
    CALayer* videoLayer = interface->rootLayer().get();

    model->setVideoFullscreenLayer(videoLayer, [protectedThis = Ref { *this }, contextId] () mutable {
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), contextId] {
            if (RefPtr page = protectedThis->m_page.get())
                page->send(Messages::VideoPresentationManagerProxy::EnterFullscreen(contextId));
        });
    });
}
#endif

void VideoPresentationManager::willExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    INFO_LOG(LOGIDENTIFIER, model->logIdentifier());

    RefPtr<HTMLVideoElement> videoElement = model->videoElement();
    if (!videoElement)
        return;

    RunLoop::main().dispatch([protectedThis = Ref { *this }, videoElement = WTFMove(videoElement), contextId] {
        videoElement->willExitFullscreen();
        if (RefPtr page = protectedThis->m_page.get())
            page->send(Messages::VideoPresentationManagerProxy::PreparedToExitFullscreen(contextId));
    });
}

void VideoPresentationManager::didEnterFullscreen(PlaybackSessionContextIdentifier contextId, std::optional<WebCore::FloatSize> size)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    if (size)
        INFO_LOG(LOGIDENTIFIER, model->logIdentifier(), *size);
    else
        INFO_LOG(LOGIDENTIFIER, model->logIdentifier(), "(empty)");

    interface->setAnimationState(VideoPresentationInterfaceContext::AnimationType::None);
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

void VideoPresentationManager::failedToEnterFullscreen(PlaybackSessionContextIdentifier contextId)
{
#if PLATFORM(IOS_FAMILY)
    RefPtr<VideoPresentationModelVideoElement> model;
    RefPtr<VideoPresentationInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);
    INFO_LOG(LOGIDENTIFIER, model->logIdentifier());

    RunLoop::main().dispatch([protectedThis = Ref { *this }, contextId, interface] {
        if (RefPtr page = protectedThis->m_page.get())
            page->send(Messages::VideoPresentationManagerProxy::CleanupFullscreen(contextId));
    });
#endif
}

void VideoPresentationManager::didExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    INFO_LOG(LOGIDENTIFIER, contextId.toUInt64());

    RefPtr<VideoPresentationModelVideoElement> model;
    RefPtr<VideoPresentationInterfaceContext> interface;
    std::tie(model, interface) = ensureModelAndInterface(contextId);

#if PLATFORM(IOS_FAMILY)
    RunLoop::main().dispatch([protectedThis = Ref { *this }, contextId, interface] {
        if (RefPtr page = protectedThis->m_page.get())
            page->send(Messages::VideoPresentationManagerProxy::CleanupFullscreen(contextId));
    });
#else
    model->waitForPreparedForInlineThen([protectedThis = Ref { *this }, contextId, interface, model]() mutable {
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), contextId, interface, model] () mutable {
            model->setVideoFullscreenLayer(nil, [protectedThis = WTFMove(protectedThis), contextId, interface] () mutable {
                RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), contextId, interface] {
                    if (interface->rootLayer()) {
                        interface->setRootLayer(nullptr);
                        interface->setLayerHostingContext(nullptr);
                    }
                    if (RefPtr page = protectedThis->m_page.get())
                        page->send(Messages::VideoPresentationManagerProxy::CleanupFullscreen(contextId));
                });
            });
        });
    });
#endif
}

void VideoPresentationManager::didCleanupFullscreen(PlaybackSessionContextIdentifier contextId)
{
    INFO_LOG(LOGIDENTIFIER, contextId.toUInt64());

    auto [model, interface] = ensureModelAndInterface(contextId);

    if (interface->rootLayer()) {
        interface->setRootLayer(nullptr);
        interface->setLayerHostingContext(nullptr);
    }

    interface->setAnimationState(VideoPresentationInterfaceContext::AnimationType::None);
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

void VideoPresentationManager::setVideoLayerGravityEnum(PlaybackSessionContextIdentifier contextId, unsigned gravity)
{
    Ref model = ensureModel(contextId);
    INFO_LOG(LOGIDENTIFIER, model->logIdentifier(), gravity);

    model->setVideoLayerGravity((MediaPlayerEnums::VideoGravity)gravity);
}

void VideoPresentationManager::fullscreenMayReturnToInline(PlaybackSessionContextIdentifier contextId, bool isPageVisible)
{
    if (!m_page)
        return;

    Ref model = ensureModel(contextId);

    if (!isPageVisible)
        model->videoElement()->scrollIntoViewIfNotVisible(false);
    RefPtr videoElement = model->videoElement();
    RefPtr { m_page.get() }->send(Messages::VideoPresentationManagerProxy::PreparedToReturnToInline(contextId, true, inlineVideoFrame(*videoElement)));
}

void VideoPresentationManager::requestRouteSharingPolicyAndContextUID(PlaybackSessionContextIdentifier contextId, CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&& reply)
{
    ensureModel(contextId).requestRouteSharingPolicyAndContextUID(WTFMove(reply));
}

void VideoPresentationManager::setCurrentlyInFullscreen(VideoPresentationInterfaceContext& interface, bool currentlyInFullscreen)
{
    interface.setTargetIsFullscreen(currentlyInFullscreen);
    m_currentlyInFullscreen = currentlyInFullscreen;
}

void VideoPresentationManager::setVideoLayerFrameFenced(PlaybackSessionContextIdentifier contextId, WebCore::FloatRect bounds, WTF::MachSendRight&& machSendRight)
{
    INFO_LOG(LOGIDENTIFIER, contextId.toUInt64());

    auto [model, interface] = ensureModelAndInterface(contextId);

    if (std::isnan(bounds.x()) || std::isnan(bounds.y()) || std::isnan(bounds.width()) || std::isnan(bounds.height())) {
        RefPtr videoElement = model->videoElement();
        auto videoRect = inlineVideoFrame(*videoElement);
        bounds = FloatRect(0, 0, videoRect.width(), videoRect.height());
    }

    if (interface->layerHostingContext() && interface->layerHostingContext()->rootLayer()) {
        interface->layerHostingContext()->setFencePort(machSendRight.sendRight());
        model->setVideoLayerFrame(bounds);
    } else
        model->setVideoSizeFenced(bounds.size(), WTFMove(machSendRight));
}

void VideoPresentationManager::updateTextTrackRepresentationForVideoElement(WebCore::HTMLVideoElement& videoElement, ShareableBitmap::Handle&& textTrack)
{
    if (!m_page)
        return;
    auto contextId = m_videoElements.get(videoElement);
    m_page->send(Messages::VideoPresentationManagerProxy::TextTrackRepresentationUpdate(contextId, WTFMove(textTrack)));
}

void VideoPresentationManager::setTextTrackRepresentationContentScaleForVideoElement(WebCore::HTMLVideoElement& videoElement, float scale)
{
    if (!m_page)
        return;
    auto contextId = m_videoElements.get(videoElement);
    m_page->send(Messages::VideoPresentationManagerProxy::TextTrackRepresentationSetContentsScale(contextId, scale));

}

void VideoPresentationManager::setTextTrackRepresentationIsHiddenForVideoElement(WebCore::HTMLVideoElement& videoElement, bool hidden)
{
    if (!m_page)
        return;
    auto contextId = m_videoElements.get(videoElement);
    m_page->send(Messages::VideoPresentationManagerProxy::TextTrackRepresentationSetHidden(contextId, hidden));

}

#if !RELEASE_LOG_DISABLED
const Logger& VideoPresentationManager::logger() const
{
    return m_playbackSessionManager->logger();
}

const void* VideoPresentationManager::logIdentifier() const
{
    return m_playbackSessionManager->logIdentifier();
}

const char* VideoPresentationManager::logClassName() const
{
    return m_playbackSessionManager->logClassName();
}

WTFLogChannel& VideoPresentationManager::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

} // namespace WebKit

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
