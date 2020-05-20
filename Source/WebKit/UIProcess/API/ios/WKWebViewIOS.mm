/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#import "WKWebViewIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "FrontBoardServicesSPI.h"
#import "NavigationState.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeScrollingPerformanceData.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "VersionChecks.h"
#import "VideoFullscreenManagerProxy.h"
#import "ViewGestureController.h"
#import "WKBackForwardListItemInternal.h"
#import "WKContentView.h"
#import "WKPasswordView.h"
#import "WKSafeBrowsingWarning.h"
#import "WKScrollView.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewContentProvider.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WKWebViewPrivate.h"
#import "WKWebViewPrivateForTestingIOS.h"
#import "WebBackForwardList.h"
#import "WebPageProxy.h"
#import "_WKActivatedElementInfoInternal.h"
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurface.h>
#import <WebCore/LocalCurrentTraitCollection.h>
#import <WebCore/MIMETypeRegistry.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(DATA_DETECTION)
#import "WKDataDetectorTypesInternal.h"
#endif

#define FORWARD_ACTION_TO_WKCONTENTVIEW(_action) \
- (void)_action:(id)sender \
{ \
    if (self.usesStandardContentView) \
        [_contentView _action ## ForWebView:sender]; \
}

#define RELEASE_LOG_IF_ALLOWED(...) RELEASE_LOG_IF(_page && _page->isAlwaysOnLoggingAllowed(), ViewState, __VA_ARGS__)

static const Seconds delayBeforeNoVisibleContentsRectsLogging = 1_s;
static const Seconds delayBeforeNoCommitsLogging = 5_s;

static int32_t deviceOrientationForUIInterfaceOrientation(UIInterfaceOrientation orientation)
{
    switch (orientation) {
    case UIInterfaceOrientationUnknown:
    case UIInterfaceOrientationPortrait:
        return 0;
    case UIInterfaceOrientationPortraitUpsideDown:
        return 180;
    case UIInterfaceOrientationLandscapeLeft:
        return -90;
    case UIInterfaceOrientationLandscapeRight:
        return 90;
    }
}

@interface UIView (UIViewInternal)
- (UIViewController *)_viewControllerForAncestor;
@end

@interface UIWindow (UIWindowInternal)
- (BOOL)_isHostedInAnotherProcess;
@end

@interface UIViewController (UIViewControllerInternal)
- (UIViewController *)_rootAncestorViewController;
- (UIViewController *)_viewControllerForSupportedInterfaceOrientations;
@end

@implementation WKWebView (WKViewInternalIOS)

- (void)setFrame:(CGRect)frame
{
    CGRect oldFrame = self.frame;
    [super setFrame:frame];

    if (!CGSizeEqualToSize(oldFrame.size, frame.size))
        [self _frameOrBoundsChanged];
}

- (void)setBounds:(CGRect)bounds
{
    CGRect oldBounds = self.bounds;
    [super setBounds:bounds];
    [_customContentFixedOverlayView setFrame:self.bounds];

    if (!CGSizeEqualToSize(oldBounds.size, bounds.size))
        [self _frameOrBoundsChanged];
}

- (void)layoutSubviews
{
    [_safeBrowsingWarning setFrame:self.bounds];
    [super layoutSubviews];
    [self _frameOrBoundsChanged];
}

#pragma mark - iOS implementation methods

- (void)_setupScrollAndContentViews
{
    CGRect bounds = self.bounds;
    _scrollView = adoptNS([[WKScrollView alloc] initWithFrame:bounds]);
    [_scrollView setInternalDelegate:self];
    [_scrollView setBouncesZoom:YES];

    if ([_scrollView respondsToSelector:@selector(_setAvoidsJumpOnInterruptedBounce:)]) {
        [_scrollView setTracksImmediatelyWhileDecelerating:NO];
        [_scrollView _setAvoidsJumpOnInterruptedBounce:YES];
    }

    if ([_configuration _editableImagesEnabled])
        [_scrollView panGestureRecognizer].allowedTouchTypes = @[ @(UITouchTypeDirect) ];

    [self _updateScrollViewInsetAdjustmentBehavior];

    [self addSubview:_scrollView.get()];

    [self _dispatchSetDeviceOrientation:[self _deviceOrientation]];

    [_contentView layer].anchorPoint = CGPointZero;
    [_contentView setFrame:bounds];
    [_scrollView addSubview:_contentView.get()];
    [_scrollView addSubview:[_contentView unscaledView]];
}

- (void)_registerForNotifications
{
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(_keyboardWillChangeFrame:) name:UIKeyboardWillChangeFrameNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardDidChangeFrame:) name:UIKeyboardDidChangeFrameNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardWillShow:) name:UIKeyboardWillShowNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardDidShow:) name:UIKeyboardDidShowNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardWillHide:) name:UIKeyboardWillHideNotification object:nil];
    [center addObserver:self selector:@selector(_windowDidRotate:) name:UIWindowDidRotateNotification object:nil];
    [center addObserver:self selector:@selector(_contentSizeCategoryDidChange:) name:UIContentSizeCategoryDidChangeNotification object:nil];

    [center addObserver:self selector:@selector(_accessibilitySettingsDidChange:) name:UIAccessibilityGrayscaleStatusDidChangeNotification object:nil];
    [center addObserver:self selector:@selector(_accessibilitySettingsDidChange:) name:UIAccessibilityInvertColorsStatusDidChangeNotification object:nil];
    [center addObserver:self selector:@selector(_accessibilitySettingsDidChange:) name:UIAccessibilityReduceMotionStatusDidChangeNotification object:nil];
}

- (BOOL)_isShowingVideoPictureInPicture
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (!_page || !_page->videoFullscreenManager())
        return false;

    return _page->videoFullscreenManager()->hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
#else
    return false;
#endif
}

- (BOOL)_mayAutomaticallyShowVideoPictureInPicture
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (!_page || !_page->videoFullscreenManager())
        return false;

    return _page->videoFullscreenManager()->mayAutomaticallyShowVideoPictureInPicture();
#else
    return false;
#endif
}

- (void)_incrementFocusPreservationCount
{
    ++_focusPreservationCount;
}

- (void)_decrementFocusPreservationCount
{
    if (_focusPreservationCount)
        --_focusPreservationCount;
}

- (void)_resetFocusPreservationCount
{
    _focusPreservationCount = 0;
}

- (BOOL)_isRetainingActiveFocusedState
{
    // Focus preservation count fulfills the same role as active focus state count.
    // However, unlike active focus state, it may be reset to 0 without impacting the
    // behavior of -_retainActiveFocusedState, and it's harmless to invoke
    // -_decrementFocusPreservationCount after resetting the count to 0.
    return _focusPreservationCount || _activeFocusedStateRetainCount;
}

- (int32_t)_deviceOrientation
{
    auto orientation = UIInterfaceOrientationUnknown;
    auto application = UIApplication.sharedApplication;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!application._appAdoptsUISceneLifecycle)
        orientation = application.statusBarOrientation;
ALLOW_DEPRECATED_DECLARATIONS_END
    else if (auto windowScene = self.window.windowScene)
        orientation = windowScene.interfaceOrientation;
    return deviceOrientationForUIInterfaceOrientation(orientation);
}

- (BOOL)_effectiveAppearanceIsDark
{
    return self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
}

- (BOOL)_effectiveUserInterfaceLevelIsElevated
{
#if HAVE(OS_DARK_MODE_SUPPORT) && !PLATFORM(WATCHOS)
    return self.traitCollection.userInterfaceLevel == UIUserInterfaceLevelElevated;
#else
    return NO;
#endif
}

- (void)_populateArchivedSubviews:(NSMutableSet *)encodedViews
{
    [super _populateArchivedSubviews:encodedViews];

    if (_scrollView)
        [encodedViews removeObject:_scrollView.get()];
    if (_customContentFixedOverlayView)
        [encodedViews removeObject:_customContentFixedOverlayView.get()];
}

