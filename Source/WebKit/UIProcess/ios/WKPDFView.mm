/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WKPDFVIEW)

#import "APIUIClient.h"
#import "FindClient.h"
#import "PDFKitSPI.h"
#import "UIKitSPI.h"
#import "WKActionSheetAssistant.h"
#import "WKKeyboardScrollingAnimator.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebEvent.h"
#import "WKWebViewIOS.h"
#import "WebPageProxy.h"
#import "_WKWebViewPrintFormatterInternal.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <WebCore/DataDetection.h>
#import <WebCore/ShareData.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/NSURLExtras.h>

@interface WKPDFView () <PDFHostViewControllerDelegate, WKActionSheetAssistantDelegate>
@end

@implementation WKPDFView {
    RetainPtr<WKActionSheetAssistant> _actionSheetAssistant;
    RetainPtr<NSData> _data;
    RetainPtr<CGPDFDocumentRef> _documentForPrinting;
    BlockPtr<void()> _findCompletion;
    RetainPtr<NSString> _findString;
    NSUInteger _findStringCount;
    NSUInteger _findStringMaxCount;
    RetainPtr<UIView> _fixedOverlayView;
    std::optional<NSUInteger> _focusedSearchResultIndex;
    NSInteger _focusedSearchResultPendingOffset;
    RetainPtr<PDFHostViewController> _hostViewController;
    CGSize _overlaidAccessoryViewsInset;
    RetainPtr<UIView> _pageNumberIndicator;
    CString _passwordForPrinting;
    WebKit::InteractionInformationAtPosition _positionInformation;
    RetainPtr<NSString> _suggestedFilename;
    WeakObjCPtr<WKWebView> _webView;
    RetainPtr<WKKeyboardScrollViewAnimator> _keyboardScrollingAnimator;
    RetainPtr<WKShareSheet> _shareSheet;
}

- (void)dealloc
{
    if (_shareSheet) {
        [_shareSheet setDelegate:nil];
        [_shareSheet dismiss];
        _shareSheet = nil;
    }
    [_actionSheetAssistant cleanupSheet];
    [[_hostViewController view] removeFromSuperview];
    [_pageNumberIndicator removeFromSuperview];
    [_keyboardScrollingAnimator invalidate];
    std::memset(_passwordForPrinting.mutableData(), 0, _passwordForPrinting.length());
    [super dealloc];
}

- (BOOL)web_handleKeyEvent:(::UIEvent *)event
{
    auto webEvent = adoptNS([[WKWebEvent alloc] initWithEvent:event]);

    if ([_keyboardScrollingAnimator beginWithEvent:webEvent.get()])
        return YES;
    [_keyboardScrollingAnimator handleKeyEvent:webEvent.get()];
    return NO;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    return [_hostViewController gestureRecognizerShouldBegin:gestureRecognizer];
}


#pragma mark WKApplicationStateTrackingView

- (UIView *)_contentView
{
    return _hostViewController ? [_hostViewController view] : self;
}


#pragma mark WKWebViewContentProvider

- (instancetype)web_initWithFrame:(CGRect)frame webView:(WKWebView *)webView mimeType:(NSString *)mimeType
{
    if (!(self = [super initWithFrame:frame webView:webView]))
        return nil;

    UIColor *backgroundColor = PDFHostViewController.backgroundColor;
    self.backgroundColor = backgroundColor;
    webView.scrollView.backgroundColor = backgroundColor;

    _keyboardScrollingAnimator = adoptNS([[WKKeyboardScrollViewAnimator alloc] initWithScrollView:webView.scrollView]);

    _webView = webView;
    return self;
}

- (void)web_setContentProviderData:(NSData *)data suggestedFilename:(NSString *)filename
{
    _data = adoptNS([data copy]);
    _suggestedFilename = adoptNS([filename copy]);

    [PDFHostViewController createHostView:[self, weakSelf = WeakObjCPtr<WKPDFView>(self)](PDFHostViewController *hostViewController) {
        ASSERT(isMainRunLoop());

        WKPDFView *autoreleasedSelf = weakSelf.getAutoreleased();
        if (!autoreleasedSelf)
            return;

        WKWebView *webView = _webView.getAutoreleased();
        if (!webView)
            return;

        if (!hostViewController)
            return;
        _hostViewController = hostViewController;

        UIView *hostView = hostViewController.view;
        hostView.frame = webView.bounds;

        UIScrollView *scrollView = webView.scrollView;
        [self removeFromSuperview];
        [scrollView addSubview:hostView];

        _actionSheetAssistant = adoptNS([[WKActionSheetAssistant alloc] initWithView:hostView]);
        [_actionSheetAssistant setDelegate:self];

        _pageNumberIndicator = hostViewController.pageNumberIndicator;
        [_fixedOverlayView addSubview:_pageNumberIndicator.get()];

        hostViewController.delegate = self;
        [hostViewController setDocumentData:_data.get() withScrollView:scrollView];
    } forExtensionIdentifier:nil];
}

