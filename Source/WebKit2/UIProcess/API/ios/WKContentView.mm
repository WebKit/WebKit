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

#import "PageClientImplIOS.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "TiledCoreAnimationDrawingAreaProxyIOS.h"
#import "UIWKRemoteView.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKGeolocationProviderIOS.h"
#import "WKProcessGroupPrivate.h"
#import "WKInteractionView.h"
#import "WebContext.h"
#import "WebPageGroup.h"
#import "WebSystemInterface.h"
#import <QuartzCore/CALayerHost.h>
#import <UIKit/UIWindow_Private.h>
#import <WebCore/ViewportArguments.h>
#import <WebCore/WebCoreThreadSystemInterface.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;
using namespace WebKit;

@interface WKContentView (Internal) <WKBrowsingContextLoadDelegateInternal>
@end

@implementation WKContentView {
    RetainPtr<WKProcessGroup> _processGroup;
    RetainPtr<WKBrowsingContextGroup> _browsingContextGroup;
    OwnPtr<PageClientImpl> _pageClient;
    RefPtr<WebPageProxy> _page;

    RetainPtr<WKBrowsingContextController> _browsingContextController;

    RetainPtr<UIView> _rootContentView;
    RetainPtr<WKInteractionView> _interactionView;
}

- (id)initWithCoder:(NSCoder *)coder
{
    if (!(self = [super initWithCoder:coder]))
        return nil;

    [self _commonInitWithProcessGroup:nil browsingContextGroup:nil];
    return self;
}

- (id)initWithFrame:(CGRect)frame
{
    return [self initWithFrame:frame processGroup:nil browsingContextGroup:nil];
}

- (id)initWithFrame:(CGRect)frame processGroup:(WKProcessGroup *)processGroup browsingContextGroup:(WKBrowsingContextGroup *)browsingContextGroup
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    [self _commonInitWithProcessGroup:processGroup browsingContextGroup:browsingContextGroup];
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

- (void)setMinimumSize:(CGSize)size
{
    _page->drawingArea()->setSize(IntSize(size), IntSize(), IntSize());
}

- (void)setViewportSize:(CGSize)size
{
    _page->setFixedLayoutSize(IntSize(size));
}

- (void)didScrollTo:(CGPoint)contentOffset
{
    _page->didFinishScrolling(contentOffset);
}

- (void)didZoomToScale:(CGFloat)scale
{
    _page->didFinishZooming(scale);
}

static void decidePolicyForGeolocationPermissionRequest(WKPageRef page, WKFrameRef frame, WKSecurityOriginRef origin, WKGeolocationPermissionRequestRef permissionRequest, const void* clientInfo)
{
    WKContentView *view = reinterpret_cast<WKContentView *>(const_cast<void*>(clientInfo));
    ASSERT([view isKindOfClass:[WKContentView class]]);

    [[view->_processGroup _geolocationProvider] decidePolicyForGeolocationRequestFromOrigin:origin frame:frame request:permissionRequest window:[view window]];
}

