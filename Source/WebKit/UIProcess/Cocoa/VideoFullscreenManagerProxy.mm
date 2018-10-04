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
#import "VideoFullscreenManagerProxy.h"

#if PLATFORM(IOS) && HAVE(AVKIT) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "APIUIClient.h"
#import "DrawingAreaProxy.h"
#import "PlaybackSessionManagerProxy.h"
#import "VideoFullscreenManagerMessages.h"
#import "VideoFullscreenManagerProxyMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/MediaPlayerEnums.h>
#import <WebCore/TimeRanges.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRight.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS)
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "UIKitSPI.h"
#import <pal/spi/cocoa/AVKitSPI.h>
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
    return [[[CALayerHost alloc] init] autorelease];
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

#if PLATFORM(IOS)
@interface WKVideoFullScreenViewController : UIViewController
- (instancetype)initWithAVPlayerViewController:(AVPlayerViewController *)viewController NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder *)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
@end

@implementation WKVideoFullScreenViewController {
    WeakObjCPtr<AVPlayerViewController> _avPlayerViewController;
}

- (instancetype)initWithAVPlayerViewController:(AVPlayerViewController *)controller
{
    if (!(self = [super initWithNibName:nil bundle:nil]))
        return nil;

    _avPlayerViewController = controller;
    self.modalPresentationCapturesStatusBarAppearance = YES;
    self.modalPresentationStyle = UIModalPresentationOverFullScreen;

    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.view.frame = UIScreen.mainScreen.bounds;
    self.view.backgroundColor = [UIColor blackColor];
    [_avPlayerViewController view].autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

@end

#endif

namespace WebKit {
using namespace WebCore;

#if PLATFORM(IOS) && !HAVE(AVKIT)

RefPtr<VideoFullscreenManagerProxy> VideoFullscreenManagerProxy::create(WebPageProxy&)
{
    return nullptr;
}

void VideoFullscreenManagerProxy::invalidate()
{
}

bool VideoFullscreenManagerProxy::hasMode(HTMLMediaElementEnums::VideoFullscreenMode) const
{
    return false;
}

bool VideoFullscreenManagerProxy::mayAutomaticallyShowVideoPictureInPicture() const
{
    return false;
}

void VideoFullscreenManagerProxy::requestHideAndExitFullscreen()
{

}

void VideoFullscreenManagerProxy::applicationDidBecomeActive()
{

}
#else

#pragma mark - VideoFullscreenModelContext

VideoFullscreenModelContext::VideoFullscreenModelContext(VideoFullscreenManagerProxy& manager, PlaybackSessionModelContext& playbackSessionModel, uint64_t contextId)
    : m_manager(&manager)
    , m_playbackSessionModel(playbackSessionModel)
    , m_contextId(contextId)
{
}

VideoFullscreenModelContext::~VideoFullscreenModelContext()
{
}

void VideoFullscreenModelContext::addClient(VideoFullscreenModelClient& client)
{
    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);
}

void VideoFullscreenModelContext::removeClient(VideoFullscreenModelClient& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
}

void VideoFullscreenModelContext::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    if (m_manager)
        m_manager->requestFullscreenMode(m_contextId, mode, finishedWithMedia);
}

void VideoFullscreenModelContext::setVideoLayerFrame(WebCore::FloatRect frame)
{
    if (m_manager)
        m_manager->setVideoLayerFrame(m_contextId, frame);
}

void VideoFullscreenModelContext::setVideoLayerGravity(WebCore::VideoFullscreenModel::VideoGravity gravity)
{
    if (m_manager)
        m_manager->setVideoLayerGravity(m_contextId, gravity);
}

void VideoFullscreenModelContext::fullscreenModeChanged(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    if (m_manager)
        m_manager->fullscreenModeChanged(m_contextId, mode);
}

bool VideoFullscreenModelContext::isVisible() const
{
    return m_manager ? m_manager->isVisible() : false;
}

#if PLATFORM(IOS)
UIViewController *VideoFullscreenModelContext::presentingViewController()
{
    if (m_manager)
        return m_manager->m_page->uiClient().presentingViewController();

    return nullptr;
}

