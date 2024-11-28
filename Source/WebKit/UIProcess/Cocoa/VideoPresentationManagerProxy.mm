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
#import "VideoPresentationManagerProxy.h"

#if ENABLE(VIDEO_PRESENTATION_MODE)

#import "APIPageConfiguration.h"
#import "APIUIClient.h"
#import "DrawingAreaProxy.h"
#import "GPUProcessProxy.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "PageClient.h"
#import "PlaybackSessionInterfaceLMK.h"
#import "PlaybackSessionManagerProxy.h"
#import "VideoPresentationInterfaceLMK.h"
#import "VideoPresentationManagerMessages.h"
#import "VideoPresentationManagerProxyMessages.h"
#import "WKVideoView.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/MediaPlayerEnums.h>
#import <WebCore/NullVideoPresentationInterface.h>
#import <WebCore/PlaybackSessionInterfaceAVKit.h>
#import <WebCore/PlaybackSessionInterfaceMac.h>
#import <WebCore/PlaybackSessionInterfaceTVOS.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/VideoPresentationInterfaceAVKit.h>
#import <WebCore/VideoPresentationInterfaceMac.h>
#import <WebCore/VideoPresentationInterfaceTVOS.h>
#import <WebCore/WebAVPlayerLayer.h>
#import <WebCore/WebAVPlayerLayerView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/LoggerHelper.h>
#import <wtf/MachSendRight.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "UIKitSPI.h"
#import <UIKit/UIView.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#endif

#if USE(EXTENSIONKIT)
#import <BrowserEngineKit/BELayerHierarchyHostingTransactionCoordinator.h>
#import <BrowserEngineKit/BELayerHierarchyHostingView.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_page->legacyMainFrameProcess().connection())

@interface WKLayerHostView : PlatformView
@property (nonatomic, assign) uint32_t contextID;
#if USE(EXTENSIONKIT)
@property (nonatomic, strong) PlatformView *visibilityPropagationView;
#endif
@end

@implementation WKLayerHostView {
#if PLATFORM(IOS_FAMILY)
    WeakObjCPtr<UIWindow> _window;
#endif
#if USE(EXTENSIONKIT)
    RetainPtr<PlatformView> _visibilityPropagationView;
@public
    RetainPtr<BELayerHierarchyHostingView> _hostingView;
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

#if USE(EXTENSIONKIT)
- (PlatformView *)visibilityPropagationView
{
    return _visibilityPropagationView.get();
}

- (void)setVisibilityPropagationView:(PlatformView *)visibilityPropagationView
{
    [_visibilityPropagationView removeFromSuperview];
    _visibilityPropagationView = visibilityPropagationView;
    [self addSubview:_visibilityPropagationView.get()];
}
#endif // USE(EXTENSIONKIT)

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
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <rdar://131638772> UIScreen.mainScreen is deprecated.
    self.view.frame = UIScreen.mainScreen.bounds;
ALLOW_DEPRECATED_DECLARATIONS_END
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

RefPtr<VideoPresentationManagerProxy> VideoPresentationManagerProxy::create(WebPageProxy&)
{
    return nullptr;
}

void VideoPresentationManagerProxy::invalidate()
{
}

bool VideoPresentationManagerProxy::hasMode(HTMLMediaElementEnums::VideoFullscreenMode) const
{
    return false;
}

bool VideoPresentationManagerProxy::mayAutomaticallyShowVideoPictureInPicture() const
{
    return false;
}

void VideoPresentationManagerProxy::requestHideAndExitFullscreen()
{
}

void VideoPresentationManagerProxy::applicationDidBecomeActive()
{
}
#else

#pragma mark - VideoPresentationModelContext

VideoPresentationModelContext::VideoPresentationModelContext(VideoPresentationManagerProxy& manager, PlaybackSessionModelContext& playbackSessionModel, PlaybackSessionContextIdentifier contextId)
    : m_manager(manager)
    , m_playbackSessionModel(playbackSessionModel)
    , m_contextId(contextId)
{
}

VideoPresentationModelContext::~VideoPresentationModelContext() = default;

void VideoPresentationModelContext::addClient(VideoPresentationModelClient& client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.add(client);
}

void VideoPresentationModelContext::removeClient(VideoPresentationModelClient& client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(client);
}

void VideoPresentationModelContext::setPlayerLayer(RetainPtr<WebAVPlayerLayer>&& playerLayer)
{
    m_playerLayer = WTFMove(playerLayer);
    [m_playerLayer setVideoDimensions:m_videoDimensions];
}

void VideoPresentationManagerProxy::setDocumentVisibility(PlaybackSessionContextIdentifier contextId, bool isDocumentVisible)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    if (RefPtr interface = findInterface(contextId))
        interface->documentVisibilityChanged(isDocumentVisible);
}

void VideoPresentationModelContext::setVideoDimensions(const WebCore::FloatSize& videoDimensions)
{
    if (m_videoDimensions == videoDimensions)
        return;

    m_videoDimensions = videoDimensions;
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, videoDimensions, ", clients=", m_clients.computeSize());
    m_clients.forEach([&](auto& client) {
        client.videoDimensionsChanged(videoDimensions);
    });
}

void VideoPresentationModelContext::requestCloseAllMediaPresentations(bool finishedWithMedia, CompletionHandler<void()>&& completionHandler)
{
    RefPtr manager = m_manager.get();
    if (!manager) {
        completionHandler();
        return;
    }

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    manager->requestCloseAllMediaPresentations(m_contextId, finishedWithMedia, WTFMove(completionHandler));
}

