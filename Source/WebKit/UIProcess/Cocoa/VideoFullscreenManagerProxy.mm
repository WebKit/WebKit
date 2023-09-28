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

#if ENABLE(VIDEO_PRESENTATION_MODE)

#import "APIUIClient.h"
#import "DrawingAreaProxy.h"
#import "GPUProcessProxy.h"
#import "MessageSenderInlines.h"
#import "PlaybackSessionManagerProxy.h"
#import "VideoFullscreenManagerMessages.h"
#import "VideoFullscreenManagerProxyMessages.h"
#import "WKVideoView.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/MediaPlayerEnums.h>
#import <WebCore/NullVideoFullscreenInterface.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/VideoFullscreenInterfaceAVKit.h>
#import <WebCore/VideoFullscreenInterfaceMac.h>
#import <WebCore/WebAVPlayerLayer.h>
#import <WebCore/WebAVPlayerLayerView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/LoggerHelper.h>
#import <wtf/MachSendRight.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "UIKitSPI.h"
#import <UIKit/UIView.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#endif

@interface WKLayerHostView : PlatformView
@property (nonatomic, assign) uint32_t contextID;
@end

@implementation WKLayerHostView {
#if PLATFORM(IOS_FAMILY)
    WeakObjCPtr<UIWindow> _window;
#endif
}

#if PLATFORM(IOS_FAMILY)
+ (Class)layerClass {
    return [CALayerHost class];
}
#else
- (CALayer *)makeBackingLayer
{
    return adoptNS([[CALayerHost alloc] init]).autorelease();
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

- (BOOL)clipsToBounds {
    return NO;
}

#if PLATFORM(IOS_FAMILY)
- (void)willMoveToWindow:(UIWindow *)newWindow {
    _window = newWindow;
    [super willMoveToWindow:newWindow];
}

- (UIWindow *)window {
    if (!_window)
        return nil;
    return [super window];
}
#endif

@end

#if PLATFORM(IOS_FAMILY)
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

#if !PLATFORM(VISION)
- (BOOL)prefersStatusBarHidden
{
    return YES;
}
#endif

@end

#endif

namespace WebKit {
using namespace WebCore;

#if PLATFORM(IOS_FAMILY) && !HAVE(AVKIT)

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

VideoFullscreenModelContext::VideoFullscreenModelContext(VideoFullscreenManagerProxy& manager, PlaybackSessionModelContext& playbackSessionModel, PlaybackSessionContextIdentifier contextId)
    : m_manager(manager)
    , m_playbackSessionModel(playbackSessionModel)
    , m_contextId(contextId)
{
}

VideoFullscreenModelContext::~VideoFullscreenModelContext() = default;

void VideoFullscreenModelContext::addClient(VideoFullscreenModelClient& client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.add(client);
}

void VideoFullscreenModelContext::removeClient(VideoFullscreenModelClient& client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(client);
}

void VideoFullscreenModelContext::setPlayerLayer(RetainPtr<WebAVPlayerLayer>&& playerLayer)
{
    m_playerLayer = WTFMove(playerLayer);
    [m_playerLayer setVideoDimensions:m_videoDimensions];
}

void VideoFullscreenModelContext::setVideoDimensions(const WebCore::FloatSize& videoDimensions)
{
    if (m_videoDimensions == videoDimensions)
        return;

    m_videoDimensions = videoDimensions;
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, videoDimensions);
    m_clients.forEach([&](auto& client) {
        client.videoDimensionsChanged(videoDimensions);
    });
}

void VideoFullscreenModelContext::requestCloseAllMediaPresentations(bool finishedWithMedia, CompletionHandler<void()>&& completionHandler)
{
    if (!m_manager) {
        completionHandler();
        return;
    }

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_manager->requestCloseAllMediaPresentations(m_contextId, finishedWithMedia, WTFMove(completionHandler));
}

void VideoFullscreenModelContext::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, mode, ", finishedWithMedia: ", finishedWithMedia);
    if (m_manager)
        m_manager->requestFullscreenMode(m_contextId, mode, finishedWithMedia);
}

void VideoFullscreenModelContext::setVideoLayerFrame(WebCore::FloatRect frame)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, frame);
    if (m_manager)
        m_manager->setVideoLayerFrame(m_contextId, frame);
}

void VideoFullscreenModelContext::setVideoLayerGravity(WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, gravity);
    if (m_manager)
        m_manager->setVideoLayerGravity(m_contextId, gravity);
}

