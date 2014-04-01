/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKWebViewInternal.h"

#if WK_API_ENABLED

#import "NavigationState.h"
#import "RemoteLayerTreeTransaction.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "UIClient.h"
#import "ViewGestureController.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKHistoryDelegatePrivate.h"
#import "WKNSData.h"
#import "WKNSURLExtras.h"
#import "WKNavigationDelegate.h"
#import "WKNavigationInternal.h"
#import "WKPreferencesInternal.h"
#import "WKProcessPoolInternal.h"
#import "WKUIDelegate.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewContentProvider.h"
#import "WebBackForwardList.h"
#import "WebCertificateInfo.h"
#import "WebContext.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import "_WKUserContentController.h"
#import "_WKVisitedLinkProviderInternal.h"
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import "WKPDFView.h"
#import "WKScrollView.h"
#import "WKWebViewContentProviderRegistry.h"
#import <UIKit/UIPeripheralHost_Private.h>

@interface UIScrollView (UIScrollViewInternal)
- (void)_adjustForAutomaticKeyboardInfo:(NSDictionary*)info animated:(BOOL)animated lastAdjustment:(CGFloat*)lastAdjustment;
@end

@interface UIPeripheralHost(UIKitInternal)
- (CGFloat)getVerticalOverlapForView:(UIView *)view usingKeyboardInfo:(NSDictionary *)info;
@end
#endif

#if PLATFORM(MAC)
#import "WKViewInternal.h"
#import <WebCore/ColorMac.h>
#endif

@implementation WKWebView {
    std::unique_ptr<WebKit::NavigationState> _navigationState;

    RetainPtr<_WKRemoteObjectRegistry> _remoteObjectRegistry;
    _WKRenderingProgressEvents _observedRenderingProgressEvents;

#if PLATFORM(IOS)
    RetainPtr<WKScrollView> _scrollView;
    RetainPtr<WKContentView> _contentView;

    BOOL _isWaitingForNewLayerTreeAfterDidCommitLoad;
    BOOL _hasStaticMinimumLayoutSize;
    CGSize _minimumLayoutSizeOverride;

    UIEdgeInsets _obscuredInsets;
    bool _isChangingObscuredInsetsInteractively;
    CGFloat _lastAdjustmentForScroller;
    CGFloat _keyboardVerticalOverlap;

    std::unique_ptr<WebKit::ViewGestureController> _gestureController;
    BOOL _allowsBackForwardNavigationGestures;

    RetainPtr<UIView <WKWebViewContentProvider>> _customContentView;
#endif
#if PLATFORM(MAC)
    RetainPtr<WKView> _wkView;
#endif
}

- (instancetype)initWithFrame:(CGRect)frame
{
    return [self initWithFrame:frame configuration:adoptNS([[WKWebViewConfiguration alloc] init]).get()];
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _configuration = adoptNS([configuration copy]);

    if (WKWebView *relatedWebView = [_configuration _relatedWebView]) {
        WKProcessPool *processPool = [_configuration processPool];
        WKProcessPool *relatedWebViewProcessPool = [relatedWebView->_configuration processPool];
        if (processPool && processPool != relatedWebViewProcessPool)
            [NSException raise:NSInvalidArgumentException format:@"Related web view %@ has process pool %@ but configuration specifies a different process pool %@", relatedWebView, relatedWebViewProcessPool, configuration.processPool];

        [_configuration setProcessPool:relatedWebViewProcessPool];
    }

    if (![_configuration processPool])
        [_configuration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];

    if (![_configuration preferences])
        [_configuration setPreferences:adoptNS([[WKPreferences alloc] init]).get()];

    if (![_configuration _userContentController])
        [_configuration _setUserContentController:adoptNS([[_WKUserContentController alloc] init]).get()];

    if (![_configuration _visitedLinkProvider])
        [_configuration _setVisitedLinkProvider:adoptNS([[_WKVisitedLinkProvider alloc] init]).get()];

#if PLATFORM(IOS)
    if (![_configuration _contentProviderRegistry])
        [_configuration _setContentProviderRegistry:adoptNS([[WKWebViewContentProviderRegistry alloc] init]).get()];
#endif

    CGRect bounds = self.bounds;

    WebKit::WebContext& context = *[_configuration processPool]->_context;

    WebKit::WebPageConfiguration webPageConfiguration;
    webPageConfiguration.preferences = [_configuration preferences]->_preferences.get();
    if (WKWebView *relatedWebView = [_configuration _relatedWebView])
        webPageConfiguration.relatedPage = relatedWebView->_page.get();

    webPageConfiguration.visitedLinkProvider = [_configuration _visitedLinkProvider]->_visitedLinkProvider.get();

    RefPtr<WebKit::WebPageGroup> pageGroup;
    NSString *groupIdentifier = configuration._groupIdentifier;
    if (groupIdentifier.length) {
        pageGroup = WebKit::WebPageGroup::create(configuration._groupIdentifier);
        webPageConfiguration.pageGroup = pageGroup.get();
    }

#if PLATFORM(IOS)
    _scrollView = adoptNS([[WKScrollView alloc] initWithFrame:bounds]);
    [_scrollView setInternalDelegate:self];
    [_scrollView setBouncesZoom:YES];
    [_scrollView setBackgroundColor:[UIColor whiteColor]];

    [self addSubview:_scrollView.get()];

    _contentView = adoptNS([[WKContentView alloc] initWithFrame:bounds context:context configuration:std::move(webPageConfiguration) webView:self]);
    _page = [_contentView page];
    [_contentView layer].anchorPoint = CGPointZero;
    [_contentView setFrame:bounds];
    [_scrollView addSubview:_contentView.get()];

    [self _frameOrBoundsChanged];

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(_keyboardWillChangeFrame:) name:UIKeyboardWillChangeFrameNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardDidChangeFrame:) name:UIKeyboardDidChangeFrameNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardWillShow:) name:UIKeyboardWillShowNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardWillHide:) name:UIKeyboardWillHideNotification object:nil];

    [[_configuration _contentProviderRegistry] addPage:*_page];
