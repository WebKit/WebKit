/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#import "WKScrollView.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKDeferringGestureRecognizer.h"
#import "WKWebViewIOS.h"
#import <WebCore/VersionChecks.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/WeakObjCPtr.h>

#if HAVE(PEPPER_UI_CORE)
#import "PepperUICoreSPI.h"
#endif

@interface WKScrollViewDelegateForwarder : NSObject <UIScrollViewDelegate>

- (instancetype)initWithInternalDelegate:(WKWebView *)internalDelegate externalDelegate:(id <UIScrollViewDelegate>)externalDelegate;

@end

@implementation WKScrollViewDelegateForwarder {
    WKWebView *_internalDelegate;
    WeakObjCPtr<id <UIScrollViewDelegate>> _externalDelegate;
}

- (instancetype)initWithInternalDelegate:(WKWebView <UIScrollViewDelegate> *)internalDelegate externalDelegate:(id <UIScrollViewDelegate>)externalDelegate
{
    self = [super init];
    if (!self)
        return nil;
    _internalDelegate = internalDelegate;
    _externalDelegate = externalDelegate;
    return self;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    auto externalDelegate = _externalDelegate.get();
    NSMethodSignature *signature = [super methodSignatureForSelector:aSelector];
    if (!signature)
        signature = [(NSObject *)_internalDelegate methodSignatureForSelector:aSelector];
    if (!signature)
        signature = [(NSObject *)externalDelegate methodSignatureForSelector:aSelector];
    return signature;
}

- (BOOL)respondsToSelector:(SEL)aSelector
{
    return [super respondsToSelector:aSelector] || [_internalDelegate respondsToSelector:aSelector] || [_externalDelegate.get() respondsToSelector:aSelector];
}

static BOOL shouldForwardScrollViewDelegateMethodToExternalDelegate(SEL selector)
{
    // We cannot forward viewForZoomingInScrollView: to the external delegate, because WebKit
    // owns the content of the scroll view, and depends on viewForZoomingInScrollView being the
    // content view. Any other view returned by the external delegate will break our behavior.
    if (sel_isEqual(selector, @selector(viewForZoomingInScrollView:)))
        return NO;

    return YES;
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    auto externalDelegate = _externalDelegate.get();
    SEL aSelector = [anInvocation selector];
    BOOL internalDelegateWillRespond = [_internalDelegate respondsToSelector:aSelector];
    BOOL externalDelegateWillRespond = shouldForwardScrollViewDelegateMethodToExternalDelegate(aSelector) && [externalDelegate respondsToSelector:aSelector];

    if (internalDelegateWillRespond && externalDelegateWillRespond)
        [_internalDelegate _willInvokeUIScrollViewDelegateCallback];

    if (internalDelegateWillRespond)
        [anInvocation invokeWithTarget:_internalDelegate];
    if (externalDelegateWillRespond)
        [anInvocation invokeWithTarget:externalDelegate.get()];

    if (internalDelegateWillRespond && externalDelegateWillRespond)
        [_internalDelegate _didInvokeUIScrollViewDelegateCallback];

    if (!internalDelegateWillRespond && !externalDelegateWillRespond)
        [super forwardInvocation:anInvocation];
}

- (id)forwardingTargetForSelector:(SEL)aSelector
{
    BOOL internalDelegateWillRespond = [_internalDelegate respondsToSelector:aSelector];
    BOOL externalDelegateWillRespond = shouldForwardScrollViewDelegateMethodToExternalDelegate(aSelector) && [_externalDelegate.get() respondsToSelector:aSelector];

    if (internalDelegateWillRespond && !externalDelegateWillRespond)
        return _internalDelegate;
    if (externalDelegateWillRespond && !internalDelegateWillRespond)
        return _externalDelegate.getAutoreleased();
    return nil;
}

@end

