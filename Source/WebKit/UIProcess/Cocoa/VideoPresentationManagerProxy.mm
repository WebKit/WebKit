/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#import "FrameInfoData.h"
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
#import "WebFrameProxy.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/HTMLMediaElement.h>
#import <WebCore/MediaPlayerEnums.h>
#import <WebCore/NullVideoPresentationInterface.h>
#import <WebCore/PlaybackSessionInterfaceAVKit.h>
#import <WebCore/PlaybackSessionInterfaceAVKitLegacy.h>
#import <WebCore/PlaybackSessionInterfaceMac.h>
#import <WebCore/PlaybackSessionInterfaceTVOS.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/VideoPresentationInterfaceAVKit.h>
#import <WebCore/VideoPresentationInterfaceAVKitLegacy.h>
#import <WebCore/VideoPresentationInterfaceMac.h>
#import <WebCore/VideoPresentationInterfaceTVOS.h>
#import <WebCore/WebAVPlayerLayer.h>
#import <WebCore/WebAVPlayerLayerView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/LoggerHelper.h>
#import <wtf/MachSendRightAnnotated.h>
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

@interface WKLayerHostView : CocoaView
@property (nonatomic, assign) uint32_t contextID;
#if USE(EXTENSIONKIT)
@property (nonatomic, strong) CocoaView *visibilityPropagationView;
#endif
@end

@implementation WKLayerHostView {
#if PLATFORM(IOS_FAMILY)
    WeakObjCPtr<UIWindow> _window;
#endif
#if USE(EXTENSIONKIT)
    RetainPtr<CocoaView> _visibilityPropagationView;
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
- (CocoaView *)visibilityPropagationView
{
    return _visibilityPropagationView.get();
}

- (void)setVisibilityPropagationView:(CocoaView *)visibilityPropagationView
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

template <typename Message>
void VideoPresentationManagerProxy::sendToWebProcess(PlaybackSessionContextIdentifier contextId, Message&& message)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    RefPtr process = WebProcessProxy::processForIdentifier(contextId.processIdentifier());
    if (!process)
        return;
    process->send(std::forward<Message>(message), page->webPageIDInProcess(*process));
}

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

void VideoPresentationManagerProxy::setDocumentVisibility(PlaybackSessionContextIdentifier contextId, bool isDocumentVisible)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    if (RefPtr interface = findInterface(contextId)) {
        interface->documentVisibilityChanged(isDocumentVisible);
        Ref model = ensureModel(contextId);
        if (model->isChildOfElementFullscreen())
            videosInElementFullscreenChanged();
    }
}

void VideoPresentationManagerProxy::setIsChildOfElementFullscreen(PlaybackSessionContextIdentifier contextId, bool isChildOfElementFullscreen)
{
    Ref model = ensureModel(contextId);

    if (std::exchange(model->m_isChildOfElementFullscreen, isChildOfElementFullscreen) != isChildOfElementFullscreen)
        videosInElementFullscreenChanged();
}

void VideoPresentationManagerProxy::hasBeenInteractedWith(PlaybackSessionContextIdentifier contextId)
{
    Ref model = ensureModel(contextId);

    if (std::exchange(m_lastInteractedWithVideo, contextId) != contextId && model->isChildOfElementFullscreen())
        videosInElementFullscreenChanged();
}

void VideoPresentationManagerProxy::videosInElementFullscreenChanged()
{
    if (RefPtr page = m_page.get())
        page->videosInElementFullscreenChanged();
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

void VideoPresentationModelContext::audioSessionCategoryChanged(WebCore::AudioSessionCategory category, WebCore::AudioSessionMode mode, WebCore::RouteSharingPolicy policy)
{
    m_clients.forEach([&](auto& client) {
        client.audioSessionCategoryChanged(category, mode, policy);
    });
}

void VideoPresentationModelContext::routingContextUIDChanged(const String& routingContextUID)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, routingContextUID);
    m_clients.forEach([&](auto& client) {
        client.routingContextUIDChanged(routingContextUID);
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

void VideoPresentationModelContext::fullscreenModeChanged(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode, ShouldNotifyMediaElement shouldNotifyMediaElement)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, mode);
    if (RefPtr manager = m_manager.get(); manager && shouldNotifyMediaElement == ShouldNotifyMediaElement::Yes)
        manager->fullscreenModeChanged(m_contextId, mode);
    m_clients.forEach([&](auto& client) {
        client.fullscreenModeChanged(mode);
    });
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

#if ENABLE(LINEAR_MEDIA_PLAYER)
void VideoPresentationModelContext::didEnterExternalPlayback()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->didEnterExternalPlayback(m_contextId);
}