#endif

#if PLATFORM(MAC)
    _wkView = [[WKView alloc] initWithFrame:bounds context:context configuration:std::move(webPageConfiguration) webView:self];
    [self addSubview:_wkView.get()];
    _page = WebKit::toImpl([_wkView pageRef]);
#endif

    _navigationState = std::make_unique<WebKit::NavigationState>(self);
    _page->setPolicyClient(_navigationState->createPolicyClient());
    _page->setLoaderClient(_navigationState->createLoaderClient());

    _page->setUIClient(std::make_unique<WebKit::UIClient>(self));

    return self;
}

- (void)dealloc
{
    [_remoteObjectRegistry _invalidate];
#if PLATFORM(IOS)
    [[_configuration _contentProviderRegistry] removePage:*_page];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
#endif

    [super dealloc];
}

- (WKWebViewConfiguration *)configuration
{
    return [[_configuration copy] autorelease];
}

- (WKBackForwardList *)backForwardList
{
    return wrapper(_page->backForwardList());
}

- (id <WKNavigationDelegate>)navigationDelegate
{
    return [_navigationState->navigationDelegate().leakRef() autorelease];
}

- (void)setNavigationDelegate:(id <WKNavigationDelegate>)navigationDelegate
{
    _navigationState->setNavigationDelegate(navigationDelegate);
}

- (id <WKUIDelegate>)UIDelegate
{
    return [static_cast<WebKit::UIClient&>(_page->uiClient()).delegate().leakRef() autorelease];
}

- (void)setUIDelegate:(id<WKUIDelegate>)UIDelegate
{
    static_cast<WebKit::UIClient&>(_page->uiClient()).setDelegate(UIDelegate);
}

- (WKNavigation *)loadRequest:(NSURLRequest *)request
{
    uint64_t navigationID = _page->loadRequest(request);
    auto navigation = _navigationState->createLoadRequestNavigation(navigationID, request);

    return [navigation.leakRef() autorelease];
}

- (WKNavigation *)goToBackForwardListItem:(WKBackForwardListItem *)item
{
    _page->goToBackForwardItem(&item._item);

    // FIXME: return a WKNavigation object.
    return nil;
}

- (NSString *)title
{
    return _page->pageLoadState().title();
}

- (NSURL *)activeURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().activeURL()];
}

- (BOOL)isLoading
{
    return _page->pageLoadState().isLoading();
}

- (double)estimatedProgress
{
    return _page->pageLoadState().estimatedProgress();
}

- (BOOL)hasOnlySecureContent
{
    return _page->pageLoadState().hasOnlySecureContent();
}

// FIXME: This should be KVO compliant.
- (BOOL)canGoBack
{
    return !!_page->backForwardList().backItem();
}

// FIXME: This should be KVO compliant.
- (BOOL)canGoForward
{
    return !!_page->backForwardList().forwardItem();
}

- (WKNavigation *)goBack
{
    _page->goBack();

    // FIXME: Return a navigation object.
    return nil;
}

- (WKNavigation *)goForward
{
    _page->goForward();

    // FIXME: Return a navigation object.
    return nil;
}

- (WKNavigation *)reload
{
    _page->reload(false);

    // FIXME: Return a navigation object.
    return nil;
}

- (WKNavigation *)reloadFromOrigin
{
    _page->reload(true);

    // FIXME: Return a navigation object.
    return nil;
}

- (void)stopLoading
{
    _page->stopLoading();
}

- (IBAction)stopLoading:(id)sender
{
    _page->stopLoading();
}

#pragma mark iOS-specific methods

#if PLATFORM(IOS)
- (void)setFrame:(CGRect)frame
{
    CGRect oldFrame = self.frame;
    [super setFrame:frame];

    if (!CGSizeEqualToSize(oldFrame.size, frame.size))
        [self _frameOrBoundsChanged];
}

- (void)setBounds:(CGRect)bounds
{
    CGRect oldBounds = self.bounds;
    [super setBounds:bounds];

    if (!CGSizeEqualToSize(oldBounds.size, bounds.size))
        [self _frameOrBoundsChanged];
}

