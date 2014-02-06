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
#import "WKNavigationDelegate.h"
#import "WKNavigationInternal.h"
#import "WKProcessClass.h"
#import "WKWebViewConfiguration.h"
#import "WebBackForwardList.h"
#import "WebPageProxy.h"
#import <WebKit2/RemoteLayerTreeTransaction.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import "WKScrollView.h"
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS)
#import "WKViewInternal.h"
#endif

@implementation WKWebView {
    RetainPtr<WKWebViewConfiguration> _configuration;
    std::unique_ptr<WebKit::NavigationState> _navigationState;

#if PLATFORM(IOS)
    RetainPtr<WKScrollView> _scrollView;
    RetainPtr<WKContentView> _contentView;

    BOOL _isWaitingForNewLayerTreeAfterDidCommitLoad;
    BOOL _hasStaticMinimumLayoutSize;
    CGSize _minimumLayoutSizeOverride;
#endif
#if PLATFORM(MAC) && !PLATFORM(IOS)
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

    if (![_configuration processClass])
        [_configuration setProcessClass:adoptNS([[WKProcessClass alloc] init]).get()];

    CGRect bounds = self.bounds;

#if PLATFORM(IOS)
    _scrollView = adoptNS([[WKScrollView alloc] initWithFrame:bounds]);
    [_scrollView setInternalDelegate:self];
    [_scrollView setBouncesZoom:YES];

    [self addSubview:_scrollView.get()];

    _contentView = adoptNS([[WKContentView alloc] initWithFrame:bounds configuration:_configuration.get()]);
    _page = _contentView->_page;
    [_contentView setDelegate:self];
    [_contentView layer].anchorPoint = CGPointZero;
    [_contentView setFrame:bounds];
    [_scrollView addSubview:_contentView.get()];

    [self _frameOrBoundsChanged];
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS)
    _wkView = [[WKView alloc] initWithFrame:bounds configuration:_configuration.get()];
    [self addSubview:_wkView.get()];
    _page = WebKit::toImpl([_wkView pageRef]);
#endif

    _navigationState = std::make_unique<WebKit::NavigationState>(self);
    _page->setPolicyClient(_navigationState->createPolicyClient());
    _page->setLoaderClient(_navigationState->createLoaderClient());

    return self;
}

- (WKWebViewConfiguration *)configuration
{
    return [[_configuration copy] autorelease];
}

- (id <WKNavigationDelegate>)navigationDelegate
{
    return [_navigationState->navigationDelegate().leakRef() autorelease];
}

- (void)setNavigationDelegate:(id <WKNavigationDelegate>)navigationDelegate
{
    _navigationState->setNavigationDelegate(navigationDelegate);
}

- (WKNavigation *)loadRequest:(NSURLRequest *)request
{
    uint64_t navigationID = _page->loadRequest(request);
    auto navigation = _navigationState->createLoadRequestNavigation(navigationID, request);

    return [navigation.leakRef() autorelease];
}

- (NSString *)title
{
    return _page->pageLoadState().title();
}

- (BOOL)isLoading
{
    return _page->pageLoadState().isLoading();
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

// FIXME: This should return a WKNavigation object.
- (void)goBack
{
    _page->goBack();
}

// FIXME: This should return a WKNavigation object.
- (void)goForward
{
    _page->goForward();
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

#pragma mark WKContentViewDelegate

- (void)contentViewDidCommitLoadForMainFrame:(WKContentView *)contentView
{
    _isWaitingForNewLayerTreeAfterDidCommitLoad = YES;
}

- (void)contentView:(WKContentView *)contentView didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    [_scrollView setMinimumZoomScale:layerTreeTransaction.minimumScaleFactor()];
    [_scrollView setMaximumZoomScale:layerTreeTransaction.maximumScaleFactor()];
    [_scrollView setZoomEnabled:layerTreeTransaction.allowsUserScaling()];
    if (![_scrollView isZooming] && ![_scrollView isZoomBouncing])
        [_scrollView setZoomScale:layerTreeTransaction.pageScaleFactor()];

    if (_isWaitingForNewLayerTreeAfterDidCommitLoad) {
        UIEdgeInsets inset = [_scrollView contentInset];
        [_scrollView setContentOffset:CGPointMake(-inset.left, -inset.top)];
        _isWaitingForNewLayerTreeAfterDidCommitLoad = NO;
    }
    
}

#pragma mark - UIScrollViewDelegate

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView
{
    ASSERT(_scrollView == scrollView);
    return _contentView.get();
}

- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view
{
    if (scrollView.pinchGestureRecognizer.state == UIGestureRecognizerStateBegan)
        [_contentView willStartUserTriggeredZoom];
    [_contentView willStartZoomOrScroll];
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
    [_contentView willStartZoomOrScroll];
}

- (void)_didFinishScroll
{
    CGPoint position = [_scrollView convertPoint:[_scrollView contentOffset] toView:_contentView.get()];
    [_contentView didFinishScrollTo:position];
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
    // If we're decelerating, scroll offset will be updated when scrollViewDidFinishDecelerating: is called.
    if (!decelerate)
        [self _didFinishScroll];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
    [self _didFinishScroll];
}

- (void)scrollViewDidScrollToTop:(UIScrollView *)scrollView
{
    [self _didFinishScroll];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    CGPoint position = [_scrollView convertPoint:[_scrollView contentOffset] toView:_contentView.get()];
    [_contentView didScrollTo:position];
}

- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale
{
    ASSERT(scrollView == _scrollView);
    [_contentView didZoomToScale:scale];
}

- (void)_frameOrBoundsChanged
{
    CGRect bounds = self.bounds;

    if (!_hasStaticMinimumLayoutSize)
        [_contentView setMinimumLayoutSize:bounds.size];
    [_scrollView setFrame:bounds];
    [_contentView setMinimumSize:bounds.size];
}

- (void)_setDocumentScale:(CGFloat)newScale
{
    CGPoint contentOffsetInDocumentCoordinates = [_scrollView convertPoint:[_scrollView contentOffset] toView:_contentView.get()];

    [_scrollView setZoomScale:newScale];
    [_contentView didZoomToScale:newScale];

    CGPoint contentOffset = [_scrollView convertPoint:contentOffsetInDocumentCoordinates fromView:_contentView.get()];
    [_scrollView setContentOffset:contentOffset];
}

#pragma mark Private API

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

#endif

#pragma mark OS X-specific methods

#if PLATFORM(MAC) && !PLATFORM(IOS)

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize
{
    [_wkView setFrame:self.bounds];
}

#endif

@end

#endif // WK_API_ENABLED