- (BOOL)_isBackground
{
    if (![self usesStandardContentView] && [_customContentView respondsToSelector:@selector(web_isBackground)])
        return [_customContentView web_isBackground];
    return [_contentView isBackground];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (WKBrowsingContextController *)browsingContextController
{
    return [_contentView browsingContextController];
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (BOOL)becomeFirstResponder
{
    UIView *currentContentView = self._currentContentView;
    if (currentContentView == _contentView && [_contentView superview])
        return [_contentView becomeFirstResponderForWebView] || [super becomeFirstResponder];

    return [currentContentView becomeFirstResponder] || [super becomeFirstResponder];
}

- (BOOL)canBecomeFirstResponder
{
    if (self._currentContentView == _contentView)
        return [_contentView canBecomeFirstResponderForWebView];

    return YES;
}

- (BOOL)resignFirstResponder
{
    if ([_contentView isFirstResponder])
        return [_contentView resignFirstResponderForWebView];

    return [super resignFirstResponder];
}

FOR_EACH_WKCONTENTVIEW_ACTION(FORWARD_ACTION_TO_WKCONTENTVIEW)

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender
{
#define FORWARD_CANPERFORMACTION_TO_WKCONTENTVIEW(_action) \
    if (action == @selector(_action:)) \
        return self.usesStandardContentView && [_contentView canPerformActionForWebView:action withSender:sender];

    FOR_EACH_WKCONTENTVIEW_ACTION(FORWARD_CANPERFORMACTION_TO_WKCONTENTVIEW)
    FOR_EACH_PRIVATE_WKCONTENTVIEW_ACTION(FORWARD_CANPERFORMACTION_TO_WKCONTENTVIEW)
    FORWARD_CANPERFORMACTION_TO_WKCONTENTVIEW(_setTextColor:sender)
    FORWARD_CANPERFORMACTION_TO_WKCONTENTVIEW(_setFontSize:sender)
    FORWARD_CANPERFORMACTION_TO_WKCONTENTVIEW(_setFont:sender)

#undef FORWARD_CANPERFORMACTION_TO_WKCONTENTVIEW

    return [super canPerformAction:action withSender:sender];
}

- (id)targetForAction:(SEL)action withSender:(id)sender
{
#define FORWARD_TARGETFORACTION_TO_WKCONTENTVIEW(_action) \
    if (action == @selector(_action:) && self.usesStandardContentView) \
        return [_contentView targetForActionForWebView:action withSender:sender];

    FOR_EACH_WKCONTENTVIEW_ACTION(FORWARD_TARGETFORACTION_TO_WKCONTENTVIEW)
    FOR_EACH_PRIVATE_WKCONTENTVIEW_ACTION(FORWARD_TARGETFORACTION_TO_WKCONTENTVIEW)
    FORWARD_TARGETFORACTION_TO_WKCONTENTVIEW(_setTextColor:sender)
    FORWARD_TARGETFORACTION_TO_WKCONTENTVIEW(_setFontSize:sender)
    FORWARD_TARGETFORACTION_TO_WKCONTENTVIEW(_setFont:sender)

#undef FORWARD_TARGETFORACTION_TO_WKCONTENTVIEW

    return [super targetForAction:action withSender:sender];
}

- (void)willFinishIgnoringCalloutBarFadeAfterPerformingAction
{
    [_contentView willFinishIgnoringCalloutBarFadeAfterPerformingAction];
}

static inline CGFloat floorToDevicePixel(CGFloat input, float deviceScaleFactor)
{
    return CGFloor(input * deviceScaleFactor) / deviceScaleFactor;
}

static inline bool pointsEqualInDevicePixels(CGPoint a, CGPoint b, float deviceScaleFactor)
{
    return fabs(a.x * deviceScaleFactor - b.x * deviceScaleFactor) < std::numeric_limits<float>::epsilon()
        && fabs(a.y * deviceScaleFactor - b.y * deviceScaleFactor) < std::numeric_limits<float>::epsilon();
}

static CGSize roundScrollViewContentSize(const WebKit::WebPageProxy& page, CGSize contentSize)
{
    float deviceScaleFactor = page.deviceScaleFactor();
    return CGSizeMake(floorToDevicePixel(contentSize.width, deviceScaleFactor), floorToDevicePixel(contentSize.height, deviceScaleFactor));
}

- (UIView *)_currentContentView
{
    return _customContentView ? [_customContentView web_contentView] : _contentView.get();
}

- (WKWebViewContentProviderRegistry *)_contentProviderRegistry
{
    return [_configuration _contentProviderRegistry];
}

- (WKSelectionGranularity)_selectionGranularity
{
    return [_configuration selectionGranularity];
}

- (void)_setHasCustomContentView:(BOOL)pageHasCustomContentView loadedMIMEType:(const WTF::String&)mimeType
{
    Class representationClass = nil;
    if (pageHasCustomContentView)
        representationClass = [[_configuration _contentProviderRegistry] providerForMIMEType:mimeType];

    if (pageHasCustomContentView && representationClass) {
        [_customContentView removeFromSuperview];
        [_customContentFixedOverlayView removeFromSuperview];

        _customContentView = adoptNS([[representationClass alloc] web_initWithFrame:self.bounds webView:self mimeType:mimeType]);
        _customContentFixedOverlayView = adoptNS([[UIView alloc] initWithFrame:self.bounds]);
        [_customContentFixedOverlayView layer].name = @"CustomContentFixedOverlay";
        [_customContentFixedOverlayView setUserInteractionEnabled:NO];

        [[_contentView unscaledView] removeFromSuperview];
        [_contentView removeFromSuperview];
        [_scrollView addSubview:_customContentView.get()];
        [self addSubview:_customContentFixedOverlayView.get()];

        [_customContentView web_setMinimumSize:self.bounds.size];
        [_customContentView web_setFixedOverlayView:_customContentFixedOverlayView.get()];

        _scrollViewBackgroundColor = WebCore::Color();
        [_scrollView setContentOffset:[self _initialContentOffsetForScrollView]];
        [_scrollView _setScrollEnabledInternal:YES];

        [self _setAvoidsUnsafeArea:NO];
    } else if (_customContentView) {
        [_customContentView removeFromSuperview];
        _customContentView = nullptr;

        [_customContentFixedOverlayView removeFromSuperview];
        _customContentFixedOverlayView = nullptr;

        [_scrollView addSubview:_contentView.get()];
        [_scrollView addSubview:[_contentView unscaledView]];
        [_scrollView setContentSize:roundScrollViewContentSize(*_page, [_contentView frame].size)];

        [_customContentFixedOverlayView setFrame:self.bounds];
        [self addSubview:_customContentFixedOverlayView.get()];
    }

    if (self.isFirstResponder) {
        UIView *currentContentView = self._currentContentView;
        if (currentContentView == _contentView ? [_contentView canBecomeFirstResponderForWebView] : currentContentView.canBecomeFirstResponder)
            [currentContentView becomeFirstResponder];
    }
}

- (void)_didFinishLoadingDataForCustomContentProviderWithSuggestedFilename:(const String&)suggestedFilename data:(NSData *)data
{
    ASSERT(_customContentView);
    [_customContentView web_setContentProviderData:data suggestedFilename:suggestedFilename];

    // FIXME: It may make more sense for custom content providers to invoke this when they're ready,
    // because there's no guarantee that all custom content providers will lay out synchronously.
    _page->didLayoutForCustomContentProvider();
}

- (void)_handleKeyUIEvent:(::UIEvent *)event
{
    // We only want to handle key events from the hardware keyboard when we are
    // first responder and a custom content view is installed; otherwise,
    // WKContentView will be the first responder and expects to get key events directly.
    if ([self isFirstResponder] && event._hidEvent) {
        if ([_customContentView respondsToSelector:@selector(web_handleKeyEvent:)]) {
            if ([_customContentView web_handleKeyEvent:event])
                return;
        }
    }

    [super _handleKeyUIEvent:event];
}

- (void)_willInvokeUIScrollViewDelegateCallback
{
    _invokingUIScrollViewDelegateCallback = YES;
}

- (void)_didInvokeUIScrollViewDelegateCallback
{
    _invokingUIScrollViewDelegateCallback = NO;
    if (_didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback) {
        _didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback = NO;
        [self _scheduleVisibleContentRectUpdate];
    }
}

static CGFloat contentZoomScale(WKWebView *webView)
{
    CGFloat scale = webView._currentContentView.layer.affineTransform.a;
    ASSERT(scale == [webView->_scrollView zoomScale]);
    return scale;
}

static WebCore::Color baseScrollViewBackgroundColor(WKWebView *webView)
{
    if (webView->_customContentView)
        return [webView->_customContentView backgroundColor].CGColor;

    if (webView->_gestureController) {
        WebCore::Color color = webView->_gestureController->backgroundColorForCurrentSnapshot();
        if (color.isValid())
            return color;
    }

    if (!webView->_page)
        return { };

    return webView->_page->pageExtendedBackgroundColor();
}

static WebCore::Color scrollViewBackgroundColor(WKWebView *webView)
{
    if (!webView.opaque)
        return WebCore::Color::transparent;

#if HAVE(OS_DARK_MODE_SUPPORT)
    WebCore::LocalCurrentTraitCollection localTraitCollection(webView.traitCollection);
#endif

    WebCore::Color color = baseScrollViewBackgroundColor(webView);

    if (!color.isValid() && webView->_contentView)
        color = [webView->_contentView backgroundColor].CGColor;

    if (!color.isValid()) {
#if HAVE(OS_DARK_MODE_SUPPORT)
        color = UIColor.systemBackgroundColor.CGColor;
#else
        color = WebCore::Color::white;
#endif
    }

    CGFloat zoomScale = contentZoomScale(webView);
    CGFloat minimumZoomScale = [webView->_scrollView minimumZoomScale];
    if (zoomScale < minimumZoomScale) {
        CGFloat slope = 12;
        CGFloat opacity = std::max<CGFloat>(1 - slope * (minimumZoomScale - zoomScale), 0);
        color = WebCore::colorWithOverrideAlpha(color.rgb(), opacity);
    }

    return color;
}

- (void)_updateScrollViewBackground
{
    WebCore::Color color = scrollViewBackgroundColor(self);

    if (_scrollViewBackgroundColor == color)
        return;

    _scrollViewBackgroundColor = color;

    auto uiBackgroundColor = adoptNS([[UIColor alloc] initWithCGColor:cachedCGColor(color)]);
    [_scrollView setBackgroundColor:uiBackgroundColor.get()];

    // Update the indicator style based on the lightness/darkness of the background color.
    if (color.lightness() <= .5f && color.isVisible())
        [_scrollView setIndicatorStyle:UIScrollViewIndicatorStyleWhite];
    else
        [_scrollView setIndicatorStyle:UIScrollViewIndicatorStyleBlack];
}

- (void)_videoControlsManagerDidChange
{
#if ENABLE(FULLSCREEN_API)
    if (_fullScreenWindowController)
        [_fullScreenWindowController videoControlsManagerDidChange];
#endif
}

- (CGPoint)_initialContentOffsetForScrollView
{
    auto combinedUnobscuredAndScrollViewInset = [self _computedContentInset];
    return CGPointMake(-combinedUnobscuredAndScrollViewInset.left, -combinedUnobscuredAndScrollViewInset.top);
}

- (CGPoint)_contentOffsetAdjustedForObscuredInset:(CGPoint)point
{
    CGPoint result = point;
    UIEdgeInsets contentInset = [self _computedObscuredInset];

    result.x -= contentInset.left;
    result.y -= contentInset.top;

    return result;
}

- (UIRectEdge)_effectiveObscuredInsetEdgesAffectedBySafeArea
{
    if (![self usesStandardContentView])
        return UIRectEdgeAll;
    return _obscuredInsetEdgesAffectedBySafeArea;
}

- (UIEdgeInsets)_computedObscuredInsetForSafeBrowsingWarning
{
    if (_haveSetObscuredInsets)
        return _obscuredInsets;

#if PLATFORM(IOS)
    return UIEdgeInsetsAdd(UIEdgeInsetsZero, self._scrollViewSystemContentInset, self._effectiveObscuredInsetEdgesAffectedBySafeArea);
#else
    return UIEdgeInsetsZero;
#endif
}

- (UIEdgeInsets)_computedObscuredInset
{
    if (!linkedOnOrAfter(WebKit::SDKVersion::FirstWhereScrollViewContentInsetsAreNotObscuringInsets)) {
        // For binary compability with third party apps, treat scroll view content insets as obscuring insets when the app is compiled
        // against a WebKit version without the fix in r229641.
        return [self _computedContentInset];
    }

    if (_haveSetObscuredInsets)
        return _obscuredInsets;

#if PLATFORM(IOS)
    if (self._safeAreaShouldAffectObscuredInsets)
        return UIEdgeInsetsAdd(UIEdgeInsetsZero, self._scrollViewSystemContentInset, self._effectiveObscuredInsetEdgesAffectedBySafeArea);
#endif

    return UIEdgeInsetsZero;
}

- (UIEdgeInsets)_computedContentInset
{
    if (_haveSetObscuredInsets)
        return _obscuredInsets;

    UIEdgeInsets insets = [_scrollView contentInset];

#if PLATFORM(IOS)
    if (self._safeAreaShouldAffectObscuredInsets)
        insets = UIEdgeInsetsAdd(insets, self._scrollViewSystemContentInset, self._effectiveObscuredInsetEdgesAffectedBySafeArea);
#endif

    return insets;
}

- (UIEdgeInsets)_computedUnobscuredSafeAreaInset
{
    if (_haveSetUnobscuredSafeAreaInsets)
        return _unobscuredSafeAreaInsets;

#if PLATFORM(IOS)
    if (!self._safeAreaShouldAffectObscuredInsets)
        return self.safeAreaInsets;
#endif

    return UIEdgeInsetsZero;
}

- (void)_processWillSwapOrDidExit
{
    // FIXME: Which ones of these need to be done in the process swap case and which ones in the exit case?
    [self _hidePasswordView];
    [self _cancelAnimatedResize];

    if (_gestureController)
        _gestureController->disconnectFromProcess();

    _viewportMetaTagWidth = WebCore::ViewportArguments::ValueAuto;
    _initialScaleFactor = 1;
    _hasCommittedLoadForMainFrame = NO;
    _needsResetViewStateAfterCommitLoadForMainFrame = NO;
    _dynamicViewportUpdateMode = WebKit::DynamicViewportUpdateMode::NotResizing;
    _waitingForEndAnimatedResize = NO;
    _waitingForCommitAfterAnimatedResize = NO;
    _animatedResizeOriginalContentWidth = 0;
    [_contentView setHidden:NO];
    _scrollOffsetToRestore = WTF::nullopt;
    _unobscuredCenterToRestore = WTF::nullopt;
    _scrollViewBackgroundColor = WebCore::Color();
    _invokingUIScrollViewDelegateCallback = NO;
    _didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback = NO;
    _didDeferUpdateVisibleContentRectsForAnyReason = NO;
    _didDeferUpdateVisibleContentRectsForUnstableScrollView = NO;
    _currentlyAdjustingScrollViewInsetsForKeyboard = NO;
    _lastSentViewLayoutSize = WTF::nullopt;
    _lastSentMaximumUnobscuredSize = WTF::nullopt;
    _lastSentDeviceOrientation = WTF::nullopt;

    _frozenVisibleContentRect = WTF::nullopt;
    _frozenUnobscuredContentRect = WTF::nullopt;

    _firstPaintAfterCommitLoadTransactionID = { };
    _firstTransactionIDAfterPageRestore = WTF::nullopt;

    _lastTransactionID = { };

    _hasScheduledVisibleRectUpdate = NO;
    _commitDidRestoreScrollPosition = NO;

    _avoidsUnsafeArea = YES;
}

- (void)_processWillSwap
{
    RELEASE_LOG_IF_ALLOWED("%p -[WKWebView _processWillSwap]", self);
    [self _processWillSwapOrDidExit];
}

- (void)_processDidExit
{
    RELEASE_LOG_IF_ALLOWED("%p -[WKWebView _processDidExit]", self);

    [self _processWillSwapOrDidExit];

    [_contentView setFrame:self.bounds];
    [_scrollView setBackgroundColor:[_contentView backgroundColor]];
    [_scrollView setContentOffset:[self _initialContentOffsetForScrollView]];
    [_scrollView setZoomScale:1];
}

- (void)_didRelaunchProcess
{
    RELEASE_LOG_IF_ALLOWED("%p -[WKWebView _didRelaunchProcess]", self);
    _hasScheduledVisibleRectUpdate = NO;
    _visibleContentRectUpdateScheduledFromScrollViewInStableState = YES;
    if (_gestureController)
        _gestureController->connectToProcess();
}

- (void)_didCommitLoadForMainFrame
{
    _firstPaintAfterCommitLoadTransactionID = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).nextLayerTreeTransactionID();

    _hasCommittedLoadForMainFrame = YES;
    _needsResetViewStateAfterCommitLoadForMainFrame = YES;

    [_scrollView _stopScrollingAndZoomingAnimations];
}

static CGPoint contentOffsetBoundedInValidRange(UIScrollView *scrollView, CGPoint contentOffset)
{
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    UIEdgeInsets contentInsets = scrollView.adjustedContentInset;
#else
    UIEdgeInsets contentInsets = scrollView.contentInset;
#endif

    CGSize contentSize = scrollView.contentSize;
    CGSize scrollViewSize = scrollView.bounds.size;

    CGPoint minimumContentOffset = CGPointMake(-contentInsets.left, -contentInsets.top);
    CGPoint maximumContentOffset = CGPointMake(std::max(minimumContentOffset.x, contentSize.width + contentInsets.right - scrollViewSize.width), std::max(minimumContentOffset.y, contentSize.height + contentInsets.bottom - scrollViewSize.height));

    return CGPointMake(std::max(std::min(contentOffset.x, maximumContentOffset.x), minimumContentOffset.x), std::max(std::min(contentOffset.y, maximumContentOffset.y), minimumContentOffset.y));
}

static void changeContentOffsetBoundedInValidRange(UIScrollView *scrollView, WebCore::FloatPoint contentOffset)
{
    scrollView.contentOffset = contentOffsetBoundedInValidRange(scrollView, contentOffset);
}

- (WebCore::FloatRect)visibleRectInViewCoordinates
{
    WebCore::FloatRect bounds = self.bounds;
    bounds.moveBy([_scrollView contentOffset]);
    WebCore::FloatRect contentViewBounds = [_contentView bounds];
    bounds.intersect(contentViewBounds);
    return bounds;
}

- (void)_didCommitLayerTreeDuringAnimatedResize:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    auto updateID = layerTreeTransaction.dynamicViewportSizeUpdateID();
    if (updateID && *updateID == _currentDynamicViewportSizeUpdateID) {
        double pageScale = layerTreeTransaction.pageScaleFactor();
        WebCore::IntPoint scrollPosition = layerTreeTransaction.scrollPosition();

        CGFloat animatingScaleTarget = [[_resizeAnimationView layer] transform].m11;
        double currentTargetScale = animatingScaleTarget * [[_contentView layer] transform].m11;
        double scale = pageScale / currentTargetScale;
        _resizeAnimationTransformAdjustments = CATransform3DMakeScale(scale, scale, 1);

        CGPoint newContentOffset = [self _contentOffsetAdjustedForObscuredInset:CGPointMake(scrollPosition.x() * pageScale, scrollPosition.y() * pageScale)];
        CGPoint currentContentOffset = [_scrollView contentOffset];

        _resizeAnimationTransformAdjustments.m41 = (currentContentOffset.x - newContentOffset.x) / animatingScaleTarget;
        _resizeAnimationTransformAdjustments.m42 = (currentContentOffset.y - newContentOffset.y) / animatingScaleTarget;

        [_resizeAnimationView layer].sublayerTransform = _resizeAnimationTransformAdjustments;

        // If we've already passed endAnimatedResize, immediately complete
        // the resize when we have an up-to-date layer tree. Otherwise,
        // we will defer completion until endAnimatedResize.
        _waitingForCommitAfterAnimatedResize = NO;
        if (!_waitingForEndAnimatedResize)
            [self _didCompleteAnimatedResize];

        return;
    }

    // If a commit arrives during the live part of a resize but before the
    // layer tree takes the current resize into account, it could change the
    // WKContentView's size. Update the resizeAnimationView's scale to ensure
    // we continue to fill the width of the resize target.

    if (_waitingForEndAnimatedResize)
        return;

    auto newViewLayoutSize = [self activeViewLayoutSize:self.bounds];
    CGFloat resizeAnimationViewScale = _animatedResizeOriginalContentWidth / newViewLayoutSize.width();
    [[_resizeAnimationView layer] setSublayerTransform:CATransform3DMakeScale(resizeAnimationViewScale, resizeAnimationViewScale, 1)];
}

- (void)_trackTransactionCommit:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    if (_didDeferUpdateVisibleContentRectsForUnstableScrollView) {
        RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _didCommitLayerTree:] - received a commit (%llu) while deferring visible content rect updates (_dynamicViewportUpdateMode %d, _needsResetViewStateAfterCommitLoadForMainFrame %d (wants commit %llu), sizeChangedSinceLastVisibleContentRectUpdate %d, [_scrollView isZoomBouncing] %d, _currentlyAdjustingScrollViewInsetsForKeyboard %d)",
        self, _page->identifier().toUInt64(), layerTreeTransaction.transactionID().toUInt64(), _dynamicViewportUpdateMode, _needsResetViewStateAfterCommitLoadForMainFrame, _firstPaintAfterCommitLoadTransactionID.toUInt64(), [_contentView sizeChangedSinceLastVisibleContentRectUpdate], [_scrollView isZoomBouncing], _currentlyAdjustingScrollViewInsetsForKeyboard);
    }

    if (_timeOfFirstVisibleContentRectUpdateWithPendingCommit) {
        auto timeSinceFirstRequestWithPendingCommit = MonotonicTime::now() - *_timeOfFirstVisibleContentRectUpdateWithPendingCommit;
        if (timeSinceFirstRequestWithPendingCommit > delayBeforeNoCommitsLogging)
            RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _didCommitLayerTree:] - finally received commit %.2fs after visible content rect update request; transactionID %llu", self, _page->identifier().toUInt64(), timeSinceFirstRequestWithPendingCommit.value(), layerTreeTransaction.transactionID().toUInt64());
        _timeOfFirstVisibleContentRectUpdateWithPendingCommit = WTF::nullopt;
    }
}

- (void)_updateScrollViewForTransaction:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    CGSize newContentSize = roundScrollViewContentSize(*_page, [_contentView frame].size);
    [_scrollView _setContentSizePreservingContentOffsetDuringRubberband:newContentSize];

    [_scrollView setMinimumZoomScale:layerTreeTransaction.minimumScaleFactor()];
    [_scrollView setMaximumZoomScale:layerTreeTransaction.maximumScaleFactor()];
    [_scrollView _setZoomEnabledInternal:layerTreeTransaction.allowsUserScaling()];

    bool hasDockedInputView = !CGRectIsEmpty(_inputViewBounds);
    bool isZoomed = layerTreeTransaction.pageScaleFactor() > layerTreeTransaction.initialScaleFactor();

    bool scrollingNeededToRevealUI = false;
    if (_maximumUnobscuredSizeOverride) {
        auto unobscuredContentRect = _page->unobscuredContentRect();
        auto maxUnobscuredSize = _page->maximumUnobscuredSize();
        scrollingNeededToRevealUI = maxUnobscuredSize.width() == unobscuredContentRect.width() && maxUnobscuredSize.height() == unobscuredContentRect.height();
    }

    bool scrollingEnabled = _page->scrollingCoordinatorProxy()->hasScrollableMainFrame() || hasDockedInputView || isZoomed || scrollingNeededToRevealUI;
    [_scrollView _setScrollEnabledInternal:scrollingEnabled];

    if (!layerTreeTransaction.scaleWasSetByUIProcess() && ![_scrollView isZooming] && ![_scrollView isZoomBouncing] && ![_scrollView _isAnimatingZoom] && [_scrollView zoomScale] != layerTreeTransaction.pageScaleFactor()) {
        LOG_WITH_STREAM(VisibleRects, stream << " updating scroll view with pageScaleFactor " << layerTreeTransaction.pageScaleFactor());
        [_scrollView setZoomScale:layerTreeTransaction.pageScaleFactor()];
    }
}

- (BOOL)_restoreScrollAndZoomStateForTransaction:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    if (!_firstTransactionIDAfterPageRestore || layerTreeTransaction.transactionID() < _firstTransactionIDAfterPageRestore.value())
        return NO;

    _firstTransactionIDAfterPageRestore = WTF::nullopt;

    BOOL needUpdateVisibleContentRects = NO;

    if (_scrollOffsetToRestore) {
        WebCore::FloatPoint scaledScrollOffset = _scrollOffsetToRestore.value();
        _scrollOffsetToRestore = WTF::nullopt;

        if (WTF::areEssentiallyEqual<float>(contentZoomScale(self), _scaleToRestore)) {
            scaledScrollOffset.scale(_scaleToRestore);
            WebCore::FloatPoint contentOffsetInScrollViewCoordinates = scaledScrollOffset - WebCore::FloatSize(_obscuredInsetsWhenSaved.left(), _obscuredInsetsWhenSaved.top());

            changeContentOffsetBoundedInValidRange(_scrollView.get(), contentOffsetInScrollViewCoordinates);
            _commitDidRestoreScrollPosition = YES;
        }

        needUpdateVisibleContentRects = YES;
    }

    if (_unobscuredCenterToRestore) {
        WebCore::FloatPoint unobscuredCenterToRestore = _unobscuredCenterToRestore.value();
        _unobscuredCenterToRestore = WTF::nullopt;

        if (WTF::areEssentiallyEqual<float>(contentZoomScale(self), _scaleToRestore)) {
            CGRect unobscuredRect = UIEdgeInsetsInsetRect(self.bounds, _obscuredInsets);
            WebCore::FloatSize unobscuredContentSizeAtNewScale = WebCore::FloatSize(unobscuredRect.size) / _scaleToRestore;
            WebCore::FloatPoint topLeftInDocumentCoordinates = unobscuredCenterToRestore - unobscuredContentSizeAtNewScale / 2;

            topLeftInDocumentCoordinates.scale(_scaleToRestore);
            topLeftInDocumentCoordinates.moveBy(WebCore::FloatPoint(-_obscuredInsets.left, -_obscuredInsets.top));

            changeContentOffsetBoundedInValidRange(_scrollView.get(), topLeftInDocumentCoordinates);
        }

        needUpdateVisibleContentRects = YES;
    }

    if (_gestureController)
        _gestureController->didRestoreScrollPosition();
    
    return needUpdateVisibleContentRects;
}

- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    [self _trackTransactionCommit:layerTreeTransaction];

    _lastTransactionID = layerTreeTransaction.transactionID();

    if (![self usesStandardContentView])
        return;

    LOG_WITH_STREAM(VisibleRects, stream << "-[WKWebView " << _page->identifier() << " _didCommitLayerTree:] transactionID " << layerTreeTransaction.transactionID() << " _dynamicViewportUpdateMode " << (int)_dynamicViewportUpdateMode);

    bool needUpdateVisibleContentRects = _page->updateLayoutViewportParameters(layerTreeTransaction);

    if (_dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing) {
        [self _didCommitLayerTreeDuringAnimatedResize:layerTreeTransaction];
        return;
    }

    if (_resizeAnimationView)
        RELEASE_LOG_IF_ALLOWED("%p -[WKWebView _didCommitLayerTree:] - dynamicViewportUpdateMode is NotResizing, but still have a live resizeAnimationView (unpaired begin/endAnimatedResize?)", self);

    [self _updateScrollViewForTransaction:layerTreeTransaction];

    _viewportMetaTagWidth = layerTreeTransaction.viewportMetaTagWidth();
    _viewportMetaTagWidthWasExplicit = layerTreeTransaction.viewportMetaTagWidthWasExplicit();
    _viewportMetaTagCameFromImageDocument = layerTreeTransaction.viewportMetaTagCameFromImageDocument();
    _initialScaleFactor = layerTreeTransaction.initialScaleFactor();

    if (_page->inStableState() && layerTreeTransaction.isInStableState() && [_stableStatePresentationUpdateCallbacks count]) {
        for (dispatch_block_t action in _stableStatePresentationUpdateCallbacks.get())
            action();

        [_stableStatePresentationUpdateCallbacks removeAllObjects];
        _stableStatePresentationUpdateCallbacks = nil;
    }

    if (![_contentView _mayDisableDoubleTapGesturesDuringSingleTap])
        [_contentView _setDoubleTapGesturesEnabled:self._allowsDoubleTapGestures];

    [self _updateScrollViewBackground];
    [self _setAvoidsUnsafeArea:layerTreeTransaction.avoidsUnsafeArea()];

    if (_gestureController)
        _gestureController->setRenderTreeSize(layerTreeTransaction.renderTreeSize());

    if (_needsResetViewStateAfterCommitLoadForMainFrame && layerTreeTransaction.transactionID() >= _firstPaintAfterCommitLoadTransactionID) {
        _needsResetViewStateAfterCommitLoadForMainFrame = NO;
        [_scrollView setContentOffset:[self _initialContentOffsetForScrollView]];
        if (_observedRenderingProgressEvents & _WKRenderingProgressEventFirstPaint)
            _navigationState->didFirstPaint();

        needUpdateVisibleContentRects = true;
    }

    if ([self _restoreScrollAndZoomStateForTransaction:layerTreeTransaction])
        needUpdateVisibleContentRects = true;

    if (needUpdateVisibleContentRects)
        [self _scheduleVisibleContentRectUpdate];

    if (WebKit::RemoteLayerTreeScrollingPerformanceData* scrollPerfData = _page->scrollingPerformanceData())
        scrollPerfData->didCommitLayerTree([self visibleRectInViewCoordinates]);
}

- (void)_layerTreeCommitComplete
{
    _commitDidRestoreScrollPosition = NO;
}

- (void)_couldNotRestorePageState
{
    // The gestureController may be waiting for the scroll position to be restored
    // in order to remove the swipe snapshot. Since the scroll position could not be
    // restored, tell the gestureController it was restored so that it no longer waits
    // for it.
    if (_gestureController)
        _gestureController->didRestoreScrollPosition();
}

- (void)_restorePageScrollPosition:(Optional<WebCore::FloatPoint>)scrollPosition scrollOrigin:(WebCore::FloatPoint)scrollOrigin previousObscuredInset:(WebCore::FloatBoxExtent)obscuredInsets scale:(double)scale
{
    if (_dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing) {
        // Defer scroll position restoration until after the current resize completes.
        RetainPtr<WKWebView> retainedSelf = self;
        _callbacksDeferredDuringResize.append([retainedSelf, scrollPosition, scrollOrigin, obscuredInsets, scale] {
            [retainedSelf _restorePageScrollPosition:scrollPosition scrollOrigin:scrollOrigin previousObscuredInset:obscuredInsets scale:scale];
        });
        return;
    }

    if (![self usesStandardContentView])
        return;

    _firstTransactionIDAfterPageRestore = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).nextLayerTreeTransactionID();
    if (scrollPosition)
        _scrollOffsetToRestore = WebCore::ScrollableArea::scrollOffsetFromPosition(WebCore::FloatPoint(scrollPosition.value()), WebCore::toFloatSize(scrollOrigin));
    else
        _scrollOffsetToRestore = WTF::nullopt;

    _obscuredInsetsWhenSaved = obscuredInsets;
    _scaleToRestore = scale;
}

- (void)_restorePageStateToUnobscuredCenter:(Optional<WebCore::FloatPoint>)center scale:(double)scale
{
    if (_dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing) {
        // Defer scroll position restoration until after the current resize completes.
        RetainPtr<WKWebView> retainedSelf = self;
        _callbacksDeferredDuringResize.append([retainedSelf, center, scale] {
            [retainedSelf _restorePageStateToUnobscuredCenter:center scale:scale];
        });
        return;
    }

    if (![self usesStandardContentView])
        return;

    _firstTransactionIDAfterPageRestore = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).nextLayerTreeTransactionID();
    _unobscuredCenterToRestore = center;

    _scaleToRestore = scale;
}

- (RefPtr<WebKit::ViewSnapshot>)_takeViewSnapshot
{
#if HAVE(CORE_ANIMATION_RENDER_SERVER)
    float deviceScale = WebCore::screenScaleFactor();
    WebCore::FloatSize snapshotSize(self.bounds.size);
    snapshotSize.scale(deviceScale);

    CATransform3D transform = CATransform3DMakeScale(deviceScale, deviceScale, 1);

#if HAVE(IOSURFACE_RGB10)
    WebCore::IOSurface::Format snapshotFormat = WebCore::screenSupportsExtendedColor() ? WebCore::IOSurface::Format::RGB10 : WebCore::IOSurface::Format::RGBA;
#else
    WebCore::IOSurface::Format snapshotFormat = WebCore::IOSurface::Format::RGBA;
#endif
    auto surface = WebCore::IOSurface::create(WebCore::expandedIntSize(snapshotSize), WebCore::sRGBColorSpaceRef(), snapshotFormat);
    if (!surface)
        return nullptr;
    CARenderServerRenderLayerWithTransform(MACH_PORT_NULL, self.layer.context.contextId, reinterpret_cast<uint64_t>(self.layer), surface->surface(), 0, 0, &transform);

#if HAVE(IOSURFACE_ACCELERATOR)
    WebCore::IOSurface::Format compressedFormat = WebCore::IOSurface::Format::YUV422;
    if (WebCore::IOSurface::allowConversionFromFormatToFormat(snapshotFormat, compressedFormat)) {
        auto viewSnapshot = WebKit::ViewSnapshot::create(nullptr);
        WebCore::IOSurface::convertToFormat(WTFMove(surface), WebCore::IOSurface::Format::YUV422, [viewSnapshot = viewSnapshot.copyRef()](std::unique_ptr<WebCore::IOSurface> convertedSurface) {
            if (convertedSurface)
                viewSnapshot->setSurface(WTFMove(convertedSurface));
        });

        return viewSnapshot;
    }
#endif // HAVE(IOSURFACE_ACCELERATOR)

    return WebKit::ViewSnapshot::create(WTFMove(surface));
#else // HAVE(CORE_ANIMATION_RENDER_SERVER)
    return nullptr;
#endif
}

- (void)_zoomToPoint:(WebCore::FloatPoint)point atScale:(double)scale animated:(BOOL)animated
{
    CFTimeInterval duration = 0;
    CGFloat zoomScale = contentZoomScale(self);

    if (animated) {
        const double maximumZoomDuration = 0.4;
        const double minimumZoomDuration = 0.1;
        const double zoomDurationFactor = 0.3;

        duration = std::min(fabs(log(zoomScale) - log(scale)) * zoomDurationFactor + minimumZoomDuration, maximumZoomDuration);
    }

    if (scale != zoomScale)
        _page->willStartUserTriggeredZooming();

    LOG_WITH_STREAM(VisibleRects, stream << "_zoomToPoint:" << point << " scale: " << scale << " duration:" << duration);

    [_scrollView _zoomToCenter:point scale:scale duration:duration];
}

- (void)_zoomToRect:(WebCore::FloatRect)targetRect atScale:(double)scale origin:(WebCore::FloatPoint)origin animated:(BOOL)animated
{
    // FIXME: Some of this could be shared with _scrollToRect.
    const double visibleRectScaleChange = contentZoomScale(self) / scale;
    const WebCore::FloatRect visibleRect([self convertRect:self.bounds toView:self._currentContentView]);
    const WebCore::FloatRect unobscuredRect([self _contentRectForUserInteraction]);

    const WebCore::FloatSize topLeftObscuredInsetAfterZoom((unobscuredRect.minXMinYCorner() - visibleRect.minXMinYCorner()) * visibleRectScaleChange);
    const WebCore::FloatSize bottomRightObscuredInsetAfterZoom((visibleRect.maxXMaxYCorner() - unobscuredRect.maxXMaxYCorner()) * visibleRectScaleChange);

    const WebCore::FloatSize unobscuredRectSizeAfterZoom(unobscuredRect.size() * visibleRectScaleChange);

    // Center to the target rect.
    WebCore::FloatPoint unobscuredRectLocationAfterZoom = targetRect.location() - (unobscuredRectSizeAfterZoom - targetRect.size()) * 0.5;

    // Center to the tap point instead in case the target rect won't fit in a direction.
    if (targetRect.width() > unobscuredRectSizeAfterZoom.width())
        unobscuredRectLocationAfterZoom.setX(origin.x() - unobscuredRectSizeAfterZoom.width() / 2);
    if (targetRect.height() > unobscuredRectSizeAfterZoom.height())
        unobscuredRectLocationAfterZoom.setY(origin.y() - unobscuredRectSizeAfterZoom.height() / 2);

    // We have computed where we want the unobscured rect to be. Now adjust for the obscuring insets.
    WebCore::FloatRect visibleRectAfterZoom(unobscuredRectLocationAfterZoom, unobscuredRectSizeAfterZoom);
    visibleRectAfterZoom.move(-topLeftObscuredInsetAfterZoom);
    visibleRectAfterZoom.expand(topLeftObscuredInsetAfterZoom + bottomRightObscuredInsetAfterZoom);

    [self _zoomToPoint:visibleRectAfterZoom.center() atScale:scale animated:animated];
}

static WebCore::FloatPoint constrainContentOffset(WebCore::FloatPoint contentOffset, WebCore::FloatSize contentSize, WebCore::FloatSize unobscuredContentSize)
{
    WebCore::FloatSize maximumContentOffset = contentSize - unobscuredContentSize;
    return contentOffset.constrainedBetween(WebCore::FloatPoint(), WebCore::FloatPoint(maximumContentOffset));
}

- (void)_scrollToContentScrollPosition:(WebCore::FloatPoint)scrollPosition scrollOrigin:(WebCore::IntPoint)scrollOrigin
{
    if (_commitDidRestoreScrollPosition || _dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing)
        return;

    WebCore::FloatPoint contentOffset = WebCore::ScrollableArea::scrollOffsetFromPosition(scrollPosition, toFloatSize(scrollOrigin));

    WebCore::FloatPoint scaledOffset = contentOffset;
    CGFloat zoomScale = contentZoomScale(self);
    scaledOffset.scale(zoomScale);

    CGPoint contentOffsetInScrollViewCoordinates = [self _contentOffsetAdjustedForObscuredInset:scaledOffset];
    contentOffsetInScrollViewCoordinates = contentOffsetBoundedInValidRange(_scrollView.get(), contentOffsetInScrollViewCoordinates);

    [_scrollView _stopScrollingAndZoomingAnimations];

    if (!CGPointEqualToPoint(contentOffsetInScrollViewCoordinates, [_scrollView contentOffset]))
        [_scrollView setContentOffset:contentOffsetInScrollViewCoordinates];
    else {
        // If we haven't changed anything, there would not be any VisibleContentRect update sent to the content.
        // The WebProcess would keep the invalid contentOffset as its scroll position.
        // To synchronize the WebProcess with what is on screen, we send the VisibleContentRect again.
        _page->resendLastVisibleContentRects();
    }
}

- (BOOL)_scrollToRect:(WebCore::FloatRect)targetRect origin:(WebCore::FloatPoint)origin minimumScrollDistance:(float)minimumScrollDistance
{
    if (![_scrollView isScrollEnabled])
        return NO;

    WebCore::FloatRect unobscuredContentRect([self _contentRectForUserInteraction]);
    WebCore::FloatPoint unobscuredContentOffset = unobscuredContentRect.location();
    WebCore::FloatSize contentSize([self._currentContentView bounds].size);

    // Center the target rect in the scroll view.
    // If the target doesn't fit in the scroll view, center on the gesture location instead.
    WebCore::FloatPoint newUnobscuredContentOffset;
    if (targetRect.width() <= unobscuredContentRect.width())
        newUnobscuredContentOffset.setX(targetRect.x() - (unobscuredContentRect.width() - targetRect.width()) / 2);
    else
        newUnobscuredContentOffset.setX(origin.x() - unobscuredContentRect.width() / 2);
    if (targetRect.height() <= unobscuredContentRect.height())
        newUnobscuredContentOffset.setY(targetRect.y() - (unobscuredContentRect.height() - targetRect.height()) / 2);
    else
        newUnobscuredContentOffset.setY(origin.y() - unobscuredContentRect.height() / 2);
    newUnobscuredContentOffset = constrainContentOffset(newUnobscuredContentOffset, contentSize, unobscuredContentRect.size());

    if (unobscuredContentOffset == newUnobscuredContentOffset) {
        if (targetRect.width() > unobscuredContentRect.width())
            newUnobscuredContentOffset.setX(origin.x() - unobscuredContentRect.width() / 2);
        if (targetRect.height() > unobscuredContentRect.height())
            newUnobscuredContentOffset.setY(origin.y() - unobscuredContentRect.height() / 2);
        newUnobscuredContentOffset = constrainContentOffset(newUnobscuredContentOffset, contentSize, unobscuredContentRect.size());
    }

    WebCore::FloatSize scrollViewOffsetDelta = newUnobscuredContentOffset - unobscuredContentOffset;
    scrollViewOffsetDelta.scale(contentZoomScale(self));

    float scrollDistance = scrollViewOffsetDelta.diagonalLength();
    if (scrollDistance < minimumScrollDistance)
        return NO;

    [_contentView willStartZoomOrScroll];

    LOG_WITH_STREAM(VisibleRects, stream << "_scrollToRect: scrolling to " << [_scrollView contentOffset] + scrollViewOffsetDelta);

    [_scrollView setContentOffset:([_scrollView contentOffset] + scrollViewOffsetDelta) animated:YES];
    return YES;
}

- (void)_zoomOutWithOrigin:(WebCore::FloatPoint)origin animated:(BOOL)animated
{
    [self _zoomToPoint:origin atScale:[_scrollView minimumZoomScale] animated:animated];
}

- (void)_zoomToInitialScaleWithOrigin:(WebCore::FloatPoint)origin animated:(BOOL)animated
{
    ASSERT(_initialScaleFactor > 0);
    [self _zoomToPoint:origin atScale:_initialScaleFactor animated:animated];
}

