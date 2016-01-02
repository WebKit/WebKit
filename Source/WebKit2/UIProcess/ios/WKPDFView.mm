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

#import "APIFindClient.h"
#import "APIUIClient.h"
#import "ApplicationStateTracker.h"
#import "CorePDFSPI.h"
#import "SessionState.h"
#import "UIKitSPI.h"
#import "WKPDFPageNumberIndicator.h"
#import "WKWebViewInternal.h"
#import "WeakObjCPtr.h"
#import "WebPageProxy.h"
#import "_WKFindDelegate.h"
#import <MobileCoreServices/UTCoreTypes.h>
#import <WebCore/FloatRect.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/WebCoreNSURLExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

using namespace WebCore;
using namespace WebKit;

const CGFloat pdfPageMargin = 8;
const CGFloat pdfMinimumZoomScale = 1;
const CGFloat pdfMaximumZoomScale = 5;

const CGFloat passwordEntryFieldPadding = 10;

const float overdrawHeightMultiplier = 1.5;

static const CGFloat smartMagnificationElementPadding = 0.05;

typedef struct {
    CGRect frame;
    RetainPtr<UIPDFPageView> view;
    RetainPtr<UIPDFPage> page;
    unsigned index;
} PDFPageInfo;

@interface WKPDFView ()
- (void)_resetZoomAnimated:(BOOL)animated;
@end

@implementation WKPDFView {
    RetainPtr<CGPDFDocumentRef> _cgPDFDocument;
    RetainPtr<UIPDFDocument> _pdfDocument;
    RetainPtr<NSString> _suggestedFilename;
    RetainPtr<WKPDFPageNumberIndicator> _pageNumberIndicator;

    RetainPtr<UIDocumentPasswordView> _passwordView;

    Vector<PDFPageInfo> _pages;
    unsigned _centerPageNumber;

    CGSize _minimumSize;
    CGSize _overlaidAccessoryViewsInset;
    WKWebView *_webView;
    UIScrollView *_scrollView;
    UIView *_fixedOverlayView;

    BOOL _isStartingZoom;
    BOOL _isPerformingSameDocumentNavigation;

    RetainPtr<WKActionSheetAssistant> _actionSheetAssistant;
    WebKit::InteractionInformationAtPosition _positionInformation;

    unsigned _currentFindPageIndex;
    unsigned _currentFindMatchIndex;
    RetainPtr<UIPDFSelection> _currentFindSelection;

    RetainPtr<NSString> _cachedFindString;
    Vector<RetainPtr<UIPDFSelection>> _cachedFindMatches;
    unsigned _cachedFindMaximumCount;
    _WKFindOptions _cachedFindOptionsAffectingResults;

    std::atomic<unsigned> _nextComputeMatchesOperationID;
    RetainPtr<NSString> _nextCachedFindString;
    unsigned _nextCachedFindMaximumCount;
    _WKFindOptions _nextCachedFindOptionsAffectingResults;

    dispatch_queue_t _findQueue;

    RetainPtr<UIWKSelectionAssistant> _webSelectionAssistant;

    std::unique_ptr<ApplicationStateTracker> _applicationStateTracker;
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

    _actionSheetAssistant = adoptNS([[WKActionSheetAssistant alloc] initWithView:self]);
    [_actionSheetAssistant setDelegate:self];

    _findQueue = dispatch_queue_create("com.apple.WebKit.WKPDFViewComputeMatchesQueue", DISPATCH_QUEUE_SERIAL);

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [self _clearPages];
    [_pageNumberIndicator removeFromSuperview];
    dispatch_release(_findQueue);
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

- (void)_didLoadPDFDocument
{
    _pdfDocument = adoptNS([[UIPDFDocument alloc] initWithCGPDFDocument:_cgPDFDocument.get()]);

    // FIXME: Restore the scroll position and page scale if navigating from the back/forward list.

    [self _computePageAndDocumentFrames];
    [self _revalidateViews];
    [self _scrollToFragment:_webView.URL.fragment];
}

- (void)web_setContentProviderData:(NSData *)data suggestedFilename:(NSString *)filename
{
    _suggestedFilename = adoptNS([filename copy]);

    [self _clearPages];

    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateWithCFData((CFDataRef)data));
    _cgPDFDocument = adoptCF(CGPDFDocumentCreateWithProvider(dataProvider.get()));

    if (!_cgPDFDocument)
        return;

    if (CGPDFDocumentIsUnlocked(_cgPDFDocument.get())) {
        [self _didLoadPDFDocument];
        return;
    }

    [self _showPasswordEntryField];
}

