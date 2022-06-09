/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#import "WKContentViewInteraction.h"

#if PLATFORM(IOS_FAMILY)

#import "APIPageConfiguration.h"
#import "AccessibilityIOS.h"
#import "FullscreenClient.h"
#import "GPUProcessProxy.h"
#import "InputViewUpdateDeferrer.h"
#import "Logging.h"
#import "PageClientImplIOS.h"
#import "PrintInfo.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "SmartMagnificationController.h"
#import "UIKitSPI.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKInspectorHighlightView.h"
#import "WKPreferencesInternal.h"
#import "WKProcessGroupPrivate.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewConfiguration.h"
#import "WKWebViewIOS.h"
#import "WebFrameProxy.h"
#import "WebKit2Initialize.h"
#import "WebPageGroup.h"
#import "WebPageMessages.h"
#import "WebPageProxyMessages.h"
#import "WebProcessPool.h"
#import "_WKFrameHandleInternal.h"
#import "_WKWebViewPrintFormatterInternal.h"
#import <CoreGraphics/CoreGraphics.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FrameView.h>
#import <WebCore/InspectorOverlay.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/Quirks.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/VelocityData.h>
#import <objc/message.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/text/TextStream.h>
#import "AppKitSoftLink.h"


@interface WKInspectorIndicationView : UIView
@end

@implementation WKInspectorIndicationView

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;
    self.userInteractionEnabled = NO;
    self.backgroundColor = [UIColor colorWithRed:(111.0 / 255.0) green:(168.0 / 255.0) blue:(220.0 / 255.0) alpha:0.66f];
    return self;
}

@end

@interface WKQuirkyNSUndoManager : NSUndoManager
@property (readonly, weak) WKContentView *contentView;
@end

@implementation WKQuirkyNSUndoManager
- (instancetype)initWithContentView:(WKContentView *)contentView
{
    if (!(self = [super init]))
        return nil;
    _contentView = contentView;
    return self;
}

- (BOOL)canUndo 
{
    return YES;
}

- (BOOL)canRedo 
{
    return YES;
}

- (void)undo 
{
    [self.contentView generateSyntheticEditingCommand:WebKit::SyntheticEditingCommandType::Undo];
}

- (void)redo 
{
    [self.contentView generateSyntheticEditingCommand:WebKit::SyntheticEditingCommandType::Redo];
}

@end

@implementation WKContentView {
    std::unique_ptr<WebKit::PageClientImpl> _pageClient;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<WKBrowsingContextController> _browsingContextController;
    ALLOW_DEPRECATED_DECLARATIONS_END

    RetainPtr<UIView> _rootContentView;
    RetainPtr<UIView> _fixedClippingView;
    RetainPtr<WKInspectorIndicationView> _inspectorIndicationView;
    RetainPtr<WKInspectorHighlightView> _inspectorHighlightView;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    RetainPtr<_UILayerHostView> _visibilityPropagationViewForWebProcess;
#if ENABLE(GPU_PROCESS)
    RetainPtr<_UILayerHostView> _visibilityPropagationViewForGPUProcess;
#endif // ENABLE(GPU_PROCESS)
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

    WebCore::HistoricalVelocityData _historicalKinematicData;

    RetainPtr<NSUndoManager> _undoManager;
    RetainPtr<WKQuirkyNSUndoManager> _quirkyUndoManager;

    uint64_t _pdfPrintCallbackID;
    RetainPtr<CGPDFDocumentRef> _printedDocument;
    Vector<RetainPtr<NSURL>> _temporaryURLsToDeleteWhenDeallocated;
}

// Evernote expects to swizzle -keyCommands on WKContentView or they crash. Remove this hack
// as soon as reasonably possible. See <rdar://problem/51759247>.
static NSArray *keyCommandsPlaceholderHackForEvernote(id self, SEL _cmd)
{
    struct objc_super super = { self, class_getSuperclass(object_getClass(self)) };
    using SuperKeyCommandsFunction = NSArray *(*)(struct objc_super*, SEL);
    return reinterpret_cast<SuperKeyCommandsFunction>(&objc_msgSendSuper)(&super, @selector(keyCommands));
}