- (CGPoint)_offsetForPageNumberIndicator
{
    WKWebView *webView = _webView.getAutoreleased();
    if (!webView)
        return CGPointZero;

    UIEdgeInsets insets = UIEdgeInsetsAdd(webView._computedUnobscuredSafeAreaInset, webView._computedObscuredInset, UIRectEdgeAll);
    return CGPointMake(insets.left, insets.top + _overlaidAccessoryViewsInset.height);
}

- (void)_movePageNumberIndicatorToPoint:(CGPoint)point animated:(BOOL)animated
{
    void (^setFrame)() = ^{
        static const CGFloat margin = 20;
        const CGRect frame = { CGPointMake(point.x + margin, point.y + margin), [_pageNumberIndicator frame].size };
        [_pageNumberIndicator setFrame:frame];
    };

    if (animated) {
        static const NSTimeInterval duration = 0.3;
        [UIView animateWithDuration:duration animations:setFrame];
        return;
    }

    setFrame();
}

- (void)_updateLayoutAnimated:(BOOL)animated
{
    [_hostViewController updatePDFViewLayout];
    [self _movePageNumberIndicatorToPoint:self._offsetForPageNumberIndicator animated:animated];
}

- (void)web_setMinimumSize:(CGSize)size
{
    self.frame = { self.frame.origin, size };
    [self _updateLayoutAnimated:NO];
}

- (void)web_setOverlaidAccessoryViewsInset:(CGSize)inset
{
    _overlaidAccessoryViewsInset = inset;
    [self _updateLayoutAnimated:YES];
}

- (void)web_computedContentInsetDidChange
{
    [self _updateLayoutAnimated:NO];
}

- (void)web_setFixedOverlayView:(UIView *)fixedOverlayView
{
    _fixedOverlayView = fixedOverlayView;
}

- (void)_scrollToURLFragment:(NSString *)fragment
{
    NSInteger pageIndex = 0;
    if ([fragment hasPrefix:@"page"])
        pageIndex = [[fragment substringFromIndex:4] integerValue] - 1;

    if (pageIndex >= 0 && pageIndex < [_hostViewController pageCount] && pageIndex != [_hostViewController currentPageIndex])
        [_hostViewController goToPageIndex:pageIndex];
}

- (void)web_didSameDocumentNavigation:(WKSameDocumentNavigationType)navigationType
{
    if (navigationType == kWKSameDocumentNavigationSessionStatePop)
        [self _scrollToURLFragment:[_webView URL].fragment];
}

static NSStringCompareOptions stringCompareOptions(_WKFindOptions findOptions)
{
    NSStringCompareOptions compareOptions = 0;
    if (findOptions & _WKFindOptionsBackwards)
        compareOptions |= NSBackwardsSearch;
    if (findOptions & _WKFindOptionsCaseInsensitive)
        compareOptions |= NSCaseInsensitiveSearch;
    return compareOptions;
}

- (void)_resetFind
{
    if (_findCompletion)
        [_hostViewController cancelFindString];

    _findCompletion = nil;
    _findString = nil;
    _findStringCount = 0;
    _findStringMaxCount = 0;
    _focusedSearchResultIndex = std::nullopt;
    _focusedSearchResultPendingOffset = 0;
}

- (void)_findString:(NSString *)string withOptions:(_WKFindOptions)options maxCount:(NSUInteger)maxCount completion:(void(^)())completion
{
    [self _resetFind];

    _findCompletion = completion;
    _findString = adoptNS([string copy]);
    _findStringMaxCount = maxCount;
    [_hostViewController findString:_findString.get() withOptions:stringCompareOptions(options)];
}