// focusedElementRect and selectionRect are both in document coordinates.
- (void)_zoomToFocusRect:(const WebCore::FloatRect&)focusedElementRectInDocumentCoordinates selectionRect:(const WebCore::FloatRect&)selectionRectInDocumentCoordinates insideFixed:(BOOL)insideFixed
    fontSize:(float)fontSize minimumScale:(double)minimumScale maximumScale:(double)maximumScale allowScaling:(BOOL)allowScaling forceScroll:(BOOL)forceScroll
{
    LOG_WITH_STREAM(VisibleRects, stream << "_zoomToFocusRect:" << focusedElementRectInDocumentCoordinates << " selectionRect:" << selectionRectInDocumentCoordinates);
    UNUSED_PARAM(insideFixed);

    const double minimumHeightToShowContentAboveKeyboard = 106;
    const CFTimeInterval formControlZoomAnimationDuration = 0.25;
    const double caretOffsetFromWindowEdge = 8;

    UIWindow *window = [_scrollView window];

    // Find the portion of the view that is visible on the screen.
    UIViewController *topViewController = [[[_scrollView _viewControllerForAncestor] _rootAncestorViewController] _viewControllerForSupportedInterfaceOrientations];
    UIView *fullScreenView = topViewController.view;
    if (!fullScreenView)
        fullScreenView = window;

    CGRect unobscuredScrollViewRectInWebViewCoordinates = UIEdgeInsetsInsetRect([self bounds], _obscuredInsets);
    CGRect visibleScrollViewBoundsInWebViewCoordinates = CGRectIntersection(unobscuredScrollViewRectInWebViewCoordinates, [fullScreenView convertRect:[fullScreenView bounds] toView:self]);
    CGRect formAssistantFrameInWebViewCoordinates = [window convertRect:_inputViewBounds toView:self];
    CGRect intersectionBetweenScrollViewAndFormAssistant = CGRectIntersection(visibleScrollViewBoundsInWebViewCoordinates, formAssistantFrameInWebViewCoordinates);
    CGSize visibleSize = visibleScrollViewBoundsInWebViewCoordinates.size;

    CGFloat visibleOffsetFromTop = 0;
    CGFloat minimumDistanceFromKeyboardToTriggerScroll = 0;
    if (!CGRectIsEmpty(intersectionBetweenScrollViewAndFormAssistant)) {
        CGFloat heightVisibleAboveFormAssistant = CGRectGetMinY(intersectionBetweenScrollViewAndFormAssistant) - CGRectGetMinY(visibleScrollViewBoundsInWebViewCoordinates);
        CGFloat heightVisibleBelowFormAssistant = CGRectGetMaxY(visibleScrollViewBoundsInWebViewCoordinates) - CGRectGetMaxY(intersectionBetweenScrollViewAndFormAssistant);

        if (heightVisibleAboveFormAssistant >= minimumHeightToShowContentAboveKeyboard || heightVisibleBelowFormAssistant < heightVisibleAboveFormAssistant) {
            visibleSize.height = heightVisibleAboveFormAssistant;
            minimumDistanceFromKeyboardToTriggerScroll = 50;
        } else {
            visibleSize.height = heightVisibleBelowFormAssistant;
            visibleOffsetFromTop = CGRectGetMaxY(intersectionBetweenScrollViewAndFormAssistant) - CGRectGetMinY(visibleScrollViewBoundsInWebViewCoordinates);
        }
    }

    // Zoom around the element's bounding frame. We use a "standard" size to determine the proper frame.
    double currentScale = contentZoomScale(self);
    double scale = currentScale;
    if (allowScaling) {
#if PLATFORM(WATCHOS)
        const CGFloat minimumMarginForZoomingToEntireFocusRectInWebViewCoordinates = 10;
        const CGFloat maximumMarginForZoomingToEntireFocusRectInWebViewCoordinates = 35;

        CGRect minimumTargetRectInDocumentCoordinates = UIRectInsetEdges(focusedElementRectInDocumentCoordinates, UIRectEdgeAll, -minimumMarginForZoomingToEntireFocusRectInWebViewCoordinates / currentScale);
        CGRect maximumTargetRectInDocumentCoordinates = UIRectInsetEdges(focusedElementRectInDocumentCoordinates, UIRectEdgeAll, -maximumMarginForZoomingToEntireFocusRectInWebViewCoordinates / currentScale);

        double clampedMaximumTargetScale = clampTo<double>(std::min(visibleSize.width / CGRectGetWidth(minimumTargetRectInDocumentCoordinates), visibleSize.height / CGRectGetHeight(minimumTargetRectInDocumentCoordinates)), minimumScale, maximumScale);
        double clampedMinimumTargetScale = clampTo<double>(std::min(visibleSize.width / CGRectGetWidth(maximumTargetRectInDocumentCoordinates), visibleSize.height / CGRectGetHeight(maximumTargetRectInDocumentCoordinates)), minimumScale, maximumScale);
        scale = clampTo<double>(currentScale, clampedMinimumTargetScale, clampedMaximumTargetScale);
#else
        const double webViewStandardFontSize = 16;
        scale = clampTo<double>(webViewStandardFontSize / fontSize, minimumScale, maximumScale);
#endif
    }

    CGFloat documentWidth = [_contentView bounds].size.width;
    scale = CGRound(documentWidth * scale) / documentWidth;

    WebCore::FloatRect focusedElementRectInNewScale = focusedElementRectInDocumentCoordinates;
    focusedElementRectInNewScale.scale(scale);
    focusedElementRectInNewScale.moveBy([_contentView frame].origin);

    BOOL selectionRectIsNotNull = !selectionRectInDocumentCoordinates.isZero();
    BOOL doNotScrollWhenContentIsAlreadyVisible = !forceScroll || [_contentView _shouldAvoidScrollingWhenFocusedContentIsVisible];
    if (doNotScrollWhenContentIsAlreadyVisible) {
        CGRect currentlyVisibleRegionInWebViewCoordinates;
        currentlyVisibleRegionInWebViewCoordinates.origin = unobscuredScrollViewRectInWebViewCoordinates.origin;
        currentlyVisibleRegionInWebViewCoordinates.origin.y += visibleOffsetFromTop;
        currentlyVisibleRegionInWebViewCoordinates.size = visibleSize;
        currentlyVisibleRegionInWebViewCoordinates.size.height -= minimumDistanceFromKeyboardToTriggerScroll;

        // Don't bother scrolling if the entire node is already visible, whether or not we got a selectionRect.
        if (CGRectContainsRect(currentlyVisibleRegionInWebViewCoordinates, [self convertRect:focusedElementRectInDocumentCoordinates fromView:_contentView.get()]))
            return;

        // Don't bother scrolling if we have a valid selectionRect and it is already visible.
        if (selectionRectIsNotNull && CGRectContainsRect(currentlyVisibleRegionInWebViewCoordinates, [self convertRect:selectionRectInDocumentCoordinates fromView:_contentView.get()]))
            return;
    }

    // We want to center the focused element within the viewport, with as much spacing on all sides as
    // we can get based on the visible area after zooming. The spacing in either dimension is half the
    // difference between the size of the DOM node and the size of the visible frame.
    // If the element is too wide to be horizontally centered or too tall to be vertically centered, we
    // instead scroll such that the left edge or top edge of the element is within the left half or top
    // half of the viewport, respectively.
    CGFloat horizontalSpaceInWebViewCoordinates = (visibleSize.width - focusedElementRectInNewScale.width()) / 2.0;
    CGFloat verticalSpaceInWebViewCoordinates = (visibleSize.height - focusedElementRectInNewScale.height()) / 2.0;

    auto topLeft = CGPointZero;
    auto scrollViewInsets = [_scrollView _effectiveContentInset];
    auto currentTopLeft = [_scrollView contentOffset];

    if (_haveSetObscuredInsets) {
        currentTopLeft.x += _obscuredInsets.left;
        currentTopLeft.y += _obscuredInsets.top;
    }

    if (horizontalSpaceInWebViewCoordinates > 0)
        topLeft.x = focusedElementRectInNewScale.x() - horizontalSpaceInWebViewCoordinates;
    else {
        auto minimumOffsetToRevealLeftEdge = std::max(-scrollViewInsets.left, focusedElementRectInNewScale.x() - visibleSize.width / 2);
        auto maximumOffsetToRevealLeftEdge = focusedElementRectInNewScale.x();
        topLeft.x = clampTo<double>(currentTopLeft.x, minimumOffsetToRevealLeftEdge, maximumOffsetToRevealLeftEdge);
    }

    if (verticalSpaceInWebViewCoordinates > 0)
        topLeft.y = focusedElementRectInNewScale.y() - verticalSpaceInWebViewCoordinates;
    else {
        auto minimumOffsetToRevealTopEdge = std::max(-scrollViewInsets.top, focusedElementRectInNewScale.y() - visibleSize.height / 2);
        auto maximumOffsetToRevealTopEdge = focusedElementRectInNewScale.y();
        topLeft.y = clampTo<double>(currentTopLeft.y, minimumOffsetToRevealTopEdge, maximumOffsetToRevealTopEdge);
    }

    topLeft.y -= visibleOffsetFromTop;

    WebCore::FloatRect documentBoundsInNewScale = [_contentView bounds];
    documentBoundsInNewScale.scale(scale);
    documentBoundsInNewScale.moveBy([_contentView frame].origin);

    CGFloat minimumAllowableHorizontalOffsetInWebViewCoordinates = -INFINITY;
    CGFloat minimumAllowableVerticalOffsetInWebViewCoordinates = -INFINITY;
    CGFloat maximumAllowableHorizontalOffsetInWebViewCoordinates = CGRectGetMaxX(documentBoundsInNewScale) - visibleSize.width;
    CGFloat maximumAllowableVerticalOffsetInWebViewCoordinates = CGRectGetMaxY(documentBoundsInNewScale) - visibleSize.height;

    if (selectionRectIsNotNull) {
        WebCore::FloatRect selectionRectInNewScale = selectionRectInDocumentCoordinates;
        selectionRectInNewScale.scale(scale);
        selectionRectInNewScale.moveBy([_contentView frame].origin);
        // Adjust the min and max allowable scroll offsets, such that the selection rect remains visible.
        minimumAllowableHorizontalOffsetInWebViewCoordinates = CGRectGetMaxX(selectionRectInNewScale) + caretOffsetFromWindowEdge - visibleSize.width;
        minimumAllowableVerticalOffsetInWebViewCoordinates = CGRectGetMaxY(selectionRectInNewScale) + caretOffsetFromWindowEdge - visibleSize.height - visibleOffsetFromTop;
        maximumAllowableHorizontalOffsetInWebViewCoordinates = std::min<CGFloat>(maximumAllowableHorizontalOffsetInWebViewCoordinates, CGRectGetMinX(selectionRectInNewScale) - caretOffsetFromWindowEdge);
        maximumAllowableVerticalOffsetInWebViewCoordinates = std::min<CGFloat>(maximumAllowableVerticalOffsetInWebViewCoordinates, CGRectGetMinY(selectionRectInNewScale) - caretOffsetFromWindowEdge - visibleOffsetFromTop);
    }

    // Constrain the left edge in document coordinates so that:
    //  - it isn't so small that the scrollVisibleRect isn't visible on the screen
    //  - it isn't so great that the document's right edge is less than the right edge of the screen
    topLeft.x = clampTo<CGFloat>(topLeft.x, minimumAllowableHorizontalOffsetInWebViewCoordinates, maximumAllowableHorizontalOffsetInWebViewCoordinates);

    // Constrain the top edge in document coordinates so that:
    //  - it isn't so small that the scrollVisibleRect isn't visible on the screen
    //  - it isn't so great that the document's bottom edge is higher than the top of the form assistant
    topLeft.y = clampTo<CGFloat>(topLeft.y, minimumAllowableVerticalOffsetInWebViewCoordinates, maximumAllowableVerticalOffsetInWebViewCoordinates);

    if (_haveSetObscuredInsets) {
        // This looks unintuitive, but is necessary in order to precisely center the focused element in the visible area.
        // The top left position already accounts for top and left obscured insets - i.e., a topLeft of (0, 0) corresponds
        // to the top- and left-most point below (and to the right of) the top inset area and left inset areas, respectively.
        // However, when telling WKScrollView to scroll to a given center position, this center position is computed relative
        // to the coordinate space of the scroll view. Thus, to compute our center position from the top left position, we
        // need to first move the top left position up and to the left, and then add half the width and height of the content
        // area (including obscured insets).
        topLeft.x -= _obscuredInsets.left;
        topLeft.y -= _obscuredInsets.top;
    }

    WebCore::FloatPoint newCenter = CGPointMake(topLeft.x + CGRectGetWidth(self.bounds) / 2, topLeft.y + CGRectGetHeight(self.bounds) / 2);

    if (scale != currentScale)
        _page->willStartUserTriggeredZooming();

    LOG_WITH_STREAM(VisibleRects, stream << "_zoomToFocusRect: zooming to " << newCenter << " scale:" << scale);

    // The newCenter has been computed in the new scale, but _zoomToCenter expected the center to be in the original scale.
    newCenter.scale(1 / scale);
    [_scrollView _zoomToCenter:newCenter scale:scale duration:formControlZoomAnimationDuration force:YES];
}

- (double)_initialScaleFactor
{
    return _initialScaleFactor;
}

- (double)_contentZoomScale
{
    return contentZoomScale(self);
}

- (double)_targetContentZoomScaleForRect:(const WebCore::FloatRect&)targetRect currentScale:(double)currentScale fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale
{
    WebCore::FloatSize unobscuredContentSize([self _contentRectForUserInteraction].size);
    double horizontalScale = unobscuredContentSize.width() * currentScale / targetRect.width();
    double verticalScale = unobscuredContentSize.height() * currentScale / targetRect.height();

    horizontalScale = std::min(std::max(horizontalScale, minimumScale), maximumScale);
    verticalScale = std::min(std::max(verticalScale, minimumScale), maximumScale);

    return fitEntireRect ? std::min(horizontalScale, verticalScale) : horizontalScale;
}

- (BOOL)_zoomToRect:(WebCore::FloatRect)targetRect withOrigin:(WebCore::FloatPoint)origin fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale minimumScrollDistance:(float)minimumScrollDistance
{
    const float maximumScaleFactorDeltaForPanScroll = 0.02;

    double currentScale = contentZoomScale(self);
    double targetScale = [self _targetContentZoomScaleForRect:targetRect currentScale:currentScale fitEntireRect:fitEntireRect minimumScale:minimumScale maximumScale:maximumScale];

    if (fabs(targetScale - currentScale) < maximumScaleFactorDeltaForPanScroll) {
        if ([self _scrollToRect:targetRect origin:origin minimumScrollDistance:minimumScrollDistance])
            return true;
    } else if (targetScale != currentScale) {
        [self _zoomToRect:targetRect atScale:targetScale origin:origin animated:YES];
        return true;
    }

    return false;
}

- (void)didMoveToWindow
{
    if (!_overridesInterfaceOrientation)
        [self _dispatchSetDeviceOrientation:[self _deviceOrientation]];
    _page->activityStateDidChange(WebCore::ActivityState::allFlags());
    _page->webViewDidMoveToWindow();
}

- (void)_setOpaqueInternal:(BOOL)opaque
{
    [super setOpaque:opaque];

    [_contentView setOpaque:opaque];

    if (!_page)
        return;

    Optional<WebCore::Color> backgroundColor;
    if (!opaque)
        backgroundColor = WebCore::Color(WebCore::Color::transparent);
    _page->setBackgroundColor(backgroundColor);

    [self _updateScrollViewBackground];
}

- (void)setOpaque:(BOOL)opaque
{
    if (opaque == self.opaque)
        return;

    [self _setOpaqueInternal:opaque];
}

- (void)setBackgroundColor:(UIColor *)backgroundColor
{
    [super setBackgroundColor:backgroundColor];
    [_contentView setBackgroundColor:backgroundColor];
    [self _updateScrollViewBackground];
}

- (BOOL)_allowsDoubleTapGestures
{
    if (_fastClickingIsDisabled)
        return YES;

    // If the page is not user scalable, we don't allow double tap gestures.
    if (![_scrollView isZoomEnabled] || [_scrollView minimumZoomScale] >= [_scrollView maximumZoomScale])
        return NO;

    // If the viewport width was not explicit, we allow double tap gestures.
    if (!_viewportMetaTagWidthWasExplicit || _viewportMetaTagCameFromImageDocument)
        return YES;

    // If the page set a viewport width that wasn't the device width, then it was
    // scaled and thus will probably need to zoom.
    if (_viewportMetaTagWidth != WebCore::ViewportArguments::ValueDeviceWidth)
        return YES;

    // At this point, we have a page that asked for width = device-width. However,
    // if the content's width and height were large, we might have had to shrink it.
    // We'll enable double tap zoom whenever we're not at the actual initial scale.
    return !WTF::areEssentiallyEqual<float>(contentZoomScale(self), _initialScaleFactor);
}

- (BOOL)_stylusTapGestureShouldCreateEditableImage
{
    return [_configuration _editableImagesEnabled];
}

#pragma mark UIScrollViewDelegate

- (BOOL)usesStandardContentView
{
    return !_customContentView && !_passwordView;
}

- (CGSize)scrollView:(UIScrollView*)scrollView contentSizeForZoomScale:(CGFloat)scale withProposedSize:(CGSize)proposedSize
{
    return roundScrollViewContentSize(*_page, proposedSize);
}

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView
{
    ASSERT(_scrollView == scrollView);
    return self._currentContentView;
}

- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view
{
    if (![self usesStandardContentView]) {
        if ([_customContentView respondsToSelector:@selector(web_scrollViewWillBeginZooming:withView:)])
            [_customContentView web_scrollViewWillBeginZooming:scrollView withView:view];
        return;
    }

    if (scrollView.pinchGestureRecognizer.state == UIGestureRecognizerStateBegan) {
        _page->willStartUserTriggeredZooming();
        [_contentView scrollViewWillStartPanOrPinchGesture];
    }
    [_contentView willStartZoomOrScroll];

    [_contentView cancelPointersForGestureRecognizer:scrollView.pinchGestureRecognizer];
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
    if (![self usesStandardContentView])
        return;

    if (scrollView.panGestureRecognizer.state == UIGestureRecognizerStateBegan)
        [_contentView scrollViewWillStartPanOrPinchGesture];

    [_contentView willStartZoomOrScroll];
#if ENABLE(CSS_SCROLL_SNAP) && ENABLE(ASYNC_SCROLLING)
    // FIXME: We will want to detect whether snapping will occur before beginning to drag. See WebPageProxy::didCommitLayerTree.
    WebKit::RemoteScrollingCoordinatorProxy* coordinator = _page->scrollingCoordinatorProxy();
    ASSERT(scrollView == _scrollView.get());
    CGFloat scrollDecelerationFactor = (coordinator && coordinator->shouldSetScrollViewDecelerationRateFast()) ? UIScrollViewDecelerationRateFast : UIScrollViewDecelerationRateNormal;
    scrollView.horizontalScrollDecelerationFactor = scrollDecelerationFactor;
    scrollView.verticalScrollDecelerationFactor = scrollDecelerationFactor;
#endif
}

- (void)_didFinishScrolling
{
    if (![self usesStandardContentView])
        return;

    [self _scheduleVisibleContentRectUpdate];
    [_contentView didFinishScrolling];
}

- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView withVelocity:(CGPoint)velocity targetContentOffset:(inout CGPoint *)targetContentOffset
{
    // Work around <rdar://problem/16374753> by avoiding deceleration while
    // zooming. We'll animate to the right place once the zoom finishes.
    if ([scrollView isZooming])
        *targetContentOffset = [scrollView contentOffset];
    else {
        if ([_contentView preventsPanningInXAxis])
            targetContentOffset->x = scrollView.contentOffset.x;
        if ([_contentView preventsPanningInYAxis])
            targetContentOffset->y = scrollView.contentOffset.y;
    }
#if ENABLE(CSS_SCROLL_SNAP) && ENABLE(ASYNC_SCROLLING)
    if (WebKit::RemoteScrollingCoordinatorProxy* coordinator = _page->scrollingCoordinatorProxy()) {
        // FIXME: Here, I'm finding the maximum horizontal/vertical scroll offsets. There's probably a better way to do this.
        CGSize maxScrollOffsets = CGSizeMake(scrollView.contentSize.width - scrollView.bounds.size.width, scrollView.contentSize.height - scrollView.bounds.size.height);

        UIEdgeInsets obscuredInset;

        id<WKUIDelegatePrivate> uiDelegatePrivate = static_cast<id <WKUIDelegatePrivate>>([self UIDelegate]);
        if ([uiDelegatePrivate respondsToSelector:@selector(_webView:finalObscuredInsetsForScrollView:withVelocity:targetContentOffset:)])
            obscuredInset = [uiDelegatePrivate _webView:self finalObscuredInsetsForScrollView:scrollView withVelocity:velocity targetContentOffset:targetContentOffset];
        else
            obscuredInset = [self _computedObscuredInset];

        CGRect unobscuredRect = UIEdgeInsetsInsetRect(self.bounds, obscuredInset);

        coordinator->adjustTargetContentOffsetForSnapping(maxScrollOffsets, velocity, unobscuredRect.origin.y, targetContentOffset);
    }
#endif
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
    // If we're decelerating, scroll offset will be updated when scrollViewDidFinishDecelerating: is called.
    if (!decelerate)
        [self _didFinishScrolling];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
    [self _didFinishScrolling];
}