- (instancetype)_commonInitializationWithProcessPool:(WebKit::WebProcessPool&)processPool configuration:(Ref<API::PageConfiguration>&&)configuration
{
    ASSERT(_pageClient);

    _page = processPool.createWebPage(*_pageClient, WTFMove(configuration));
    _page->initializeWebPage();
    _page->setIntrinsicDeviceScaleFactor(WebCore::screenScaleFactor([UIScreen mainScreen]));
    _page->setUseFixedLayout(true);
    _page->setDelegatesScrolling(true);
    _page->setScreenIsBeingCaptured([[[self window] screen] isCaptured]);

    // In order to ensure that we get a unique DisplayRefreshMonitor per-DrawingArea (necessary because DisplayRefreshMonitor
    // is driven by this class), give each page a unique DisplayID derived from WebPage's unique ID.
    // FIXME: While using the high end of the range of DisplayIDs makes a collision with real, non-RemoteLayerTreeDrawingArea
    // DisplayIDs less likely, it is not entirely safe to have a RemoteLayerTreeDrawingArea and TiledCoreAnimationDrawingArea
    // coeexist in the same process.
    _page->windowScreenDidChange(std::numeric_limits<uint32_t>::max() - _page->webPageID().toUInt64(), std::nullopt);

#if ENABLE(FULLSCREEN_API)
    _page->setFullscreenClient(makeUnique<WebKit::FullscreenClient>(self.webView));
#endif

    WebKit::WebProcessPool::statistics().wkViewCount++;

    _rootContentView = adoptNS([[UIView alloc] init]);
    [_rootContentView layer].name = @"RootContent";
    [_rootContentView layer].masksToBounds = NO;
    [_rootContentView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

    _fixedClippingView = adoptNS([[UIView alloc] init]);
    [_fixedClippingView layer].name = @"FixedClipping";
    [_fixedClippingView layer].masksToBounds = YES;
    [_fixedClippingView layer].anchorPoint = CGPointZero;

    [self addSubview:_fixedClippingView.get()];
    [_fixedClippingView addSubview:_rootContentView.get()];

    if (!linkedOnOrAfter(SDKVersion::FirstWithLazyGestureRecognizerInstallation))
        [self setUpInteraction];
    [self setUserInteractionEnabled:YES];

    self.layer.hitTestsAsOpaque = YES;

#if HAVE(UI_FOCUS_EFFECT)
    if ([self respondsToSelector:@selector(setFocusEffect:)])
        self.focusEffect = nil;
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    [self _setupVisibilityPropagationViewForWebProcess];
#if ENABLE(GPU_PROCESS)
    [self _setupVisibilityPropagationViewForGPUProcess];
#endif
#endif

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillResignActive:) name:UIApplicationWillResignActiveNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidEnterBackground:) name:UIApplicationDidEnterBackgroundNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillEnterForeground:) name:UIApplicationWillEnterForegroundNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_screenCapturedDidChange:) name:UIScreenCapturedDidChangeNotification object:[UIScreen mainScreen]];

    if (WebCore::IOSApplication::isEvernote() && !linkedOnOrAfter(SDKVersion::FirstWhereWKContentViewDoesNotOverrideKeyCommands))
        class_addMethod(self.class, @selector(keyCommands), reinterpret_cast<IMP>(&keyCommandsPlaceholderHackForEvernote), method_getTypeEncoding(class_getInstanceMethod(self.class, @selector(keyCommands))));

    return self;
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
- (void)_setupVisibilityPropagationViewForWebProcess
{
    auto processIdentifier = _page->process().processIdentifier();
    auto contextID = _page->contextIDForVisibilityPropagationInWebProcess();
    if (!processIdentifier || !contextID)
        return;

    ASSERT(!_visibilityPropagationViewForWebProcess);
    // Propagate the view's visibility state to the WebContent process so that it is marked as "Foreground Running" when necessary.
    _visibilityPropagationViewForWebProcess = adoptNS([[_UILayerHostView alloc] initWithFrame:CGRectZero pid:processIdentifier contextID:contextID]);
    RELEASE_LOG(Process, "Created visibility propagation view %p (contextID=%u) for WebContent process with PID=%d", _visibilityPropagationViewForWebProcess.get(), contextID, processIdentifier);
    [self addSubview:_visibilityPropagationViewForWebProcess.get()];
}

