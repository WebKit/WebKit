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

#if PLATFORM(IOS)

#import "WKWebViewInternal.h"
#import <CoreGraphics/CGFloat.h>

@interface UIScrollView (UIScrollViewInternalHack)
- (CGFloat)_rubberBandOffsetForOffset:(CGFloat)newOffset maxOffset:(CGFloat)maxOffset minOffset:(CGFloat)minOffset range:(CGFloat)range outside:(BOOL *)outside;
@end

@interface WKScrollViewDelegateForwarder : NSObject <UIScrollViewDelegate>

- (instancetype)initWithInternalDelegate:(WKWebView *)internalDelegate externalDelegate:(id <UIScrollViewDelegate>)externalDelegate;

@end

@implementation WKScrollViewDelegateForwarder {
    WKWebView *_internalDelegate;
    id <UIScrollViewDelegate> _externalDelegate;
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
    NSMethodSignature *signature = [super methodSignatureForSelector:aSelector];
    if (!signature)
        signature = [(NSObject *)_internalDelegate methodSignatureForSelector:aSelector];
    if (!signature)
        signature = [(NSObject *)_externalDelegate methodSignatureForSelector:aSelector];
    return signature;
}

- (BOOL)respondsToSelector:(SEL)aSelector
{
    return [super respondsToSelector:aSelector] || [_internalDelegate respondsToSelector:aSelector] || [_externalDelegate respondsToSelector:aSelector];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    SEL aSelector = [anInvocation selector];
    BOOL internalDelegateWillRespond = [_internalDelegate respondsToSelector:aSelector];
    BOOL externalDelegateWillRespond = [_externalDelegate respondsToSelector:aSelector];

    if (internalDelegateWillRespond && externalDelegateWillRespond)
        [_internalDelegate _willInvokeUIScrollViewDelegateCallback];

    if (internalDelegateWillRespond)
        [anInvocation invokeWithTarget:_internalDelegate];
    if (externalDelegateWillRespond)
        [anInvocation invokeWithTarget:_externalDelegate];

    if (internalDelegateWillRespond && externalDelegateWillRespond)
        [_internalDelegate _didInvokeUIScrollViewDelegateCallback];

    if (!internalDelegateWillRespond && !externalDelegateWillRespond)
        [super forwardInvocation:anInvocation];
}

- (id)forwardingTargetForSelector:(SEL)aSelector
{
    BOOL internalDelegateWillRespond = [_internalDelegate respondsToSelector:aSelector];
    BOOL externalDelegateWillRespond = [_externalDelegate respondsToSelector:aSelector];

    if (internalDelegateWillRespond && !externalDelegateWillRespond)
        return _internalDelegate;
    if (externalDelegateWillRespond && !internalDelegateWillRespond)
        return _externalDelegate;
    return nil;
}

@end

@implementation WKScrollView {
    id <UIScrollViewDelegate> _externalDelegate;
    WKScrollViewDelegateForwarder *_delegateForwarder;
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
    if (_externalDelegate == delegate)
        return;
    _externalDelegate = delegate;
    [self _updateDelegate];
}

- (id <UIScrollViewDelegate>)delegate
{
    return _externalDelegate;
}

- (void)_updateDelegate
{
    WKScrollViewDelegateForwarder *oldForwarder = _delegateForwarder;
    _delegateForwarder = nil;
    if (!_externalDelegate)
        [super setDelegate:_internalDelegate];
    else if (!_internalDelegate)
        [super setDelegate:_externalDelegate];
    else {
        _delegateForwarder = [[WKScrollViewDelegateForwarder alloc] initWithInternalDelegate:_internalDelegate externalDelegate:_externalDelegate];
        [super setDelegate:_delegateForwarder];
    }
    [oldForwarder release];
}

- (void)dealloc
{
    [_delegateForwarder release];
    [super dealloc];
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

    [_internalDelegate _updateVisibleContentRects];
}

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

    if (CGSizeEqualToSize(currentContentSize, CGSizeZero) || CGSizeEqualToSize(currentContentSize, contentSize) || self.zoomScale < self.minimumZoomScale) {
        [self setContentSize:contentSize];
        return;
    }

    CGSize rubberbandAmount = [self _currentTopLeftRubberbandAmount];

    [self setContentSize:contentSize];

    if (!CGSizeEqualToSize(rubberbandAmount, CGSizeZero))
        [self _restoreContentOffsetWithRubberbandAmount:rubberbandAmount];
}

@end

#endif // PLATFORM(IOS)