- (UIScrollView *)scrollView
{
    return _scrollView.get();
}

- (WKBrowsingContextController *)browsingContextController
{
    return [_contentView browsingContextController];
}

- (void)_setHasCustomContentView:(BOOL)pageHasCustomContentView loadedMIMEType:(const WTF::String&)mimeType
{
    if (pageHasCustomContentView) {
        [_customContentView removeFromSuperview];

        Class representationClass = [[_configuration _contentProviderRegistry] providerForMIMEType:mimeType];
        ASSERT(representationClass);
        _customContentView = adoptNS([[representationClass alloc] init]);

        [_contentView removeFromSuperview];
        [_scrollView addSubview:_customContentView.get()];

        [_customContentView web_setMinimumSize:self.bounds.size];
        [_customContentView web_setScrollView:_scrollView.get()];
    } else if (_customContentView) {
        [_customContentView removeFromSuperview];
        _customContentView = nullptr;

        [_scrollView addSubview:_contentView.get()];
        [_scrollView setContentSize:[_contentView frame].size];
    }
}

- (void)_didFinishLoadingDataForCustomContentProviderWithSuggestedFilename:(const String&)suggestedFilename data:(NSData *)data
{
    ASSERT(_customContentView);
    [_customContentView web_setContentProviderData:data];
}

- (void)_didCommitLoadForMainFrame
{
    _isWaitingForNewLayerTreeAfterDidCommitLoad = YES;
}

// This is a convenience method that will convert _page->pageExtendedBackgroundColor() from a WebCore::Color to a UIColor *.
- (UIColor *)pageExtendedBackgroundColor
{
    WebCore::Color color = _page->pageExtendedBackgroundColor();
    if (!color.isValid())
        return nil;

    return [UIColor colorWithRed:(color.red() / 255.0) green:(color.green() / 255.0) blue:(color.blue() / 255.0) alpha:(color.alpha() / 255.0)];
}

static CGFloat contentZoomScale(WKWebView* webView)
{
    UIView *zoomView;
    if (webView->_customContentView)
        zoomView = webView->_customContentView.get();
    else
        zoomView = webView->_contentView.get();

    CGFloat scale = [[zoomView layer] affineTransform].a;
    ASSERT(scale == [webView->_scrollView zoomScale]);
    return scale;
}

- (void)_updateScrollViewBackground
{
    UIColor *pageExtendedBackgroundColor = [self pageExtendedBackgroundColor];

    CGFloat zoomScale = contentZoomScale(self);
    CGFloat minimumZoomScale = [_scrollView minimumZoomScale];
    if (zoomScale < minimumZoomScale) {
        CGFloat slope = 12;
        CGFloat opacity = std::max(1 - slope * (minimumZoomScale - zoomScale), static_cast<CGFloat>(0));
        pageExtendedBackgroundColor = [pageExtendedBackgroundColor colorWithAlphaComponent:opacity];
    }

    [_scrollView setBackgroundColor:pageExtendedBackgroundColor];
}

- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    ASSERT(!_customContentView);

    [_scrollView setContentSize:[_contentView frame].size];
    [_scrollView setMinimumZoomScale:layerTreeTransaction.minimumScaleFactor()];
    [_scrollView setMaximumZoomScale:layerTreeTransaction.maximumScaleFactor()];
    [_scrollView setZoomEnabled:layerTreeTransaction.allowsUserScaling()];
    if (!layerTreeTransaction.scaleWasSetByUIProcess() && ![_scrollView isZooming] && ![_scrollView isZoomBouncing] && ![_scrollView _isAnimatingZoom])
        [_scrollView setZoomScale:layerTreeTransaction.pageScaleFactor()];
    
    [self _updateScrollViewBackground];

    if (_gestureController)
        _gestureController->setRenderTreeSize(layerTreeTransaction.renderTreeSize());

    if (_isWaitingForNewLayerTreeAfterDidCommitLoad) {
        UIEdgeInsets inset = [_scrollView contentInset];
        [_scrollView setContentOffset:CGPointMake(-inset.left, -inset.top)];
        _isWaitingForNewLayerTreeAfterDidCommitLoad = NO;
    }

}

- (RetainPtr<CGImageRef>)_takeViewSnapshot
{
    // FIXME: We should be able to use acquire an IOSurface directly, instead of going to CGImage here and back in ViewSnapshotStore.
    UIGraphicsBeginImageContextWithOptions(self.bounds.size, YES, self.window.screen.scale);
    [self drawViewHierarchyInRect:[self bounds] afterScreenUpdates:NO];
    UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return image.CGImage;
}

- (void)_zoomToPoint:(WebCore::FloatPoint)point atScale:(double)scale
{
    double maximumZoomDuration = 0.4;
    double minimumZoomDuration = 0.1;
    double zoomDurationFactor = 0.3;

    CGFloat zoomScale = contentZoomScale(self);
    CFTimeInterval duration = std::min(fabs(log(zoomScale) - log(scale)) * zoomDurationFactor + minimumZoomDuration, maximumZoomDuration);

    if (scale != zoomScale)
        [_contentView willStartUserTriggeredZoom];

    [_scrollView _zoomToCenter:point scale:scale duration:duration];
}

