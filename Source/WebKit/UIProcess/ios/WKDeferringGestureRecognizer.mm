/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "WKDeferringGestureRecognizer.h"

#if PLATFORM(IOS_FAMILY)

#import <wtf/WeakObjCPtr.h>

@implementation WKDeferringGestureRecognizer {
    WeakObjCPtr<id <WKDeferringGestureRecognizerDelegate>> _deferringGestureDelegate;
}

- (instancetype)initWithDeferringGestureDelegate:(id <WKDeferringGestureRecognizerDelegate>)deferringGestureDelegate
{
    if (self = [super init])
        _deferringGestureDelegate = deferringGestureDelegate;
    return self;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    auto shouldDeferGestures = [_deferringGestureDelegate.get() deferringGestureRecognizer:self willBeginTouchesWithEvent:event];
    [super touchesBegan:touches withEvent:event];

    if (shouldDeferGestures == WebKit::ShouldDeferGestures::No)
        self.state = UIGestureRecognizerStateFailed;
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesEnded:touches withEvent:event];

    if (self.immediatelyFailsAfterTouchEnd)
        self.state = UIGestureRecognizerStateFailed;

    [_deferringGestureDelegate.get() deferringGestureRecognizer:self didEndTouchesWithEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesCancelled:touches withEvent:event];
    self.state = UIGestureRecognizerStateFailed;
}

- (void)endDeferral:(WebKit::ShouldPreventGestures)shouldPreventGestures
{
    if (shouldPreventGestures == WebKit::ShouldPreventGestures::Yes)
        self.state = UIGestureRecognizerStateEnded;
    else
        self.state = UIGestureRecognizerStateFailed;
}

- (BOOL)canBePreventedByGestureRecognizer:(UIGestureRecognizer *)preventingGestureRecognizer
{
    return NO;
}

- (BOOL)shouldDeferGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    return [_deferringGestureDelegate.get() deferringGestureRecognizer:self shouldDeferOtherGestureRecognizer:gestureRecognizer];
}

- (void)setState:(UIGestureRecognizerState)state
{
    auto previousState = self.state;
    [super setState:state];

    if (previousState != self.state)
        [_deferringGestureDelegate.get() deferringGestureRecognizer:self didTransitionToState:state];
}

@end

#endif // PLATFORM(IOS_FAMILY)