void VideoFullscreenModelContext::fullscreenModeChanged(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, mode);
    if (m_manager)
        m_manager->fullscreenModeChanged(m_contextId, mode);
}

#if PLATFORM(IOS_FAMILY)
UIViewController *VideoFullscreenModelContext::presentingViewController()
{
    if (m_manager)
        return m_manager->m_page->uiClient().presentingViewController();

    return nullptr;
}

RetainPtr<UIViewController> VideoFullscreenModelContext::createVideoFullscreenViewController(AVPlayerViewController *avPlayerViewController)
{
    return adoptNS([[WKVideoFullScreenViewController alloc] initWithAVPlayerViewController:avPlayerViewController]);
}
#endif

void VideoFullscreenModelContext::requestUpdateInlineRect()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->requestUpdateInlineRect(m_contextId);
}

void VideoFullscreenModelContext::requestVideoContentLayer()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->requestVideoContentLayer(m_contextId);
}

void VideoFullscreenModelContext::returnVideoContentLayer()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->returnVideoContentLayer(m_contextId);
}

void VideoFullscreenModelContext::returnVideoView()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->returnVideoView(m_contextId);
}

void VideoFullscreenModelContext::didSetupFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->didSetupFullscreen(m_contextId);
}

void VideoFullscreenModelContext::failedToEnterFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->failedToEnterFullscreen(m_contextId);
}

void VideoFullscreenModelContext::didEnterFullscreen(const WebCore::FloatSize& size)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, size);
    if (m_manager)
        m_manager->didEnterFullscreen(m_contextId, size);
}

void VideoFullscreenModelContext::willExitFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->willExitFullscreen(m_contextId);
}

void VideoFullscreenModelContext::didExitFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->didExitFullscreen(m_contextId);
}

void VideoFullscreenModelContext::didCleanupFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->didCleanupFullscreen(m_contextId);
}

void VideoFullscreenModelContext::fullscreenMayReturnToInline()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->fullscreenMayReturnToInline(m_contextId);
}

void VideoFullscreenModelContext::requestRouteSharingPolicyAndContextUID(CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&& completionHandler)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->requestRouteSharingPolicyAndContextUID(m_contextId, WTFMove(completionHandler));
    else
        completionHandler(WebCore::RouteSharingPolicy::Default, emptyString());
}

void VideoFullscreenModelContext::didEnterPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->hasVideoInPictureInPictureDidChange(true);
}

void VideoFullscreenModelContext::didExitPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_manager)
        m_manager->hasVideoInPictureInPictureDidChange(false);
}

void VideoFullscreenModelContext::willEnterPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_clients.forEach([&](auto& client) {
        client.willEnterPictureInPicture();
    });
}

void VideoFullscreenModelContext::failedToEnterPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_clients.forEach([&](auto& client) {
        client.failedToEnterPictureInPicture();
    });
}

void VideoFullscreenModelContext::willExitPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_clients.forEach([&](auto& client) {
        client.willExitPictureInPicture();
    });
}

#if !RELEASE_LOG_DISABLED
const void* VideoFullscreenModelContext::logIdentifier() const
{
    return m_playbackSessionModel->logIdentifier();
}

const void* VideoFullscreenModelContext::nextChildIdentifier() const
{
    return LoggerHelper::childLogIdentifier(m_playbackSessionModel->logIdentifier(), ++m_childIdentifierSeed);
}

const Logger* VideoFullscreenModelContext::loggerPtr() const
{
    return m_playbackSessionModel->loggerPtr();
}

WTFLogChannel& VideoFullscreenModelContext::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

#pragma mark - VideoFullscreenManagerProxy

Ref<VideoFullscreenManagerProxy> VideoFullscreenManagerProxy::create(WebPageProxy& page, PlaybackSessionManagerProxy& playbackSessionManagerProxy)
{
    return adoptRef(*new VideoFullscreenManagerProxy(page, playbackSessionManagerProxy));
}

VideoFullscreenManagerProxy::VideoFullscreenManagerProxy(WebPageProxy& page, PlaybackSessionManagerProxy& playbackSessionManagerProxy)
    : m_page(page)
    , m_playbackSessionManagerProxy(playbackSessionManagerProxy)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_page->process().addMessageReceiver(Messages::VideoFullscreenManagerProxy::messageReceiverName(), m_page->webPageID(), *this);
}