#if ENABLE(GPU_PROCESS)
- (void)_setupVisibilityPropagationViewForGPUProcess
{
    auto* gpuProcess = _page->process().processPool().gpuProcess();
    if (!gpuProcess)
        return;
    auto processIdentifier = gpuProcess->processIdentifier();
    auto contextID = _page->contextIDForVisibilityPropagationInGPUProcess();
    if (!processIdentifier || !contextID)
        return;

    if (_visibilityPropagationViewForGPUProcess)
        return;

    // Propagate the view's visibility state to the GPU process so that it is marked as "Foreground Running" when necessary.
    _visibilityPropagationViewForGPUProcess = adoptNS([[_UILayerHostView alloc] initWithFrame:CGRectZero pid:processIdentifier contextID:contextID]);
    RELEASE_LOG(Process, "Created visibility propagation view %p (contextID=%u) for GPU process with PID=%d", _visibilityPropagationViewForGPUProcess.get(), contextID, processIdentifier);
    [self addSubview:_visibilityPropagationViewForGPUProcess.get()];
}
#endif // ENABLE(GPU_PROCESS)

- (void)_removeVisibilityPropagationViewForWebProcess
{
    if (!_visibilityPropagationViewForWebProcess)
        return;

    RELEASE_LOG(Process, "Removing visibility propagation view %p", _visibilityPropagationViewForWebProcess.get());
    [_visibilityPropagationViewForWebProcess removeFromSuperview];
    _visibilityPropagationViewForWebProcess = nullptr;
}

- (void)_removeVisibilityPropagationViewForGPUProcess
{
    if (!_visibilityPropagationViewForGPUProcess)
        return;

    RELEASE_LOG(Process, "Removing visibility propagation view %p", _visibilityPropagationViewForGPUProcess.get());
    [_visibilityPropagationViewForGPUProcess removeFromSuperview];
    _visibilityPropagationViewForGPUProcess = nullptr;
}
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

- (instancetype)initWithFrame:(CGRect)frame processPool:(NakedRef<WebKit::WebProcessPool>)processPool configuration:(Ref<API::PageConfiguration>&&)configuration webView:(WKWebView *)webView
{
    if (!(self = [super initWithFrame:frame webView:webView]))
        return nil;

    WebKit::InitializeWebKit2();

    _pageClient = makeUnique<WebKit::PageClientImpl>(self, webView);
    _webView = webView;

    return [self _commonInitializationWithProcessPool:processPool configuration:WTFMove(configuration)];
}

- (void)dealloc
{
    [self cleanUpInteraction];

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    _page->close();

    WebKit::WebProcessPool::statistics().wkViewCount--;

    [self _removeTemporaryFilesIfNecessary];
    
    [super dealloc];
}

- (void)_removeTemporaryFilesIfNecessary
{
    if (_temporaryURLsToDeleteWhenDeallocated.isEmpty())
        return;
    
    auto deleteTemporaryFiles = makeBlockPtr([urls = std::exchange(_temporaryURLsToDeleteWhenDeallocated, { })] {
        ASSERT(!RunLoop::isMain());
        auto manager = adoptNS([[NSFileManager alloc] init]);
        auto coordinator = adoptNS([[NSFileCoordinator alloc] init]);
        for (auto& url : urls) {
            if (![manager fileExistsAtPath:[url path]])
                continue;
            NSError *error = nil;
            [coordinator coordinateWritingItemAtURL:url.get() options:NSFileCoordinatorWritingForDeleting error:&error byAccessor:^(NSURL *coordinatedURL) {
                NSError *error = nil;
                if (![manager removeItemAtURL:coordinatedURL error:&error] || error)
                    LOG_ERROR("WKContentViewInteraction failed to remove file at path %@ with error %@", coordinatedURL.path, error);
            }];
            if (error)
                LOG_ERROR("WKContentViewInteraction failed to coordinate removal of temporary file at path %@ with error %@", url.get(), error);
        }
    });

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), deleteTemporaryFiles.get());
}

- (void)_removeTemporaryDirectoriesWhenDeallocated:(Vector<RetainPtr<NSURL>>&&)urls
{
    _temporaryURLsToDeleteWhenDeallocated.appendVector(WTFMove(urls));
}

- (WebKit::WebPageProxy*)page
{
    return _page.get();
}

- (WKWebView *)webView
{
    return _webView.getAutoreleased();
}

- (UIView *)rootContentView
{
    return _rootContentView.get();
}

