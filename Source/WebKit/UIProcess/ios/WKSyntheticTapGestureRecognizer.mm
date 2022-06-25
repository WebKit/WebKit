/*
 * Copyright (C) 2014 - 2019 Apple Inc. All rights reserved.
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
#import "WKSyntheticTapGestureRecognizer.h"

#if PLATFORM(IOS_FAMILY)

#import <UIKit/UIGestureRecognizerSubclass.h>
#import <wtf/RetainPtr.h>

@implementation WKSyntheticTapGestureRecognizer {
    __weak id _gestureIdentifiedTarget;
    SEL _gestureIdentifiedAction;
    __weak id _gestureFailedTarget;
    SEL _gestureFailedAction;
    __weak id _resetTarget;
    SEL _resetAction;
    RetainPtr<NSNumber> _lastActiveTouchIdentifier;
    __weak UIScrollView *_lastTouchedScrollView;
}

- (void)setGestureIdentifiedTarget:(id)target action:(SEL)action
{
    _gestureIdentifiedTarget = target;
    _gestureIdentifiedAction = action;
}

- (void)setGestureFailedTarget:(id)target action:(SEL)action
{
    _gestureFailedTarget = target;
    _gestureFailedAction = action;
}

- (void)setResetTarget:(id)target action:(SEL)action
{
    _resetTarget = target;
    _resetAction = action;
}

- (void)setState:(UIGestureRecognizerState)state
{
    if (state == UIGestureRecognizerStateEnded)
        [_gestureIdentifiedTarget performSelector:_gestureIdentifiedAction withObject:self];
    else if (state == UIGestureRecognizerStateFailed)
        [_gestureFailedTarget performSelector:_gestureFailedAction withObject:self];
    [super setState:state];
}

- (void)reset
{
    [super reset];
    [_resetTarget performSelector:_resetAction withObject:self];
    _lastActiveTouchIdentifier = nil;
    _lastTouchedScrollView = nil;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesBegan:touches withEvent:event];

    for (UITouch *touch in touches) {
        if ([touch.view isKindOfClass:UIScrollView.class]) {
            _lastTouchedScrollView = (UIScrollView *)touch.view;
            break;
        }
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesEnded:touches withEvent:event];
    if (!_supportingWebTouchEventsGestureRecognizer)
        return;

    NSMapTable<NSNumber *, UITouch *> *activeTouches = [_supportingWebTouchEventsGestureRecognizer activeTouchesByIdentifier];
    for (NSNumber *touchIdentifier in activeTouches) {
        UITouch *touch = [activeTouches objectForKey:touchIdentifier];
        if ([touch.gestureRecognizers containsObject:self]) {
            _lastActiveTouchIdentifier = touchIdentifier;
            break;
        }
    }
}

- (UIScrollView *)lastTouchedScrollView
{
    return _lastTouchedScrollView;
}

- (NSNumber*)lastActiveTouchIdentifier
{
    return _lastActiveTouchIdentifier.get();
}

@end

#endif