- (void)_commonInitWithProcessGroup:(WKProcessGroup *)processGroup browsingContextGroup:(WKBrowsingContextGroup *)browsingContextGroup
{
    // FIXME: This should not be necessary, find why hit testing does not work otherwise.
    // <rdar://problem/12287363>
    self.backgroundColor = [UIColor blackColor];

    InitWebCoreThreadSystemInterface();
    InitWebCoreSystemInterface();
    RunLoop::initializeMainRunLoop();

    _processGroup = processGroup ? processGroup : adoptNS([[WKProcessGroup alloc] init]);
    _browsingContextGroup = browsingContextGroup ? browsingContextGroup : adoptNS([[WKBrowsingContextGroup alloc] initWithIdentifier:nil]);
    _pageClient = PageClientImpl::create(self);
    WebContext* processContext = toImpl([_processGroup _contextRef]);
    _page = processContext->createWebPage(_pageClient.get(), toImpl([_browsingContextGroup _pageGroupRef]));
    _page->initializeWebPage();
    _page->setIntrinsicDeviceScaleFactor([UIScreen mainScreen].scale);
    _page->setUseFixedLayout(true);

    WKPageUIClient pageUIClient = {
        kWKPageUIClientCurrentVersion,
        self,
        0, // createNewPage_deprecatedForUseWithV0
        0, // showPage
        0, // closeOtherPage
        0, // takeFocus
        0, // focus
        0, // unfocus
        0, // runJavaScriptAlert
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        0, // setStatusText
        0, // mouseDidMoveOverElement_deprecatedForUseWithV0
        0, // missingPluginButtonClicked
        0, // didNotHandleKeyEvent
        0, // didNotHandleWheelEvent
        0, // toolbarsAreVisible
        0, // setToolbarsAreVisible
        0, // menuBarIsVisible
        0, // setMenuBarIsVisible
        0, // statusBarIsVisible
        0, // setStatusBarIsVisible
        0, // isResizable
        0, // setIsResizable
        0, // getWindowFrameOtherPage
        0, // setWindowFrameOtherPage
        0, // runBeforeUnloadConfirmPanel
        0, // didDraw
        0, // pageDidScroll
        0, // exceededDatabaseQuota
        0, // runOpenPanel
        decidePolicyForGeolocationPermissionRequest,
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        0, // printFrame
        0, // runModal
        0, // didCompleteRubberBandForMainFrame
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        0, // createOtherPage
        0, // mouseDidMoveOverElement
        0, // decidePolicyForNotificationPermissionRequest
        0, // unavailablePluginButtonClicked
        0, // showColorPicker
        0, // hideColorPicker
        0, // unavailablePluginButtonClicked
    };
    WKPageSetPageUIClient(toAPI(_page.get()), &pageUIClient);

    _browsingContextController = adoptNS([[WKBrowsingContextController alloc] _initWithPageRef:toAPI(_page.get())]);
    [_browsingContextController setLoadDelegateInternal:self];

    WebContext::statistics().wkViewCount++;

    _rootContentView = adoptNS([[UIView alloc] init]);
    [[_rootContentView layer] setMasksToBounds:NO];
    [_rootContentView setUserInteractionEnabled:NO];

    [self addSubview:_rootContentView.get()];

    _interactionView = adoptNS([[WKInteractionView alloc] init]);
    [_interactionView setPage:_page];
    [self addSubview:_interactionView.get()];
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

- (void)_didChangeViewportArguments:(const WebCore::ViewportArguments&)arguments
{
    if ([_delegate respondsToSelector:@selector(contentView:didChangeViewportArgumentsSize:initialScale:minimumScale:maximumScale:allowsUserScaling:)])
        [_delegate contentView:self
didChangeViewportArgumentsSize:CGSizeMake(arguments.width, arguments.height)
                  initialScale:arguments.zoom
                  minimumScale:arguments.minZoom
                  maximumScale:arguments.maxZoom
             allowsUserScaling:arguments.userZoom];
}

#pragma mark PageClientImpl methods

- (std::unique_ptr<DrawingAreaProxy>)_createDrawingAreaProxy
{
    return std::make_unique<RemoteLayerTreeDrawingAreaProxy>(_page.get());
}

- (void)_processDidCrash
{
    // FIXME: Implement.
}

- (void)_didRelaunchProcess
{
    // FIXME: Implement.
}

- (void)browsingContextControllerDidCommitLoad:(WKBrowsingContextController *)sender
{
    ASSERT(sender == _browsingContextController);
    if ([_delegate respondsToSelector:@selector(contentViewdidCommitLoadForMainFrame:)])
        [_delegate contentViewdidCommitLoadForMainFrame:self];
}

- (void)_didChangeContentSize:(CGSize)contentsSize
{
    [self setBounds:{CGPointZero, contentsSize}];
    [_interactionView setFrame:CGRectMake(0, 0, contentsSize.width, contentsSize.height)];
    [_rootContentView setFrame:CGRectMake(0, 0, contentsSize.width, contentsSize.height)];

    if ([_delegate respondsToSelector:@selector(contentView:contentsSizeDidChange:)])
        [_delegate contentView:self contentsSizeDidChange:contentsSize];
}

- (void)_mainDocumentDidReceiveMobileDocType
{
    if ([_delegate respondsToSelector:@selector(contentViewDidReceiveMobileDocType:)])
        [_delegate contentViewDidReceiveMobileDocType:self];

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

@end
