/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "APIPageConfiguration.h"
#import "AccessibilityIOS.h"
#import "ApplicationStateTracker.h"
#import "PageClientImplIOS.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "SmartMagnificationController.h"
#import "UIKitSPI.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKInspectorHighlightView.h"
#import "WKPreferencesInternal.h"
#import "WKProcessGroupPrivate.h"
#import "WKWebViewConfiguration.h"
#import "WKWebViewInternal.h"
#import "WebFrameProxy.h"
#import "WebKit2Initialize.h"
#import "WebPageGroup.h"
#import "WebProcessPool.h"
#import "WebSystemInterface.h"
#import <CoreGraphics/CoreGraphics.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FrameView.h>
#import <WebCore/InspectorOverlay.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/QuartzCoreSPI.h>
#import <wtf/CurrentTime.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;
using namespace WebKit;

namespace WebKit {
class HistoricalVelocityData {
public:
    struct VelocityData {
        VelocityData()
            : horizontalVelocity(0)
            , verticalVelocity(0)
            , scaleChangeRate(0)
        {
        }

        VelocityData(double horizontalVelocity, double verticalVelocity, double scaleChangeRate)
            : horizontalVelocity(horizontalVelocity)
            , verticalVelocity(verticalVelocity)
            , scaleChangeRate(scaleChangeRate)
        {
        }

        double horizontalVelocity;
        double verticalVelocity;
        double scaleChangeRate;
    };

    HistoricalVelocityData()
        : m_historySize(0)
        , m_latestDataIndex(0)
        , m_lastAppendTimestamp(0)
    {
    }

    VelocityData velocityForNewData(CGPoint newPosition, double scale, double timestamp)
    {
        // Due to all the source of rect update, the input is very noisy. To smooth the output, we accumulate all changes
        // within 1 frame as a single update. No speed computation is ever done on data within the same frame.
        const double filteringThreshold = 1 / 60.;

        VelocityData velocityData;
        if (m_historySize > 0) {
            unsigned oldestDataIndex;
            unsigned distanceToLastHistoricalData = m_historySize - 1;
            if (distanceToLastHistoricalData <= m_latestDataIndex)
                oldestDataIndex = m_latestDataIndex - distanceToLastHistoricalData;
            else
                oldestDataIndex = m_historySize - (distanceToLastHistoricalData - m_latestDataIndex);

            double timeDelta = timestamp - m_history[oldestDataIndex].timestamp;
            if (timeDelta > filteringThreshold) {
                Data& oldestData = m_history[oldestDataIndex];
                velocityData = VelocityData((newPosition.x - oldestData.position.x) / timeDelta, (newPosition.y - oldestData.position.y) / timeDelta, (scale - oldestData.scale) / timeDelta);
            }
        }

        double timeSinceLastAppend = timestamp - m_lastAppendTimestamp;
        if (timeSinceLastAppend > filteringThreshold)
            append(newPosition, scale, timestamp);
        else
            m_history[m_latestDataIndex] = { timestamp, newPosition, scale };
        return velocityData;
    }

    void clear() { m_historySize = 0; }

private:
    void append(CGPoint newPosition, double scale, double timestamp)
    {
        m_latestDataIndex = (m_latestDataIndex + 1) % maxHistoryDepth;
        m_history[m_latestDataIndex] = { timestamp, newPosition, scale };

        unsigned size = m_historySize + 1;
        if (size <= maxHistoryDepth)
            m_historySize = size;

        m_lastAppendTimestamp = timestamp;
    }


    static const unsigned maxHistoryDepth = 3;

    unsigned m_historySize;
    unsigned m_latestDataIndex;
    double m_lastAppendTimestamp;

    struct Data {
        double timestamp;
        CGPoint position;
        double scale;
    } m_history[maxHistoryDepth];
};
} // namespace WebKit

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

@implementation WKContentView {
    std::unique_ptr<PageClientImpl> _pageClient;
    RetainPtr<WKBrowsingContextController> _browsingContextController;

    RetainPtr<UIView> _rootContentView;
    RetainPtr<UIView> _fixedClippingView;
    RetainPtr<WKInspectorIndicationView> _inspectorIndicationView;
    RetainPtr<WKInspectorHighlightView> _inspectorHighlightView;

    HistoricalVelocityData _historicalKinematicData;

    RetainPtr<NSUndoManager> _undoManager;

    std::unique_ptr<ApplicationStateTracker> _applicationStateTracker;
}

