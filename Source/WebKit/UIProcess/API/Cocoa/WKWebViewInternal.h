/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#import <WebKit/WKWebView.h>
#import <WebKit/_WKTextExtraction.h>

#ifdef __cplusplus
#if !__has_feature(modules)

#import "PDFPluginIdentifier.h"
#import <WebCore/CocoaView.h>
#import <WebCore/CocoaWritingToolsTypes.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/FixedContainerEdges.h>
#import <WebKit/WKShareSheet.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import "_WKAttachmentInternal.h"
#import "_WKTextExtractionInternal.h"
#import "_WKWebViewPrintFormatterInternal.h"
#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/HashMap.h>
#import <wtf/NakedPtr.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/Variant.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>

#if ENABLE(SCREEN_TIME)
#import <ScreenTime/STWebpageController.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "DynamicViewportSizeUpdate.h"
#import "UIKitSPI.h"
#import "WKBrowserEngineDefinitions.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFullScreenWindowControllerIOS.h"
#import <WebCore/BoxExtents.h>
#import <WebCore/FloatRect.h>
#import <WebCore/IntDegrees.h>
#import <WebCore/PlatformLayerIdentifier.h>
#import <WebCore/ViewportArguments.h>
#endif

#if PLATFORM(IOS_FAMILY)

#if ENABLE(WRITING_TOOLS)
#define WK_WEB_VIEW_PROTOCOLS <WKBEScrollViewDelegate, WTWritingToolsDelegate, UITextInputTraits>
#else
#define WK_WEB_VIEW_PROTOCOLS <WKBEScrollViewDelegate>
#endif // ENABLE(WRITING_TOOLS)

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

#if ENABLE(WRITING_TOOLS)
#define WK_WEB_VIEW_PROTOCOLS <WKShareSheetDelegate, WTWritingToolsDelegate, NSTextInputTraits>
#else
#define WK_WEB_VIEW_PROTOCOLS <WKShareSheetDelegate>
#endif // ENABLE(WRITING_TOOLS)

#endif // PLATFORM(MAC)

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
struct ExceptionData;
struct ExceptionDetails;
struct TextAnimationData;
enum class BoxSide : uint8_t;
enum class WheelScrollGestureState : uint8_t;

namespace WritingTools {
enum class TextSuggestionState : uint8_t;
}

#if HAVE(DIGITAL_CREDENTIALS_UI)
struct DigitalCredentialsRequestData;
struct DigitalCredentialsResponseData;
struct MobileDocumentRequest;
struct OpenID4VPRequest;
#endif

struct NodeIdentifierType;
using NodeIdentifier = ObjectIdentifier<NodeIdentifierType>;
} // namespace WebCore

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
enum class HideScrollPocketReason : uint8_t {
    FullScreen          = 1 << 0,
    ScrolledToTop       = 1 << 1,
    SiteSpecificQuirk   = 1 << 2,
};

enum class PreferSolidColorHardPocketReason : uint8_t {
    AttachedInspector   = 1 << 0,
    RequestedByClient   = 1 << 1,
};
}

@class NSScrollPocket;
@class WKColorExtensionView;
@class WKContentView;
@class WKPasswordView;
@class WKScrollGeometry;
@class WKScrollView;
@class WKTextExtractionInteraction;
@class WKWebViewContentProviderRegistry;
@class _WKFrameHandle;
@class _WKWarningView;

#if HAVE(DIGITAL_CREDENTIALS_UI)
@class WKDigitalCredentialsPicker;
#endif

#if ENABLE(SCREEN_TIME)
@class WKScreenTimeConfigurationObserver;
#endif

#if ENABLE(WRITING_TOOLS)
@class WTTextSuggestion;
@class WTSession;
@protocol WKIntelligenceTextEffectCoordinating;
#endif

#if PLATFORM(IOS_FAMILY)
@class WKFullScreenWindowController;
@protocol WKWebViewContentProvider;
#endif

#if PLATFORM(MAC)
@class WKTextFinderClient;
#endif

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
@class WKPDFPageNumberIndicator;
#endif

