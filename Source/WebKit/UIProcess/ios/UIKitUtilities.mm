/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "UIKitUtilities.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import <WebCore/FloatPoint.h>

#if HAVE(UI_SCROLL_VIEW_TRANSFERS_SCROLLING_TO_PARENT)

@interface UIScrollView (Staging_112474630)
@property (nonatomic) BOOL transfersHorizontalScrollingToParent;
@property (nonatomic) BOOL transfersVerticalScrollingToParent;
@end

#endif // HAVE(UI_SCROLL_VIEW_TRANSFERS_SCROLLING_TO_PARENT)

#if HAVE(UI_SCROLL_VIEW_APIS_ADDED_IN_RADAR_112474145)

@interface UIScrollView (Staging_112474145)
@property (nonatomic, readonly, getter=isScrollAnimating) BOOL scrollAnimating;
@property (nonatomic, readonly, getter=isZoomAnimating) BOOL zoomAnimating;
- (void)stopScrollingAndZooming;
@end

#endif // HAVE(UI_SCROLL_VIEW_APIS_ADDED_IN_RADAR_112474145)

@interface UIScrollView (Staging_116693098)
- (void)showScrollIndicatorsForContentOffsetChanges:(void (NS_NOESCAPE ^)(void))changes;
@end

@implementation UIScrollView (WebKitInternal)

- (BOOL)_wk_isInterruptingDeceleration
{
    return self.decelerating && self.tracking;
}

- (CGFloat)_wk_contentWidthIncludingInsets
{
    auto inset = self.adjustedContentInset;
    return self.contentSize.width + inset.left + inset.right;
}

- (CGFloat)_wk_contentHeightIncludingInsets
{
    auto inset = self.adjustedContentInset;
    return self.contentSize.height + inset.top + inset.bottom;
}

- (BOOL)_wk_isScrolledBeyondExtents
{
    auto contentOffset = self.contentOffset;
    auto inset = self.adjustedContentInset;
    if (contentOffset.x < -inset.left || contentOffset.y < -inset.top)
        return YES;

    auto contentSize = self.contentSize;
    auto boundsSize = self.bounds.size;
    auto maxScrollExtent = CGPointMake(
        contentSize.width + inset.right - std::min<CGFloat>(boundsSize.width, self._wk_contentWidthIncludingInsets),
        contentSize.height + inset.bottom - std::min<CGFloat>(boundsSize.height, self._wk_contentHeightIncludingInsets)
    );
    return contentOffset.x > maxScrollExtent.x || contentOffset.y > maxScrollExtent.y;
}

- (BOOL)_wk_isScrollAnimating
{
#if HAVE(UI_SCROLL_VIEW_APIS_ADDED_IN_RADAR_112474145)
    static BOOL hasScrollAnimating = [UIScrollView instancesRespondToSelector:@selector(isScrollAnimating)];
    return hasScrollAnimating && self.scrollAnimating;
#else
    return self.isAnimatingScroll;
#endif
}

- (BOOL)_wk_isZoomAnimating
{
#if HAVE(UI_SCROLL_VIEW_APIS_ADDED_IN_RADAR_112474145)
    static BOOL hasZoomAnimating = [UIScrollView instancesRespondToSelector:@selector(isZoomAnimating)];
    return hasZoomAnimating && self.zoomAnimating;
#else
    return self.isAnimatingZoom;
#endif
}

- (void)_wk_stopScrollingAndZooming
{
#if HAVE(UI_SCROLL_VIEW_APIS_ADDED_IN_RADAR_112474145)
    static BOOL hasStopScrollingAndZooming = [UIScrollView instancesRespondToSelector:@selector(stopScrollingAndZooming)];
    if (hasStopScrollingAndZooming)
        [self stopScrollingAndZooming];
#else
    [self _stopScrollingAndZoomingAnimations];
#endif
}

- (CGPoint)_wk_clampToScrollExtents:(CGPoint)contentOffset
{
    // See also: -[UIScrollView _adjustedContentOffsetForContentOffset:].
    auto bounds = CGRect { contentOffset, self.bounds.size };
    auto effectiveInset = self.adjustedContentInset;

    if (!self.zoomBouncing && self.zooming)
        return contentOffset;

    auto contentMinX = -effectiveInset.left;
    auto contentMinY = -effectiveInset.top;
    auto contentSize = self.contentSize;
    auto contentMaxX = contentSize.width + effectiveInset.right;
    auto contentMaxY = contentSize.height + effectiveInset.bottom;

    if (CGRectGetWidth(bounds) >= self._wk_contentWidthIncludingInsets || CGRectGetMinX(bounds) < contentMinX)
        contentOffset.x = contentMinX;
    else if (CGRectGetMaxX(bounds) > contentMaxX)
        contentOffset.x = contentMaxX - CGRectGetWidth(bounds);

    if (CGRectGetHeight(bounds) >= self._wk_contentHeightIncludingInsets || CGRectGetMinY(bounds) < contentMinY)
        contentOffset.y = contentMinY;
    else if (CGRectGetMaxY(bounds) > contentMaxY)
        contentOffset.y = contentMaxY - CGRectGetHeight(bounds);

    return contentOffset;
}