- (instancetype)_commonInitializationWithProcessPool:(WebKit::WebProcessPool&)processPool configuration:(Ref<API::PageConfiguration>&&)configuration
{
    ASSERT(_pageClient);

    _page = processPool.createWebPage(*_pageClient, WTFMove(configuration));
    _page->initializeWebPage();
    _page->setIntrinsicDeviceScaleFactor(screenScaleFactor([UIScreen mainScreen]));
    _page->setUseFixedLayout(true);
    _page->setDelegatesScrolling(true);

    WebProcessPool::statistics().wkViewCount++;

    _rootContentView = adoptNS([[UIView alloc] init]);
    [_rootContentView layer].masksToBounds = NO;
    [_rootContentView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

    _fixedClippingView = adoptNS([[UIView alloc] init]);
    [_fixedClippingView layer].masksToBounds = YES;
    [_fixedClippingView layer].anchorPoint = CGPointZero;
#ifndef NDEBUG
    [[_fixedClippingView layer] setName:@"Fixed clipping"];
#endif

    [self addSubview:_fixedClippingView.get()];
    [_fixedClippingView addSubview:_rootContentView.get()];

    [self setupInteraction];
    [self setUserInteractionEnabled:YES];

    self.layer.hitTestsAsOpaque = YES;

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillResignActive:) name:UIApplicationWillResignActiveNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:[UIApplication sharedApplication]];

    return self;
}

- (instancetype)initWithFrame:(CGRect)frame processPool:(WebKit::WebProcessPool&)processPool configuration:(Ref<API::PageConfiguration>&&)configuration webView:(WKWebView *)webView
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    InitializeWebKit2();

    _pageClient = std::make_unique<PageClientImpl>(self, webView);
    _webView = webView;

    return [self _commonInitializationWithProcessPool:processPool configuration:WTFMove(configuration)];
}

- (void)dealloc
{
    [self cleanupInteraction];

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    _page->close();

    WebProcessPool::statistics().wkViewCount--;

    [super dealloc];
}

- (WebPageProxy*)page
{
    return _page.get();
}

- (void)willMoveToWindow:(UIWindow *)newWindow
{
    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    UIWindow *window = self.window;

    if (window) {
        [defaultCenter removeObserver:self name:UIWindowDidMoveToScreenNotification object:window];

        if (!newWindow) {
            ASSERT(_applicationStateTracker);
            _applicationStateTracker = nullptr;
        }
    }

    if (newWindow) {
        [defaultCenter addObserver:self selector:@selector(_windowDidMoveToScreenNotification:) name:UIWindowDidMoveToScreenNotification object:newWindow];

        [self _updateForScreen:newWindow.screen];
    }
}

- (void)didMoveToWindow
{
    if (!self.window)
        return;

    ASSERT(!_applicationStateTracker);
    _applicationStateTracker = std::make_unique<ApplicationStateTracker>(self, @selector(_applicationDidEnterBackground), @selector(_applicationWillEnterForeground));
}

- (WKBrowsingContextController *)browsingContextController
{
    if (!_browsingContextController)
        _browsingContextController = adoptNS([[WKBrowsingContextController alloc] _initWithPageRef:toAPI(_page.get())]);

    return _browsingContextController.get();
}

- (WKPageRef)_pageRef
{
    return toAPI(_page.get());
}

- (BOOL)isAssistingNode
{
    return [self isEditable];
}

- (BOOL)isBackground
{
    if (!_applicationStateTracker)
        return YES;

    return _applicationStateTracker->isInBackground();
}

