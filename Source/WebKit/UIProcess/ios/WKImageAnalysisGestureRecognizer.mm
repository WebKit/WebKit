/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WKImageAnalysisGestureRecognizer.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(IMAGE_ANALYSIS)

#import "UIKitUtilities.h"
#import <wtf/WeakObjCPtr.h>

@implementation WKImageAnalysisGestureRecognizer {
    __weak UIView <WKImageAnalysisGestureRecognizerDelegate> *_imageAnalysisGestureRecognizerDelegate;
    WeakObjCPtr<UIScrollView> _lastTouchedScrollView;
}

- (instancetype)initWithImageAnalysisGestureDelegate:(UIView <WKImageAnalysisGestureRecognizerDelegate> *)delegate
{
    if (!(self = [super init]))
        return nil;

    _imageAnalysisGestureRecognizerDelegate = delegate;
    self.delegate = delegate;
    self.minimumPressDuration = 0.1;
    self.allowableMovement = 0;
    self.name = @"Image analysis";
    return self;
}

- (void)reset
{
    [super reset];

    _lastTouchedScrollView = nil;
}

- (UIScrollView *)lastTouchedScrollView
{
    return _lastTouchedScrollView.getAutoreleased();
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesBegan:touches withEvent:event];

    if (RetainPtr scrollView = WebKit::scrollViewForTouches(touches))
        _lastTouchedScrollView = scrollView.get();

    [self beginAfterExceedingForceThresholdIfNeeded:touches];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesMoved:touches withEvent:event];

    [self beginAfterExceedingForceThresholdIfNeeded:touches];
}

- (void)beginAfterExceedingForceThresholdIfNeeded:(NSSet<UITouch *> *)touches
{
    if (self.state != UIGestureRecognizerStatePossible)
        return;

    if (touches.count > 1)
        return;

    constexpr CGFloat forceThreshold = 1.5;
    if (touches.anyObject.force < forceThreshold)
        return;

    self.state = UIGestureRecognizerStateBegan;
}

- (void)setState:(UIGestureRecognizerState)state
{
    auto previousState = self.state;
    super.state = state;

    auto newState = self.state;
    if (previousState == newState)
        return;

    if (newState == UIGestureRecognizerStateBegan)
        [_imageAnalysisGestureRecognizerDelegate imageAnalysisGestureDidBegin:self];
    else if (newState == UIGestureRecognizerStateFailed)
        [_imageAnalysisGestureRecognizerDelegate imageAnalysisGestureDidFail:self];
}

@end

#endif // PLATFORM(IOS_FAMILY) && ENABLE(IMAGE_ANALYSIS)