- (void)willMoveToWindow:(UIWindow *)newWindow
{
    [super willMoveToWindow:newWindow];

    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    UIWindow *window = self.window;

    if (window)
        [defaultCenter removeObserver:self name:UIWindowDidMoveToScreenNotification object:window];

    if (newWindow) {
        [defaultCenter addObserver:self selector:@selector(_windowDidMoveToScreenNotification:) name:UIWindowDidMoveToScreenNotification object:newWindow];

        [self _updateForScreen:newWindow.screen];
    }
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];

    if (self.window)
        [self setUpInteraction];
    else
        [self cleanUpInteractionPreviewContainers];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (WKBrowsingContextController *)browsingContextController
{
    if (!_browsingContextController)
        _browsingContextController = adoptNS([[WKBrowsingContextController alloc] _initWithPageRef:toAPI(_page.get())]);

    return _browsingContextController.get();
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (WKPageRef)_pageRef
{
    return toAPI(_page.get());
}

- (BOOL)isFocusingElement
{
    return [self isEditable];
}

- (void)_showInspectorHighlight:(const WebCore::InspectorOverlay::Highlight&)highlight
{
    if (!_inspectorHighlightView) {
        _inspectorHighlightView = adoptNS([[WKInspectorHighlightView alloc] initWithFrame:CGRectZero]);
        [self insertSubview:_inspectorHighlightView.get() aboveSubview:_rootContentView.get()];
    }
    [_inspectorHighlightView update:highlight scale:[self _contentZoomScale] frame:_page->unobscuredContentRect()];
}

- (void)_hideInspectorHighlight
{
    if (_inspectorHighlightView) {
        [_inspectorHighlightView removeFromSuperview];
        _inspectorHighlightView = nil;
    }
}

- (BOOL)isShowingInspectorIndication
{
    return !!_inspectorIndicationView;
}

- (void)setShowingInspectorIndication:(BOOL)show
{
    if (show) {
        if (!_inspectorIndicationView) {
            _inspectorIndicationView = adoptNS([[WKInspectorIndicationView alloc] initWithFrame:[self bounds]]);
            [_inspectorIndicationView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
            [self insertSubview:_inspectorIndicationView.get() aboveSubview:_rootContentView.get()];
        }
    } else {
        if (_inspectorIndicationView) {
            [_inspectorIndicationView removeFromSuperview];
            _inspectorIndicationView = nil;
        }
    }
}

- (void)updateFixedClippingView:(WebCore::FloatRect)fixedPositionRectForUI
{
    WebCore::FloatRect clippingBounds = [self bounds];
    clippingBounds.unite(fixedPositionRectForUI);

    [_fixedClippingView setCenter:clippingBounds.location()]; // Not really the center since we set an anchor point.
    [_fixedClippingView setBounds:clippingBounds];
}

- (void)_didExitStableState
{
    _needsDeferredEndScrollingSelectionUpdate = self.shouldHideSelectionWhenScrolling;
    if (!_needsDeferredEndScrollingSelectionUpdate)
        return;

    [_textInteractionAssistant deactivateSelection];
}

static WebCore::FloatBoxExtent floatBoxExtent(UIEdgeInsets insets)
{
    return WebCore::FloatBoxExtent(insets.top, insets.right, insets.bottom, insets.left);
}

- (CGRect)_computeUnobscuredContentRectRespectingInputViewBounds:(CGRect)unobscuredContentRect inputViewBounds:(CGRect)inputViewBounds
{
    // The input view bounds are in window coordinates, but the unobscured rect is in content coordinates. Account for this by converting input view bounds to content coordinates.
    CGRect inputViewBoundsInContentCoordinates = [self.window convertRect:inputViewBounds toView:self];
    if (CGRectGetHeight(inputViewBoundsInContentCoordinates))
        unobscuredContentRect.size.height = std::min<float>(CGRectGetHeight(unobscuredContentRect), CGRectGetMinY(inputViewBoundsInContentCoordinates) - CGRectGetMinY(unobscuredContentRect));
    return unobscuredContentRect;
}

- (void)didUpdateVisibleRect:(CGRect)visibleContentRect
    unobscuredRect:(CGRect)unobscuredContentRect
    contentInsets:(UIEdgeInsets)contentInsets
    unobscuredRectInScrollViewCoordinates:(CGRect)unobscuredRectInScrollViewCoordinates
    obscuredInsets:(UIEdgeInsets)obscuredInsets
    unobscuredSafeAreaInsets:(UIEdgeInsets)unobscuredSafeAreaInsets
    inputViewBounds:(CGRect)inputViewBounds
    scale:(CGFloat)zoomScale minimumScale:(CGFloat)minimumScale
    viewStability:(OptionSet<WebKit::ViewStabilityFlag>)viewStability
    enclosedInScrollableAncestorView:(BOOL)enclosedInScrollableAncestorView
    sendEvenIfUnchanged:(BOOL)sendEvenIfUnchanged
{
    auto drawingArea = _page->drawingArea();
    if (!drawingArea)
        return;

    MonotonicTime timestamp = MonotonicTime::now();
    WebCore::VelocityData velocityData;
    bool inStableState = viewStability.isEmpty();
    if (!inStableState)
        velocityData = _historicalKinematicData.velocityForNewData(visibleContentRect.origin, zoomScale, timestamp);
    else {
        _historicalKinematicData.clear();
        velocityData = { 0, 0, 0, timestamp };
    }

    CGRect unobscuredContentRectRespectingInputViewBounds = [self _computeUnobscuredContentRectRespectingInputViewBounds:unobscuredContentRect inputViewBounds:inputViewBounds];
    WebCore::FloatRect fixedPositionRectForLayout = _page->computeLayoutViewportRect(unobscuredContentRect, unobscuredContentRectRespectingInputViewBounds, _page->layoutViewportRect(), zoomScale, WebCore::FrameView::LayoutViewportConstraint::ConstrainedToDocumentRect);

    WebKit::VisibleContentRectUpdateInfo visibleContentRectUpdateInfo(
        visibleContentRect,
        unobscuredContentRect,
        floatBoxExtent(contentInsets),
        unobscuredRectInScrollViewCoordinates,
        unobscuredContentRectRespectingInputViewBounds,
        fixedPositionRectForLayout,
        floatBoxExtent(obscuredInsets),
        floatBoxExtent(unobscuredSafeAreaInsets),
        zoomScale,
        viewStability,
        _sizeChangedSinceLastVisibleContentRectUpdate,
        self.webView._allowsViewportShrinkToFit,
        enclosedInScrollableAncestorView,
        velocityData,
        downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*drawingArea).lastCommittedLayerTreeTransactionID());

    LOG_WITH_STREAM(VisibleRects, stream << "-[WKContentView didUpdateVisibleRect]" << visibleContentRectUpdateInfo.dump());

    bool wasStableState = _page->inStableState();

    _page->updateVisibleContentRects(visibleContentRectUpdateInfo, sendEvenIfUnchanged);

    auto layoutViewport = _page->unconstrainedLayoutViewportRect();
    _page->adjustLayersForLayoutViewport(layoutViewport);

    _sizeChangedSinceLastVisibleContentRectUpdate = NO;

    drawingArea->updateDebugIndicator();
    
    [self updateFixedClippingView:layoutViewport];

    if (wasStableState && !inStableState)
        [self _didExitStableState];
}

- (void)didFinishScrolling
{
    [self _didEndScrollingOrZooming];
}

- (void)didInterruptScrolling
{
    _historicalKinematicData.clear();
}

- (void)willStartZoomOrScroll
{
    [self _willStartScrollingOrZooming];
}

- (void)didZoomToScale:(CGFloat)scale
{
    [self _didEndScrollingOrZooming];
}

- (NSUndoManager *)undoManagerForWebView
{
    if (self.focusedElementInformation.shouldSynthesizeKeyEventsForEditing && self.hasHiddenContentEditable) {
        if (!_quirkyUndoManager)
            _quirkyUndoManager = adoptNS([[WKQuirkyNSUndoManager alloc] initWithContentView:self]);
        return _quirkyUndoManager.get();
    }
    if (!_undoManager)
        _undoManager = adoptNS([[NSUndoManager alloc] init]);
    return _undoManager.get();
}

- (UIInterfaceOrientation)interfaceOrientation
{
    return self.window.windowScene.interfaceOrientation;
}

- (BOOL)canBecomeFocused
{
    auto delegate = static_cast<id <WKUIDelegatePrivate>>(self.webView.UIDelegate);
    if ([delegate respondsToSelector:@selector(_webViewCanBecomeFocused:)])
        return [delegate _webViewCanBecomeFocused:self.webView];

    return [delegate respondsToSelector:@selector(_webView:takeFocus:)];
}

- (void)didUpdateFocusInContext:(UIFocusUpdateContext *)context withAnimationCoordinator:(UIFocusAnimationCoordinator *)coordinator
{
    if (context.nextFocusedView == self) {
        if (context.focusHeading & UIFocusHeadingNext)
            [self _becomeFirstResponderWithSelectionMovingForward:YES completionHandler:nil];
        else if (context.focusHeading & UIFocusHeadingPrevious)
            [self _becomeFirstResponderWithSelectionMovingForward:NO completionHandler:nil];
    }
}

#pragma mark Internal

- (void)_windowDidMoveToScreenNotification:(NSNotification *)notification
{
    ASSERT(notification.object == self.window);

    UIScreen *screen = notification.userInfo[UIWindowNewScreenUserInfoKey];
    [self _updateForScreen:screen];
}

- (void)_updateForScreen:(UIScreen *)screen
{
    ASSERT(screen);
    _page->setIntrinsicDeviceScaleFactor(WebCore::screenScaleFactor(screen));
    [self _accessibilityRegisterUIProcessTokens];
}

- (void)_setAccessibilityWebProcessToken:(NSData *)data
{
    // This means the web process has checked in and we should send information back to that process.
    [self _accessibilityRegisterUIProcessTokens];
}

static void storeAccessibilityRemoteConnectionInformation(id element, pid_t pid, mach_port_t sendPort, NSUUID *uuid)
{
    // The accessibility bundle needs to know the uuid, pid and mach_port that this object will refer to.
    objc_setAssociatedObject(element, (void*)[@"ax-uuid" hash], uuid, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(element, (void*)[@"ax-pid" hash], @(pid), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(element, (void*)[@"ax-machport" hash], @(sendPort), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}


- (void)_updateRemoteAccessibilityRegistration:(BOOL)registerProcess
{
#if PLATFORM(MACCATALYST)
    pid_t pid = 0;
    if (registerProcess)
        pid = _page->process().processIdentifier();
    else
        pid = [objc_getAssociatedObject(self, (void*)[@"ax-pid" hash]) intValue];
    if (!pid)
        return;

    if (registerProcess)
        [WebKit::getNSAccessibilityRemoteUIElementClass() registerRemoteUIProcessIdentifier:pid];
    else
        [WebKit::getNSAccessibilityRemoteUIElementClass() unregisterRemoteUIProcessIdentifier:pid];
#endif
}

- (void)_accessibilityRegisterUIProcessTokens
{
    auto uuid = [NSUUID UUID];
    NSData *remoteElementToken = WebKit::newAccessibilityRemoteToken(uuid);

    // Store information about the WebProcess that can later be retrieved by the iOS Accessibility runtime.
    if (_page->process().state() == WebKit::WebProcessProxy::State::Running) {
        [self _updateRemoteAccessibilityRegistration:YES];
        IPC::Connection* connection = _page->process().connection();
        storeAccessibilityRemoteConnectionInformation(self, _page->process().processIdentifier(), connection->identifier().port, uuid);

        IPC::DataReference elementToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteElementToken bytes]), [remoteElementToken length]);
        _page->registerUIProcessAccessibilityTokens(elementToken, elementToken);
    }
}

- (void)_webViewDestroyed
{
    _webView = nil;
}

#pragma mark PageClientImpl methods

- (std::unique_ptr<WebKit::DrawingAreaProxy>)_createDrawingAreaProxy:(WebKit::WebProcessProxy&)process
{
    return makeUnique<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page, process);
}