@protocol _WKTextManipulationDelegate;
@protocol _WKInputDelegate;
@protocol _WKAppHighlightDelegate;

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
@protocol _WKImmersiveEnvironmentDelegate;
#endif

enum class SimilarToOriginalTextTag : uint8_t { Value };
using TextValidationMapValue = Variant<String, SimilarToOriginalTextTag>;

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

// This holds state that should be reset when the web process exits.
struct PerWebProcessState {
    CGFloat viewportMetaTagWidth { WebCore::ViewportArguments::ValueAuto };
    CGFloat initialScaleFactor { 1 };
    BOOL hasCommittedLoadForMainFrame { NO };

    WebKit::DynamicViewportUpdateMode dynamicViewportUpdateMode { WebKit::DynamicViewportUpdateMode::NotResizing };

    WebCore::InteractiveWidget viewportMetaTagInteractiveWidget { WebCore::InteractiveWidget::ResizesVisual };

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

    BOOL viewportMetaTagWidthWasExplicit { NO };
    BOOL viewportMetaTagCameFromImageDocument { NO };
    BOOL lastTransactionWasInStableState { NO };

    std::optional<WebCore::FloatSize> lastSentViewLayoutSize;
    std::optional<WebCore::IntDegrees> lastSentDeviceOrientation;
    std::optional<WebCore::IntDegrees> lastSentOrientationForMediaCapture;
    std::optional<CGFloat> lastSentMinimumEffectiveDeviceWidth;

    std::optional<CGRect> frozenVisibleContentRect;
    std::optional<CGRect> frozenUnobscuredContentRect;

    std::optional<WebKit::TransactionID> resetViewStateAfterTransactionID;
    std::optional<WebKit::TransactionID> lastTransactionID;

    std::optional<WebKit::TransactionID> firstTransactionIDAfterPageRestore;

    Markable<WebCore::PlatformLayerIdentifier> pendingFindLayerID;
    Markable<WebCore::PlatformLayerIdentifier> committedFindLayerID;

    std::optional<LiveResizeParameters> liveResizeParameters;

    std::optional<WebKit::TransactionID> firstTransactionIDAfterObscuredInsetChange;
};

#endif // PLATFORM(IOS_FAMILY)

@interface WKWebView () WK_WEB_VIEW_PROTOCOLS {

@package
    RetainPtr<WKWebViewConfiguration> _configuration;
    const RefPtr<WebKit::WebPageProxy> _page;

    const std::unique_ptr<WebKit::NavigationState> _navigationState;
    const std::unique_ptr<WebKit::UIDelegate> _uiDelegate;
    const std::unique_ptr<WebKit::IconLoadingDelegate> _iconLoadingDelegate;
    const std::unique_ptr<WebKit::ResourceLoadDelegate> _resourceLoadDelegate;

    WeakObjCPtr<id <_WKTextManipulationDelegate>> _textManipulationDelegate;
    WeakObjCPtr<id <_WKInputDelegate>> _inputDelegate;
    WeakObjCPtr<id <_WKAppHighlightDelegate>> _appHighlightDelegate;

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    WeakObjCPtr<id <_WKImmersiveEnvironmentDelegate>> _immersiveEnvironmentDelegate;
#endif

    RetainPtr<_WKWarningView> _warningView;

    std::optional<BOOL> _resolutionForShareSheetImmediateCompletionForTesting;

    _WKSelectionAttributes _selectionAttributes;
    _WKRenderingProgressEvents _observedRenderingProgressEvents;
    BOOL _usePlatformFindUI;
    BOOL _usesAutomaticContentInsetBackgroundFill;
    BOOL _shouldSuppressTopColorExtensionView;
#if PLATFORM(MAC)
    OptionSet<WebKit::PreferSolidColorHardPocketReason> _preferSolidColorHardPocketReasons;
    BOOL _isGettingAdjustedColorForTopContentInsetColorFromDelegate;
    RetainPtr<NSColor> _overrideTopScrollEdgeEffectColor;
#endif