- (void)_zoomToRect:(WebCore::FloatRect)targetRect atScale:(double)scale origin:(WebCore::FloatPoint)origin
{
    WebCore::FloatSize unobscuredContentSize([self _contentRectForUserInteraction].size);
    WebCore::FloatSize targetRectSizeAfterZoom = targetRect.size();
    targetRectSizeAfterZoom.scale(scale);

    // Center the target rect in the scroll view.
    // If the target doesn't fit in the scroll view, center on the gesture location instead.
    WebCore::FloatPoint zoomCenter = targetRect.center();

    if (targetRectSizeAfterZoom.width() > unobscuredContentSize.width())
        zoomCenter.setX(origin.x());
    if (targetRectSizeAfterZoom.height() > unobscuredContentSize.height())
        zoomCenter.setY(origin.y());

    [self _zoomToPoint:zoomCenter atScale:scale];
}

static WebCore::FloatPoint constrainContentOffset(WebCore::FloatPoint contentOffset, WebCore::FloatSize contentSize, WebCore::FloatSize unobscuredContentSize)
{
    WebCore::FloatSize maximumContentOffset = contentSize - unobscuredContentSize;
    contentOffset = contentOffset.shrunkTo(WebCore::FloatPoint(maximumContentOffset.width(), maximumContentOffset.height()));
    contentOffset = contentOffset.expandedTo(WebCore::FloatPoint());
    return contentOffset;
}

- (BOOL)_scrollToRect:(WebCore::FloatRect)targetRect origin:(WebCore::FloatPoint)origin minimumScrollDistance:(float)minimumScrollDistance
{
    WebCore::FloatRect unobscuredContentRect([self _contentRectForUserInteraction]);
    WebCore::FloatPoint unobscuredContentOffset = unobscuredContentRect.location();
    WebCore::FloatSize contentSize([_contentView bounds].size);

    // Center the target rect in the scroll view.
    // If the target doesn't fit in the scroll view, center on the gesture location instead.
    WebCore::FloatPoint newUnobscuredContentOffset;
    if (targetRect.width() <= unobscuredContentRect.width())
        newUnobscuredContentOffset.setX(targetRect.x() - (unobscuredContentRect.width() - targetRect.width()) / 2);
    else
        newUnobscuredContentOffset.setX(origin.x() - unobscuredContentRect.width() / 2);
    if (targetRect.height() <= unobscuredContentRect.height())
        newUnobscuredContentOffset.setY(targetRect.y() - (unobscuredContentRect.height() - targetRect.height()) / 2);
    else
        newUnobscuredContentOffset.setY(origin.y() - unobscuredContentRect.height() / 2);
    newUnobscuredContentOffset = constrainContentOffset(newUnobscuredContentOffset, contentSize, unobscuredContentRect.size());

    if (unobscuredContentOffset == newUnobscuredContentOffset) {
        if (targetRect.width() > unobscuredContentRect.width())
            newUnobscuredContentOffset.setX(origin.x() - unobscuredContentRect.width() / 2);
        if (targetRect.height() > unobscuredContentRect.height())
            newUnobscuredContentOffset.setY(origin.y() - unobscuredContentRect.height() / 2);
        newUnobscuredContentOffset = constrainContentOffset(newUnobscuredContentOffset, contentSize, unobscuredContentRect.size());
    }

    WebCore::FloatSize scrollViewOffsetDelta = newUnobscuredContentOffset - unobscuredContentOffset;
    scrollViewOffsetDelta.scale(contentZoomScale(self));

    float scrollDistance = scrollViewOffsetDelta.diagonalLength();
    if (scrollDistance < minimumScrollDistance)
        return false;

    [_scrollView setContentOffset:([_scrollView contentOffset] + scrollViewOffsetDelta) animated:YES];
    return true;
}

- (void)_zoomOutWithOrigin:(WebCore::FloatPoint)origin
{
    [self _zoomToPoint:origin atScale:[_scrollView minimumZoomScale]];
}

- (BOOL)_zoomToRect:(WebCore::FloatRect)targetRect withOrigin:(WebCore::FloatPoint)origin fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale minimumScrollDistance:(float)minimumScrollDistance
{
    const float maximumScaleFactorDeltaForPanScroll = 0.02;

    double currentScale = contentZoomScale(self);

    WebCore::FloatSize unobscuredContentSize([self _contentRectForUserInteraction].size);
    double horizontalScale = unobscuredContentSize.width() * currentScale / targetRect.width();
    double verticalScale = unobscuredContentSize.height() * currentScale / targetRect.height();

    horizontalScale = std::min(std::max(horizontalScale, minimumScale), maximumScale);
    verticalScale = std::min(std::max(verticalScale, minimumScale), maximumScale);

    double targetScale = fitEntireRect ? std::min(horizontalScale, verticalScale) : horizontalScale;
    if (fabs(targetScale - currentScale) < maximumScaleFactorDeltaForPanScroll) {
        if ([self _scrollToRect:targetRect origin:origin minimumScrollDistance:minimumScrollDistance])
            return true;
    } else if (targetScale != currentScale) {
        [self _zoomToRect:targetRect atScale:targetScale origin:origin];
        return true;
    }
    
    return false;
}