void VideoPresentationModelContext::didExitExternalPlayback()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr manager = m_manager.get())
        manager->didExitExternalPlayback(m_contextId);
}
#endif

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

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
void VideoPresentationModelContext::requestShowCaptionDisplaySettingsPreview()
{
    if (RefPtr manager = m_manager.get())
        manager->requestShowCaptionDisplaySettingsPreview(m_contextId);
}

void VideoPresentationModelContext::requestHideCaptionDisplaySettingsPreview()
{
    if (RefPtr manager = m_manager.get())
        manager->requestHideCaptionDisplaySettingsPreview(m_contextId);
}
#endif

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
    return Ref { m_playbackSessionModel }->loggerPtr();
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

    for (auto& [model, interface] : contextMap.values())
        invalidateInterface(interface);
}

void VideoPresentationManagerProxy::invalidateInterface(WebCore::PlatformVideoPresentationInterface& interface)
{
    interface.setVideoPresentationModel(nullptr);

    if (RetainPtr layerHostView = interface.layerHostView()) {
        [layerHostView removeFromSuperview];
        interface.setLayerHostView(nullptr);
    }

    if (RetainPtr playerLayer = interface.playerLayer()) {
        playerLayer.get().presentationModel = nil;
        interface.setPlayerLayer(nullptr);
    }

#if PLATFORM(IOS_FAMILY)
    if (auto *playerLayerView = interface.playerLayerView()) {
        [playerLayerView removeFromSuperview];
        interface.setPlayerLayerView(nullptr);
    }

    interface.setVideoView(nullptr);
#endif
    interface.invalidate();
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
    if (auto contextId = m_playbackSessionManagerProxy->controlsManagerContextId())
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
    RefPtr page = m_page.get();
    if (!page)
        return callback({ }, { });
    RefPtr process = WebProcessProxy::processForIdentifier(contextId.processIdentifier());
    if (!process)
        return callback({ }, { });
    process->sendWithAsyncReply(Messages::VideoPresentationManager::RequestRouteSharingPolicyAndContextUID(contextId.object()), WTFMove(callback), page->webPageIDInProcess(*process));
}

static Ref<PlatformVideoPresentationInterface> videoPresentationInterface(WebPageProxy& page, PlatformPlaybackSessionInterface& playbackSessionInterface)
{
#if HAVE(AVKIT_CONTENT_SOURCE)
    if (page.preferences().isAVKitContentSourceEnabled())
        return VideoPresentationInterfaceAVKit::create(playbackSessionInterface);
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (page.preferences().linearMediaPlayerEnabled())
        return VideoPresentationInterfaceLMK::create(playbackSessionInterface);
#endif

#if PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(VISION)
    return VideoPresentationInterfaceAVKitLegacy::create(playbackSessionInterface);
#elif PLATFORM(APPLETV)
    return VideoPresentationInterfaceTVOS::create(playbackSessionInterface);
#else
    return PlatformVideoPresentationInterface::create(playbackSessionInterface);
#endif
}

VideoPresentationManagerProxy::ModelInterfacePair VideoPresentationManagerProxy::createModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    Ref page = *m_page;
    Ref playbackSessionManagerProxy = m_playbackSessionManagerProxy;
    Ref playbackSessionModel = playbackSessionManagerProxy->ensureModel(contextId);
    Ref model = VideoPresentationModelContext::create(*this, playbackSessionModel, contextId);
    Ref playbackSessionInterface = playbackSessionManagerProxy->ensureInterface(contextId);
    Ref interface = videoPresentationInterface(page.get(), playbackSessionInterface.get());

#if HAVE(PIP_SKIP_PREROLL)
    interface->setPlaybackStateEnabled(page->preferences().pictureInPicturePlaybackStateEnabled());
#endif

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    interface->setPrefersSpatialAudioExperience(page->preferences().preferSpatialAudioExperience());
#endif

    playbackSessionManagerProxy->addClientForContext(contextId);

    interface->setVideoPresentationModel(model.ptr());

    return std::make_pair(WTFMove(model), WTFMove(interface));
}