- (void)scrollViewDidScrollToTop:(UIScrollView *)scrollView
{
    [self _didFinishScrolling];
}

- (CGPoint)_scrollView:(UIScrollView *)scrollView adjustedOffsetForOffset:(CGPoint)offset translation:(CGPoint)translation startPoint:(CGPoint)start locationInView:(CGPoint)locationInView horizontalVelocity:(inout double *)hv verticalVelocity:(inout double *)vv
{
    if (![_contentView preventsPanningInXAxis] && ![_contentView preventsPanningInYAxis]) {
        [_contentView cancelPointersForGestureRecognizer:scrollView.panGestureRecognizer];
        return offset;
    }

    CGPoint adjustedContentOffset = CGPointMake(offset.x, offset.y);
    if ([_contentView preventsPanningInXAxis])
        adjustedContentOffset.x = start.x;
    if ([_contentView preventsPanningInYAxis])
        adjustedContentOffset.y = start.y;

    if ((![_contentView preventsPanningInXAxis] && adjustedContentOffset.x != start.x)
        || (![_contentView preventsPanningInYAxis] && adjustedContentOffset.y != start.y)) {
        [_contentView cancelPointersForGestureRecognizer:scrollView.panGestureRecognizer];
    }

    return adjustedContentOffset;
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    if (![self usesStandardContentView] && [_customContentView respondsToSelector:@selector(web_scrollViewDidScroll:)])
        [_customContentView web_scrollViewDidScroll:(UIScrollView *)scrollView];

    [self _scheduleVisibleContentRectUpdateAfterScrollInView:scrollView];

    if (WebKit::RemoteLayerTreeScrollingPerformanceData* scrollPerfData = _page->scrollingPerformanceData())
        scrollPerfData->didScroll([self visibleRectInViewCoordinates]);
}

- (void)scrollViewDidZoom:(UIScrollView *)scrollView
{
    if (![self usesStandardContentView] && [_customContentView respondsToSelector:@selector(web_scrollViewDidZoom:)])
        [_customContentView web_scrollViewDidZoom:scrollView];

    [self _updateScrollViewBackground];
    [self _scheduleVisibleContentRectUpdateAfterScrollInView:scrollView];
}

- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale
{
    if (![self usesStandardContentView] && [_customContentView respondsToSelector:@selector(web_scrollViewDidEndZooming:withView:atScale:)])
        [_customContentView web_scrollViewDidEndZooming:scrollView withView:view atScale:scale];

    ASSERT(scrollView == _scrollView);
    // FIXME: remove when rdar://problem/36065495 is fixed.
    // When rotating with two fingers down, UIScrollView can set a bogus content view position.
    // "Center" is top left because we set the anchorPoint to 0,0.
    [_contentView setCenter:self.bounds.origin];

    [self _scheduleVisibleContentRectUpdateAfterScrollInView:scrollView];
    [_contentView didZoomToScale:scale];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
    [self _didFinishScrolling];
}

- (void)_scrollViewDidInterruptDecelerating:(UIScrollView *)scrollView
{
    if (![self usesStandardContentView])
        return;

    [_contentView didInterruptScrolling];
    [self _scheduleVisibleContentRectUpdateAfterScrollInView:scrollView];
}

#pragma mark end UIScrollViewDelegate

- (CGRect)_visibleRectInEnclosingView:(UIView *)enclosingView
{
    if (!enclosingView)
        return self.bounds;

    CGRect exposedRect = [enclosingView convertRect:enclosingView.bounds toView:self];
    return CGRectIntersectsRect(exposedRect, self.bounds) ? CGRectIntersection(exposedRect, self.bounds) : CGRectZero;
}

- (CGRect)_visibleContentRect
{
    if (_frozenVisibleContentRect)
        return _frozenVisibleContentRect.value();

    CGRect visibleRectInContentCoordinates = [self convertRect:self.bounds toView:_contentView.get()];

    if (UIView *enclosingView = [self _enclosingViewForExposedRectComputation]) {
        CGRect viewVisibleRect = [self _visibleRectInEnclosingView:enclosingView];
        CGRect viewVisibleContentRect = [self convertRect:viewVisibleRect toView:_contentView.get()];
        visibleRectInContentCoordinates = CGRectIntersection(visibleRectInContentCoordinates, viewVisibleContentRect);
    }

    return visibleRectInContentCoordinates;
}

// Called when some ancestor UIScrollView scrolls.
- (void)_didScroll
{
    [self _scheduleVisibleContentRectUpdateAfterScrollInView:[self _scroller]];

    const NSTimeInterval ScrollingEndedTimerInterval = 0.032;
    if (!_enclosingScrollViewScrollTimer) {
        _enclosingScrollViewScrollTimer = adoptNS([[NSTimer alloc] initWithFireDate:[NSDate dateWithTimeIntervalSinceNow:ScrollingEndedTimerInterval]
            interval:0 target:self selector:@selector(_enclosingScrollerScrollingEnded:) userInfo:nil repeats:YES]);
        [[NSRunLoop mainRunLoop] addTimer:_enclosingScrollViewScrollTimer.get() forMode:NSDefaultRunLoopMode];
    }
    _didScrollSinceLastTimerFire = YES;
}

- (void)_enclosingScrollerScrollingEnded:(NSTimer *)timer
{
    if (_didScrollSinceLastTimerFire) {
        _didScrollSinceLastTimerFire = NO;
        return;
    }

    [self _scheduleVisibleContentRectUpdate];
    [_enclosingScrollViewScrollTimer invalidate];
    _enclosingScrollViewScrollTimer = nil;
}

- (UIEdgeInsets)_scrollViewSystemContentInset
{
    // It's not safe to access the scroll view's safeAreaInsets or _systemContentInset from
    // inside our layoutSubviews implementation, because they aren't updated until afterwards.
    // Instead, depend on the fact that the UIScrollView and WKWebView are in the same coordinate
    // space, and map the WKWebView's own insets into the scroll view manually.
    return UIEdgeInsetsAdd([_scrollView _contentScrollInset], self.safeAreaInsets, [_scrollView _edgesApplyingSafeAreaInsetsToContentInset]);
}

- (WebCore::FloatSize)activeViewLayoutSize:(const CGRect&)bounds
{
    if (_viewLayoutSizeOverride)
        return WebCore::FloatSize(_viewLayoutSizeOverride.value());

// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    return WebCore::FloatSize(UIEdgeInsetsInsetRect(CGRectMake(0, 0, bounds.size.width, bounds.size.height), self._scrollViewSystemContentInset).size);
#else
    return WebCore::FloatSize { bounds.size };
#endif
}

- (void)_dispatchSetViewLayoutSize:(WebCore::FloatSize)viewLayoutSize
{
    if (_lastSentViewLayoutSize && CGSizeEqualToSize(_lastSentViewLayoutSize.value(), viewLayoutSize))
        return;

    LOG_WITH_STREAM(VisibleRects, stream << "-[WKWebView " << _page->identifier() << " _dispatchSetViewLayoutSize:] " << viewLayoutSize << " contentZoomScale " << contentZoomScale(self));
    _page->setViewportConfigurationViewLayoutSize(viewLayoutSize, _page->layoutSizeScaleFactor(), _page->minimumEffectiveDeviceWidth());
    _lastSentViewLayoutSize = viewLayoutSize;
}

- (void)_dispatchSetMaximumUnobscuredSize:(WebCore::FloatSize)maximumUnobscuredSize
{
    if (_lastSentMaximumUnobscuredSize && CGSizeEqualToSize(_lastSentMaximumUnobscuredSize.value(), maximumUnobscuredSize))
        return;

    _page->setMaximumUnobscuredSize(maximumUnobscuredSize);
    _lastSentMaximumUnobscuredSize = maximumUnobscuredSize;
}

- (void)_dispatchSetDeviceOrientation:(int32_t)deviceOrientation
{
    if (_lastSentDeviceOrientation && _lastSentDeviceOrientation.value() == deviceOrientation)
        return;

    _page->setDeviceOrientation(deviceOrientation);
    _lastSentDeviceOrientation = deviceOrientation;
}

- (void)_frameOrBoundsChanged
{
    CGRect bounds = self.bounds;
    [_scrollView setFrame:bounds];

    if (_dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing) {
        if (!_viewLayoutSizeOverride)
            [self _dispatchSetViewLayoutSize:[self activeViewLayoutSize:self.bounds]];
        if (!_maximumUnobscuredSizeOverride)
            [self _dispatchSetMaximumUnobscuredSize:WebCore::FloatSize(bounds.size)];

        BOOL sizeChanged = NO;
        if (_page) {
            if (auto drawingArea = _page->drawingArea())
                sizeChanged = drawingArea->setSize(WebCore::IntSize(bounds.size));
        }

        if (sizeChanged & [self usesStandardContentView])
            [_contentView setSizeChangedSinceLastVisibleContentRectUpdate:YES];
    }

    [_customContentView web_setMinimumSize:bounds.size];
    [self _scheduleVisibleContentRectUpdate];
}

// Unobscured content rect where the user can interact. When the keyboard is up, this should be the area above or below the keyboard, wherever there is enough space.
- (CGRect)_contentRectForUserInteraction
{
    // FIXME: handle split keyboard.
    UIEdgeInsets obscuredInsets = _obscuredInsets;
    obscuredInsets.bottom = std::max(_obscuredInsets.bottom, _inputViewBounds.size.height);
    CGRect unobscuredRect = UIEdgeInsetsInsetRect(self.bounds, obscuredInsets);
    return [self convertRect:unobscuredRect toView:self._currentContentView];
}

// Ideally UIScrollView would expose this for us: <rdar://problem/21394567>.
- (BOOL)_scrollViewIsRubberBanding
{
    float deviceScaleFactor = _page->deviceScaleFactor();

    CGPoint contentOffset = [_scrollView contentOffset];
    CGPoint boundedOffset = contentOffsetBoundedInValidRange(_scrollView.get(), contentOffset);
    return !pointsEqualInDevicePixels(contentOffset, boundedOffset, deviceScaleFactor);
}

// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
- (void)safeAreaInsetsDidChange
{
    [super safeAreaInsetsDidChange];

    [self _scheduleVisibleContentRectUpdate];
    [_safeBrowsingWarning setContentInset:[self _computedObscuredInsetForSafeBrowsingWarning]];
}
#endif

- (void)_scheduleVisibleContentRectUpdate
{
    // For visible rect updates not associated with a specific UIScrollView, just consider our own scroller.
    [self _scheduleVisibleContentRectUpdateAfterScrollInView:_scrollView.get()];
}

- (BOOL)_scrollViewIsInStableState:(UIScrollView *)scrollView
{
    BOOL isStableState = !([scrollView isDragging] || [scrollView isDecelerating] || [scrollView isZooming] || [scrollView _isAnimatingZoom] || [scrollView _isScrollingToTop]);

    if (isStableState && scrollView == _scrollView.get())
        isStableState = !_isChangingObscuredInsetsInteractively;

    if (isStableState && scrollView == _scrollView.get())
        isStableState = ![self _scrollViewIsRubberBanding];

    if (isStableState)
        isStableState = !scrollView._isInterruptingDeceleration;

    if (NSNumber *stableOverride = self._stableStateOverride)
        isStableState = stableOverride.boolValue;

    return isStableState;
}

- (void)_addUpdateVisibleContentRectPreCommitHandler
{
    auto retainedSelf = retainPtr(self);
    [CATransaction addCommitHandler:[retainedSelf] {
        WKWebView *webView = retainedSelf.get();
        if (![webView _isValid]) {
            RELEASE_LOG_IF(webView._page && webView._page->isAlwaysOnLoggingAllowed(), ViewState, "In CATransaction preCommitHandler, WKWebView %p is invalid", webView);
            return;
        }

        @try {
            [webView _updateVisibleContentRects];
        } @catch (NSException *exception) {
            RELEASE_LOG_IF(webView._page && webView._page->isAlwaysOnLoggingAllowed(), ViewState, "In CATransaction preCommitHandler, -[WKWebView %p _updateVisibleContentRects] threw an exception", webView);
        }
        webView->_hasScheduledVisibleRectUpdate = NO;
    } forPhase:kCATransactionPhasePreCommit];
}

- (void)_scheduleVisibleContentRectUpdateAfterScrollInView:(UIScrollView *)scrollView
{
    _visibleContentRectUpdateScheduledFromScrollViewInStableState = [self _scrollViewIsInStableState:scrollView];

    if (_hasScheduledVisibleRectUpdate) {
        auto timeNow = MonotonicTime::now();
        if ((timeNow - _timeOfRequestForVisibleContentRectUpdate) > delayBeforeNoVisibleContentsRectsLogging) {
            RELEASE_LOG_IF_ALLOWED("-[WKWebView %p _scheduleVisibleContentRectUpdateAfterScrollInView]: _hasScheduledVisibleRectUpdate is true but haven't updated visible content rects for %.2fs (last update was %.2fs ago) - valid %d",
                self, (timeNow - _timeOfRequestForVisibleContentRectUpdate).value(), (timeNow - _timeOfLastVisibleContentRectUpdate).value(), [self _isValid]);
        }
        return;
    }

    _hasScheduledVisibleRectUpdate = YES;
    _timeOfRequestForVisibleContentRectUpdate = MonotonicTime::now();

    CATransactionPhase transactionPhase = [CATransaction currentPhase];
    if (transactionPhase == kCATransactionPhaseNull || transactionPhase == kCATransactionPhasePreLayout) {
        [self _addUpdateVisibleContentRectPreCommitHandler];
        return;
    }

    dispatch_async(dispatch_get_main_queue(), [retainedSelf = retainPtr(self)] {
        WKWebView *webView = retainedSelf.get();
        if (![webView _isValid])
            return;
        [webView _addUpdateVisibleContentRectPreCommitHandler];
    });
}

static bool scrollViewCanScroll(UIScrollView *scrollView)
{
    if (!scrollView)
        return NO;

    UIEdgeInsets contentInset = scrollView.contentInset;
    CGSize contentSize = scrollView.contentSize;
    CGSize boundsSize = scrollView.bounds.size;

    return (contentSize.width + contentInset.left + contentInset.right) > boundsSize.width
        || (contentSize.height + contentInset.top + contentInset.bottom) > boundsSize.height;
}

- (CGRect)_contentBoundsExtendedForRubberbandingWithScale:(CGFloat)scaleFactor
{
    CGPoint contentOffset = [_scrollView contentOffset];
    CGPoint boundedOffset = contentOffsetBoundedInValidRange(_scrollView.get(), contentOffset);

    CGFloat horiontalRubberbandAmountInContentCoordinates = (contentOffset.x - boundedOffset.x) / scaleFactor;
    CGFloat verticalRubberbandAmountInContentCoordinates = (contentOffset.y - boundedOffset.y) / scaleFactor;

    CGRect extendedBounds = [_contentView bounds];

    if (horiontalRubberbandAmountInContentCoordinates < 0) {
        extendedBounds.origin.x += horiontalRubberbandAmountInContentCoordinates;
        extendedBounds.size.width -= horiontalRubberbandAmountInContentCoordinates;
    } else if (horiontalRubberbandAmountInContentCoordinates > 0)
        extendedBounds.size.width += horiontalRubberbandAmountInContentCoordinates;

    if (verticalRubberbandAmountInContentCoordinates < 0) {
        extendedBounds.origin.y += verticalRubberbandAmountInContentCoordinates;
        extendedBounds.size.height -= verticalRubberbandAmountInContentCoordinates;
    } else if (verticalRubberbandAmountInContentCoordinates > 0)
        extendedBounds.size.height += verticalRubberbandAmountInContentCoordinates;

    return extendedBounds;
}

- (UIEdgeInsets)currentlyVisibleContentInsetsWithScale:(CGFloat)scaleFactor obscuredInsets:(UIEdgeInsets)obscuredInsets
{
    // The following logic computes the extent to which the bottom, top, left and right content insets are visible.
    auto scrollViewInsets = [_scrollView contentInset];
    auto scrollViewBounds = [_scrollView bounds];
    auto scrollViewContentSize = [_scrollView contentSize];
    auto scrollViewOriginIncludingInset = UIEdgeInsetsInsetRect(scrollViewBounds, obscuredInsets).origin;
    auto maximumVerticalScrollExtentWithoutRevealingBottomContentInset = scrollViewContentSize.height - CGRectGetHeight(scrollViewBounds);
    auto maximumHorizontalScrollExtentWithoutRevealingRightContentInset = scrollViewContentSize.width - CGRectGetWidth(scrollViewBounds);
    auto contentInsets = UIEdgeInsetsZero;

    if (scrollViewInsets.left > 0 && scrollViewOriginIncludingInset.x < 0)
        contentInsets.left = std::min(-scrollViewOriginIncludingInset.x, scrollViewInsets.left) / scaleFactor;

    if (scrollViewInsets.top > 0 && scrollViewOriginIncludingInset.y < 0)
        contentInsets.top = std::min(-scrollViewOriginIncludingInset.y, scrollViewInsets.top) / scaleFactor;

    if (scrollViewInsets.right > 0 && scrollViewOriginIncludingInset.x > maximumHorizontalScrollExtentWithoutRevealingRightContentInset)
        contentInsets.right = std::min(scrollViewOriginIncludingInset.x - maximumHorizontalScrollExtentWithoutRevealingRightContentInset, scrollViewInsets.right) / scaleFactor;

    if (scrollViewInsets.bottom > 0 && scrollViewOriginIncludingInset.y > maximumVerticalScrollExtentWithoutRevealingBottomContentInset)
        contentInsets.bottom = std::min(scrollViewOriginIncludingInset.y - maximumVerticalScrollExtentWithoutRevealingBottomContentInset, scrollViewInsets.bottom) / scaleFactor;

    return contentInsets;
}

