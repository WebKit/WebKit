/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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

#import "WKWebView.h"

#ifdef __cplusplus

#import "PDFPluginIdentifier.h"
#import "WKIntelligenceTextEffectCoordinator.h"
#import "WKTextAnimationType.h"
#import <WebKit/WKShareSheet.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import "_WKAttachmentInternal.h"
#import "_WKWebViewPrintFormatterInternal.h"
#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <variant>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/NakedPtr.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>

#if PLATFORM(IOS_FAMILY)
#import "DynamicViewportSizeUpdate.h"
#import "UIKitSPI.h"
#import "WKBrowserEngineDefinitions.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFullScreenWindowControllerIOS.h"
#import <WebCore/FloatRect.h>
#import <WebCore/IntDegrees.h>
#import <WebCore/LengthBox.h>
#import <WebCore/PlatformLayerIdentifier.h>
#import <WebCore/ViewportArguments.h>
#endif

#if PLATFORM(IOS_FAMILY)

#if ENABLE(WRITING_TOOLS)
#define WK_WEB_VIEW_PROTOCOLS <WKBEScrollViewDelegate, WTWritingToolsDelegate, UITextInputTraits>
#else
#define WK_WEB_VIEW_PROTOCOLS <WKBEScrollViewDelegate>
#endif

#endif

#if PLATFORM(MAC)

#if ENABLE(WRITING_TOOLS)
#define WK_WEB_VIEW_PROTOCOLS <WKShareSheetDelegate, WTWritingToolsDelegate, NSTextInputTraits>
#else
#define WK_WEB_VIEW_PROTOCOLS <WKShareSheetDelegate>
#endif

#endif

#if !defined(WK_WEB_VIEW_PROTOCOLS)
#define WK_WEB_VIEW_PROTOCOLS
#endif

#if USE(APPKIT)
using CocoaEdgeInsets = NSEdgeInsets;
#endif

#if PLATFORM(IOS_FAMILY)
using CocoaEdgeInsets = UIEdgeInsets;
#endif

typedef const struct OpaqueWKPage* WKPageRef;

namespace API {
class Attachment;
}

namespace WebCore {
struct AppHighlight;
struct ExceptionDetails;
enum class WheelScrollGestureState : uint8_t;
}

namespace WebKit {
enum class ContinueUnsafeLoad : bool;
class BrowsingWarning;
class IconLoadingDelegate;
class NavigationState;
class PointerTouchCompatibilitySimulator;
class ResourceLoadDelegate;
class UIDelegate;
class ViewSnapshot;
class WebPageProxy;
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
@class WKScrollView;
@class WKTextExtractionItem;
@class WKTextExtractionRequest;
@class WKWebViewContentProviderRegistry;
@class _WKFrameHandle;
@class _WKWarningView;

#if ENABLE(WRITING_TOOLS)
@class WTTextSuggestion;
@class WTSession;
#endif

#if PLATFORM(IOS_FAMILY)
@class WKFullScreenWindowController;
@protocol WKWebViewContentProvider;
#endif

#if PLATFORM(MAC)
@class WKTextFinderClient;
#endif

@protocol _WKTextManipulationDelegate;
@protocol _WKInputDelegate;
@protocol _WKAppHighlightDelegate;

#if PLATFORM(IOS_FAMILY)
struct LiveResizeParameters {
    CGFloat viewWidth;
    CGPoint initialScrollPosition;
};

struct OverriddenLayoutParameters {
    CGSize viewLayoutSize { CGSizeZero };
    CGSize minimumUnobscuredSize { CGSizeZero };
    CGSize maximumUnobscuredSize { CGSizeZero };
};

struct OverriddenZoomScaleParameters {
    CGFloat minimumZoomScale { 1 };
    CGFloat maximumZoomScale { 1 };
    BOOL allowUserScaling { YES };
};

// This holds state that should be reset when the web process exits.
struct PerWebProcessState {
    CGFloat viewportMetaTagWidth { WebCore::ViewportArguments::ValueAuto };
    CGFloat initialScaleFactor { 1 };
    BOOL hasCommittedLoadForMainFrame { NO };
    BOOL needsResetViewStateAfterCommitLoadForMainFrame { NO };

    WebKit::DynamicViewportUpdateMode dynamicViewportUpdateMode { WebKit::DynamicViewportUpdateMode::NotResizing };

    BOOL waitingForEndAnimatedResize { NO };
    BOOL waitingForCommitAfterAnimatedResize { NO };

    CGFloat animatedResizeOriginalContentWidth { 0 };

    CGRect animatedResizeOldBounds { CGRectZero }; // FIXME: Use std::optional<>

    std::optional<WebCore::FloatPoint> scrollOffsetToRestore;
    std::optional<WebCore::FloatPoint> unobscuredCenterToRestore;