- (void)web_countStringMatches:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
    [self _findString:string withOptions:options maxCount:maxCount completion:^{
        ASSERT([_findString isEqualToString:string]);
        if (auto page = [_webView _page])
            page->findClient().didCountStringMatches(page, _findString.get(), _findStringCount);
    }];
}

- (BOOL)_computeFocusedSearchResultIndexWithOptions:(_WKFindOptions)options didWrapAround:(BOOL *)didWrapAround
{
    BOOL isBackwards = options & _WKFindOptionsBackwards;
    NSInteger singleOffset = isBackwards ? -1 : 1;

    if (_findCompletion) {
        ASSERT(!_focusedSearchResultIndex);
        _focusedSearchResultPendingOffset += singleOffset;
        return NO;
    }

    if (!_findStringCount)
        return NO;

    NSInteger newIndex;
    if (_focusedSearchResultIndex) {
        ASSERT(!_focusedSearchResultPendingOffset);
        newIndex = *_focusedSearchResultIndex + singleOffset;
    } else {
        newIndex = isBackwards ? _findStringCount - 1 : 0;
        newIndex += std::exchange(_focusedSearchResultPendingOffset, 0);
    }

    if (newIndex < 0 || static_cast<NSUInteger>(newIndex) >= _findStringCount) {
        if (!(options & _WKFindOptionsWrapAround))
            return NO;

        NSUInteger wrappedIndex = std::abs(newIndex) % _findStringCount;
        if (newIndex < 0)
            wrappedIndex = _findStringCount - wrappedIndex;
        newIndex = wrappedIndex;
        *didWrapAround = YES;
    }

    _focusedSearchResultIndex = newIndex;
    ASSERT(*_focusedSearchResultIndex < _findStringCount);
    return YES;
}

- (void)_focusOnSearchResultWithOptions:(_WKFindOptions)options
{
    auto page = [_webView _page];
    if (!page)
        return;

    BOOL didWrapAround = NO;
    if (![self _computeFocusedSearchResultIndexWithOptions:options didWrapAround:&didWrapAround]) {
        if (!_findCompletion)
            page->findClient().didFailToFindString(page, _findString.get());
        return;
    }

    auto focusedIndex = *_focusedSearchResultIndex;
    [_hostViewController focusOnSearchResultAtIndex:focusedIndex];
    page->findClient().didFindString(page, _findString.get(), { }, _findStringCount, focusedIndex, didWrapAround);
}

- (void)web_findString:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
    if ([_findString isEqualToString:string]) {
        [self _focusOnSearchResultWithOptions:options];
        return;
    }

    [self _findString:string withOptions:options maxCount:maxCount completion:^{
        ASSERT([_findString isEqualToString:string]);
        [self _focusOnSearchResultWithOptions:options];
    }];
}

- (void)web_hideFindUI
{
    [self _resetFind];
}

- (UIView *)web_contentView
{
    return self._contentView;
}

+ (BOOL)web_requiresCustomSnapshotting
{
    static bool hasGlobalCaptureEntitlement = WTF::processHasEntitlement("com.apple.QuartzCore.global-capture");
    return !hasGlobalCaptureEntitlement;
}

- (void)web_scrollViewDidScroll:(UIScrollView *)scrollView
{
    [_hostViewController updatePDFViewLayout];
}

- (void)web_scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view
{
    [_hostViewController updatePDFViewLayout];
}

- (void)web_scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale
{
    [_hostViewController updatePDFViewLayout];
}

- (void)web_scrollViewDidZoom:(UIScrollView *)scrollView
{
    [_hostViewController updatePDFViewLayout];
}

- (void)web_beginAnimatedResizeWithUpdates:(void (^)(void))updateBlock
{
    [_hostViewController beginPDFViewRotation];
    updateBlock();
    [_hostViewController endPDFViewRotation];
}

- (void)web_snapshotRectInContentViewCoordinates:(CGRect)rectInContentViewCoordinates snapshotWidth:(CGFloat)snapshotWidth completionHandler:(void (^)(CGImageRef))completionHandler
{
    CGRect rectInHostViewCoordinates = [self._contentView convertRect:rectInContentViewCoordinates toView:[_hostViewController view]];
    [_hostViewController snapshotViewRect:rectInHostViewCoordinates snapshotWidth:@(snapshotWidth) afterScreenUpdates:NO withResult:^(UIImage *image) {
        completionHandler(image.CGImage);
    }];
}