void VideoPresentationModelContext::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, mode, ", finishedWithMedia: ", finishedWithMedia);
    if (RefPtr manager = m_manager.get())
        manager->requestFullscreenMode(m_contextId, mode, finishedWithMedia);
}

void VideoPresentationModelContext::setVideoLayerFrame(WebCore::FloatRect frame)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, frame);
    if (RefPtr manager = m_manager.get())
        manager->setVideoLayerFrame(m_contextId, frame);
}

void VideoPresentationModelContext::setVideoLayerGravity(WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, gravity);
    if (RefPtr manager = m_manager.get())
        manager->setVideoLayerGravity(m_contextId, gravity);
}

void VideoPresentationModelContext::setVideoFullscreenFrame(WebCore::FloatRect frame)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, frame);
    if (RefPtr manager = m_manager.get())
        manager->setVideoFullscreenFrame(m_contextId, frame);
}

void VideoPresentationModelContext::fullscreenModeChanged(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, mode);
    if (RefPtr manager = m_manager.get())
        manager->fullscreenModeChanged(m_contextId, mode);
}

#if PLATFORM(IOS_FAMILY)
UIViewController *VideoPresentationModelContext::presentingViewController()
{
    if (!m_manager || !m_manager->m_page)
        return nullptr;

    if (RefPtr pageClient = m_manager->m_page->pageClient())
        return pageClient->presentingViewController();
    return nullptr;
}

RetainPtr<UIViewController> VideoPresentationModelContext::createVideoFullscreenViewController(AVPlayerViewController *avPlayerViewController)
{
    return adoptNS([[WKVideoFullScreenViewController alloc] initWithAVPlayerViewController:avPlayerViewController]);
}
#endif

void VideoPresentationModelContext::requestUpdateInlineRect()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->requestUpdateInlineRect(m_contextId);
}

void VideoPresentationModelContext::requestVideoContentLayer()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->requestVideoContentLayer(m_contextId);
}

void VideoPresentationModelContext::returnVideoContentLayer()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->returnVideoContentLayer(m_contextId);
}

void VideoPresentationModelContext::returnVideoView()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->returnVideoView(m_contextId);
}

void VideoPresentationModelContext::didSetupFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->didSetupFullscreen(m_contextId);
}

void VideoPresentationModelContext::failedToEnterFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->failedToEnterFullscreen(m_contextId);
}

void VideoPresentationModelContext::didEnterFullscreen(const WebCore::FloatSize& size)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, size);
    if (RefPtr manager = m_manager.get())
        manager->didEnterFullscreen(m_contextId, size);
}

void VideoPresentationModelContext::willExitFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->willExitFullscreen(m_contextId);
}

void VideoPresentationModelContext::didExitFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->didExitFullscreen(m_contextId);
}

void VideoPresentationModelContext::didCleanupFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->didCleanupFullscreen(m_contextId);
}

void VideoPresentationModelContext::fullscreenMayReturnToInline()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->fullscreenMayReturnToInline(m_contextId);
}

void VideoPresentationModelContext::requestRouteSharingPolicyAndContextUID(CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&& completionHandler)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->requestRouteSharingPolicyAndContextUID(m_contextId, WTFMove(completionHandler));
    else
        completionHandler(WebCore::RouteSharingPolicy::Default, emptyString());
}

void VideoPresentationModelContext::didEnterPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->hasVideoInPictureInPictureDidChange(true);
}

void VideoPresentationModelContext::didExitPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->hasVideoInPictureInPictureDidChange(false);
}

void VideoPresentationModelContext::willEnterPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_clients.forEach([&](auto& client) {
        client.willEnterPictureInPicture();
    });
}

void VideoPresentationModelContext::failedToEnterPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_clients.forEach([&](auto& client) {
        client.failedToEnterPictureInPicture();
    });
}

void VideoPresentationModelContext::willExitPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_clients.forEach([&](auto& client) {
        client.willExitPictureInPicture();
    });
}

void VideoPresentationModelContext::setRequiresTextTrackRepresentation(bool requiresTextTrackRepresentation)
{
    if (RefPtr manager = m_manager.get())
        manager->setRequiresTextTrackRepresentation(m_contextId, requiresTextTrackRepresentation);
}

void VideoPresentationModelContext::setTextTrackRepresentationBounds(const IntRect& bounds)
{
    if (RefPtr manager = m_manager.get())
        manager->setTextTrackRepresentationBounds(m_contextId, bounds);
}

#if !RELEASE_LOG_DISABLED
uint64_t VideoPresentationModelContext::logIdentifier() const
{
    return m_playbackSessionModel->logIdentifier();
}

uint64_t VideoPresentationModelContext::nextChildIdentifier() const
{
    return LoggerHelper::childLogIdentifier(m_playbackSessionModel->logIdentifier(), ++m_childIdentifierSeed);
}

const Logger* VideoPresentationModelContext::loggerPtr() const
{
    return m_playbackSessionModel->loggerPtr();
}

WTFLogChannel& VideoPresentationModelContext::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

#pragma mark - VideoPresentationManagerProxy

Ref<VideoPresentationManagerProxy> VideoPresentationManagerProxy::create(WebPageProxy& page, PlaybackSessionManagerProxy& playbackSessionManagerProxy)
{
    return adoptRef(*new VideoPresentationManagerProxy(page, playbackSessionManagerProxy));
}