#pragma mark - UIScrollViewDelegate

- (BOOL)usesStandardContentView
{
    return !_customContentView;
}

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView
{
    ASSERT(_scrollView == scrollView);

    if (_customContentView)
        return _customContentView.get();

    return _contentView.get();
}

- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view
{
    if (![self usesStandardContentView])
        return;

    if (scrollView.pinchGestureRecognizer.state == UIGestureRecognizerStateBegan)
        [_contentView willStartUserTriggeredZoom];
    [_contentView willStartZoomOrScroll];
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
    if (![self usesStandardContentView])
        return;

    if (scrollView.panGestureRecognizer.state == UIGestureRecognizerStateBegan)
        [_contentView willStartUserTriggeredScroll];
    [_contentView willStartZoomOrScroll];
}

- (void)_didFinishScrolling
{
    if (![self usesStandardContentView])
        return;

    [self _updateVisibleContentRects];
    [_contentView didFinishScrolling];
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
    // If we're decelerating, scroll offset will be updated when scrollViewDidFinishDecelerating: is called.
    if (!decelerate)
        [self _didFinishScrolling];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
    [self _didFinishScrolling];
}

- (void)scrollViewDidScrollToTop:(UIScrollView *)scrollView
{
    [self _didFinishScrolling];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    [self _updateVisibleContentRects];
}

- (void)scrollViewDidZoom:(UIScrollView *)scrollView
{
    [self _updateScrollViewBackground];
    [self _updateVisibleContentRects];
}

- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale
{
    ASSERT(scrollView == _scrollView);
    [self _updateVisibleContentRects];
    [_contentView didZoomToScale:scale];
}

- (void)_frameOrBoundsChanged
{
    CGRect bounds = self.bounds;

    if (!_hasStaticMinimumLayoutSize)
        [_contentView setMinimumLayoutSize:bounds.size];
    [_scrollView setFrame:bounds];
    [_contentView setMinimumSize:bounds.size];
    [_customContentView web_setMinimumSize:bounds.size];
    [self _updateVisibleContentRects];
}

// Unobscured content rect where the user can interact. When the keyboard is up, this should be the area above or bellow the keyboard, wherever there is enough space.
- (CGRect)_contentRectForUserInteraction
{
    // FIXME: handle split keyboard.
    UIEdgeInsets obscuredInsets = _obscuredInsets;
    obscuredInsets.bottom = std::max(_obscuredInsets.bottom, _keyboardVerticalOverlap);
    CGRect unobscuredRect = UIEdgeInsetsInsetRect(self.bounds, obscuredInsets);
    return [self convertRect:unobscuredRect toView:_contentView.get()];
}

- (void)_updateVisibleContentRects
{
    if (![self usesStandardContentView])
        return;

    CGRect fullViewRect = self.bounds;
    CGRect visibleRectInContentCoordinates = [self convertRect:fullViewRect toView:_contentView.get()];

    CGRect unobscuredRect = UIEdgeInsetsInsetRect(fullViewRect, _obscuredInsets);
    CGRect unobscuredRectInContentCoordinates = [self convertRect:unobscuredRect toView:_contentView.get()];

    CGFloat scaleFactor = contentZoomScale(self);

    BOOL isStableState = !(_isChangingObscuredInsetsInteractively || [_scrollView isDragging] || [_scrollView isDecelerating] || [_scrollView isZooming] || [_scrollView isZoomBouncing] || [_scrollView _isAnimatingZoom]);
    [_contentView didUpdateVisibleRect:visibleRectInContentCoordinates unobscuredRect:unobscuredRectInContentCoordinates scale:scaleFactor inStableState:isStableState];
}

- (void)_keyboardChangedWithInfo:(NSDictionary *)keyboardInfo adjustScrollView:(BOOL)adjustScrollView
{
    _keyboardVerticalOverlap = [[UIPeripheralHost sharedInstance] getVerticalOverlapForView:self usingKeyboardInfo:keyboardInfo];
    [self _updateVisibleContentRects];

    if (adjustScrollView)
        [_scrollView _adjustForAutomaticKeyboardInfo:keyboardInfo animated:YES lastAdjustment:&_lastAdjustmentForScroller];
}

- (void)_keyboardWillChangeFrame:(NSNotification *)notification
{
    if ([_contentView isAssistingNode])
        [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:YES];
}

- (void)_keyboardDidChangeFrame:(NSNotification *)notification
{
    [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:NO];
}

- (void)_keyboardWillShow:(NSNotification *)notification
{
    if ([_contentView isAssistingNode])
        [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:YES];
}

- (void)_keyboardWillHide:(NSNotification *)notification
{
    // Ignore keyboard will hide notifications sent during rotation. They're just there for
    // backwards compatibility reasons and processing the will hide notification would
    // temporarily screw up the the unobscured view area.
    if ([[UIPeripheralHost sharedInstance] rotationState])
        return;

    [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:YES];
}