const VideoPresentationManagerProxy::ModelInterfacePair& VideoPresentationManagerProxy::ensureModelAndInterface(PlaybackSessionContextIdentifier contextId)
{
    auto addResult = m_contextMap.ensure(contextId, [&] {
        return createModelAndInterface(contextId);
    });
    return addResult.iterator->value;
}

Ref<VideoPresentationModelContext> VideoPresentationManagerProxy::ensureModel(PlaybackSessionContextIdentifier contextId)
{
    return ensureModelAndInterface(contextId).first;
}

Ref<PlatformVideoPresentationInterface> VideoPresentationManagerProxy::ensureInterface(PlaybackSessionContextIdentifier contextId)
{
    return ensureModelAndInterface(contextId).second;
}

const VideoPresentationManagerProxy::ModelInterfacePair* VideoPresentationManagerProxy::findModelAndInterface(PlaybackSessionContextIdentifier contextId) const
{
    auto it = m_contextMap.find(contextId);
    if (it == m_contextMap.end())
        return nullptr;
    return &(it->value);
}

RefPtr<PlatformVideoPresentationInterface> VideoPresentationManagerProxy::findInterface(PlaybackSessionContextIdentifier contextId) const
{
    if (auto* modelAndInterface = findModelAndInterface(contextId))
        return modelAndInterface->second.ptr();
    return nullptr;
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
        invalidateInterface(ensureInterface(contextId));
        m_playbackSessionManagerProxy->removeClientForContext(contextId);
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
    page->hasVideoInPictureInPictureDidChange(value);
    m_pipChangeObservers.forEach([value] (auto& observer) { observer(value); });
}

PlatformLayerContainer VideoPresentationManagerProxy::createLayerWithID(PlaybackSessionContextIdentifier contextId, const WebCore::HostingContext& hostingContext, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& nativeSize, float hostingDeviceScaleFactor)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    addClientForContext(contextId);

    if (model->videoDimensions().isEmpty() && !nativeSize.isEmpty())
        model->setVideoDimensions(nativeSize);

    RetainPtr<WKLayerHostView> view = createLayerHostViewWithID(contextId, hostingContext, initialSize, hostingDeviceScaleFactor);

    if (!interface->playerLayer()) {
        ALWAYS_LOG(LOGIDENTIFIER, model->logIdentifier(), ", Creating AVPlayerLayer, initialSize: ", initialSize, ", nativeSize: ", nativeSize);
        auto playerLayer = adoptNS([[WebAVPlayerLayer alloc] init]);
        RetainPtr viewLayer = [view layer];

        [playerLayer setPresentationModel:model.ptr()];
        [playerLayer setVideoSublayer:viewLayer.get()];

        // The videoView may already be reparented in fullscreen, so only parent the view
        // if it has no existing parent:
        if (![viewLayer superlayer])
            [playerLayer addSublayer:viewLayer.get()];

        interface->setPlayerLayer(playerLayer.get());

        [playerLayer setFrame:CGRectMake(0, 0, initialSize.width(), initialSize.height())];
        [playerLayer setNeedsLayout];
        [playerLayer layoutIfNeeded];
    }

    sendToWebProcess(contextId, Messages::VideoPresentationManager::EnsureUpdatedVideoDimensions(contextId.object(), nativeSize));

    return interface->playerLayer();
}

