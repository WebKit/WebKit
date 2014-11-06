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

#import "WKWebViewPrivate.h"

#if WK_API_ENABLED

#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import <WebCore/FloatRect.h>
#import <UIKit/UIScrollView_Private.h>
#endif

#if PLATFORM(IOS)
#define WK_WEB_VIEW_PROTOCOLS <UIScrollViewDelegate>
#endif

#if !defined(WK_WEB_VIEW_PROTOCOLS)
#define WK_WEB_VIEW_PROTOCOLS
#endif

namespace WebCore {
struct Highlight;
}

namespace WebKit {
class ViewSnapshot;
class WebPageProxy;
struct PrintInfo;
}

@class _WKFrameHandle;

@interface WKWebView () WK_WEB_VIEW_PROTOCOLS {

@package
    RetainPtr<WKWebViewConfiguration> _configuration;

    RefPtr<WebKit::WebPageProxy> _page;
}

#if PLATFORM(IOS)

@property (nonatomic, setter=_setUsesMinimalUI:) BOOL _usesMinimalUI;

- (void)_processDidExit;

- (void)_didCommitLoadForMainFrame;
- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction;

- (void)_dynamicViewportUpdateChangedTargetToScale:(double)newScale position:(CGPoint)newScrollPosition nextValidLayerTreeTransactionID:(uint64_t)nextValidLayerTreeTransactionID;
- (void)_restorePageStateToExposedRect:(WebCore::FloatRect)exposedRect scale:(double)scale;
- (void)_restorePageStateToUnobscuredCenter:(WebCore::FloatPoint)center scale:(double)scale;

- (PassRefPtr<WebKit::ViewSnapshot>)_takeViewSnapshot;

- (void)_scrollToContentOffset:(WebCore::FloatPoint)contentOffset;
- (BOOL)_scrollToRect:(WebCore::FloatRect)targetRect origin:(WebCore::FloatPoint)origin minimumScrollDistance:(float)minimumScrollDistance;
- (void)_zoomToFocusRect:(WebCore::FloatRect)focusedElementRect selectionRect:(WebCore::FloatRect)selectionRectInDocumentCoordinates fontSize:(float)fontSize minimumScale:(double)minimumScale maximumScale:(double)maximumScale allowScaling:(BOOL)allowScaling forceScroll:(BOOL)forceScroll;
- (BOOL)_zoomToRect:(WebCore::FloatRect)targetRect withOrigin:(WebCore::FloatPoint)origin fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale minimumScrollDistance:(float)minimumScrollDistance;
- (void)_zoomOutWithOrigin:(WebCore::FloatPoint)origin;

- (void)_setHasCustomContentView:(BOOL)hasCustomContentView loadedMIMEType:(const WTF::String&)mimeType;
- (void)_didFinishLoadingDataForCustomContentProviderWithSuggestedFilename:(const WTF::String&)suggestedFilename data:(NSData *)data;
- (void)_setViewportMetaTagWidth:(float)newWidth;

- (void)_willInvokeUIScrollViewDelegateCallback;
- (void)_didInvokeUIScrollViewDelegateCallback;

- (void)_updateVisibleContentRects;

@property (nonatomic, readonly) UIEdgeInsets _computedContentInset;
#else
@property (nonatomic, setter=_setIgnoresNonWheelMouseEvents:) BOOL _ignoresNonWheelMouseEvents;
#endif

@end

WKWebView* fromWebPageProxy(WebKit::WebPageProxy&);

#if PLATFORM(IOS)
@interface WKWebView (_WKWebViewPrintFormatter)
- (NSInteger)_computePageCountAndStartDrawingToPDFForFrame:(_WKFrameHandle *)frame printInfo:(const WebKit::PrintInfo&)printInfo firstPage:(uint32_t)firstPage computedTotalScaleFactor:(double&)totalScaleFactor;
- (void)_endPrinting;
@property (nonatomic, setter=_setPrintedDocument:) CGPDFDocumentRef _printedDocument;
@end
#endif

#endif
