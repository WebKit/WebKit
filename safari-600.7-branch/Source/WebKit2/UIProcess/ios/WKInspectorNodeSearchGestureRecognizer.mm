/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKInspectorNodeSearchGestureRecognizer.h"

#if PLATFORM(IOS)

#import <UIKit/UIGestureRecognizerSubclass.h>
#import <wtf/RetainPtr.h>

@implementation WKInspectorNodeSearchGestureRecognizer {
    RetainPtr<UITouch> _touch;
}

- (CGPoint)locationInView:(UIView *)view
{
    return [_touch locationInView:view];
}

- (void)_processTouches:(NSSet *)touches state:(UIGestureRecognizerState)newState
{
    ASSERT(_touch);
    if ([touches containsObject:_touch.get()])
        self.state = newState;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    if (self.state != UIGestureRecognizerStatePossible)
        return;

    ASSERT(!_touch);
    _touch = [touches anyObject];

    [self _processTouches:touches state:UIGestureRecognizerStateBegan];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self _processTouches:touches state:UIGestureRecognizerStateChanged];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self _processTouches:touches state:UIGestureRecognizerStateEnded];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self _processTouches:touches state:UIGestureRecognizerStateCancelled];
}

- (void)reset
{
    _touch = nil;
}

@end

#endif