UIViewController *VideoFullscreenModelContext::createVideoFullscreenViewController(AVPlayerViewController *avPlayerViewController)
{
    return [[WKVideoFullScreenViewController alloc] initWithAVPlayerViewController:avPlayerViewController];
}
#endif

void VideoFullscreenModelContext::requestUpdateInlineRect()
{
    if (m_manager)
        m_manager->requestUpdateInlineRect(m_contextId);
}

void VideoFullscreenModelContext::requestVideoContentLayer()
{
    if (m_manager)
        m_manager->requestVideoContentLayer(m_contextId);
}

void VideoFullscreenModelContext::returnVideoContentLayer()
{
    if (m_manager)
        m_manager->returnVideoContentLayer(m_contextId);
}

void VideoFullscreenModelContext::didSetupFullscreen()
{
    if (m_manager)
        m_manager->didSetupFullscreen(m_contextId);
}

void VideoFullscreenModelContext::didEnterFullscreen()
{
    if (m_manager)
        m_manager->didEnterFullscreen(m_contextId);
}

void VideoFullscreenModelContext::willExitFullscreen()
{
    if (m_manager)
        m_manager->willExitFullscreen(m_contextId);
}

void VideoFullscreenModelContext::didExitFullscreen()
{
    if (m_manager)
        m_manager->didExitFullscreen(m_contextId);
}

void VideoFullscreenModelContext::didCleanupFullscreen()
{
    if (m_manager)
        m_manager->didCleanupFullscreen(m_contextId);
}

void VideoFullscreenModelContext::fullscreenMayReturnToInline()
{
    if (m_manager)
        m_manager->fullscreenMayReturnToInline(m_contextId);
}

void VideoFullscreenModelContext::willEnterPictureInPicture()
{
    for (auto& client : m_clients)
        client->willEnterPictureInPicture();
}

void VideoFullscreenModelContext::didEnterPictureInPicture()
{
    for (auto& client : m_clients)
        client->didEnterPictureInPicture();
}

void VideoFullscreenModelContext::failedToEnterPictureInPicture()
{
    for (auto& client : m_clients)
        client->failedToEnterPictureInPicture();
}

void VideoFullscreenModelContext::willExitPictureInPicture()
{
    for (auto& client : m_clients)
        client->willExitPictureInPicture();
}

void VideoFullscreenModelContext::didExitPictureInPicture()
{
    for (auto& client : m_clients)
        client->didExitPictureInPicture();
}

#pragma mark - VideoFullscreenManagerProxy

RefPtr<VideoFullscreenManagerProxy> VideoFullscreenManagerProxy::create(WebPageProxy& page, PlaybackSessionManagerProxy& playbackSessionManagerProxy)
{
    return adoptRef(new VideoFullscreenManagerProxy(page, playbackSessionManagerProxy));
}

VideoFullscreenManagerProxy::VideoFullscreenManagerProxy(WebPageProxy& page, PlaybackSessionManagerProxy& playbackSessionManagerProxy)
    : m_page(&page)
    , m_playbackSessionManagerProxy(playbackSessionManagerProxy)
{
    m_page->process().addMessageReceiver(Messages::VideoFullscreenManagerProxy::messageReceiverName(), m_page->pageID(), *this);
}

VideoFullscreenManagerProxy::~VideoFullscreenManagerProxy()
{
    if (!m_page)
        return;
    invalidate();
}

void VideoFullscreenManagerProxy::invalidate()
{
    m_page->process().removeMessageReceiver(Messages::VideoFullscreenManagerProxy::messageReceiverName(), m_page->pageID());
    m_page = nullptr;

    auto contextMap = WTFMove(m_contextMap);
    m_clientCounts.clear();

    for (auto& tuple : contextMap.values()) {
        RefPtr<VideoFullscreenModelContext> model;
        RefPtr<PlatformVideoFullscreenInterface> interface;
        std::tie(model, interface) = tuple;

        interface->invalidate();
        [model->layerHostView() removeFromSuperview];
        model->setLayerHostView(nullptr);
    }
}

