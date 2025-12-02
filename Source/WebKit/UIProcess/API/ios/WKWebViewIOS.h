/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#import "WKBaseScrollView.h"
#import "WKWebViewInternal.h"
#import "_WKTapHandlingResult.h"
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"

@class WKBEScrollViewScrollUpdate;

namespace WebKit {
enum class TapHandlingResult : uint8_t;
}

@interface WKWebView (WKViewInternalIOS) <WKBaseScrollViewDelegate>

- (void)_setupScrollAndContentViews;
- (void)_registerForNotifications;

- (void)_keyboardWillChangeFrame:(NSNotification *)notification;
- (void)_keyboardDidChangeFrame:(NSNotification *)notification;
- (void)_keyboardWillShow:(NSNotification *)notification;
- (void)_keyboardDidShow:(NSNotification *)notification;
- (void)_keyboardWillHide:(NSNotification *)notification;
- (void)_windowDidRotate:(NSNotification *)notification;
- (void)_contentSizeCategoryDidChange:(NSNotification *)notification;
- (void)_accessibilitySettingsDidChange:(NSNotification *)notification;

- (void)_frameOrBoundsMayHaveChanged;
- (BOOL)_shouldDeferGeometryUpdates;
#if HAVE(UIKIT_RESIZABLE_WINDOWS)
- (void)_invalidateResizeAssertions;
#endif

- (BOOL)usesStandardContentView;

- (void)_processDidExit;
- (void)_processWillSwap;
- (void)_didRelaunchProcess;

- (WKScrollView *)_wkScrollView;
- (UIView *)_currentContentView;

- (void)_didCommitLoadForMainFrame;
- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction mainFrameData:(const std::optional<WebKit::MainFrameData>&)mainFrameData pageData:(const WebKit::PageData&)pageData transactionID:(const WebKit::TransactionID&)transactionID;
- (void)_layerTreeCommitComplete;

- (void)_couldNotRestorePageState;
- (void)_restorePageScrollPosition:(std::optional<WebCore::FloatPoint>)scrollPosition scrollOrigin:(WebCore::FloatPoint)scrollOrigin previousObscuredInset:(WebCore::FloatBoxExtent)insets scale:(double)scale;
- (void)_restorePageStateToUnobscuredCenter:(std::optional<WebCore::FloatPoint>)center scale:(double)scale; // FIXME: needs scroll origin?

- (RefPtr<WebKit::ViewSnapshot>)_takeViewSnapshot;

- (void)_scrollToContentScrollPosition:(WebCore::FloatPoint)scrollPosition scrollOrigin:(WebCore::IntPoint)scrollOrigin animated:(BOOL)animated;
- (BOOL)_scrollToRect:(WebCore::FloatRect)targetRect origin:(WebCore::FloatPoint)origin minimumScrollDistance:(float)minimumScrollDistance;

- (double)_initialScaleFactor;
- (double)_contentZoomScale;

- (double)_targetContentZoomScaleForRect:(const WebCore::FloatRect&)targetRect currentScale:(double)currentScale fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale;
- (void)_zoomToFocusRect:(const WebCore::FloatRect&)focusedElementRect selectionRect:(const WebCore::FloatRect&)selectionRectInDocumentCoordinates fontSize:(float)fontSize minimumScale:(double)minimumScale maximumScale:(double)maximumScale allowScaling:(BOOL)allowScaling forceScroll:(BOOL)forceScroll;
- (BOOL)_zoomToRect:(WebCore::FloatRect)targetRect withOrigin:(WebCore::FloatPoint)origin fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale minimumScrollDistance:(float)minimumScrollDistance;
- (void)_zoomOutWithOrigin:(WebCore::FloatPoint)origin animated:(BOOL)animated;
- (void)_zoomToInitialScaleWithOrigin:(WebCore::FloatPoint)origin animated:(BOOL)animated;
- (void)_didFinishScrolling:(UIScrollView *)scrollView;

- (void)_setHasCustomContentView:(BOOL)hasCustomContentView loadedMIMEType:(const WTF::String&)mimeType;
- (void)_didFinishLoadingDataForCustomContentProviderWithSuggestedFilename:(const WTF::String&)suggestedFilename data:(NSData *)data;

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
- (void)_updateOverlayRegions;
#endif

- (void)_willInvokeUIScrollViewDelegateCallback;
- (void)_didInvokeUIScrollViewDelegateCallback;

- (void)_scheduleVisibleContentRectUpdate;
- (void)_scheduleForcedVisibleContentRectUpdate;

- (void)_didCompleteAnimatedResize;

- (void)_didStartProvisionalLoadForMainFrame;
- (void)_didFinishNavigation:(API::Navigation*)navigation;
- (void)_didFailNavigation:(API::Navigation*)navigation;
- (void)_didSameDocumentNavigationForMainFrame:(WebKit::SameDocumentNavigationType)navigationType;

- (BOOL)_isShowingVideoPictureInPicture;
- (BOOL)_mayAutomaticallyShowVideoPictureInPicture;

- (void)_resetCachedScrollViewBackgroundColor;
- (void)_updateScrollViewBackground;
- (void)_updateScrollViewIndicatorStyle;

- (void)_videoControlsManagerDidChange;
- (void)_videosInElementFullscreenChanged;

- (void)_navigationGestureDidBegin;
- (void)_navigationGestureDidEnd;
- (BOOL)_isNavigationSwipeGestureRecognizer:(UIGestureRecognizer *)recognizer;