    CocoaEdgeInsets _minimumViewportInset;
    CocoaEdgeInsets _maximumViewportInset;

#if ENABLE(WRITING_TOOLS)
    RetainPtr<NSMapTable<NSUUID *, WTTextSuggestion *>> _writingToolsTextSuggestions;
    RetainPtr<WTSession> _activeWritingToolsSession;

    RetainPtr<id<WKIntelligenceTextEffectCoordinating>> _intelligenceTextEffectCoordinator;

    NSUInteger _partialIntelligenceTextAnimationCount;
    BOOL _writingToolsTextReplacementsFinished;
#endif

#if ENABLE(SCREEN_TIME)
    RetainPtr<STWebpageController> _screenTimeWebpageController;
    RetainPtr<WKScreenTimeConfigurationObserver> _screenTimeConfigurationObserver;
#if PLATFORM(MAC)
    RetainPtr<NSVisualEffectView> _screenTimeBlurredSnapshot;
#else
    RetainPtr<UIVisualEffectView> _screenTimeBlurredSnapshot;
#endif
#endif

#if PLATFORM(MAC)
    const std::unique_ptr<WebKit::WebViewImpl> _impl;
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

#if HAVE(UI_CONVERSATION_CONTEXT)
    RetainPtr<UIConversationContext> _conversationContextFromClient;
#endif

    RetainPtr<_WKRemoteObjectRegistry> _remoteObjectRegistry;
    
    PerWebProcessState _perProcessState;

    std::optional<OverriddenLayoutParameters> _overriddenLayoutParameters;
#if PLATFORM(IOS_FAMILY)
    BOOL _forcesInitialScaleFactor;
    BOOL _automaticallyAdjustsViewLayoutSizesWithObscuredInset;
    BOOL _avoidsUnsafeArea;
#endif
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
    BOOL _needsScrollend;

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

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
    RetainPtr<_WKSpatialBackdropSource> _cachedSpatialBackdropSource;
#endif

    BOOL _didAccessBackForwardList;
    BOOL _dontResetTransientActivationAfterRunJavaScript;

#if ENABLE(PAGE_LOAD_OBSERVER)
    RetainPtr<NSString> _pendingPageLoadObserverHost;
#endif

#if ENABLE(GAMEPAD)
    BOOL _gamepadsRecentlyAccessed;
    RetainPtr<id> _gamepadsRecentlyAccessedState;
#endif

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    BOOL _isScrollingWithOverlayRegion;
#endif

    WebCore::FixedContainerEdges _fixedContainerEdges;

    RetainPtr<WKScrollGeometry> _currentScrollGeometry;

    BOOL _allowsMagnification;

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    std::pair<Markable<WebKit::PDFPluginIdentifier>, RetainPtr<WKPDFPageNumberIndicator>> _pdfPageNumberIndicator;
#endif

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    WebCore::RectEdges<RetainPtr<WKColorExtensionView>> _fixedColorExtensionViews;
    OptionSet<WebKit::HideScrollPocketReason> _reasonsToHideTopScrollPocket;
    BOOL _needsTopScrollPocketDueToVisibleContentInset;
    BOOL _shouldUpdateNeedsTopScrollPocketDueToVisibleContentInset;
#endif

#if ENABLE(TEXT_EXTRACTION_FILTER)
    HashMap<unsigned /* string hash */, TextValidationMapValue> _textValidationCache;
#endif
}

- (BOOL)_isValid;
- (void)_didChangeEditorState;

#if PLATFORM(MAC) && HAVE(NSWINDOW_SNAPSHOT_READINESS_HANDLER)
- (void)_invalidateWindowSnapshotReadinessHandler;
#endif

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
- (void)_spatialBackdropSourceDidChange;
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
- (void)_canEnterImmersiveElementFromURL:(const URL&)url completion:(CompletionHandler<void(bool)>&&)completion;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
- (void)_didRemoveAttachment:(API::Attachment&)attachment;
- (void)_didInsertAttachment:(API::Attachment&)attachment withSource:(NSString *)source;
- (void)_didInvalidateDataForAttachment:(API::Attachment&)attachment;
#endif

#if ENABLE(APP_HIGHLIGHTS)
- (void)_storeAppHighlight:(const WebCore::AppHighlight&)info;
#endif

