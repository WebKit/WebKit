/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "WKContentViewInternal.h"

#import "InteractionInformationAtPosition.h"
#import "PageClientImplIOS.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "WebKit2Initialize.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKGeolocationProviderIOS.h"
#import "WKInteractionView.h"
#import "WKProcessGroupPrivate.h"
#import "WKProcessClassInternal.h"
#import "WKWebViewConfiguration.h"
#import "WebContext.h"
#import "WebFrameProxy.h"
#import "WebPageGroup.h"
#import "WebSystemInterface.h"
#import <UIKit/UIWindow_Private.h>
#import <WebCore/ViewportArguments.h>
#import <wtf/RetainPtr.h>

#if __has_include(<QuartzCore/QuartzCorePrivate.h>)
#import <QuartzCore/QuartzCorePrivate.h>
#endif

@interface CALayer (Details)
@property BOOL hitTestsAsOpaque;
@end

using namespace WebCore;
using namespace WebKit;

@implementation WKContentView {
    std::unique_ptr<PageClientImpl> _pageClient;
    RetainPtr<WKBrowsingContextController> _browsingContextController;

    RetainPtr<UIView> _rootContentView;
    RetainPtr<WKInteractionView> _interactionView;

    WebCore::FloatPoint _currentExposedRectPosition;
}

- (id)initWithFrame:(CGRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef
{
    return [self initWithFrame:frame contextRef:contextRef pageGroupRef:pageGroupRef relatedToPage:nullptr];
}

- (id)initWithFrame:(CGRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef relatedToPage:(WKPageRef)relatedPage
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    [self _commonInitializationWithContextRef:contextRef pageGroupRef:pageGroupRef relatedToPage:relatedPage];
    return self;
}

- (id)initWithFrame:(CGRect)frame processGroup:(WKProcessGroup *)processGroup browsingContextGroup:(WKBrowsingContextGroup *)browsingContextGroup
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    [self _commonInitializationWithContextRef:processGroup._contextRef pageGroupRef:browsingContextGroup._pageGroupRef relatedToPage:nullptr];
    return self;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    InitializeWebKit2();
    RunLoop::initializeMainRunLoop();

    _pageClient = std::make_unique<PageClientImpl>(self);

    _page = configuration.processClass->_context->createWebPage(*_pageClient, nullptr);
    _page->initializeWebPage();
    _page->setIntrinsicDeviceScaleFactor([UIScreen mainScreen].scale);
    _page->setUseFixedLayout(true);

    WebContext::statistics().wkViewCount++;

    _rootContentView = adoptNS([[UIView alloc] init]);
    [_rootContentView layer].masksToBounds = NO;
    [_rootContentView setUserInteractionEnabled:NO];

    [self addSubview:_rootContentView.get()];

    _interactionView = adoptNS([[WKInteractionView alloc] init]);
    [_interactionView setPage:_page];
    [self addSubview:_interactionView.get()];

    self.layer.hitTestsAsOpaque = YES;

    return self;
}

- (void)dealloc
{
    _page->close();

    WebContext::statistics().wkViewCount--;

    [super dealloc];
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
        _browsingContextController = [[WKBrowsingContextController alloc] _initWithPageRef:toAPI(_page.get())];

    return _browsingContextController.get();
}

- (WKContentType)contentType
{
    if (_page->mainFrame()->mimeType() == "text/plain")
        return WKContentType::PlainText;
    else if (_page->mainFrame()->isDisplayingStandaloneImageDocument())
        return WKContentType::Image;
    return WKContentType::Standard;
}

- (WKPageRef)_pageRef
{
    return toAPI(_page.get());
}

- (void)setMinimumSize:(CGSize)size
{
    _page->drawingArea()->setSize(IntSize(size), IntSize(), IntSize());
}

- (FloatRect)fixedPositionRectFromExposedRect:(FloatRect)exposedRect scale:(float)scale
{
    // FIXME: This should modify the rect based on the scale.
    UNUSED_PARAM(scale);
    return exposedRect;
}

- (void)_updateViewExposedRect
{
    FloatPoint exposedRectPosition = _currentExposedRectPosition;
    exposedRectPosition.scale(_page->pageScaleFactor(), _page->pageScaleFactor());

    if (auto drawingArea = _page->drawingArea())
        drawingArea->setExposedRect(FloatRect(exposedRectPosition, _page->drawingArea()->size()));
}

- (void)_updateFixedPositionRect
{
    auto drawingArea = _page->drawingArea();
    if (!drawingArea)
        return;
    FloatRect exposedRect(_currentExposedRectPosition, drawingArea->size());
    FloatRect fixedPosRect = [self fixedPositionRectFromExposedRect:exposedRect scale:_page->pageScaleFactor()];
    drawingArea->setCustomFixedPositionRect(fixedPosRect);
}

- (void)setViewportSize:(CGSize)size
{
    _page->setFixedLayoutSize(IntSize(size));
    [self _updateViewExposedRect];
}

- (void)didFinishScrollTo:(CGPoint)contentOffset
{
    _currentExposedRectPosition = contentOffset;
    _page->didFinishScrolling(contentOffset);
    [self _updateViewExposedRect];
    [self _updateFixedPositionRect];
}

- (void)didScrollTo:(CGPoint)contentOffset
{
    _currentExposedRectPosition = contentOffset;
    [self _updateViewExposedRect];

    _page->scrollingCoordinatorProxy()->scrollPositionChangedViaDelegatedScrolling(_page->scrollingCoordinatorProxy()->rootScrollingNodeID(), roundedIntPoint(contentOffset));
}

