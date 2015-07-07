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

#import "PageClientImplIOS.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "SmartMagnificationController.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKGeolocationProviderIOS.h"
#import "WKInspectorHighlightView.h"
#import "WKPreferencesInternal.h"
#import "WKProcessGroupPrivate.h"
#import "WKProcessPoolInternal.h"
#import "WKWebViewConfiguration.h"
#import "WKWebViewInternal.h"
#import "WebContext.h"
#import "WebFrameProxy.h"
#import "WebKit2Initialize.h"
#import "WebKitSystemInterfaceIOS.h"
#import "WebPageGroup.h"
#import "WebSystemInterface.h"
#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIWindow_Private.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FrameView.h>
#import <WebCore/InspectorOverlay.h>
#import <WebCore/NotImplemented.h>
#import <wtf/CurrentTime.h>
#import <wtf/RetainPtr.h>

#if __has_include(<QuartzCore/QuartzCorePrivate.h>)
#import <QuartzCore/QuartzCorePrivate.h>
#endif

@interface CALayer (Details)
@property BOOL hitTestsAsOpaque;
@end

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
}

- (instancetype)initWithFrame:(CGRect)frame context:(WebKit::WebContext&)context configuration:(WebKit::WebPageConfiguration)webPageConfiguration webView:(WKWebView *)webView
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    InitializeWebKit2();

    _pageClient = std::make_unique<PageClientImpl>(self, webView);

    _page = context.createWebPage(*_pageClient, WTF::move(webPageConfiguration));
    _page->initializeWebPage();
    _page->setIntrinsicDeviceScaleFactor(WKGetScaleFactorForScreen([UIScreen mainScreen]));
    _page->setUseFixedLayout(true);
    _page->setDelegatesScrolling(true);

    _webView = webView;
    
    _isBackground = [UIApplication sharedApplication].applicationState == UIApplicationStateBackground;

    WebContext::statistics().wkViewCount++;

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

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidEnterBackground:) name:UIApplicationDidEnterBackgroundNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillEnterForeground:) name:UIApplicationWillEnterForegroundNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillResignActive:) name:UIApplicationWillResignActiveNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:[UIApplication sharedApplication]];

    return self;
}

- (void)dealloc
{
    [self cleanupInteraction];

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    _page->close();

    WebContext::statistics().wkViewCount--;

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

    if (window)
        [defaultCenter removeObserver:self name:UIWindowDidMoveToScreenNotification object:window];

    if (newWindow)
        [defaultCenter addObserver:self selector:@selector(_windowDidMoveToScreenNotification:) name:UIWindowDidMoveToScreenNotification object:newWindow];
}

- (void)didMoveToWindow
{
    if (self.window)
        [self _updateForScreen:self.window.screen];
    _page->viewStateDidChange(ViewState::AllFlags);
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
    return _isBackground;
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
    scale:(CGFloat)zoomScale minimumScale:(CGFloat)minimumScale inStableState:(BOOL)isStableState isChangingObscuredInsetsInteractively:(BOOL)isChangingObscuredInsetsInteractively
{
    double timestamp = monotonicallyIncreasingTime();
    HistoricalVelocityData::VelocityData velocityData;
    if (!isStableState)
        velocityData = _historicalKinematicData.velocityForNewData(visibleRect.origin, zoomScale, timestamp);
    else
        _historicalKinematicData.clear();

    FloatRect fixedPositionRectForLayout = _page->computeCustomFixedPositionRect(unobscuredRect, zoomScale, WebPageProxy::UnobscuredRectConstraint::ConstrainedToDocumentRect);
    _page->updateVisibleContentRects(visibleRect, unobscuredRect, unobscuredRectInScrollViewCoordinates, fixedPositionRectForLayout,
        zoomScale, isStableState, isChangingObscuredInsetsInteractively, timestamp, velocityData.horizontalVelocity, velocityData.verticalVelocity, velocityData.scaleChangeRate);

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

- (void)willStartZoomOrScroll
{
    [self _willStartScrollingOrZooming];
}

- (void)didZoomToScale:(CGFloat)scale
{
    [self _didEndScrollingOrZooming];
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
    _page->setIntrinsicDeviceScaleFactor(WKGetScaleFactorForScreen(screen));
    [self _accessibilityRegisterUIProcessTokens];
}

- (void)_setAccessibilityWebProcessToken:(NSData *)data
{
    // This means the web process has checked in and we should send information back to that process.
    [self _accessibilityRegisterUIProcessTokens];
}

- (void)_accessibilityRegisterUIProcessTokens
{
    RetainPtr<CFUUIDRef> uuid = adoptCF(CFUUIDCreate(kCFAllocatorDefault));
    NSData *remoteElementToken = WKAXRemoteToken(uuid.get());

    // Store information about the WebProcess that can later be retrieved by the iOS Accessibility runtime.
    if (_page->process().state() == WebProcessProxy::State::Running) {
        IPC::Connection* connection = _page->process().connection();
        WKAXStoreRemoteConnectionInformation(self, _page->process().processIdentifier(), connection->identifier().port, uuid.get());

        IPC::DataReference elementToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteElementToken bytes]), [remoteElementToken length]);
        _page->registerUIProcessAccessibilityTokens(elementToken, elementToken);
    }
}