VideoPresentationManagerProxy::VideoPresentationManagerProxy(WebPageProxy& page, PlaybackSessionManagerProxy& playbackSessionManagerProxy)
    : m_page(page)
    , m_playbackSessionManagerProxy(playbackSessionManagerProxy)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    RefPtr protectedPage = m_page.get();
    protectedPage->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::VideoPresentationManagerProxy::messageReceiverName(), protectedPage->webPageIDInMainFrameProcess(), *this);
}

VideoPresentationManagerProxy::~VideoPresentationManagerProxy()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    callCloseCompletionHandlers();

    invalidate();
}

void VideoPresentationManagerProxy::invalidate()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr page = m_page.get()) {
        page->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::VideoPresentationManagerProxy::messageReceiverName(), page->webPageIDInMainFrameProcess());
        m_page = nullptr;
    }

    auto contextMap = std::exchange(m_contextMap, { });
    m_clientCounts.clear();

    for (auto& [model, interface] : contextMap.values()) {
        interface->invalidate();
        [model->layerHostView() removeFromSuperview];
        model->setLayerHostView(nullptr);
        [model->playerLayer() setPresentationModel:nil];
    }
}

void VideoPresentationManagerProxy::requestHideAndExitFullscreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    for (auto& [model, interface] : m_contextMap.values())
        interface->requestHideAndExitFullscreen();
}

bool VideoPresentationManagerProxy::hasMode(HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
    for (auto& [model, interface] : m_contextMap.values()) {
        if (interface->hasMode(mode))
            return true;
    }
    return false;
}

bool VideoPresentationManagerProxy::mayAutomaticallyShowVideoPictureInPicture() const
{
    for (auto& [model, interface] : m_contextMap.values()) {
        if (interface->mayAutomaticallyShowVideoPictureInPicture())
            return true;
    }
    return false;
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
bool VideoPresentationManagerProxy::isPlayingVideoInEnhancedFullscreen() const
{
    for (auto& [model, interface] : m_contextMap.values()) {
        if (interface->isPlayingVideoInEnhancedFullscreen())
            return true;
    }

    return false;
}
#endif

RefPtr<PlatformVideoPresentationInterface> VideoPresentationManagerProxy::controlsManagerInterface()
{
    if (auto contextId = protectedPlaybackSessionManagerProxy()->controlsManagerContextId())
        return ensureInterface(*contextId);
    return nullptr;
}

void VideoPresentationManagerProxy::applicationDidBecomeActive()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    for (auto& [model, interface] : m_contextMap.values())
        interface->applicationDidBecomeActive();
}

void VideoPresentationManagerProxy::requestRouteSharingPolicyAndContextUID(PlaybackSessionContextIdentifier contextId, CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::VideoPresentationManager::RequestRouteSharingPolicyAndContextUID(contextId), WTFMove(callback), page->webPageIDInMainFrameProcess());
    else
        callback({ }, { });
}

VideoPresentationManagerProxy::ModelInterfaceTuple VideoPresentationManagerProxy::createModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    Ref playbackSessionManagerProxy = m_playbackSessionManagerProxy;
    Ref playbackSessionModel = playbackSessionManagerProxy->ensureModel(contextId);
    Ref model = VideoPresentationModelContext::create(*this, playbackSessionModel, contextId);
    Ref playbackSessionInterface = playbackSessionManagerProxy->ensureInterface(contextId);

    RefPtr<PlatformVideoPresentationInterface> interface;
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_page->preferences().linearMediaPlayerEnabled())
        interface = VideoPresentationInterfaceLMK::create(playbackSessionInterface.get());
    else
        interface = VideoPresentationInterfaceAVKit::create(playbackSessionInterface.get());
#else
    interface = PlatformVideoPresentationInterface::create(playbackSessionInterface.get());
#endif

    playbackSessionManagerProxy->addClientForContext(contextId);

    interface->setVideoPresentationModel(model.ptr());

    return std::make_tuple(WTFMove(model), interface.releaseNonNull());
}

const VideoPresentationManagerProxy::ModelInterfaceTuple& VideoPresentationManagerProxy::ensureModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_contextMap.ensure(contextId, [&] {
        return createModelAndInterface(contextId);
    });
    return addResult.iterator->value;
}

Ref<VideoPresentationModelContext> VideoPresentationManagerProxy::ensureModel(PlaybackSessionContextIdentifier contextId)
{
    return std::get<0>(ensureModelAndInterface(contextId));
}

Ref<PlatformVideoPresentationInterface> VideoPresentationManagerProxy::ensureInterface(PlaybackSessionContextIdentifier contextId)
{
    return std::get<1>(ensureModelAndInterface(contextId));
}

RefPtr<PlatformVideoPresentationInterface> VideoPresentationManagerProxy::findInterface(PlaybackSessionContextIdentifier contextId) const
{
    auto it = m_contextMap.find(contextId);
    if (it == m_contextMap.end())
        return nullptr;

    return std::get<1>(it->value).ptr();
}

void VideoPresentationManagerProxy::ensureClientForContext(PlaybackSessionContextIdentifier contextId)
{
    m_clientCounts.add(contextId, 1);
}

void VideoPresentationManagerProxy::addClientForContext(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_clientCounts.add(contextId, 1);
    if (!addResult.isNewEntry)
        addResult.iterator->value++;
}