- (void)web_setMinimumSize:(CGSize)size
{
    if (_passwordView) {
        [self _updatePasswordEntryField];
        return;
    }

    _minimumSize = size;

    CGFloat oldDocumentLeftFraction = 0;
    CGFloat oldDocumentTopFraction = 0;
    CGSize contentSize = _scrollView.contentSize;
    if (contentSize.width && contentSize.height) {
        CGPoint contentOffset = _scrollView.contentOffset;
        UIEdgeInsets contentInset = _scrollView.contentInset;
        oldDocumentLeftFraction = (contentOffset.x + contentInset.left) / contentSize.width;
        oldDocumentTopFraction = (contentOffset.y + contentInset.top) / contentSize.height;
    }

    [self _computePageAndDocumentFrames];

    CGSize newContentSize = _scrollView.contentSize;
    UIEdgeInsets contentInset = _scrollView.contentInset;
    [_scrollView setContentOffset:CGPointMake((oldDocumentLeftFraction * newContentSize.width) - contentInset.left, (oldDocumentTopFraction * newContentSize.height) - contentInset.top) animated:NO];

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

- (void)_ensureViewForPage:(PDFPageInfo&)pageInfo
{
    if (pageInfo.view)
        return;

    pageInfo.view = adoptNS([[UIPDFPageView alloc] initWithPage:pageInfo.page.get() tiledContent:YES]);
    [pageInfo.view setUseBackingLayer:YES];
    [pageInfo.view setDelegate:self];
    [[pageInfo.view annotationController] setDelegate:self];
    [self addSubview:pageInfo.view.get()];

    [pageInfo.view setFrame:pageInfo.frame];
    [pageInfo.view contentLayer].contentsScale = self.window.screen.scale;
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

    for (auto& pageInfo : _pages) {
        if (!CGRectIntersectsRect(pageInfo.frame, targetRectWithOverdraw) && pageInfo.index != _currentFindPageIndex) {
            [pageInfo.view removeFromSuperview];
            pageInfo.view = nullptr;
            continue;
        }

        if (!_centerPageNumber && CGRectIntersectsRect(pageInfo.frame, targetRectForCenterPage))
            _centerPageNumber = pageInfo.index + 1;

        [self _ensureViewForPage:pageInfo];
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

- (void)_scrollToFragment:(NSString *)fragment
{
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

- (void)web_didSameDocumentNavigation:(WKSameDocumentNavigationType)navigationType
{
    // Check for kWKSameDocumentNavigationSessionStatePop instead of kWKSameDocumentNavigationAnchorNavigation since the
    // latter is only called once when navigating to the same anchor in succession. If the user navigates to a page
    // then scrolls back and clicks on the same link a second time, we want to scroll again.
    if (navigationType != kWKSameDocumentNavigationSessionStatePop)
        return;

    // FIXME: restore the scroll position and page scale if navigating back from a fragment.

    [self _scrollToFragment:_webView.URL.fragment];
}

- (void)_computePageAndDocumentFrames
{
    if (_passwordView)
        return;

    NSUInteger pageCount = [_pdfDocument numberOfPages];
    [_pageNumberIndicator setPageCount:pageCount];
    
    [self _clearPages];

    _pages.reserveCapacity(pageCount);

    CGRect pageFrame = CGRectMake(0, 0, _minimumSize.width, _minimumSize.height);
    for (NSUInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
        UIPDFPage *page = [_pdfDocument pageAtIndex:pageIndex];
        if (!page)
            continue;

        CGSize pageSize = [page cropBoxAccountForRotation].size;
        pageFrame.size.height = pageSize.height / pageSize.width * pageFrame.size.width;
        CGRect pageFrameWithMarginApplied = CGRectInset(pageFrame, pdfPageMargin, pdfPageMargin);

        PDFPageInfo pageInfo;
        pageInfo.page = page;
        pageInfo.frame = pageFrameWithMarginApplied;
        pageInfo.index = pageIndex;
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

- (void)_highlightLinkAnnotation:(UIPDFLinkAnnotation *)linkAnnotation forDuration:(NSTimeInterval)duration completionHandler:(void (^)(void))completionHandler
{
    static const CGFloat highlightBorderRadius = 3;
    static const CGFloat highlightColorComponent = 26.0 / 255;
    static UIColor *highlightColor = [[UIColor alloc] initWithRed:highlightColorComponent green:highlightColorComponent blue:highlightColorComponent alpha:0.3];

    UIPDFPageView *pageView = linkAnnotation.annotationController.pageView;
    CGRect highlightViewFrame = [self convertRect:[pageView convertRectFromPDFPageSpace:linkAnnotation.Rect] fromView:pageView];
    RetainPtr<_UIHighlightView> highlightView = adoptNS([[_UIHighlightView alloc] initWithFrame:CGRectInset(highlightViewFrame, -highlightBorderRadius, -highlightBorderRadius)]);
    [highlightView setOpaque:NO];
    [highlightView setCornerRadius:highlightBorderRadius];
    [highlightView setColor:highlightColor];

    ASSERT(isMainThread());
    [self addSubview:highlightView.get()];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, duration * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        [highlightView removeFromSuperview];
        completionHandler();
    });
}

- (NSURL *)_URLForLinkAnnotation:(UIPDFLinkAnnotation *)linkAnnotation
{
    NSURL *documentURL = _webView.URL;

    if (NSURL *url = linkAnnotation.url)
        return [NSURL URLWithString:url.relativeString relativeToURL:documentURL];

    if (NSUInteger pageNumber = linkAnnotation.pageNumber) {
        String anchorString = ASCIILiteral("#page");
        anchorString.append(String::number(pageNumber));
        return [NSURL URLWithString:anchorString relativeToURL:documentURL];
    }

    return nil;
}

#pragma mark Find-in-Page

static NSStringCompareOptions stringCompareOptions(_WKFindOptions options)
{
    NSStringCompareOptions findOptions = 0;
    if (options & _WKFindOptionsCaseInsensitive)
        findOptions |= NSCaseInsensitiveSearch;
    if (options & _WKFindOptionsBackwards)
        findOptions |= NSBackwardsSearch;
    return findOptions;
}

- (void)_computeMatchesForString:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount completionHandler:(void (^)(BOOL success))completionHandler
{
    if (!_pdfDocument) {
        completionHandler(NO);
        return;
    }

    _WKFindOptions optionsAffectingResults = options & ~_WKFindOptionsIrrelevantForBatchResults;

    // If this search is equivalent to the currently cached search, bail and call the completion handler, because the existing cached results are valid.
    if (!_cachedFindMatches.isEmpty() && [_cachedFindString isEqualToString:string] && _cachedFindOptionsAffectingResults == optionsAffectingResults && _cachedFindMaximumCount == maxCount) {
        // Also, cancel any running search, because it will eventually replace the now-valid results.
        ++_nextComputeMatchesOperationID;

        completionHandler(YES);
        return;
    }

    // If this search is equivalent to the currently running asynchronous search, bail as if this search were cancelled; the original search's completion handler will eventually fire.
    if ([_nextCachedFindString isEqualToString:string] && _nextCachedFindOptionsAffectingResults == optionsAffectingResults && _nextCachedFindMaximumCount == maxCount) {
        completionHandler(NO);
        return;
    }

    NSStringCompareOptions findOptions = stringCompareOptions(optionsAffectingResults);

    Vector<PDFPageInfo> pages = _pages;

    unsigned computeMatchesOperationID = ++_nextComputeMatchesOperationID;
    _nextCachedFindString = string;
    _nextCachedFindOptionsAffectingResults = optionsAffectingResults;
    _nextCachedFindMaximumCount = maxCount;

    RetainPtr<WKPDFView> retainedSelf = self;
    typeof(completionHandler) completionHandlerCopy = Block_copy(completionHandler);

    dispatch_async(_findQueue, [pages, string, findOptions, optionsAffectingResults, maxCount, computeMatchesOperationID, retainedSelf, completionHandlerCopy] {
        Vector<RetainPtr<UIPDFSelection>> matches;

        for (unsigned pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
            UIPDFSelection *match = nullptr;
            while ((match = [pages[pageIndex].page findString:string fromSelection:match options:findOptions])) {
                matches.append(match);
                if (matches.size() > maxCount)
                    goto maxCountExceeded;

                // If we've enqueued another search, cancel this one.
                if (retainedSelf->_nextComputeMatchesOperationID != computeMatchesOperationID) {
                    dispatch_async(dispatch_get_main_queue(), [completionHandlerCopy] {
                        completionHandlerCopy(NO);
                        Block_release(completionHandlerCopy);
                    });
                    return;
                }
            };
        }

    maxCountExceeded:
        dispatch_async(dispatch_get_main_queue(), [computeMatchesOperationID, string, optionsAffectingResults, maxCount, matches, completionHandlerCopy, retainedSelf] {

            // If another search has been enqueued in the meantime, ignore this result.
            if (retainedSelf->_nextComputeMatchesOperationID != computeMatchesOperationID) {
                Block_release(completionHandlerCopy);
                return;
            }

            retainedSelf->_cachedFindString = string;
            retainedSelf->_cachedFindOptionsAffectingResults = optionsAffectingResults;
            retainedSelf->_cachedFindMaximumCount = maxCount;
            retainedSelf->_cachedFindMatches = matches;

            retainedSelf->_nextCachedFindString = nil;

            completionHandlerCopy(YES);
            Block_release(completionHandlerCopy);
        });
    });
}

- (void)web_countStringMatches:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
    RefPtr<WebKit::WebPageProxy> page = _webView->_page;
    [self _computeMatchesForString:string options:options maxCount:maxCount completionHandler:^(BOOL success) {
        if (!success)
            return;
        page->findClient().didCountStringMatches(page.get(), string, _cachedFindMatches.size());
    }];
}

- (void)_didFindMatch:(UIPDFSelection *)match
{
    for (auto& pageInfo : _pages) {
        if (pageInfo.page == match.page) {
            [self _ensureViewForPage:pageInfo];

            [pageInfo.view highlightSearchSelection:match animated:NO];

            _currentFindPageIndex = pageInfo.index;
            _currentFindSelection = match;

            CGRect zoomRect = [pageInfo.view convertRectFromPDFPageSpace:match.bounds];
            [self zoom:pageInfo.view.get() to:zoomRect atPoint:CGPointZero kind:kUIPDFObjectKindText];

            return;
        }
    }
}

- (void)web_findString:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
    if (_currentFindSelection)
        [_pages[_currentFindPageIndex].view clearSearchHighlights];

    RetainPtr<UIPDFSelection> previousFindSelection = _currentFindSelection;
    unsigned previousFindPageIndex = 0;
    if (previousFindSelection) {
        previousFindPageIndex = _currentFindPageIndex;
        if (![_cachedFindString isEqualToString:string]) {
            NSUInteger location = [_currentFindSelection startIndex];
            if (location)
                previousFindSelection = adoptNS([[UIPDFSelection alloc] initWithPage:[_currentFindSelection page] fromIndex:location - 1 toIndex:location]);
        }
    }

    NSStringCompareOptions findOptions = stringCompareOptions(options);
    bool backwards = (options & _WKFindOptionsBackwards);
    RefPtr<WebKit::WebPageProxy> page = _webView->_page;

    [self _computeMatchesForString:string options:options maxCount:maxCount completionHandler:^(BOOL success) {
        if (!success)
            return;

        unsigned pageIndex = previousFindPageIndex;
        for (unsigned i = 0; i < _pages.size(); ++i) {
            UIPDFSelection *match = [_pages[pageIndex].page findString:string fromSelection:(pageIndex == previousFindPageIndex ? previousFindSelection.get() : nil) options:findOptions];

            if (!match) {
                if (!pageIndex && backwards)
                    pageIndex = _pages.size() - 1;
                else if (pageIndex == _pages.size() - 1 && !backwards)
                    pageIndex = 0;
                else
                    pageIndex += backwards ? -1 : 1;
                continue;
            }

            [self _didFindMatch:match];

            if (_cachedFindMatches.size() <= maxCount) {
                _currentFindMatchIndex = 0;
                for (const auto& knownMatch : _cachedFindMatches) {
                    if (match.stringRange.location == [knownMatch stringRange].location && match.stringRange.length == [knownMatch stringRange].length) {
                        page->findClient().didFindString(page.get(), string, { }, _cachedFindMatches.size(), _currentFindMatchIndex);
                        break;
                    }
                    _currentFindMatchIndex++;
                }
            }

            return;
        }
        
        page->findClient().didFailToFindString(page.get(), string);
    }];
}

- (void)web_hideFindUI
{
    if (_currentFindSelection)
        [_pages[_currentFindPageIndex].view clearSearchHighlights];

    _currentFindSelection = nullptr;
    _cachedFindString = nullptr;
    _cachedFindMatches.clear();
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
    if (![annotation isKindOfClass:[UIPDFLinkAnnotation class]])
        return;

    UIPDFLinkAnnotation *linkAnnotation = (UIPDFLinkAnnotation *)annotation;
    RetainPtr<NSURL> url = [self _URLForLinkAnnotation:linkAnnotation];
    if (!url)
        return;

    CGPoint documentPoint = [controller.pageView convertPoint:point toView:self];
    CGPoint screenPoint = [self.window convertPoint:[self convertPoint:documentPoint toView:nil] toWindow:nil];
    RetainPtr<WKWebView> retainedWebView = _webView;

    [self _highlightLinkAnnotation:linkAnnotation forDuration:.2 completionHandler:^{
        retainedWebView->_page->navigateToPDFLinkWithSimulatedClick([url absoluteString], roundedIntPoint(documentPoint), roundedIntPoint(screenPoint));
    }];
}

- (void)annotation:(UIPDFAnnotation *)annotation isBeingPressedAtPoint:(CGPoint)point controller:(UIPDFAnnotationController *)controller
{
    if (![annotation isKindOfClass:[UIPDFLinkAnnotation class]])
        return;

    UIPDFLinkAnnotation *linkAnnotation = (UIPDFLinkAnnotation *)annotation;
    NSURL *url = [self _URLForLinkAnnotation:linkAnnotation];
    if (!url)
        return;

    _positionInformation.url = url.absoluteString;
    _positionInformation.point = roundedIntPoint([controller.pageView convertPoint:point toView:self]);
    _positionInformation.bounds = roundedIntRect([self convertRect:[controller.pageView convertRectFromPDFPageSpace:annotation.Rect] fromView:controller.pageView]);

    [self _highlightLinkAnnotation:linkAnnotation forDuration:.75 completionHandler:^{
        [_actionSheetAssistant showLinkSheet];
    }];
}

#pragma mark WKActionSheetAssistantDelegate

- (const WebKit::InteractionInformationAtPosition&)positionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    return _positionInformation;
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant performAction:(WebKit::SheetAction)action
{
    if (action != WebKit::SheetAction::Copy)
        return;

    NSDictionary *representations = @{
        (NSString *)kUTTypeUTF8PlainText : _positionInformation.url,
        (NSString *)kUTTypeURL : [NSURL URLWithString:_positionInformation.url]
    };

    [UIPasteboard generalPasteboard].items = @[ representations ];
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant openElementAtLocation:(CGPoint)location
{
    CGPoint screenPoint = [self.window convertPoint:[self convertPoint:location toView:nil] toWindow:nil];
    _webView->_page->navigateToPDFLinkWithSimulatedClick(_positionInformation.url, roundedIntPoint(location), roundedIntPoint(screenPoint));
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant shareElementWithURL:(NSURL *)url rect:(CGRect)boundingRect
{
    _webSelectionAssistant = adoptNS([[UIWKSelectionAssistant alloc] initWithView:self]);
    [_webSelectionAssistant showShareSheetFor:userVisibleString(url) fromRect:boundingRect];
    _webSelectionAssistant = nil;
}

#if HAVE(APP_LINKS)
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeAppLinkActionsForElement:(_WKActivatedElementInfo *)element
{
    return _webView->_page->uiClient().shouldIncludeAppLinkActionsForElement(element);
}
#endif

- (RetainPtr<NSArray>)actionSheetAssistant:(WKActionSheetAssistant *)assistant decideActionsForElement:(_WKActivatedElementInfo *)element defaultActions:(RetainPtr<NSArray>)defaultActions
{
    return _webView->_page->uiClient().actionsForElement(element, WTFMove(defaultActions));
}

#pragma mark Password protection UI

- (void)_updatePasswordEntryField
{
    [_passwordView setFrame:CGRectMake(0, 0, _webView.bounds.size.width, _webView.bounds.size.height)];
    [_scrollView setContentSize:[_passwordView bounds].size];
}

- (void)_keyboardDidShow:(NSNotification *)notification
{
    UITextField *passwordField = [_passwordView passwordField];
    if (!passwordField.isEditing)
        return;

    CGRect keyboardRect = [UIPeripheralHost visiblePeripheralFrame];
    if (CGRectIsEmpty(keyboardRect))
        return;

    UIWindow *window = _scrollView.window;
    keyboardRect = [window convertRect:keyboardRect fromWindow:nil];
    keyboardRect = [_scrollView convertRect:keyboardRect fromView:window];

    CGRect passwordFieldFrame = [passwordField convertRect:passwordField.bounds toView:_scrollView];

    CGSize contentSize = [_passwordView bounds].size;
    contentSize.height += CGRectGetHeight(keyboardRect);
    [_scrollView setContentSize:contentSize];

    if (CGRectIntersectsRect(passwordFieldFrame, keyboardRect)) {
        CGFloat yDelta = CGRectGetMaxY(passwordFieldFrame) - CGRectGetMinY(keyboardRect);

        CGPoint contentOffset = _scrollView.contentOffset;
        contentOffset.y += yDelta + passwordEntryFieldPadding;

        [_scrollView setContentOffset:contentOffset animated:YES];
    }
}

- (void)_showPasswordEntryField
{
    [_scrollView setMinimumZoomScale:1];
    [_scrollView setMaximumZoomScale:1];
    [_scrollView setBackgroundColor:[UIColor groupTableViewBackgroundColor]];

    _passwordView = adoptNS([[UIDocumentPasswordView alloc] initWithDocumentName:_suggestedFilename.get()]);
    [_passwordView setPasswordDelegate:self];

    [self _updatePasswordEntryField];

    [self addSubview:_passwordView.get()];
}

- (void)_hidePasswordEntryField
{
    [_passwordView removeFromSuperview];
    _passwordView = nil;

    [_scrollView setMinimumZoomScale:pdfMinimumZoomScale];
    [_scrollView setMaximumZoomScale:pdfMaximumZoomScale];
    [_scrollView setBackgroundColor:[UIColor grayColor]];
}

- (void)userDidEnterPassword:(NSString *)password forPasswordView:(UIDocumentPasswordView *)passwordView
{
    [self _tryToUnlockWithPassword:password];
}

- (void)didBeginEditingPassword:(UITextField *)passwordField inView:(UIDocumentPasswordView *)passwordView
{
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_keyboardDidShow:) name:UIKeyboardDidShowNotification object:nil];
}

- (void)didEndEditingPassword:(UITextField *)passwordField inView:(UIDocumentPasswordView *)passwordView
{
    [_scrollView setContentSize:[_passwordView frame].size];
    [_scrollView setContentOffset:CGPointMake(-_scrollView.contentInset.left, -_scrollView.contentInset.top) animated:YES];

    [[NSNotificationCenter defaultCenter] removeObserver:self name:UIKeyboardDidShowNotification object:nil];
}

- (void)_didFailToUnlock
{
    [[_passwordView passwordField] setText:@""];
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:WEB_UI_STRING("The document could not be opened with that password.", "PDF password failure alert message") message:@"" preferredStyle:UIAlertControllerStyleAlert];

    UIAlertAction* defaultAction = [UIAlertAction actionWithTitle:WEB_UI_STRING_KEY("OK", "OK (PDF password failure alert)", "OK button label in PDF password failure alert") style:UIAlertActionStyleDefault handler:[](UIAlertAction *) { }];
    
    [alert addAction:defaultAction];

    [self.window.rootViewController presentViewController:alert animated:YES completion:nil];
}

- (BOOL)_tryToUnlockWithPassword:(NSString *)password
{
    if (CGPDFDocumentUnlockWithPassword(_cgPDFDocument.get(), [password UTF8String])) {
        [self _hidePasswordEntryField];
        [self _didLoadPDFDocument];
        return YES;
    }

    [self _didFailToUnlock];
    return NO;
}

- (void)willMoveToWindow:(UIWindow *)newWindow
{
    if (newWindow)
        return;
    
    ASSERT(self.window);
    ASSERT(_applicationStateTracker);
    _applicationStateTracker = nullptr;
}

- (void)didMoveToWindow
{
    if (!self.window)
        return;

    ASSERT(!_applicationStateTracker);
    _applicationStateTracker = std::make_unique<ApplicationStateTracker>(self, @selector(_applicationDidEnterBackground), @selector(_applicationWillEnterForeground));
}

- (BOOL)isBackground
{
    if (!_applicationStateTracker)
        return YES;

    return _applicationStateTracker->isInBackground();
}

- (void)_applicationDidEnterBackground
{
    _webView->_page->applicationDidEnterBackground();
    _webView->_page->viewStateDidChange(ViewState::AllFlags & ~ViewState::IsInWindow);
}

- (void)_applicationWillEnterForeground
{
    _webView->_page->applicationWillEnterForeground();
    if (auto drawingArea = _webView->_page->drawingArea())
        drawingArea->hideContentUntilAnyUpdate();
    _webView->_page->viewStateDidChange(ViewState::AllFlags & ~ViewState::IsInWindow, true, WebPageProxy::ViewStateChangeDispatchMode::Immediate);
}

@end

#endif /* PLATFORM(IOS) */