VideoFullscreenManagerProxy::~VideoFullscreenManagerProxy()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    callCloseCompletionHandlers();

    if (!m_page)
        return;
    invalidate();
}

void VideoFullscreenManagerProxy::invalidate()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_page->process().removeMessageReceiver(Messages::VideoFullscreenManagerProxy::messageReceiverName(), m_page->webPageID());
    m_page = nullptr;

    auto contextMap = WTFMove(m_contextMap);
    m_clientCounts.clear();

    for (auto& [model, interface] : contextMap.values()) {
        interface->invalidate();
        [model->layerHostView() removeFromSuperview];
        model->setLayerHostView(nullptr);
        [model->playerLayer() setFullscreenModel:nil];
    }
}

void VideoFullscreenManagerProxy::requestHideAndExitFullscreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    for (auto& [model, interface] : m_contextMap.values())
        interface->requestHideAndExitFullscreen();
}

bool VideoFullscreenManagerProxy::hasMode(HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
    for (auto& [model, interface] : m_contextMap.values()) {
        if (interface->hasMode(mode))
            return true;
    }
    return false;
}

bool VideoFullscreenManagerProxy::mayAutomaticallyShowVideoPictureInPicture() const
{
    for (auto& [model, interface] : m_contextMap.values()) {
        if (interface->mayAutomaticallyShowVideoPictureInPicture())
            return true;
    }
    return false;
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
bool VideoFullscreenManagerProxy::isPlayingVideoInEnhancedFullscreen() const
{
    for (auto& [model, interface] : m_contextMap.values()) {
        if (interface->isPlayingVideoInEnhancedFullscreen())
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
    ALWAYS_LOG(LOGIDENTIFIER);
    for (auto& [model, interface] : m_contextMap.values())
        interface->applicationDidBecomeActive();
}

void VideoFullscreenManagerProxy::requestRouteSharingPolicyAndContextUID(PlaybackSessionContextIdentifier contextId, CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&& callback)
{
    m_page->sendWithAsyncReply(Messages::VideoFullscreenManager::RequestRouteSharingPolicyAndContextUID(contextId), WTFMove(callback));
}

VideoFullscreenManagerProxy::ModelInterfaceTuple VideoFullscreenManagerProxy::createModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto& playbackSessionModel = m_playbackSessionManagerProxy->ensureModel(contextId);
    auto model = VideoFullscreenModelContext::create(*this, playbackSessionModel, contextId);
    auto& playbackSessionInterface = m_playbackSessionManagerProxy->ensureInterface(contextId);
    Ref<PlatformVideoFullscreenInterface> interface = PlatformVideoFullscreenInterface::create(playbackSessionInterface);
    m_playbackSessionManagerProxy->addClientForContext(contextId);

    interface->setVideoFullscreenModel(model.ptr());

    return std::make_tuple(WTFMove(model), WTFMove(interface));
}

VideoFullscreenManagerProxy::ModelInterfaceTuple& VideoFullscreenManagerProxy::ensureModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_contextMap.add(contextId, ModelInterfaceTuple());
    if (addResult.isNewEntry)
        addResult.iterator->value = createModelAndInterface(contextId);
    return addResult.iterator->value;
}

VideoFullscreenModelContext& VideoFullscreenManagerProxy::ensureModel(PlaybackSessionContextIdentifier contextId)
{
    return *std::get<0>(ensureModelAndInterface(contextId));
}

PlatformVideoFullscreenInterface& VideoFullscreenManagerProxy::ensureInterface(PlaybackSessionContextIdentifier contextId)
{
    return *std::get<1>(ensureModelAndInterface(contextId));
}

PlatformVideoFullscreenInterface* VideoFullscreenManagerProxy::findInterface(PlaybackSessionContextIdentifier contextId) const
{
    auto it = m_contextMap.find(contextId);
    if (it == m_contextMap.end())
        return nullptr;

    return std::get<1>(it->value).get();
}

void VideoFullscreenManagerProxy::ensureClientForContext(PlaybackSessionContextIdentifier contextId)
{
    m_clientCounts.add(contextId, 1);
}

void VideoFullscreenManagerProxy::addClientForContext(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_clientCounts.add(contextId, 1);
    if (!addResult.isNewEntry)
        addResult.iterator->value++;
}

