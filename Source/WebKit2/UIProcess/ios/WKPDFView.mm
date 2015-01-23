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
#import "WKPDFView.h"

#if PLATFORM(IOS)

#import "SessionState.h"
#import "WKPDFPageNumberIndicator.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <CorePDF/UIPDFDocument.h>
#import <CorePDF/UIPDFLinkAnnotation.h>
#import <CorePDF/UIPDFPage.h>
#import <CorePDF/UIPDFPageView.h>
#import <UIKit/UIScrollView_Private.h>
#import <WebCore/FloatRect.h>
#import <chrono>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

using namespace WebCore;

const CGFloat pdfPageMargin = 8;
const CGFloat pdfMinimumZoomScale = 1;
const CGFloat pdfMaximumZoomScale = 5;

const float overdrawHeightMultiplier = 1.5;

static const CGFloat smartMagnificationElementPadding = 0.05;

typedef struct {
    CGRect frame;
    RetainPtr<UIPDFPageView> view;
    RetainPtr<UIPDFPage> page;
} PDFPageInfo;

@interface WKPDFView ()
- (void)_resetZoomAnimated:(BOOL)animated;
@end

@implementation WKPDFView {
    RetainPtr<UIPDFDocument> _pdfDocument;
    RetainPtr<NSString> _suggestedFilename;
    RetainPtr<WKPDFPageNumberIndicator> _pageNumberIndicator;

    Vector<PDFPageInfo> _pages;
    unsigned _centerPageNumber;

    CGSize _minimumSize;
    CGSize _overlaidAccessoryViewsInset;
    WKWebView *_webView;
    UIScrollView *_scrollView;
    UIView *_fixedOverlayView;

    BOOL _isStartingZoom;
    BOOL _isPerformingSameDocumentNavigation;
}

- (instancetype)web_initWithFrame:(CGRect)frame webView:(WKWebView *)webView
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    self.backgroundColor = [UIColor grayColor];

    _webView = webView;

    _scrollView = webView.scrollView;
    [_scrollView setMinimumZoomScale:pdfMinimumZoomScale];
    [_scrollView setMaximumZoomScale:pdfMaximumZoomScale];
    [_scrollView setBackgroundColor:[UIColor grayColor]];

    return self;
}

- (void)dealloc
{
    [self _clearPages];
    [_pageNumberIndicator removeFromSuperview];
    [super dealloc];
}

- (NSString *)suggestedFilename
{
    return _suggestedFilename.get();
}

- (CGPDFDocumentRef)pdfDocument
{
    return [_pdfDocument CGDocument];
}

- (void)_clearPages
{
    for (auto& page : _pages) {
        [page.view removeFromSuperview];
        [page.view setDelegate:nil];
        [[page.view annotationController] setDelegate:nil];
    }
    
    _pages.clear();
}

- (void)web_setContentProviderData:(NSData *)data suggestedFilename:(NSString *)filename
{
    _suggestedFilename = adoptNS([filename copy]);

    [self _clearPages];

    RetainPtr<CGDataProvider> dataProvider = adoptCF(CGDataProviderCreateWithCFData((CFDataRef)data));
    RetainPtr<CGPDFDocumentRef> cgPDFDocument = adoptCF(CGPDFDocumentCreateWithProvider(dataProvider.get()));
    _pdfDocument = adoptNS([[UIPDFDocument alloc] initWithCGPDFDocument:cgPDFDocument.get()]);

    // FIXME: restore the scroll position and page scale if navigating from the back/forward list.

    [self _computePageAndDocumentFrames];
    [self _revalidateViews];
}

- (void)web_setMinimumSize:(CGSize)size
{
    _minimumSize = size;

    [self _computePageAndDocumentFrames];
    [self _revalidateViews];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    if (scrollView.isZoomBouncing || scrollView._isAnimatingZoom)
        return;

    [self _revalidateViews];

    if (!_isPerformingSameDocumentNavigation)
        [_pageNumberIndicator show];
}