#pragma mark PageClientImpl methods

- (std::unique_ptr<DrawingAreaProxy>)_createDrawingAreaProxy
{
    return std::make_unique<RemoteLayerTreeDrawingAreaProxy>(_page.get());
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
    [_webView _didCommitLoadForMainFrame];
}

- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    CGSize contentsSize = layerTreeTransaction.contentsSize();
    CGRect contentBounds = { CGPointZero, contentsSize };
    CGRect oldBounds = [self bounds];

    BOOL boundsChanged = !CGRectEqualToRect(oldBounds, contentBounds);
    if (boundsChanged)
        [self setBounds:contentBounds];

    [_webView _didCommitLayerTree:layerTreeTransaction];
    
    if (boundsChanged) {
        FloatRect fixedPositionRect = _page->computeCustomFixedPositionRect(_page->unobscuredContentRect(), [[_webView scrollView] zoomScale]);
        [self updateFixedClippingView:fixedPositionRect];
    }
    
    [self _updateChangedSelection];
}

- (void)_setAcceleratedCompositingRootView:(UIView *)rootView
{
    for (UIView* subview in [_rootContentView subviews])
        [subview removeFromSuperview];

    [_rootContentView addSubview:rootView];
}

- (void)_decidePolicyForGeolocationRequestFromOrigin:(WebSecurityOrigin&)origin frame:(WebFrameProxy&)frame request:(GeolocationPermissionRequestProxy&)permissionRequest
{
    [[wrapper(_page->process().context()) _geolocationProvider] decidePolicyForGeolocationRequestFromOrigin:toAPI(&origin) frame:toAPI(&frame) request:toAPI(&permissionRequest) window:[self window]];
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
    return [_webView _zoomOutWithOrigin:origin];
}

- (void)_applicationWillResignActive:(NSNotification*)notification
{
    _page->applicationWillResignActive();
}

- (void)_applicationDidEnterBackground:(NSNotification*)notification
{
    _isBackground = YES;
    _page->viewStateDidChange(ViewState::AllFlags & ~ViewState::IsInWindow);
}

- (void)_applicationWillEnterForeground:(NSNotification*)notification
{
    _isBackground = NO;
    _page->applicationWillEnterForeground();
    if (auto drawingArea = _page->drawingArea())
        drawingArea->hideContentUntilNextUpdate();
    _page->viewStateDidChange(ViewState::AllFlags & ~ViewState::IsInWindow, true, WebPageProxy::ViewStateChangeDispatchMode::Immediate);
}

- (void)_applicationDidBecomeActive:(NSNotification*)notification
{
    _page->applicationDidBecomeActive();
}

@end

#endif // PLATFORM(IOS)