void VideoFullscreenManagerProxy::removeClientForContext(PlaybackSessionContextIdentifier contextId)
{
    if (!m_clientCounts.contains(contextId))
        return;

    int clientCount = m_clientCounts.get(contextId);
    ASSERT(clientCount > 0);
    clientCount--;

    if (clientCount <= 0) {
        auto& interface = ensureInterface(contextId);
        interface.setVideoFullscreenModel(nullptr);
        interface.invalidate();
        m_playbackSessionManagerProxy->removeClientForContext(contextId);
        m_clientCounts.remove(contextId);
        m_contextMap.remove(contextId);
        return;
    }

    m_clientCounts.set(contextId, clientCount);
}

void VideoFullscreenManagerProxy::forEachSession(Function<void(VideoFullscreenModelContext&, PlatformVideoFullscreenInterface&)>&& callback)
{
    if (m_contextMap.isEmpty())
        return;

    for (auto& value : copyToVector(m_contextMap.values())) {
        RefPtr<VideoFullscreenModelContext> model;
        RefPtr<PlatformVideoFullscreenInterface> interface;
        std::tie(model, interface) = value;

        ASSERT(model);
        ASSERT(interface);
        if (!model || !interface)
            continue;

        callback(*model, *interface);
    }
}

void VideoFullscreenManagerProxy::requestBitmapImageForCurrentTime(PlaybackSessionContextIdentifier identifier, CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&)>&& completionHandler)
{
    RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
    if (!gpuProcess) {
        completionHandler(std::nullopt);
        return;
    }

    auto* interface = findInterface(identifier);
    if (!interface) {
        completionHandler(std::nullopt);
        return;
    }

    auto playerIdentifier = valueOrDefault(interface->playerIdentifier());
    if (!playerIdentifier) {
        completionHandler(std::nullopt);
        return;
    }

    gpuProcess->requestBitmapImageForCurrentTime(m_page->process().coreProcessIdentifier(), playerIdentifier, WTFMove(completionHandler));
}

void VideoFullscreenManagerProxy::addVideoInPictureInPictureDidChangeObserver(const VideoInPictureInPictureDidChangeObserver& observer)
{
    ASSERT(!m_pipChangeObservers.contains(observer));
    m_pipChangeObservers.add(observer);
}

void VideoFullscreenManagerProxy::hasVideoInPictureInPictureDidChange(bool value)
{
    ALWAYS_LOG(LOGIDENTIFIER, value);
    RefPtr page = m_page.get();
    page->uiClient().hasVideoInPictureInPictureDidChange(page.get(), value);
    m_pipChangeObservers.forEach([value] (auto& observer) { observer(value); });
}

PlatformLayerContainer VideoFullscreenManagerProxy::createLayerWithID(PlaybackSessionContextIdentifier contextId, WebKit::LayerHostingContextID videoLayerID, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& nativeSize, float hostingDeviceScaleFactor)
{
    auto& [model, interface] = ensureModelAndInterface(contextId);
    addClientForContext(contextId);

    if (model->videoDimensions().isEmpty() && !nativeSize.isEmpty())
        model->setVideoDimensions(nativeSize);

    RetainPtr<WKLayerHostView> view = createLayerHostViewWithID(contextId, videoLayerID, initialSize, hostingDeviceScaleFactor);

    if (!model->playerLayer()) {
        ALWAYS_LOG(LOGIDENTIFIER, model->logIdentifier(), ", Creating AVPlayerLayer, initialSize: ", initialSize, ", nativeSize: ", nativeSize);
        auto playerLayer = adoptNS([[WebAVPlayerLayer alloc] init]);

        [playerLayer setFullscreenModel:model.get()];
        [playerLayer setVideoSublayer:[view layer]];

        // The videoView may already be reparented in fullscreen, so only parent the view
        // if it has no existing parent:
        if (![[view layer] superlayer])
            [playerLayer addSublayer:[view layer]];

        model->setPlayerLayer(playerLayer.get());

        [playerLayer setFrame:CGRectMake(0, 0, initialSize.width(), initialSize.height())];
        [playerLayer setNeedsLayout];
        [playerLayer layoutIfNeeded];
    }

    return model->playerLayer();
}