- (void)_showPasswordViewWithDocumentName:(NSString *)documentName passwordHandler:(void (^)(NSString *))passwordHandler;
- (void)_hidePasswordView;
- (void)_didRequestPasswordForDocument;
- (void)_didStopRequestingPasswordForDocument;

- (void)_addShortcut:(id)sender;
- (void)_define:(id)sender;
- (void)_lookup:(id)sender;
- (void)_share:(id)sender;
- (void)_promptForReplace:(id)sender;
- (void)_transliterateChinese:(id)sender;
- (void)replace:(id)sender;
- (void)_translate:(id)sender;

- (IBAction)alignCenter:(id)sender;
- (IBAction)alignJustified:(id)sender;
- (IBAction)alignLeft:(id)sender;
- (IBAction)alignRight:(id)sender;

#if HAVE(UIFINDINTERACTION)
- (void)find:(id)sender;
- (void)findNext:(id)sender;
- (void)findPrevious:(id)sender;
- (void)findAndReplace:(id)sender;
- (void)useSelectionForFind:(id)sender;
- (void)_findSelected:(id)sender;

- (id<UITextSearching>)_searchableObject;

- (void)_showFindOverlay;
- (void)_hideFindOverlay;
#endif

- (void)_nextAccessoryTab:(id)sender;
- (void)_previousAccessoryTab:(id)sender;

- (void)_incrementFocusPreservationCount;
- (void)_decrementFocusPreservationCount;
- (NSUInteger)_resetFocusPreservationCount;

- (void)_setOpaqueInternal:(BOOL)opaque;
- (NSString *)_contentSizeCategory;
- (void)_dispatchSetDeviceOrientation:(WebCore::IntDegrees)deviceOrientation;
- (WebCore::FloatSize)activeViewLayoutSize:(const CGRect&)bounds;
- (void)_updateScrollViewInsetAdjustmentBehavior;
- (void)_resetScrollViewInsetAdjustmentBehavior;

- (void)_beginAnimatedFullScreenExit;
- (void)_endAnimatedFullScreenExit;

- (BOOL)_effectiveAppearanceIsDark;
- (BOOL)_effectiveUserInterfaceLevelIsElevated;

- (_UIDataOwner)_effectiveDataOwner:(_UIDataOwner)clientSuppliedDataOwner;

#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)
- (void)_beginLiveResize;
- (void)_endLiveResize;
#endif

#if ENABLE(LOCKDOWN_MODE_API)
+ (void)_clearLockdownModeWarningNeeded;
#endif

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
- (void)scrollView:(WKBaseScrollView *)scrollView handleScrollUpdate:(WKBEScrollViewScrollUpdate *)update completion:(void (^)(BOOL handled))completion;
#endif

- (UIColor *)_insertionPointColor;

@property (nonatomic, readonly) WKPasswordView *_passwordView;
@property (nonatomic, readonly) WKWebViewContentProviderRegistry *_contentProviderRegistry;
@property (nonatomic, readonly) WKSelectionGranularity _selectionGranularity WK_API_DEPRECATED("This property is ignored; selection granularity is always `character`.", ios(8.0, 11.0), visionos(1.0, 1.0));

@property (nonatomic, readonly) BOOL _shouldAvoidSecurityHeuristicScoreUpdates;

@property (nonatomic, readonly) BOOL _isBackground;
@property (nonatomic, readonly) BOOL _allowsDoubleTapGestures;
@property (nonatomic, readonly) BOOL _haveSetObscuredInsets;
@property (nonatomic, readonly) UIEdgeInsets _computedObscuredInset;
@property (nonatomic, readonly) UIEdgeInsets _computedUnobscuredSafeAreaInset;
@property (nonatomic, readonly, getter=_isRetainingActiveFocusedState) BOOL _retainingActiveFocusedState;
@property (nonatomic, readonly) WebCore::IntDegrees _deviceOrientationIgnoringOverrides;

- (void)_setObscuredInsetsInternal:(UIEdgeInsets)obscuredInsets;

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
@property (nonatomic, readonly) BOOL _isWindowResizingEnabled;
#endif

@property (nonatomic, readonly) BOOL _isSimulatingCompatibilityPointerTouches;
@property (nonatomic, readonly) WKBaseScrollView *_scrollViewInternal;
@property (nonatomic, readonly) CGRect _contentRectForUserInteraction;

@property (nonatomic, readonly) BOOL _haveSetUnobscuredSafeAreaInsets;
@property (nonatomic, readonly) BOOL _hasOverriddenLayoutParameters;
- (void)_resetContentOffset;
- (void)_resetUnobscuredSafeAreaInsets;
- (void)_resetObscuredInsets;

@property (nonatomic, setter=_setForcesInitialScaleFactor:) BOOL _forcesInitialScaleFactor;

- (void)_setPointerTouchCompatibilitySimulatorEnabled:(BOOL)enabled;

#if ENABLE(PAGE_LOAD_OBSERVER)
- (void)_updatePageLoadObserverState NS_DIRECT;
#endif

#if ENABLE(MODEL_PROCESS)
- (void)_willInvalidateDraggedModelWithContainerView:(UIView *)containerView;
#endif

- (BOOL)_isInStableState:(UIScrollView *)scrollView;

- (UIEdgeInsets)currentlyVisibleContentInsetsWithScale:(CGFloat)scaleFactor obscuredInsets:(UIEdgeInsets)obscuredInsets;

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
@property (nonatomic, readonly) BOOL _shouldHideTopScrollPocket;
#endif

@end

_WKTapHandlingResult wkTapHandlingResult(WebKit::TapHandlingResult);

#endif // PLATFORM(IOS_FAMILY)