void VideoPresentationManagerProxy::removeClientForContext(PlaybackSessionContextIdentifier contextId)
{
    if (!m_clientCounts.contains(contextId))
        return;

    int clientCount = m_clientCounts.get(contextId);
    ASSERT(clientCount > 0);
    clientCount--;
    ALWAYS_LOG(LOGIDENTIFIER, clientCount);

    if (clientCount <= 0) {
        Ref interface = ensureInterface(contextId);
        interface->setVideoPresentationModel(nullptr);
        interface->invalidate();
        protectedPlaybackSessionManagerProxy()->removeClientForContext(contextId);
        m_clientCounts.remove(contextId);
        m_contextMap.remove(contextId);

        if (RefPtr page = m_page.get())
            page->didCleanupFullscreen(contextId);

        return;
    }

    m_clientCounts.set(contextId, clientCount);
}

void VideoPresentationManagerProxy::forEachSession(Function<void(VideoPresentationModelContext&, PlatformVideoPresentationInterface&)>&& callback)
{
    if (m_contextMap.isEmpty())
        return;

    for (const auto& value : copyToVector(m_contextMap.values())) {
        auto [model, interface] = value;
        callback(model, interface);
    }
}

void VideoPresentationManagerProxy::requestBitmapImageForCurrentTime(PlaybackSessionContextIdentifier identifier, CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&)>&& completionHandler)
{
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
    if (!gpuProcess) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr interface = findInterface(identifier);
    if (!interface) {
        completionHandler(std::nullopt);
        return;
    }

    auto playerIdentifier = interface->playerIdentifier();
    if (!playerIdentifier) {
        completionHandler(std::nullopt);
        return;
    }

    gpuProcess->requestBitmapImageForCurrentTime(page->protectedLegacyMainFrameProcess()->coreProcessIdentifier(), *playerIdentifier, WTFMove(completionHandler));
}

void VideoPresentationManagerProxy::addVideoInPictureInPictureDidChangeObserver(const VideoInPictureInPictureDidChangeObserver& observer)
{
    ASSERT(!m_pipChangeObservers.contains(observer));
    m_pipChangeObservers.add(observer);
}

void VideoPresentationManagerProxy::hasVideoInPictureInPictureDidChange(bool value)
{
    ALWAYS_LOG(LOGIDENTIFIER, value);
    RefPtr page = m_page.get();
    if (!page)
        return;
    page->uiClient().hasVideoInPictureInPictureDidChange(page.get(), value);
    m_pipChangeObservers.forEach([value] (auto& observer) { observer(value); });
}

PlatformLayerContainer VideoPresentationManagerProxy::createLayerWithID(PlaybackSessionContextIdentifier contextId, WebKit::LayerHostingContextID videoLayerID, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& nativeSize, float hostingDeviceScaleFactor)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    addClientForContext(contextId);

    Ref protectedModel = model;
    if (protectedModel->videoDimensions().isEmpty() && !nativeSize.isEmpty())
        protectedModel->setVideoDimensions(nativeSize);

    RetainPtr<WKLayerHostView> view = createLayerHostViewWithID(contextId, videoLayerID, initialSize, hostingDeviceScaleFactor);

    if (!protectedModel->playerLayer()) {
        ALWAYS_LOG(LOGIDENTIFIER, protectedModel->logIdentifier(), ", Creating AVPlayerLayer, initialSize: ", initialSize, ", nativeSize: ", nativeSize);
        auto playerLayer = adoptNS([[WebAVPlayerLayer alloc] init]);

        [playerLayer setPresentationModel:protectedModel.ptr()];
        [playerLayer setVideoSublayer:[view layer]];

        // The videoView may already be reparented in fullscreen, so only parent the view
        // if it has no existing parent:
        if (![[view layer] superlayer])
            [playerLayer addSublayer:[view layer]];

        protectedModel->setPlayerLayer(playerLayer.get());

        [playerLayer setFrame:CGRectMake(0, 0, initialSize.width(), initialSize.height())];
        [playerLayer setNeedsLayout];
        [playerLayer layoutIfNeeded];
    }

    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::EnsureUpdatedVideoDimensions(contextId, nativeSize), page->webPageIDInMainFrameProcess());

    return protectedModel->playerLayer();
}

RetainPtr<WKLayerHostView> VideoPresentationManagerProxy::createLayerHostViewWithID(PlaybackSessionContextIdentifier contextId, WebKit::LayerHostingContextID videoLayerID, const WebCore::FloatSize& initialSize, float hostingDeviceScaleFactor)
{
    auto [model, interface] = ensureModelAndInterface(contextId);

    RetainPtr<WKLayerHostView> view = static_cast<WKLayerHostView*>(model->layerHostView());
    if (!view) {
        view = adoptNS([[WKLayerHostView alloc] init]);
#if PLATFORM(IOS_FAMILY)
        [view setUserInteractionEnabled:NO];
#endif
#if PLATFORM(MAC)
        [view setWantsLayer:YES];
#endif
        model->setLayerHostView(view);

#if USE(EXTENSIONKIT)
        auto hostingView = adoptNS([[BELayerHierarchyHostingView alloc] init]);
        view->_hostingView = hostingView;
        [view addSubview:hostingView.get()];
        auto layer = [hostingView layer];
#else
        auto layer = [view layer];
#endif
        layer.masksToBounds = NO;
        layer.name = @"WKLayerHostView layer";
        layer.frame = CGRectMake(0, 0, initialSize.width(), initialSize.height());
    }

#if USE(EXTENSIONKIT)
    RefPtr page = m_page.get();
    if (RefPtr gpuProcess = page ? page->configuration().processPool().gpuProcess() : nullptr) {
        RetainPtr handle = LayerHostingContext::createHostingHandle(gpuProcess->processID(), videoLayerID);
        [view->_hostingView setHandle:handle.get()];
    } else
        RELEASE_LOG_ERROR(Media, "VideoPresentationManagerProxy::createLayerHostViewWithID: Unable to initialize hosting view, no GPU process");
#else
    [view setContextID:videoLayerID];
#endif

    interface->setupCaptionsLayer([view layer], initialSize);

    return view;
}

