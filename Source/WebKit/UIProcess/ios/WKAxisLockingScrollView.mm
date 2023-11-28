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
#import "WKAxisLockingScrollView.h"

#if PLATFORM(IOS_FAMILY)

#include <wtf/RetainPtr.h>

@interface UIScrollView (GestureRecognizerDelegate) <UIGestureRecognizerDelegate>
@end

@implementation WKAxisLockingScrollView {
    RetainPtr<UIPanGestureRecognizer> _axisLockingPanGestureRecognizer;
    UIAxis _axesToPreventMomentumScrolling;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _axesToPreventMomentumScrolling = UIAxisNeither;
    [self.panGestureRecognizer addTarget:self action:@selector(_updatePanGestureToPreventScrolling)];
    return self;
}

- (void)addGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    if (self.panGestureRecognizer == gestureRecognizer) {
        if (!_axisLockingPanGestureRecognizer) {
            _axisLockingPanGestureRecognizer = adoptNS([[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(_updatePanGestureToPreventScrolling)]);
            [_axisLockingPanGestureRecognizer setName:@"Scroll axis locking"];
            [_axisLockingPanGestureRecognizer setDelegate:self];
        }
        [self addGestureRecognizer:_axisLockingPanGestureRecognizer.get()];
    }

    [super addGestureRecognizer:gestureRecognizer];
}

- (void)removeGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    if (self.panGestureRecognizer == gestureRecognizer) {
        if (auto gesture = std::exchange(_axisLockingPanGestureRecognizer, nil))
            [self removeGestureRecognizer:gesture.get()];
    }

    [super removeGestureRecognizer:gestureRecognizer];
}

- (void)_updatePanGestureToPreventScrolling
{
    auto panGesture = self.panGestureRecognizer;
    switch (self.panGestureRecognizer.state) {
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
        return;
    case UIGestureRecognizerStateBegan:
    case UIGestureRecognizerStateChanged:
        break;
    }

    auto axesToPrevent = self._axesToPreventScrollingFromDelegate;
    if (axesToPrevent == UIAxisNeither)
        return;

    auto adjustedTranslation = [panGesture translationInView:nil];
    bool translationChanged = false;
    if ((axesToPrevent & UIAxisHorizontal) && std::abs(adjustedTranslation.x) > CGFLOAT_EPSILON) {
        adjustedTranslation.x = 0;
        _axesToPreventMomentumScrolling |= UIAxisHorizontal;
        translationChanged = true;
    }

    if ((axesToPrevent & UIAxisVertical) && std::abs(adjustedTranslation.y) > CGFLOAT_EPSILON) {
        adjustedTranslation.y = 0;
        _axesToPreventMomentumScrolling |= UIAxisVertical;
        translationChanged = true;
    }

    if (translationChanged)
        [panGesture setTranslation:adjustedTranslation inView:nil];
}

- (UIAxis)_axesToPreventScrollingFromDelegate
{
    auto delegate = self.scrollAxisLockingDelegate;
    return delegate ? [delegate axesToPreventScrollingForPanGestureInScrollView:self] : UIAxisBoth;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if (gestureRecognizer == _axisLockingPanGestureRecognizer || otherGestureRecognizer == _axisLockingPanGestureRecognizer)
        return YES;

    static BOOL callIntoSuperclass = [UIScrollView instancesRespondToSelector:@selector(gestureRecognizer:shouldRecognizeSimultaneouslyWithGestureRecognizer:)];
    if (!callIntoSuperclass)
        return NO;

    return [super gestureRecognizer:gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:otherGestureRecognizer];
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    if (self.panGestureRecognizer == gestureRecognizer)
        _axesToPreventMomentumScrolling = UIAxisNeither;

    static BOOL callIntoSuperclass = [UIScrollView instancesRespondToSelector:@selector(gestureRecognizerShouldBegin:)];
    if (!callIntoSuperclass)
        return YES;

    return [super gestureRecognizerShouldBegin:gestureRecognizer];
}

@end

#endif // PLATFORM(IOS_FAMILY)