- (void)setAllowsBackForwardNavigationGestures:(BOOL)allowsBackForwardNavigationGestures
{
    if (_allowsBackForwardNavigationGestures == allowsBackForwardNavigationGestures)
        return;

    _allowsBackForwardNavigationGestures = allowsBackForwardNavigationGestures;

    if (allowsBackForwardNavigationGestures) {
        if (!_gestureController) {
            _gestureController = std::make_unique<WebKit::ViewGestureController>(*_page);
            _gestureController->installSwipeHandler(self, [self scrollView]);
        }
    } else
        _gestureController = nullptr;

    _page->setShouldRecordNavigationSnapshots(allowsBackForwardNavigationGestures);
}

- (BOOL)allowsBackForwardNavigationGestures
{
    return _allowsBackForwardNavigationGestures;
}

#endif

#pragma mark OS X-specific methods

#if PLATFORM(MAC)

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize
{
    [_wkView setFrame:self.bounds];
}

- (void)setAllowsBackForwardNavigationGestures:(BOOL)allowsBackForwardNavigationGestures
{
    [_wkView setAllowsBackForwardNavigationGestures:allowsBackForwardNavigationGestures];
}

- (BOOL)allowsBackForwardNavigationGestures
{
    return [_wkView allowsBackForwardNavigationGestures];
}

- (void)setAllowsMagnification:(BOOL)allowsMagnification
{
    [_wkView setAllowsMagnification:allowsMagnification];
}

- (BOOL)allowsMagnification
{
    return [_wkView allowsMagnification];
}

- (void)setMagnification:(CGFloat)magnification
{
    [_wkView setMagnification:magnification];
}

- (CGFloat)magnification
{
    return [_wkView magnification];
}

- (void)setMagnification:(CGFloat)magnification centeredAtPoint:(CGPoint)point
{
    [_wkView setMagnification:magnification centeredAtPoint:NSPointFromCGPoint(point)];
}

#endif

@end

@implementation WKWebView (WKPrivate)

- (_WKRemoteObjectRegistry *)_remoteObjectRegistry
{
    if (!_remoteObjectRegistry) {
        _remoteObjectRegistry = adoptNS([[_WKRemoteObjectRegistry alloc] _initWithMessageSender:*_page]);
        _page->process().context().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->pageID(), [_remoteObjectRegistry remoteObjectRegistry]);
    }

    return _remoteObjectRegistry.get();
}

- (WKBrowsingContextHandle *)_handle
{
    return [[[WKBrowsingContextHandle alloc] _initWithPageID:_page->pageID()] autorelease];
}

- (_WKRenderingProgressEvents)_observedRenderingProgressEvents
{
    return _observedRenderingProgressEvents;
}

- (id <WKHistoryDelegatePrivate>)_historyDelegate
{
    return [_navigationState->historyDelegate().leakRef() autorelease];
}

- (void)_setHistoryDelegate:(id <WKHistoryDelegatePrivate>)historyDelegate
{
    _navigationState->setHistoryDelegate(historyDelegate);
}

- (NSURL *)_unreachableURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().unreachableURL()];
}

- (void)_loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL
{
    _page->loadAlternateHTMLString(string, [baseURL _web_originalDataAsWTFString], [unreachableURL _web_originalDataAsWTFString]);
}

- (WKNavigation *)_reload
{
    _page->reload(false);

    // FIXME: return a WKNavigation object.
    return nil;
}

- (NSArray *)_certificateChain
{
    if (WebKit::WebFrameProxy* mainFrame = _page->mainFrame())
        return mainFrame->certificateInfo() ? (NSArray *)mainFrame->certificateInfo()->certificateInfo().certificateChain() : nil;

    return nil;
}

- (NSURL *)_committedURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().url()];
}

- (NSString *)_applicationNameForUserAgent
{
    return _page->applicationNameForUserAgent();
}

- (void)_setApplicationNameForUserAgent:(NSString *)applicationNameForUserAgent
{
    _page->setApplicationNameForUserAgent(applicationNameForUserAgent);
}

- (NSString *)_customUserAgent
{
    return _page->customUserAgent();
}

- (void)_setCustomUserAgent:(NSString *)_customUserAgent
{
    _page->setCustomUserAgent(_customUserAgent);
}

- (pid_t)_webProcessIdentifier
{
    return _page->processIdentifier();
}

- (NSData *)_sessionState
{
    return [wrapper(*_page->sessionStateData(nullptr, nullptr).leakRef()) autorelease];
}

static void releaseNSData(unsigned char*, const void* data)
{
    [(NSData *)data release];
}

- (void)_restoreFromSessionState:(NSData *)sessionState
{
    [sessionState retain];
    _page->restoreFromSessionStateData(API::Data::createWithoutCopying((const unsigned char*)sessionState.bytes, sessionState.length, releaseNSData, sessionState).get());
}

- (void)_close
{
    _page->close();
}