RetainPtr<WKLayerHostView> VideoPresentationManagerProxy::createLayerHostViewWithID(PlaybackSessionContextIdentifier contextId, const WebCore::HostingContext& hostingContext, const WebCore::FloatSize& initialSize, float hostingDeviceScaleFactor)
{
    auto [model, interface] = ensureModelAndInterface(contextId);

    RetainPtr<WKLayerHostView> view = static_cast<WKLayerHostView*>(interface->layerHostView());
    if (!view) {
        view = adoptNS([[WKLayerHostView alloc] init]);
#if PLATFORM(IOS_FAMILY)
        [view setUserInteractionEnabled:NO];
#endif
#if PLATFORM(MAC)
        [view setWantsLayer:YES];
#endif
        interface->setLayerHostView(view);

#if USE(EXTENSIONKIT)
        auto hostingView = adoptNS([[BELayerHierarchyHostingView alloc] init]);
        view->_hostingView = hostingView;
        [view addSubview:hostingView.get()];
        RetainPtr layer = [hostingView layer];
#else
        RetainPtr layer = [view layer];
#endif
        layer.get().masksToBounds = NO;
        layer.get().name = @"WKLayerHostView layer";
        layer.get().frame = CGRectMake(0, 0, initialSize.width(), initialSize.height());
    }

#if USE(EXTENSIONKIT)
    RetainPtr<BELayerHierarchyHandle> layerHandle;
#if ENABLE(MACH_PORT_LAYER_HOSTING)
    layerHandle = LayerHostingContext::createHostingHandle(WTF::MachSendRightAnnotated { hostingContext.sendRightAnnotated });
#else
    RefPtr page = m_page.get();
    if (RefPtr gpuProcess = page ? page->configuration().processPool().gpuProcess() : nullptr)
        layerHandle = LayerHostingContext::createHostingHandle(gpuProcess->processID(), hostingContext.contextID);
#endif
    if (layerHandle)
        [view->_hostingView setHandle:layerHandle.get()];
    else
        RELEASE_LOG_ERROR(Media, "VideoPresentationManagerProxy::createLayerHostViewWithID: could not create layer handle");
#else
    [view setContextID:hostingContext.contextID];
#endif

    interface->setupCaptionsLayer(retainPtr([view layer]).get(), initialSize);

    return view;
}

#if USE(EXTENSIONKIT)
void VideoPresentationManagerProxy::setVisibilityPropagationViewForLayerHostView(UIView *visibilityPropagationView, WKLayerHostView *layerHostView)
{
    if (RefPtr page = m_page.get()) {
        if (RefPtr pageClient = page->pageClient()) {
            RetainPtr oldVisibilityPropagationView = [layerHostView visibilityPropagationView];
            if (oldVisibilityPropagationView)
                pageClient->removeVisibilityPropagationView(oldVisibilityPropagationView.get());
        }
    }

    [layerHostView setVisibilityPropagationView:visibilityPropagationView];
}
#endif

#if PLATFORM(IOS_FAMILY)
RefPtr<PlatformVideoPresentationInterface> VideoPresentationManagerProxy::returningToStandbyInterface() const
{
    if (m_contextMap.isEmpty())
        return nullptr;

    for (auto& value : copyToVector(m_contextMap.values())) {
        Ref interface = WTFMove(value.second);
        if (interface->returningToStandby())
            return interface;
    }
    return nullptr;
}

RetainPtr<WKVideoView> VideoPresentationManagerProxy::createViewWithID(PlaybackSessionContextIdentifier contextId, const WebCore::HostingContext& hostingContext, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& nativeSize, float hostingDeviceScaleFactor)
{
    RELEASE_LOG(Media, "VideoPresentationManagerProxy::createViewWithID: context ID %d", hostingContext.contextID);

    auto [model, interface] = ensureModelAndInterface(contextId);
    addClientForContext(contextId);

    RetainPtr<WKLayerHostView> view = createLayerHostViewWithID(contextId, hostingContext, initialSize, hostingDeviceScaleFactor);

    if (!interface->videoView()) {
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

        interface->setPlayerLayer(WTFMove(playerLayer));
        interface->setPlayerLayerView(playerView.get());
        interface->setVideoView(videoView.get());
    }

    sendToWebProcess(contextId, Messages::VideoPresentationManager::EnsureUpdatedVideoDimensions(contextId.object(), nativeSize));

    return dynamic_objc_cast<WKVideoView>(interface->videoView());
}
#endif

void VideoPresentationManagerProxy::willRemoveLayerForID(PlaybackSessionContextIdentifier contextId)
{
    removeClientForContext(contextId);
}

std::optional<SharedPreferencesForWebProcess> VideoPresentationManagerProxy::sharedPreferencesForWebProcess(IPC::Connection& connection) const
{
    return WebProcessProxy::fromConnection(connection)->sharedPreferencesForWebProcess();
}