// Consistent with the value of `_UISmartEpsilon` in UIKit.
static constexpr auto epsilonForComputingScrollability = 0.0001;

- (BOOL)_wk_canScrollHorizontallyWithoutBouncing
{
    return self._wk_contentWidthIncludingInsets - CGRectGetWidth(self.bounds) > epsilonForComputingScrollability;
}

- (BOOL)_wk_canScrollVerticallyWithoutBouncing
{
    return self._wk_contentHeightIncludingInsets - CGRectGetHeight(self.bounds) > epsilonForComputingScrollability;
}

- (void)_wk_setTransfersHorizontalScrollingToParent:(BOOL)value
{
#if HAVE(UI_SCROLL_VIEW_TRANSFERS_SCROLLING_TO_PARENT)
    static BOOL hasAPI = [UIScrollView instancesRespondToSelector:@selector(setTransfersHorizontalScrollingToParent:)];
    if (hasAPI) {
        self.transfersHorizontalScrollingToParent = value;
        return;
    }
#endif
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    self._allowsParentToBeginHorizontally = value;
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)_wk_setTransfersVerticalScrollingToParent:(BOOL)value
{
#if HAVE(UI_SCROLL_VIEW_TRANSFERS_SCROLLING_TO_PARENT)
    static BOOL hasAPI = [UIScrollView instancesRespondToSelector:@selector(setTransfersVerticalScrollingToParent:)];
    if (hasAPI) {
        self.transfersVerticalScrollingToParent = value;
        return;
    }
#endif
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    self._allowsParentToBeginVertically = value;
    ALLOW_DEPRECATED_DECLARATIONS_END
}

static UIAxis axesForDelta(WebCore::FloatSize delta)
{
    UIAxis axes = UIAxisNeither;
    if (delta.width())
        axes |= UIAxisHorizontal;
    if (delta.height())
        axes |= UIAxisVertical;
    return axes;
}

- (void)_wk_setContentOffsetAndShowScrollIndicators:(CGPoint)offset animated:(BOOL)animated
{
    static bool hasAPI = [UIScrollView instancesRespondToSelector:@selector(showScrollIndicatorsForContentOffsetChanges:)];
    if (hasAPI) {
        [self showScrollIndicatorsForContentOffsetChanges:[animated, offset, self] {
            [self setContentOffset:offset animated:animated];
        }];
        return;
    }

    [self setContentOffset:offset animated:animated];

    auto axes = axesForDelta(WebCore::FloatPoint(self.contentOffset) - WebCore::FloatPoint(offset));
    [self _flashScrollIndicatorsForAxes:axes persistingPreviousFlashes:YES];
}

@end

@implementation UIView (WebKitInternal)

- (UIScrollView *)_wk_parentScrollView
{
    for (RetainPtr parent = [self superview]; parent; parent = [parent superview]) {
        if (RetainPtr scrollView = dynamic_objc_cast<UIScrollView>(parent.get()))
            return scrollView.get();
    }
    return nil;
}

- (UIViewController *)_wk_viewControllerForFullScreenPresentation
{
    auto controller = self.window.rootViewController;
    UIViewController *nextPresentedController = nil;
    while ((nextPresentedController = controller.presentedViewController))
        controller = nextPresentedController;
    return controller.viewIfLoaded.window == self.window ? controller : nil;
}

@end

@implementation UIViewController (WebKitInternal)

- (BOOL)_wk_isInFullscreenPresentation
{
    return self.activePresentationController && self.modalPresentationStyle == UIModalPresentationFullScreen;
}

@end

@implementation UIGestureRecognizer (WebKitInternal)

- (BOOL)_wk_isTextInteractionLoupeGesture
{
    return [self.name isEqualToString:@"UITextInteractionNameInteractiveRefinement"];
}

- (BOOL)_wk_isTextInteractionTapGesture
{
    return [self.name isEqualToString:@"UITextInteractionNameSingleTap"];
}

- (BOOL)_wk_hasRecognizedOrEnded
{
    switch (self.state) {
    case UIGestureRecognizerStateBegan:
    case UIGestureRecognizerStateChanged:
    case UIGestureRecognizerStateEnded:
        return YES;
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
        return NO;
    }
    ASSERT_NOT_REACHED();
    return NO;
}

@end

namespace WebKit {

RetainPtr<UIAlertController> createUIAlertController(NSString *title, NSString *message)
{
    // FIXME: revisit this once UIKit bug is resolved (see also rdar://101140177).
    auto alertTitle = adoptNS([[NSMutableAttributedString alloc] initWithString:title]);
    [alertTitle addAttribute:NSFontAttributeName value:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline] range:NSMakeRange(0, title.length)];

    auto alert = adoptNS([[UIAlertController alloc] init]);
    alert.get().attributedTitle = alertTitle.get();
    alert.get().title = title;
    alert.get().message = message;
    alert.get().preferredStyle = UIAlertControllerStyleAlert;
    return alert;
}

UIScrollView *scrollViewForTouches(NSSet<UITouch *> *touches)
{
    for (UITouch *touch in touches) {
        if (auto scrollView = dynamic_objc_cast<UIScrollView>(touch.view))
            return scrollView;
    }
    return nil;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