- (BOOL)_privateBrowsingEnabled
{
    return [_configuration preferences]->_preferences->privateBrowsingEnabled();
}

- (void)_setPrivateBrowsingEnabled:(BOOL)privateBrowsingEnabled
{
    [_configuration preferences]->_preferences->setPrivateBrowsingEnabled(privateBrowsingEnabled);
}

- (BOOL)_allowsRemoteInspection
{
#if ENABLE(REMOTE_INSPECTOR)
    return _page->allowsRemoteInspection();
#else
    return NO;
#endif
}

- (void)_setAllowsRemoteInspection:(BOOL)allow
{
#if ENABLE(REMOTE_INSPECTOR)
    _page->setAllowsRemoteInspection(allow);
#endif
}

static inline WebCore::LayoutMilestones layoutMilestones(_WKRenderingProgressEvents events)
{
    WebCore::LayoutMilestones milestones = 0;

    if (events & _WKRenderingProgressEventFirstLayout)
        milestones |= WebCore::DidFirstLayout;

    if (events & _WKRenderingProgressEventFirstPaintWithSignificantArea)
        milestones |= WebCore::DidHitRelevantRepaintedObjectsAreaThreshold;

    return milestones;
}

- (void)_setObservedRenderingProgressEvents:(_WKRenderingProgressEvents)observedRenderingProgressEvents
{
    _observedRenderingProgressEvents = observedRenderingProgressEvents;
    _page->listenForLayoutMilestones(layoutMilestones(observedRenderingProgressEvents));
}

- (void)_runJavaScriptInMainFrame:(NSString *)scriptString
{
    _page->runJavaScriptInMainFrame(scriptString, WebKit::ScriptValueCallback::create([](bool, WebKit::WebSerializedScriptValue*){}));
}

- (_WKPaginationMode)_paginationMode
{
    switch (_page->paginationMode()) {
    case WebCore::Pagination::Unpaginated:
        return _WKPaginationModeUnpaginated;
    case WebCore::Pagination::LeftToRightPaginated:
        return _WKPaginationModeLeftToRight;
    case WebCore::Pagination::RightToLeftPaginated:
        return _WKPaginationModeRightToLeft;
    case WebCore::Pagination::TopToBottomPaginated:
        return _WKPaginationModeTopToBottom;
    case WebCore::Pagination::BottomToTopPaginated:
        return _WKPaginationModeBottomToTop;
    }

    ASSERT_NOT_REACHED();
    return _WKPaginationModeUnpaginated;
}

- (void)_setPaginationMode:(_WKPaginationMode)paginationMode
{
    WebCore::Pagination::Mode mode;
    switch (paginationMode) {
    case _WKPaginationModeUnpaginated:
        mode = WebCore::Pagination::Unpaginated;
        break;
    case _WKPaginationModeLeftToRight:
        mode = WebCore::Pagination::LeftToRightPaginated;
        break;
    case _WKPaginationModeRightToLeft:
        mode = WebCore::Pagination::RightToLeftPaginated;
        break;
    case _WKPaginationModeTopToBottom:
        mode = WebCore::Pagination::TopToBottomPaginated;
        break;
    case _WKPaginationModeBottomToTop:
        mode = WebCore::Pagination::BottomToTopPaginated;
        break;
    default:
        return;
    }

    _page->setPaginationMode(mode);
}

- (BOOL)_paginationBehavesLikeColumns
{
    return _page->paginationBehavesLikeColumns();
}

- (void)_setPaginationBehavesLikeColumns:(BOOL)behavesLikeColumns
{
    _page->setPaginationBehavesLikeColumns(behavesLikeColumns);
}

- (CGFloat)_pageLength
{
    return _page->pageLength();
}

- (void)_setPageLength:(CGFloat)pageLength
{
    _page->setPageLength(pageLength);
}

- (CGFloat)_gapBetweenPages
{
    return _page->gapBetweenPages();
}

- (void)_setGapBetweenPages:(CGFloat)gapBetweenPages
{
    _page->setGapBetweenPages(gapBetweenPages);
}

- (NSUInteger)_pageCount
{
    return _page->pageCount();
}

- (BOOL)_supportsTextZoom
{
    return _page->supportsTextZoom();
}

- (double)_textZoomFactor
{
    return _page->textZoomFactor();
}

- (void)_setTextZoomFactor:(double)zoomFactor
{
    _page->setTextZoomFactor(zoomFactor);
}

- (double)_pageZoomFactor
{
    return _page->pageZoomFactor();
}

- (void)_setPageZoomFactor:(double)zoomFactor
{
    _page->setPageZoomFactor(zoomFactor);
}

#pragma mark iOS-specific methods

#if PLATFORM(IOS)

- (CGSize)_minimumLayoutSizeOverride
{
    ASSERT(_hasStaticMinimumLayoutSize);
    return _minimumLayoutSizeOverride;
}

- (void)_setMinimumLayoutSizeOverride:(CGSize)minimumLayoutSizeOverride
{
    _hasStaticMinimumLayoutSize = YES;
    _minimumLayoutSizeOverride = minimumLayoutSizeOverride;
    [_contentView setMinimumLayoutSize:minimumLayoutSizeOverride];
}