RetainPtr<WKLayerHostView> VideoFullscreenManagerProxy::createLayerHostViewWithID(PlaybackSessionContextIdentifier contextId, WebKit::LayerHostingContextID videoLayerID, const WebCore::FloatSize& initialSize, float hostingDeviceScaleFactor)
{
    auto& [model, interface] = ensureModelAndInterface(contextId);

    RetainPtr<WKLayerHostView> view = static_cast<WKLayerHostView*>(model->layerHostView());
    if (!view) {
        view = adoptNS([[WKLayerHostView alloc] init]);
#if PLATFORM(MAC)
        [view setWantsLayer:YES];
#endif
        model->setLayerHostView(view);

        [view layer].masksToBounds = NO;
        [view layer].name = @"WKLayerHostView layer";
        [view layer].frame = CGRectMake(0, 0, initialSize.width(), initialSize.height());
    }

    [view setContextID:videoLayerID];

    interface->setupCaptionsLayer([view layer], initialSize);

    return view;
}

#if PLATFORM(IOS_FAMILY)
RetainPtr<WKVideoView> VideoFullscreenManagerProxy::createViewWithID(PlaybackSessionContextIdentifier contextId, WebKit::LayerHostingContextID videoLayerID, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& nativeSize, float hostingDeviceScaleFactor)
{
    auto& [model, interface] = ensureModelAndInterface(contextId);
    addClientForContext(contextId);

    RetainPtr<WKLayerHostView> view = createLayerHostViewWithID(contextId, videoLayerID, initialSize, hostingDeviceScaleFactor);

    if (!model->videoView()) {
        ALWAYS_LOG(LOGIDENTIFIER, model->logIdentifier(), ", Creating AVPlayerLayerView");
        auto initialFrame = CGRectMake(0, 0, initialSize.width(), initialSize.height());
        auto playerView = adoptNS([allocWebAVPlayerLayerViewInstance() initWithFrame:initialFrame]);

        RetainPtr playerLayer { (WebAVPlayerLayer *)[playerView layer] };

        [playerLayer setVideoDimensions:nativeSize];
        [playerLayer setFullscreenModel:model.get()];
        [playerLayer setVideoSublayer:[view layer]];

        [playerView addSubview:view.get()];

        // The videoView may already be reparented in fullscreen, so only parent the view
        // if it has no existing parent:
        if (![[view layer] superlayer])
            [playerLayer addSublayer:[view layer]];

        auto videoView = adoptNS([[WKVideoView alloc] initWithFrame:initialFrame]);
        [videoView addSubview:playerView.get()];

        model->setPlayerLayer(WTFMove(playerLayer));
        model->setPlayerView(playerView.get());
        model->setVideoView(videoView.get());
    }

    return model->videoView();
}
#endif

void VideoFullscreenManagerProxy::willRemoveLayerForID(PlaybackSessionContextIdentifier contextId)
{
    removeClientForContext(contextId);
}

#pragma mark Messages from VideoFullscreenManager

void VideoFullscreenManagerProxy::setupFullscreenWithID(PlaybackSessionContextIdentifier contextId, WebKit::LayerHostingContextID videoLayerID, const WebCore::FloatRect& screenRect, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& videoDimensions, float hostingDeviceScaleFactor, HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode, bool allowsPictureInPicture, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    auto& [model, interface] = ensureModelAndInterface(contextId);

    // Do not add another refcount for this contextId if the interface is already in
    // a fullscreen mode, lest the refcounts get out of sync, as removeClientForContext
    // is only called once both PiP and video fullscreen are fully exited.
    if (interface->mode() == HTMLMediaElementEnums::VideoFullscreenModeNone)
        addClientForContext(contextId);

    if (m_mockVideoPresentationModeEnabled) {
        if (!videoDimensions.isEmpty())
            m_mockPictureInPictureWindowSize.setHeight(DefaultMockPictureInPictureWindowWidth /  videoDimensions.aspectRatio());
#if PLATFORM(IOS_FAMILY)
        requestVideoContentLayer(contextId);
#else
        didSetupFullscreen(contextId);
#endif
        return;
    }

    RetainPtr<WKLayerHostView> view = createLayerHostViewWithID(contextId, videoLayerID, initialSize, hostingDeviceScaleFactor);

#if PLATFORM(IOS_FAMILY)
    auto* rootNode = downcast<RemoteLayerTreeDrawingAreaProxy>(*m_page->drawingArea()).remoteLayerTreeHost().rootNode();
    UIView *parentView = rootNode ? rootNode->uiView() : nil;
    interface->setupFullscreen(*model->layerHostView(), screenRect, videoDimensions, parentView, videoFullscreenMode, allowsPictureInPicture, standby, blocksReturnToFullscreenFromPictureInPicture);
#else
    UNUSED_PARAM(videoDimensions);
    UNUSED_PARAM(blocksReturnToFullscreenFromPictureInPicture);
    IntRect initialWindowRect;
    m_page->rootViewToWindow(enclosingIntRect(screenRect), initialWindowRect);
    interface->setupFullscreen(*model->layerHostView(), initialWindowRect, m_page->platformWindow(), videoFullscreenMode, allowsPictureInPicture);
#endif
}

