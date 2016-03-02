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
#import "WebVideoFullscreenManagerProxy.h"

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "WebVideoFullscreenManagerMessages.h"
#import "WebVideoFullscreenManagerProxyMessages.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/QuartzCoreSPI.h>
#import <WebCore/TimeRanges.h>
#import <WebKitSystemInterface.h>

#if PLATFORM(IOS)
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "UIKitSPI.h"
#endif

@interface WKLayerHostView : PlatformView
@property (nonatomic, assign) uint32_t contextID;
@end

@implementation WKLayerHostView

#if PLATFORM(IOS)
+ (Class)layerClass {
    return [CALayerHost class];
}
#else
- (CALayer *)makeBackingLayer
{
    return [[CALayerHost alloc] init];
}
#endif

- (uint32_t)contextID {
    return [[self layerHost] contextId];
}

- (void)setContextID:(uint32_t)contextID {
    [[self layerHost] setContextId:contextID];
}

- (CALayerHost *)layerHost {
    return (CALayerHost *)[self layer];
}

@end

using namespace WebCore;

namespace WebKit {

#if PLATFORM(IOS) && !HAVE(AVKIT)

RefPtr<WebVideoFullscreenManagerProxy> WebVideoFullscreenManagerProxy::create(WebPageProxy&)
{
    return nullptr;
}

void WebVideoFullscreenManagerProxy::invalidate()
{
}

bool WebVideoFullscreenManagerProxy::hasMode(HTMLMediaElementEnums::VideoFullscreenMode) const
{
    return false;
}

bool WebVideoFullscreenManagerProxy::mayAutomaticallyShowVideoPictureInPicture() const
{
    return false;
}

void WebVideoFullscreenManagerProxy::requestHideAndExitFullscreen()
{

}

void WebVideoFullscreenManagerProxy::applicationDidBecomeActive()
{

}
#else

#pragma mark - WebVideoFullscreenModelContext

void WebVideoFullscreenModelContext::play()
{
    if (m_manager)
        m_manager->play(m_contextId);
}

void WebVideoFullscreenModelContext::pause()
{
    if (m_manager)
        m_manager->pause(m_contextId);
}

void WebVideoFullscreenModelContext::togglePlayState()
{
    if (m_manager)
        m_manager->togglePlayState(m_contextId);
}

void WebVideoFullscreenModelContext::beginScrubbing()
{
    if (m_manager)
        m_manager->beginScrubbing(m_contextId);
}

void WebVideoFullscreenModelContext::endScrubbing()
{
    if (m_manager)
        m_manager->endScrubbing(m_contextId);
}

void WebVideoFullscreenModelContext::seekToTime(double time)
{
    if (m_manager)
        m_manager->seekToTime(m_contextId, time);
}

void WebVideoFullscreenModelContext::fastSeek(double time)
{
    if (m_manager)
        m_manager->fastSeek(m_contextId, time);
}

void WebVideoFullscreenModelContext::beginScanningForward()
{
    if (m_manager)
        m_manager->beginScanningForward(m_contextId);
}

void WebVideoFullscreenModelContext::beginScanningBackward()
{
    if (m_manager)
        m_manager->beginScanningBackward(m_contextId);
}

void WebVideoFullscreenModelContext::endScanning()
{
    if (m_manager)
        m_manager->endScanning(m_contextId);
}

void WebVideoFullscreenModelContext::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    if (m_manager)
        m_manager->requestFullscreenMode(m_contextId, mode);
}

void WebVideoFullscreenModelContext::setVideoLayerFrame(WebCore::FloatRect frame)
{
    if (m_manager)
        m_manager->setVideoLayerFrame(m_contextId, frame);
}

void WebVideoFullscreenModelContext::setVideoLayerGravity(WebCore::WebVideoFullscreenModel::VideoGravity gravity)
{
    if (m_manager)
        m_manager->setVideoLayerGravity(m_contextId, gravity);
}

void WebVideoFullscreenModelContext::selectAudioMediaOption(uint64_t optionId)
{
    if (m_manager)
        m_manager->selectAudioMediaOption(m_contextId, optionId);
}

