/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#import "SameDocumentNavigationType.h"
#import "WKShareSheet.h"
#import "WKWebViewConfiguration.h"
#import "_WKAttachmentInternal.h"
#import "_WKWebViewPrintFormatterInternal.h"
#import <wtf/CompletionHandler.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/Variant.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFullScreenWindowControllerIOS.h"
#import <WebCore/FloatRect.h>
#import <WebCore/LengthBox.h>
#endif

#if PLATFORM(IOS_FAMILY)
#define WK_WEB_VIEW_PROTOCOLS <UIScrollViewDelegate>
#endif

#if PLATFORM(MAC)
#define WK_WEB_VIEW_PROTOCOLS <WKShareSheetDelegate>
#endif

#if !defined(WK_WEB_VIEW_PROTOCOLS)
#define WK_WEB_VIEW_PROTOCOLS
#endif

typedef const struct OpaqueWKPage* WKPageRef;

namespace API {
class Attachment;
}

namespace WebKit {
enum class ContinueUnsafeLoad : bool;
class SafeBrowsingWarning;
class ViewSnapshot;
class WebPageProxy;
struct PrintInfo;
}

namespace WebCore {
class URL;
}

@class WKWebViewContentProviderRegistry;
@class WKPasswordView;
@class _WKFrameHandle;

@interface WKWebView () WK_WEB_VIEW_PROTOCOLS {

@package
    RetainPtr<WKWebViewConfiguration> _configuration;

    RefPtr<WebKit::WebPageProxy> _page;
}

#if PLATFORM(IOS_FAMILY)
- (void)_processDidExit;
- (void)_processWillSwap;
- (void)_didRelaunchProcess;

- (void)_didCommitLoadForMainFrame;
- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction;
- (void)_layerTreeCommitComplete;

- (void)_couldNotRestorePageState;
- (void)_restorePageScrollPosition:(std::optional<WebCore::FloatPoint>)scrollPosition scrollOrigin:(WebCore::FloatPoint)scrollOrigin previousObscuredInset:(WebCore::FloatBoxExtent)insets scale:(double)scale;
- (void)_restorePageStateToUnobscuredCenter:(std::optional<WebCore::FloatPoint>)center scale:(double)scale; // FIXME: needs scroll origin?

- (RefPtr<WebKit::ViewSnapshot>)_takeViewSnapshot;

- (void)_scrollToContentScrollPosition:(WebCore::FloatPoint)scrollPosition scrollOrigin:(WebCore::IntPoint)scrollOrigin;
- (BOOL)_scrollToRect:(WebCore::FloatRect)targetRect origin:(WebCore::FloatPoint)origin minimumScrollDistance:(float)minimumScrollDistance;
- (void)_zoomToFocusRect:(WebCore::FloatRect)focusedElementRect selectionRect:(WebCore::FloatRect)selectionRectInDocumentCoordinates insideFixed:(BOOL)insideFixed fontSize:(float)fontSize minimumScale:(double)minimumScale maximumScale:(double)maximumScale allowScaling:(BOOL)allowScaling forceScroll:(BOOL)forceScroll;
- (BOOL)_zoomToRect:(WebCore::FloatRect)targetRect withOrigin:(WebCore::FloatPoint)origin fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale minimumScrollDistance:(float)minimumScrollDistance;
- (void)_zoomOutWithOrigin:(WebCore::FloatPoint)origin animated:(BOOL)animated;
- (void)_zoomToInitialScaleWithOrigin:(WebCore::FloatPoint)origin animated:(BOOL)animated;
- (void)_didFinishScrolling;

- (void)_setHasCustomContentView:(BOOL)hasCustomContentView loadedMIMEType:(const WTF::String&)mimeType;
- (void)_didFinishLoadingDataForCustomContentProviderWithSuggestedFilename:(const WTF::String&)suggestedFilename data:(NSData *)data;

- (void)_willInvokeUIScrollViewDelegateCallback;
- (void)_didInvokeUIScrollViewDelegateCallback;

- (void)_scheduleVisibleContentRectUpdate;

- (void)_didCompleteAnimatedResize;

- (void)_didStartProvisionalLoadForMainFrame;
- (void)_didFinishLoadForMainFrame;
- (void)_didFailLoadForMainFrame;
- (void)_didSameDocumentNavigationForMainFrame:(WebKit::SameDocumentNavigationType)navigationType;

- (BOOL)_isShowingVideoPictureInPicture;
- (BOOL)_mayAutomaticallyShowVideoPictureInPicture;

- (void)_updateScrollViewBackground;

- (void)_videoControlsManagerDidChange;

- (void)_navigationGestureDidBegin;
- (void)_navigationGestureDidEnd;
- (BOOL)_isNavigationSwipeGestureRecognizer:(UIGestureRecognizer *)recognizer;

- (void)_showPasswordViewWithDocumentName:(NSString *)documentName passwordHandler:(void (^)(NSString *))passwordHandler;
- (void)_hidePasswordView;

- (void)_didChangeEditorState;

- (void)_addShortcut:(id)sender;
- (void)_define:(id)sender;
- (void)_lookup:(id)sender;
- (void)_share:(id)sender;
- (void)_showTextStyleOptions:(id)sender;
- (void)_promptForReplace:(id)sender;
- (void)_transliterateChinese:(id)sender;
- (void)replace:(id)sender;

- (void)_nextAccessoryTab:(id)sender;
- (void)_previousAccessoryTab:(id)sender;

- (void)_incrementFocusPreservationCount;
- (void)_decrementFocusPreservationCount;
- (void)_resetFocusPreservationCount;

@property (nonatomic, readonly) WKPasswordView *_passwordView;

@property (nonatomic, readonly) BOOL _isBackground;

@property (nonatomic, readonly) WKWebViewContentProviderRegistry *_contentProviderRegistry;

@property (nonatomic, readonly) WKSelectionGranularity _selectionGranularity;

@property (nonatomic, readonly) BOOL _allowsDoubleTapGestures;
@property (nonatomic, readonly) BOOL _haveSetObscuredInsets;
@property (nonatomic, readonly) UIEdgeInsets _computedObscuredInset;
@property (nonatomic, readonly) UIEdgeInsets _computedUnobscuredSafeAreaInset;
@property (nonatomic, readonly, getter=_isRetainingActiveFocusedState) BOOL _retainingActiveFocusedState;
#endif

#if ENABLE(ACCESSIBILITY_EVENTS)
- (void)_updateAccessibilityEventsEnabled;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
- (void)_didRemoveAttachment:(API::Attachment&)attachment;
- (void)_didInsertAttachment:(API::Attachment&)attachment withSource:(NSString *)source;
#endif

- (void)_showSafeBrowsingWarning:(const WebKit::SafeBrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, WebCore::URL>&&)>&&)completionHandler;
- (void)_clearSafeBrowsingWarning;

- (std::optional<BOOL>)_resolutionForShareSheetImmediateCompletionForTesting;

- (WKPageRef)_pageForTesting;
- (WebKit::WebPageProxy*)_page;

@end

WKWebView* fromWebPageProxy(WebKit::WebPageProxy&);

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
@interface WKWebView (FullScreenAPI)
-(BOOL)hasFullScreenWindowController;
-(WKFullScreenWindowController *)fullScreenWindowController;
-(void)closeFullScreenWindowController;
@end
#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOSMAC)
@interface WKWebView (_WKWebViewPrintFormatter)
@property (nonatomic, readonly) id <_WKWebViewPrintProvider> _printProvider;
@end
#endif

#endif
