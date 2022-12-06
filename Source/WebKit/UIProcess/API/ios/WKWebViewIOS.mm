/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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

#import "APIFindClient.h"
#import "FrontBoardServicesSPI.h"
#import "NativeWebWheelEvent.h"
#import "NavigationState.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeScrollingPerformanceData.h"
#import "RemoteLayerTreeViews.h"
#import "RemoteScrollingCoordinatorProxyIOS.h"
#import "ScrollingTreeScrollingNodeDelegateIOS.h"
#import "TapHandlingResult.h"
#import "UIKitSPI.h"
#import "VideoFullscreenManagerProxy.h"
#import "ViewGestureController.h"
#import "WKBackForwardListItemInternal.h"
#import "WKContentViewInteraction.h"
#import "WKPasswordView.h"
#import "WKProcessPoolPrivate.h"
#import "WKSafeBrowsingWarning.h"
#import "WKScrollView.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewContentProvider.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WKWebViewPrivate.h"
#import "WKWebViewPrivateForTestingIOS.h"
#import "WebBackForwardList.h"
#import "WebIOSEventFactory.h"
#import "WebPage.h"
#import "WebPageProxy.h"
#import "_WKActivatedElementInfoInternal.h"
#import <WebCore/ColorCocoa.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurfacePool.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Box.h>
#import <wtf/FixedVector.h>
#import <wtf/SystemTracing.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(DATA_DETECTION)
#import "WKDataDetectorTypesInternal.h"
#endif

#if ENABLE(LOCKDOWN_MODE_API)
#import "_WKSystemPreferencesInternal.h"
#import <WebCore/LocalizedStrings.h>
#import <wtf/spi/cf/CFBundleSPI.h>
#endif

#if HAVE(UI_EVENT_ATTRIBUTION)
#import <UIKit/UIEventAttribution.h>
#endif

#define FORWARD_ACTION_TO_WKCONTENTVIEW(_action) \
- (void)_action:(id)sender \
{ \
    if (self.usesStandardContentView) \
        [_contentView _action ## ForWebView:sender]; \
}

#define WKWEBVIEW_RELEASE_LOG(...) RELEASE_LOG(ViewState, __VA_ARGS__)

static const Seconds delayBeforeNoVisibleContentsRectsLogging = 1_s;
static const Seconds delayBeforeNoCommitsLogging = 5_s;
static const unsigned highlightMargin = 5;

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

@implementation WKWebView (WKViewInternalIOS)

- (void)setFrame:(CGRect)frame
{
    bool sizeChanged = !CGSizeEqualToSize(self.frame.size, frame.size);
    if (sizeChanged)
        [self _frameOrBoundsWillChange];

    [super setFrame:frame];

    if (sizeChanged) {
        [self _frameOrBoundsMayHaveChanged];

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
        [self _acquireResizeAssertionForReason:@"-[WKWebView setFrame:]"];
#endif
    }
}

- (void)setBounds:(CGRect)bounds
{
    bool sizeChanged = !CGSizeEqualToSize(self.bounds.size, bounds.size);
    if (sizeChanged)
        [self _frameOrBoundsWillChange];

    [super setBounds:bounds];
    [_customContentFixedOverlayView setFrame:self.bounds];

    if (sizeChanged) {
        [self _frameOrBoundsMayHaveChanged];

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
        [self _acquireResizeAssertionForReason:@"-[WKWebView setBounds:]"];
#endif
    }
}

- (void)layoutSubviews
{
    [_safeBrowsingWarning setFrame:self.bounds];
    [super layoutSubviews];
    [self _frameOrBoundsMayHaveChanged];
}

#pragma mark - iOS implementation methods

- (void)_setupScrollAndContentViews
{
    CGRect bounds = self.bounds;
    _scrollView = adoptNS([[WKScrollView alloc] initWithFrame:bounds]);
    [_scrollView setInternalDelegate:self];
    [_scrollView setBouncesZoom:YES];

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
    [_scrollView _setAllowsAsyncScrollEvent:YES];
#endif

    if ([_scrollView respondsToSelector:@selector(_setAvoidsJumpOnInterruptedBounce:)]) {
        [_scrollView setTracksImmediatelyWhileDecelerating:NO];
        [_scrollView _setAvoidsJumpOnInterruptedBounce:YES];
    }

    _scrollViewDefaultAllowedTouchTypes = [_scrollView panGestureRecognizer].allowedTouchTypes;

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

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    [center addObserver:self selector:@selector(_enhancedWindowingToggled:) name:_UIWindowSceneEnhancedWindowingModeChanged object:nil];
#endif
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

- (void)_dynamicUserInterfaceTraitDidChange
{
    if (!_page)
        return;
    _page->effectiveAppearanceDidChange();
    [self _updateScrollViewBackground];
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

- (NSUndoManager *)undoManager
{
    if (self._currentContentView == _contentView)
        return [_contentView undoManagerForWebView];

    return [super undoManager];
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

#if HAVE(UIFINDINTERACTION)
    if (action == @selector(find:) || action == @selector(findNext:) || action == @selector(findPrevious:))
        return _findInteractionEnabled;

    if (action == @selector(findAndReplace:))
        return _findInteractionEnabled && self.supportsTextReplacement;
#endif

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

#if HAVE(UIFINDINTERACTION)

- (void)find:(id)sender
{
    [_findInteraction presentFindNavigatorShowingReplace:NO];
}

- (void)findNext:(id)sender
{
    [_findInteraction findNext];
}

- (void)findPrevious:(id)sender
{
    [_findInteraction findPrevious];
}

- (void)findAndReplace:(id)sender
{
    [_findInteraction presentFindNavigatorShowingReplace:YES];
}

#endif

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

- (WKScrollView *)_wkScrollView
{
    return _scrollView.get();
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

        _perProcessState.scrollViewBackgroundColor = WebCore::Color();
        [_scrollView setContentOffset:[self _initialContentOffsetForScrollView]];
        [_scrollView panGestureRecognizer].allowedTouchTypes = _scrollViewDefaultAllowedTouchTypes.get();
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

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
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
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (void)_willInvokeUIScrollViewDelegateCallback
{
    _perProcessState.invokingUIScrollViewDelegateCallback = YES;
}

- (void)_didInvokeUIScrollViewDelegateCallback
{
    _perProcessState.invokingUIScrollViewDelegateCallback = NO;
    if (_perProcessState.didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback) {
        _perProcessState.didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback = NO;
        [self _scheduleVisibleContentRectUpdate];
    }
}

static CGFloat contentZoomScale(WKWebView *webView)
{
    CGFloat scale = webView._currentContentView.layer.affineTransform.a;
    ASSERT(scale == [webView->_scrollView zoomScale]);
    return scale;
}

enum class AllowPageBackgroundColorOverride : bool { No, Yes };

static WebCore::Color baseScrollViewBackgroundColor(WKWebView *webView, AllowPageBackgroundColorOverride allowPageBackgroundColorOverride)
{
    if (webView->_customContentView)
        return WebCore::roundAndClampToSRGBALossy([webView->_customContentView backgroundColor].CGColor);

    if (webView->_gestureController) {
        WebCore::Color color = webView->_gestureController->backgroundColorForCurrentSnapshot();
        if (color.isValid())
            return color;
    }

    if (!webView->_page)
        return { };

    return allowPageBackgroundColorOverride == AllowPageBackgroundColorOverride::Yes ? webView->_page->underPageBackgroundColor() : webView->_page->pageExtendedBackgroundColor();
}

static WebCore::Color scrollViewBackgroundColor(WKWebView *webView, AllowPageBackgroundColorOverride allowPageBackgroundColorOverride)
{
    if (!webView.opaque)
        return WebCore::Color::transparentBlack;

    WebCore::Color color;
    auto computeScrollViewBackgroundColor = [&]() {
        color = baseScrollViewBackgroundColor(webView, allowPageBackgroundColorOverride);

        if (!color.isValid() && webView->_contentView)
            color = WebCore::roundAndClampToSRGBALossy([webView->_contentView backgroundColor].CGColor);

        if (!color.isValid()) {
#if HAVE(OS_DARK_MODE_SUPPORT)
            color = WebCore::roundAndClampToSRGBALossy(UIColor.systemBackgroundColor.CGColor);
#else
            color = WebCore::Color::white;
#endif
        }
    };

#if HAVE(OS_DARK_MODE_SUPPORT)
    [webView.traitCollection performAsCurrentTraitCollection:computeScrollViewBackgroundColor];
#else
    computeScrollViewBackgroundColor();
#endif

    return color;
}

- (void)_resetCachedScrollViewBackgroundColor
{
    _perProcessState.scrollViewBackgroundColor = WebCore::Color();
}

- (void)_updateScrollViewBackground
{
    auto newScrollViewBackgroundColor = scrollViewBackgroundColor(self, AllowPageBackgroundColorOverride::Yes);
    if (_perProcessState.scrollViewBackgroundColor != newScrollViewBackgroundColor) {
        [_scrollView _setBackgroundColorInternal:cocoaColor(newScrollViewBackgroundColor).get()];
        _perProcessState.scrollViewBackgroundColor = newScrollViewBackgroundColor;
    }

    [self _updateScrollViewIndicatorStyle];
}

- (void)_updateScrollViewIndicatorStyle
{
    // Update the indicator style based on the lightness/darkness of the background color.
    auto newPageBackgroundColor = scrollViewBackgroundColor(self, AllowPageBackgroundColorOverride::No);
    if (newPageBackgroundColor.lightness() <= .5f && newPageBackgroundColor.isVisible())
        [_scrollView _setIndicatorStyleInternal:UIScrollViewIndicatorStyleWhite];
    else
        [_scrollView _setIndicatorStyleInternal:UIScrollViewIndicatorStyleBlack];
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

    return UIEdgeInsetsAdd(UIEdgeInsetsZero, self._scrollViewSystemContentInset, self._effectiveObscuredInsetEdgesAffectedBySafeArea);
}

- (UIEdgeInsets)_contentInsetsFromSystemMinimumLayoutMargins
{
    if (auto controller = [UIViewController _viewControllerForFullScreenPresentationFromView:self]) {
        auto margins = controller.systemMinimumLayoutMargins;
        auto insets = UIEdgeInsetsMake(margins.top, margins.leading, margins.bottom, margins.trailing);
        if (_page && _page->userInterfaceLayoutDirection() == WebCore::UserInterfaceLayoutDirection::RTL)
            std::swap(insets.left, insets.right);

        if (auto view = controller.viewIfLoaded) {
            auto adjustInsetEdge = [](CGFloat& insetEdge, CGFloat distanceFromEdge) {
                insetEdge -= std::max<CGFloat>(0, distanceFromEdge);
                insetEdge = std::max<CGFloat>(0, insetEdge);
            };

            auto viewBounds = view.bounds;
            auto webViewBoundsInView = [self convertRect:self.bounds toView:view];
            adjustInsetEdge(insets.top, CGRectGetMinY(webViewBoundsInView) - CGRectGetMinY(viewBounds));
            adjustInsetEdge(insets.left, CGRectGetMinX(webViewBoundsInView) - CGRectGetMinX(viewBounds));
            adjustInsetEdge(insets.bottom, CGRectGetMaxY(viewBounds) - CGRectGetMaxY(webViewBoundsInView));
            adjustInsetEdge(insets.right, CGRectGetMaxX(viewBounds) - CGRectGetMaxX(webViewBoundsInView));
        }
        return insets;
    }
    return UIEdgeInsetsZero;
}

- (UIEdgeInsets)_computedObscuredInset
{
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ScrollViewContentInsetsAreNotObscuringInsets)) {
        // For binary compability with third party apps, treat scroll view content insets as obscuring insets when the app is compiled
        // against a WebKit version without the fix in r229641.
        return [self _computedContentInset];
    }

    if (_haveSetObscuredInsets)
        return _obscuredInsets;

    if (self._safeAreaShouldAffectObscuredInsets)
        return UIEdgeInsetsAdd(UIEdgeInsetsZero, self._scrollViewSystemContentInset, self._effectiveObscuredInsetEdgesAffectedBySafeArea);

    return UIEdgeInsetsZero;
}

- (UIEdgeInsets)_computedContentInset
{
    if (_haveSetObscuredInsets)
        return _obscuredInsets;

    UIEdgeInsets insets = [_scrollView contentInset];
    if (self._safeAreaShouldAffectObscuredInsets) {
#if PLATFORM(WATCHOS)
        // On watchOS, PepperUICore swizzles -[UIScrollView contentInset], such that it includes -_contentScrollInset as well.
        // To avoid double-counting -_contentScrollInset when determining the total content inset, we only apply safe area insets here.
        auto additionalScrollViewContentInset = self.safeAreaInsets;
#else
        auto additionalScrollViewContentInset = self._scrollViewSystemContentInset;
#endif
        insets = UIEdgeInsetsAdd(insets, additionalScrollViewContentInset, self._effectiveObscuredInsetEdgesAffectedBySafeArea);
    }

    return insets;
}

- (UIEdgeInsets)_computedUnobscuredSafeAreaInset
{
    if (_haveSetUnobscuredSafeAreaInsets)
        return _unobscuredSafeAreaInsets;

    if (!self._safeAreaShouldAffectObscuredInsets) {
        auto safeAreaInsets = self.safeAreaInsets;
#if PLATFORM(WATCHOS)
        safeAreaInsets = UIEdgeInsetsAdd(safeAreaInsets, self._contentInsetsFromSystemMinimumLayoutMargins, self._effectiveObscuredInsetEdgesAffectedBySafeArea);
#endif
        return safeAreaInsets;
    }

    return UIEdgeInsetsZero;
}

- (void)_processWillSwapOrDidExit
{
    // FIXME: Which ones of these need to be done in the process swap case and which ones in the exit case?
    [self _hidePasswordView];
    [self _cancelAnimatedResize];
    [self _destroyResizeAnimationView];
    [_contentView setHidden:NO];

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    [self _invalidateResizeAssertions];
#endif

    if (_gestureController)
        _gestureController->disconnectFromProcess();

    _perProcessState = { };
}

- (void)_processWillSwap
{
    WKWEBVIEW_RELEASE_LOG("%p -[WKWebView _processWillSwap]", self);
    [self _processWillSwapOrDidExit];
}

- (void)_processDidExit
{
    WKWEBVIEW_RELEASE_LOG("%p -[WKWebView _processDidExit]", self);

    [self _processWillSwapOrDidExit];

    [_contentView setFrame:self.bounds];
    [_scrollView _setBackgroundColorInternal:[_contentView backgroundColor]];
    [_scrollView setContentOffset:[self _initialContentOffsetForScrollView]];
    [_scrollView setZoomScale:1];
}

- (void)_didRelaunchProcess
{
    WKWEBVIEW_RELEASE_LOG("%p -[WKWebView _didRelaunchProcess] (pid=%d)", self, _page->processIdentifier());
    _perProcessState.hasScheduledVisibleRectUpdate = NO;
    _viewStabilityWhenVisibleContentRectUpdateScheduled = { };
    if (_gestureController)
        _gestureController->connectToProcess();
}

- (void)_didCommitLoadForMainFrame
{
    _perProcessState.firstPaintAfterCommitLoadTransactionID = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).nextLayerTreeTransactionID();

    _perProcessState.hasCommittedLoadForMainFrame = YES;
    _perProcessState.needsResetViewStateAfterCommitLoadForMainFrame = YES;

    if (![self _scrollViewIsRubberBandingForRefreshControl])
        [_scrollView _stopScrollingAndZoomingAnimations];

#if HAVE(UIFINDINTERACTION)
    if (_findInteractionEnabled)
        [_findInteraction dismissFindNavigator];
#endif
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
    if (updateID != _currentDynamicViewportSizeUpdateID)
        return;

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
    _perProcessState.waitingForCommitAfterAnimatedResize = NO;
    if (!_perProcessState.waitingForEndAnimatedResize)
        [self _didCompleteAnimatedResize];
}

- (void)_trackTransactionCommit:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    if (_perProcessState.didDeferUpdateVisibleContentRectsForUnstableScrollView) {
        WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _didCommitLayerTree:] - received a commit (%llu) while deferring visible content rect updates (dynamicViewportUpdateMode %d, needsResetViewStateAfterCommitLoadForMainFrame %d (wants commit %llu), sizeChangedSinceLastVisibleContentRectUpdate %d, [_scrollView isZoomBouncing] %d, currentlyAdjustingScrollViewInsetsForKeyboard %d)",
        self, _page->identifier().toUInt64(), layerTreeTransaction.transactionID().toUInt64(), _perProcessState.dynamicViewportUpdateMode, _perProcessState.needsResetViewStateAfterCommitLoadForMainFrame, _perProcessState.firstPaintAfterCommitLoadTransactionID.toUInt64(), [_contentView sizeChangedSinceLastVisibleContentRectUpdate], [_scrollView isZoomBouncing], _perProcessState.currentlyAdjustingScrollViewInsetsForKeyboard);
    }

    if (_timeOfFirstVisibleContentRectUpdateWithPendingCommit) {
        auto timeSinceFirstRequestWithPendingCommit = MonotonicTime::now() - *_timeOfFirstVisibleContentRectUpdateWithPendingCommit;
        if (timeSinceFirstRequestWithPendingCommit > delayBeforeNoCommitsLogging)
            WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _didCommitLayerTree:] - finally received commit %.2fs after visible content rect update request; transactionID %llu", self, _page->identifier().toUInt64(), timeSinceFirstRequestWithPendingCommit.value(), layerTreeTransaction.transactionID().toUInt64());
        _timeOfFirstVisibleContentRectUpdateWithPendingCommit = std::nullopt;
    }
}