- (void)_updateVisibleContentRects
{
    BOOL inStableState = _visibleContentRectUpdateScheduledFromScrollViewInStableState;

    if (![self usesStandardContentView]) {
        [_passwordView setFrame:self.bounds];
        [_customContentView web_computedContentInsetDidChange];
        _didDeferUpdateVisibleContentRectsForAnyReason = YES;
        RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - usesStandardContentView is NO, bailing", self, _page->identifier().toUInt64());
        return;
    }

    auto timeNow = MonotonicTime::now();
    if (_timeOfFirstVisibleContentRectUpdateWithPendingCommit) {
        auto timeSinceFirstRequestWithPendingCommit = timeNow - *_timeOfFirstVisibleContentRectUpdateWithPendingCommit;
        if (timeSinceFirstRequestWithPendingCommit > delayBeforeNoCommitsLogging)
            RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - have not received a commit %.2fs after visible content rect update; lastTransactionID %llu", self, _page->identifier().toUInt64(), timeSinceFirstRequestWithPendingCommit.value(), _lastTransactionID.toUInt64());
    }

    if (_invokingUIScrollViewDelegateCallback) {
        _didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback = YES;
        _didDeferUpdateVisibleContentRectsForAnyReason = YES;
        RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - _invokingUIScrollViewDelegateCallback is YES, bailing", self, _page->identifier().toUInt64());
        return;
    }

    if (_dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing
        || (_needsResetViewStateAfterCommitLoadForMainFrame && ![_contentView sizeChangedSinceLastVisibleContentRectUpdate])
        || [_scrollView isZoomBouncing]
        || _currentlyAdjustingScrollViewInsetsForKeyboard) {
        _didDeferUpdateVisibleContentRectsForAnyReason = YES;
        _didDeferUpdateVisibleContentRectsForUnstableScrollView = YES;
        RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - scroll view state is non-stable, bailing (_dynamicViewportUpdateMode %d, _needsResetViewStateAfterCommitLoadForMainFrame %d, sizeChangedSinceLastVisibleContentRectUpdate %d, [_scrollView isZoomBouncing] %d, _currentlyAdjustingScrollViewInsetsForKeyboard %d)",
            self, _page->identifier().toUInt64(), _dynamicViewportUpdateMode, _needsResetViewStateAfterCommitLoadForMainFrame, [_contentView sizeChangedSinceLastVisibleContentRectUpdate], [_scrollView isZoomBouncing], _currentlyAdjustingScrollViewInsetsForKeyboard);
        return;
    }

    if (_didDeferUpdateVisibleContentRectsForAnyReason)
        RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - performing first visible content rect update after deferring updates", self, _page->identifier().toUInt64());

    _didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback = NO;
    _didDeferUpdateVisibleContentRectsForUnstableScrollView = NO;
    _didDeferUpdateVisibleContentRectsForAnyReason = NO;

    CGRect visibleRectInContentCoordinates = [self _visibleContentRect];

    UIEdgeInsets computedContentInsetUnadjustedForKeyboard = [self _computedObscuredInset];
    if (!_haveSetObscuredInsets)
        computedContentInsetUnadjustedForKeyboard.bottom -= _totalScrollViewBottomInsetAdjustmentForKeyboard;

    CGFloat scaleFactor = contentZoomScale(self);
    CGRect unobscuredRect = UIEdgeInsetsInsetRect(self.bounds, computedContentInsetUnadjustedForKeyboard);
    CGRect unobscuredRectInContentCoordinates = _frozenUnobscuredContentRect ? _frozenUnobscuredContentRect.value() : [self convertRect:unobscuredRect toView:_contentView.get()];
    unobscuredRectInContentCoordinates = CGRectIntersection(unobscuredRectInContentCoordinates, [self _contentBoundsExtendedForRubberbandingWithScale:scaleFactor]);

    auto contentInsets = [self currentlyVisibleContentInsetsWithScale:scaleFactor obscuredInsets:computedContentInsetUnadjustedForKeyboard];

#if ENABLE(CSS_SCROLL_SNAP) && ENABLE(ASYNC_SCROLLING)
    if (inStableState) {
        WebKit::RemoteScrollingCoordinatorProxy* coordinator = _page->scrollingCoordinatorProxy();
        if (coordinator && coordinator->hasActiveSnapPoint()) {
            CGPoint currentPoint = [_scrollView contentOffset];
            CGPoint activePoint = coordinator->nearestActiveContentInsetAdjustedSnapPoint(unobscuredRect.origin.y, currentPoint);

            if (!CGPointEqualToPoint(activePoint, currentPoint)) {
                RetainPtr<WKScrollView> strongScrollView = _scrollView;
                dispatch_async(dispatch_get_main_queue(), [strongScrollView, activePoint] {
                    [strongScrollView setContentOffset:activePoint animated:NO];
                });
            }
        }
    }
#endif

    [_contentView didUpdateVisibleRect:visibleRectInContentCoordinates
        unobscuredRect:unobscuredRectInContentCoordinates
        contentInsets:contentInsets
        unobscuredRectInScrollViewCoordinates:unobscuredRect
        obscuredInsets:_obscuredInsets
        unobscuredSafeAreaInsets:[self _computedUnobscuredSafeAreaInset]
        inputViewBounds:_inputViewBounds
        scale:scaleFactor minimumScale:[_scrollView minimumZoomScale]
        inStableState:inStableState
        isChangingObscuredInsetsInteractively:_isChangingObscuredInsetsInteractively
        enclosedInScrollableAncestorView:scrollViewCanScroll([self _scroller])];

    while (!_visibleContentRectUpdateCallbacks.isEmpty()) {
        auto callback = _visibleContentRectUpdateCallbacks.takeLast();
        callback();
    }

    if ((timeNow - _timeOfRequestForVisibleContentRectUpdate) > delayBeforeNoVisibleContentsRectsLogging)
        RELEASE_LOG_IF_ALLOWED("%p -[WKWebView _updateVisibleContentRects:] finally ran %.2fs after being scheduled", self, (timeNow - _timeOfRequestForVisibleContentRectUpdate).value());

    _timeOfLastVisibleContentRectUpdate = timeNow;
    if (!_timeOfFirstVisibleContentRectUpdateWithPendingCommit)
        _timeOfFirstVisibleContentRectUpdateWithPendingCommit = timeNow;
}

- (void)_didStartProvisionalLoadForMainFrame
{
    if (_gestureController)
        _gestureController->didStartProvisionalLoadForMainFrame();
}

static WebCore::FloatSize activeMaximumUnobscuredSize(WKWebView *webView, const CGRect& bounds)
{
    return WebCore::FloatSize(webView->_maximumUnobscuredSizeOverride.valueOr(bounds.size));
}

static int32_t activeOrientation(WKWebView *webView)
{
    return webView->_overridesInterfaceOrientation ? deviceOrientationForUIInterfaceOrientation(webView->_interfaceOrientationOverride) : webView->_page->deviceOrientation();
}

- (void)_cancelAnimatedResize
{
    RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _cancelAnimatedResize] _dynamicViewportUpdateMode %d", self, _page->identifier().toUInt64(), _dynamicViewportUpdateMode);

    if (_dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing)
        return;

    if (!_customContentView) {
        if (_resizeAnimationView) {
            NSUInteger indexOfResizeAnimationView = [[_scrollView subviews] indexOfObject:_resizeAnimationView.get()];
            [_scrollView insertSubview:_contentView.get() atIndex:indexOfResizeAnimationView];
            [_scrollView insertSubview:[_contentView unscaledView] atIndex:indexOfResizeAnimationView + 1];
            [_resizeAnimationView removeFromSuperview];
            _resizeAnimationView = nil;
        }

        [_contentView setHidden:NO];
        _resizeAnimationTransformAdjustments = CATransform3DIdentity;
    }

    _dynamicViewportUpdateMode = WebKit::DynamicViewportUpdateMode::NotResizing;
    [self _scheduleVisibleContentRectUpdate];
}

- (void)_didCompleteAnimatedResize
{
    if (_dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing)
        return;

    [_contentView setHidden:NO];

    if (!_resizeAnimationView) {
        // Paranoia. If _resizeAnimationView is null we'll end up setting a zero scale on the content view.
        RELEASE_LOG_IF_ALLOWED("%p -[WKWebView _didCompleteAnimatedResize:] - _resizeAnimationView is nil", self);
        [self _cancelAnimatedResize];
        return;
    }

    RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _didCompleteAnimatedResize]", self, _page->identifier().toUInt64());

    NSUInteger indexOfResizeAnimationView = [[_scrollView subviews] indexOfObject:_resizeAnimationView.get()];
    [_scrollView insertSubview:_contentView.get() atIndex:indexOfResizeAnimationView];
    [_scrollView insertSubview:[_contentView unscaledView] atIndex:indexOfResizeAnimationView + 1];

    CALayer *contentLayer = [_contentView layer];
    CGFloat adjustmentScale = _resizeAnimationTransformAdjustments.m11;
    contentLayer.sublayerTransform = CATransform3DIdentity;

    CGFloat animatingScaleTarget = [[_resizeAnimationView layer] transform].m11;
    CATransform3D contentLayerTransform = contentLayer.transform;
    CGFloat currentScale = [[_resizeAnimationView layer] transform].m11 * contentLayerTransform.m11;

    // We cannot use [UIScrollView setZoomScale:] directly because the UIScrollView delegate would get a callback with
    // an invalid contentOffset. The real content offset is only set below.
    // Since there is no public API for setting both the zoomScale and the contentOffset, we set the zoomScale manually
    // on the zoom layer and then only change the contentOffset.
    CGFloat adjustedScale = adjustmentScale * currentScale;
    contentLayerTransform.m11 = adjustedScale;
    contentLayerTransform.m22 = adjustedScale;
    contentLayer.transform = contentLayerTransform;

    CGPoint currentScrollOffset = [_scrollView contentOffset];
    double horizontalScrollAdjustement = _resizeAnimationTransformAdjustments.m41 * animatingScaleTarget;
    double verticalScrollAdjustment = _resizeAnimationTransformAdjustments.m42 * animatingScaleTarget;

    [_scrollView setContentSize:roundScrollViewContentSize(*_page, [_contentView frame].size)];
    [_scrollView setContentOffset:CGPointMake(currentScrollOffset.x - horizontalScrollAdjustement, currentScrollOffset.y - verticalScrollAdjustment)];

    [_resizeAnimationView removeFromSuperview];
    _resizeAnimationView = nil;
    _resizeAnimationTransformAdjustments = CATransform3DIdentity;

    _dynamicViewportUpdateMode = WebKit::DynamicViewportUpdateMode::NotResizing;
    [self _scheduleVisibleContentRectUpdate];

    CGRect newBounds = self.bounds;
    auto newViewLayoutSize = [self activeViewLayoutSize:newBounds];
    auto newMaximumUnobscuredSize = activeMaximumUnobscuredSize(self, newBounds);
    int32_t newOrientation = activeOrientation(self);

    if (!_lastSentViewLayoutSize || newViewLayoutSize != _lastSentViewLayoutSize.value())
        [self _dispatchSetViewLayoutSize:newViewLayoutSize];

    if (!_lastSentMaximumUnobscuredSize || newMaximumUnobscuredSize != _lastSentMaximumUnobscuredSize.value())
        [self _dispatchSetMaximumUnobscuredSize:WebCore::FloatSize(newMaximumUnobscuredSize)];

    if (!_lastSentDeviceOrientation || newOrientation != _lastSentDeviceOrientation.value())
        [self _dispatchSetDeviceOrientation:newOrientation];

    while (!_callbacksDeferredDuringResize.isEmpty())
        _callbacksDeferredDuringResize.takeLast()();
}

- (void)_didFinishLoadForMainFrame
{
    if (_gestureController)
        _gestureController->didFinishLoadForMainFrame();
}

- (void)_didFailLoadForMainFrame
{
    if (_gestureController)
        _gestureController->didFailLoadForMainFrame();
}

- (void)_didSameDocumentNavigationForMainFrame:(WebKit::SameDocumentNavigationType)navigationType
{
    [_customContentView web_didSameDocumentNavigation:toAPI(navigationType)];

    if (_gestureController)
        _gestureController->didSameDocumentNavigationForMainFrame(navigationType);
}

- (void)_keyboardChangedWithInfo:(NSDictionary *)keyboardInfo adjustScrollView:(BOOL)adjustScrollView
{
    NSValue *endFrameValue = [keyboardInfo objectForKey:UIKeyboardFrameEndUserInfoKey];
    if (!endFrameValue)
        return;

    // The keyboard rect is always in screen coordinates. In the view services case the window does not
    // have the interface orientation rotation transformation; its host does. So, it makes no sense to
    // clip the keyboard rect against its screen.
    if ([[self window] _isHostedInAnotherProcess])
        _inputViewBounds = [self.window convertRect:[endFrameValue CGRectValue] fromWindow:nil];
    else
        _inputViewBounds = [self.window convertRect:CGRectIntersection([endFrameValue CGRectValue], self.window.screen.bounds) fromWindow:nil];

    if ([[UIPeripheralHost sharedInstance] isUndocked])
        _inputViewBounds = CGRectZero;

    if (adjustScrollView) {
        CGFloat bottomInsetBeforeAdjustment = [_scrollView contentInset].bottom;
        SetForScope<BOOL> insetAdjustmentGuard(_currentlyAdjustingScrollViewInsetsForKeyboard, YES);
        [_scrollView _adjustForAutomaticKeyboardInfo:keyboardInfo animated:YES lastAdjustment:&_lastAdjustmentForScroller];
        CGFloat bottomInsetAfterAdjustment = [_scrollView contentInset].bottom;
        // FIXME: This "total bottom content inset adjustment" mechanism hasn't worked since iOS 11, since -_adjustForAutomaticKeyboardInfo:animated:lastAdjustment:
        // no longer sets -[UIScrollView contentInset] for apps linked on or after iOS 11. We should consider removing this logic, since the original bug this was
        // intended to fix, <rdar://problem/23202254>, remains fixed through other means.
        if (bottomInsetBeforeAdjustment != bottomInsetAfterAdjustment)
            _totalScrollViewBottomInsetAdjustmentForKeyboard += bottomInsetAfterAdjustment - bottomInsetBeforeAdjustment;
    }

    [self _scheduleVisibleContentRectUpdate];
}

- (BOOL)_shouldUpdateKeyboardWithInfo:(NSDictionary *)keyboardInfo
{
    if ([_contentView isFocusingElement])
        return YES;

    NSNumber *isLocalKeyboard = [keyboardInfo valueForKey:UIKeyboardIsLocalUserInfoKey];
    return isLocalKeyboard && !isLocalKeyboard.boolValue;
}

- (void)_keyboardWillChangeFrame:(NSNotification *)notification
{
    if ([self _shouldUpdateKeyboardWithInfo:notification.userInfo])
        [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:YES];
}

- (void)_keyboardDidChangeFrame:(NSNotification *)notification
{
    [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:NO];
}

- (void)_keyboardWillShow:(NSNotification *)notification
{
    if ([self _shouldUpdateKeyboardWithInfo:notification.userInfo])
        [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:YES];

    _page->setIsKeyboardAnimatingIn(true);
}

- (void)_keyboardDidShow:(NSNotification *)notification
{
    _page->setIsKeyboardAnimatingIn(false);
}

- (void)_keyboardWillHide:(NSNotification *)notification
{
    if ([_contentView shouldIgnoreKeyboardWillHideNotification])
        return;

    [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:YES];
}

- (void)_windowDidRotate:(NSNotification *)notification
{
    if (!_overridesInterfaceOrientation)
        [self _dispatchSetDeviceOrientation:[self _deviceOrientation]];
}

- (void)_contentSizeCategoryDidChange:(NSNotification *)notification
{
    _page->contentSizeCategoryDidChange([self _contentSizeCategory]);
}

- (void)_accessibilitySettingsDidChange:(NSNotification *)notification
{
    _page->accessibilitySettingsDidChange();
}

- (NSString *)_contentSizeCategory
{
    return [[UIApplication sharedApplication] preferredContentSizeCategory];
}

- (BOOL)_isNavigationSwipeGestureRecognizer:(UIGestureRecognizer *)recognizer
{
    if (!_gestureController)
        return NO;
    return _gestureController->isNavigationSwipeGestureRecognizer(recognizer);
}

- (void)_navigationGestureDidBegin
{
    // During a back/forward swipe, there's a view interposed between this view and the content view that has
    // an offset and animation on it, which results in computing incorrect rectangles. Work around by using
    // frozen rects during swipes.
    CGRect fullViewRect = self.bounds;
    CGRect unobscuredRect = UIEdgeInsetsInsetRect(fullViewRect, [self _computedObscuredInset]);

    _frozenVisibleContentRect = [self convertRect:fullViewRect toView:_contentView.get()];
    _frozenUnobscuredContentRect = [self convertRect:unobscuredRect toView:_contentView.get()];

    LOG_WITH_STREAM(VisibleRects, stream << "_navigationGestureDidBegin: freezing visibleContentRect " << WebCore::FloatRect(_frozenVisibleContentRect.value()) << " UnobscuredContentRect " << WebCore::FloatRect(_frozenUnobscuredContentRect.value()));
}

- (void)_navigationGestureDidEnd
{
    _frozenVisibleContentRect = WTF::nullopt;
    _frozenUnobscuredContentRect = WTF::nullopt;
}