void VideoPresentationManagerProxy::swapFullscreenModes(PlaybackSessionContextIdentifier firstContextId, PlaybackSessionContextIdentifier secondContextId)
{
    auto firstInterface = findInterface(firstContextId);
    auto secondInterface = findInterface(secondContextId);
    if (!firstInterface || !secondInterface)
        return;

    auto firstFullscreenMode = firstInterface->mode();
    auto secondFullscreenMode = secondInterface->mode();

    firstInterface->swapFullscreenModesWith(*secondInterface);

    // Do not allow our client context count to get out of sync; this will cause
    // the interfaces to be torn down prematurely.
    auto firstIsInFullscreen = firstInterface->mode() & HTMLMediaElement::VideoFullscreenModeStandard;
    auto firstWasInFullscreen = firstFullscreenMode & HTMLMediaElement::VideoFullscreenModeStandard;

    if (firstIsInFullscreen != firstWasInFullscreen) {
        if (firstIsInFullscreen)
            addClientForContext(firstContextId);
        else
            removeClientForContext(firstContextId);
    }

    auto secondIsInFullscreen = secondInterface->mode() & HTMLMediaElement::VideoFullscreenModeStandard;
    auto secondWasInFullscreen = secondFullscreenMode & HTMLMediaElement::VideoFullscreenModeStandard;

    if (secondIsInFullscreen != secondWasInFullscreen) {
        if (secondIsInFullscreen)
            addClientForContext(secondContextId);
        else
            removeClientForContext(secondContextId);
    }
}

#pragma mark Messages from VideoPresentationManager

void VideoPresentationManagerProxy::setupFullscreenWithID(PlaybackSessionContextIdentifier contextId, const WebCore::HostingContext& hostingContext, const WebCore::FloatRect& screenRect, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& videoDimensions, float hostingDeviceScaleFactor, HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode, bool allowsPictureInPicture, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
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
    if (!interface->videoView())
        createViewWithID(contextId, hostingContext, initialSize, videoDimensions, hostingDeviceScaleFactor);
    ASSERT(interface->videoView());
#endif

    RetainPtr view = interface->layerHostView() ? static_cast<WKLayerHostView*>(interface->layerHostView()) : createLayerHostViewWithID(contextId, hostingContext, initialSize, hostingDeviceScaleFactor);
#if USE(EXTENSIONKIT)
    RefPtr pageClient = page->pageClient();
    if (RetainPtr visibilityPropagationView = pageClient ? pageClient->createVisibilityPropagationView() : nil)
        setVisibilityPropagationViewForLayerHostView(visibilityPropagationView.get(), view.get());
#else
    UNUSED_VARIABLE(view);
#endif

#if PLATFORM(IOS_FAMILY)
    auto* rootNode = downcast<RemoteLayerTreeDrawingAreaProxy>(*page->drawingArea()).remoteLayerTreeHost().rootNode();
    UIView *parentView = rootNode ? rootNode->uiView() : nil;
    interface->setupFullscreen(screenRect, videoDimensions, parentView, videoFullscreenMode, allowsPictureInPicture, standby, blocksReturnToFullscreenFromPictureInPicture);
#else
    UNUSED_PARAM(videoDimensions);
    UNUSED_PARAM(blocksReturnToFullscreenFromPictureInPicture);
    IntRect initialWindowRect;
    page->rootViewToWindow(enclosingIntRect(screenRect), initialWindowRect);
    interface->setupFullscreen(initialWindowRect, page->protectedPlatformWindow().get(), videoFullscreenMode, allowsPictureInPicture);
    interface->setupCaptionsLayer(retainPtr([view layer]).get(), initialSize);
#endif
}

void VideoPresentationManagerProxy::setPlayerIdentifier(PlaybackSessionContextIdentifier contextId, std::optional<MediaPlayerIdentifier> playerIdentifier)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    if (auto interface = findInterface(contextId))
        interface->setPlayerIdentifier(playerIdentifier);
}

void VideoPresentationManagerProxy::audioSessionCategoryChanged(PlaybackSessionContextIdentifier contextId, WebCore::AudioSessionCategory category, WebCore::AudioSessionMode mode, WebCore::RouteSharingPolicy policy)
{
    Ref { ensureModel(contextId) }->audioSessionCategoryChanged(category, mode, policy);
}

void VideoPresentationManagerProxy::routingContextUIDChanged(PlaybackSessionContextIdentifier contextId, const String& routingContextUID)
{
    Ref { ensureModel(contextId) }->routingContextUIDChanged(routingContextUID);
}

void VideoPresentationManagerProxy::setHasVideo(PlaybackSessionContextIdentifier contextId, bool hasVideo)
{
    if (m_mockVideoPresentationModeEnabled)
        return;

    if (auto* modelAndInterface = findModelAndInterface(contextId)) {
        modelAndInterface->first->m_hasVideo = hasVideo;
        Ref { modelAndInterface->second }->hasVideoChanged(hasVideo);
    }
}