- (void)_updateScrollViewForTransaction:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    CGSize newContentSize = roundScrollViewContentSize(*_page, [_contentView frame].size);
    [_scrollView _setContentSizePreservingContentOffsetDuringRubberband:newContentSize];

    [_scrollView setMinimumZoomScale:layerTreeTransaction.minimumScaleFactor()];
    [_scrollView setMaximumZoomScale:layerTreeTransaction.maximumScaleFactor()];
    [_scrollView _setZoomEnabledInternal:layerTreeTransaction.allowsUserScaling()];

    auto horizontalOverscrollBehavior = WebCore::OverscrollBehavior::Auto;
    auto verticalOverscrollBehavior = WebCore::OverscrollBehavior::Auto;

    if (auto rootNode = _page->scrollingCoordinatorProxy()->rootNode()) {
        horizontalOverscrollBehavior = rootNode->horizontalOverscrollBehavior();
        verticalOverscrollBehavior = rootNode->verticalOverscrollBehavior();
    }
    
    WebKit::ScrollingTreeScrollingNodeDelegateIOS::updateScrollViewForOverscrollBehavior(_scrollView.get(), horizontalOverscrollBehavior, verticalOverscrollBehavior, WebKit::ScrollingTreeScrollingNodeDelegateIOS::AllowOverscrollToPreventScrollPropagation::No);

    bool hasDockedInputView = !CGRectIsEmpty(_inputViewBoundsInWindow);
    bool isZoomed = layerTreeTransaction.pageScaleFactor() > layerTreeTransaction.initialScaleFactor();

    bool scrollingNeededToRevealUI = false;
    if (_maximumUnobscuredSizeOverride) {
        auto unobscuredContentRect = _page->unobscuredContentRect();
        auto maxUnobscuredSize = _page->maximumUnobscuredSize();
        auto minUnobscuredSize = _page->minimumUnobscuredSize();
        scrollingNeededToRevealUI = minUnobscuredSize != maxUnobscuredSize && maxUnobscuredSize == unobscuredContentRect.size();
    }

    bool scrollingEnabled = _page->scrollingCoordinatorProxy()->hasScrollableOrZoomedMainFrame() || hasDockedInputView || isZoomed || scrollingNeededToRevealUI;

    // FIXME: <rdar://99001670> Get the default list of allowed touch types from UIKit instead of caching the returned value.
    [_scrollView panGestureRecognizer].allowedTouchTypes = scrollingEnabled ? _scrollViewDefaultAllowedTouchTypes.get() : @[ ];
    [_scrollView _setScrollEnabledInternal:YES];

    if (!layerTreeTransaction.scaleWasSetByUIProcess() && ![_scrollView isZooming] && ![_scrollView isZoomBouncing] && ![_scrollView _isAnimatingZoom] && !WebKit::scalesAreEssentiallyEqual([_scrollView zoomScale], layerTreeTransaction.pageScaleFactor())) {
        LOG_WITH_STREAM(VisibleRects, stream << " updating scroll view with pageScaleFactor " << layerTreeTransaction.pageScaleFactor());
        [_scrollView setZoomScale:layerTreeTransaction.pageScaleFactor()];
    }
}

- (BOOL)_restoreScrollAndZoomStateForTransaction:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    if (!_perProcessState.firstTransactionIDAfterPageRestore || layerTreeTransaction.transactionID() < _perProcessState.firstTransactionIDAfterPageRestore.value())
        return NO;

    _perProcessState.firstTransactionIDAfterPageRestore = std::nullopt;

    BOOL needUpdateVisibleContentRects = NO;

    if (_perProcessState.scrollOffsetToRestore && ![self _scrollViewIsRubberBandingForRefreshControl]) {
        WebCore::FloatPoint scaledScrollOffset = _perProcessState.scrollOffsetToRestore.value();
        _perProcessState.scrollOffsetToRestore = std::nullopt;

        if (WebKit::scalesAreEssentiallyEqual(contentZoomScale(self), _scaleToRestore)) {
            scaledScrollOffset.scale(_scaleToRestore);
            WebCore::FloatPoint contentOffsetInScrollViewCoordinates = scaledScrollOffset - WebCore::FloatSize(_obscuredInsetsWhenSaved.left(), _obscuredInsetsWhenSaved.top());

            changeContentOffsetBoundedInValidRange(_scrollView.get(), contentOffsetInScrollViewCoordinates);
            _perProcessState.commitDidRestoreScrollPosition = YES;
        }

        needUpdateVisibleContentRects = YES;
    }

    if (_perProcessState.unobscuredCenterToRestore && ![self _scrollViewIsRubberBandingForRefreshControl]) {
        WebCore::FloatPoint unobscuredCenterToRestore = _perProcessState.unobscuredCenterToRestore.value();
        _perProcessState.unobscuredCenterToRestore = std::nullopt;

        if (WebKit::scalesAreEssentiallyEqual(contentZoomScale(self), _scaleToRestore)) {
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

    _perProcessState.lastTransactionID = layerTreeTransaction.transactionID();

    if (![self usesStandardContentView])
        return;

    LOG_WITH_STREAM(VisibleRects, stream << "-[WKWebView " << _page->identifier() << " _didCommitLayerTree:] transactionID " << layerTreeTransaction.transactionID() << " dynamicViewportUpdateMode " << (int)_perProcessState.dynamicViewportUpdateMode);

    bool needUpdateVisibleContentRects = _page->updateLayoutViewportParameters(layerTreeTransaction);

    if (_perProcessState.dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing) {
        [self _didCommitLayerTreeDuringAnimatedResize:layerTreeTransaction];
        return;
    }

    if (_resizeAnimationView)
        WKWEBVIEW_RELEASE_LOG("%p -[WKWebView _didCommitLayerTree:] - dynamicViewportUpdateMode is NotResizing, but still have a live resizeAnimationView (unpaired begin/endAnimatedResize?)", self);

    [self _updateScrollViewForTransaction:layerTreeTransaction];

    _perProcessState.viewportMetaTagWidth = layerTreeTransaction.viewportMetaTagWidth();
    _perProcessState.viewportMetaTagWidthWasExplicit = layerTreeTransaction.viewportMetaTagWidthWasExplicit();
    _perProcessState.viewportMetaTagCameFromImageDocument = layerTreeTransaction.viewportMetaTagCameFromImageDocument();
    _perProcessState.initialScaleFactor = layerTreeTransaction.initialScaleFactor();

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

    if (_perProcessState.needsResetViewStateAfterCommitLoadForMainFrame && layerTreeTransaction.transactionID() >= _perProcessState.firstPaintAfterCommitLoadTransactionID) {
        _perProcessState.needsResetViewStateAfterCommitLoadForMainFrame = NO;
        if (![self _scrollViewIsRubberBandingForRefreshControl])
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

    if (_perProcessState.pendingFindLayerID) {
        CALayer *layer = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).remoteLayerTreeHost().layerForID(_perProcessState.pendingFindLayerID);
        if (layer.superlayer) {
            _perProcessState.committedFindLayerID = std::exchange(_perProcessState.pendingFindLayerID, 0);
            _page->findClient().didAddLayerForFindOverlay(_page.get(), layer);
        }
    }

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    [self _updateOverlayRegions:layerTreeTransaction.changedLayerProperties() destroyedLayers:layerTreeTransaction.destroyedLayers()];
#endif
}

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
static void addOverlayEventRegions(WebCore::GraphicsLayer::PlatformLayerID layerID, const WebKit::RemoteLayerTreeTransaction::LayerPropertiesMap& changedLayerPropertiesMap, HashSet<WebCore::GraphicsLayer::PlatformLayerID>& overlayRegionIDs, const WebKit::RemoteLayerTreeHost& layerTreeHost)
{
    using WebKit::RemoteLayerTreeTransaction;
    const auto& it = changedLayerPropertiesMap.find(layerID);
    if (it == changedLayerPropertiesMap.end())
        return;

    const auto& layerProperties = *it->value;
    CGRect rect = layerProperties.eventRegion.region().bounds();
    if ((layerProperties.changedProperties & RemoteLayerTreeTransaction::EventRegionChanged) && !CGRectIsEmpty(rect))
        overlayRegionIDs.add(layerID);

    for (auto childLayerID : layerProperties.children)
        addOverlayEventRegions(childLayerID, changedLayerPropertiesMap, overlayRegionIDs, layerTreeHost);
}

- (void)_updateOverlayRegions:(const WebKit::RemoteLayerTreeTransaction::LayerPropertiesMap&)changedLayerPropertiesMap destroyedLayers:(const Vector<WebCore::GraphicsLayer::PlatformLayerID>&)destroyedLayers
{
    BOOL skipRecursiveRegionUpdate = !changedLayerPropertiesMap.size() && !destroyedLayers.size();

    auto& layerTreeProxy = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea());
    auto& layerTreeHost = layerTreeProxy.remoteLayerTreeHost();
    auto* scrollingCoordinatorProxy = downcast<WebKit::RemoteScrollingCoordinatorProxyIOS>(_page->scrollingCoordinatorProxy());
    if (!scrollingCoordinatorProxy)
        return;

    scrollingCoordinatorProxy->removeFixedScrollingNodeLayerIDs(destroyedLayers);
    const auto& fixedIDs = scrollingCoordinatorProxy->fixedScrollingNodeLayerIDs();

    auto overlayRegionIDs = layerTreeHost.overlayRegionIDs();
    for (auto layerID : destroyedLayers)
        overlayRegionIDs.remove(layerID);

    if (!skipRecursiveRegionUpdate) {
        for (auto layerID : fixedIDs)
            addOverlayEventRegions(layerID, changedLayerPropertiesMap, overlayRegionIDs, layerTreeHost);
    }

    Vector<CGRect> overlayRegions;
    for (const auto layerID : overlayRegionIDs) {
        if (const auto* node = layerTreeHost.nodeForID(layerID))
            overlayRegions.append([node->uiView() convertRect:node->eventRegion().region().bounds() toView:self]);
    }

    if ([_scrollView _updateOverlayRegions:overlayRegions])
        layerTreeProxy.updateOverlayRegionIDs(overlayRegionIDs);
}
#endif // ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)