- (void)_processDidExit
{
    [self _updateRemoteAccessibilityRegistration:NO];
    [self cleanUpInteraction];

    [self setShowingInspectorIndication:NO];
    [self _hideInspectorHighlight];

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    [self _removeVisibilityPropagationViewForWebProcess];
#endif

    _pdfPrintCallbackID = 0;
}

#if ENABLE(GPU_PROCESS)
- (void)_gpuProcessDidExit
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    [self _removeVisibilityPropagationViewForGPUProcess];
#endif
}
#endif

- (void)_processWillSwap
{
    // FIXME: Should we do something differently?
    [self _processDidExit];
}

- (void)_didRelaunchProcess
{
    [self _accessibilityRegisterUIProcessTokens];
    [self setUpInteraction];
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    [self _setupVisibilityPropagationViewForWebProcess];
#if ENABLE(GPU_PROCESS)
    [self _setupVisibilityPropagationViewForGPUProcess];
#endif
#endif
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
- (void)_webProcessDidCreateContextForVisibilityPropagation
{
    [self _setupVisibilityPropagationViewForWebProcess];
}

- (void)_gpuProcessDidCreateContextForVisibilityPropagation
{
    [self _setupVisibilityPropagationViewForGPUProcess];
}
#endif

- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    CGSize contentsSize = layerTreeTransaction.contentsSize();
    CGPoint scrollOrigin = -layerTreeTransaction.scrollOrigin();
    CGRect contentBounds = { scrollOrigin, contentsSize };

    LOG_WITH_STREAM(VisibleRects, stream << "-[WKContentView _didCommitLayerTree:] transactionID " <<  layerTreeTransaction.transactionID() << " contentBounds " << WebCore::FloatRect(contentBounds));

    BOOL boundsChanged = !CGRectEqualToRect([self bounds], contentBounds);
    if (boundsChanged)
        [self setBounds:contentBounds];

    [_webView _didCommitLayerTree:layerTreeTransaction];

    if (_interactionViewsContainerView) {
        WebCore::FloatPoint scaledOrigin = layerTreeTransaction.scrollOrigin();
        float scale = self.webView.scrollView.zoomScale;
        scaledOrigin.scale(scale);
        [_interactionViewsContainerView setFrame:CGRectMake(scaledOrigin.x(), scaledOrigin.y(), 0, 0)];
    }
    
    if (boundsChanged) {
        // FIXME: factor computeLayoutViewportRect() into something that gives us this rect.
        WebCore::FloatRect fixedPositionRect = _page->computeLayoutViewportRect(_page->unobscuredContentRect(), _page->unobscuredContentRectRespectingInputViewBounds(), _page->layoutViewportRect(), self.webView.scrollView.zoomScale);
        [self updateFixedClippingView:fixedPositionRect];

        // We need to push the new content bounds to the webview to update fixed position rects.
        [_webView _scheduleVisibleContentRectUpdate];
    }
    
    // Updating the selection requires a full editor state. If the editor state is missing post layout
    // data then it means there is a layout pending and we're going to be called again after the layout
    // so we delay the selection update.
    if (!_page->editorState().isMissingPostLayoutData)
        [self _updateChangedSelection];
}