- (NSData *)web_dataRepresentation
{
    return _data.get();
}

- (NSString *)web_suggestedFilename
{
    return _suggestedFilename.get();
}

- (BOOL)web_isBackground
{
    return self.isBackground;
}


#pragma mark PDFHostViewControllerDelegate

- (void)pdfHostViewController:(PDFHostViewController *)controller updatePageCount:(NSInteger)pageCount
{
    [self _scrollToURLFragment:[_webView URL].fragment];
}

- (void)pdfHostViewController:(PDFHostViewController *)controller documentDidUnlockWithPassword:(NSString *)password
{
    _passwordForPrinting = [password UTF8String];
}

- (void)pdfHostViewController:(PDFHostViewController *)controller findStringUpdate:(NSUInteger)numFound done:(BOOL)done
{
    if (numFound > _findStringMaxCount && !done) {
        [controller cancelFindStringWithHighlightsCleared:NO];
        done = YES;
    }
    
    if (!done)
        return;
    
    if (auto findCompletion = std::exchange(_findCompletion, nil)) {
        _findStringCount = numFound;
        findCompletion();
    }
}

- (NSURL *)_URLWithPageIndex:(NSInteger)pageIndex
{
    return [NSURL URLWithString:[NSString stringWithFormat:@"#page%ld", (long)pageIndex + 1] relativeToURL:[_webView URL]];
}

- (void)_goToURL:(NSURL *)url atLocation:(CGPoint)location
{
    auto page = [_webView _page];
    if (!page)
        return;

    UIView *hostView = [_hostViewController view];
    CGPoint locationInScreen = [hostView.window convertPoint:[hostView convertPoint:location toView:nil] toWindow:nil];
    page->navigateToPDFLinkWithSimulatedClick(url.absoluteString, WebCore::roundedIntPoint(location), WebCore::roundedIntPoint(locationInScreen));
}

- (void)pdfHostViewController:(PDFHostViewController *)controller goToURL:(NSURL *)url
{
    // FIXME: We'd use the real tap location if we knew it.
    [self _goToURL:url atLocation:CGPointMake(0, 0)];
}

- (void)pdfHostViewController:(PDFHostViewController *)controller goToPageIndex:(NSInteger)pageIndex withViewFrustum:(CGRect)documentViewRect
{
    [self _goToURL:[self _URLWithPageIndex:pageIndex] atLocation:documentViewRect.origin];
}

- (void)_showActionSheetForURL:(NSURL *)url atLocation:(CGPoint)location withAnnotationRect:(CGRect)annotationRect
{
    WKWebView *webView = _webView.getAutoreleased();
    if (!webView)
        return;

    WebKit::InteractionInformationAtPosition positionInformation;
    positionInformation.bounds = WebCore::roundedIntRect(annotationRect);
    positionInformation.request.point = WebCore::roundedIntPoint(location);
    positionInformation.url = url;

    _positionInformation = WTFMove(positionInformation);

#if ENABLE(DATA_DETECTION)
    if (WebCore::DataDetection::canBePresentedByDataDetectors(_positionInformation.url)) {
        [_actionSheetAssistant showDataDetectorsUIForPositionInformation:positionInformation];
        return;
    }
#endif

    [_actionSheetAssistant showLinkSheet];
}

- (void)pdfHostViewController:(PDFHostViewController *)controller didLongPressURL:(NSURL *)url atLocation:(CGPoint)location withAnnotationRect:(CGRect)annotationRect
{
    [self _showActionSheetForURL:url atLocation:location withAnnotationRect:annotationRect];
}

- (void)pdfHostViewController:(PDFHostViewController *)controller didLongPressPageIndex:(NSInteger)pageIndex atLocation:(CGPoint)location withAnnotationRect:(CGRect)annotationRect
{
    [self _showActionSheetForURL:[self _URLWithPageIndex:pageIndex] atLocation:location withAnnotationRect:annotationRect];
}

- (void)pdfHostViewControllerExtensionProcessDidCrash:(PDFHostViewController *)controller
{
    // FIXME 40916725: PDFKit should dispatch this message to the main thread like it does for other delegate messages.
    RunLoop::main().dispatch([webView = _webView] {
        if (auto page = [webView _page])
            page->dispatchProcessDidTerminate(WebKit::ProcessTerminationReason::Crash);
    });
}


#pragma mark WKActionSheetAssistantDelegate

