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
#import "WKWebView.h"

#if WK_API_ENABLED

#import "WKNavigationInternal.h"
#import "WKProcessClass.h"
#import "WKWebViewConfiguration.h"
#import "WebPageProxy.h"
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import "WKContentViewInternal.h"
#import "WKScrollView.h"
#import <UIKit/UIScrollView_Private.h>
#import <UIKit/_UIWebViewportHandler.h>

static const float minWebViewScale = 0.25;
static const float maxWebViewScale = 5;
static _UIWebViewportConfiguration standardViewportConfiguration = { { UIWebViewportStandardViewportWidth, UIWebViewportGrowsAndShrinksToFitHeight }, UIWebViewportScaleForScalesToFit, minWebViewScale, maxWebViewScale, true
};

@interface WKWebView () <UIScrollViewDelegate, WKContentViewDelegate, _UIWebViewportHandlerDelegate>
@end
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS)
#import "WKViewInternal.h"
#endif

@implementation WKWebView {
    RetainPtr<WKWebViewConfiguration> _configuration;
    RefPtr<WebKit::WebPageProxy> _page;

#if PLATFORM(IOS)
    RetainPtr<WKScrollView> _scrollView;
    RetainPtr<WKContentView> _contentView;
    RetainPtr<_UIWebViewportHandler> _viewportHandler;

    BOOL _userHasChangedPageScale;
    BOOL _hasStaticMinimumLayoutSize;
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

    _viewportHandler = adoptNS([[_UIWebViewportHandler alloc] init]);
    [_viewportHandler setDelegate:self];

    [self _frameOrBoundsChanged];
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS)
    _wkView = [[WKView alloc] initWithFrame:bounds configuration:_configuration.get()];
    [self addSubview:_wkView.get()];
    _page = WebKit::toImpl([_wkView pageRef]);
#endif

    return self;
}

- (WKWebViewConfiguration *)configuration
{
    return [[_configuration copy] autorelease];
}

- (WKNavigation *)loadRequest:(NSURLRequest *)request
{
    _page->loadRequest(request);

    return [[[WKNavigation alloc] initWithRequest:request] autorelease];
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

- (void)contentView:(WKContentView *)contentView contentsSizeDidChange:(CGSize)newSize
{
    CGFloat zoomScale = [_scrollView zoomScale];
    CGSize contentsSizeInScrollViewCoordinates = CGSizeMake(newSize.width * zoomScale, newSize.height * zoomScale);
    [_scrollView setContentSize:contentsSizeInScrollViewCoordinates];

    [_viewportHandler update:^{
         [_viewportHandler setDocumentBounds:{CGPointZero, newSize}];
    }];
}

- (void)contentViewDidCommitLoadForMainFrame:(WKContentView *)contentView
{
    _userHasChangedPageScale = NO;

    WKContentType contentType = [_contentView contentType];
    [_viewportHandler update:^{
        [_viewportHandler clearWebKitViewportConfigurationFlags];
        _UIWebViewportConfiguration configuration = standardViewportConfiguration;

        if (contentType == PlainText) {
            CGFloat screenWidth = [[UIScreen mainScreen] bounds].size.width;
            configuration.size.width = screenWidth;
        } else if (contentType == WKContentType::Image)
            configuration.minimumScale = 0.01;

        [_viewportHandler resetViewportConfiguration:&configuration];
    }];
}

- (void)contentViewDidReceiveMobileDocType:(WKContentView *)contentView
{
    [_viewportHandler update:^{
        _UIWebViewportConfiguration configuration = standardViewportConfiguration;
        configuration.minimumScale = 1;
        configuration.size = CGSizeMake(320, UIWebViewportGrowsAndShrinksToFitHeight);
        [_viewportHandler resetViewportConfiguration:&configuration];
    }];
}

- (void)contentView:(WKContentView *)contentView didChangeViewportArgumentsSize:(CGSize)newSize initialScale:(float)initialScale minimumScale:(float)minimumScale maximumScale:(float)maximumScale allowsUserScaling:(float)allowsUserScaling
{
    [_viewportHandler update:^{
        [_viewportHandler applyWebKitViewportArgumentsSize:newSize initialScale:initialScale minimumScale:minimumScale maximumScale:maximumScale allowsUserScaling:allowsUserScaling];
    }];
}

#pragma mark - _UIWebViewportHandlerDelegate

- (void)viewportHandlerDidChangeScales:(_UIWebViewportHandler *)viewportHandler
{
    ASSERT(viewportHandler == _viewportHandler);
    [_scrollView setMinimumZoomScale:viewportHandler.minimumScale];
    [_scrollView setMaximumZoomScale:viewportHandler.maximumScale];
    [_scrollView setZoomEnabled:viewportHandler.allowsUserScaling];

    if (!_userHasChangedPageScale)
        [self _setDocumentScale:viewportHandler.initialScale];
    else {
        CGFloat currentScale = [_scrollView zoomScale];
        CGFloat validScale = std::max(std::min(currentScale, static_cast<CGFloat>(viewportHandler.maximumScale)), static_cast<CGFloat>(viewportHandler.minimumScale));
        [self _setDocumentScale:validScale];
    }
}

- (void)viewportHandler:(_UIWebViewportHandler *)viewportHandler didChangeViewportSize:(CGSize)newSize
{
    ASSERT(viewportHandler == _viewportHandler);
    [_contentView setViewportSize:newSize];
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
        _userHasChangedPageScale = YES;
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

    if (!_hasStaticMinimumLayoutSize) {
        [_viewportHandler update:^{
            [_viewportHandler setAvailableViewSize:bounds.size];
        }];
    }
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