#if ENABLE(SCREEN_TIME)
- (void)_installScreenTimeWebpageControllerIfNeeded;
- (void)_uninstallScreenTimeWebpageController;

- (void)_updateScreenTimeViewGeometry;
- (void)_updateScreenTimeBasedOnWindowVisibility;
#endif

#if ENABLE(WRITING_TOOLS)
- (void)_proofreadingSessionShowDetailsForSuggestionWithUUID:(NSUUID *)replacementUUID relativeToRect:(CGRect)rect;

- (void)_proofreadingSessionUpdateState:(WebCore::WritingTools::TextSuggestionState)state forSuggestionWithUUID:(NSUUID *)replacementUUID;

- (CocoaWritingToolsResultOptions)allowedWritingToolsResultOptions;

- (void)_didEndPartialIntelligenceTextAnimation;
- (BOOL)_writingToolsTextReplacementsFinished;

- (void)_addTextAnimationForAnimationID:(NSUUID *)uuid withData:(const WebCore::TextAnimationData&)styleData;
- (void)_removeTextAnimationForAnimationID:(NSUUID *)uuid;

#endif

- (void)_internalDoAfterNextPresentationUpdate:(void (^)(void))updateBlock withoutWaitingForPainting:(BOOL)withoutWaitingForPainting withoutWaitingForAnimatedResize:(BOOL)withoutWaitingForAnimatedResize;

- (void)_doAfterNextVisibleContentRectAndPresentationUpdate:(void (^)(void))updateBlock;

- (void)_recalculateViewportSizesWithMinimumViewportInset:(CocoaEdgeInsets)minimumViewportInset maximumViewportInset:(CocoaEdgeInsets)maximumViewportInset throwOnInvalidInput:(BOOL)throwOnInvalidInput;

- (void)_showWarningView:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler;
- (void)_showBrowsingWarning:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler;
- (void)_clearWarningView;
- (void)_clearBrowsingWarning;
- (void)_clearWarningViewIfForMainFrameNavigation;
- (void)_clearBrowsingWarningIfForMainFrameNavigation;

- (std::optional<BOOL>)_resolutionForShareSheetImmediateCompletionForTesting;

- (void)_didAccessBackForwardList NS_DIRECT;

#if HAVE(DIGITAL_CREDENTIALS_UI)
- (void)_showDigitalCredentialsPicker:(const WebCore::DigitalCredentialsRequestData&)requestData completionHandler:(WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&)completionHandler;
- (void)_dismissDigitalCredentialsPicker:(WTF::CompletionHandler<void(bool)>&&)completionHandler;
#endif

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
- (void)_updateFixedColorExtensionViews;
- (void)_updateFixedColorExtensionViewFrames;
- (void)_updatePrefersSolidColorHardPocket;
- (BOOL)_hasVisibleColorExtensionView:(WebCore::BoxSide)side;
- (void)_addReasonToHideTopScrollPocket:(WebKit::HideScrollPocketReason)reason;
- (void)_removeReasonToHideTopScrollPocket:(WebKit::HideScrollPocketReason)reason;
- (void)_updateTopScrollPocketCaptureColor;
- (void)_updateHiddenScrollPocketEdges;
- (void)_doAfterAdjustingColorForTopContentInsetFromUIDelegate:(Function<void()>&&)callback;
#endif

#if PLATFORM(MAC) && ENABLE(CONTENT_INSET_BACKGROUND_FILL)
- (NSColor *)_adjustedColorForTopContentInsetColorFromUIDelegate:(NSColor *)proposedColor;
@property (nonatomic, readonly) RetainPtr<NSScrollPocket> _copyTopScrollPocket;
- (void)_addReasonToPreferSolidColorHardPocket:(WebKit::PreferSolidColorHardPocketReason)reason;
- (void)_removeReasonToPreferSolidColorHardPocket:(WebKit::PreferSolidColorHardPocketReason)reason;
#endif

#if ENABLE(GAMEPAD)
- (void)_setGamepadsRecentlyAccessed:(BOOL)gamepadsRecentlyAccessed;