    WebCore::Color scrollViewBackgroundColor;

    BOOL isAnimatingFullScreenExit { NO };

    BOOL invokingUIScrollViewDelegateCallback { NO };

    BOOL didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback { NO };
    BOOL didDeferUpdateVisibleContentRectsForAnyReason { NO };
    BOOL didDeferUpdateVisibleContentRectsForUnstableScrollView { NO };

    BOOL currentlyAdjustingScrollViewInsetsForKeyboard { NO };

    BOOL hasScheduledVisibleRectUpdate { NO };
    BOOL commitDidRestoreScrollPosition { NO };

    BOOL avoidsUnsafeArea { YES };

    BOOL viewportMetaTagWidthWasExplicit { NO };
    BOOL viewportMetaTagCameFromImageDocument { NO };

    std::optional<WebCore::FloatSize> lastSentViewLayoutSize;
    std::optional<WebCore::IntDegrees> lastSentDeviceOrientation;
    std::optional<WebCore::IntDegrees> lastSentOrientationForMediaCapture;
    std::optional<CGFloat> lastSentMinimumEffectiveDeviceWidth;

    std::optional<CGRect> frozenVisibleContentRect;
    std::optional<CGRect> frozenUnobscuredContentRect;

    WebKit::TransactionID firstPaintAfterCommitLoadTransactionID;
    WebKit::TransactionID lastTransactionID;

    std::optional<WebKit::TransactionID> firstTransactionIDAfterPageRestore;

    Markable<WebCore::PlatformLayerIdentifier> pendingFindLayerID;
    Markable<WebCore::PlatformLayerIdentifier> committedFindLayerID;

    std::optional<LiveResizeParameters> liveResizeParameters;
};

#endif // PLATFORM(IOS_FAMILY)

@interface WKWebView () WK_WEB_VIEW_PROTOCOLS {

@package
    RetainPtr<WKWebViewConfiguration> _configuration;
    RefPtr<WebKit::WebPageProxy> _page;

    std::unique_ptr<WebKit::NavigationState> _navigationState;
    std::unique_ptr<WebKit::UIDelegate> _uiDelegate;
    std::unique_ptr<WebKit::IconLoadingDelegate> _iconLoadingDelegate;
    std::unique_ptr<WebKit::ResourceLoadDelegate> _resourceLoadDelegate;

    WeakObjCPtr<id <_WKTextManipulationDelegate>> _textManipulationDelegate;
    WeakObjCPtr<id <_WKInputDelegate>> _inputDelegate;
    WeakObjCPtr<id <_WKAppHighlightDelegate>> _appHighlightDelegate;

    RetainPtr<_WKWarningView> _warningView;

    std::optional<BOOL> _resolutionForShareSheetImmediateCompletionForTesting;

    _WKSelectionAttributes _selectionAttributes;
    _WKRenderingProgressEvents _observedRenderingProgressEvents;
    BOOL _usePlatformFindUI;

    CocoaEdgeInsets _minimumViewportInset;
    CocoaEdgeInsets _maximumViewportInset;

#if ENABLE(WRITING_TOOLS)
    RetainPtr<NSMapTable<NSUUID *, WTTextSuggestion *>> _writingToolsTextSuggestions;
    RetainPtr<WTSession> _activeWritingToolsSession;

    RetainPtr<WKIntelligenceTextEffectCoordinator> _intelligenceTextEffectCoordinator;

    NSUInteger _partialIntelligenceTextAnimationCount;
    BOOL _writingToolsTextReplacementsFinished;
#endif

#if PLATFORM(MAC)
    std::unique_ptr<WebKit::WebViewImpl> _impl;
    RetainPtr<WKTextFinderClient> _textFinderClient;
#if HAVE(NSWINDOW_SNAPSHOT_READINESS_HANDLER)
    BlockPtr<void()> _windowSnapshotReadinessHandler;
#endif
#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
    RetainPtr<WKScrollView> _scrollView;
    RetainPtr<WKContentView> _contentView;
    RefPtr<WebKit::ViewGestureController> _gestureController;
    Vector<BlockPtr<void ()>> _visibleContentRectUpdateCallbacks;
    RetainPtr<WKWebViewContentProviderRegistry> _contentProviderRegistry;
#if ENABLE(FULLSCREEN_API)
    RetainPtr<WKFullScreenWindowController> _fullScreenWindowController;
#endif

    BOOL _findInteractionEnabled;
#if HAVE(UIFINDINTERACTION)
    struct FindOverlays {
        RetainPtr<UIView> top;
        RetainPtr<UIView> right;
        RetainPtr<UIView> bottom;
        RetainPtr<UIView> left;
    };
    std::optional<FindOverlays> _findOverlaysOutsideContentView;
    RetainPtr<UIFindInteraction> _findInteraction;
#endif