void VideoFullscreenManagerProxy::requestHideAndExitFullscreen()
{
    for (auto& tuple : m_contextMap.values())
        std::get<1>(tuple)->requestHideAndExitFullscreen();
}

bool VideoFullscreenManagerProxy::hasMode(HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
    for (auto& tuple : m_contextMap.values()) {
        if (std::get<1>(tuple)->hasMode(mode))
            return true;
    }
    return false;
}

bool VideoFullscreenManagerProxy::mayAutomaticallyShowVideoPictureInPicture() const
{
    for (auto& tuple : m_contextMap.values()) {
        if (std::get<1>(tuple)->mayAutomaticallyShowVideoPictureInPicture())
            return true;
    }
    return false;
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
bool VideoFullscreenManagerProxy::isPlayingVideoInEnhancedFullscreen() const
{
    for (auto& tuple : m_contextMap.values()) {
        if (std::get<1>(tuple)->isPlayingVideoInEnhancedFullscreen())
            return true;
    }
    
    return false;
}
#endif

PlatformVideoFullscreenInterface* VideoFullscreenManagerProxy::controlsManagerInterface()
{
    if (auto contextId = m_playbackSessionManagerProxy->controlsManagerContextId())
        return &ensureInterface(contextId);
    return nullptr;
}

void VideoFullscreenManagerProxy::applicationDidBecomeActive()
{
    for (auto& tuple : m_contextMap.values())
        std::get<1>(tuple)->applicationDidBecomeActive();
}

VideoFullscreenManagerProxy::ModelInterfaceTuple VideoFullscreenManagerProxy::createModelAndInterface(uint64_t contextId)
{
    auto& playbackSessionModel = m_playbackSessionManagerProxy->ensureModel(contextId);
    Ref<VideoFullscreenModelContext> model = VideoFullscreenModelContext::create(*this, playbackSessionModel, contextId);
    auto& playbackSessionInterface = m_playbackSessionManagerProxy->ensureInterface(contextId);
    Ref<PlatformVideoFullscreenInterface> interface = PlatformVideoFullscreenInterface::create(playbackSessionInterface);
    m_playbackSessionManagerProxy->addClientForContext(contextId);

    interface->setVideoFullscreenModel(&model.get());
    interface->setVideoFullscreenChangeObserver(&model.get());

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

VideoFullscreenManagerProxy::ModelInterfaceTuple& VideoFullscreenManagerProxy::ensureModelAndInterface(uint64_t contextId)
{
    auto addResult = m_contextMap.add(contextId, ModelInterfaceTuple());
    if (addResult.isNewEntry)
        addResult.iterator->value = createModelAndInterface(contextId);
    return addResult.iterator->value;
}

VideoFullscreenModelContext& VideoFullscreenManagerProxy::ensureModel(uint64_t contextId)
{
    return *std::get<0>(ensureModelAndInterface(contextId));
}

PlatformVideoFullscreenInterface& VideoFullscreenManagerProxy::ensureInterface(uint64_t contextId)
{
    return *std::get<1>(ensureModelAndInterface(contextId));
}

void VideoFullscreenManagerProxy::addClientForContext(uint64_t contextId)
{
    auto addResult = m_clientCounts.add(contextId, 1);
    if (!addResult.isNewEntry)
        addResult.iterator->value++;
}

void VideoFullscreenManagerProxy::removeClientForContext(uint64_t contextId)
{
    ASSERT(m_clientCounts.contains(contextId));

    int clientCount = m_clientCounts.get(contextId);
    ASSERT(clientCount > 0);
    clientCount--;

    if (clientCount <= 0) {
        ensureInterface(contextId).setVideoFullscreenModel(nullptr);
        m_playbackSessionManagerProxy->removeClientForContext(contextId);
        m_clientCounts.remove(contextId);
        m_contextMap.remove(contextId);
        return;
    }

    m_clientCounts.set(contextId, clientCount);
}

#pragma mark Messages from VideoFullscreenManager

void VideoFullscreenManagerProxy::setupFullscreenWithID(uint64_t contextId, uint32_t videoLayerID, const WebCore::IntRect& initialRect, float hostingDeviceScaleFactor, HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode, bool allowsPictureInPicture, bool standby)
{
    ASSERT(videoLayerID);
    RefPtr<VideoFullscreenModelContext> model;
    RefPtr<PlatformVideoFullscreenInterface> interface;

    std::tie(model, interface) = ensureModelAndInterface(contextId);
    addClientForContext(contextId);

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
    interface->setupFullscreen(*model->layerHostView(), initialRect, parentView, videoFullscreenMode, allowsPictureInPicture, standby);
#else
    IntRect initialWindowRect;
    m_page->rootViewToWindow(initialRect, initialWindowRect);
    interface->setupFullscreen(*model->layerHostView(), initialWindowRect, m_page->platformWindow(), videoFullscreenMode, allowsPictureInPicture);
#endif
    m_page->uiClient().hasVideoInPictureInPictureDidChange(m_page, videoFullscreenMode & MediaPlayerEnums::VideoFullscreenModePictureInPicture);
}

void VideoFullscreenManagerProxy::setHasVideo(uint64_t contextId, bool hasVideo)
{
    ensureInterface(contextId).hasVideoChanged(hasVideo);
}

void VideoFullscreenManagerProxy::setVideoDimensions(uint64_t contextId, const FloatSize& videoDimensions)
{
    ensureInterface(contextId).videoDimensionsChanged(videoDimensions);
}

void VideoFullscreenManagerProxy::enterFullscreen(uint64_t contextId)
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

void VideoFullscreenManagerProxy::exitFullscreen(uint64_t contextId, WebCore::IntRect finalRect)
{
#if PLATFORM(IOS)
    ensureInterface(contextId).exitFullscreen(finalRect);
#else
    IntRect finalWindowRect;
    m_page->rootViewToWindow(finalRect, finalWindowRect);
    ensureInterface(contextId).exitFullscreen(finalWindowRect, m_page->platformWindow());
#endif
}

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
void VideoFullscreenManagerProxy::exitFullscreenWithoutAnimationToMode(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
    ensureInterface(contextId).exitFullscreenWithoutAnimationToMode(targetMode);
}
#endif

#if PLATFORM(IOS)

void VideoFullscreenManagerProxy::setInlineRect(uint64_t contextId, const WebCore::IntRect& inlineRect, bool visible)
{
    ensureInterface(contextId).setInlineRect(inlineRect, visible);
}

void VideoFullscreenManagerProxy::setHasVideoContentLayer(uint64_t contextId, bool value)
{
    ensureInterface(contextId).setHasVideoContentLayer(value);
}

#else

NO_RETURN_DUE_TO_ASSERT void VideoFullscreenManagerProxy::setInlineRect(uint64_t, const WebCore::IntRect&, bool)
{
    ASSERT_NOT_REACHED();
}

NO_RETURN_DUE_TO_ASSERT void VideoFullscreenManagerProxy::setHasVideoContentLayer(uint64_t, bool)
{
    ASSERT_NOT_REACHED();
}

#endif

void VideoFullscreenManagerProxy::cleanupFullscreen(uint64_t contextId)
{
    ensureInterface(contextId).cleanupFullscreen();
}

void VideoFullscreenManagerProxy::preparedToReturnToInline(uint64_t contextId, bool visible, WebCore::IntRect inlineRect)
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

void VideoFullscreenManagerProxy::preparedToExitFullscreen(uint64_t contextId)
{
    ensureInterface(contextId).preparedToExitFullscreen();
}

#pragma mark Messages to VideoFullscreenManager

void VideoFullscreenManagerProxy::requestFullscreenMode(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    m_page->send(Messages::VideoFullscreenManager::RequestFullscreenMode(contextId, mode, finishedWithMedia), m_page->pageID());
}

void VideoFullscreenManagerProxy::requestUpdateInlineRect(uint64_t contextId)
{
    m_page->send(Messages::VideoFullscreenManager::RequestUpdateInlineRect(contextId), m_page->pageID());
}

void VideoFullscreenManagerProxy::requestVideoContentLayer(uint64_t contextId)
{
    m_page->send(Messages::VideoFullscreenManager::RequestVideoContentLayer(contextId), m_page->pageID());
}

void VideoFullscreenManagerProxy::returnVideoContentLayer(uint64_t contextId)
{
    m_page->send(Messages::VideoFullscreenManager::ReturnVideoContentLayer(contextId), m_page->pageID());
}

void VideoFullscreenManagerProxy::didSetupFullscreen(uint64_t contextId)
{
    m_page->send(Messages::VideoFullscreenManager::DidSetupFullscreen(contextId), m_page->pageID());
}

void VideoFullscreenManagerProxy::willExitFullscreen(uint64_t contextId)
{
    m_page->send(Messages::VideoFullscreenManager::WillExitFullscreen(contextId), m_page->pageID());
}

void VideoFullscreenManagerProxy::didExitFullscreen(uint64_t contextId)
{
    m_page->send(Messages::VideoFullscreenManager::DidExitFullscreen(contextId), m_page->pageID());
    m_page->didExitFullscreen();
}

void VideoFullscreenManagerProxy::didEnterFullscreen(uint64_t contextId)
{
    m_page->send(Messages::VideoFullscreenManager::DidEnterFullscreen(contextId), m_page->pageID());
    m_page->didEnterFullscreen();
}

void VideoFullscreenManagerProxy::didCleanupFullscreen(uint64_t contextId)
{
    RefPtr<VideoFullscreenModelContext> model;
    RefPtr<PlatformVideoFullscreenInterface> interface;

    std::tie(model, interface) = ensureModelAndInterface(contextId);

    [CATransaction flush];
    [model->layerHostView() removeFromSuperview];
    model->setLayerHostView(nullptr);
    m_page->send(Messages::VideoFullscreenManager::DidCleanupFullscreen(contextId), m_page->pageID());

    interface->setMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
    removeClientForContext(contextId);
}

void VideoFullscreenManagerProxy::setVideoLayerFrame(uint64_t contextId, WebCore::FloatRect frame)
{
    @autoreleasepool {
#if PLATFORM(IOS)
        mach_port_name_t fencePort = [UIWindow _synchronizeDrawingAcrossProcesses];
#else
        MachSendRight fenceSendRight;
        if (DrawingAreaProxy* drawingArea = m_page->drawingArea())
            fenceSendRight = drawingArea->createFence();
        mach_port_name_t fencePort = fenceSendRight.leakSendRight();
#endif

        m_page->send(Messages::VideoFullscreenManager::SetVideoLayerFrameFenced(contextId, frame, IPC::Attachment(fencePort, MACH_MSG_TYPE_MOVE_SEND)), m_page->pageID());
    }
}

void VideoFullscreenManagerProxy::setVideoLayerGravity(uint64_t contextId, WebCore::VideoFullscreenModel::VideoGravity gravity)
{
    m_page->send(Messages::VideoFullscreenManager::SetVideoLayerGravityEnum(contextId, (unsigned)gravity), m_page->pageID());
}

void VideoFullscreenManagerProxy::fullscreenModeChanged(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    m_page->uiClient().hasVideoInPictureInPictureDidChange(m_page, mode & MediaPlayerEnums::VideoFullscreenModePictureInPicture);
    m_page->send(Messages::VideoFullscreenManager::FullscreenModeChanged(contextId, mode), m_page->pageID());
}

bool VideoFullscreenManagerProxy::isVisible() const
{
    return m_page->isViewVisible() && m_page->isInWindow();
}

void VideoFullscreenManagerProxy::fullscreenMayReturnToInline(uint64_t contextId)
{
    bool isViewVisible = m_page->isViewVisible();
    m_page->send(Messages::VideoFullscreenManager::FullscreenMayReturnToInline(contextId, isViewVisible), m_page->pageID());
}

#endif

} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