- (void)_showInspectorHighlight:(const WebCore::Highlight&)highlight
{
    if (!_inspectorHighlightView) {
        _inspectorHighlightView = adoptNS([[WKInspectorHighlightView alloc] initWithFrame:CGRectZero]);
        [self insertSubview:_inspectorHighlightView.get() aboveSubview:_rootContentView.get()];
    }

    [_inspectorHighlightView update:highlight];
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

- (void)updateFixedClippingView:(FloatRect)fixedPositionRectForUI
{
    FloatRect clippingBounds = [self bounds];
    clippingBounds.unite(fixedPositionRectForUI);

    [_fixedClippingView setCenter:clippingBounds.location()]; // Not really the center since we set an anchor point.
    [_fixedClippingView setBounds:clippingBounds];
}

- (void)didUpdateVisibleRect:(CGRect)visibleRect unobscuredRect:(CGRect)unobscuredRect unobscuredRectInScrollViewCoordinates:(CGRect)unobscuredRectInScrollViewCoordinates
    obscuredInset:(CGSize)obscuredInset scale:(CGFloat)zoomScale minimumScale:(CGFloat)minimumScale inStableState:(BOOL)isStableState isChangingObscuredInsetsInteractively:(BOOL)isChangingObscuredInsetsInteractively
{
    double timestamp = monotonicallyIncreasingTime();
    HistoricalVelocityData::VelocityData velocityData;
    if (!isStableState)
        velocityData = _historicalKinematicData.velocityForNewData(visibleRect.origin, zoomScale, timestamp);
    else
        _historicalKinematicData.clear();

    FloatRect fixedPositionRectForLayout = _page->computeCustomFixedPositionRect(unobscuredRect, zoomScale, WebPageProxy::UnobscuredRectConstraint::ConstrainedToDocumentRect);
    _page->updateVisibleContentRects(visibleRect, unobscuredRect, unobscuredRectInScrollViewCoordinates, fixedPositionRectForLayout, WebCore::FloatSize(obscuredInset),
        zoomScale, isStableState, isChangingObscuredInsetsInteractively, _webView._allowsViewportShrinkToFit, timestamp, velocityData.horizontalVelocity, velocityData.verticalVelocity, velocityData.scaleChangeRate);

    RemoteScrollingCoordinatorProxy* scrollingCoordinator = _page->scrollingCoordinatorProxy();
    FloatRect fixedPositionRect = _page->computeCustomFixedPositionRect(_page->unobscuredContentRect(), zoomScale);
    scrollingCoordinator->viewportChangedViaDelegatedScrolling(scrollingCoordinator->rootScrollingNodeID(), fixedPositionRect, zoomScale);

    if (auto drawingArea = _page->drawingArea())
        drawingArea->updateDebugIndicator();
        
    [self updateFixedClippingView:fixedPositionRect];
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

- (NSUndoManager *)undoManager
{
    if (!_undoManager)
        _undoManager = adoptNS([[NSUndoManager alloc] init]);

    return _undoManager.get();
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
    _page->setIntrinsicDeviceScaleFactor(screenScaleFactor(screen));
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

- (void)_accessibilityRegisterUIProcessTokens
{
    auto uuid = [NSUUID UUID];
    NSData *remoteElementToken = newAccessibilityRemoteToken(uuid);

    // Store information about the WebProcess that can later be retrieved by the iOS Accessibility runtime.
    if (_page->process().state() == WebProcessProxy::State::Running) {
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

- (std::unique_ptr<DrawingAreaProxy>)_createDrawingAreaProxy
{
    return std::make_unique<RemoteLayerTreeDrawingAreaProxy>(*_page);
}

- (void)_processDidExit
{
    [self cleanupInteraction];

    [self setShowingInspectorIndication:NO];
    [self _hideInspectorHighlight];
}

- (void)_didRelaunchProcess
{
    [self _accessibilityRegisterUIProcessTokens];
    [self setupInteraction];
}

- (void)_didCommitLoadForMainFrame
{
    [self _stopAssistingNode];
    [self _cancelLongPressGestureRecognizer];
    [_webView _didCommitLoadForMainFrame];
}

- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    CGSize contentsSize = layerTreeTransaction.contentsSize();
    CGPoint scrollOrigin = -layerTreeTransaction.scrollOrigin();
    CGRect contentBounds = { scrollOrigin, contentsSize };

    BOOL boundsChanged = !CGRectEqualToRect([self bounds], contentBounds);
    if (boundsChanged)
        [self setBounds:contentBounds];

    [_webView _didCommitLayerTree:layerTreeTransaction];

    if (_interactionViewsContainerView) {
        FloatPoint scaledOrigin = layerTreeTransaction.scrollOrigin();
        float scale = [[_webView scrollView] zoomScale];
        scaledOrigin.scale(scale, scale);
        [_interactionViewsContainerView setFrame:CGRectMake(scaledOrigin.x(), scaledOrigin.y(), 0, 0)];
    }
    
    if (boundsChanged) {
        FloatRect fixedPositionRect = _page->computeCustomFixedPositionRect(_page->unobscuredContentRect(), [[_webView scrollView] zoomScale]);
        [self updateFixedClippingView:fixedPositionRect];

        // We need to push the new content bounds to the webview to update fixed position rects.
        [_webView _updateVisibleContentRects];
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

- (void)_applicationWillResignActive:(NSNotification*)notification
{
    _page->applicationWillResignActive();
}

- (void)_applicationDidEnterBackground
{
    _page->applicationDidEnterBackground();
    _page->viewStateDidChange(ViewState::AllFlags & ~ViewState::IsInWindow);
}

- (void)_applicationWillEnterForeground
{
    _page->applicationWillEnterForeground();
    if (auto drawingArea = _page->drawingArea())
        drawingArea->hideContentUntilAnyUpdate();
    _page->viewStateDidChange(ViewState::AllFlags & ~ViewState::IsInWindow, true, WebPageProxy::ViewStateChangeDispatchMode::Immediate);
}

- (void)_applicationDidBecomeActive:(NSNotification*)notification
{
    _page->applicationDidBecomeActive();
}

@end

#endif // PLATFORM(IOS)