    RetainPtr<_WKRemoteObjectRegistry> _remoteObjectRegistry;
    
    PerWebProcessState _perProcessState;

    std::optional<OverriddenLayoutParameters> _overriddenLayoutParameters;
    std::optional<OverriddenZoomScaleParameters> _overriddenZoomScaleParameters;
    CGRect _inputViewBoundsInWindow;

    BOOL _fastClickingIsDisabled;
    BOOL _allowsLinkPreview;

    UIEdgeInsets _obscuredInsets;
    BOOL _haveSetObscuredInsets;
    BOOL _isChangingObscuredInsetsInteractively;

    UIEdgeInsets _unobscuredSafeAreaInsets;
    BOOL _haveSetUnobscuredSafeAreaInsets;
    BOOL _needsToPresentLockdownModeMessage;
    UIRectEdge _obscuredInsetEdgesAffectedBySafeArea;
    UIInterfaceOrientationMask _supportedInterfaceOrientations;

    UIInterfaceOrientation _interfaceOrientationOverride;
    BOOL _overridesInterfaceOrientation;

    BOOL _allowsViewportShrinkToFit;

    WebKit::DynamicViewportSizeUpdateID _currentDynamicViewportSizeUpdateID;
    CATransform3D _resizeAnimationTransformAdjustments;
    CGFloat _animatedResizeOldMinimumEffectiveDeviceWidth;
    WebCore::IntDegrees _animatedResizeOldOrientation;
    UIEdgeInsets _animatedResizeOldObscuredInsets;
    RetainPtr<UIView> _resizeAnimationView;
    CGFloat _lastAdjustmentForScroller;

    std::pair<CGSize, UIInterfaceOrientation> _lastKnownWindowSizeAndOrientation;
    RetainPtr<NSTimer> _endLiveResizeTimer;

    WebCore::FloatBoxExtent _obscuredInsetsWhenSaved;

    double _scaleToRestore;

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    Vector<RetainPtr<id<_UIInvalidatable>>> _resizeAssertions;
#endif

    BOOL _allowsBackForwardNavigationGestures;

    RetainPtr<UIView <WKWebViewContentProvider>> _customContentView;
    RetainPtr<UIView> _customContentFixedOverlayView;

    RetainPtr<NSTimer> _enclosingScrollViewScrollTimer;
    BOOL _didScrollSinceLastTimerFire;


    // This value tracks the current adjustment added to the bottom inset due to the keyboard sliding out from the bottom
    // when computing obscured content insets. This is used when updating the visible content rects where we should not
    // include this adjustment.
    CGFloat _totalScrollViewBottomInsetAdjustmentForKeyboard;

    BOOL _alwaysSendNextVisibleContentRectUpdate;
    BOOL _contentViewShouldBecomeFirstResponderAfterNavigationGesture;


    Vector<WTF::Function<void ()>> _callbacksDeferredDuringResize;
    RetainPtr<NSMutableArray> _stableStatePresentationUpdateCallbacks;

    RetainPtr<WKPasswordView> _passwordView;

    OptionSet<WebKit::ViewStabilityFlag> _viewStabilityWhenVisibleContentRectUpdateScheduled;

    std::optional<WebCore::WheelScrollGestureState> _currentScrollGestureState;
    uint64_t _wheelEventCountInCurrentScrollGesture;

    _WKDragInteractionPolicy _dragInteractionPolicy;

    // For release-logging for <rdar://problem/39281269>.
    MonotonicTime _timeOfRequestForVisibleContentRectUpdate;
    MonotonicTime _timeOfLastVisibleContentRectUpdate;

    std::optional<MonotonicTime> _timeOfFirstVisibleContentRectUpdateWithPendingCommit;

    NSUInteger _focusPreservationCount;
    NSUInteger _activeFocusedStateRetainCount;

    RetainPtr<NSArray<NSNumber *>> _scrollViewDefaultAllowedTouchTypes;
    std::unique_ptr<WebKit::PointerTouchCompatibilitySimulator> _pointerTouchCompatibilitySimulator;
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(VISION)
    String _defaultSTSLabel;
#endif