#if PLATFORM(VISION)
@property (nonatomic, readonly) BOOL _gamepadsConnected;
- (void)_gamepadsConnectedStateChanged;
- (void)_setAllowGamepadsInput:(BOOL)allowGamepadsInput;
- (void)_setAllowGamepadsAccess;
#endif
#endif

- (void)_updateFixedContainerEdges:(const WebCore::FixedContainerEdges&)edges;
- (void)_updateScrollGeometryWithContentOffset:(CGPoint)contentOffset contentSize:(CGSize)contentSize;

- (WKPageRef)_pageForTesting;
- (NakedPtr<WebKit::WebPageProxy>)_page;
- (RefPtr<WebKit::WebPageProxy>)_protectedPage;
#if ENABLE(SCREEN_TIME)
- (STWebpageController *)_screenTimeWebpageController;
#if PLATFORM(MAC)
- (NSVisualEffectView *)_screenTimeBlurredSnapshot;
#else
- (UIVisualEffectView *)_screenTimeBlurredSnapshot;
#endif
#endif

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)

- (void)_createPDFPageNumberIndicator:(WebKit::PDFPluginIdentifier)identifier withFrame:(CGRect)rect pageCount:(size_t)pageCount;
- (void)_removePDFPageNumberIndicator:(WebKit::PDFPluginIdentifier)identifier;
- (void)_updatePDFPageNumberIndicator:(WebKit::PDFPluginIdentifier)identifier withFrame:(CGRect)rect;
- (void)_updatePDFPageNumberIndicator:(WebKit::PDFPluginIdentifier)identifier currentPage:(size_t)pageIndex;
- (void)_updatePDFPageNumberIndicatorIfNeeded;
- (void)_removeAnyPDFPageNumberIndicator;

#endif

@property (nonatomic, setter=_setHasActiveNowPlayingSession:) BOOL _hasActiveNowPlayingSession;

@property (nonatomic, readonly) RetainPtr<WKWebView> _horizontallyAttachedInspectorWebView;

@end

RetainPtr<NSError> nsErrorFromExceptionDetails(const std::optional<WebCore::ExceptionDetails>&);

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

WebCore::CocoaColor *sampledFixedPositionContentColor(const WebCore::FixedContainerEdges&, WebCore::BoxSide);

#if ENABLE(TEXT_EXTRACTION_FILTER)

@interface WKWebView (TextExtractionFilter)
- (void)_clearTextExtractionFilterCache;
@end

#endif

#endif // !__has_feature(modules)
#endif // __cplusplus

@class WKTextExtractionItem;

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@interface WKWebView (NonCpp)

#if PLATFORM(MAC)
@property (nonatomic, setter=_setAlwaysBounceVertical:) BOOL _alwaysBounceVertical;
@property (nonatomic, setter=_setAlwaysBounceHorizontal:) BOOL _alwaysBounceHorizontal;

- (void)_setContentOffsetX:(nullable NSNumber *)x y:(nullable NSNumber *)y animated:(BOOL)animated NS_SWIFT_NAME(_setContentOffset(x:y:animated:));
#endif

#if PLATFORM(IOS_FAMILY)
@property (nonatomic, setter=_setAllowsMagnification:) BOOL _allowsMagnification;
#endif

@property (nonatomic, readonly) NSString *_nameForVisualIdentificationOverlay;

- (void)_setNeedsScrollGeometryUpdates:(BOOL)needsScrollGeometryUpdates;

- (void)_scrollToEdge:(_WKRectEdge)edge animated:(BOOL)animated;

- (void)_requestTextExtraction:(_WKTextExtractionConfiguration *)configuration completionHandler:(NS_SWIFT_UI_ACTOR void (^)(WKTextExtractionItem * _Nullable))completionHandler;

- (void)_describeInteraction:(_WKTextExtractionInteraction *)interaction completionHandler:(NS_SWIFT_UI_ACTOR void (^)(NSString * NS_NULLABLE_RESULT, NSError * _Nullable))completionHandler WK_SWIFT_ASYNC_NAME(_describe(interaction:));

@end

NS_HEADER_AUDIT_END(nullability, sendability)
