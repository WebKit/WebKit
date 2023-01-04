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

#import "WKWebViewInternal.h"
#import "_WKTapHandlingResult.h"

@class UIScrollEvent;

#if PLATFORM(IOS_FAMILY)

namespace WebKit {
enum class TapHandlingResult : uint8_t;
}

@interface WKWebView (WKViewInternalIOS)

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
- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction;
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

- (void)_navigationGestureDidBegin;
- (void)_navigationGestureDidEnd;
- (BOOL)_isNavigationSwipeGestureRecognizer:(UIGestureRecognizer *)recognizer;

- (void)_showPasswordViewWithDocumentName:(NSString *)documentName passwordHandler:(void (^)(NSString *))passwordHandler;
- (void)_hidePasswordView;

- (void)_addShortcut:(id)sender;
- (void)_define:(id)sender;
- (void)_lookup:(id)sender;
- (void)_share:(id)sender;
- (void)_showTextStyleOptions:(id)sender;
- (void)_promptForReplace:(id)sender;
- (void)_transliterateChinese:(id)sender;
- (void)replace:(id)sender;
- (void)_translate:(id)sender;

#if HAVE(UIFINDINTERACTION)
- (void)find:(id)sender;
- (void)findNext:(id)sender;
- (void)findPrevious:(id)sender;
- (void)findAndReplace:(id)sender;
- (void)useSelectionForFind:(id)sender;
- (void)_findSelected:(id)sender;

- (id<UITextSearching>)_searchableObject;
#endif

- (void)_nextAccessoryTab:(id)sender;
- (void)_previousAccessoryTab:(id)sender;

- (void)_incrementFocusPreservationCount;
- (void)_decrementFocusPreservationCount;
- (void)_resetFocusPreservationCount;

- (void)_setOpaqueInternal:(BOOL)opaque;
- (NSString *)_contentSizeCategory;
- (void)_dispatchSetDeviceOrientation:(int32_t)deviceOrientation;
- (WebCore::FloatSize)activeViewLayoutSize:(const CGRect&)bounds;
- (void)_updateScrollViewInsetAdjustmentBehavior;

- (BOOL)_effectiveAppearanceIsDark;
- (BOOL)_effectiveUserInterfaceLevelIsElevated;

#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)
- (void)_beginLiveResize;
- (void)_endLiveResize;
#endif

#if ENABLE(LOCKDOWN_MODE_API)
+ (void)_clearLockdownModeWarningNeeded;
#endif

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
- (void)_scrollView:(UIScrollView *)scrollView asynchronouslyHandleScrollEvent:(UIScrollEvent *)scrollEvent completion:(void (^)(BOOL handled))completion;
#endif

@property (nonatomic, readonly) WKPasswordView *_passwordView;
@property (nonatomic, readonly) WKWebViewContentProviderRegistry *_contentProviderRegistry;
@property (nonatomic, readonly) WKSelectionGranularity _selectionGranularity;

@property (nonatomic, readonly) BOOL _shouldAvoidSecurityHeuristicScoreUpdates;

@property (nonatomic, readonly) BOOL _isBackground;
@property (nonatomic, readonly) BOOL _allowsDoubleTapGestures;
@property (nonatomic, readonly) BOOL _haveSetObscuredInsets;
@property (nonatomic, readonly) UIEdgeInsets _computedObscuredInset;
@property (nonatomic, readonly) UIEdgeInsets _computedUnobscuredSafeAreaInset;
@property (nonatomic, readonly, getter=_isRetainingActiveFocusedState) BOOL _retainingActiveFocusedState;
@property (nonatomic, readonly) int32_t _deviceOrientation;

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
@property (nonatomic, readonly) BOOL _isWindowResizingEnabled;
#endif

@end

_WKTapHandlingResult wkTapHandlingResult(WebKit::TapHandlingResult);

#endif // PLATFORM(IOS_FAMILY)