@implementation WKScrollView {
    WeakObjCPtr<id <UIScrollViewDelegate>> _externalDelegate;
    RetainPtr<WKScrollViewDelegateForwarder> _delegateForwarder;

// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    BOOL _contentInsetAdjustmentBehaviorWasExternallyOverridden;
#endif
    CGFloat _keyboardBottomInsetAdjustment;
    BOOL _scrollEnabledByClient;
    BOOL _scrollEnabledInternal;
    BOOL _zoomEnabledByClient;
    BOOL _zoomEnabledInternal;
}

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];

    if (!self)
        return nil;

    _scrollEnabledByClient = YES;
    _scrollEnabledInternal = YES;
    _zoomEnabledByClient = YES;
    _zoomEnabledInternal = YES;

    self.alwaysBounceVertical = YES;
    self.directionalLockEnabled = YES;
    [self _setIndicatorInsetAdjustmentBehavior:UIScrollViewIndicatorInsetAdjustmentAlways];

// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    _contentInsetAdjustmentBehaviorWasExternallyOverridden = (self.contentInsetAdjustmentBehavior != UIScrollViewContentInsetAdjustmentAutomatic);
#endif
    
#if HAVE(PEPPER_UI_CORE)
    [self _configureDigitalCrownScrolling];
#endif

    return self;
}

- (void)setInternalDelegate:(WKWebView <UIScrollViewDelegate> *)internalDelegate
{
    if (internalDelegate == _internalDelegate)
        return;
    _internalDelegate = internalDelegate;
    [self _updateDelegate];
}

- (void)setDelegate:(id <UIScrollViewDelegate>)delegate
{
    if (_externalDelegate.get().get() == delegate)
        return;
    _externalDelegate = delegate;
    [self _updateDelegate];
}

