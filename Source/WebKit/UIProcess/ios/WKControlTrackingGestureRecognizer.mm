/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKControlTrackingGestureRecognizer.h"

#if ENABLE(CONTROL_TRACKING_REGIONS)

#import "UIKitUtilities.h"
#import <UIKit/UIGestureRecognizerSubclass.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

// The delay UIScrollView uses before passing touches to its content (delaysContentTouches).
static constexpr Seconds holdDuration = 150_ms;

// Same as UIPanGestureRecognizer's hysteresis, so this recognizer doesn't delay scrolling.
static constexpr CGFloat hysteresis = 10;

static bool canScroll(UIScrollView *scrollView, UIAxis axis)
{
    if (!scrollView.scrollEnabled)
        return false;
    if (axis == UIAxisHorizontal)
        return scrollView._wk_canScrollHorizontallyWithoutBouncing || (scrollView.bounces && scrollView.alwaysBounceHorizontal);
    return scrollView._wk_canScrollVerticallyWithoutBouncing || (scrollView.bounces && scrollView.alwaysBounceVertical);
}

static bool hasEnclosingScrollViewThatCanScroll(UIView *view, UIAxis axis)
{
    for (RetainPtr ancestor = view; ancestor; ancestor = [ancestor superview]) {
        if (RetainPtr scrollView = dynamic_objc_cast<UIScrollView>(ancestor.get()); scrollView && canScroll(scrollView.get(), axis))
            return true;
    }
    return false;
}

@implementation WKControlTrackingGestureRecognizer {
    std::optional<WebCore::ControlTrackingTarget> _controlTrackingTarget;
    WeakObjCPtr<UIView> _rootView;
    CGPoint _startLocationInRootView;
    CGPoint _startLocationInWindow;
    WeakObjCPtr<UIView> _touchedView;
    BOOL _isTap;
}

- (void)setControlTrackingTarget:(const WebCore::ControlTrackingTarget&)controlTrackingTarget rootView:(UIView *)rootView
{
    _controlTrackingTarget = controlTrackingTarget;
    _rootView = rootView;
}

- (std::optional<WebCore::ControlTrackingTarget>)controlTrackingTarget
{
    return _controlTrackingTarget;
}

- (CGPoint)startLocationInRootView
{
    return _startLocationInRootView;
}

- (CGPoint)locationInRootView
{
    return [self locationInView:protect(_rootView).get()];
}

- (BOOL)isTap
{
    return _isTap;
}

- (void)reset
{
    [super reset];

    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_holdDurationDidElapse) object:nil];
    _controlTrackingTarget = std::nullopt;
    _rootView = nil;
    _touchedView = nil;
    _isTap = NO;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesBegan:touches withEvent:event];

    RetainPtr<UITouch> touch = touches.anyObject;
    _startLocationInRootView = [touch locationInView:protect(_rootView).get()];
    _startLocationInWindow = [touch locationInView:nil];
    _touchedView = [touch view];
    [self performSelector:@selector(_holdDurationDidElapse) withObject:nil afterDelay:holdDuration.seconds() inModes:@[ NSRunLoopCommonModes ]];
}

- (void)_holdDurationDidElapse
{
    if (self.state == UIGestureRecognizerStatePossible)
        self.state = UIGestureRecognizerStateBegan;
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesMoved:touches withEvent:event];

    if (self.state == UIGestureRecognizerStateBegan || self.state == UIGestureRecognizerStateChanged) {
        self.state = UIGestureRecognizerStateChanged;
        return;
    }

    if (self.state != UIGestureRecognizerStatePossible)
        return;

    auto location = [touches.anyObject locationInView:nil];
    bool movedHorizontally = std::abs(location.x - _startLocationInWindow.x) > hysteresis;
    bool movedVertically = std::abs(location.y - _startLocationInWindow.y) > hysteresis;
    if (!movedHorizontally && !movedVertically)
        return;

    RetainPtr touchedView = _touchedView.get();
    bool movedAlongAxisNoScrollerCanScroll = (movedHorizontally && !hasEnclosingScrollViewThatCanScroll(touchedView.get(), UIAxisHorizontal))
        || (movedVertically && !hasEnclosingScrollViewThatCanScroll(touchedView.get(), UIAxisVertical));
    self.state = movedAlongAxisNoScrollerCanScroll ? UIGestureRecognizerStateBegan : UIGestureRecognizerStateFailed;
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesEnded:touches withEvent:event];

    // End rather than fail on a tap. The double-tap recognizers require this one to fail and would otherwise treat two taps on the switch as a double tap.
    _isTap = self.state == UIGestureRecognizerStatePossible;
    self.state = UIGestureRecognizerStateEnded;
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesCancelled:touches withEvent:event];

    self.state = UIGestureRecognizerStateCancelled;
}

@end

#endif // ENABLE(CONTROL_TRACKING_REGIONS)