void VideoFullscreenManagerProxy::setPlayerIdentifier(PlaybackSessionContextIdentifier contextId, std::optional<MediaPlayerIdentifier> playerIdentifier)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    if (auto* interface = findInterface(contextId))
        interface->setPlayerIdentifier(playerIdentifier);
}

void VideoFullscreenManagerProxy::setHasVideo(PlaybackSessionContextIdentifier contextId, bool hasVideo)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    if (auto* interface = findInterface(contextId))
        interface->hasVideoChanged(hasVideo);
}

void VideoFullscreenManagerProxy::setVideoDimensions(PlaybackSessionContextIdentifier contextId, const FloatSize& videoDimensions)
{
    auto& [model, interface] = ensureModelAndInterface(contextId);
    model->setVideoDimensions(videoDimensions);

    if (m_mockVideoPresentationModeEnabled) {
        if (videoDimensions.isEmpty())
            return;

        m_mockPictureInPictureWindowSize.setHeight(DefaultMockPictureInPictureWindowWidth / videoDimensions.aspectRatio());
        return;
    }
}

void VideoFullscreenManagerProxy::enterFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (m_mockVideoPresentationModeEnabled) {
        didEnterFullscreen(contextId, m_mockPictureInPictureWindowSize);
        return;
    }

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

void VideoFullscreenManagerProxy::exitFullscreen(PlaybackSessionContextIdentifier contextId, WebCore::FloatRect finalRect, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(m_contextMap.contains(contextId));
    if (!m_contextMap.contains(contextId)) {
        completionHandler(false);
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    IntRect finalWindowRect;
    m_page->rootViewToWindow(enclosingIntRect(finalRect), finalWindowRect);
#else
    if (hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard))
        m_page->fullscreenMayReturnToInline();
#endif

    if (m_mockVideoPresentationModeEnabled) {
#if PLATFORM(IOS_FAMILY)
        returnVideoContentLayer(contextId);
#else
        didExitFullscreen(contextId);
#endif
        completionHandler(true);
        return;
    }

#if PLATFORM(IOS_FAMILY)
    completionHandler(ensureInterface(contextId).exitFullscreen(finalRect));
#else
    completionHandler(ensureInterface(contextId).exitFullscreen(finalWindowRect, m_page->platformWindow()));
#endif
}

void VideoFullscreenManagerProxy::exitFullscreenWithoutAnimationToMode(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
    if (m_mockVideoPresentationModeEnabled) {
        fullscreenModeChanged(contextId, targetMode);
        return;
    }

    ensureInterface(contextId).exitFullscreenWithoutAnimationToMode(targetMode);

    hasVideoInPictureInPictureDidChange(targetMode & MediaPlayerEnums::VideoFullscreenModePictureInPicture);
}

#if PLATFORM(IOS_FAMILY)

void VideoFullscreenManagerProxy::setInlineRect(PlaybackSessionContextIdentifier contextId, const WebCore::FloatRect& inlineRect, bool visible)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    ensureInterface(contextId).setInlineRect(inlineRect, visible);
}

void VideoFullscreenManagerProxy::setHasVideoContentLayer(PlaybackSessionContextIdentifier contextId, bool value)
{
    if (m_mockVideoPresentationModeEnabled) {
        if (value)
            didSetupFullscreen(contextId);
        else
            didExitFullscreen(contextId);

        return;
    }

    ensureInterface(contextId).setHasVideoContentLayer(value);
}

#else

NO_RETURN_DUE_TO_ASSERT void VideoFullscreenManagerProxy::setInlineRect(PlaybackSessionContextIdentifier, const WebCore::FloatRect&, bool)
{
    ASSERT_NOT_REACHED();
}

NO_RETURN_DUE_TO_ASSERT void VideoFullscreenManagerProxy::setHasVideoContentLayer(PlaybackSessionContextIdentifier, bool)
{
    ASSERT_NOT_REACHED();
}