- (std::optional<WebKit::InteractionInformationAtPosition>)positionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    return _positionInformation;
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant performAction:(WebKit::SheetAction)action
{
    if (action != WebKit::SheetAction::Copy)
        return;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSDictionary *representations = @{
        (NSString *)kUTTypeUTF8PlainText : (NSString *)_positionInformation.url.string(),
        (NSString *)kUTTypeURL : (NSURL *)_positionInformation.url,
    };
ALLOW_DEPRECATED_DECLARATIONS_END

    [UIPasteboard generalPasteboard].items = @[ representations ];
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant openElementAtLocation:(CGPoint)location
{
    [self _goToURL:_positionInformation.url atLocation:location];
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant shareElementWithURL:(NSURL *)url rect:(CGRect)boundingRect
{
    WKWebView *webView = _webView.getAutoreleased();
    if (!webView)
        return;

    WebCore::ShareDataWithParsedURL shareData;
    shareData.url = { url };
    
    [_shareSheet dismiss];

    _shareSheet = adoptNS([[WKShareSheet alloc] initWithView:webView]);
    [_shareSheet setDelegate:self];
    [_shareSheet presentWithParameters:shareData inRect: { [[_hostViewController view] convertRect:boundingRect toView:webView] } completionHandler:[] (bool success) { }];
}

- (void)shareSheetDidDismiss:(WKShareSheet *)shareSheet
{
    ASSERT(_shareSheet == shareSheet);
    
    [_shareSheet setDelegate:nil];
    _shareSheet = nil;
}

#if HAVE(APP_LINKS)
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeAppLinkActionsForElement:(_WKActivatedElementInfo *)element
{
    auto page = [_webView _page];
    if (!page)
        return NO;

    return page->uiClient().shouldIncludeAppLinkActionsForElement(element);
}
#endif

- (RetainPtr<NSArray>)actionSheetAssistant:(WKActionSheetAssistant *)assistant decideActionsForElement:(_WKActivatedElementInfo *)element defaultActions:(RetainPtr<NSArray>)defaultActions
{
    auto page = [_webView _page];
    if (!page)
        return nil;

    return page->uiClient().actionsForElement(element, WTFMove(defaultActions));
}

- (NSDictionary *)dataDetectionContextForActionSheetAssistant:(WKActionSheetAssistant *)assistant positionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation
{
    auto webView = _webView.getAutoreleased();
    if (!webView)
        return nil;

    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>(webView.UIDelegate);
    if (![uiDelegate respondsToSelector:@selector(_dataDetectionContextForWebView:)])
        return nil;

    return [uiDelegate _dataDetectionContextForWebView:webView];
}

@end


#pragma mark _WKWebViewPrintProvider

@interface WKPDFView (_WKWebViewPrintFormatter) <_WKWebViewPrintProvider>
@end

@implementation WKPDFView (_WKWebViewPrintFormatter)

- (CGPDFDocumentRef)_ensureDocumentForPrinting
{
    if (_documentForPrinting)
        return _documentForPrinting.get();

    auto dataProvider = adoptCF(CGDataProviderCreateWithCFData((CFDataRef)_data.get()));
    auto pdfDocument = adoptCF(CGPDFDocumentCreateWithProvider(dataProvider.get()));
    if (!CGPDFDocumentIsUnlocked(pdfDocument.get())) {
        if (!CGPDFDocumentUnlockWithPassword(pdfDocument.get(), _passwordForPrinting.data()))
            return nullptr;
    }

    if (!CGPDFDocumentAllowsPrinting(pdfDocument.get()))
        return nullptr;

    _documentForPrinting = WTFMove(pdfDocument);
    return _documentForPrinting.get();
}

- (NSUInteger)_wk_pageCountForPrintFormatter:(_WKWebViewPrintFormatter *)printFormatter
{
    CGPDFDocumentRef documentForPrinting = [self _ensureDocumentForPrinting];
    if (!documentForPrinting)
        return 0;

    size_t pageCount = CGPDFDocumentGetNumberOfPages(documentForPrinting);
    if (printFormatter.snapshotFirstPage)
        return std::min<NSUInteger>(pageCount, 1);
    return pageCount;
}

- (CGPDFDocumentRef)_wk_printedDocument
{
    return [self _ensureDocumentForPrinting];
}

@end

#endif // ENABLE(WKPDFVIEW)