void WebVideoFullscreenModelContext::selectLegibleMediaOption(uint64_t optionId)
{
    if (m_manager)
        m_manager->selectLegibleMediaOption(m_contextId, optionId);
}

void WebVideoFullscreenModelContext::fullscreenModeChanged(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    if (m_manager)
        m_manager->fullscreenModeChanged(m_contextId, mode);
}

bool WebVideoFullscreenModelContext::isVisible() const
{
    return m_manager ? m_manager->isVisible() : false;
}

void WebVideoFullscreenModelContext::didSetupFullscreen()
{
    if (m_manager)
        m_manager->didSetupFullscreen(m_contextId);
}

void WebVideoFullscreenModelContext::didEnterFullscreen()
{
    if (m_manager)
        m_manager->didEnterFullscreen(m_contextId);
}

void WebVideoFullscreenModelContext::didExitFullscreen()
{
    if (m_manager)
        m_manager->didExitFullscreen(m_contextId);
}

void WebVideoFullscreenModelContext::didCleanupFullscreen()
{
    if (m_manager)
        m_manager->didCleanupFullscreen(m_contextId);
}

void WebVideoFullscreenModelContext::fullscreenMayReturnToInline()
{
    if (m_manager)
        m_manager->fullscreenMayReturnToInline(m_contextId);
}

#pragma mark - WebVideoFullscreenManagerProxy

RefPtr<WebVideoFullscreenManagerProxy> WebVideoFullscreenManagerProxy::create(WebPageProxy& page)
{
    return adoptRef(new WebVideoFullscreenManagerProxy(page));
}

WebVideoFullscreenManagerProxy::WebVideoFullscreenManagerProxy(WebPageProxy& page)
    : m_page(&page)
{
    m_page->process().addMessageReceiver(Messages::WebVideoFullscreenManagerProxy::messageReceiverName(), m_page->pageID(), *this);
}

WebVideoFullscreenManagerProxy::~WebVideoFullscreenManagerProxy()
{
    if (!m_page)
        return;
    invalidate();
}

void WebVideoFullscreenManagerProxy::invalidate()
{
    m_page->process().removeMessageReceiver(Messages::WebVideoFullscreenManagerProxy::messageReceiverName(), m_page->pageID());
    m_page = nullptr;

    for (auto& tuple : m_contextMap.values()) {
        RefPtr<WebVideoFullscreenModelContext> model;
        RefPtr<PlatformWebVideoFullscreenInterface> interface;
        std::tie(model, interface) = tuple;

        interface->invalidate();
        [model->layerHostView() removeFromSuperview];
        model->setLayerHostView(nullptr);
    }

    m_contextMap.clear();
}

void WebVideoFullscreenManagerProxy::requestHideAndExitFullscreen()
{
    for (auto& tuple : m_contextMap.values())
        std::get<1>(tuple)->requestHideAndExitFullscreen();
}

bool WebVideoFullscreenManagerProxy::hasMode(HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
    for (auto& tuple : m_contextMap.values()) {
        if (std::get<1>(tuple)->hasMode(mode))
            return true;
    }
    return false;
}

bool WebVideoFullscreenManagerProxy::mayAutomaticallyShowVideoPictureInPicture() const
{
    for (auto& tuple : m_contextMap.values()) {
        if (std::get<1>(tuple)->mayAutomaticallyShowVideoPictureInPicture())
            return true;
    }
    return false;
}

void WebVideoFullscreenManagerProxy::applicationDidBecomeActive()
{
    for (auto& tuple : m_contextMap.values())
        std::get<1>(tuple)->applicationDidBecomeActive();
}