- (void)_layerTreeCommitComplete
{
    [_webView _layerTreeCommitComplete];
}

- (void)_setAcceleratedCompositingRootView:(UIView *)rootView
{
    for (UIView* subview in [_rootContentView subviews])
        [subview removeFromSuperview];

    [_rootContentView addSubview:rootView];
}

- (BOOL)_scrollToRect:(CGRect)targetRect withOrigin:(CGPoint)origin minimumScrollDistance:(CGFloat)minimumScrollDistance
{
    return [_webView _scrollToRect:targetRect origin:origin minimumScrollDistance:minimumScrollDistance];
}

- (void)_zoomToFocusRect:(CGRect)rectToFocus selectionRect:(CGRect)selectionRect fontSize:(float)fontSize minimumScale:(double)minimumScale maximumScale:(double)maximumScale allowScaling:(BOOL)allowScaling forceScroll:(BOOL)forceScroll
{
    [_webView _zoomToFocusRect:rectToFocus
                 selectionRect:selectionRect
                      fontSize:fontSize
                  minimumScale:minimumScale
                  maximumScale:maximumScale
              allowScaling:allowScaling
                   forceScroll:forceScroll];
}

- (BOOL)_zoomToRect:(CGRect)targetRect withOrigin:(CGPoint)origin fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale minimumScrollDistance:(CGFloat)minimumScrollDistance
{
    return [_webView _zoomToRect:targetRect withOrigin:origin fitEntireRect:fitEntireRect minimumScale:minimumScale maximumScale:maximumScale minimumScrollDistance:minimumScrollDistance];
}