- (void)_revalidateViews
{
    if (_isStartingZoom)
        return;

    CGRect targetRect = [_scrollView convertRect:_scrollView.bounds toView:self];

    // We apply overdraw after applying scale in order to avoid excessive
    // memory use caused by scaling the overdraw.
    CGRect targetRectWithOverdraw = CGRectInset(targetRect, 0, -targetRect.size.height * overdrawHeightMultiplier);
    CGRect targetRectForCenterPage = CGRectInset(targetRect, 0, targetRect.size.height / 2 - pdfPageMargin * 2);

    _centerPageNumber = 0;
    unsigned currentPage = 0;

    for (auto& pageInfo : _pages) {
        ++currentPage;

        if (!CGRectIntersectsRect(pageInfo.frame, targetRectWithOverdraw)) {
            [pageInfo.view removeFromSuperview];
            pageInfo.view = nullptr;
            continue;
        }

        if (!_centerPageNumber && CGRectIntersectsRect(pageInfo.frame, targetRectForCenterPage))
            _centerPageNumber = currentPage;

        if (pageInfo.view)
            continue;

        pageInfo.view = adoptNS([[UIPDFPageView alloc] initWithPage:pageInfo.page.get() tiledContent:YES]);
        [pageInfo.view setUseBackingLayer:YES];
        [pageInfo.view setDelegate:self];
        [[pageInfo.view annotationController] setDelegate:self];
        [self addSubview:pageInfo.view.get()];

        [pageInfo.view setFrame:pageInfo.frame];
        [pageInfo.view contentLayer].contentsScale = self.window.screen.scale;
    }

    [self _updatePageNumberIndicator];
}

- (CGPoint)_offsetForPageNumberIndicator
{
    UIEdgeInsets contentInset = [_webView _computedContentInset];
    return CGPointMake(contentInset.left, contentInset.top + _overlaidAccessoryViewsInset.height);
}

- (void)_updatePageNumberIndicator
{
    if (_isPerformingSameDocumentNavigation)
        return;

    if (!_pageNumberIndicator)
        _pageNumberIndicator = adoptNS([[WKPDFPageNumberIndicator alloc] initWithFrame:CGRectZero]);

    [_fixedOverlayView addSubview:_pageNumberIndicator.get()];

    [_pageNumberIndicator setCurrentPageNumber:_centerPageNumber];
    [_pageNumberIndicator moveToPoint:[self _offsetForPageNumberIndicator] animated:NO];
}

- (void)web_setOverlaidAccessoryViewsInset:(CGSize)inset
{
    _overlaidAccessoryViewsInset = inset;
    [_pageNumberIndicator moveToPoint:[self _offsetForPageNumberIndicator] animated:YES];
}

- (void)web_computedContentInsetDidChange
{
    [self _updatePageNumberIndicator];
}

- (void)web_setFixedOverlayView:(UIView *)fixedOverlayView
{
    _fixedOverlayView = fixedOverlayView;

    if (_pageNumberIndicator)
        [_fixedOverlayView addSubview:_pageNumberIndicator.get()];
}

- (void)web_didSameDocumentNavigation:(WKSameDocumentNavigationType)navigationType
{
    // Check for kWKSameDocumentNavigationSessionStatePop instead of kWKSameDocumentNavigationAnchorNavigation since the
    // latter is only called once when navigating to the same anchor in succession. If the user navigates to a page
    // then scrolls back and clicks on the same link a second time, we want to scroll again.
    if (navigationType != kWKSameDocumentNavigationSessionStatePop)
        return;

    // FIXME: restore the scroll position and page scale if navigating back from a fragment.

    NSString *fragment = _webView.URL.fragment;
    if (![fragment hasPrefix:@"page"])
        return;

    NSInteger pageIndex = [[fragment substringFromIndex:4] integerValue] - 1;
    if (pageIndex < 0 || static_cast<std::size_t>(pageIndex) >= _pages.size())
        return;

    _isPerformingSameDocumentNavigation = YES;

    [_pageNumberIndicator hide];
    [self _resetZoomAnimated:NO];

    // Ensure that the page margin is visible below the content inset.
    const CGFloat verticalOffset = _pages[pageIndex].frame.origin.y - _webView._computedContentInset.top - pdfPageMargin;
    [_scrollView setContentOffset:CGPointMake(_scrollView.contentOffset.x, verticalOffset) animated:NO];

    _isPerformingSameDocumentNavigation = NO;
}