#if PLATFORM(IOS_FAMILY)
RefPtr<PlatformVideoPresentationInterface> VideoPresentationManagerProxy::returningToStandbyInterface() const
{
    if (m_contextMap.isEmpty())
        return nullptr;

    for (auto& value : copyToVector(m_contextMap.values())) {
        Ref interface = std::get<1>(value);
        if (interface->returningToStandby())
            return interface;
    }
    return nullptr;
}

RetainPtr<WKVideoView> VideoPresentationManagerProxy::createViewWithID(PlaybackSessionContextIdentifier contextId, WebKit::LayerHostingContextID videoLayerID, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& nativeSize, float hostingDeviceScaleFactor)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    addClientForContext(contextId);

    RetainPtr<WKLayerHostView> view = createLayerHostViewWithID(contextId, videoLayerID, initialSize, hostingDeviceScaleFactor);

    if (!model->videoView()) {
        ALWAYS_LOG(LOGIDENTIFIER, model->logIdentifier(), ", Creating AVPlayerLayerView");
        auto initialFrame = CGRectMake(0, 0, initialSize.width(), initialSize.height());
        auto playerView = adoptNS([allocWebAVPlayerLayerViewInstance() initWithFrame:initialFrame]);

        model->setVideoDimensions(nativeSize);

        RetainPtr playerLayer { (WebAVPlayerLayer *)[playerView layer] };
        [playerLayer setVideoDimensions:nativeSize];
        [playerLayer setPresentationModel:model.ptr()];
        [playerLayer setVideoSublayer:[view layer]];

        [playerView addSubview:view.get()];
        [playerView setUserInteractionEnabled:NO];

        // The videoView may already be reparented in fullscreen, so only parent the view
        // if it has no existing parent:
        if (![[view layer] superlayer])
            [playerLayer addSublayer:[view layer]];

        auto videoView = adoptNS([[WKVideoView alloc] initWithFrame:initialFrame playerView:playerView.get()]);

        model->setPlayerLayer(WTFMove(playerLayer));
        model->setPlayerView(playerView.get());
        model->setVideoView(videoView.get());
    }

    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::EnsureUpdatedVideoDimensions(contextId, nativeSize), page->webPageIDInMainFrameProcess());

    return model->videoView();
}
#endif

void VideoPresentationManagerProxy::willRemoveLayerForID(PlaybackSessionContextIdentifier contextId)
{
    removeClientForContext(contextId);
}

std::optional<SharedPreferencesForWebProcess> VideoPresentationManagerProxy::sharedPreferencesForWebProcess() const
{
    if (!m_page)
        return std::nullopt;

    // FIXME: Remove SUPPRESS_UNCOUNTED_ARG once https://github.com/llvm/llvm-project/pull/111198 lands.
    SUPPRESS_UNCOUNTED_ARG return m_page->legacyMainFrameProcess().sharedPreferencesForWebProcess();
}

#pragma mark Messages from VideoPresentationManager