- (void)_zoomOutWithOrigin:(CGPoint)origin
{
    return [_webView _zoomOutWithOrigin:origin animated:YES];
}

- (void)_zoomToInitialScaleWithOrigin:(CGPoint)origin
{
    return [_webView _zoomToInitialScaleWithOrigin:origin animated:YES];
}

- (double)_initialScaleFactor
{
    return [_webView _initialScaleFactor];
}

- (double)_contentZoomScale
{
    return [_webView _contentZoomScale];
}

- (double)_targetContentZoomScaleForRect:(const WebCore::FloatRect&)targetRect currentScale:(double)currentScale fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale
{
    return [_webView _targetContentZoomScaleForRect:targetRect currentScale:currentScale fitEntireRect:fitEntireRect minimumScale:minimumScale maximumScale:maximumScale];
}

- (void)_applicationWillResignActive:(NSNotification*)notification
{
    _page->applicationWillResignActive();
}

- (void)_applicationDidBecomeActive:(NSNotification*)notification
{
    _page->applicationDidBecomeActive();
}

- (void)_applicationDidEnterBackground:(NSNotification*)notification
{
    if (!self.window)
        _page->applicationDidEnterBackgroundForMedia();
}

- (void)_applicationWillEnterForeground:(NSNotification*)notification
{
    if (!self.window)
        _page->applicationWillEnterForegroundForMedia();
}