void VideoPresentationManagerProxy::setVideoDimensions(PlaybackSessionContextIdentifier contextId, const FloatSize& videoDimensions)
{
    auto [model, interface] = ensureModelAndInterface(contextId);
    bool videosInElementFullscrenChanged = model->videoDimensions() != videoDimensions && model->isChildOfElementFullscreen();
    model->setVideoDimensions(videoDimensions);

    if (m_mockVideoPresentationModeEnabled) {
        if (videoDimensions.isEmpty())
            return;

        m_mockPictureInPictureWindowSize.setHeight(DefaultMockPictureInPictureWindowWidth / videoDimensions.aspectRatio());
        return;
    }
    if (videosInElementFullscrenChanged)
        this->videosInElementFullscreenChanged();
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

        auto& otherInterface = contextPair.value.second;
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
    completionHandler(ensureInterface(contextId)->exitFullscreen(finalWindowRect, page->protectedPlatformWindow().get()));
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

    ensureInterface(contextId)->setMode(mode, VideoPresentationModel::ShouldNotifyMediaElement::No);
}

void VideoPresentationManagerProxy::clearVideoFullscreenMode(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    MESSAGE_CHECK((mode | HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask) == HTMLMediaElementEnums::VideoFullscreenModeAllValidBitsMask);

#if PLATFORM(MAC)
    ensureInterface(contextId)->clearMode(mode, VideoPresentationModel::ShouldNotifyMediaElement::Yes);
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
    ensureInterface(contextId)->preparedToReturnToInline(visible, inlineWindowRect, page->protectedPlatformWindow().get());
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
    sendToWebProcess(contextId, Messages::VideoPresentationManager::SetRequiresTextTrackRepresentation(contextId.object(), requiresTextTrackRepresentation));
}

void VideoPresentationManagerProxy::setTextTrackRepresentationBounds(PlaybackSessionContextIdentifier contextId , const IntRect& bounds)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::SetTextTrackRepresentationBounds(contextId.object(), bounds));
}

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

void VideoPresentationManagerProxy::performCaptionDisplaySettingsAction(PlaybackSessionContextIdentifier contextId, Function<void(WebPageProxy&, const FrameInfoData&, WebCore::HTMLMediaElementIdentifier)>&& action)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr mainFrame = page->mainFrame();
    if (!mainFrame)
        return;

    WebCore::HTMLMediaElementIdentifier htmlMediaElementIdentifier { contextId.object() };

    mainFrame->getFrameInfo([protectedPage = RefPtr { page }, action = WTFMove(action), htmlMediaElementIdentifier](std::optional<FrameInfoData>&& frameInfo) {
        if (!frameInfo || !protectedPage)
            return;

        action(*protectedPage, *frameInfo, htmlMediaElementIdentifier);
    });
}

void VideoPresentationManagerProxy::requestShowCaptionDisplaySettingsPreview(PlaybackSessionContextIdentifier contextId)
{
    performCaptionDisplaySettingsAction(contextId, [](WebPageProxy& page, const FrameInfoData& frameInfo, WebCore::HTMLMediaElementIdentifier htmlMediaElementIdentifier) {
        page.showCaptionDisplaySettingsPreview(frameInfo, htmlMediaElementIdentifier);
    });
}

void VideoPresentationManagerProxy::requestHideCaptionDisplaySettingsPreview(PlaybackSessionContextIdentifier contextId)
{
    performCaptionDisplaySettingsAction(contextId, [](WebPageProxy& page, const FrameInfoData& frameInfo, WebCore::HTMLMediaElementIdentifier htmlMediaElementIdentifier) {
        page.hideCaptionDisplaySettingsPreview(frameInfo, htmlMediaElementIdentifier);
    });
}
#endif

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
    sendToWebProcess(contextId, Messages::VideoPresentationManager::RequestFullscreenMode(contextId.object(), mode, finishedWithMedia));
}

void VideoPresentationManagerProxy::requestUpdateInlineRect(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::RequestUpdateInlineRect(contextId.object()));
}

void VideoPresentationManagerProxy::requestVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::RequestVideoContentLayer(contextId.object()));
}

void VideoPresentationManagerProxy::returnVideoContentLayer(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::ReturnVideoContentLayer(contextId.object()));
}