- (UIEdgeInsets)_obscuredInsets
{
    return _obscuredInsets;
}

- (void)_setObscuredInsets:(UIEdgeInsets)obscuredInsets
{
    ASSERT(obscuredInsets.top >= 0);
    ASSERT(obscuredInsets.left >= 0);
    ASSERT(obscuredInsets.bottom >= 0);
    ASSERT(obscuredInsets.right >= 0);
    _obscuredInsets = obscuredInsets;
    [self _updateVisibleContentRects];
}

- (UIColor *)_pageExtendedBackgroundColor
{
    // This is deprecated.
    return nil;
}

- (void)_setBackgroundExtendsBeyondPage:(BOOL)backgroundExtends
{
    _page->setBackgroundExtendsBeyondPage(backgroundExtends);
}

- (BOOL)_backgroundExtendsBeyondPage
{
    return _page->backgroundExtendsBeyondPage();
}

- (void)_beginInteractiveObscuredInsetsChange
{
    ASSERT(!_isChangingObscuredInsetsInteractively);
    _isChangingObscuredInsetsInteractively = YES;
}

- (void)_endInteractiveObscuredInsetsChange
{
    ASSERT(_isChangingObscuredInsetsInteractively);
    _isChangingObscuredInsetsInteractively = NO;
    [self _updateVisibleContentRects];
}

- (void)_showInspectorIndication
{
    [_contentView setShowingInspectorIndication:YES];
}

- (void)_hideInspectorIndication
{
    [_contentView setShowingInspectorIndication:NO];
}

- (void)_snapshotRect:(CGRect)rectInViewCoordinates intoImageOfWidth:(CGFloat)imageWidth completionHandler:(void(^)(CGImageRef))completionHandler
{
    CGRect snapshotRectInContentCoordinates = [self convertRect:rectInViewCoordinates toView:_contentView.get()];
    CGFloat imageHeight = imageWidth / snapshotRectInContentCoordinates.size.width * snapshotRectInContentCoordinates.size.height;
    CGSize imageSize = CGSizeMake(imageWidth, imageHeight);

    void(^copiedCompletionHandler)(CGImageRef) = [completionHandler copy];
    _page->takeSnapshot(WebCore::enclosingIntRect(snapshotRectInContentCoordinates), WebCore::expandedIntSize(WebCore::FloatSize(imageSize)), WebKit::SnapshotOptionsExcludeDeviceScaleFactor, [copiedCompletionHandler](bool, const WebKit::ShareableBitmap::Handle& imageHandle) {
        if (imageHandle.isNull()) {
            copiedCompletionHandler(nullptr);
            [copiedCompletionHandler release];
            return;
        }

        RefPtr<WebKit::ShareableBitmap> bitmap = WebKit::ShareableBitmap::create(imageHandle, WebKit::SharedMemory::ReadOnly);

        if (!bitmap) {
            copiedCompletionHandler(nullptr);
            [copiedCompletionHandler release];
            return;
        }

        RetainPtr<CGImageRef> cgImage;
        cgImage = bitmap->makeCGImage();
        copiedCompletionHandler(cgImage.get());
        [copiedCompletionHandler release];
    });
}

- (BOOL)_isDisplayingPDF
{
    return [_customContentView isKindOfClass:[WKPDFView class]];
}

#else

#pragma mark - OS X-specific methods

- (NSColor *)_pageExtendedBackgroundColor
{
    WebCore::Color color = _page->pageExtendedBackgroundColor();
    if (!color.isValid())
        return nil;

    return nsColor(color);
}

- (BOOL)_drawsTransparentBackground
{
    return _page->drawsTransparentBackground();
}

- (void)_setDrawsTransparentBackground:(BOOL)drawsTransparentBackground
{
    _page->setDrawsTransparentBackground(drawsTransparentBackground);
}

- (void)_setTopContentInset:(CGFloat)contentInset
{
    _page->setTopContentInset(contentInset);
}

- (CGFloat)_topContentInset
{
    return _page->topContentInset();
}

#endif

@end

#if !TARGET_OS_IPHONE

@implementation WKWebView (WKIBActions)

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = item.action;

    if (action == @selector(goBack:))
        return !!_page->backForwardList().backItem();

    if (action == @selector(goForward:))
        return !!_page->backForwardList().forwardItem();

    if (action == @selector(stopLoading:)) {
        // FIXME: Return no if we're stopped.
        return YES;
    }

    if (action == @selector(reload:) || action == @selector(reloadFromOrigin:)) {
        // FIXME: Return no if we're loading.
        return YES;
    }

    return NO;
}

- (IBAction)goBack:(id)sender
{
    [self goBack];
}

- (IBAction)goForward:(id)sender
{
    [self goForward];
}

- (IBAction)reload:(id)sender
{
    [self reload];
}

- (IBAction)reloadFromOrigin:(id)sender
{
    [self reloadFromOrigin];
}

@end

#endif

#endif // WK_API_ENABLED