- (void)didZoomToScale:(CGFloat)scale
{
    _page->didFinishZooming(scale);
    [self _updateViewExposedRect];
    [self _updateFixedPositionRect];
}

#pragma mark Internal

- (void)_commonInitializationWithContextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef relatedToPage:(WKPageRef)relatedPage
{
    InitializeWebKit2();
    RunLoop::initializeMainRunLoop();

    _pageClient = std::make_unique<PageClientImpl>(self);
    _page = toImpl(contextRef)->createWebPage(*_pageClient, toImpl(pageGroupRef), toImpl(relatedPage));
    _page->initializeWebPage();
    _page->setIntrinsicDeviceScaleFactor([UIScreen mainScreen].scale);
    _page->setUseFixedLayout(true);

    WebContext::statistics().wkViewCount++;

    _rootContentView = adoptNS([[UIView alloc] init]);
    [[_rootContentView layer] setMasksToBounds:NO];
    [_rootContentView setUserInteractionEnabled:NO];

    [self addSubview:_rootContentView.get()];

    _interactionView = adoptNS([[WKInteractionView alloc] init]);
    [_interactionView setPage:_page];
    [self addSubview:_interactionView.get()];

    self.layer.hitTestsAsOpaque = YES;
}

- (void)_windowDidMoveToScreenNotification:(NSNotification *)notification
{
    ASSERT(notification.object == self.window);

    UIScreen *screen = notification.userInfo[UIWindowNewScreenUserInfoKey];
    [self _updateForScreen:screen];
}

- (void)_updateForScreen:(UIScreen *)screen
{
    ASSERT(screen);
    _page->setIntrinsicDeviceScaleFactor(screen.scale);
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
    // FIXME: Implement.
}

- (void)_didCommitLoadForMainFrame
{
    if ([_delegate respondsToSelector:@selector(contentViewDidCommitLoadForMainFrame:)])
        [_delegate contentViewDidCommitLoadForMainFrame:self];
}

- (void)_didChangeContentSize:(CGSize)contentsSize
{
    [self setBounds:{CGPointZero, contentsSize}];
    [_interactionView setFrame:CGRectMake(0, 0, contentsSize.width, contentsSize.height)];
    [_rootContentView setFrame:CGRectMake(0, 0, contentsSize.width, contentsSize.height)];

    if ([_delegate respondsToSelector:@selector(contentView:contentsSizeDidChange:)])
        [_delegate contentView:self contentsSizeDidChange:contentsSize];
}

- (void)_didReceiveMobileDocTypeForMainFrame
{
    if ([_delegate respondsToSelector:@selector(contentViewDidReceiveMobileDocType:)])
        [_delegate contentViewDidReceiveMobileDocType:self];
}

- (void)_didChangeViewportArguments:(const WebCore::ViewportArguments&)arguments
{
    if ([_delegate respondsToSelector:@selector(contentView:didChangeViewportArgumentsSize:initialScale:minimumScale:maximumScale:allowsUserScaling:)])
        [_delegate contentView:self didChangeViewportArgumentsSize:CGSizeMake(arguments.width, arguments.height) initialScale:arguments.zoom minimumScale:arguments.minZoom maximumScale:arguments.maxZoom allowsUserScaling:arguments.userZoom];
}

- (void)_didGetTapHighlightForRequest:(uint64_t)requestID color:(const Color&)color quads:(const Vector<FloatQuad>&)highlightedQuads topLeftRadius:(const IntSize&)topLeftRadius topRightRadius:(const IntSize&)topRightRadius bottomLeftRadius:(const IntSize&)bottomLeftRadius bottomRightRadius:(const IntSize&)bottomRightRadius
{
    [_interactionView _didGetTapHighlightForRequest:requestID color:color quads:highlightedQuads topLeftRadius:topLeftRadius topRightRadius:topRightRadius bottomLeftRadius:bottomLeftRadius bottomRightRadius:bottomRightRadius];
}

- (void)_setAcceleratedCompositingRootLayer:(CALayer *)rootLayer
{
    [[_rootContentView layer] setSublayers:@[rootLayer]];
}

// FIXME: change the name. Leave it for now to make it easier to refer to the UIKit implementation.
- (void)_startAssistingNode
{
    [_interactionView _startAssistingNode];
}

- (void)_stopAssistingNode
{
    [_interactionView _stopAssistingNode];
}

- (void)_selectionChanged
{
    [_interactionView _selectionChanged];
}

- (BOOL)_interpretKeyEvent:(WebIOSEvent *)theEvent isCharEvent:(BOOL)isCharEvent
{
    return [_interactionView _interpretKeyEvent:theEvent isCharEvent:isCharEvent];
}

- (void)_positionInformationDidChange:(const InteractionInformationAtPosition&)info
{
    [_interactionView _positionInformationDidChange:info];
}

- (void)_decidePolicyForGeolocationRequestFromOrigin:(WebSecurityOrigin&)origin frame:(WebFrameProxy&)frame request:(GeolocationPermissionRequestProxy&)permissionRequest
{
    // FIXME: The line below is commented out since wrapper(WebContext&) now returns a WKProcessClass.
    // As part of the new API we should figure out where geolocation fits in, see <rdar://problem/15885544>.
    // [[wrapper(_page->process().context()) _geolocationProvider] decidePolicyForGeolocationRequestFromOrigin:toAPI(&origin) frame:toAPI(&frame) request:toAPI(&permissionRequest) window:[self window]];
}

@end