- (void)_layerTreeCommitComplete
{
    _perProcessState.commitDidRestoreScrollPosition = NO;
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

- (void)_restorePageScrollPosition:(std::optional<WebCore::FloatPoint>)scrollPosition scrollOrigin:(WebCore::FloatPoint)scrollOrigin previousObscuredInset:(WebCore::FloatBoxExtent)obscuredInsets scale:(double)scale
{
    if (self._shouldDeferGeometryUpdates) {
        // Defer scroll position restoration until after the current resize completes.
        RetainPtr<WKWebView> retainedSelf = self;
        _callbacksDeferredDuringResize.append([retainedSelf, scrollPosition, scrollOrigin, obscuredInsets, scale] {
            [retainedSelf _restorePageScrollPosition:scrollPosition scrollOrigin:scrollOrigin previousObscuredInset:obscuredInsets scale:scale];
        });
        return;
    }

    if (![self usesStandardContentView])
        return;

    _perProcessState.firstTransactionIDAfterPageRestore = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).nextLayerTreeTransactionID();
    if (scrollPosition)
        _perProcessState.scrollOffsetToRestore = WebCore::ScrollableArea::scrollOffsetFromPosition(WebCore::FloatPoint(scrollPosition.value()), WebCore::toFloatSize(scrollOrigin));
    else
        _perProcessState.scrollOffsetToRestore = std::nullopt;

    _obscuredInsetsWhenSaved = obscuredInsets;
    _scaleToRestore = scale;
}

- (void)_restorePageStateToUnobscuredCenter:(std::optional<WebCore::FloatPoint>)center scale:(double)scale
{
    if (self._shouldDeferGeometryUpdates) {
        // Defer scroll position restoration until after the current resize completes.
        RetainPtr<WKWebView> retainedSelf = self;
        _callbacksDeferredDuringResize.append([retainedSelf, center, scale] {
            [retainedSelf _restorePageStateToUnobscuredCenter:center scale:scale];
        });
        return;
    }

    if (![self usesStandardContentView])
        return;

    _perProcessState.firstTransactionIDAfterPageRestore = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).nextLayerTreeTransactionID();
    _perProcessState.unobscuredCenterToRestore = center;

    _scaleToRestore = scale;
}

- (RefPtr<WebKit::ViewSnapshot>)_takeViewSnapshot
{
#if HAVE(CORE_ANIMATION_RENDER_SERVER)
    float deviceScale = WebCore::screenScaleFactor();
    WebCore::FloatSize snapshotSize(self.bounds.size);
    snapshotSize.scale(deviceScale);

    if (snapshotSize.isEmpty())
        return nullptr;

    CATransform3D transform = CATransform3DMakeScale(deviceScale, deviceScale, 1);

#if HAVE(IOSURFACE_RGB10)
    WebCore::IOSurface::Format snapshotFormat = WebCore::screenSupportsExtendedColor() ? WebCore::IOSurface::Format::RGB10 : WebCore::IOSurface::Format::BGRA;
#else
    WebCore::IOSurface::Format snapshotFormat = WebCore::IOSurface::Format::BGRA;
#endif
    auto surface = WebCore::IOSurface::create(nullptr, WebCore::expandedIntSize(snapshotSize), WebCore::DestinationColorSpace::SRGB(), snapshotFormat);
    if (!surface)
        return nullptr;
    CARenderServerRenderLayerWithTransform(MACH_PORT_NULL, self.layer.context.contextId, reinterpret_cast<uint64_t>(self.layer), surface->surface(), 0, 0, &transform);

#if HAVE(IOSURFACE_ACCELERATOR)
    WebCore::IOSurface::Format compressedFormat = WebCore::IOSurface::Format::YUV422;
    if (WebCore::IOSurface::allowConversionFromFormatToFormat(snapshotFormat, compressedFormat)) {
        auto viewSnapshot = WebKit::ViewSnapshot::create(nullptr);
        WebCore::IOSurface::convertToFormat(nullptr, WTFMove(surface), WebCore::IOSurface::Format::YUV422, [viewSnapshot](std::unique_ptr<WebCore::IOSurface> convertedSurface) {
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

- (void)_scrollToContentScrollPosition:(WebCore::FloatPoint)scrollPosition scrollOrigin:(WebCore::IntPoint)scrollOrigin animated:(BOOL)animated
{
    if (_perProcessState.commitDidRestoreScrollPosition || self._shouldDeferGeometryUpdates)
        return;

    // Don't allow content to do programmatic scrolls for non-scrollable pages when zoomed.
    if (!_page->scrollingCoordinatorProxy()->hasScrollableMainFrame() && ([_scrollView zoomScale] > [_scrollView minimumZoomScale] || [_scrollView zoomScale] < [_scrollView minimumZoomScale])) {
        [self _scheduleForcedVisibleContentRectUpdate];
        return;
    }

    WebCore::FloatPoint contentOffset = WebCore::ScrollableArea::scrollOffsetFromPosition(scrollPosition, toFloatSize(scrollOrigin));

    WebCore::FloatPoint scaledOffset = contentOffset;
    CGFloat zoomScale = contentZoomScale(self);
    scaledOffset.scale(zoomScale);

    CGPoint contentOffsetInScrollViewCoordinates = [self _contentOffsetAdjustedForObscuredInset:scaledOffset];
    contentOffsetInScrollViewCoordinates = contentOffsetBoundedInValidRange(_scrollView.get(), contentOffsetInScrollViewCoordinates);

    [_scrollView _stopScrollingAndZoomingAnimations];

    CGPoint scrollViewContentOffset = [_scrollView contentOffset];

    if (!CGPointEqualToPoint(contentOffsetInScrollViewCoordinates, scrollViewContentOffset)) {
        if (WTF::areEssentiallyEqual<float>(scrollPosition.x(), 0) && scrollViewContentOffset.x < 0)
            contentOffsetInScrollViewCoordinates.x = scrollViewContentOffset.x;

        if (WTF::areEssentiallyEqual<float>(scrollPosition.y(), 0) && scrollViewContentOffset.y < 0)
            contentOffsetInScrollViewCoordinates.y = scrollViewContentOffset.y;

        [_scrollView setContentOffset:contentOffsetInScrollViewCoordinates animated:animated];
    } else {
        // If we haven't changed anything, there would not be any VisibleContentRect update sent to the content.
        // The WebProcess would keep the invalid contentOffset as its scroll position.
        // To synchronize the WebProcess with what is on screen, we send the VisibleContentRect again.
        _page->resendLastVisibleContentRects();
    }
}

- (float)_adjustScrollRectToAvoidHighlightOverlay:(WebCore::FloatRect)targetRect
{
#if ENABLE(APP_HIGHLIGHTS)
    WebCore::FloatRect overlayRect = [self convertRect:_page->appHighlightsOverlayRect() fromCoordinateSpace:self.window.screen.coordinateSpace];
    
    if (CGRectIsNull(overlayRect))
        return 0;
        
    overlayRect.expand(highlightMargin, highlightMargin);
    
    if (!targetRect.intersects(overlayRect))
        return 0;
    
    float topGap = overlayRect.y() - [self bounds].origin.y;
    float bottomGap = (self.bounds.size.height + self.bounds.origin.y) - overlayRect.maxY();
    
    float midScreen = self.center.y;

    if (topGap > bottomGap) {
        auto midGap = topGap / 2 + self.bounds.origin.y;
        auto diff = midScreen - midGap;
        return diff;
    }
    
    auto midGap = bottomGap / 2 + self.bounds.origin.y;
    auto diff = midGap - midScreen;
    return diff;
#else
    return 0;
#endif
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
    
    WebCore::FloatRect startRect = targetRect;
    WebCore::FloatRect convertedStartRect = [self convertRect:startRect fromView:self._currentContentView];
    convertedStartRect.move(-scrollViewOffsetDelta);

    float additionalOffset = [self _adjustScrollRectToAvoidHighlightOverlay:convertedStartRect];

    scrollViewOffsetDelta += WebCore::FloatSize(0, additionalOffset);
    
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
    ASSERT(_perProcessState.initialScaleFactor > 0);
    [self _zoomToPoint:origin atScale:_perProcessState.initialScaleFactor animated:animated];
}

- (BOOL)_selectionRectIsFullyVisibleAndNonEmpty
{
    auto rect = _page->selectionBoundingRectInRootViewCoordinates();
    return !rect.isEmpty() && CGRectContainsRect(self._contentRectForUserInteraction, rect);
}

- (void)_scrollToAndRevealSelectionIfNeeded
{
    if (![_scrollView isScrollEnabled])
        return;

    auto selectionRect = _page->selectionBoundingRectInRootViewCoordinates();
    constexpr CGFloat selectionRectPadding = 4;
    selectionRect.inflate(selectionRectPadding);
    selectionRect.intersect([_contentView bounds]);
    if (selectionRect.isEmpty())
        return;

    WebCore::FloatRect userInteractionContentRect = self._contentRectForUserInteraction;
    auto scrollDeltaInContentCoordinates = CGSizeZero;

    if (userInteractionContentRect.maxY() < selectionRect.maxY())
        scrollDeltaInContentCoordinates.height = selectionRect.maxY() - userInteractionContentRect.maxY();
    else if (userInteractionContentRect.y() > selectionRect.y())
        scrollDeltaInContentCoordinates.height = selectionRect.y() - userInteractionContentRect.y();

    if (userInteractionContentRect.maxX() < selectionRect.maxX())
        scrollDeltaInContentCoordinates.width = selectionRect.maxX() - userInteractionContentRect.maxX();
    else if (userInteractionContentRect.x() > selectionRect.x())
        scrollDeltaInContentCoordinates.width = selectionRect.x() - userInteractionContentRect.x();

    if (CGSizeEqualToSize(scrollDeltaInContentCoordinates, CGSizeZero))
        return;

    auto scale = contentZoomScale(self);
    auto newContentOffset = [_scrollView contentOffset];
    newContentOffset.x += scrollDeltaInContentCoordinates.width * scale;
    newContentOffset.y += scrollDeltaInContentCoordinates.height * scale;
    [_scrollView setContentOffset:newContentOffset animated:YES];
}

- (void)_zoomToFocusRect:(const WebCore::FloatRect&)focusedElementRectInDocumentCoordinates selectionRect:(const WebCore::FloatRect&)selectionRectInDocumentCoordinates
    fontSize:(float)fontSize minimumScale:(double)minimumScale maximumScale:(double)maximumScale allowScaling:(BOOL)allowScaling forceScroll:(BOOL)forceScroll
{
    LOG_WITH_STREAM(VisibleRects, stream << "_zoomToFocusRect:" << focusedElementRectInDocumentCoordinates << " selectionRect:" << selectionRectInDocumentCoordinates);

    const double minimumHeightToShowContentAboveKeyboard = 106;
    const CFTimeInterval formControlZoomAnimationDuration = 0.25;
    const double caretOffsetFromWindowEdge = 8;

    UIWindow *window = [_scrollView window];

    CGRect unobscuredScrollViewRectInWebViewCoordinates = UIEdgeInsetsInsetRect([self bounds], _obscuredInsets);
    CGRect visibleScrollViewBoundsInWebViewCoordinates = CGRectIntersection(unobscuredScrollViewRectInWebViewCoordinates, [window convertRect:window.bounds toView:self]);
    CGRect formAssistantFrameInWebViewCoordinates = [window convertRect:_inputViewBoundsInWindow toView:self];
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
    return _perProcessState.initialScaleFactor;
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
    [self _presentLockdownModeAlertIfNeeded];
#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    if (_page->hasRunningProcess() && self.window)
        _page->setIsWindowResizingEnabled(self._isWindowResizingEnabled);
    [self _invalidateResizeAssertions];
#endif
#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)
    [self _destroyEndLiveResizeObserver];
    [self _endLiveResize];
#endif
}

#if HAVE(UIKIT_RESIZABLE_WINDOWS)

- (BOOL)_isWindowResizingEnabled
{
    UIWindowScene *scene = self.window.windowScene;
    return [scene respondsToSelector:@selector(_enhancedWindowingEnabled)] && scene._enhancedWindowingEnabled;
}

#endif // HAVE(UIKIT_RESIZABLE_WINDOWS)

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
- (void)_setInteractionRegionsEnabled:(BOOL)enabled
{
    WKWEBVIEW_RELEASE_LOG("%p -[WKWebView _setInteractionRegionsEnabled] page - %p", self, _page.get());
    if (_page && _page->preferences().interactionRegionsEnabled())
        _page->setInteractionRegionsEnabled(enabled);
}
#endif // ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)


- (void)_setOpaqueInternal:(BOOL)opaque
{
    [super setOpaque:opaque];

    [_contentView setOpaque:opaque];

    if (!_page)
        return;

    std::optional<WebCore::Color> backgroundColor;
    if (!opaque)
        backgroundColor = WebCore::Color(WebCore::Color::transparentBlack);
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
    if (!_perProcessState.viewportMetaTagWidthWasExplicit || _perProcessState.viewportMetaTagCameFromImageDocument)
        return YES;

    // If the page set a viewport width that wasn't the device width, then it was
    // scaled and thus will probably need to zoom.
    if (_perProcessState.viewportMetaTagWidth != WebCore::ViewportArguments::ValueDeviceWidth)
        return YES;

    // At this point, we have a page that asked for width = device-width. However,
    // if the content's width and height were large, we might have had to shrink it.
    // We'll enable double tap zoom whenever we're not at the actual initial scale.
    return !WebKit::scalesAreEssentiallyEqual(contentZoomScale(self), _perProcessState.initialScaleFactor);
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

#if ENABLE(ASYNC_SCROLLING)
    // FIXME: We will want to detect whether snapping will occur before beginning to drag. See WebPageProxy::didCommitLayerTree.
    auto* coordinator = downcast<WebKit::RemoteScrollingCoordinatorProxyIOS>(_page->scrollingCoordinatorProxy());
    ASSERT(scrollView == _scrollView.get());
    [_scrollView _setDecelerationRateInternal:(coordinator && coordinator->shouldSetScrollViewDecelerationRateFast()) ? UIScrollViewDecelerationRateFast : UIScrollViewDecelerationRateNormal];

    coordinator->setRootNodeIsInUserScroll(true);
#endif
}

- (void)_didFinishScrolling:(UIScrollView *)scrollView
{
    if (scrollView != _scrollView.get())
        return;

    if (![self usesStandardContentView])
        return;

    [self _scheduleVisibleContentRectUpdate];
    [_contentView didFinishScrolling];

#if ENABLE(ASYNC_SCROLLING)
    _page->scrollingCoordinatorProxy()->setRootNodeIsInUserScroll(false);
#endif
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
#if ENABLE(ASYNC_SCROLLING)
    if (auto* coordinator = downcast<WebKit::RemoteScrollingCoordinatorProxyIOS>(_page->scrollingCoordinatorProxy())) {
        // FIXME: Here, I'm finding the maximum horizontal/vertical scroll offsets. There's probably a better way to do this.
        CGSize maxScrollOffsets = CGSizeMake(scrollView.contentSize.width - scrollView.bounds.size.width, scrollView.contentSize.height - scrollView.bounds.size.height);

        UIEdgeInsets obscuredInset;

        id<WKUIDelegatePrivate> uiDelegatePrivate = static_cast<id <WKUIDelegatePrivate>>([self UIDelegate]);
        if ([uiDelegatePrivate respondsToSelector:@selector(_webView:finalObscuredInsetsForScrollView:withVelocity:targetContentOffset:)])
            obscuredInset = [uiDelegatePrivate _webView:self finalObscuredInsetsForScrollView:scrollView withVelocity:velocity targetContentOffset:targetContentOffset];
        else
            obscuredInset = [self _computedObscuredInset];

        CGRect unobscuredRect = UIEdgeInsetsInsetRect(self.bounds, obscuredInset);

        coordinator->adjustTargetContentOffsetForSnapping(maxScrollOffsets, velocity, unobscuredRect.origin.y, scrollView.contentOffset, targetContentOffset);
    }
#endif
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
    // If we're decelerating, scroll offset will be updated when scrollViewDidFinishDecelerating: is called.
    if (!decelerate)
        [self _didFinishScrolling:scrollView];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
    [self _didFinishScrolling:scrollView];
}

- (void)scrollViewDidScrollToTop:(UIScrollView *)scrollView
{
    [self _didFinishScrolling:scrollView];
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

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
- (void)_scrollView:(UIScrollView *)scrollView asynchronouslyHandleScrollEvent:(UIScrollEvent *)scrollEvent completion:(void (^)(BOOL handled))completion
{
    BOOL isHandledByDefault = !scrollView.scrollEnabled;

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    if ([scrollEvent _scrollDeviceCategory] == _UIScrollDeviceCategoryOverlayScroll) {
        completion(isHandledByDefault);
        return;
    }
#endif

    if (scrollEvent.phase == UIScrollPhaseMayBegin) {
        completion(isHandledByDefault);
        return;
    }

    if (scrollEvent.phase == UIScrollPhaseBegan) {
        _currentScrollGestureState = std::nullopt;
        _wheelEventCountInCurrentScrollGesture = 0;
    }

    WebCore::IntPoint scrollLocation = WebCore::roundedIntPoint([scrollEvent locationInView:_contentView.get()]);
    auto eventListeners = WebKit::eventListenerTypesAtPoint(_contentView.get(), scrollLocation);
    bool hasWheelHandlers = eventListeners.contains(WebCore::EventListenerRegionType::Wheel);
    if (!hasWheelHandlers) {
        completion(isHandledByDefault);
        return;
    }

    // Scroll events with zero delta are not dispatched to the page, so cannot be
    // cancelled, so we can short-circuit them here.
    // We make an exception for end-phase events, similar to the logic in
    // EventHandler::handleWheelEventInAppropriateEnclosingBox.
    CGVector deltaVector = [scrollEvent _adjustedAcceleratedDeltaInView:_contentView.get()];
    if (!deltaVector.dx && !deltaVector.dy && scrollEvent.phase != UIScrollPhaseEnded)  {
        completion(isHandledByDefault);
        return;
    }

    bool hasActiveWheelHandlers = eventListeners.contains(WebCore::EventListenerRegionType::NonPassiveWheel);
    bool isCancelable = hasActiveWheelHandlers && (!_currentScrollGestureState || _currentScrollGestureState == WebCore::WheelScrollGestureState::Blocking);

    std::optional<WebKit::WebWheelEvent::Phase> overridePhase;
    // The first event with non-zero delta in a given gesture should be considered the
    // "Began" event in the WebCore sense (e.g. for deciding cancelability). Note that
    // this may not be a UIScrollPhaseBegin event, nor even necessarily the first UIScrollPhaseChanged event.
    if (!_wheelEventCountInCurrentScrollGesture)
        overridePhase = WebKit::WebWheelEvent::PhaseBegan;
    auto event = WebIOSEventFactory::createWebWheelEvent(scrollEvent, _contentView.get(), overridePhase);

    _wheelEventCountInCurrentScrollGesture++;
    _page->dispatchWheelEventWithoutScrolling(event, [weakSelf = WeakObjCPtr<WKWebView>(self), strongCompletion = makeBlockPtr(completion), isCancelable, isHandledByDefault](bool handled) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            strongCompletion(isHandledByDefault);
            return;
        }

        if (isCancelable) {
            if (!strongSelf->_currentScrollGestureState)
                strongSelf->_currentScrollGestureState = handled ? WebCore::WheelScrollGestureState::Blocking : WebCore::WheelScrollGestureState::NonBlocking;
            strongCompletion(isHandledByDefault || handled);
        }
    });

    if (!isCancelable)
        completion(isHandledByDefault);
}
#endif // HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

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
    [self _didFinishScrolling:scrollView];
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
    if (_perProcessState.frozenVisibleContentRect)
        return _perProcessState.frozenVisibleContentRect.value();

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

    return WebCore::FloatSize(UIEdgeInsetsInsetRect(CGRectMake(0, 0, bounds.size.width, bounds.size.height), self._scrollViewSystemContentInset).size);
}

- (void)_dispatchSetViewLayoutSize:(WebCore::FloatSize)viewLayoutSize
{
    if (_perProcessState.lastSentViewLayoutSize && CGSizeEqualToSize(_perProcessState.lastSentViewLayoutSize.value(), viewLayoutSize))
        return;

    LOG_WITH_STREAM(VisibleRects, stream << "-[WKWebView " << _page->identifier() << " _dispatchSetViewLayoutSize:] " << viewLayoutSize << " contentZoomScale " << contentZoomScale(self));
    _page->setViewportConfigurationViewLayoutSize(viewLayoutSize, _page->layoutSizeScaleFactor(), _page->minimumEffectiveDeviceWidth());
    _perProcessState.lastSentViewLayoutSize = viewLayoutSize;
}

- (void)_dispatchSetDeviceOrientation:(int32_t)deviceOrientation
{
    if (_perProcessState.lastSentDeviceOrientation && _perProcessState.lastSentDeviceOrientation.value() == deviceOrientation)
        return;

    _page->setDeviceOrientation(deviceOrientation);
    _perProcessState.lastSentDeviceOrientation = deviceOrientation;
}

- (BOOL)_updateScrollViewContentInsetsIfNecessary
{
#if PLATFORM(WATCHOS)
    return [_scrollView _setContentScrollInsetInternal:self._safeAreaShouldAffectObscuredInsets ? self._contentInsetsFromSystemMinimumLayoutMargins : UIEdgeInsetsZero];
#else
    return NO;
#endif
}

#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)

- (void)_beginAutomaticLiveResizeIfNeeded
{
    if (_perProcessState.liveResizeParameters)
        return;

    if (!self.window)
        return;

    if (!self.window.windowScene._isInLiveResize)
        return;

    [self _beginLiveResize];
    
    _endLiveResizeNotificationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:_UIWindowSceneDidEndLiveResizeNotification object:self.window.windowScene queue:NSOperationQueue.mainQueue usingBlock:makeBlockPtr([weakSelf = WeakObjCPtr<WKWebView>(self)] (NSNotification *) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;
        
        [strongSelf _destroyEndLiveResizeObserver];
        [strongSelf _endLiveResize];
    }).get()];
}

