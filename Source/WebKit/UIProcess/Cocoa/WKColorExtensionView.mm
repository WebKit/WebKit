/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "WKColorExtensionView.h"

#import <QuartzCore/CoreAnimation.h>
#import <wtf/RetainPtr.h>

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)

@interface WKColorExtensionView () <CAAnimationDelegate>
@end

@implementation WKColorExtensionView {
    BOOL _isVisible;
    __weak id<WKColorExtensionViewDelegate> _delegate;
    RetainPtr<WebCore::CocoaColor> _targetColor;
}

- (instancetype)initWithFrame:(CGRect)frame delegate:(id<WKColorExtensionViewDelegate>)delegate
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _delegate = delegate;
    self.hidden = YES;
    return self;
}

- (void)updateColor:(WebCore::CocoaColor *)color
{
    [self _updateColor:color visible:YES];
}

- (void)fadeOut
{
    [self _updateColor:[WebCore::CocoaColor clearColor] visible:NO];
}

- (void)_updateColor:(WebCore::CocoaColor *)color visible:(BOOL)visible
{
    if (!visible && !_isVisible)
        return;

    BOOL isBeingRevealed = self.hidden && visible;
    if (isBeingRevealed)
        [self cancelFadeAnimation];

    BOOL wasVisible = std::exchange(_isVisible, visible);
    if (wasVisible && !visible)
        [retainPtr(_delegate) colorExtensionViewWillDisappear:self];

    if (visible)
        self.hidden = NO;

    static constexpr auto animationDuration = 0.1;

    RetainPtr fromColor = [self.layer backgroundColor];
    RetainPtr toColor = [color CGColor];
    _targetColor = color;
    self.layer.backgroundColor = toColor.get();

    if (isBeingRevealed) {
        [retainPtr(_delegate) colorExtensionViewDidAppear:self];
        return;
    }

    RetainPtr animation = [CABasicAnimation animationWithKeyPath:@"backgroundColor"];
    [animation setFromValue:(__bridge id)fromColor.get()];
    [animation setToValue:(__bridge id)toColor.get()];
    [animation setDuration:animationDuration];
    [animation setFillMode:kCAFillModeForwards];
    [animation setRemovedOnCompletion:NO];
    [animation setDelegate:self];

    [self.layer addAnimation:animation.get() forKey:@"WKColorExtensionViewFade"];
}

- (void)animationDidStop:(CAAnimation *)animation finished:(BOOL)finished
{
    if (!finished)
        return;

    if (!_isVisible)
        self.hidden = YES;
}

- (BOOL)isHiddenOrFadingOut
{
    return self.hidden || !_isVisible;
}

- (void)cancelFadeAnimation
{
    if (!_targetColor)
        return;

    RetainPtr layer = [self layer];
    [layer removeAnimationForKey:@"WKColorExtensionViewFade"];
    [layer setBackgroundColor:RetainPtr { [_targetColor CGColor] }.get()];
    self.hidden = !_isVisible;
}

@end

#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)