WebVideoFullscreenManagerProxy::ModelInterfaceTuple WebVideoFullscreenManagerProxy::createModelAndInterface(uint64_t contextId)
{
    Ref<WebVideoFullscreenModelContext> model = WebVideoFullscreenModelContext::create(*this, contextId);
    Ref<PlatformWebVideoFullscreenInterface> interface = PlatformWebVideoFullscreenInterface::create();

    interface->setWebVideoFullscreenModel(&model.get());
    interface->setWebVideoFullscreenChangeObserver(&model.get());

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

WebVideoFullscreenManagerProxy::ModelInterfaceTuple& WebVideoFullscreenManagerProxy::ensureModelAndInterface(uint64_t contextId)
{
    auto addResult = m_contextMap.add(contextId, ModelInterfaceTuple());
    if (addResult.isNewEntry)
        addResult.iterator->value = createModelAndInterface(contextId);
    return addResult.iterator->value;
}

WebVideoFullscreenModelContext& WebVideoFullscreenManagerProxy::ensureModel(uint64_t contextId)
{
    return *std::get<0>(ensureModelAndInterface(contextId));
}

PlatformWebVideoFullscreenInterface& WebVideoFullscreenManagerProxy::ensureInterface(uint64_t contextId)
{
    return *std::get<1>(ensureModelAndInterface(contextId));
}

#pragma mark Messages from WebVideoFullscreenManager

void WebVideoFullscreenManagerProxy::setupFullscreenWithID(uint64_t contextId, uint32_t videoLayerID, const WebCore::IntRect& initialRect, float hostingDeviceScaleFactor, HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode, bool allowsPictureInPicture)
{
    ASSERT(videoLayerID);
    RefPtr<WebVideoFullscreenModelContext> model;
    RefPtr<PlatformWebVideoFullscreenInterface> interface;

    std::tie(model, interface) = ensureModelAndInterface(contextId);

    RetainPtr<WKLayerHostView> view = static_cast<WKLayerHostView*>(model->layerHostView());
    if (!view) {
        view = adoptNS([[WKLayerHostView alloc] init]);
#if PLATFORM(MAC)
        [view setWantsLayer:YES];
#endif
        model->setLayerHostView(view);
    }
    [view setContextID:videoLayerID];
    if (hostingDeviceScaleFactor != 1) {
        // Invert the scale transform added in the WebProcess to fix <rdar://problem/18316542>.
        float inverseScale = 1 / hostingDeviceScaleFactor;
        [[view layer] setSublayerTransform:CATransform3DMakeScale(inverseScale, inverseScale, 1)];
    }

#if PLATFORM(IOS)
    UIView *parentView = downcast<RemoteLayerTreeDrawingAreaProxy>(*m_page->drawingArea()).remoteLayerTreeHost().rootLayer();
    interface->setupFullscreen(*model->layerHostView(), initialRect, parentView, videoFullscreenMode, allowsPictureInPicture);
#else
    IntRect initialWindowRect;
    m_page->rootViewToWindow(initialRect, initialWindowRect);
    interface->setupFullscreen(*model->layerHostView(), initialWindowRect, m_page->platformWindow(), videoFullscreenMode, allowsPictureInPicture);
#endif
}

void WebVideoFullscreenManagerProxy::setUpVideoControlsManagerWithID(uint64_t contextId)
{
#if PLATFORM(MAC)
    // If there is an existing controls manager, clean it up.
    if (m_controlsManagerContextId)
        m_contextMap.remove(m_controlsManagerContextId);

    RefPtr<WebVideoFullscreenModelContext> model;
    RefPtr<PlatformWebVideoFullscreenInterface> interface;

    std::tie(model, interface) = ensureModelAndInterface(contextId);
    m_controlsManagerContextId = contextId;
    interface->ensureControlsManager();
#else
    UNUSED_PARAM(contextId);
#endif
}

void WebVideoFullscreenManagerProxy::resetMediaState(uint64_t contextId)
{
    ensureInterface(contextId).resetMediaState();
}

void WebVideoFullscreenManagerProxy::setCurrentTime(uint64_t contextId, double currentTime, double hostTime)
{
    ensureInterface(contextId).setCurrentTime(currentTime, hostTime);
}

void WebVideoFullscreenManagerProxy::setBufferedTime(uint64_t contextId, double bufferedTime)
{
    ensureInterface(contextId).setBufferedTime(bufferedTime);
}

void WebVideoFullscreenManagerProxy::setVideoDimensions(uint64_t contextId, bool hasVideo, unsigned width, unsigned height)
{
    ensureInterface(contextId).setVideoDimensions(hasVideo, width, height);
}

void WebVideoFullscreenManagerProxy::setSeekableRangesVector(uint64_t contextId, Vector<std::pair<double, double>> ranges)
{
    RefPtr<TimeRanges> timeRanges = TimeRanges::create();
    for (const auto& range : ranges) {
        ASSERT(isfinite(range.first));
        ASSERT(isfinite(range.second));
        ASSERT(range.second >= range.first);
        timeRanges->add(range.first, range.second);
    }

    ensureInterface(contextId).setSeekableRanges(*timeRanges);
}

void WebVideoFullscreenManagerProxy::setCanPlayFastReverse(uint64_t contextId, bool value)
{
    ensureInterface(contextId).setCanPlayFastReverse(value);
}

void WebVideoFullscreenManagerProxy::setAudioMediaSelectionOptions(uint64_t contextId, Vector<String> options, uint64_t selectedIndex)
{
    ensureInterface(contextId).setAudioMediaSelectionOptions(options, selectedIndex);
}

void WebVideoFullscreenManagerProxy::setLegibleMediaSelectionOptions(uint64_t contextId, Vector<String> options, uint64_t selectedIndex)
{
    ensureInterface(contextId).setLegibleMediaSelectionOptions(options, selectedIndex);
}

void WebVideoFullscreenManagerProxy::setExternalPlaybackProperties(uint64_t contextId, bool enabled, uint32_t targetType, String localizedDeviceName)
{
    WebVideoFullscreenInterface::ExternalPlaybackTargetType type = static_cast<WebVideoFullscreenInterface::ExternalPlaybackTargetType>(targetType);
    ASSERT(type == WebVideoFullscreenInterface::TargetTypeAirPlay || type == WebVideoFullscreenInterface::TargetTypeTVOut || type == WebVideoFullscreenInterface::TargetTypeNone);
    
    ensureInterface(contextId).setExternalPlayback(enabled, type, localizedDeviceName);
}

void WebVideoFullscreenManagerProxy::setWirelessVideoPlaybackDisabled(uint64_t contextId, bool disabled)
{
    ensureInterface(contextId).setWirelessVideoPlaybackDisabled(disabled);
}

void WebVideoFullscreenManagerProxy::setDuration(uint64_t contextId, double duration)
{
    ensureInterface(contextId).setDuration(duration);
}

void WebVideoFullscreenManagerProxy::setRate(uint64_t contextId, bool isPlaying, double rate)
{
    ensureInterface(contextId).setRate(isPlaying, rate);
}

void WebVideoFullscreenManagerProxy::enterFullscreen(uint64_t contextId)
{
    auto& interface = ensureInterface(contextId);
    interface.enterFullscreen();

    // Only one context can be in a given full screen mode at a time:
    for (auto& contextPair : m_contextMap) {
        auto& otherContextId = contextPair.key;
        if (contextId == otherContextId)
            continue;

        auto& otherInterface = std::get<1>(contextPair.value);
        if (otherInterface->hasMode(interface.mode()))
            otherInterface->requestHideAndExitFullscreen();
    }
}

void WebVideoFullscreenManagerProxy::exitFullscreen(uint64_t contextId, WebCore::IntRect finalRect)
{
#if PLATFORM(IOS)
    ensureInterface(contextId).exitFullscreen(finalRect);
#else
    IntRect finalWindowRect;
    m_page->rootViewToWindow(finalRect, finalWindowRect);
    ensureInterface(contextId).exitFullscreen(finalWindowRect, m_page->platformWindow());
#endif
}

void WebVideoFullscreenManagerProxy::cleanupFullscreen(uint64_t contextId)
{
    ensureInterface(contextId).cleanupFullscreen();
}

void WebVideoFullscreenManagerProxy::preparedToReturnToInline(uint64_t contextId, bool visible, WebCore::IntRect inlineRect)
{
    m_page->fullscreenMayReturnToInline();

#if PLATFORM(IOS)
    ensureInterface(contextId).preparedToReturnToInline(visible, inlineRect);
#else
    IntRect inlineWindowRect;
    m_page->rootViewToWindow(inlineRect, inlineWindowRect);
    ensureInterface(contextId).preparedToReturnToInline(visible, inlineWindowRect, m_page->platformWindow());
#endif
}

#pragma mark Messages to WebVideoFullscreenManager

void WebVideoFullscreenManagerProxy::play(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::Play(contextId), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::pause(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::Pause(contextId), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::togglePlayState(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::TogglePlayState(contextId), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::beginScrubbing(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::BeginScrubbing(contextId), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::endScrubbing(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::EndScrubbing(contextId), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::seekToTime(uint64_t contextId, double time)
{
    m_page->send(Messages::WebVideoFullscreenManager::SeekToTime(contextId, time), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::fastSeek(uint64_t contextId, double time)
{
    m_page->send(Messages::WebVideoFullscreenManager::FastSeek(contextId, time), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::beginScanningForward(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::BeginScanningForward(contextId), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::beginScanningBackward(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::BeginScanningBackward(contextId), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::endScanning(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::EndScanning(contextId), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::requestFullscreenMode(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    m_page->send(Messages::WebVideoFullscreenManager::RequestFullscreenMode(contextId, mode), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::didSetupFullscreen(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::DidSetupFullscreen(contextId), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::didExitFullscreen(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::DidExitFullscreen(contextId), m_page->pageID());
    m_page->didExitFullscreen();
}

void WebVideoFullscreenManagerProxy::didEnterFullscreen(uint64_t contextId)
{
    m_page->send(Messages::WebVideoFullscreenManager::DidEnterFullscreen(contextId), m_page->pageID());
    m_page->didEnterFullscreen();
}

void WebVideoFullscreenManagerProxy::didCleanupFullscreen(uint64_t contextId)
{
    auto& model = ensureModel(contextId);

    [CATransaction flush];
    [model.layerHostView() removeFromSuperview];
    model.setLayerHostView(nullptr);
    m_page->send(Messages::WebVideoFullscreenManager::DidCleanupFullscreen(contextId), m_page->pageID());

    m_contextMap.remove(contextId);
}

void WebVideoFullscreenManagerProxy::setVideoLayerFrame(uint64_t contextId, WebCore::FloatRect frame)
{
    @autoreleasepool {
#if PLATFORM(IOS)
        mach_port_name_t fencePort = [UIWindow _synchronizeDrawingAcrossProcesses];
#else
        mach_port_name_t fencePort = 0;
#endif

        m_page->send(Messages::WebVideoFullscreenManager::SetVideoLayerFrameFenced(contextId, frame, IPC::Attachment(fencePort, MACH_MSG_TYPE_MOVE_SEND)), m_page->pageID());
    }
}

void WebVideoFullscreenManagerProxy::setVideoLayerGravity(uint64_t contextId, WebCore::WebVideoFullscreenModel::VideoGravity gravity)
{
    m_page->send(Messages::WebVideoFullscreenManager::SetVideoLayerGravityEnum(contextId, (unsigned)gravity), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::selectAudioMediaOption(uint64_t contextId, uint64_t index)
{
    m_page->send(Messages::WebVideoFullscreenManager::SelectAudioMediaOption(contextId, index), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::selectLegibleMediaOption(uint64_t contextId, uint64_t index)
{
    m_page->send(Messages::WebVideoFullscreenManager::SelectLegibleMediaOption(contextId, index), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::fullscreenModeChanged(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    m_page->send(Messages::WebVideoFullscreenManager::FullscreenModeChanged(contextId, mode), m_page->pageID());
}

bool WebVideoFullscreenManagerProxy::isVisible() const
{
    return m_page->isViewVisible() && m_page->isInWindow();
}

PlatformWebVideoFullscreenInterface& WebVideoFullscreenManagerProxy::controlsManagerInterface()
{
    return ensureInterface(m_controlsManagerContextId);
}

void WebVideoFullscreenManagerProxy::fullscreenMayReturnToInline(uint64_t contextId)
{
    bool isViewVisible = m_page->isViewVisible();
    m_page->send(Messages::WebVideoFullscreenManager::FullscreenMayReturnToInline(contextId, isViewVisible), m_page->pageID());
}

#endif

} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