void VideoPresentationManagerProxy::returnVideoView(PlaybackSessionContextIdentifier contextId)
{
#if PLATFORM(IOS_FAMILY)
    Ref interface = ensureInterface(contextId);
    auto *playerView = interface->playerLayerView();
    auto *videoView = interface->layerHostView();
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
    RefPtr page = m_page.get();
    if (page)
        page->willEnterFullscreen(contextId);

    enterFullscreen(contextId);
#else
    sendToWebProcess(contextId, Messages::VideoPresentationManager::DidSetupFullscreen(contextId.object()));
#endif
}

void VideoPresentationManagerProxy::willExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::WillExitFullscreen(contextId.object()));
}

void VideoPresentationManagerProxy::didExitFullscreen(PlaybackSessionContextIdentifier contextId)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    sendToWebProcess(contextId, Messages::VideoPresentationManager::DidExitFullscreen(contextId.object()));

#if PLATFORM(IOS_FAMILY)
    if (ensureInterface(contextId)->changingStandbyOnly())
        page->didExitStandby(contextId);
    else
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    page->didExitFullscreen(contextId);
#endif

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

    sendToWebProcess(contextId, Messages::VideoPresentationManager::DidEnterFullscreen(contextId.object(), optionalSize));

#if PLATFORM(IOS_FAMILY)
    if (ensureInterface(contextId)->changingStandbyOnly())
        page->didEnterStandby(contextId);
    else
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    page->didEnterFullscreen(contextId);
#endif
}

void VideoPresentationManagerProxy::failedToEnterFullscreen(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::FailedToEnterFullscreen(contextId.object()));
}

void VideoPresentationManagerProxy::didCleanupFullscreen(PlaybackSessionContextIdentifier contextId)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto [model, interface] = ensureModelAndInterface(contextId);

#if USE(EXTENSIONKIT)
    if (RetainPtr layerHostView = dynamic_objc_cast<WKLayerHostView>(interface->layerHostView()))
        setVisibilityPropagationViewForLayerHostView(nil, layerHostView.get());
#endif

    [interface->protectedLayerHostView() removeFromSuperview];
    interface->removeCaptionsLayer();
    if (RetainPtr playerLayer = interface->playerLayer()) {
        // Return the video layer to the player layer
        RetainPtr videoView = interface->layerHostView();
        [playerLayer addSublayer:retainPtr([videoView layer]).get()];
        [playerLayer layoutSublayers];
    } else {
        [CATransaction flush];
        [interface->protectedLayerHostView() removeFromSuperview];
        interface->setLayerHostView(nullptr);
    }

    sendToWebProcess(contextId, Messages::VideoPresentationManager::DidCleanupFullscreen(contextId.object()));

    if (!hasMode(HTMLMediaElementEnums::VideoFullscreenModeInWindow)) {
        interface->setMode(HTMLMediaElementEnums::VideoFullscreenModeNone, VideoPresentationModel::ShouldNotifyMediaElement::No);
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
    WTF::MachSendRightAnnotated sendRightAnnotated;
#if PLATFORM(IOS_FAMILY)
#if USE(EXTENSIONKIT)
    auto view = dynamic_objc_cast<WKLayerHostView>(interface->layerHostView());
    if (view && view->_hostingView) {
        auto hostingUpdateCoordinator = [BELayerHierarchyHostingTransactionCoordinator coordinatorWithError:nil];
        [hostingUpdateCoordinator addLayerHierarchyHostingView:view->_hostingView.get()];
#if ENABLE(MACH_PORT_LAYER_HOSTING)
        sendRightAnnotated = LayerHostingContext::fence(hostingUpdateCoordinator);
#else
        XPCObjectPtr<xpc_object_t> xpcRepresentationHostingCoordinator = [hostingUpdateCoordinator createXPCRepresentation];
        sendRightAnnotated.sendRight = MachSendRight::adopt(xpc_dictionary_copy_mach_send(xpcRepresentationHostingCoordinator.get(), machPortKey));
#endif
        RELEASE_LOG(Media, "VideoPresentationManagerProxy::setVideoLayerFrame: x=%f y=%f w=%f h=%f send right %d, fence data size %lu", frame.x(), frame.y(), frame.width(), frame.height(), sendRightAnnotated.sendRight.sendRight(), sendRightAnnotated.data.size());
        sendToWebProcess(contextId, Messages::VideoPresentationManager::SetVideoLayerFrameFenced(contextId.object(), frame, WTFMove(sendRightAnnotated)));
        [hostingUpdateCoordinator commit];
        return;
    }
#else
    sendRightAnnotated.sendRight = MachSendRight::adopt([UIWindow _synchronizeDrawingAcrossProcesses]);
#endif // USE(EXTENSIONKIT)
#else
    if (RefPtr drawingArea = page->drawingArea())
        sendRightAnnotated.sendRight = drawingArea->createFence();
#endif

    sendToWebProcess(contextId, Messages::VideoPresentationManager::SetVideoLayerFrameFenced(contextId.object(), frame, WTFMove(sendRightAnnotated)));
}