#endif

void VideoFullscreenManagerProxy::cleanupFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (m_mockVideoPresentationModeEnabled) {
        didCleanupFullscreen(contextId);
        return;
    }

    ensureInterface(contextId).cleanupFullscreen();
}

void VideoFullscreenManagerProxy::preparedToReturnToInline(PlaybackSessionContextIdentifier contextId, bool visible, WebCore::FloatRect inlineRect)
{
    m_page->fullscreenMayReturnToInline();

#if !PLATFORM(IOS_FAMILY)
    IntRect inlineWindowRect;
    m_page->rootViewToWindow(enclosingIntRect(inlineRect), inlineWindowRect);
#endif

    if (m_mockVideoPresentationModeEnabled)
        return;

#if PLATFORM(IOS_FAMILY)
    ensureInterface(contextId).preparedToReturnToInline(visible, inlineRect);
#else
    ensureInterface(contextId).preparedToReturnToInline(visible, inlineWindowRect, m_page->platformWindow());
#endif
}

void VideoFullscreenManagerProxy::preparedToExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    ensureInterface(contextId).preparedToExitFullscreen();
}

void VideoFullscreenManagerProxy::textTrackRepresentationUpdate(PlaybackSessionContextIdentifier contextId, ShareableBitmap::Handle&& textTrack)
{
    auto bitmap = ShareableBitmap::create(WTFMove(textTrack));
    if (!bitmap)
        return;
    
    auto platformImage = bitmap->createPlatformImage();
    ensureInterface(contextId).setTrackRepresentationImage(platformImage);
}

void VideoFullscreenManagerProxy::textTrackRepresentationSetContentsScale(PlaybackSessionContextIdentifier contextId, float scale)
{
    ensureInterface(contextId).setTrackRepresentationContentsScale(scale);
}

void VideoFullscreenManagerProxy::textTrackRepresentationSetHidden(PlaybackSessionContextIdentifier contextId, bool hidden)
{
    ensureInterface(contextId).setTrackRepresentationHidden(hidden);
}

#pragma mark Messages to VideoFullscreenManager

void VideoFullscreenManagerProxy::callCloseCompletionHandlers()
{
    auto closeMediaCallbacks = WTFMove(m_closeCompletionHandlers);
    for (auto& callback : closeMediaCallbacks)
        callback();
}

void VideoFullscreenManagerProxy::requestCloseAllMediaPresentations(PlaybackSessionContextIdentifier contextId, bool finishedWithMedia, CompletionHandler<void()>&& completionHandler)
{
    if (!hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        && !hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        completionHandler();
        return;
    }

    m_closeCompletionHandlers.append(WTFMove(completionHandler));
    requestFullscreenMode(contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone, finishedWithMedia);
}

void VideoFullscreenManagerProxy::requestFullscreenMode(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    m_page->send(Messages::VideoFullscreenManager::RequestFullscreenMode(contextId, mode, finishedWithMedia));
}

void VideoFullscreenManagerProxy::requestUpdateInlineRect(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::VideoFullscreenManager::RequestUpdateInlineRect(contextId));
}

void VideoFullscreenManagerProxy::requestVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::VideoFullscreenManager::RequestVideoContentLayer(contextId));
}

void VideoFullscreenManagerProxy::returnVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::VideoFullscreenManager::ReturnVideoContentLayer(contextId));
}

void VideoFullscreenManagerProxy::returnVideoView(PlaybackSessionContextIdentifier contextId)
{
#if PLATFORM(IOS_FAMILY)
    auto& model = ensureModel(contextId);
    auto *playerView = model.playerView();
    auto *videoView = model.layerHostView();
    if (playerView && videoView) {
        [playerView addSubview:videoView];
        [playerView setNeedsLayout];
        [playerView layoutIfNeeded];
    }
#else
    UNUSED_PARAM(contextId);
#endif
}

void VideoFullscreenManagerProxy::didSetupFullscreen(PlaybackSessionContextIdentifier contextId)
{
#if PLATFORM(IOS_FAMILY)
    enterFullscreen(contextId);
#else
    m_page->send(Messages::VideoFullscreenManager::DidSetupFullscreen(contextId));
#endif
}

void VideoFullscreenManagerProxy::willExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::VideoFullscreenManager::WillExitFullscreen(contextId));
}