- (void)_computePageAndDocumentFrames
{
    NSUInteger pageCount = [_pdfDocument numberOfPages];
    [_pageNumberIndicator setPageCount:pageCount];
    
    [self _clearPages];

    _pages.reserveCapacity(pageCount);

    CGRect pageFrame = CGRectMake(0, 0, _minimumSize.width, _minimumSize.height);
    for (NSUInteger pageNumber = 0; pageNumber < pageCount; ++pageNumber) {
        UIPDFPage *page = [_pdfDocument pageAtIndex:pageNumber];
        if (!page)
            continue;

        CGSize pageSize = [page cropBoxAccountForRotation].size;
        pageFrame.size.height = pageSize.height / pageSize.width * pageFrame.size.width;
        CGRect pageFrameWithMarginApplied = CGRectInset(pageFrame, pdfPageMargin, pdfPageMargin);

        PDFPageInfo pageInfo;
        pageInfo.page = page;
        pageInfo.frame = pageFrameWithMarginApplied;
        _pages.append(pageInfo);
        pageFrame.origin.y += pageFrame.size.height - pdfPageMargin;
    }

    CGFloat scale = _scrollView.zoomScale;
    CGRect newFrame = [self frame];
    newFrame.size.width = _minimumSize.width * scale;
    newFrame.size.height = std::max(pageFrame.origin.y + pdfPageMargin, _minimumSize.height) * scale;

    [self setFrame:newFrame];
    [_scrollView setContentSize:newFrame.size];
}

- (void)_resetZoomAnimated:(BOOL)animated
{
    _isStartingZoom = YES;

    CGRect scrollViewBounds = _scrollView.bounds;
    CGPoint centerOfPageInDocumentCoordinates = [_scrollView convertPoint:CGPointMake(CGRectGetMidX(scrollViewBounds), CGRectGetMidY(scrollViewBounds)) toView:self];
    [_webView _zoomOutWithOrigin:centerOfPageInDocumentCoordinates animated:animated];

    _isStartingZoom = NO;
}

#pragma mark UIPDFPageViewDelegate

- (void)zoom:(UIPDFPageView *)pageView to:(CGRect)targetRect atPoint:(CGPoint)origin kind:(UIPDFObjectKind)kind
{
    _isStartingZoom = YES;

    BOOL isImage = kind == kUIPDFObjectKindGraphic;

    if (!isImage)
        targetRect = CGRectInset(targetRect, smartMagnificationElementPadding * targetRect.size.width, smartMagnificationElementPadding * targetRect.size.height);

    CGRect rectInDocumentCoordinates = [pageView convertRect:targetRect toView:self];
    CGPoint originInDocumentCoordinates = [pageView convertPoint:origin toView:self];

    [_webView _zoomToRect:rectInDocumentCoordinates withOrigin:originInDocumentCoordinates fitEntireRect:isImage minimumScale:pdfMinimumZoomScale maximumScale:pdfMaximumZoomScale minimumScrollDistance:0];

    _isStartingZoom = NO;
}

- (void)resetZoom:(UIPDFPageView *)pageView
{
    [self _resetZoomAnimated:YES];
}

#pragma mark UIPDFAnnotationControllerDelegate

- (void)annotation:(UIPDFAnnotation *)annotation wasTouchedAtPoint:(CGPoint)point controller:(UIPDFAnnotationController *)controller
{
    ASSERT(isMainThread());

    if (![annotation isKindOfClass:[UIPDFLinkAnnotation class]])
        return;

    UIPDFLinkAnnotation *linkAnnotation = (UIPDFLinkAnnotation *)annotation;
    String urlString;
    if (NSURL *url = linkAnnotation.url)
        urlString = url.absoluteString;
    else if (NSUInteger pageNumber = linkAnnotation.pageNumber) {
        urlString = ASCIILiteral("#page");
        urlString.append(String::number(pageNumber));
    }

    if (urlString.isEmpty())
        return;

    CGPoint documentPoint = [controller.pageView convertPoint:point toView:self];
    CGPoint screenPoint = [self.window convertPoint:[self convertPoint:documentPoint toView:nil] toWindow:nil];
    static const int64_t dispatchOffset = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(200)).count();
    RetainPtr<WKWebView> retainedWebView = _webView;

    // Call navigateToURLWithSimulatedClick() on a delay so that a tap highlight can be shown.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, dispatchOffset), dispatch_get_main_queue(), ^ {
        retainedWebView->_page->navigateToURLWithSimulatedClick(urlString, roundedIntPoint(documentPoint), roundedIntPoint(screenPoint));
    });
}

@end

#endif /* PLATFORM(IOS) */