- (id <UIScrollViewDelegate>)delegate
{
    return _externalDelegate.getAutoreleased();
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ([otherGestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return [(WKDeferringGestureRecognizer *)otherGestureRecognizer shouldDeferGestureRecognizer:gestureRecognizer];

    return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return [(WKDeferringGestureRecognizer *)gestureRecognizer shouldDeferGestureRecognizer:otherGestureRecognizer];

    return NO;
}

- (void)_updateDelegate
{
    auto oldForwarder = std::exchange(_delegateForwarder, nil);
    auto externalDelegate = _externalDelegate.get();
    if (!externalDelegate)
        [super setDelegate:_internalDelegate];
    else if (!_internalDelegate)
        [super setDelegate:externalDelegate.get()];
    else {
        _delegateForwarder = adoptNS([[WKScrollViewDelegateForwarder alloc] initWithInternalDelegate:_internalDelegate externalDelegate:externalDelegate.get()]);
        [super setDelegate:_delegateForwarder.get()];
    }
}

static inline bool valuesAreWithinOnePixel(CGFloat a, CGFloat b)
{
    return CGFAbs(a - b) < 1;
}

- (CGFloat)_rubberBandOffsetForOffset:(CGFloat)newOffset maxOffset:(CGFloat)maxOffset minOffset:(CGFloat)minOffset range:(CGFloat)range outside:(BOOL *)outside
{
    UIEdgeInsets contentInsets = self.contentInset;
    CGSize contentSize = self.contentSize;
    CGRect bounds = self.bounds;

    CGFloat minimalHorizontalRange = bounds.size.width - contentInsets.left - contentInsets.right;
    CGFloat contentWidthAtMinimumScale = contentSize.width * (self.minimumZoomScale / self.zoomScale);
    if (contentWidthAtMinimumScale < minimalHorizontalRange) {
        CGFloat unobscuredEmptyHorizontalMarginAtMinimumScale = minimalHorizontalRange - contentWidthAtMinimumScale;
        minimalHorizontalRange -= unobscuredEmptyHorizontalMarginAtMinimumScale;
    }
    if (contentSize.width < minimalHorizontalRange) {
        if (valuesAreWithinOnePixel(minOffset, -contentInsets.left)
            && valuesAreWithinOnePixel(maxOffset, contentSize.width + contentInsets.right - bounds.size.width)
            && valuesAreWithinOnePixel(range, bounds.size.width)) {

            CGFloat emptyHorizontalMargin = (minimalHorizontalRange - contentSize.width) / 2;
            minOffset -= emptyHorizontalMargin;
            maxOffset = minOffset;
        }
    }

    CGFloat minimalVerticalRange = bounds.size.height - contentInsets.top - contentInsets.bottom;
    CGFloat contentHeightAtMinimumScale = contentSize.height * (self.minimumZoomScale / self.zoomScale);
    if (contentHeightAtMinimumScale < minimalVerticalRange) {
        CGFloat unobscuredEmptyVerticalMarginAtMinimumScale = minimalVerticalRange - contentHeightAtMinimumScale;
        minimalVerticalRange -= unobscuredEmptyVerticalMarginAtMinimumScale;
    }
    if (contentSize.height < minimalVerticalRange) {
        if (valuesAreWithinOnePixel(minOffset, -contentInsets.top)
            && valuesAreWithinOnePixel(maxOffset, contentSize.height + contentInsets.bottom - bounds.size.height)
            && valuesAreWithinOnePixel(range, bounds.size.height)) {

            CGFloat emptyVerticalMargin = (minimalVerticalRange - contentSize.height) / 2;
            minOffset -= emptyVerticalMargin;
            maxOffset = minOffset;
        }
    }

    return [super _rubberBandOffsetForOffset:newOffset maxOffset:maxOffset minOffset:minOffset range:range outside:outside];
}

- (void)setContentInset:(UIEdgeInsets)contentInset
{
    [super setContentInset:contentInset];

    [_internalDelegate _scheduleVisibleContentRectUpdate];
}

// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

- (BOOL)_contentInsetAdjustmentBehaviorWasExternallyOverridden
{
    return _contentInsetAdjustmentBehaviorWasExternallyOverridden;
}

- (void)setContentInsetAdjustmentBehavior:(UIScrollViewContentInsetAdjustmentBehavior)insetAdjustmentBehavior
{
    _contentInsetAdjustmentBehaviorWasExternallyOverridden = YES;

    if ([self contentInsetAdjustmentBehavior] == insetAdjustmentBehavior)
        return;

    [super setContentInsetAdjustmentBehavior:insetAdjustmentBehavior];
    [_internalDelegate _scheduleVisibleContentRectUpdate];
}

- (void)_setContentInsetAdjustmentBehaviorInternal:(UIScrollViewContentInsetAdjustmentBehavior)insetAdjustmentBehavior
{
    if ([self contentInsetAdjustmentBehavior] == insetAdjustmentBehavior)
        return;

    [super setContentInsetAdjustmentBehavior:insetAdjustmentBehavior];
}

#endif

// Fetch top/left rubberband amounts (as negative values).
- (CGSize)_currentTopLeftRubberbandAmount
{
    UIEdgeInsets edgeInsets = [self contentInset];

    CGSize rubberbandAmount = CGSizeZero;

    CGPoint contentOffset = [self contentOffset];
    if (contentOffset.x < -edgeInsets.left)
        rubberbandAmount.width = std::min<CGFloat>(contentOffset.x + -edgeInsets.left, 0);

    if (contentOffset.y < -edgeInsets.top)
        rubberbandAmount.height = std::min<CGFloat>(contentOffset.y + edgeInsets.top, 0);
    
    return rubberbandAmount;
}

- (void)_restoreContentOffsetWithRubberbandAmount:(CGSize)rubberbandAmount
{
    UIEdgeInsets edgeInsets = [self contentInset];
    CGPoint adjustedOffset = [self contentOffset];

    if (rubberbandAmount.width < 0)
        adjustedOffset.x = -edgeInsets.left + rubberbandAmount.width;

    if (rubberbandAmount.height < 0)
        adjustedOffset.y = -edgeInsets.top + rubberbandAmount.height;

    [self setContentOffset:adjustedOffset];
}

- (void)_setContentSizePreservingContentOffsetDuringRubberband:(CGSize)contentSize
{
    CGSize currentContentSize = [self contentSize];

    BOOL mightBeRubberbanding = self.isDragging || self.isVerticalBouncing || self.isHorizontalBouncing;
    if (!mightBeRubberbanding || CGSizeEqualToSize(currentContentSize, CGSizeZero) || CGSizeEqualToSize(currentContentSize, contentSize) || self.zoomScale < self.minimumZoomScale) {
        // FIXME: rdar://problem/65277759 Find out why iOS Mail needs this call even when the contentSize has not changed.
        [self setContentSize:contentSize];
        return;
    }

    CGSize rubberbandAmount = [self _currentTopLeftRubberbandAmount];

    [self setContentSize:contentSize];

    if (!CGSizeEqualToSize(rubberbandAmount, CGSizeZero))
        [self _restoreContentOffsetWithRubberbandAmount:rubberbandAmount];
}

- (void)_adjustForAutomaticKeyboardInfo:(NSDictionary *)info animated:(BOOL)animated lastAdjustment:(CGFloat*)lastAdjustment
{
    _keyboardBottomInsetAdjustment = [[UIPeripheralHost sharedInstance] getVerticalOverlapForView:self usingKeyboardInfo:info];

    [super _adjustForAutomaticKeyboardInfo:info animated:animated lastAdjustment:lastAdjustment];
}

- (UIEdgeInsets)_systemContentInset
{
    UIEdgeInsets systemContentInset = [super _systemContentInset];

    // Internal clients who use setObscuredInsets include the keyboard height in their
    // manually overridden insets, so we don't need to re-add it here.
    if (_internalDelegate._haveSetObscuredInsets)
        return systemContentInset;

    // Match the inverse of the condition that UIScrollView uses to decide whether
    // to include keyboard insets in the systemContentInset. We always want
    // keyboard insets applied, even when web content has chosen to disable automatic
    // safe area inset adjustment.
    if (linkedOnOrAfter(WebCore::SDKVersion::FirstWhereUIScrollViewDoesNotApplyKeyboardInsetsUnconditionally) && self.contentInsetAdjustmentBehavior == UIScrollViewContentInsetAdjustmentNever)
        systemContentInset.bottom += _keyboardBottomInsetAdjustment;

    return systemContentInset;
}

- (void)setScrollEnabled:(BOOL)value
{
    _scrollEnabledByClient = value;
    [self _updateScrollability];
}

- (void)_setScrollEnabledInternal:(BOOL)value
{
    _scrollEnabledInternal = value;
    [self _updateScrollability];
}

- (void)_updateScrollability
{
    [super setScrollEnabled:(_scrollEnabledByClient && _scrollEnabledInternal)];
}

- (void)setZoomEnabled:(BOOL)value
{
    _zoomEnabledByClient = value;
    [self _updateZoomability];
}

- (void)_setZoomEnabledInternal:(BOOL)value
{
    _zoomEnabledInternal = value;
    [self _updateZoomability];
}

- (void)_updateZoomability
{
    [super setZoomEnabled:(_zoomEnabledByClient && _zoomEnabledInternal)];
}

#if PLATFORM(WATCHOS)

- (void)addGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    [super addGestureRecognizer:gestureRecognizer];

    if (gestureRecognizer == self.pinchGestureRecognizer)
        gestureRecognizer.allowedTouchTypes = @[];
}

#endif // PLATFORM(WATCHOS)

#if HAVE(PEPPER_UI_CORE)

- (void)_configureDigitalCrownScrolling
{
    self.showsVerticalScrollIndicator = NO;
    self.crownInputScrollDirection = PUICCrownInputScrollDirectionVertical;
}

- (CGPoint)_puic_contentOffsetForCrownInputSequencerOffset:(double)sequencerOffset
{
    CGPoint targetOffset = [super _puic_contentOffsetForCrownInputSequencerOffset:sequencerOffset];
    auto scrollDirection = self.puic_crownInputScrollDirection;
    if (scrollDirection == PUICCrownInputScrollDirectionVertical)
        targetOffset.x = self.contentOffset.x;
    else if (scrollDirection == PUICCrownInputScrollDirectionHorizontal)
        targetOffset.y = self.contentOffset.y;
    return targetOffset;
}

#endif // HAVE(PEPPER_UI_CORE)

@end

#endif // PLATFORM(IOS_FAMILY)