- (void)_showPasswordViewWithDocumentName:(NSString *)documentName passwordHandler:(void (^)(NSString *))passwordHandler
{
    ASSERT(!_passwordView);
    _passwordView = adoptNS([[WKPasswordView alloc] initWithFrame:self.bounds documentName:documentName]);
    [_passwordView setUserDidEnterPassword:passwordHandler];
    [_passwordView showInScrollView:_scrollView.get()];
    self._currentContentView.hidden = YES;
}

- (void)_hidePasswordView
{
    if (!_passwordView)
        return;

    self._currentContentView.hidden = NO;
    [_passwordView hide];
    _passwordView = nil;
}

- (WKPasswordView *)_passwordView
{
    return _passwordView.get();
}

- (void)_updateScrollViewInsetAdjustmentBehavior
{
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if (![_scrollView _contentInsetAdjustmentBehaviorWasExternallyOverridden])
        [_scrollView _setContentInsetAdjustmentBehaviorInternal:self._safeAreaShouldAffectObscuredInsets ? UIScrollViewContentInsetAdjustmentAlways : UIScrollViewContentInsetAdjustmentNever];
#endif
}

- (void)_setAvoidsUnsafeArea:(BOOL)avoidsUnsafeArea
{
    if (_avoidsUnsafeArea == avoidsUnsafeArea)
        return;

    _avoidsUnsafeArea = avoidsUnsafeArea;

    [self _updateScrollViewInsetAdjustmentBehavior];
    [self _scheduleVisibleContentRectUpdate];

    id <WKUIDelegatePrivate> uiDelegate = (id <WKUIDelegatePrivate>)[self UIDelegate];
    if ([uiDelegate respondsToSelector:@selector(_webView:didChangeSafeAreaShouldAffectObscuredInsets:)])
        [uiDelegate _webView:self didChangeSafeAreaShouldAffectObscuredInsets:avoidsUnsafeArea];
}

- (BOOL)_haveSetObscuredInsets
{
    return _haveSetObscuredInsets;
}

#if ENABLE(FULLSCREEN_API)
- (void)removeFromSuperview
{
    [super removeFromSuperview];

    if ([_fullScreenWindowController isFullScreen])
        [_fullScreenWindowController webViewDidRemoveFromSuperviewWhileInFullscreen];
}
#endif

- (void)_firePresentationUpdateForPendingStableStatePresentationCallbacks
{
    RetainPtr<WKWebView> strongSelf = self;
    [self _doAfterNextPresentationUpdate:[strongSelf] {
        dispatch_async(dispatch_get_main_queue(), [strongSelf] {
            if ([strongSelf->_stableStatePresentationUpdateCallbacks count])
                [strongSelf _firePresentationUpdateForPendingStableStatePresentationCallbacks];
        });
    }];
}

static WebCore::UserInterfaceLayoutDirection toUserInterfaceLayoutDirection(UISemanticContentAttribute contentAttribute)
{
    auto direction = [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:contentAttribute];
    switch (direction) {
    case UIUserInterfaceLayoutDirectionLeftToRight:
        return WebCore::UserInterfaceLayoutDirection::LTR;
    case UIUserInterfaceLayoutDirectionRightToLeft:
        return WebCore::UserInterfaceLayoutDirection::RTL;
    }

    ASSERT_NOT_REACHED();
    return WebCore::UserInterfaceLayoutDirection::LTR;
}

- (void)setSemanticContentAttribute:(UISemanticContentAttribute)contentAttribute
{
    [super setSemanticContentAttribute:contentAttribute];

    if (_page)
        _page->setUserInterfaceLayoutDirection(toUserInterfaceLayoutDirection(contentAttribute));
}

@end

@implementation WKWebView (WKPrivateIOS)

- (CGRect)_contentVisibleRect
{
    return [self convertRect:[self bounds] toView:self._currentContentView];
}

// Deprecated SPI.
- (CGSize)_minimumLayoutSizeOverride
{
    ASSERT(_viewLayoutSizeOverride);
    return _viewLayoutSizeOverride.valueOr(CGSizeZero);
}

- (void)_setViewLayoutSizeOverride:(CGSize)viewLayoutSizeOverride
{
    _viewLayoutSizeOverride = viewLayoutSizeOverride;

    if (_dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing)
        [self _dispatchSetViewLayoutSize:WebCore::FloatSize(viewLayoutSizeOverride)];
}

// Deprecated SPI
- (CGSize)_maximumUnobscuredSizeOverride
{
    ASSERT(_maximumUnobscuredSizeOverride);
    return _maximumUnobscuredSizeOverride.valueOr(CGSizeZero);
}

- (void)_setMaximumUnobscuredSizeOverride:(CGSize)size
{
    ASSERT(size.width <= self.bounds.size.width && size.height <= self.bounds.size.height);
    _maximumUnobscuredSizeOverride = size;

    if (_dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing)
        [self _dispatchSetMaximumUnobscuredSize:WebCore::FloatSize(size)];
}

- (UIEdgeInsets)_obscuredInsets
{
    return _obscuredInsets;
}

- (void)_setObscuredInsets:(UIEdgeInsets)obscuredInsets
{
    ASSERT(obscuredInsets.top >= 0);
    ASSERT(obscuredInsets.left >= 0);
    ASSERT(obscuredInsets.bottom >= 0);
    ASSERT(obscuredInsets.right >= 0);

    _haveSetObscuredInsets = YES;

    if (UIEdgeInsetsEqualToEdgeInsets(_obscuredInsets, obscuredInsets))
        return;

    _obscuredInsets = obscuredInsets;

    [self _scheduleVisibleContentRectUpdate];
    [_safeBrowsingWarning setContentInset:[self _computedObscuredInsetForSafeBrowsingWarning]];
}

- (UIRectEdge)_obscuredInsetEdgesAffectedBySafeArea
{
    return _obscuredInsetEdgesAffectedBySafeArea;
}

- (void)_setObscuredInsetEdgesAffectedBySafeArea:(UIRectEdge)edges
{
    if (edges == _obscuredInsetEdgesAffectedBySafeArea)
        return;

    _obscuredInsetEdgesAffectedBySafeArea = edges;

    [self _scheduleVisibleContentRectUpdate];
}

- (UIEdgeInsets)_unobscuredSafeAreaInsets
{
    return _unobscuredSafeAreaInsets;
}

- (void)_setUnobscuredSafeAreaInsets:(UIEdgeInsets)unobscuredSafeAreaInsets
{
    ASSERT(unobscuredSafeAreaInsets.top >= 0);
    ASSERT(unobscuredSafeAreaInsets.left >= 0);
    ASSERT(unobscuredSafeAreaInsets.bottom >= 0);
    ASSERT(unobscuredSafeAreaInsets.right >= 0);

    _haveSetUnobscuredSafeAreaInsets = YES;

    if (UIEdgeInsetsEqualToEdgeInsets(_unobscuredSafeAreaInsets, unobscuredSafeAreaInsets))
        return;

    _unobscuredSafeAreaInsets = unobscuredSafeAreaInsets;

    [self _scheduleVisibleContentRectUpdate];
}

- (BOOL)_safeAreaShouldAffectObscuredInsets
{
    if (![self usesStandardContentView])
        return NO;
    return _avoidsUnsafeArea;
}

- (UIView *)_enclosingViewForExposedRectComputation
{
    return [self _scroller];
}

- (void)_setInterfaceOrientationOverride:(UIInterfaceOrientation)interfaceOrientation
{
    _overridesInterfaceOrientation = YES;
    _interfaceOrientationOverride = interfaceOrientation;

    if (_dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing)
        [self _dispatchSetDeviceOrientation:deviceOrientationForUIInterfaceOrientation(_interfaceOrientationOverride)];
}

- (UIInterfaceOrientation)_interfaceOrientationOverride
{
    ASSERT(_overridesInterfaceOrientation);
    return _interfaceOrientationOverride;
}

- (void)_clearInterfaceOrientationOverride
{
    _overridesInterfaceOrientation = NO;
    _interfaceOrientationOverride = UIInterfaceOrientationPortrait;
}

- (void)_setAllowsViewportShrinkToFit:(BOOL)allowShrinkToFit
{
    _allowsViewportShrinkToFit = allowShrinkToFit;
}

- (BOOL)_allowsViewportShrinkToFit
{
    return _allowsViewportShrinkToFit;
}

- (BOOL)_isDisplayingPDF
{
    for (auto& mimeType : WebCore::MIMETypeRegistry::pdfMIMETypes()) {
        Class providerClass = [[_configuration _contentProviderRegistry] providerForMIMEType:mimeType];
        if ([_customContentView isKindOfClass:providerClass])
            return YES;
    }

    return NO;
}

- (NSData *)_dataForDisplayedPDF
{
    if (![self _isDisplayingPDF])
        return nil;
    return [_customContentView web_dataRepresentation];
}

- (NSString *)_suggestedFilenameForDisplayedPDF
{
    if (![self _isDisplayingPDF])
        return nil;
    return [_customContentView web_suggestedFilename];
}

- (_WKWebViewPrintFormatter *)_webViewPrintFormatter
{
    UIViewPrintFormatter *viewPrintFormatter = self.viewPrintFormatter;
    ASSERT([viewPrintFormatter isKindOfClass:[_WKWebViewPrintFormatter class]]);
    return (_WKWebViewPrintFormatter *)viewPrintFormatter;
}

- (_WKDragInteractionPolicy)_dragInteractionPolicy
{
    return _dragInteractionPolicy;
}

- (void)_setDragInteractionPolicy:(_WKDragInteractionPolicy)policy
{
    if (_dragInteractionPolicy == policy)
        return;

    _dragInteractionPolicy = policy;
#if ENABLE(DRAG_SUPPORT)
    [_contentView _didChangeDragInteractionPolicy];
#endif
}

- (BOOL)_shouldAvoidResizingWhenInputViewBoundsChange
{
    return [_contentView _shouldAvoidResizingWhenInputViewBoundsChange];
}

- (BOOL)_contentViewIsFirstResponder
{
    return self._currentContentView.isFirstResponder;
}

- (CGRect)_uiTextCaretRect
{
    // Force the selection view to update if needed.
    [_contentView _updateChangedSelection];

    return [[_contentView valueForKeyPath:@"interactionAssistant.selectionView.selection.caretRect"] CGRectValue];
}

- (UIView *)_safeBrowsingWarning
{
    return _safeBrowsingWarning.get();
}

- (CGPoint)_convertPointFromContentsToView:(CGPoint)point
{
    return [self convertPoint:point fromView:self._currentContentView];
}

- (CGPoint)_convertPointFromViewToContents:(CGPoint)point
{
    return [self convertPoint:point toView:self._currentContentView];
}

- (void)_doAfterNextStablePresentationUpdate:(dispatch_block_t)updateBlock
{
    if (![self usesStandardContentView]) {
        dispatch_async(dispatch_get_main_queue(), updateBlock);
        return;
    }

    auto updateBlockCopy = makeBlockPtr(updateBlock);

    if (_stableStatePresentationUpdateCallbacks)
        [_stableStatePresentationUpdateCallbacks addObject:updateBlockCopy.get()];
    else {
        _stableStatePresentationUpdateCallbacks = adoptNS([[NSMutableArray alloc] initWithObjects:updateBlockCopy.get(), nil]);
        [self _firePresentationUpdateForPendingStableStatePresentationCallbacks];
    }
}

- (void)_setFont:(UIFont *)font sender:(id)sender
{
    if (self.usesStandardContentView)
        [_contentView _setFontForWebView:font sender:sender];
}

- (void)_setFontSize:(CGFloat)fontSize sender:(id)sender
{
    if (self.usesStandardContentView)
        [_contentView _setFontSizeForWebView:fontSize sender:sender];
}

- (void)_setTextColor:(UIColor *)color sender:(id)sender
{
    if (self.usesStandardContentView)
        [_contentView _setTextColorForWebView:color sender:sender];
}

- (void)_detectDataWithTypes:(WKDataDetectorTypes)types completionHandler:(dispatch_block_t)completion
{
#if ENABLE(DATA_DETECTION)
    _page->detectDataInAllFrames(fromWKDataDetectorTypes(types), [completion = makeBlockPtr(completion), page = makeWeakPtr(_page.get())] (auto& result) {
        if (page)
            page->setDataDetectionResult(result);
        if (completion)
            completion();
    });
#else
    UNUSED_PARAM(types);
    UNUSED_PARAM(completion);
#endif
}

- (void)_requestActivatedElementAtPosition:(CGPoint)position completionBlock:(void (^)(_WKActivatedElementInfo *))block
{
    auto infoRequest = WebKit::InteractionInformationRequest(WebCore::roundedIntPoint(position));
    infoRequest.includeSnapshot = true;

    [_contentView doAfterPositionInformationUpdate:[capturedBlock = makeBlockPtr(block)] (WebKit::InteractionInformationAtPosition information) {
        capturedBlock([_WKActivatedElementInfo activatedElementInfoWithInteractionInformationAtPosition:information userInfo:nil]);
    } forRequest:infoRequest];
}

- (void)didStartFormControlInteraction
{
    // For subclasses to override.
}

- (void)didEndFormControlInteraction
{
    // For subclasses to override.
}

- (void)_beginInteractiveObscuredInsetsChange
{
    ASSERT(!_isChangingObscuredInsetsInteractively);
    _isChangingObscuredInsetsInteractively = YES;
}

- (void)_endInteractiveObscuredInsetsChange
{
    ASSERT(_isChangingObscuredInsetsInteractively);
    _isChangingObscuredInsetsInteractively = NO;
    [self _scheduleVisibleContentRectUpdate];
}

- (void)_hideContentUntilNextUpdate
{
    if (auto* area = _page->drawingArea())
        area->hideContentUntilAnyUpdate();
}