void VideoPresentationManagerProxy::setupFullscreenWithID(PlaybackSessionContextIdentifier contextId, WebKit::LayerHostingContextID videoLayerID, const WebCore::FloatRect& screenRect, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& videoDimensions, float hostingDeviceScaleFactor, HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode, bool allowsPictureInPicture, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto [model, interface] = ensureModelAndInterface(contextId);

    // Do not add another refcount for this contextId if the interface is already in
    // a fullscreen mode, lest the refcounts get out of sync, as removeClientForContext
    // is only called once both PiP and video fullscreen are fully exited.
    if (interface->mode() == HTMLMediaElementEnums::VideoFullscreenModeNone || interface->mode() == HTMLMediaElementEnums::VideoFullscreenModeInWindow)
        addClientForContext(contextId);

    if (m_mockVideoPresentationModeEnabled) {
        if (!videoDimensions.isEmpty())
            m_mockPictureInPictureWindowSize.setHeight(DefaultMockPictureInPictureWindowWidth / videoDimensions.aspectRatio());
#if PLATFORM(IOS_FAMILY)
        requestVideoContentLayer(contextId);
#else
        didSetupFullscreen(contextId);
#endif
        return;
    }

#if PLATFORM(IOS_FAMILY)
    MESSAGE_CHECK((videoFullscreenMode | HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask) == HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask);
#else
    MESSAGE_CHECK(videoFullscreenMode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture); // setupFullscreen() ASSERTs this so catch it here while we can still fail the message
#endif

#if PLATFORM(IOS_FAMILY)
    // The video may not have been rendered yet, which would have triggered a call to createViewWithID/createLayerHostViewWithID making the AVPlayerLayer and AVPlayerLayerView not yet set. Create them as needed.
    if (!model->videoView())
        createViewWithID(contextId, videoLayerID, initialSize, videoDimensions, hostingDeviceScaleFactor);
    ASSERT(model->videoView());
#endif

    RetainPtr view = model->layerHostView() ? static_cast<WKLayerHostView*>(model->layerHostView()) : createLayerHostViewWithID(contextId, videoLayerID, initialSize, hostingDeviceScaleFactor);
#if USE(EXTENSIONKIT)
    RefPtr pageClient = page->pageClient();
    if (UIView *visibilityPropagationView = pageClient ? pageClient->createVisibilityPropagationView() : nullptr)
        [view setVisibilityPropagationView:visibilityPropagationView];
#else
    UNUSED_VARIABLE(view);
#endif

#if PLATFORM(IOS_FAMILY)
    auto* rootNode = downcast<RemoteLayerTreeDrawingAreaProxy>(*page->drawingArea()).remoteLayerTreeHost().rootNode();
    UIView *parentView = rootNode ? rootNode->uiView() : nil;
    interface->setupFullscreen(*model->layerHostView(), screenRect, videoDimensions, parentView, videoFullscreenMode, allowsPictureInPicture, standby, blocksReturnToFullscreenFromPictureInPicture);
#else
    UNUSED_PARAM(videoDimensions);
    UNUSED_PARAM(blocksReturnToFullscreenFromPictureInPicture);
    IntRect initialWindowRect;
    page->rootViewToWindow(enclosingIntRect(screenRect), initialWindowRect);
    interface->setupFullscreen(*model->layerHostView(), initialWindowRect, page->platformWindow(), videoFullscreenMode, allowsPictureInPicture);
#endif
}

void VideoPresentationManagerProxy::setPlayerIdentifier(PlaybackSessionContextIdentifier contextId, std::optional<MediaPlayerIdentifier> playerIdentifier)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    if (auto interface = findInterface(contextId))
        interface->setPlayerIdentifier(playerIdentifier);
}

void VideoPresentationManagerProxy::setHasVideo(PlaybackSessionContextIdentifier contextId, bool hasVideo)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    if (auto interface = findInterface(contextId))
        interface->hasVideoChanged(hasVideo);
}

void VideoPresentationManagerProxy::setVideoDimensions(PlaybackSessionContextIdentifier contextId, const FloatSize& videoDimensions)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    Ref { model }->setVideoDimensions(videoDimensions);

    if (m_mockVideoPresentationModeEnabled) {
        if (videoDimensions.isEmpty())
            return;

        m_mockPictureInPictureWindowSize.setHeight(DefaultMockPictureInPictureWindowWidth / videoDimensions.aspectRatio());
        return;
    }
}

void VideoPresentationManagerProxy::enterFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (m_mockVideoPresentationModeEnabled) {
        didEnterFullscreen(contextId, m_mockPictureInPictureWindowSize);
        return;
    }

    Ref interface = ensureInterface(contextId);
    interface->enterFullscreen();

    // Only one context can be in a given full screen mode at a time:
    for (auto& contextPair : m_contextMap) {
        auto& otherContextId = contextPair.key;
        if (contextId == otherContextId)
            continue;

        auto& otherInterface = std::get<1>(contextPair.value);
        if (otherInterface->hasMode(interface->mode()))
            otherInterface->requestHideAndExitFullscreen();
    }
}

void VideoPresentationManagerProxy::exitFullscreen(PlaybackSessionContextIdentifier contextId, WebCore::FloatRect finalRect, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler(false);
        return;
    }

    ASSERT(m_contextMap.contains(contextId));
    if (!m_contextMap.contains(contextId)) {
        completionHandler(false);
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    IntRect finalWindowRect;
    page->rootViewToWindow(enclosingIntRect(finalRect), finalWindowRect);
#else
    if (hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard))
        page->fullscreenMayReturnToInline();
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
    completionHandler(ensureInterface(contextId)->exitFullscreen(finalRect));
#else
    completionHandler(ensureInterface(contextId)->exitFullscreen(finalWindowRect, page->platformWindow()));
#endif
}

void VideoPresentationManagerProxy::exitFullscreenWithoutAnimationToMode(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
    MESSAGE_CHECK((targetMode | HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask) == HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask);

    if (m_mockVideoPresentationModeEnabled) {
        fullscreenModeChanged(contextId, targetMode);
        return;
    }

    ensureInterface(contextId)->exitFullscreenWithoutAnimationToMode(targetMode);

    hasVideoInPictureInPictureDidChange(targetMode & MediaPlayerEnums::VideoFullscreenModePictureInPicture);
}

void VideoPresentationManagerProxy::setVideoFullscreenMode(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    MESSAGE_CHECK((mode | HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask) == HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask);

    ensureInterface(contextId)->setMode(mode, false);
}

void VideoPresentationManagerProxy::clearVideoFullscreenMode(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    MESSAGE_CHECK((mode | HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask) == HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask);

#if PLATFORM(MAC)
    ensureInterface(contextId)->clearMode(mode);
#endif
}

#if PLATFORM(IOS_FAMILY)

void VideoPresentationManagerProxy::setInlineRect(PlaybackSessionContextIdentifier contextId, const WebCore::FloatRect& inlineRect, bool visible)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    ensureInterface(contextId)->setInlineRect(inlineRect, visible);
}