- (void)_destroyEndLiveResizeObserver
{
    if (!_endLiveResizeNotificationObserver)
        return;

    [[NSNotificationCenter defaultCenter] removeObserver:_endLiveResizeNotificationObserver.get()];
    _endLiveResizeNotificationObserver = nil;
}

- (void)_updateLiveResizeTransform
{
    CGFloat scale = self.bounds.size.width / _perProcessState.liveResizeParameters->viewWidth;
    CGAffineTransform transform = CGAffineTransformMakeScale(scale, scale);

    CGPoint newContentOffset = [self _contentOffsetAdjustedForObscuredInset:CGPointMake(_perProcessState.liveResizeParameters->initialScrollPosition.x * scale, _perProcessState.liveResizeParameters->initialScrollPosition.y * scale)];
    CGPoint currentContentOffset = [_scrollView contentOffset];

    transform.tx = currentContentOffset.x - newContentOffset.x;
    transform.ty = currentContentOffset.y - newContentOffset.y;

    [_resizeAnimationView setTransform:transform];
}

#endif // HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)

- (void)_frameOrBoundsWillChange
{
#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)
    if (_page && _page->preferences().automaticLiveResizeEnabled())
        [self _beginAutomaticLiveResizeIfNeeded];
#endif
}

- (void)_frameOrBoundsMayHaveChanged
{
    CGRect bounds = self.bounds;
    [_scrollView setFrame:bounds];

#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)
    if (_perProcessState.liveResizeParameters)
        [self _updateLiveResizeTransform];
#endif

    if (!self._shouldDeferGeometryUpdates) {
        if (!_viewLayoutSizeOverride)
            [self _dispatchSetViewLayoutSize:[self activeViewLayoutSize:self.bounds]];
        if (!_maximumUnobscuredSizeOverride)
            _page->setDefaultUnobscuredSize(WebCore::FloatSize(bounds.size));
        [self _recalculateViewportSizesWithMinimumViewportInset:_minimumViewportInset maximumViewportInset:_maximumViewportInset throwOnInvalidInput:NO];

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

#if HAVE(UIKIT_RESIZABLE_WINDOWS)

- (void)_acquireResizeAssertionForReason:(NSString *)reason
{
    UIWindowScene *windowScene = self.window.windowScene;
    if (!windowScene)
        return;
    if (![windowScene respondsToSelector:@selector(_holdLiveResizeSnapshotForReason:)])
        return;

    if (_resizeAssertions.isEmpty()) {
        auto didInvalidateResizeAssertions = Box<bool>::create(false);

        [self _doAfterNextVisibleContentRectUpdate:makeBlockPtr([weakSelf = WeakObjCPtr<WKWebView>(self), didInvalidateResizeAssertions] {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            if (*didInvalidateResizeAssertions)
                return;

            [strongSelf _invalidateResizeAssertions];

            *didInvalidateResizeAssertions = true;
        }).get()];

        RunLoop::main().dispatchAfter(1_s, [weakSelf = WeakObjCPtr<WKWebView>(self), didInvalidateResizeAssertions] {
            if (*didInvalidateResizeAssertions)
                return;

            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            WKWEBVIEW_RELEASE_LOG("WKWebView %p next visible content rect update took too long; clearing resize assertions", strongSelf.get());
            [strongSelf _invalidateResizeAssertions];

            *didInvalidateResizeAssertions = true;
        });
    }

    _resizeAssertions.append(retainPtr([windowScene _holdLiveResizeSnapshotForReason:reason]));
}

- (void)_invalidateResizeAssertions
{
    for (auto resizeAssertion : std::exchange(_resizeAssertions, { }))
        [resizeAssertion _invalidate];
}

#endif // HAVE(UIKIT_RESIZABLE_WINDOWS)

// Unobscured content rect where the user can interact. When the keyboard is up, this should be the area above or below the keyboard, wherever there is enough space.
- (CGRect)_contentRectForUserInteraction
{
    // FIXME: handle split keyboard.
    UIEdgeInsets obscuredInsets = _obscuredInsets;
    obscuredInsets.bottom = std::max(_obscuredInsets.bottom, _inputViewBoundsInWindow.size.height);
    CGRect unobscuredRect = UIEdgeInsetsInsetRect(self.bounds, obscuredInsets);
    return [self convertRect:unobscuredRect toView:self._currentContentView];
}

// Ideally UIScrollView would expose this for us: <rdar://problem/21394567>.
- (BOOL)_scrollViewIsRubberBanding:(UIScrollView *)scrollView
{
    float deviceScaleFactor = _page->deviceScaleFactor();

    CGPoint contentOffset = [scrollView contentOffset];
    CGPoint boundedOffset = contentOffsetBoundedInValidRange(scrollView, contentOffset);
    return !pointsEqualInDevicePixels(contentOffset, boundedOffset, deviceScaleFactor);
}

- (BOOL)_scrollViewIsRubberBandingForRefreshControl
{
    if (![_scrollView refreshControl])
        return NO;

    CGPoint contentOffset = [_scrollView contentOffset];
    UIEdgeInsets contentInsets = [_scrollView adjustedContentInset];
    return contentOffset.y < -contentInsets.top && [self _scrollViewIsRubberBanding:_scrollView.get()];
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

- (void)_scheduleForcedVisibleContentRectUpdate
{
    _alwaysSendNextVisibleContentRectUpdate = YES;
    [self _scheduleVisibleContentRectUpdate];
}

- (OptionSet<WebKit::ViewStabilityFlag>)_viewStabilityState:(UIScrollView *)scrollView
{
    OptionSet<WebKit::ViewStabilityFlag> stabilityFlags;

    if (scrollView.isDragging || scrollView.isZooming || scrollView._isInterruptingDeceleration)
        stabilityFlags.add(WebKit::ViewStabilityFlag::ScrollViewInteracting);

    if (scrollView.isDecelerating || scrollView._isAnimatingZoom || scrollView._isScrollingToTop)
        stabilityFlags.add(WebKit::ViewStabilityFlag::ScrollViewAnimatedScrollOrZoom);

    if (scrollView == _scrollView.get() && _isChangingObscuredInsetsInteractively)
        stabilityFlags.add(WebKit::ViewStabilityFlag::ChangingObscuredInsetsInteractively);

    if ([self _scrollViewIsRubberBanding:scrollView])
        stabilityFlags.add(WebKit::ViewStabilityFlag::ScrollViewRubberBanding);

    if (NSNumber *stableOverride = self._stableStateOverride) {
        if (stableOverride.boolValue)
            stabilityFlags = { };
        else
            stabilityFlags.add(WebKit::ViewStabilityFlag::UnstableForTesting);
    }

    return stabilityFlags;
}

- (void)_addUpdateVisibleContentRectPreCommitHandler
{
    auto retainedSelf = retainPtr(self);
    [CATransaction addCommitHandler:[retainedSelf] {
        WKWebView *webView = retainedSelf.get();
        if (![webView _isValid]) {
            WKWEBVIEW_RELEASE_LOG("In CATransaction preCommitHandler, WKWebView %p is invalid", webView);
            return;
        }

        @try {
            [webView _updateVisibleContentRects];
        } @catch (NSException *exception) {
            WKWEBVIEW_RELEASE_LOG("In CATransaction preCommitHandler, -[WKWebView %p _updateVisibleContentRects] threw an exception", webView);
        }
        webView->_perProcessState.hasScheduledVisibleRectUpdate = NO;
    } forPhase:kCATransactionPhasePreCommit];
}

- (void)_scheduleVisibleContentRectUpdateAfterScrollInView:(UIScrollView *)scrollView
{
    _viewStabilityWhenVisibleContentRectUpdateScheduled = [self _viewStabilityState:scrollView];

    if (_perProcessState.hasScheduledVisibleRectUpdate) {
        auto timeNow = MonotonicTime::now();
        if ((timeNow - _timeOfRequestForVisibleContentRectUpdate) > delayBeforeNoVisibleContentsRectsLogging) {
            WKWEBVIEW_RELEASE_LOG("-[WKWebView %p _scheduleVisibleContentRectUpdateAfterScrollInView]: hasScheduledVisibleRectUpdate is true but haven't updated visible content rects for %.2fs (last update was %.2fs ago) - valid %d",
                self, (timeNow - _timeOfRequestForVisibleContentRectUpdate).value(), (timeNow - _timeOfLastVisibleContentRectUpdate).value(), [self _isValid]);
        }
        return;
    }

    _perProcessState.hasScheduledVisibleRectUpdate = YES;
    _timeOfRequestForVisibleContentRectUpdate = MonotonicTime::now();

    CATransactionPhase transactionPhase = [CATransaction currentPhase];
    if (transactionPhase == kCATransactionPhaseNull || transactionPhase == kCATransactionPhasePreLayout) {
        [self _addUpdateVisibleContentRectPreCommitHandler];
        return;
    }

    RunLoop::main().dispatch([retainedSelf = retainPtr(self)] {
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

    CGFloat horizontalRubberbandAmountInContentCoordinates = (contentOffset.x - boundedOffset.x) / scaleFactor;
    CGFloat verticalRubberbandAmountInContentCoordinates = (contentOffset.y - boundedOffset.y) / scaleFactor;

    CGRect extendedBounds = [_contentView bounds];

    if (horizontalRubberbandAmountInContentCoordinates < 0) {
        extendedBounds.origin.x += horizontalRubberbandAmountInContentCoordinates;
        extendedBounds.size.width -= horizontalRubberbandAmountInContentCoordinates;
    } else if (horizontalRubberbandAmountInContentCoordinates > 0)
        extendedBounds.size.width += horizontalRubberbandAmountInContentCoordinates;

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

- (BOOL)_shouldDeferGeometryUpdates
{
    return _perProcessState.liveResizeParameters || _perProcessState.dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing;
}

- (void)_updateVisibleContentRects
{
    auto viewStability = _viewStabilityWhenVisibleContentRectUpdateScheduled;

    if (![self usesStandardContentView]) {
        [_passwordView setFrame:self.bounds];
        [_customContentView web_computedContentInsetDidChange];
        _perProcessState.didDeferUpdateVisibleContentRectsForAnyReason = YES;
        WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - usesStandardContentView is NO, bailing", self, _page->identifier().toUInt64());
        return;
    }

    auto timeNow = MonotonicTime::now();
    if (_timeOfFirstVisibleContentRectUpdateWithPendingCommit) {
        auto timeSinceFirstRequestWithPendingCommit = timeNow - *_timeOfFirstVisibleContentRectUpdateWithPendingCommit;
        if (timeSinceFirstRequestWithPendingCommit > delayBeforeNoCommitsLogging)
            WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - have not received a commit %.2fs after visible content rect update; lastTransactionID %llu", self, _page->identifier().toUInt64(), timeSinceFirstRequestWithPendingCommit.value(), _perProcessState.lastTransactionID.toUInt64());
    }

    if (_perProcessState.invokingUIScrollViewDelegateCallback) {
        _perProcessState.didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback = YES;
        _perProcessState.didDeferUpdateVisibleContentRectsForAnyReason = YES;
        WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - _invokingUIScrollViewDelegateCallback is YES, bailing", self, _page->identifier().toUInt64());
        return;
    }

    if (!CGRectIsEmpty(_perProcessState.animatedResizeOldBounds))
        [self _cancelAnimatedResize];

    if (self._shouldDeferGeometryUpdates
        || (_perProcessState.needsResetViewStateAfterCommitLoadForMainFrame && ![_contentView sizeChangedSinceLastVisibleContentRectUpdate])
        || [_scrollView isZoomBouncing]
        || _perProcessState.currentlyAdjustingScrollViewInsetsForKeyboard) {
        _perProcessState.didDeferUpdateVisibleContentRectsForAnyReason = YES;
        _perProcessState.didDeferUpdateVisibleContentRectsForUnstableScrollView = YES;
        WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - scroll view state is non-stable, bailing (dynamicViewportUpdateMode %d, needsResetViewStateAfterCommitLoadForMainFrame %d, sizeChangedSinceLastVisibleContentRectUpdate %d, [_scrollView isZoomBouncing] %d, currentlyAdjustingScrollViewInsetsForKeyboard %d)",
            self, _page->identifier().toUInt64(), _perProcessState.dynamicViewportUpdateMode, _perProcessState.needsResetViewStateAfterCommitLoadForMainFrame, [_contentView sizeChangedSinceLastVisibleContentRectUpdate], [_scrollView isZoomBouncing], _perProcessState.currentlyAdjustingScrollViewInsetsForKeyboard);
        return;
    }

    if (_perProcessState.didDeferUpdateVisibleContentRectsForAnyReason)
        WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _updateVisibleContentRects:] - performing first visible content rect update after deferring updates", self, _page->identifier().toUInt64());

    _perProcessState.didDeferUpdateVisibleContentRectsForUIScrollViewDelegateCallback = NO;
    _perProcessState.didDeferUpdateVisibleContentRectsForUnstableScrollView = NO;
    _perProcessState.didDeferUpdateVisibleContentRectsForAnyReason = NO;

    [self _updateScrollViewContentInsetsIfNecessary];

    CGRect visibleRectInContentCoordinates = [self _visibleContentRect];

    UIEdgeInsets computedContentInsetUnadjustedForKeyboard = [self _computedObscuredInset];
    if (!_haveSetObscuredInsets)
        computedContentInsetUnadjustedForKeyboard.bottom -= _totalScrollViewBottomInsetAdjustmentForKeyboard;

    CGFloat scaleFactor = contentZoomScale(self);
    CGRect unobscuredRect = UIEdgeInsetsInsetRect(self.bounds, computedContentInsetUnadjustedForKeyboard);
    WebCore::FloatRect unobscuredRectInContentCoordinates = WebCore::FloatRect(_perProcessState.frozenUnobscuredContentRect ? _perProcessState.frozenUnobscuredContentRect.value() : [self convertRect:unobscuredRect toView:_contentView.get()]);
    unobscuredRectInContentCoordinates.intersect([self _contentBoundsExtendedForRubberbandingWithScale:scaleFactor]);

    auto contentInsets = [self currentlyVisibleContentInsetsWithScale:scaleFactor obscuredInsets:computedContentInsetUnadjustedForKeyboard];

#if ENABLE(ASYNC_SCROLLING)
    if (viewStability.isEmpty()) {
        auto* coordinator = downcast<WebKit::RemoteScrollingCoordinatorProxyIOS>(_page->scrollingCoordinatorProxy());
        if (coordinator && coordinator->hasActiveSnapPoint()) {
            CGPoint currentPoint = [_scrollView contentOffset];
            CGPoint activePoint = coordinator->nearestActiveContentInsetAdjustedSnapOffset(unobscuredRect.origin.y, currentPoint);

            if (!CGPointEqualToPoint(activePoint, currentPoint)) {
                RetainPtr<WKScrollView> strongScrollView = _scrollView;
                RunLoop::main().dispatch([strongScrollView, activePoint] {
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
        inputViewBounds:_inputViewBoundsInWindow
        scale:scaleFactor minimumScale:[_scrollView minimumZoomScale]
        viewStability:viewStability
        enclosedInScrollableAncestorView:scrollViewCanScroll([self _scroller])
        sendEvenIfUnchanged:_alwaysSendNextVisibleContentRectUpdate];

    while (!_visibleContentRectUpdateCallbacks.isEmpty()) {
        auto callback = _visibleContentRectUpdateCallbacks.takeLast();
        callback();
    }

    if ((timeNow - _timeOfRequestForVisibleContentRectUpdate) > delayBeforeNoVisibleContentsRectsLogging)
        WKWEBVIEW_RELEASE_LOG("%p -[WKWebView _updateVisibleContentRects:] finally ran %.2fs after being scheduled", self, (timeNow - _timeOfRequestForVisibleContentRectUpdate).value());

    _alwaysSendNextVisibleContentRectUpdate = NO;
    _timeOfLastVisibleContentRectUpdate = timeNow;
    if (!_timeOfFirstVisibleContentRectUpdateWithPendingCommit)
        _timeOfFirstVisibleContentRectUpdateWithPendingCommit = timeNow;
}

- (void)_didStartProvisionalLoadForMainFrame
{
    if (_gestureController)
        _gestureController->didStartProvisionalLoadForMainFrame();
}

static WebCore::FloatSize activeMinimumUnobscuredSize(WKWebView *webView, const CGRect& bounds)
{
    return WebCore::FloatSize(webView->_minimumUnobscuredSizeOverride.value_or(bounds.size));
}

static WebCore::FloatSize activeMaximumUnobscuredSize(WKWebView *webView, const CGRect& bounds)
{
    return WebCore::FloatSize(webView->_maximumUnobscuredSizeOverride.value_or(bounds.size));
}

static int32_t activeOrientation(WKWebView *webView)
{
    return webView->_overridesInterfaceOrientation ? deviceOrientationForUIInterfaceOrientation(webView->_interfaceOrientationOverride) : webView->_page->deviceOrientation();
}

- (void)_ensureResizeAnimationView
{
    if (_resizeAnimationView)
        return;

    NSUInteger indexOfContentView = [[_scrollView subviews] indexOfObject:_contentView.get()];
    _resizeAnimationView = adoptNS([[UIView alloc] init]);
    [_resizeAnimationView layer].name = @"ResizeAnimation";
    [_scrollView insertSubview:_resizeAnimationView.get() atIndex:indexOfContentView];
    [_resizeAnimationView addSubview:_contentView.get()];
    [_resizeAnimationView addSubview:[_contentView unscaledView]];
}

- (void)_destroyResizeAnimationView
{
    if (!_resizeAnimationView)
        return;

    NSUInteger indexOfResizeAnimationView = [[_scrollView subviews] indexOfObject:_resizeAnimationView.get()];
    [_scrollView insertSubview:_contentView.get() atIndex:indexOfResizeAnimationView];
    [_scrollView insertSubview:[_contentView unscaledView] atIndex:indexOfResizeAnimationView + 1];
    [_resizeAnimationView removeFromSuperview];
    _resizeAnimationView = nil;
}

- (void)_cancelAnimatedResize
{
    WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _cancelAnimatedResize] dynamicViewportUpdateMode %d", self, _page->identifier().toUInt64(), _perProcessState.dynamicViewportUpdateMode);

    if (_perProcessState.dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing)
        return;

    if (!_customContentView) {
        [self _destroyResizeAnimationView];

        [_contentView setHidden:NO];
        _resizeAnimationTransformAdjustments = CATransform3DIdentity;
    }

    _perProcessState.dynamicViewportUpdateMode = WebKit::DynamicViewportUpdateMode::NotResizing;
    _perProcessState.animatedResizeOldBounds = { };
    [self _scheduleVisibleContentRectUpdate];
}

- (void)_didCompleteAnimatedResize
{
    if (_perProcessState.dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing)
        return;

    [_contentView setHidden:NO];

    if (!_resizeAnimationView) {
        // Paranoia. If _resizeAnimationView is null we'll end up setting a zero scale on the content view.
        WKWEBVIEW_RELEASE_LOG("%p -[WKWebView _didCompleteAnimatedResize:] - _resizeAnimationView is nil", self);
        [self _cancelAnimatedResize];
        return;
    }

    WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _didCompleteAnimatedResize]", self, _page->identifier().toUInt64());

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

    _perProcessState.dynamicViewportUpdateMode = WebKit::DynamicViewportUpdateMode::NotResizing;
    _perProcessState.animatedResizeOldBounds = { };

    [self _didStopDeferringGeometryUpdates];
}

- (void)_didStopDeferringGeometryUpdates
{
    [self _scheduleVisibleContentRectUpdate];

    CGRect newBounds = self.bounds;
    auto newViewLayoutSize = [self activeViewLayoutSize:newBounds];
    auto newMinimumUnobscuredSize = activeMinimumUnobscuredSize(self, newBounds);
    auto newMaximumUnobscuredSize = activeMaximumUnobscuredSize(self, newBounds);
    int32_t newOrientation = activeOrientation(self);

    if (!_perProcessState.lastSentViewLayoutSize || newViewLayoutSize != _perProcessState.lastSentViewLayoutSize.value())
        [self _dispatchSetViewLayoutSize:newViewLayoutSize];

    if (_minimumUnobscuredSizeOverride)
        _page->setMinimumUnobscuredSize(WebCore::FloatSize(newMinimumUnobscuredSize));
    if (_maximumUnobscuredSizeOverride) {
        _page->setDefaultUnobscuredSize(WebCore::FloatSize(newMaximumUnobscuredSize));
        _page->setMaximumUnobscuredSize(WebCore::FloatSize(newMaximumUnobscuredSize));
    }
    [self _recalculateViewportSizesWithMinimumViewportInset:_minimumViewportInset maximumViewportInset:_maximumViewportInset throwOnInvalidInput:NO];

    if (!_perProcessState.lastSentDeviceOrientation || newOrientation != _perProcessState.lastSentDeviceOrientation.value())
        [self _dispatchSetDeviceOrientation:newOrientation];

    while (!_callbacksDeferredDuringResize.isEmpty())
        _callbacksDeferredDuringResize.takeLast()();
}

- (void)_didFinishNavigation:(API::Navigation*)navigation
{
    if (_gestureController)
        _gestureController->didFinishNavigation(navigation);
}

- (void)_didFailNavigation:(API::Navigation*)navigation
{
    if (_gestureController)
        _gestureController->didFailNavigation(navigation);
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

    auto previousInputViewBounds = _inputViewBoundsInWindow;
    BOOL selectionWasVisible = self._selectionRectIsFullyVisibleAndNonEmpty;

    _inputViewBoundsInWindow = ([&] {
        if (UIPeripheralHost.sharedInstance.isUndocked)
            return CGRectZero;

        auto keyboardFrameInScreen = endFrameValue.CGRectValue;
        // The keyboard rect is always in screen coordinates. In the view services case the window does not
        // have the interface orientation rotation transformation; its host does. So, it makes no sense to
        // clip the keyboard rect against its screen.
        if (!self.window._isHostedInAnotherProcess)
            keyboardFrameInScreen = CGRectIntersection(keyboardFrameInScreen, self.window.screen.bounds);

        return [self.window convertRect:keyboardFrameInScreen fromCoordinateSpace:self.window.screen.coordinateSpace];
    })();

    if (adjustScrollView) {
        CGFloat bottomInsetBeforeAdjustment = [_scrollView contentInset].bottom;
        SetForScope insetAdjustmentGuard(_perProcessState.currentlyAdjustingScrollViewInsetsForKeyboard, YES);
        [_scrollView _adjustForAutomaticKeyboardInfo:keyboardInfo animated:YES lastAdjustment:&_lastAdjustmentForScroller];
        CGFloat bottomInsetAfterAdjustment = [_scrollView contentInset].bottom;
        // FIXME: This "total bottom content inset adjustment" mechanism hasn't worked since iOS 11, since -_adjustForAutomaticKeyboardInfo:animated:lastAdjustment:
        // no longer sets -[UIScrollView contentInset] for apps linked on or after iOS 11. We should consider removing this logic, since the original bug this was
        // intended to fix, <rdar://problem/23202254>, remains fixed through other means.
        if (bottomInsetBeforeAdjustment != bottomInsetAfterAdjustment)
            _totalScrollViewBottomInsetAdjustmentForKeyboard += bottomInsetAfterAdjustment - bottomInsetBeforeAdjustment;
    }

    if (selectionWasVisible && [_contentView _hasFocusedElement] && !CGRectIsEmpty(previousInputViewBounds) && !CGRectIsEmpty(_inputViewBoundsInWindow) && !CGRectEqualToRect(previousInputViewBounds, _inputViewBoundsInWindow))
        [self _scrollToAndRevealSelectionIfNeeded];

    [self _scheduleVisibleContentRectUpdate];
}

- (BOOL)_shouldUpdateKeyboardWithInfo:(NSDictionary *)keyboardInfo
{
    if ([_contentView isFocusingElement])
        return YES;

#if HAVE(UIFINDINTERACTION)
    if ([_findInteraction isFindNavigatorVisible])
        return YES;
#endif

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
    [_contentView _keyboardWillShow];
}

- (void)_keyboardDidShow:(NSNotification *)notification
{
    _page->setIsKeyboardAnimatingIn(false);
    [_contentView _keyboardDidShow];
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

    _perProcessState.frozenVisibleContentRect = [self convertRect:fullViewRect toView:_contentView.get()];
    _perProcessState.frozenUnobscuredContentRect = [self convertRect:unobscuredRect toView:_contentView.get()];
    _contentViewShouldBecomeFirstResponderAfterNavigationGesture = [_contentView isFirstResponder];

    LOG_WITH_STREAM(VisibleRects, stream << "_navigationGestureDidBegin: freezing visibleContentRect " << WebCore::FloatRect(_perProcessState.frozenVisibleContentRect.value()) << " UnobscuredContentRect " << WebCore::FloatRect(_perProcessState.frozenUnobscuredContentRect.value()));
}

- (void)_navigationGestureDidEnd
{
    _perProcessState.frozenVisibleContentRect = std::nullopt;
    _perProcessState.frozenUnobscuredContentRect = std::nullopt;

    if (_contentViewShouldBecomeFirstResponderAfterNavigationGesture) {
        if (self.window && ![_contentView isFirstResponder])
            [_contentView becomeFirstResponder];
        _contentViewShouldBecomeFirstResponderAfterNavigationGesture = NO;
    }
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
    if (_perProcessState.avoidsUnsafeArea == avoidsUnsafeArea)
        return;

    _perProcessState.avoidsUnsafeArea = avoidsUnsafeArea;

    if ([self _updateScrollViewContentInsetsIfNecessary] && !self._shouldDeferGeometryUpdates && !_viewLayoutSizeOverride)
        [self _dispatchSetViewLayoutSize:[self activeViewLayoutSize:self.bounds]];

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
        RunLoop::main().dispatch([strongSelf] {
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

- (void)buildMenuWithBuilder:(id <UIMenuBuilder>)builder
{
    if (self.usesStandardContentView)
        [_contentView buildMenuForWebViewWithBuilder:builder];

    [super buildMenuWithBuilder:builder];
}

- (BOOL)_shouldAvoidSecurityHeuristicScoreUpdates
{
    return [_contentView _shouldAvoidSecurityHeuristicScoreUpdates];
}

#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)

- (void)_beginLiveResize
{
    if (_perProcessState.liveResizeParameters) {
        RELEASE_LOG_FAULT(Resize, "Error: _beginLiveResize called with an outstanding live resize.");
        return;
    }

    if (_perProcessState.dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing) {
        RELEASE_LOG_FAULT(Resize, "Error: _beginLiveResize called during an animated resize.");
        return;
    }

    WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _beginLiveResize]", self, _page->identifier().toUInt64());

    _perProcessState.liveResizeParameters = { { self.bounds.size.width, self.scrollView.contentOffset } };

    [self _ensureResizeAnimationView];
}

- (void)_endLiveResize
{
    WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _endLiveResize]", self, _page->identifier().toUInt64());

    if (!_perProcessState.liveResizeParameters)
        return;

    UIView *liveResizeSnapshotView = [self snapshotViewAfterScreenUpdates:NO];
    [liveResizeSnapshotView setFrame:self.bounds];
    [self addSubview:liveResizeSnapshotView];

    _perProcessState.liveResizeParameters = std::nullopt;

    ASSERT(_perProcessState.dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing);
    [self _destroyResizeAnimationView];
    [self _didStopDeferringGeometryUpdates];

    [self _doAfterNextPresentationUpdate:^{
        [liveResizeSnapshotView removeFromSuperview];
    }];    
}

#endif // HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)

#if HAVE(UIFINDINTERACTION)

- (id<UITextSearching>)_searchableObject
{
    if ([_customContentView conformsToProtocol:@protocol(UITextSearching)])
        return (id<UITextSearching>)_customContentView.get();

    return _contentView.get();
}

- (BOOL)supportsTextReplacement
{
    if ([_customContentView respondsToSelector:@selector(supportsTextReplacement)])
        return [(id<UITextSearching>)_customContentView.get() supportsTextReplacement];

    return [_contentView supportsTextReplacementForWebView];
}

#endif // HAVE(UIFINDINTERACTION)

#if HAVE(UIKIT_RESIZABLE_WINDOWS)

- (void)_enhancedWindowingToggled:(NSNotification *)notification
{
    if (dynamic_objc_cast<UIWindowScene>(notification.object) != self.window.windowScene)
        return;

    if (!_page || !_page->hasRunningProcess())
        return;

    _page->setIsWindowResizingEnabled(self._isWindowResizingEnabled);
}

#endif // HAVE(UIKIT_RESIZABLE_WINDOWS)

#if ENABLE(LOCKDOWN_MODE_API)

constexpr auto WebKitLockdownModeAlertShownKey = @"WebKitLockdownModeAlertShown";

static bool lockdownModeWarningNeeded = true;

+ (void)_clearLockdownModeWarningNeeded
{
    lockdownModeWarningNeeded = true;
}

static bool isLockdownModeWarningNeeded()
{
    // Only present the alert if the app is not Safari
    // and we've never presented the alert before
    if (WebCore::IOSApplication::isMobileSafari())
        return false;

    if (![WKProcessPool _lockdownModeEnabledGloballyForTesting] || [[NSUserDefaults standardUserDefaults] boolForKey:WebKitLockdownModeAlertShownKey])
        return false;

    return true;
}

- (void)_presentLockdownMode
{
    lockdownModeWarningNeeded = isLockdownModeWarningNeeded();
    if (!lockdownModeWarningNeeded)
        return;

    auto message = WEB_UI_NSSTRING(@"Certain experiences and features may not function as expected. You can turn off Lockdown Mode for this app in Settings.", "Lockdown Mode alert message");

    auto decisionHandler = makeBlockPtr([message, protectedSelf = retainPtr(self)](WKDialogResult result) mutable {
        if (result == WKDialogResultAskAgain) {
            lockdownModeWarningNeeded = true;
            return;
        }

        lockdownModeWarningNeeded = false;

        if (result == WKDialogResultHandled) {
            [[NSUserDefaults standardUserDefaults] setBool:YES forKey:WebKitLockdownModeAlertShownKey];
            return;
        }

        RunLoop::main().dispatch([message = retainPtr(message), protectedSelf = WTFMove(protectedSelf)] {
            NSString *appDisplayName = [[NSBundle mainBundle] objectForInfoDictionaryKey:(__bridge NSString *)_kCFBundleDisplayNameKey];
            if (!appDisplayName)
                appDisplayName = [[NSBundle mainBundle] objectForInfoDictionaryKey:(__bridge NSString *)kCFBundleNameKey];

            UIAlertController *alert = [UIAlertController alertControllerWithTitle:[NSString stringWithFormat:WEB_UI_NSSTRING(@"Lockdown Mode is Turned On For “%@“", "Lockdown Mode alert title"), appDisplayName] message:message.get() preferredStyle:UIAlertControllerStyleAlert];

            [alert addAction:[UIAlertAction actionWithTitle:WEB_UI_NSSTRING(@"OK", "Lockdown Mode alert OK button") style:UIAlertActionStyleDefault handler:nil]];

            UIViewController *presentationViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:protectedSelf.get()];
            [presentationViewController presentViewController:alert animated:YES completion:nil];
            [[NSUserDefaults standardUserDefaults] setBool:YES forKey:WebKitLockdownModeAlertShownKey];
        });
    });
    
#if PLATFORM(IOS)
    if ([self.UIDelegate respondsToSelector:@selector(webView:showLockdownModeFirstUseMessage:completionHandler:)]) {
        [self.UIDelegate webView:self showLockdownModeFirstUseMessage:message completionHandler:decisionHandler.get()];
        return;
    }
#endif

    decisionHandler(WKDialogResultShowDefault);
}

- (void)_presentLockdownModeAlertIfNeeded
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [self _presentLockdownMode];
        _needsToPresentLockdownModeMessage = NO;
    });

    if (!lockdownModeWarningNeeded)
        return;

    if (!_needsToPresentLockdownModeMessage)
        return;

    [self _presentLockdownMode];
    _needsToPresentLockdownModeMessage = NO;
}
#else
- (void)_presentLockdownModeAlertIfNeeded
{
}
#endif

@end

@implementation WKWebView (WKPrivateIOS)

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
- (void)_setUIEventAttribution:(UIEventAttribution *)attribution
{
#if HAVE(UI_EVENT_ATTRIBUTION)
    if (attribution) {
        WebCore::PrivateClickMeasurement measurement(
            WebCore::PrivateClickMeasurement::SourceID(attribution.sourceIdentifier),
            WebCore::PCM::SourceSite(attribution.reportEndpoint),
            WebCore::PCM::AttributionDestinationSite(attribution.destinationURL),
            WebCore::applicationBundleIdentifier(),
            WallTime::now(),
            WebCore::PCM::AttributionEphemeral::No
        );
        _page->setPrivateClickMeasurement({{ WTFMove(measurement), attribution.sourceDescription, attribution.purchaser }});
    } else
        _page->setPrivateClickMeasurement(std::nullopt);
#endif
}

- (UIEventAttribution *)_uiEventAttribution
{
#if HAVE(UI_EVENT_ATTRIBUTION)
    auto& measurement = _page->privateClickMeasurement();
    if (!measurement)
        return nil;

    URL destinationURL { makeString("https://", measurement->pcm.destinationSite().registrableDomain.string()) };
    return adoptNS([[UIEventAttribution alloc] initWithSourceIdentifier:measurement->pcm.sourceID() destinationURL:destinationURL sourceDescription:measurement->sourceDescription purchaser:measurement->purchaser]).autorelease();
#else
    return nil;
#endif
}

- (void)_setEphemeralUIEventAttribution:(UIEventAttribution *)attribution
{
    // FIXME: Deprecate and remove this version without a bundle ID.
    [self _setEphemeralUIEventAttribution:attribution forApplicationWithBundleID:@"com.apple.mobilesafari"];
}

- (void)_setEphemeralUIEventAttribution:(UIEventAttribution *)attribution forApplicationWithBundleID:(NSString *)bundleID
{
#if HAVE(UI_EVENT_ATTRIBUTION)
    if (attribution) {
        WebCore::PrivateClickMeasurement measurement(
            WebCore::PrivateClickMeasurement::SourceID(attribution.sourceIdentifier),
            WebCore::PCM::SourceSite(attribution.reportEndpoint),
            WebCore::PCM::AttributionDestinationSite(attribution.destinationURL),
            bundleID,
            WallTime::now(),
            WebCore::PCM::AttributionEphemeral::Yes
        );
        _page->setPrivateClickMeasurement({{ WTFMove(measurement), attribution.sourceDescription, attribution.purchaser }});
    } else
        _page->setPrivateClickMeasurement(std::nullopt);
#endif
}

- (UIEventAttribution *)_ephemeralUIEventAttribution
{
    return self._uiEventAttribution;
}

#endif // !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

- (CGRect)_contentVisibleRect
{
    return [self convertRect:[self bounds] toView:self._currentContentView];
}

// Deprecated SPI.
- (CGSize)_minimumLayoutSizeOverride
{
    ASSERT(_viewLayoutSizeOverride);
    return _viewLayoutSizeOverride.value_or(CGSizeZero);
}

- (void)_setViewLayoutSizeOverride:(CGSize)viewLayoutSizeOverride
{
    _viewLayoutSizeOverride = viewLayoutSizeOverride;

    if (!self._shouldDeferGeometryUpdates)
        [self _dispatchSetViewLayoutSize:WebCore::FloatSize(viewLayoutSizeOverride)];
}

- (CGSize)_minimumUnobscuredSizeOverride
{
    ASSERT(_minimumUnobscuredSizeOverride);
    return _minimumUnobscuredSizeOverride.value_or(CGSizeZero);
}

- (void)_setMinimumUnobscuredSizeOverride:(CGSize)size
{
    ASSERT((!self.bounds.size.width || size.width <= self.bounds.size.width) && (!self.bounds.size.height || size.height <= self.bounds.size.height));
    _minimumUnobscuredSizeOverride = size;

    if (!self._shouldDeferGeometryUpdates)
        _page->setMinimumUnobscuredSize(WebCore::FloatSize(size));
}

// Deprecated SPI
- (CGSize)_maximumUnobscuredSizeOverride
{
    ASSERT(_maximumUnobscuredSizeOverride);
    return _maximumUnobscuredSizeOverride.value_or(CGSizeZero);
}

- (void)_setMaximumUnobscuredSizeOverride:(CGSize)size
{
    ASSERT((!self.bounds.size.width || size.width <= self.bounds.size.width) && (!self.bounds.size.height || size.height <= self.bounds.size.height));
    _maximumUnobscuredSizeOverride = size;

    if (!self._shouldDeferGeometryUpdates) {
        _page->setDefaultUnobscuredSize(WebCore::FloatSize(size));
        _page->setMaximumUnobscuredSize(WebCore::FloatSize(size));
    }
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
    return _perProcessState.avoidsUnsafeArea;
}

- (UIView *)_enclosingViewForExposedRectComputation
{
    return [self _scroller];
}

- (void)_setInterfaceOrientationOverride:(UIInterfaceOrientation)interfaceOrientation
{
    if (_overridesInterfaceOrientation && interfaceOrientation == _interfaceOrientationOverride)
        return;

    _overridesInterfaceOrientation = YES;
    _interfaceOrientationOverride = interfaceOrientation;

    if (!self._shouldDeferGeometryUpdates)
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
    for (auto& type : WebCore::MIMETypeRegistry::pdfMIMETypes()) {
        Class providerClass = [[_configuration _contentProviderRegistry] providerForMIMEType:@(type.characters())];
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
        RunLoop::main().dispatch([updateBlock = makeBlockPtr(updateBlock)] {
            updateBlock();
        });
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
    _page->detectDataInAllFrames(fromWKDataDetectorTypes(types), [completion = makeBlockPtr(completion), page = WeakPtr { _page.get() }] (auto& result) {
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
    bool hadPendingAnimatedResize = _perProcessState.dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing;
    CGRect oldBounds = self.bounds;
    WebCore::FloatRect oldUnobscuredContentRect = _page->unobscuredContentRect();

    auto isOldBoundsValid = !CGRectIsEmpty(oldBounds) || !CGRectIsEmpty(_perProcessState.animatedResizeOldBounds);
    if (![self usesStandardContentView] || !_perProcessState.hasCommittedLoadForMainFrame || !isOldBoundsValid || oldUnobscuredContentRect.isEmpty() || _perProcessState.liveResizeParameters) {
        if ([_customContentView respondsToSelector:@selector(web_beginAnimatedResizeWithUpdates:)])
            [_customContentView web_beginAnimatedResizeWithUpdates:updateBlock];
        else
            updateBlock();
        return;
    }

    WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _beginAnimatedResizeWithUpdates:]", self, _page->identifier().toUInt64());

    _perProcessState.dynamicViewportUpdateMode = WebKit::DynamicViewportUpdateMode::ResizingWithAnimation;

    CGFloat oldMinimumEffectiveDeviceWidth;
    int32_t oldOrientation;
    UIEdgeInsets oldObscuredInsets;
    if (!CGRectIsEmpty(_perProcessState.animatedResizeOldBounds)) {
        oldBounds = _perProcessState.animatedResizeOldBounds;
        oldMinimumEffectiveDeviceWidth = _animatedResizeOldMinimumEffectiveDeviceWidth;
        oldOrientation = _animatedResizeOldOrientation;
        oldObscuredInsets = _animatedResizeOldObscuredInsets;
        _perProcessState.animatedResizeOldBounds = { };
    } else {
        oldMinimumEffectiveDeviceWidth = [self _minimumEffectiveDeviceWidth];
        oldOrientation = activeOrientation(self);
        oldObscuredInsets = _obscuredInsets;
    }
    auto oldViewLayoutSize = [self activeViewLayoutSize:oldBounds];
    auto oldMinimumUnobscuredSize = activeMinimumUnobscuredSize(self, oldBounds);
    auto oldMaximumUnobscuredSize = activeMaximumUnobscuredSize(self, oldBounds);

    updateBlock();

    CGRect newBounds = self.bounds;
    auto newMinimumEffectiveDeviceWidth = [self _minimumEffectiveDeviceWidth];
    auto newViewLayoutSize = [self activeViewLayoutSize:newBounds];
    auto newMinimumUnobscuredSize = activeMinimumUnobscuredSize(self, newBounds);
    auto newMaximumUnobscuredSize = activeMaximumUnobscuredSize(self, newBounds);
    int32_t newOrientation = activeOrientation(self);
    UIEdgeInsets newObscuredInsets = _obscuredInsets;
    CGRect futureUnobscuredRectInSelfCoordinates = UIEdgeInsetsInsetRect(newBounds, _obscuredInsets);
    CGRect contentViewBounds = [_contentView bounds];

    if (CGRectIsEmpty(newBounds) || newViewLayoutSize.isEmpty() || CGRectIsEmpty(futureUnobscuredRectInSelfCoordinates) || CGRectIsEmpty(contentViewBounds)) {
        if (!CGRectIsEmpty(newBounds))
            [self _cancelAnimatedResize];
        else {
            _perProcessState.animatedResizeOldBounds = oldBounds;
            _animatedResizeOldMinimumEffectiveDeviceWidth = oldMinimumEffectiveDeviceWidth;
            _animatedResizeOldOrientation = oldOrientation;
            _animatedResizeOldObscuredInsets = oldObscuredInsets;
            _perProcessState.waitingForCommitAfterAnimatedResize = YES;
            _perProcessState.waitingForEndAnimatedResize = YES;
        }

        [self _frameOrBoundsMayHaveChanged];
        if (_viewLayoutSizeOverride)
            [self _dispatchSetViewLayoutSize:newViewLayoutSize];
        if (_minimumUnobscuredSizeOverride)
            _page->setMinimumUnobscuredSize(WebCore::FloatSize(newMinimumUnobscuredSize));
        if (_maximumUnobscuredSizeOverride) {
            _page->setDefaultUnobscuredSize(WebCore::FloatSize(newMaximumUnobscuredSize));
            _page->setMaximumUnobscuredSize(WebCore::FloatSize(newMaximumUnobscuredSize));
        }
        if (_overridesInterfaceOrientation)
            [self _dispatchSetDeviceOrientation:newOrientation];

        return;
    }

    if (!hadPendingAnimatedResize
        && CGRectEqualToRect(oldBounds, newBounds)
        && oldViewLayoutSize == newViewLayoutSize
        && oldMinimumUnobscuredSize == newMinimumUnobscuredSize
        && oldMaximumUnobscuredSize == newMaximumUnobscuredSize
        && oldOrientation == newOrientation
        && oldMinimumEffectiveDeviceWidth == newMinimumEffectiveDeviceWidth
        && UIEdgeInsetsEqualToEdgeInsets(oldObscuredInsets, newObscuredInsets)) {
        [self _cancelAnimatedResize];
        return;
    }

    _resizeAnimationTransformAdjustments = CATransform3DIdentity;

    [self _ensureResizeAnimationView];

    CGSize contentSizeInContentViewCoordinates = contentViewBounds.size;
    [_scrollView setMinimumZoomScale:std::min(newViewLayoutSize.width() / contentSizeInContentViewCoordinates.width, [_scrollView minimumZoomScale])];
    [_scrollView setMaximumZoomScale:std::max(newViewLayoutSize.width() / contentSizeInContentViewCoordinates.width, [_scrollView maximumZoomScale])];

    // Compute the new scale to keep the current content width in the scrollview.
    CGFloat oldWebViewWidthInContentViewCoordinates = oldUnobscuredContentRect.width();
    _perProcessState.animatedResizeOriginalContentWidth = [&] {
#if HAVE(UIKIT_RESIZABLE_WINDOWS)
        if (self._isWindowResizingEnabled)
            return contentSizeInContentViewCoordinates.width;
#endif
        return std::min(contentSizeInContentViewCoordinates.width, oldWebViewWidthInContentViewCoordinates);
    }();
    CGFloat targetScale = newViewLayoutSize.width() / _perProcessState.animatedResizeOriginalContentWidth;
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

    _perProcessState.lastSentViewLayoutSize = newViewLayoutSize;
    _perProcessState.lastSentDeviceOrientation = newOrientation;

    _page->dynamicViewportSizeUpdate(newViewLayoutSize, newMinimumUnobscuredSize, newMaximumUnobscuredSize, visibleRectInContentCoordinates, unobscuredRectInContentCoordinates, futureUnobscuredRectInSelfCoordinates, unobscuredSafeAreaInsetsExtent, targetScale, newOrientation, newMinimumEffectiveDeviceWidth, ++_currentDynamicViewportSizeUpdateID);
    if (WebKit::DrawingAreaProxy* drawingArea = _page->drawingArea())
        drawingArea->setSize(WebCore::IntSize(newBounds.size));

    _perProcessState.waitingForCommitAfterAnimatedResize = YES;
    _perProcessState.waitingForEndAnimatedResize = YES;
}

- (void)_endAnimatedResize
{
    WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _endAnimatedResize] dynamicViewportUpdateMode %d", self, _page->identifier().toUInt64(), _perProcessState.dynamicViewportUpdateMode);

    // If we already have an up-to-date layer tree, immediately complete
    // the resize. Otherwise, we will defer completion until we do.
    _perProcessState.waitingForEndAnimatedResize = NO;
    if (!_perProcessState.waitingForCommitAfterAnimatedResize)
        [self _didCompleteAnimatedResize];
}

- (void)_resizeWhileHidingContentWithUpdates:(void (^)(void))updateBlock
{
    WKWEBVIEW_RELEASE_LOG("%p (pageProxyID=%llu) -[WKWebView _resizeWhileHidingContentWithUpdates:]", self, _page->identifier().toUInt64());

    [self _beginAnimatedResizeWithUpdates:updateBlock];
    if (_perProcessState.dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::ResizingWithAnimation) {
        [_contentView setHidden:YES];
        _perProcessState.dynamicViewportUpdateMode = WebKit::DynamicViewportUpdateMode::ResizingWithDocumentHidden;
        
        // _resizeWhileHidingContentWithUpdates is used by itself; the client will
        // not call endAnimatedResize, so we can't wait for it.
        _perProcessState.waitingForEndAnimatedResize = NO;
    }
}

- (void)_setSuppressSoftwareKeyboard:(BOOL)suppressSoftwareKeyboard
{
    super._suppressSoftwareKeyboard = suppressSoftwareKeyboard;
    [_contentView updateSoftwareKeyboardSuppressionStateFromWebView];
}

// FIXME (<rdar://problem/80986330>): This method should be updated to take an image
// width in points (for consistency) and a completion handler with a UIImage parameter
// (to avoid redundant copies for PDFs), once it is no longer in use by internal clients.
- (void)_snapshotRectAfterScreenUpdates:(BOOL)afterScreenUpdates rectInViewCoordinates:(CGRect)rectInViewCoordinates intoImageOfWidth:(CGFloat)imageWidth completionHandler:(void(^)(CGImageRef))completionHandler
{
    if (self._shouldDeferGeometryUpdates) {
        // Defer snapshotting until after the current resize completes.
        _callbacksDeferredDuringResize.append([retainedSelf = retainPtr(self), afterScreenUpdates, rectInViewCoordinates, imageWidth, completionHandler = makeBlockPtr(completionHandler)] {
            [retainedSelf _snapshotRectAfterScreenUpdates:afterScreenUpdates rectInViewCoordinates:rectInViewCoordinates intoImageOfWidth:imageWidth completionHandler:completionHandler.get()];
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
        TraceScope snapshotScope(RenderServerSnapshotStart, RenderServerSnapshotEnd);
        auto surface = WebCore::IOSurface::create(nullptr, WebCore::expandedIntSize(WebCore::FloatSize(imageSize)), WebCore::DestinationColorSpace::SRGB());
        if (!surface) {
            completionHandler(nullptr);
            return;
        }
        if (afterScreenUpdates)
            [CATransaction synchronize];
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

    _page->takeSnapshot(WebCore::enclosingIntRect(snapshotRectInContentCoordinates), WebCore::expandedIntSize(WebCore::FloatSize(imageSize)), WebKit::SnapshotOptionsExcludeDeviceScaleFactor, [completionHandler = makeBlockPtr(completionHandler)](const WebKit::ShareableBitmapHandle& imageHandle) {
        if (imageHandle.isNull())
            return completionHandler(nil);

        auto bitmap = WebKit::ShareableBitmap::create(imageHandle, WebKit::SharedMemory::Protection::ReadOnly);

        if (!bitmap)
            return completionHandler(nil);

        completionHandler(bitmap->makeCGImage().get());
    });
}

- (void)_snapshotRect:(CGRect)rectInViewCoordinates intoImageOfWidth:(CGFloat)imageWidth completionHandler:(void(^)(CGImageRef))completionHandler
{
    [self _snapshotRectAfterScreenUpdates:NO rectInViewCoordinates:rectInViewCoordinates intoImageOfWidth:imageWidth completionHandler:completionHandler];
}

- (void)_overrideLayoutParametersWithMinimumLayoutSize:(CGSize)minimumLayoutSize maximumUnobscuredSizeOverride:(CGSize)maximumUnobscuredSizeOverride
{
    LOG_WITH_STREAM(VisibleRects, stream << "-[WKWebView " << _page->identifier() << " _overrideLayoutParametersWithMinimumLayoutSize:" << WebCore::FloatSize(minimumLayoutSize) << " maximumUnobscuredSizeOverride:" << WebCore::FloatSize(maximumUnobscuredSizeOverride) << "]");

    if (minimumLayoutSize.width < 0 || minimumLayoutSize.height < 0)
        RELEASE_LOG_FAULT(VisibleRects, "%s: Error: attempting to override layout parameters with negative width or height: %@", __PRETTY_FUNCTION__, NSStringFromCGSize(minimumLayoutSize));

    [self _setViewLayoutSizeOverride:CGSizeMake(std::max<CGFloat>(0, minimumLayoutSize.width), std::max<CGFloat>(0, minimumLayoutSize.height))];
    [self _setMinimumUnobscuredSizeOverride:minimumLayoutSize];
    [self _setMaximumUnobscuredSizeOverride:maximumUnobscuredSizeOverride];
}

- (void)_clearOverrideLayoutParameters
{
    _viewLayoutSizeOverride = std::nullopt;
    _minimumUnobscuredSizeOverride = std::nullopt;
    _maximumUnobscuredSizeOverride = std::nullopt;
}

static std::optional<WebCore::ViewportArguments> viewportArgumentsFromDictionary(NSDictionary<NSString *, NSString *> *viewportArgumentPairs)
{
    if (!viewportArgumentPairs)
        return std::nullopt;

    WebCore::ViewportArguments viewportArguments(WebCore::ViewportArguments::ViewportMeta);

    [viewportArgumentPairs enumerateKeysAndObjectsUsingBlock:makeBlockPtr([&] (NSString *key, NSString *value, BOOL* stop) {
        if (![key isKindOfClass:[NSString class]] || ![value isKindOfClass:[NSString class]])
            [NSException raise:NSInvalidArgumentException format:@"-[WKWebView _overrideViewportWithArguments:]: Keys and values must all be NSStrings."];
        String keyString = key;
        String valueString = value;
        WebCore::setViewportFeature(viewportArguments, keyString, valueString, [] (WebCore::ViewportErrorCode, const String& errorMessage) {
            NSLog(@"-[WKWebView _overrideViewportWithArguments:]: Error parsing viewport argument: %s", errorMessage.utf8().data());
        });
    }).get()];

    return viewportArguments;
}

- (void)_overrideViewportWithArguments:(NSDictionary<NSString *, NSString *> *)arguments
{
    if (!_page)
        return;

    _page->setOverrideViewportArguments(viewportArgumentsFromDictionary(arguments));
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
    return adoptNS([[self] {
        --_activeFocusedStateRetainCount;
    } copy]).autorelease();
}

- (void)_becomeFirstResponderWithSelectionMovingForward:(BOOL)selectingForward completionHandler:(void (^)(BOOL didBecomeFirstResponder))completionHandler
{
    decltype(completionHandler) completionHandlerCopy = nil;
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
    [_contentView _accessibilityRetrieveRectsAtSelectionOffset:offset withText:text completionHandler:[capturedCompletionHandler = makeBlockPtr(completionHandler)] (const Vector<WebCore::SelectionGeometry>& selectionRects) {
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

- (void)_willOpenAppLink
{
    if (_page)
        _page->willOpenAppLink();
}

- (void)_isNavigatingToAppBoundDomain:(void(^)(BOOL))completionHandler
{
    _page->isNavigatingToAppBoundDomainTesting([completionHandler = makeBlockPtr(completionHandler)] (bool isAppBound) {
        completionHandler(isAppBound);
    });
}

- (void)_isForcedIntoAppBoundMode:(void(^)(BOOL))completionHandler
{
    _page->isForcedIntoAppBoundModeTesting([completionHandler = makeBlockPtr(completionHandler)] (bool isForcedIntoAppBoundMode) {
        completionHandler(isForcedIntoAppBoundMode);
    });
}

#if HAVE(UIFINDINTERACTION)

- (BOOL)_findInteractionEnabled
{
    return _findInteractionEnabled;
}

- (void)_setFindInteractionEnabled:(BOOL)enabled
{
    [self setFindInteractionEnabled:enabled];
}

- (_UIFindInteraction *)_findInteraction
{
    return (_UIFindInteraction *)_findInteraction.get();
}

- (UITextRange *)selectedTextRange
{
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition inDocument:(UITextSearchDocumentIdentifier)document
{
    return [_contentView offsetFromPosition:from toPosition:toPosition inDocument:document];
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (NSComparisonResult)compareFoundRange:(UITextRange *)fromRange toRange:(UITextRange *)toRange inDocument:(UITextSearchDocumentIdentifier)document
{
    return [_contentView compareFoundRange:fromRange toRange:toRange inDocument:document];
}

- (void)performTextSearchWithQueryString:(NSString *)string usingOptions:(UITextSearchOptions *)options resultAggregator:(id<UITextSearchAggregator>)aggregator
{
    [[self _searchableObject] performTextSearchWithQueryString:string usingOptions:options resultAggregator:aggregator];
}

- (void)replaceFoundTextInRange:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document withText:(NSString *)replacementText
{
    [_contentView replaceFoundTextInRange:range inDocument:document withText:replacementText];
}

- (void)decorateFoundTextRange:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document usingStyle:(UITextSearchFoundTextStyle)style
{
    [_contentView decorateFoundTextRange:range inDocument:document usingStyle:style];
}

- (void)scrollRangeToVisible:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document
{
    [_contentView scrollRangeToVisible:range inDocument:document];
}

- (void)clearAllDecoratedFoundText
{
    [_contentView clearAllDecoratedFoundText];
}

- (void)didBeginTextSearchOperation
{
    [_contentView didBeginTextSearchOperation];
}

- (void)didEndTextSearchOperation
{
    [_contentView didEndTextSearchOperation];
}

- (void)_requestRectForFoundTextRange:(UITextRange *)ranges completionHandler:(void (^)(CGRect rect))completionHandler
{
    [_contentView requestRectForFoundTextRange:ranges completionHandler:completionHandler];
}

- (UIFindSession *)findInteraction:(UIFindInteraction *)interaction sessionForView:(UIView *)view
{
    auto findSession = adoptNS([[UITextSearchingFindSession alloc] initWithSearchableObject:[self _searchableObject]]);
    return findSession.autorelease();
}

- (void)findInteraction:(UIFindInteraction *)interaction didBeginFindSession:(UIFindSession *)session
{
    if ([self _searchableObject] == _contentView.get())
        [_contentView didBeginTextSearchOperation];
}

- (void)findInteraction:(UIFindInteraction *)interaction didEndFindSession:(UIFindSession *)session
{
    if ([self _searchableObject] == _contentView.get())
        [_contentView didEndTextSearchOperation];
}

- (void)_addLayerForFindOverlay
{
    if (!_page || _perProcessState.pendingFindLayerID || _perProcessState.committedFindLayerID)
        return;

    _page->addLayerForFindOverlay([weakSelf = WeakObjCPtr<WKWebView>(self)] (WebCore::GraphicsLayer::PlatformLayerID layerID) {
        auto strongSelf = weakSelf.get();
        if (strongSelf)
            strongSelf->_perProcessState.pendingFindLayerID = layerID;
    });
}

- (void)_removeLayerForFindOverlay
{
    if (!_page)
        return;

    if (!_perProcessState.pendingFindLayerID && !_perProcessState.committedFindLayerID)
        return;

    _perProcessState.pendingFindLayerID = 0;
    _perProcessState.committedFindLayerID = 0;

    _page->removeLayerForFindOverlay([weakSelf = WeakObjCPtr<WKWebView>(self)] {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        if (auto* page = strongSelf->_page.get())
            page->findClient().didRemoveLayerForFindOverlay(page);
    });
}

- (CALayer *)_layerForFindOverlay
{
    if (!_page || !_perProcessState.committedFindLayerID)
        return nil;

    return downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).remoteLayerTreeHost().layerForID(_perProcessState.committedFindLayerID);
}

#endif // HAVE(UIFINDINTERACTION)

- (UITextInputTraits *)_textInputTraits
{
    if (self.usesStandardContentView)
        return [_contentView textInputTraitsForWebView];

    return nil;
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

#undef WKWEBVIEW_RELEASE_LOG

_WKTapHandlingResult wkTapHandlingResult(WebKit::TapHandlingResult result)
{
    switch (result) {
    case WebKit::TapHandlingResult::DidNotHandleTapAsClick:
        return _WKTapHandlingResultDidNotHandleTapAsClick;
    case WebKit::TapHandlingResult::NonMeaningfulClick:
        return _WKTapHandlingResultNonMeaningfulClick;
    case WebKit::TapHandlingResult::MeaningfulClick:
        return _WKTapHandlingResultMeaningfulClick;
    }
    ASSERT_NOT_REACHED();
    return _WKTapHandlingResultDidNotHandleTapAsClick;
}

#endif // PLATFORM(IOS_FAMILY)
