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

#import "PDFPluginIdentifier.h"
#import "SameDocumentNavigationType.h"
#import "WKShareSheet.h"
#import "WKWebViewConfiguration.h"
#import "WKWebViewPrivate.h"
#import "_WKAttachmentInternal.h"
#import "_WKWebViewPrintFormatterInternal.h"
#import <wtf/CompletionHandler.h>
#import <wtf/NakedPtr.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/Variant.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "DynamicViewportSizeUpdate.h"
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
class IconLoadingDelegate;
class InspectorDelegate;
class NavigationState;
class ResourceLoadDelegate;
class SafeBrowsingWarning;
class ViewSnapshot;
class WebPageProxy;
class UIDelegate;
struct PrintInfo;
#if PLATFORM(MAC)
class WebViewImpl;
#endif
#if PLATFORM(IOS_FAMILY)
class ViewGestureController;
#endif
}

@class WKContentView;
@class WKPasswordView;
@class WKSafeBrowsingWarning;
@class WKScrollView;
@class WKWebViewContentProviderRegistry;
@class _WKFrameHandle;

#if PLATFORM(IOS_FAMILY)
@class WKFullScreenWindowController;
@protocol WKWebViewContentProvider;
#endif

#if PLATFORM(MAC)
@class WKTextFinderClient;
#endif

@protocol _WKTextManipulationDelegate;
@protocol _WKInputDelegate;

@interface WKWebView () WK_WEB_VIEW_PROTOCOLS {

@package
    RetainPtr<WKWebViewConfiguration> _configuration;
    RefPtr<WebKit::WebPageProxy> _page;

    std::unique_ptr<WebKit::NavigationState> _navigationState;
    std::unique_ptr<WebKit::UIDelegate> _uiDelegate;
    std::unique_ptr<WebKit::IconLoadingDelegate> _iconLoadingDelegate;
    std::unique_ptr<WebKit::ResourceLoadDelegate> _resourceLoadDelegate;
    std::unique_ptr<WebKit::InspectorDelegate> _inspectorDelegate;

    WeakObjCPtr<id <_WKTextManipulationDelegate>> _textManipulationDelegate;
    WeakObjCPtr<id <_WKInputDelegate>> _inputDelegate;

    RetainPtr<WKSafeBrowsingWarning> _safeBrowsingWarning;

    Optional<BOOL> _resolutionForShareSheetImmediateCompletionForTesting;

    _WKSelectionAttributes _selectionAttributes;
    _WKRenderingProgressEvents _observedRenderingProgressEvents;
    BOOL _usePlatformFindUI;

#if PLATFORM(MAC)
    std::unique_ptr<WebKit::WebViewImpl> _impl;
    RetainPtr<WKTextFinderClient> _textFinderClient;

    // Only used with UI-side compositing.
    RetainPtr<WKScrollView> _scrollView;
    RetainPtr<WKContentView> _contentView;
#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
    RetainPtr<WKScrollView> _scrollView;
    RetainPtr<WKContentView> _contentView;
    std::unique_ptr<WebKit::ViewGestureController> _gestureController;
    Vector<BlockPtr<void ()>> _visibleContentRectUpdateCallbacks;

#if ENABLE(FULLSCREEN_API)
    RetainPtr<WKFullScreenWindowController> _fullScreenWindowController;
#endif

    RetainPtr<_WKRemoteObjectRegistry> _remoteObjectRegistry;

    Optional<CGSize> _viewLayoutSizeOverride;
    Optional<WebCore::FloatSize> _lastSentViewLayoutSize;
    Optional<CGSize> _maximumUnobscuredSizeOverride;
    Optional<WebCore::FloatSize> _lastSentMaximumUnobscuredSize;
    CGRect _inputViewBounds;

    CGFloat _viewportMetaTagWidth;
    BOOL _viewportMetaTagWidthWasExplicit;
    BOOL _viewportMetaTagCameFromImageDocument;
    CGFloat _initialScaleFactor;
    BOOL _fastClickingIsDisabled;

    BOOL _allowsLinkPreview;

    UIEdgeInsets _obscuredInsets;
    BOOL _haveSetObscuredInsets;
    BOOL _isChangingObscuredInsetsInteractively;

    UIEdgeInsets _unobscuredSafeAreaInsets;
    BOOL _haveSetUnobscuredSafeAreaInsets;
    BOOL _avoidsUnsafeArea;
    UIRectEdge _obscuredInsetEdgesAffectedBySafeArea;

    UIInterfaceOrientation _interfaceOrientationOverride;
    BOOL _overridesInterfaceOrientation;
    Optional<int32_t> _lastSentDeviceOrientation;

    BOOL _allowsViewportShrinkToFit;

