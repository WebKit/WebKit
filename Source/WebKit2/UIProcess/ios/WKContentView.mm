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
#import "WebKit2Initialize.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKGeolocationProviderIOS.h"
#import "WKPreferencesInternal.h"
#import "WKProcessGroupPrivate.h"
#import "WKProcessPoolInternal.h"
#import "WKWebViewInternal.h"
#import "WKWebViewConfiguration.h"
#import "WebContext.h"
#import "WebFrameProxy.h"
#import "WebPageGroup.h"
#import "WebSystemInterface.h"
#import "WebKitSystemInterfaceIOS.h"
#import <WebCore/FrameView.h>
#import <UIKit/UIWindow_Private.h>
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
    RetainPtr<WKInspectorIndicationView> _inspectorIndicationView;

    WKWebView *_webView;

    HistoricalVelocityData _historicalKinematicData;
}

- (instancetype)initWithFrame:(CGRect)frame context:(WebKit::WebContext&)context configuration:(WebKit::WebPageConfiguration)webPageConfiguration webView:(WKWebView *)webView
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    InitializeWebKit2();

    _pageClient = std::make_unique<PageClientImpl>(self, webView);

    _page = context.createWebPage(*_pageClient, std::move(webPageConfiguration));
    _page->initializeWebPage();
    _page->setIntrinsicDeviceScaleFactor(WKGetScaleFactorForScreen([UIScreen mainScreen]));
    _page->setUseFixedLayout(true);
    _page->setDelegatesScrolling(true);

    _webView = webView;

    WebContext::statistics().wkViewCount++;

    _rootContentView = adoptNS([[UIView alloc] init]);
    [_rootContentView layer].masksToBounds = NO;

    [self addSubview:_rootContentView.get()];

    [self setupInteraction];
    [self setUserInteractionEnabled:YES];

    self.layer.hitTestsAsOpaque = YES;

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
    _page->viewStateDidChange(ViewState::IsInWindow);
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

- (BOOL)isShowingInspectorIndication
{
    return !!_inspectorIndicationView;
}

- (void)setShowingInspectorIndication:(BOOL)show
{
    if (show) {
        if (!_inspectorIndicationView) {
            _inspectorIndicationView = adoptNS([[WKInspectorIndicationView alloc] initWithFrame:[self bounds]]);
            [self insertSubview:_inspectorIndicationView.get() aboveSubview:_rootContentView.get()];
        }
    } else {
        if (_inspectorIndicationView) {
            [_inspectorIndicationView removeFromSuperview];
            _inspectorIndicationView = nil;
        }
    }
}

static inline FloatRect fixedPositionRectFromExposedRect(CGRect unobscuredRect, CGSize documentSize, CGFloat scale)
{
    return FrameView::rectForViewportConstrainedObjects(enclosingLayoutRect(unobscuredRect), roundedLayoutSize(FloatSize(documentSize)), scale, false, StickToViewportBounds);
}

- (void)didUpdateVisibleRect:(CGRect)visibleRect unobscuredRect:(CGRect)unobscuredRect scale:(CGFloat)zoomScale inStableState:(BOOL)isStableState
{
    double timestamp = monotonicallyIncreasingTime();
    HistoricalVelocityData::VelocityData velocityData;
    if (!isStableState)
        velocityData = _historicalKinematicData.velocityForNewData(visibleRect.origin, zoomScale, timestamp);
    else
        _historicalKinematicData.clear();

    double scaleNoiseThreshold = 0.0005;
    CGFloat filteredScale = zoomScale;
    if (!isStableState && fabs(filteredScale - _page->displayedContentScale()) < scaleNoiseThreshold) {
        // Tiny changes of scale during interactive zoom cause content to jump by one pixel, creating
        // visual noise. We filter those useless updates.
        filteredScale = _page->displayedContentScale();
    }

    FloatRect customFixedPositionRect = fixedPositionRectFromExposedRect(unobscuredRect, [self bounds].size, zoomScale);
    _page->updateVisibleContentRects(VisibleContentRectUpdateInfo(_page->nextVisibleContentRectUpdateID(), visibleRect, unobscuredRect, customFixedPositionRect, filteredScale, isStableState, timestamp, velocityData.horizontalVelocity, velocityData.verticalVelocity, velocityData.scaleChangeRate));
    
    RemoteScrollingCoordinatorProxy* scrollingCoordinator = _page->scrollingCoordinatorProxy();
    scrollingCoordinator->viewportChangedViaDelegatedScrolling(scrollingCoordinator->rootScrollingNodeID(), unobscuredRect, zoomScale);

    if (auto drawingArea = _page->drawingArea())
        drawingArea->updateDebugIndicator();
}

- (void)setMinimumSize:(CGSize)size
{
    _page->drawingArea()->setSize(IntSize(size), IntSize(), IntSize());
}

- (void)setMinimumLayoutSize:(CGSize)size
{
    _page->setViewportConfigurationMinimumLayoutSize(IntSize(CGCeiling(size.width), CGCeiling(size.height)));
}

- (void)didFinishScrolling
{
    [self _didEndScrollingOrZooming];
}

- (void)willStartZoomOrScroll
{
    [self _willStartScrollingOrZooming];
}

- (void)willStartUserTriggeredScroll
{
    [self _willStartUserTriggeredScrollingOrZooming];
}

- (void)willStartUserTriggeredZoom
{
    [self _willStartUserTriggeredScrollingOrZooming];
    _page->willStartUserTriggeredZooming();
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
    // FIXME: Implement.
}

- (void)_didRelaunchProcess
{
    [self _accessibilityRegisterUIProcessTokens];
}

- (void)_didCommitLoadForMainFrame
{
    [_webView _didCommitLoadForMainFrame];
    [self _stopAssistingNode];
}

- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    CGSize contentsSize = layerTreeTransaction.contentsSize();

    [self setBounds:{CGPointZero, contentsSize}];
    [_rootContentView setFrame:CGRectMake(0, 0, contentsSize.width, contentsSize.height)];
    [_inspectorIndicationView setFrame:[self bounds]];

    [_webView _didCommitLayerTree:layerTreeTransaction];
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
    // FIXME: The line below is commented out since wrapper(WebContext&) now returns a WKProcessPool.
    // As part of the new API we should figure out where geolocation fits in, see <rdar://problem/15885544>.
    // [[wrapper(_page->process().context()) _geolocationProvider] decidePolicyForGeolocationRequestFromOrigin:toAPI(&origin) frame:toAPI(&frame) request:toAPI(&permissionRequest) window:[self window]];
}

- (RetainPtr<CGImageRef>)_takeViewSnapshot
{
    return [_webView _takeViewSnapshot];
}

- (BOOL)_scrollToRect:(CGRect)targetRect withOrigin:(CGPoint)origin minimumScrollDistance:(CGFloat)minimumScrollDistance
{
    return [_webView _scrollToRect:targetRect origin:origin minimumScrollDistance:minimumScrollDistance];
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

- (void)_applicationWillEnterForeground:(NSNotification*)notification
{
    _page->applicationWillEnterForeground();
}

- (void)_applicationDidBecomeActive:(NSNotification*)notification
{
    _page->applicationDidBecomeActive();
}

@end

#endif // PLATFORM(IOS)