- (void)_screenCapturedDidChange:(NSNotification *)notification
{
    _page->setScreenIsBeingCaptured([[[self window] screen] isCaptured]);
}

@end

#pragma mark Printing

@interface WKContentView (_WKWebViewPrintFormatter) <_WKWebViewPrintProvider>
@end

@implementation WKContentView (_WKWebViewPrintFormatter)

- (NSUInteger)_wk_pageCountForPrintFormatter:(_WKWebViewPrintFormatter *)printFormatter
{
    if (_pdfPrintCallbackID)
        [self _waitForDrawToPDFCallback];

    WebCore::FrameIdentifier frameID;
    if (_WKFrameHandle *handle = printFormatter.frameToPrint)
        frameID = handle->_frameHandle->frameID();
    else if (auto mainFrame = _page->mainFrame())
        frameID = mainFrame->frameID();
    else
        return 0;

    // The first page can have a smaller content rect than subsequent pages if a top content inset
    // is specified. Since WebKit requires a uniform content rect for each page during layout, use
    // the intersection of the first and non-first page rects.
    // FIXME: Teach WebCore::PrintContext to accept an initial content offset when paginating.
    CGRect printingRect = CGRectIntersection([printFormatter _pageContentRect:YES], [printFormatter _pageContentRect:NO]);
    if (CGRectIsEmpty(printingRect))
        return 0;

    WebKit::PrintInfo printInfo;
    printInfo.pageSetupScaleFactor = 1;
    printInfo.snapshotFirstPage = printFormatter.snapshotFirstPage;

    // FIXME: Paginate when exporting PDFs taller than 200"
    if (printInfo.snapshotFirstPage) {
        static const CGFloat maximumPDFHeight = 200 * 72; // maximum PDF height for a single page is 200 inches
        CGSize contentSize = self.bounds.size;
        printingRect = (CGRect) { CGPointZero, { contentSize.width, std::min(contentSize.height, maximumPDFHeight) } };
        [printFormatter _setSnapshotPaperRect:printingRect];
    }
    printInfo.availablePaperWidth = CGRectGetWidth(printingRect);
    printInfo.availablePaperHeight = CGRectGetHeight(printingRect);

    auto retainedSelf = retainPtr(self);
    auto pair = _page->computePagesForPrintingAndDrawToPDF(frameID, printInfo, [retainedSelf](const IPC::SharedBufferCopy& pdfData) {
        retainedSelf->_pdfPrintCallbackID = 0;
        if (pdfData.isEmpty())
            return;

        auto data = pdfData.buffer()->createCFData();
        auto dataProvider = adoptCF(CGDataProviderCreateWithCFData(data.get()));
        retainedSelf->_printedDocument = adoptCF(CGPDFDocumentCreateWithProvider(dataProvider.get()));
    });
    _pdfPrintCallbackID = pair.second;
    return pair.first;
}

- (BOOL)_waitForDrawToPDFCallback
{
    ASSERT(_pdfPrintCallbackID);
    if (!_page->process().connection()->waitForAsyncCallbackAndDispatchImmediately<Messages::WebPage::DrawToPDFiOS>(std::exchange(_pdfPrintCallbackID, 0), Seconds::infinity()))
        return false;
    ASSERT(!_pdfPrintCallbackID);
    return true;
}

- (CGPDFDocumentRef)_wk_printedDocument
{
    if (_pdfPrintCallbackID) {
        if (![self _waitForDrawToPDFCallback])
            return nullptr;
    }

    return _printedDocument.get();
}

@end

#endif // PLATFORM(IOS_FAMILY)