void VideoPresentationManagerProxy::setVideoLayerGravity(PlaybackSessionContextIdentifier contextId, WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    // FIXME: gravity should be sent as an enum instead of an unsigned.
    sendToWebProcess(contextId, Messages::VideoPresentationManager::SetVideoLayerGravityEnum(contextId.object(), (unsigned)gravity));
}

void VideoPresentationManagerProxy::setVideoFullscreenFrame(PlaybackSessionContextIdentifier contextId, WebCore::FloatRect frame)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::SetVideoFullscreenFrame(contextId.object(), frame));
}

void VideoPresentationManagerProxy::fullscreenModeChanged(PlaybackSessionContextIdentifier contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::FullscreenModeChanged(contextId.object(), mode));
}

void VideoPresentationManagerProxy::fullscreenMayReturnToInline(PlaybackSessionContextIdentifier contextId)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    sendToWebProcess(contextId, Messages::VideoPresentationManager::FullscreenMayReturnToInline(contextId.object(), page->isViewVisible()));
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void VideoPresentationManagerProxy::didEnterExternalPlayback(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::DidEnterExternalPlayback(contextId.object()));
}

void VideoPresentationManagerProxy::didExitExternalPlayback(PlaybackSessionContextIdentifier contextId)
{
    sendToWebProcess(contextId, Messages::VideoPresentationManager::DidExitExternalPlayback(contextId.object()));
}
#endif

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
WKSPlayableViewControllerHost *VideoPresentationManagerProxy::playableViewController(PlaybackSessionContextIdentifier identifier) const
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

RefPtr<PlatformVideoPresentationInterface> VideoPresentationManagerProxy::bestVideoForElementFullscreen()
{
    if (m_lastInteractedWithVideo) {
        if (auto* modelAndInterface = findModelAndInterface(*m_lastInteractedWithVideo); modelAndInterface && modelAndInterface->first->isChildOfElementFullscreen())
            return Ref { modelAndInterface->second };
    }
#if PLATFORM(IOS_FAMILY)
    if (!m_page)
        return nullptr;

    RefPtr pageClient = m_page->pageClient();
    if (!pageClient)
        return nullptr;

    RetainPtr window = pageClient->platformWindow();
    if (!window)
        return nullptr;

    CGRect windowBounds = [window bounds];
    Vector<Ref<PlatformVideoPresentationInterface>> candidates;
    for (auto& modelInterface : m_contextMap.values()) {
        auto [model, interface] = modelInterface;
        if (!model->isChildOfElementFullscreen())
            continue;

        candidates.append(interface);
    }

    float largestArea = 0;
    auto unobscuredArea = FloatRect { windowBounds }.area();

    RefPtr<WebCore::PlatformVideoPresentationInterface> largestVisibleVideo;
    constexpr auto minimumViewportRatioForLargestMediaElement = 0.25;
    float minimumAreaForLargestElement = minimumViewportRatioForLargestMediaElement * unobscuredArea;
    for (auto& candidate : candidates) {
        RetainPtr candidateLayer = candidate->playerLayer();
        if (!candidateLayer)
            continue;
        auto candidateRect = [candidateLayer convertRect:[candidateLayer bounds] toLayer:nil];
        auto intersectionRect = intersection(windowBounds, candidateRect);
        if (intersectionRect.isEmpty())
            continue;

        auto area = intersectionRect.area();
        if (area <= largestArea)
            continue;

        if (area < minimumAreaForLargestElement)
            continue;

        largestArea = area;
        largestVisibleVideo = candidate.ptr();
    }

    return largestVisibleVideo;
#else
    return nullptr;
#endif
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
