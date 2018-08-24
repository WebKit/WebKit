/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "WKKeyboardScrollingAnimator.h"

#if PLATFORM(IOS)

#import "UIKitSPI.h"
#import <WebCore/FloatPoint.h>
#import <WebCore/WebEvent.h>
#import <algorithm>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/RetainPtr.h>

namespace WebKit {

enum class KeyboardScrollingAnimatorState : uint8_t {
    WaitingForFirstEvent,
    WaitingForRepeat,
    Animating
};

};

@implementation WKKeyboardScrollingAnimator {
    id <WKKeyboardScrollable> _scrollable;
    CFTimeInterval _startTime;
    WebCore::FloatPoint _scrollOffsetPerIncrement;
    RetainPtr<CADisplayLink> _displayLink;
    WebKit::KeyboardScrollingAnimatorState _state;
}

- (instancetype)init
{
    return nil;
}

- (instancetype)initWithScrollable:(id <WKKeyboardScrollable>)scrollable
{
    self = [super init];
    if (!self)
        return nil;

    _scrollable = scrollable;

    return self;
}

- (void)invalidate
{
    [self stopAnimatedScroll];
    _scrollable = nil;
}

- (std::optional<WebCore::FloatPoint>)_scrollOffsetForEvent:(::WebEvent *)event
{
    static const unsigned kWebSpaceKey = 0x20;

    if (![_scrollable isKeyboardScrollable])
        return std::nullopt;

    NSString *charactersIgnoringModifiers = event.charactersIgnoringModifiers;
    if (!charactersIgnoringModifiers.length)
        return std::nullopt;

    enum class Direction : uint8_t { Up, Down, Left, Right };
    enum class Key : uint8_t { Other, LeftArrow, RightArrow, UpArrow, DownArrow, PageUp, PageDown, Space };
    
    auto key = ^{
        if ([charactersIgnoringModifiers isEqualToString:UIKeyInputLeftArrow])
            return Key::LeftArrow;
        if ([charactersIgnoringModifiers isEqualToString:UIKeyInputRightArrow])
            return Key::RightArrow;
        if ([charactersIgnoringModifiers isEqualToString:UIKeyInputUpArrow])
            return Key::UpArrow;
        if ([charactersIgnoringModifiers isEqualToString:UIKeyInputDownArrow])
            return Key::DownArrow;
        if ([charactersIgnoringModifiers isEqualToString:UIKeyInputPageDown])
            return Key::PageDown;
        if ([charactersIgnoringModifiers isEqualToString:UIKeyInputPageUp])
            return Key::PageUp;
        if ([charactersIgnoringModifiers characterAtIndex:0] == kWebSpaceKey)
            return Key::Space;
        return Key::Other;
    }();
    
    if (key == Key::Other)
        return std::nullopt;
    
    BOOL shiftPressed = event.modifierFlags & WebEventFlagMaskShift;
    BOOL altPressed = event.modifierFlags & WebEventFlagMaskAlternate;
    BOOL cmdPressed = event.modifierFlags & WebEventFlagMaskCommand;
    
    auto increment = ^{
        switch (key) {
        case Key::LeftArrow:
        case Key::RightArrow:
            return WebKit::ScrollingIncrement::Line;
        case Key::UpArrow:
        case Key::DownArrow:
            if (altPressed)
                return WebKit::ScrollingIncrement::Page;
            if (cmdPressed)
                return WebKit::ScrollingIncrement::Document;
            return WebKit::ScrollingIncrement::Line;
        case Key::PageUp:
        case Key::PageDown:
        case Key::Space:
            return WebKit::ScrollingIncrement::Page;
        case Key::Other:
            ASSERT_NOT_REACHED();
            return WebKit::ScrollingIncrement::Line;
        };
    }();

    auto direction = ^() {
        switch (key) {
        case Key::LeftArrow:
            return Direction::Left;
        case Key::RightArrow:
            return Direction::Right;
        case Key::UpArrow:
        case Key::PageUp:
            return Direction::Up;
        case Key::DownArrow:
        case Key::PageDown:
            return Direction::Down;
        case Key::Space:
            return shiftPressed ? Direction::Up : Direction::Down;
        case Key::Other:
            ASSERT_NOT_REACHED();
            return Direction::Down;
        };
    }();

    bool isHorizontal = direction == Direction::Left || direction == Direction::Right;
    CGFloat scrollDistance = [_scrollable distanceForScrollingIncrement:increment];
    
    if (direction == Direction::Up || direction == Direction::Left)
        scrollDistance = -scrollDistance;
    
    return isHorizontal ? WebCore::FloatPoint(scrollDistance, 0) : WebCore::FloatPoint(0, scrollDistance);
}

- (BOOL)beginWithEvent:(::WebEvent *)event
{
    if (_state != WebKit::KeyboardScrollingAnimatorState::WaitingForFirstEvent)
        return NO;
    
    if (event.type != WebEventKeyDown)
        return NO;

    auto offset = [self _scrollOffsetForEvent:event];
    if (!offset)
        return NO;

    _state = WebKit::KeyboardScrollingAnimatorState::WaitingForRepeat;
    _scrollOffsetPerIncrement = offset.value();

    // The first keyboard event that starts scrolling performs its own
    // discrete animated scroll. Continuously animated scrolling starts
    // when the key repeats.
    [_scrollable scrollByContentOffset:_scrollOffsetPerIncrement animated:YES];

    return YES;
}

- (BOOL)handleKeyEvent:(::WebEvent *)event
{
    if (_state == WebKit::KeyboardScrollingAnimatorState::WaitingForFirstEvent)
        return NO;

    auto offset = [self _scrollOffsetForEvent:event];
    if (!offset || event.type == WebEventKeyUp) {
        [self stopAnimatedScroll];
        return NO;
    }

    if (_state == WebKit::KeyboardScrollingAnimatorState::WaitingForRepeat)
        [self startAnimatedScroll];

    return YES;
}

- (void)startAnimatedScroll
{
    ASSERT(!_displayLink);

    _state = WebKit::KeyboardScrollingAnimatorState::Animating;
    _startTime = CACurrentMediaTime();
    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkFired:)];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)stopAnimatedScroll
{
    _state = WebKit::KeyboardScrollingAnimatorState::WaitingForFirstEvent;
    [_displayLink invalidate];
    _displayLink = nil;
}

- (void)displayLinkFired:(CADisplayLink *)sender
{
    const CFTimeInterval secondsPerScrollIncrement = 0.05; // Seconds it should take to cover one increment when at full speed.
    const float maximumVelocity = 2000; // Maximum velocity in pixels per second. Empirically determined.
    const CFTimeInterval accelerationDuration = 0.3; // Duration of acceleration in seconds. This matches UIScrollView.

    auto velocity = _scrollOffsetPerIncrement.scaled(1. / secondsPerScrollIncrement);
    velocity = velocity.constrainedBetween({ -maximumVelocity, -maximumVelocity }, { maximumVelocity, maximumVelocity });

    CFTimeInterval frameDuration = sender.targetTimestamp - sender.timestamp;
    CFTimeInterval timeFromAnimationStart = sender.timestamp - _startTime;
    float accelerationFactor = [[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut] _solveForInput:std::min<CFTimeInterval>(timeFromAnimationStart, accelerationDuration) / accelerationDuration];

    auto contentOffset = velocity.scaled(accelerationFactor * frameDuration);
    [_scrollable scrollByContentOffset:contentOffset animated:NO];
}

@end

#endif // PLATFORM(IOS)