void VideoPresentationManagerProxy::setHasVideoContentLayer(PlaybackSessionContextIdentifier contextId, bool value)
{
    if (m_mockVideoPresentationModeEnabled) {
        if (value)
            didSetupFullscreen(contextId);
        else
            didExitFullscreen(contextId);

        return;
    }

    ensureInterface(contextId)->setHasVideoContentLayer(value);
}

#else

NO_RETURN_DUE_TO_ASSERT void VideoPresentationManagerProxy::setInlineRect(PlaybackSessionContextIdentifier, const WebCore::FloatRect&, bool)
{
    ASSERT_NOT_REACHED();
}

NO_RETURN_DUE_TO_ASSERT void VideoPresentationManagerProxy::setHasVideoContentLayer(PlaybackSessionContextIdentifier, bool)
{
    ASSERT_NOT_REACHED();
}

#endif

void VideoPresentationManagerProxy::cleanupFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (m_mockVideoPresentationModeEnabled) {
        didCleanupFullscreen(contextId);
        return;
    }

    ensureInterface(contextId)->cleanupFullscreen();
}

void VideoPresentationManagerProxy::preparedToReturnToInline(PlaybackSessionContextIdentifier contextId, bool visible, WebCore::FloatRect inlineRect)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->fullscreenMayReturnToInline();

#if !PLATFORM(IOS_FAMILY)
    IntRect inlineWindowRect;
    page->rootViewToWindow(enclosingIntRect(inlineRect), inlineWindowRect);
#endif

    if (m_mockVideoPresentationModeEnabled)
        return;

#if PLATFORM(IOS_FAMILY)
    ensureInterface(contextId)->preparedToReturnToInline(visible, inlineRect);
#else
    ensureInterface(contextId)->preparedToReturnToInline(visible, inlineWindowRect, page->platformWindow());
#endif
}

void VideoPresentationManagerProxy::preparedToExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    ensureInterface(contextId)->preparedToExitFullscreen();
}

void VideoPresentationManagerProxy::setRequiresTextTrackRepresentation(PlaybackSessionContextIdentifier contextId , bool requiresTextTrackRepresentation)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::SetRequiresTextTrackRepresentation(contextId, requiresTextTrackRepresentation), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::setTextTrackRepresentationBounds(PlaybackSessionContextIdentifier contextId , const IntRect& bounds)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::SetTextTrackRepresentationBounds(contextId, bounds), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::textTrackRepresentationUpdate(PlaybackSessionContextIdentifier contextId, ShareableBitmap::Handle&& textTrack)
{
    auto bitmap = ShareableBitmap::create(WTFMove(textTrack));
    if (!bitmap)
        return;
    
    auto platformImage = bitmap->createPlatformImage();
    ensureInterface(contextId)->setTrackRepresentationImage(platformImage);
}

void VideoPresentationManagerProxy::textTrackRepresentationSetContentsScale(PlaybackSessionContextIdentifier contextId, float scale)
{
    ensureInterface(contextId)->setTrackRepresentationContentsScale(scale);
}

void VideoPresentationManagerProxy::textTrackRepresentationSetHidden(PlaybackSessionContextIdentifier contextId, bool hidden)
{
    ensureInterface(contextId)->setTrackRepresentationHidden(hidden);
}

#pragma mark Messages to VideoPresentationManager

void VideoPresentationManagerProxy::callCloseCompletionHandlers()
{
    auto closeMediaCallbacks = WTFMove(m_closeCompletionHandlers);
    for (auto& callback : closeMediaCallbacks)
        callback();
}

void VideoPresentationManagerProxy::requestCloseAllMediaPresentations(PlaybackSessionContextIdentifier contextId, bool finishedWithMedia, CompletionHandler<void()>&& completionHandler)
{
    if (!hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        && !hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        completionHandler();
        return;
    }

    m_closeCompletionHandlers.append(WTFMove(completionHandler));
    requestFullscreenMode(contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone, finishedWithMedia);
}

void VideoPresentationManagerProxy::requestFullscreenMode(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::RequestFullscreenMode(contextId, mode, finishedWithMedia), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::requestUpdateInlineRect(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::RequestUpdateInlineRect(contextId), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::requestVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::RequestVideoContentLayer(contextId), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::returnVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::ReturnVideoContentLayer(contextId), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::returnVideoView(PlaybackSessionContextIdentifier contextId)
{
#if PLATFORM(IOS_FAMILY)
    Ref model = ensureModel(contextId);
    auto *playerView = model->playerView();
    auto *videoView = model->layerHostView();
    if (playerView && videoView) {
        [playerView addSubview:videoView];
        [playerView setNeedsLayout];
        [playerView layoutIfNeeded];
    }
#else
    UNUSED_PARAM(contextId);
#endif
}

void VideoPresentationManagerProxy::didSetupFullscreen(PlaybackSessionContextIdentifier contextId)
{
#if PLATFORM(IOS_FAMILY)
    enterFullscreen(contextId);
#else
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::DidSetupFullscreen(contextId), page->webPageIDInMainFrameProcess());
#endif
}

void VideoPresentationManagerProxy::willExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::WillExitFullscreen(contextId), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::didExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::DidExitFullscreen(contextId), page->webPageIDInMainFrameProcess());

#if PLATFORM(IOS_FAMILY)
    if (ensureInterface(contextId)->changingStandbyOnly()) {
        callCloseCompletionHandlers();
        return;
    }