- (void)_beginAnimatedResizeWithUpdates:(void (^)(void))updateBlock
{
    CGRect oldBounds = self.bounds;
    WebCore::FloatRect oldUnobscuredContentRect = _page->unobscuredContentRect();

    if (![self usesStandardContentView] || !_hasCommittedLoadForMainFrame || CGRectIsEmpty(oldBounds) || oldUnobscuredContentRect.isEmpty()) {
        if ([_customContentView respondsToSelector:@selector(web_beginAnimatedResizeWithUpdates:)])
            [_customContentView web_beginAnimatedResizeWithUpdates:updateBlock];
        else
            updateBlock();
        return;
    }

    RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _beginAnimatedResizeWithUpdates:]", self, _page->identifier().toUInt64());

    _dynamicViewportUpdateMode = WebKit::DynamicViewportUpdateMode::ResizingWithAnimation;

    auto oldMinimumEffectiveDeviceWidth = [self _minimumEffectiveDeviceWidth];
    auto oldViewLayoutSize = [self activeViewLayoutSize:self.bounds];
    auto oldMaximumUnobscuredSize = activeMaximumUnobscuredSize(self, oldBounds);
    int32_t oldOrientation = activeOrientation(self);
    UIEdgeInsets oldObscuredInsets = _obscuredInsets;

    updateBlock();

    CGRect newBounds = self.bounds;
    auto newMinimumEffectiveDeviceWidth = [self _minimumEffectiveDeviceWidth];
    auto newViewLayoutSize = [self activeViewLayoutSize:newBounds];
    auto newMaximumUnobscuredSize = activeMaximumUnobscuredSize(self, newBounds);
    int32_t newOrientation = activeOrientation(self);
    UIEdgeInsets newObscuredInsets = _obscuredInsets;
    CGRect futureUnobscuredRectInSelfCoordinates = UIEdgeInsetsInsetRect(newBounds, _obscuredInsets);
    CGRect contentViewBounds = [_contentView bounds];

    ASSERT_WITH_MESSAGE(!(_viewLayoutSizeOverride && newViewLayoutSize.isEmpty()), "Clients controlling the layout size should maintain a valid layout size to minimize layouts.");
    if (CGRectIsEmpty(newBounds) || newViewLayoutSize.isEmpty() || CGRectIsEmpty(futureUnobscuredRectInSelfCoordinates) || CGRectIsEmpty(contentViewBounds)) {
        [self _cancelAnimatedResize];
        [self _frameOrBoundsChanged];
        if (_viewLayoutSizeOverride)
            [self _dispatchSetViewLayoutSize:newViewLayoutSize];
        if (_maximumUnobscuredSizeOverride)
            [self _dispatchSetMaximumUnobscuredSize:WebCore::FloatSize(newMaximumUnobscuredSize)];
        if (_overridesInterfaceOrientation)
            [self _dispatchSetDeviceOrientation:newOrientation];

        return;
    }

    if (CGRectEqualToRect(oldBounds, newBounds)
        && oldViewLayoutSize == newViewLayoutSize
        && oldMaximumUnobscuredSize == newMaximumUnobscuredSize
        && oldOrientation == newOrientation
        && oldMinimumEffectiveDeviceWidth == newMinimumEffectiveDeviceWidth
        && UIEdgeInsetsEqualToEdgeInsets(oldObscuredInsets, newObscuredInsets)) {
        [self _cancelAnimatedResize];
        return;
    }

    _resizeAnimationTransformAdjustments = CATransform3DIdentity;

    if (!_resizeAnimationView) {
        NSUInteger indexOfContentView = [[_scrollView subviews] indexOfObject:_contentView.get()];
        _resizeAnimationView = adoptNS([[UIView alloc] init]);
        [_resizeAnimationView layer].name = @"ResizeAnimation";
        [_scrollView insertSubview:_resizeAnimationView.get() atIndex:indexOfContentView];
        [_resizeAnimationView addSubview:_contentView.get()];
        [_resizeAnimationView addSubview:[_contentView unscaledView]];
    }

    CGSize contentSizeInContentViewCoordinates = contentViewBounds.size;
    [_scrollView setMinimumZoomScale:std::min(newViewLayoutSize.width() / contentSizeInContentViewCoordinates.width, [_scrollView minimumZoomScale])];
    [_scrollView setMaximumZoomScale:std::max(newViewLayoutSize.width() / contentSizeInContentViewCoordinates.width, [_scrollView maximumZoomScale])];

    // Compute the new scale to keep the current content width in the scrollview.
    CGFloat oldWebViewWidthInContentViewCoordinates = oldUnobscuredContentRect.width();
    _animatedResizeOriginalContentWidth = std::min(contentSizeInContentViewCoordinates.width, oldWebViewWidthInContentViewCoordinates);
    CGFloat targetScale = newViewLayoutSize.width() / _animatedResizeOriginalContentWidth;
    CGFloat resizeAnimationViewAnimationScale = targetScale / contentZoomScale(self);
    [_resizeAnimationView setTransform:CGAffineTransformMakeScale(resizeAnimationViewAnimationScale, resizeAnimationViewAnimationScale)];

    // Compute a new position to keep the content centered.
    CGPoint originalContentCenter = oldUnobscuredContentRect.center();
    CGPoint originalContentCenterInSelfCoordinates = [self convertPoint:originalContentCenter fromView:_contentView.get()];
    CGPoint futureUnobscuredRectCenterInSelfCoordinates = CGPointMake(futureUnobscuredRectInSelfCoordinates.origin.x + futureUnobscuredRectInSelfCoordinates.size.width / 2, futureUnobscuredRectInSelfCoordinates.origin.y + futureUnobscuredRectInSelfCoordinates.size.height / 2);

    CGPoint originalContentOffset = [_scrollView contentOffset];
    CGPoint contentOffset = originalContentOffset;
    contentOffset.x += (originalContentCenterInSelfCoordinates.x - futureUnobscuredRectCenterInSelfCoordinates.x);
    contentOffset.y += (originalContentCenterInSelfCoordinates.y - futureUnobscuredRectCenterInSelfCoordinates.y);

    // Limit the new offset within the scrollview, we do not want to rubber band programmatically.
    CGSize futureContentSizeInSelfCoordinates = CGSizeMake(contentSizeInContentViewCoordinates.width * targetScale, contentSizeInContentViewCoordinates.height * targetScale);
    CGFloat maxHorizontalOffset = futureContentSizeInSelfCoordinates.width - newBounds.size.width + _obscuredInsets.right;
    contentOffset.x = std::min(contentOffset.x, maxHorizontalOffset);
    CGFloat maxVerticalOffset = futureContentSizeInSelfCoordinates.height - newBounds.size.height + _obscuredInsets.bottom;
    contentOffset.y = std::min(contentOffset.y, maxVerticalOffset);

    contentOffset.x = std::max(contentOffset.x, -_obscuredInsets.left);
    contentOffset.y = std::max(contentOffset.y, -_obscuredInsets.top);

    // Make the top/bottom edges "sticky" within 1 pixel.
    if (oldUnobscuredContentRect.maxY() > contentSizeInContentViewCoordinates.height - 1)
        contentOffset.y = maxVerticalOffset;
    if (oldUnobscuredContentRect.y() < 1)
        contentOffset.y = [self _initialContentOffsetForScrollView].y;

    // FIXME: if we have content centered after double tap to zoom, we should also try to keep that rect in view.
    [_scrollView setContentSize:roundScrollViewContentSize(*_page, futureContentSizeInSelfCoordinates)];
    [_scrollView setContentOffset:contentOffset];

    CGRect visibleRectInContentCoordinates = [self convertRect:newBounds toView:_contentView.get()];
    CGRect unobscuredRectInContentCoordinates = [self convertRect:futureUnobscuredRectInSelfCoordinates toView:_contentView.get()];

    UIEdgeInsets unobscuredSafeAreaInsets = [self _computedUnobscuredSafeAreaInset];
    WebCore::FloatBoxExtent unobscuredSafeAreaInsetsExtent(unobscuredSafeAreaInsets.top, unobscuredSafeAreaInsets.right, unobscuredSafeAreaInsets.bottom, unobscuredSafeAreaInsets.left);

    _lastSentViewLayoutSize = newViewLayoutSize;
    _lastSentMaximumUnobscuredSize = newMaximumUnobscuredSize;
    _lastSentDeviceOrientation = newOrientation;

    _page->dynamicViewportSizeUpdate(newViewLayoutSize, newMaximumUnobscuredSize, visibleRectInContentCoordinates, unobscuredRectInContentCoordinates, futureUnobscuredRectInSelfCoordinates, unobscuredSafeAreaInsetsExtent, targetScale, newOrientation, newMinimumEffectiveDeviceWidth, ++_currentDynamicViewportSizeUpdateID);
    if (WebKit::DrawingAreaProxy* drawingArea = _page->drawingArea())
        drawingArea->setSize(WebCore::IntSize(newBounds.size));

    _waitingForCommitAfterAnimatedResize = YES;
    _waitingForEndAnimatedResize = YES;
}

- (void)_endAnimatedResize
{
    RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _endAnimatedResize] _dynamicViewportUpdateMode %d", self, _page->identifier().toUInt64(), _dynamicViewportUpdateMode);

    // If we already have an up-to-date layer tree, immediately complete
    // the resize. Otherwise, we will defer completion until we do.
    _waitingForEndAnimatedResize = NO;
    if (!_waitingForCommitAfterAnimatedResize)
        [self _didCompleteAnimatedResize];
}

- (void)_resizeWhileHidingContentWithUpdates:(void (^)(void))updateBlock
{
    RELEASE_LOG_IF_ALLOWED("%p (pageProxyID=%llu) -[WKWebView _resizeWhileHidingContentWithUpdates:]", self, _page->identifier().toUInt64());

    [self _beginAnimatedResizeWithUpdates:updateBlock];
    if (_dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::ResizingWithAnimation) {
        [_contentView setHidden:YES];
        _dynamicViewportUpdateMode = WebKit::DynamicViewportUpdateMode::ResizingWithDocumentHidden;
        
        // _resizeWhileHidingContentWithUpdates is used by itself; the client will
        // not call endAnimatedResize, so we can't wait for it.
        _waitingForEndAnimatedResize = NO;
    }
}

- (void)_setSuppressSoftwareKeyboard:(BOOL)suppressSoftwareKeyboard
{
    [super _setSuppressSoftwareKeyboard:suppressSoftwareKeyboard];
    [_contentView _setSuppressSoftwareKeyboard:suppressSoftwareKeyboard];
}

- (void)_snapshotRect:(CGRect)rectInViewCoordinates intoImageOfWidth:(CGFloat)imageWidth completionHandler:(void(^)(CGImageRef))completionHandler
{
    if (_dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing) {
        // Defer snapshotting until after the current resize completes.
        void (^copiedCompletionHandler)(CGImageRef) = [completionHandler copy];
        RetainPtr<WKWebView> retainedSelf = self;
        _callbacksDeferredDuringResize.append([retainedSelf, rectInViewCoordinates, imageWidth, copiedCompletionHandler] {
            [retainedSelf _snapshotRect:rectInViewCoordinates intoImageOfWidth:imageWidth completionHandler:copiedCompletionHandler];
            [copiedCompletionHandler release];
        });
        return;
    }

    CGRect snapshotRectInContentCoordinates = [self convertRect:rectInViewCoordinates toView:self._currentContentView];
    CGFloat imageScale = imageWidth / snapshotRectInContentCoordinates.size.width;
    CGFloat imageHeight = imageScale * snapshotRectInContentCoordinates.size.height;
    CGSize imageSize = CGSizeMake(imageWidth, imageHeight);

    if ([[_customContentView class] web_requiresCustomSnapshotting]) {
        [_customContentView web_snapshotRectInContentViewCoordinates:snapshotRectInContentCoordinates snapshotWidth:imageWidth completionHandler:completionHandler];
        return;
    }

#if HAVE(CORE_ANIMATION_RENDER_SERVER)
    // If we are parented and not hidden, and thus won't incur a significant penalty from paging in tiles, snapshot the view hierarchy directly.
    NSString *displayName = self.window.screen.displayConfiguration.name;
    if (displayName && !self.window.hidden) {
        auto surface = WebCore::IOSurface::create(WebCore::expandedIntSize(WebCore::FloatSize(imageSize)), WebCore::sRGBColorSpaceRef());
        if (!surface) {
            completionHandler(nullptr);
            return;
        }
        CGFloat imageScaleInViewCoordinates = imageWidth / rectInViewCoordinates.size.width;
        CATransform3D transform = CATransform3DMakeScale(imageScaleInViewCoordinates, imageScaleInViewCoordinates, 1);
        transform = CATransform3DTranslate(transform, -rectInViewCoordinates.origin.x, -rectInViewCoordinates.origin.y, 0);
        CARenderServerRenderDisplayLayerWithTransformAndTimeOffset(MACH_PORT_NULL, (CFStringRef)displayName, self.layer.context.contextId, reinterpret_cast<uint64_t>(self.layer), surface->surface(), 0, 0, &transform, 0);
        completionHandler(WebCore::IOSurface::sinkIntoImage(WTFMove(surface)).get());
        return;
    }
#endif

    if (_customContentView) {
        ASSERT(![[_customContentView class] web_requiresCustomSnapshotting]);
        UIGraphicsBeginImageContextWithOptions(imageSize, YES, 1);

        UIView *customContentView = _customContentView.get();
        [customContentView.backgroundColor set];
        UIRectFill(CGRectMake(0, 0, imageWidth, imageHeight));

        CGContextRef context = UIGraphicsGetCurrentContext();
        CGContextTranslateCTM(context, -snapshotRectInContentCoordinates.origin.x * imageScale, -snapshotRectInContentCoordinates.origin.y * imageScale);
        CGContextScaleCTM(context, imageScale, imageScale);
        [customContentView.layer renderInContext:context];

        completionHandler([UIGraphicsGetImageFromCurrentImageContext() CGImage]);

        UIGraphicsEndImageContext();
        return;
    }

    void(^copiedCompletionHandler)(CGImageRef) = [completionHandler copy];
    _page->takeSnapshot(WebCore::enclosingIntRect(snapshotRectInContentCoordinates), WebCore::expandedIntSize(WebCore::FloatSize(imageSize)), WebKit::SnapshotOptionsExcludeDeviceScaleFactor, [=](const WebKit::ShareableBitmap::Handle& imageHandle, WebKit::CallbackBase::Error) {
        if (imageHandle.isNull()) {
            copiedCompletionHandler(nullptr);
            [copiedCompletionHandler release];
            return;
        }

        auto bitmap = WebKit::ShareableBitmap::create(imageHandle, WebKit::SharedMemory::Protection::ReadOnly);

        if (!bitmap) {
            copiedCompletionHandler(nullptr);
            [copiedCompletionHandler release];
            return;
        }

        RetainPtr<CGImageRef> cgImage;
        cgImage = bitmap->makeCGImage();
        copiedCompletionHandler(cgImage.get());
        [copiedCompletionHandler release];
    });
}

- (void)_overrideLayoutParametersWithMinimumLayoutSize:(CGSize)minimumLayoutSize maximumUnobscuredSizeOverride:(CGSize)maximumUnobscuredSizeOverride
{
    LOG_WITH_STREAM(VisibleRects, stream << "-[WKWebView " << _page->identifier() << " _overrideLayoutParametersWithMinimumLayoutSize:" << WebCore::FloatSize(minimumLayoutSize) << " maximumUnobscuredSizeOverride:" << WebCore::FloatSize(maximumUnobscuredSizeOverride) << "]");

    [self _setViewLayoutSizeOverride:minimumLayoutSize];
    [self _setMaximumUnobscuredSizeOverride:maximumUnobscuredSizeOverride];
}

- (void)_clearOverrideLayoutParameters
{
    _viewLayoutSizeOverride = WTF::nullopt;
    _maximumUnobscuredSizeOverride = WTF::nullopt;
}

static WTF::Optional<WebCore::ViewportArguments> viewportArgumentsFromDictionary(NSDictionary<NSString *, NSString *> *viewportArgumentPairs, bool viewportFitEnabled)
{
    if (!viewportArgumentPairs)
        return WTF::nullopt;

    WebCore::ViewportArguments viewportArguments(WebCore::ViewportArguments::ViewportMeta);

    [viewportArgumentPairs enumerateKeysAndObjectsUsingBlock:makeBlockPtr([&] (NSString *key, NSString *value, BOOL* stop) {
        if (![key isKindOfClass:[NSString class]] || ![value isKindOfClass:[NSString class]])
            [NSException raise:NSInvalidArgumentException format:@"-[WKWebView _overrideViewportWithArguments:]: Keys and values must all be NSStrings."];
        String keyString = key;
        String valueString = value;
        WebCore::setViewportFeature(viewportArguments, keyString, valueString, viewportFitEnabled, [] (WebCore::ViewportErrorCode, const String& errorMessage) {
            NSLog(@"-[WKWebView _overrideViewportWithArguments:]: Error parsing viewport argument: %s", errorMessage.utf8().data());
        });
    }).get()];

    return viewportArguments;
}

- (void)_overrideViewportWithArguments:(NSDictionary<NSString *, NSString *> *)arguments
{
    if (!_page)
        return;

    _page->setOverrideViewportArguments(viewportArgumentsFromDictionary(arguments, _page->preferences().viewportFitEnabled()));
}

- (UIView *)_viewForFindUI
{
    return [self viewForZoomingInScrollView:[self scrollView]];
}

- (void)_setOverlaidAccessoryViewsInset:(CGSize)inset
{
    [_customContentView web_setOverlaidAccessoryViewsInset:inset];
}

- (void (^)(void))_retainActiveFocusedState
{
    ++_activeFocusedStateRetainCount;

    // FIXME: Use something like CompletionHandlerCallChecker to ensure that the returned block is called before it's released.
    return [[[self] {
        --_activeFocusedStateRetainCount;
    } copy] autorelease];
}

- (void)_becomeFirstResponderWithSelectionMovingForward:(BOOL)selectingForward completionHandler:(void (^)(BOOL didBecomeFirstResponder))completionHandler
{
    typeof(completionHandler) completionHandlerCopy = nil;
    if (completionHandler)
        completionHandlerCopy = Block_copy(completionHandler);

    [_contentView _becomeFirstResponderWithSelectionMovingForward:selectingForward completionHandler:[completionHandlerCopy](BOOL didBecomeFirstResponder) {
        if (!completionHandlerCopy)
            return;

        completionHandlerCopy(didBecomeFirstResponder);
        Block_release(completionHandlerCopy);
    }];
}

- (id)_snapshotLayerContentsForBackForwardListItem:(WKBackForwardListItem *)item
{
    if (_page->backForwardList().currentItem() == &item._item)
        _page->recordNavigationSnapshot(*_page->backForwardList().currentItem());

    if (auto* viewSnapshot = item._item.snapshot())
        return viewSnapshot->asLayerContents();

    return nil;
}

- (NSArray *)_dataDetectionResults
{
#if ENABLE(DATA_DETECTION)
    return [_contentView _dataDetectionResults];
#else
    return nil;
#endif
}

- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text completionHandler:(void (^)(NSArray<NSValue *> *rects))completionHandler
{
    [_contentView _accessibilityRetrieveRectsAtSelectionOffset:offset withText:text completionHandler:[capturedCompletionHandler = makeBlockPtr(completionHandler)] (const Vector<WebCore::SelectionRect>& selectionRects) {
        if (!capturedCompletionHandler)
            return;
        capturedCompletionHandler(createNSArray(selectionRects, [] (auto& rect) {
            return [NSValue valueWithCGRect:rect.rect()];
        }).get());
    }];
}

- (void)_accessibilityStoreSelection
{
    [_contentView _accessibilityStoreSelection];
}

- (void)_accessibilityClearSelection
{
    [_contentView _accessibilityClearSelection];
}

- (void)_accessibilityRetrieveSpeakSelectionContent
{
    [_contentView accessibilityRetrieveSpeakSelectionContent];
}

// This method is for subclasses to override.
// Currently it's only in TestRunnerWKWebView.
- (void)_accessibilityDidGetSpeakSelectionContent:(NSString *)content
{
}

- (UIView *)_fullScreenPlaceholderView
{
#if ENABLE(FULLSCREEN_API)
    if ([_fullScreenWindowController isFullScreen])
        return [_fullScreenWindowController webViewPlaceholder];
#endif // ENABLE(FULLSCREEN_API)
    return nil;
}

- (void)_grantAccessToAssetServices
{
#if PLATFORM(IOS)
    if (_page)
        _page->grantAccessToAssetServices();
#endif
}

- (void)_revokeAccessToAssetServices
{
#if PLATFORM(IOS)
    if (_page)
        _page->revokeAccessToAssetServices();
#endif
}

- (void)_willOpenAppLink
{
    if (_page)
        _page->willOpenAppLink();
}

@end // WKWebView (WKPrivateIOS)

#if ENABLE(FULLSCREEN_API)

@implementation WKWebView (FullScreenAPI_Private)

- (BOOL)hasFullScreenWindowController
{
    return !!_fullScreenWindowController;
}

- (void)closeFullScreenWindowController
{
    if (!_fullScreenWindowController)
        return;

    [_fullScreenWindowController close];
    _fullScreenWindowController = nullptr;
}

@end

@implementation WKWebView (FullScreenAPI_Internal)

- (WKFullScreenWindowController *)fullScreenWindowController
{
    if (!_fullScreenWindowController)
        _fullScreenWindowController = adoptNS([[WKFullScreenWindowController alloc] initWithWebView:self]);

    return _fullScreenWindowController.get();
}

@end

#endif // ENABLE(FULLSCREEN_API)

@implementation WKWebView (_WKWebViewPrintFormatter)

- (Class)_printFormatterClass
{
    return [_WKWebViewPrintFormatter class];
}

- (id <_WKWebViewPrintProvider>)_printProvider
{
    id printProvider = _customContentView ? _customContentView.get() : _contentView.get();
    if ([printProvider conformsToProtocol:@protocol(_WKWebViewPrintProvider)])
        return printProvider;
    return nil;
}

@end

#endif // PLATFORM(IOS_FAMILY)