    BOOL _didAccessBackForwardList;
    BOOL _dontResetTransientActivationAfterRunJavaScript;

#if ENABLE(PAGE_LOAD_OBSERVER)
    RetainPtr<NSString> _pendingPageLoadObserverHost;
#endif

#if ENABLE(GAMEPAD)
    RetainPtr<id> _gamepadsRecentlyAccessedState;
#endif

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    BOOL _isScrollingWithOverlayRegion;
#endif
}

- (BOOL)_isValid;
- (void)_didChangeEditorState;

#if PLATFORM(MAC) && HAVE(NSWINDOW_SNAPSHOT_READINESS_HANDLER)
- (void)_invalidateWindowSnapshotReadinessHandler;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
- (void)_didRemoveAttachment:(API::Attachment&)attachment;
- (void)_didInsertAttachment:(API::Attachment&)attachment withSource:(NSString *)source;
- (void)_didInvalidateDataForAttachment:(API::Attachment&)attachment;
#endif

#if ENABLE(APP_HIGHLIGHTS)
- (void)_storeAppHighlight:(const WebCore::AppHighlight&)info;
#endif

#if ENABLE(WRITING_TOOLS)
- (void)_proofreadingSessionShowDetailsForSuggestionWithUUID:(NSUUID *)replacementUUID relativeToRect:(CGRect)rect;

- (void)_proofreadingSessionUpdateState:(WebCore::WritingTools::TextSuggestionState)state forSuggestionWithUUID:(NSUUID *)replacementUUID;

#if PLATFORM(MAC)
// FIXME: (rdar://130540028) Remove uses of the old WritingToolsAllowedInputOptions API in favor of the new WritingToolsResultOptions API, and remove staging.
- (PlatformWritingToolsResultOptions)writingToolsAllowedInputOptions;
- (PlatformWritingToolsResultOptions)allowedWritingToolsResultOptions;
#else
// FIXME: (rdar://130540028) Remove uses of the old WritingToolsAllowedInputOptions API in favor of the new WritingToolsResultOptions API, and remove staging.
- (PlatformWritingToolsResultOptions)writingToolsAllowedInputOptions;
- (PlatformWritingToolsResultOptions)allowedWritingToolsResultOptions;
#endif

- (void)_didEndPartialIntelligenceTextAnimation;
- (BOOL)_writingToolsTextReplacementsFinished;

- (void)_addTextAnimationForAnimationID:(NSUUID *)uuid withData:(const WebCore::TextAnimationData&)styleData;
- (void)_removeTextAnimationForAnimationID:(NSUUID *)uuid;

#endif

- (void)_internalDoAfterNextPresentationUpdate:(void (^)(void))updateBlock withoutWaitingForPainting:(BOOL)withoutWaitingForPainting withoutWaitingForAnimatedResize:(BOOL)withoutWaitingForAnimatedResize;

- (void)_doAfterNextVisibleContentRectAndPresentationUpdate:(void (^)(void))updateBlock;

- (void)_recalculateViewportSizesWithMinimumViewportInset:(CocoaEdgeInsets)minimumViewportInset maximumViewportInset:(CocoaEdgeInsets)maximumViewportInset throwOnInvalidInput:(BOOL)throwOnInvalidInput;

- (void)_showWarningView:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(std::variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler;
- (void)_showBrowsingWarning:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(std::variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler;
- (void)_clearWarningView;
- (void)_clearBrowsingWarning;
- (void)_clearWarningViewIfForMainFrameNavigation;
- (void)_clearBrowsingWarningIfForMainFrameNavigation;

- (std::optional<BOOL>)_resolutionForShareSheetImmediateCompletionForTesting;

- (void)_didAccessBackForwardList NS_DIRECT;

#if ENABLE(GAMEPAD)
- (void)_setGamepadsRecentlyAccessed:(BOOL)gamepadsRecentlyAccessed;

#if PLATFORM(VISION)
@property (nonatomic, readonly) BOOL _gamepadsConnected;
- (void)_gamepadsConnectedStateChanged;
- (void)_setAllowGamepadsInput:(BOOL)allowGamepadsInput;
- (void)_setAllowGamepadsAccess;
#endif
#endif

- (WKPageRef)_pageForTesting;
- (NakedPtr<WebKit::WebPageProxy>)_page;
- (RefPtr<WebKit::WebPageProxy>)_protectedPage;

@property (nonatomic, setter=_setHasActiveNowPlayingSession:) BOOL _hasActiveNowPlayingSession;

@end

RetainPtr<NSError> nsErrorFromExceptionDetails(const WebCore::ExceptionDetails&);

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
@interface WKWebView (FullScreenAPI_Internal)
- (WKFullScreenWindowController *)fullScreenWindowController;
#if PLATFORM(VISION)
- (UIMenu *)fullScreenWindowSceneDimmingAction;
#endif
@end
#endif

#if PLATFORM(IOS_FAMILY)
@interface WKWebView (_WKWebViewPrintFormatter)
@property (nonatomic, readonly) id <_WKWebViewPrintProvider> _printProvider;
@end
#endif

@interface WKWebView (WKTextExtraction)
- (void)_requestTextExtractionForSwift:(WKTextExtractionRequest *)context;
- (void)_requestTextExtraction:(CGRect)rect completionHandler:(void(^)(WKTextExtractionItem *))completionHandler;
@end

#endif // __cplusplus

@interface WKWebView (NonCpp)

@end