#endif
    page->didExitFullscreen(contextId);
    callCloseCompletionHandlers();
}

void VideoPresentationManagerProxy::didEnterFullscreen(PlaybackSessionContextIdentifier contextId, const WebCore::FloatSize& size)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    std::optional<FloatSize> optionalSize;
    if (!size.isEmpty())
        optionalSize = size;

    page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::DidEnterFullscreen(contextId, optionalSize), page->webPageIDInMainFrameProcess());

#if PLATFORM(IOS_FAMILY)
    if (ensureInterface(contextId)->changingStandbyOnly())
        return;
#endif
    page->didEnterFullscreen(contextId);
}

void VideoPresentationManagerProxy::failedToEnterFullscreen(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::FailedToEnterFullscreen(contextId), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::didCleanupFullscreen(PlaybackSessionContextIdentifier contextId)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto [model, interface] = ensureModelAndInterface(contextId);

#if USE(EXTENSIONKIT)
    if (auto layerHostView = dynamic_objc_cast<WKLayerHostView>(model->layerHostView()))
        [layerHostView setVisibilityPropagationView:nil];
#endif

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

    page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::DidCleanupFullscreen(contextId), page->webPageIDInMainFrameProcess());

    if (!hasMode(HTMLMediaElementEnums::VideoFullscreenModeInWindow)) {
        interface->setMode(HTMLMediaElementEnums::VideoFullscreenModeNone, false);
        removeClientForContext(contextId);
    }

    page->didCleanupFullscreen(contextId);
}

void VideoPresentationManagerProxy::setVideoLayerFrame(PlaybackSessionContextIdentifier contextId, WebCore::FloatRect frame)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto [model, interface] = ensureModelAndInterface(contextId);
    interface->setCaptionsFrame(CGRectMake(0, 0, frame.width(), frame.height()));
    MachSendRight fenceSendRight;
#if PLATFORM(IOS_FAMILY)
#if USE(EXTENSIONKIT)
    auto view = dynamic_objc_cast<WKLayerHostView>(model->layerHostView());
    if (view && view->_hostingView) {
        auto hostingUpdateCoordinator = [BELayerHierarchyHostingTransactionCoordinator coordinatorWithError:nil];
        [hostingUpdateCoordinator addLayerHierarchyHostingView:view->_hostingView.get()];
        OSObjectPtr<xpc_object_t> xpcRepresentationHostingCoordinator = [hostingUpdateCoordinator createXPCRepresentation];
        fenceSendRight = MachSendRight::adopt(xpc_dictionary_copy_mach_send(xpcRepresentationHostingCoordinator.get(), machPortKey));
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::SetVideoLayerFrameFenced(contextId, frame, WTFMove(fenceSendRight)), page->webPageIDInMainFrameProcess());
        [hostingUpdateCoordinator commit];
        return;
    }
#else
    fenceSendRight = MachSendRight::adopt([UIWindow _synchronizeDrawingAcrossProcesses]);
#endif // USE(EXTENSIONKIT)
#else
    if (DrawingAreaProxy* drawingArea = page->drawingArea())
        fenceSendRight = drawingArea->createFence();
#endif

    page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::SetVideoLayerFrameFenced(contextId, frame, WTFMove(fenceSendRight)), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::setVideoLayerGravity(PlaybackSessionContextIdentifier contextId, WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    // FIXME: gravity should be sent as an enum instead of an unsigned.
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::SetVideoLayerGravityEnum(contextId, (unsigned)gravity), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::setVideoFullscreenFrame(PlaybackSessionContextIdentifier contextId, WebCore::FloatRect frame)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::SetVideoFullscreenFrame(contextId, frame), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::fullscreenModeChanged(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::FullscreenModeChanged(contextId, mode), page->webPageIDInMainFrameProcess());
}

void VideoPresentationManagerProxy::fullscreenMayReturnToInline(PlaybackSessionContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::VideoPresentationManager::FullscreenMayReturnToInline(contextId, page->isViewVisible()), page->webPageIDInMainFrameProcess());
}

#endif

#if PLATFORM(IOS_FAMILY)

AVPlayerViewController *VideoPresentationManagerProxy::playerViewController(PlaybackSessionContextIdentifier identifier) const
{
    if (RefPtr interface = findInterface(identifier))
        return interface->avPlayerViewController();
    return nil;
}

#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(LINEAR_MEDIA_PLAYER)
LMPlayableViewController *VideoPresentationManagerProxy::playableViewController(PlaybackSessionContextIdentifier identifier) const
{
    if (RefPtr interface = findInterface(identifier))
        return interface->playableViewController();
    return nil;
}
#endif

#if !RELEASE_LOG_DISABLED
const Logger& VideoPresentationManagerProxy::logger() const
{
    return m_playbackSessionManagerProxy->logger();
}

uint64_t VideoPresentationManagerProxy::logIdentifier() const
{
    return m_playbackSessionManagerProxy->logIdentifier();
}

ASCIILiteral VideoPresentationManagerProxy::logClassName() const
{
    return m_playbackSessionManagerProxy->logClassName();
}

WTFLogChannel& VideoPresentationManagerProxy::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

Ref<PlaybackSessionManagerProxy> VideoPresentationManagerProxy::protectedPlaybackSessionManagerProxy() const
{
    return m_playbackSessionManagerProxy;
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