    BOOL _hasCommittedLoadForMainFrame;
    BOOL _needsResetViewStateAfterCommitLoadForMainFrame;
    WebKit::TransactionID _firstPaintAfterCommitLoadTransactionID;
    WebKit::TransactionID _lastTransactionID;
    WebKit::DynamicViewportUpdateMode _dynamicViewportUpdateMode;
    WebKit::DynamicViewportSizeUpdateID _currentDynamicViewportSizeUpdateID;
    CATransform3D _resizeAnimationTransformAdjustments;
    CGFloat _animatedResizeOriginalContentWidth;
    RetainPtr<UIView> _resizeAnimationView;
    CGFloat _lastAdjustmentForScroller;
    Optional<CGRect> _frozenVisibleContentRect;
    Optional<CGRect> _frozenUnobscuredContentRect;

    BOOL _commitDidRestoreScrollPosition;
    Optional<WebCore::FloatPoint> _scrollOffsetToRestore;
    WebCore::FloatBoxExtent _obscuredInsetsWhenSaved;

    Optional<WebCore::FloatPoint> _unobscuredCenterToRestore;
    Optional<WebKit::TransactionID> _firstTransactionIDAfterPageRestore;
    double _scaleToRestore;

    BOOL _allowsBackForwardNavigationGestures;

    RetainPtr<UIView <WKWebViewContentProvider>> _customContentView;
    RetainPtr<UIView> _customContentFixedOverlayView;

    RetainPtr<NSTimer> _enclosingScrollViewScrollTimer;
    BOOL _didScrollSinceLastTimerFire;

    WebCore::Color _scrollViewBackgroundColor;

    // This value tracks the current adjustment added to the bottom inset due to the keyboard sliding out from the bottom
    // when computing obscured content insets. This is used when updating the visible content rects where we should not
    // include this adjustment.
    CGFloat _totalScrollViewBottomInsetAdjustmentForKeyboard;
    BOOL _currentlyAdjustingScrollViewInsetsForKeyboard;

    BOOL _invokingUIScrollViewDelegateCallback;
    BOOL _didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback;
    BOOL _didDeferUpdateVisibleContentRectsForAnyReason;
    BOOL _didDeferUpdateVisibleContentRectsForUnstableScrollView;
    BOOL _alwaysSendNextVisibleContentRectUpdate;
    BOOL _contentViewShouldBecomeFirstResponderAfterNavigationGesture;

    BOOL _waitingForEndAnimatedResize;
    BOOL _waitingForCommitAfterAnimatedResize;

    Vector<WTF::Function<void ()>> _callbacksDeferredDuringResize;
    RetainPtr<NSMutableArray> _stableStatePresentationUpdateCallbacks;

    RetainPtr<WKPasswordView> _passwordView;

    BOOL _hasScheduledVisibleRectUpdate;
    BOOL _visibleContentRectUpdateScheduledFromScrollViewInStableState;

    _WKDragInteractionPolicy _dragInteractionPolicy;

    // For release-logging for <rdar://problem/39281269>.
    MonotonicTime _timeOfRequestForVisibleContentRectUpdate;
    MonotonicTime _timeOfLastVisibleContentRectUpdate;

    Optional<MonotonicTime> _timeOfFirstVisibleContentRectUpdateWithPendingCommit;

    NSUInteger _focusPreservationCount;
    NSUInteger _activeFocusedStateRetainCount;
#endif
}

- (BOOL)_isValid;
- (void)_didChangeEditorState;

#if ENABLE(ATTACHMENT_ELEMENT)
- (void)_didRemoveAttachment:(API::Attachment&)attachment;
- (void)_didInsertAttachment:(API::Attachment&)attachment withSource:(NSString *)source;
- (void)_didInvalidateDataForAttachment:(API::Attachment&)attachment;
#endif

- (void)_internalDoAfterNextPresentationUpdate:(void (^)(void))updateBlock withoutWaitingForPainting:(BOOL)withoutWaitingForPainting withoutWaitingForAnimatedResize:(BOOL)withoutWaitingForAnimatedResize;

- (void)_showSafeBrowsingWarning:(const WebKit::SafeBrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler;
- (void)_clearSafeBrowsingWarning;
- (void)_clearSafeBrowsingWarningIfForMainFrameNavigation;

- (Optional<BOOL>)_resolutionForShareSheetImmediateCompletionForTesting;

- (WKPageRef)_pageForTesting;
- (NakedPtr<WebKit::WebPageProxy>)_page;

@end

WKWebView* fromWebPageProxy(WebKit::WebPageProxy&);

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
@interface WKWebView (FullScreenAPI_Internal)
-(WKFullScreenWindowController *)fullScreenWindowController;
@end
#endif

#if PLATFORM(IOS_FAMILY)
@interface WKWebView (_WKWebViewPrintFormatter)
@property (nonatomic, readonly) id <_WKWebViewPrintProvider> _printProvider;
@end
#endif
