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
#import "TestContextMenuDriver.h"

#import "UIKitSPI.h"
#import <wtf/BlockPtr.h>

#if PLATFORM(IOS) && USE(UICONTEXTMENU)

@implementation TestContextMenuDriver

- (id<_UIClickInteractionDriverDelegate>)delegate
{
    return _delegate.get().get();
}

- (void)setDelegate:(id<_UIClickInteractionDriverDelegate>)delegate
{
    _delegate = delegate;
}

- (UIView *)view
{
    return _view.get().get();
}

- (void)setView:(UIView *)view
{
    _view = view;
}

- (CGFloat)allowableMovement
{
    return _allowableMovement;
}

- (void)setAllowableMovement:(CGFloat)allowableMovement
{
    _allowableMovement = allowableMovement;
}

- (UIGestureRecognizer *)primaryGestureRecognizer
{
    return _primaryGestureRecognizer.get();
}

- (void)setPrimaryGestureRecognizer:(UIGestureRecognizer *)primaryGestureRecognizer
{
    _primaryGestureRecognizer = primaryGestureRecognizer;
}

- (NSTimeInterval)touchDuration
{
    return _touchDuration;
}

- (void)setTouchDuration:(NSTimeInterval)touchDuration
{
    _touchDuration = touchDuration;
}

- (CGPoint)locationInCoordinateSpace:(id <UICoordinateSpace>)coordinateSpace
{
    return CGPointZero;
}

- (void)cancelInteraction
{
}

- (void)begin:(void(^)(BOOL))completionHandler
{
    auto completionBlock = makeBlockPtr(completionHandler);
    [self.delegate clickDriver:(id<_UIClickInteractionDriving>)self shouldBegin:^(BOOL result) {
        [self.delegate clickDriver:(id<_UIClickInteractionDriving>)self didPerformEvent:_UIClickInteractionEventBegan];
        completionBlock(result);
    }];
}

- (void)clickDown
{
    [self.delegate clickDriver:(id<_UIClickInteractionDriving>)self didPerformEvent:_UIClickInteractionEventClickedDown];
}

- (void)clickUp
{
    [self.delegate clickDriver:(id<_UIClickInteractionDriving>)self didPerformEvent:_UIClickInteractionEventClickedUp];
}

- (void)end
{
    [self.delegate clickDriver:(id<_UIClickInteractionDriving>)self didPerformEvent:_UIClickInteractionEventEnded];
}

@end

#endif