void VideoFullscreenManagerProxy::didExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::VideoFullscreenManager::DidExitFullscreen(contextId));

#if PLATFORM(IOS_FAMILY)
    if (ensureInterface(contextId).changingStandbyOnly()) {
        callCloseCompletionHandlers();
        return;
    }
#endif
    m_page->didExitFullscreen(contextId);
    callCloseCompletionHandlers();
}

void VideoFullscreenManagerProxy::didEnterFullscreen(PlaybackSessionContextIdentifier contextId, const WebCore::FloatSize& size)
{
    std::optional<FloatSize> optionalSize;
    if (!size.isEmpty())
        optionalSize = size;

    m_page->send(Messages::VideoFullscreenManager::DidEnterFullscreen(contextId, optionalSize));

#if PLATFORM(IOS_FAMILY)
    if (ensureInterface(contextId).changingStandbyOnly())
        return;
#endif
    m_page->didEnterFullscreen(contextId);
}

void VideoFullscreenManagerProxy::failedToEnterFullscreen(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::VideoFullscreenManager::FailedToEnterFullscreen(contextId));
}

void VideoFullscreenManagerProxy::didCleanupFullscreen(PlaybackSessionContextIdentifier contextId)
{
    auto& [model, interface] = ensureModelAndInterface(contextId);

    [model->layerHostView() removeFromSuperview];
    interface->removeCaptionsLayer();
    if (auto playerLayer = model->playerLayer()) {
        // Return the video layer to the player layer
        auto videoView = model->layerHostView();
        [playerLayer addSublayer:[videoView layer]];
        [playerLayer layoutSublayers];
    } else {
        [CATransaction flush];
        [model->layerHostView() removeFromSuperview];
        model->setLayerHostView(nullptr);
    }

    m_page->send(Messages::VideoFullscreenManager::DidCleanupFullscreen(contextId));

    interface->setMode(HTMLMediaElementEnums::VideoFullscreenModeNone, false);
    removeClientForContext(contextId);
}

void VideoFullscreenManagerProxy::setVideoLayerFrame(PlaybackSessionContextIdentifier contextId, WebCore::FloatRect frame)
{
    auto& [model, interface] = ensureModelAndInterface(contextId);
    interface->setCaptionsFrame(CGRectMake(0, 0, frame.width(), frame.height()));
#if PLATFORM(IOS_FAMILY)
    auto fenceSendRight = MachSendRight::adopt([UIWindow _synchronizeDrawingAcrossProcesses]);
#else
    MachSendRight fenceSendRight;
    if (DrawingAreaProxy* drawingArea = m_page->drawingArea())
        fenceSendRight = drawingArea->createFence();
#endif

    m_page->send(Messages::VideoFullscreenManager::SetVideoLayerFrameFenced(contextId, frame, WTFMove(fenceSendRight)));
}

void VideoFullscreenManagerProxy::setVideoLayerGravity(PlaybackSessionContextIdentifier contextId, WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    m_page->send(Messages::VideoFullscreenManager::SetVideoLayerGravityEnum(contextId, (unsigned)gravity));
}

void VideoFullscreenManagerProxy::fullscreenModeChanged(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    m_page->send(Messages::VideoFullscreenManager::FullscreenModeChanged(contextId, mode));
}

void VideoFullscreenManagerProxy::fullscreenMayReturnToInline(PlaybackSessionContextIdentifier contextId)
{
    m_page->send(Messages::VideoFullscreenManager::FullscreenMayReturnToInline(contextId, m_page->isViewVisible()));
}

#endif

#if PLATFORM(IOS_FAMILY)

AVPlayerViewController *VideoFullscreenManagerProxy::playerViewController(PlaybackSessionContextIdentifier identifier) const
{
    auto* interface = findInterface(identifier);
    return interface ? interface->avPlayerViewController() : nil;
}

#endif // PLATFORM(IOS_FAMILY)

#if !RELEASE_LOG_DISABLED
const Logger& VideoFullscreenManagerProxy::logger() const
{
    return m_playbackSessionManagerProxy->logger();
}

const void* VideoFullscreenManagerProxy::logIdentifier() const
{
    return m_playbackSessionManagerProxy->logIdentifier();
}

const char* VideoFullscreenManagerProxy::logClassName() const
{
    return m_playbackSessionManagerProxy->logClassName();
}

WTFLogChannel& VideoFullscreenManagerProxy::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

} // namespace WebKit

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
