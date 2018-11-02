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

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import <QuartzCore/CADisplayLink.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/KeyEventCodesIOS.h>
#import <WebCore/RectEdges.h>
#import <WebCore/WebEvent.h>
#import <WebKit/UIKitSPI.h>
#import <algorithm>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

namespace WebKit {

enum class ScrollingDirection : uint8_t { Up, Down, Left, Right };

struct KeyboardScroll {
    WebCore::FloatSize offset; // Points per increment.
    WebCore::FloatSize maximumVelocity; // Points per second.
    WebCore::FloatSize force;

    WebKit::ScrollingIncrement increment;
    WebKit::ScrollingDirection direction;
};

struct KeyboardScrollParameters {
    CGFloat springMass { 1 };
    CGFloat springStiffness { 109 };
    CGFloat springDamping { 20 };

    CGFloat maximumVelocityMultiplier { 25 };
    CGFloat timeToMaximumVelocity { 1 };

    CGFloat rubberBandForce { 5000 };
};

}

@protocol WKKeyboardScrollableInternal <NSObject>
@required
- (BOOL)isKeyboardScrollable;
- (CGFloat)distanceForIncrement:(WebKit::ScrollingIncrement)increment;
- (void)scrollToContentOffset:(WebCore::FloatPoint)offset animated:(BOOL)animated;
- (void)scrollWithScrollToExtentAnimationTo:(CGPoint)offset;
- (CGPoint)contentOffset;
- (CGSize)interactiveScrollVelocity;
- (CGPoint)boundedContentOffset:(CGPoint)offset;
- (WebCore::RectEdges<bool>)scrollableDirectionsFromOffset:(CGPoint)offset;
- (WebCore::RectEdges<bool>)rubberbandableDirections;
- (void)didFinishScrolling;

@end

@interface WKKeyboardScrollingAnimator : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithScrollable:(id <WKKeyboardScrollableInternal>)scrollable;

- (void)invalidate;

- (void)willStartInteractiveScroll;

- (BOOL)beginWithEvent:(::WebEvent *)event;
- (void)handleKeyEvent:(::WebEvent *)event;

@end

@implementation WKKeyboardScrollingAnimator {
    id <WKKeyboardScrollableInternal> _scrollable;
    RetainPtr<CADisplayLink> _displayLink;

    std::optional<WebKit::KeyboardScroll> _currentScroll;

    BOOL _hasPressedScrollingKey;

    WebCore::FloatSize _velocity; // Points per second.

    WebCore::FloatPoint _idealPosition;
    WebCore::FloatPoint _currentPosition;
    WebCore::FloatPoint _idealPositionForMinimumTravel;
}

- (instancetype)init
{
    return nil;
}

- (instancetype)initWithScrollable:(id <WKKeyboardScrollableInternal>)scrollable
{
    self = [super init];
    if (!self)
        return nil;

    _scrollable = scrollable;

    return self;
}

- (const WebKit::KeyboardScrollParameters &)parameters
{
    static const WebKit::KeyboardScrollParameters parameters;
    return parameters;
}

- (void)invalidate
{
    [self stopAnimatedScroll];
    [self stopDisplayLink];
    _scrollable = nil;
}

static WebCore::FloatSize unitVector(WebKit::ScrollingDirection direction)
{
    switch (direction) {
    case WebKit::ScrollingDirection::Up:
        return { 0, -1 };
    case WebKit::ScrollingDirection::Down:
        return { 0, 1 };
    case WebKit::ScrollingDirection::Left:
        return { -1, 0 };
    case WebKit::ScrollingDirection::Right:
        return { 1, 0 };
    }
}

static WebCore::FloatSize perpendicularAbsoluteUnitVector(WebKit::ScrollingDirection direction)
{
    switch (direction) {
    case WebKit::ScrollingDirection::Up:
    case WebKit::ScrollingDirection::Down:
        return { 1, 0 };
    case WebKit::ScrollingDirection::Left:
    case WebKit::ScrollingDirection::Right:
        return { 0, 1 };
    }
}

static WebCore::PhysicalBoxSide boxSide(WebKit::ScrollingDirection direction)
{
    switch (direction) {
    case WebKit::ScrollingDirection::Up:
        return WebCore::PhysicalBoxSide::Top;
    case WebKit::ScrollingDirection::Down:
        return WebCore::PhysicalBoxSide::Bottom;
    case WebKit::ScrollingDirection::Left:
        return WebCore::PhysicalBoxSide::Left;
    case WebKit::ScrollingDirection::Right:
        return WebCore::PhysicalBoxSide::Right;
    }
}

- (std::optional<WebKit::KeyboardScroll>)keyboardScrollForEvent:(::WebEvent *)event
{
    static const unsigned kWebSpaceKey = 0x20;

    if (![_scrollable isKeyboardScrollable])
        return std::nullopt;

    NSString *charactersIgnoringModifiers = event.charactersIgnoringModifiers;
    if (!charactersIgnoringModifiers.length)
        return std::nullopt;

    enum class Key : uint8_t { Other, LeftArrow, RightArrow, UpArrow, DownArrow, PageUp, PageDown, Space };
    
    auto key = ^{
        auto firstCharacter = [charactersIgnoringModifiers characterAtIndex:0];
        switch (firstCharacter) {
        case NSLeftArrowFunctionKey:
            return Key::LeftArrow;
        case NSRightArrowFunctionKey:
            return Key::RightArrow;
        case NSUpArrowFunctionKey:
            return Key::UpArrow;
        case NSDownArrowFunctionKey:
            return Key::DownArrow;
        case NSPageDownFunctionKey:
            return Key::PageDown;
        case NSPageUpFunctionKey:
            return Key::PageUp;
        case kWebSpaceKey:
            return Key::Space;
        default:
            return Key::Other;
        };
    }();
    
    if (key == Key::Other)
        return std::nullopt;
    
    BOOL shiftPressed = event.modifierFlags & WebEventFlagMaskShiftKey;
    BOOL altPressed = event.modifierFlags & WebEventFlagMaskOptionKey;
    BOOL cmdPressed = event.modifierFlags & WebEventFlagMaskCommandKey;

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
            return WebKit::ScrollingDirection::Left;
        case Key::RightArrow:
            return WebKit::ScrollingDirection::Right;
        case Key::UpArrow:
        case Key::PageUp:
            return WebKit::ScrollingDirection::Up;
        case Key::DownArrow:
        case Key::PageDown:
            return WebKit::ScrollingDirection::Down;
        case Key::Space:
            return shiftPressed ? WebKit::ScrollingDirection::Up : WebKit::ScrollingDirection::Down;
        case Key::Other:
            ASSERT_NOT_REACHED();
            return WebKit::ScrollingDirection::Down;
        };
    }();

    CGFloat scrollDistance = [_scrollable distanceForIncrement:increment];

    WebKit::KeyboardScroll scroll;
    scroll.offset = unitVector(direction).scaled(scrollDistance);
    scroll.increment = increment;
    scroll.direction = direction;
    scroll.maximumVelocity = scroll.offset.scaled(self.parameters.maximumVelocityMultiplier);

    // Apply a constant force to achieve Vmax in timeToMaximumVelocity seconds.
    // F_constant = m * Vmax / t
    scroll.force = scroll.maximumVelocity.scaled(self.parameters.springMass / self.parameters.timeToMaximumVelocity);
    
    return scroll;
}

- (BOOL)beginWithEvent:(::WebEvent *)event
{
    if (event.type != WebEventKeyDown)
        return NO;

    auto scroll = [self keyboardScrollForEvent:event];
    if (!scroll)
        return NO;

    if (_hasPressedScrollingKey)
        return NO;

    if (![_scrollable rubberbandableDirections].at(boxSide(scroll->direction)))
        return NO;

    _hasPressedScrollingKey = YES;
    _currentScroll = scroll;

    if (scroll->increment == WebKit::ScrollingIncrement::Document) {
        _velocity = { };
        [self stopAnimatedScroll];
        [self stopDisplayLink];
        [_scrollable scrollWithScrollToExtentAnimationTo:[_scrollable boundedContentOffset:_currentPosition + scroll->offset]];
        return YES;
    }

    [self startDisplayLinkIfNeeded];

    _currentPosition = WebCore::FloatPoint([_scrollable contentOffset]);
    _velocity += WebCore::FloatSize([_scrollable interactiveScrollVelocity]);
    _idealPositionForMinimumTravel = _currentPosition + _currentScroll->offset;

    return YES;
}

- (void)handleKeyEvent:(::WebEvent *)event
{
    if (!_hasPressedScrollingKey)
        return;

    auto scroll = [self keyboardScrollForEvent:event];
    if (!scroll || event.type == WebEventKeyUp) {
        [self stopAnimatedScroll];
        _hasPressedScrollingKey = NO;
    }
}

static WebCore::FloatPoint farthestPointInDirection(WebCore::FloatPoint a, WebCore::FloatPoint b, WebKit::ScrollingDirection direction)
{
    switch (direction) {
    case WebKit::ScrollingDirection::Up:
        return WebCore::FloatPoint(a.x(), std::min(a.y(), b.y()));
    case WebKit::ScrollingDirection::Down:
        return WebCore::FloatPoint(a.x(), std::max(a.y(), b.y()));
    case WebKit::ScrollingDirection::Left:
        return WebCore::FloatPoint(std::min(a.x(), b.x()), a.y());
    case WebKit::ScrollingDirection::Right:
        return WebCore::FloatPoint(std::max(a.x(), b.x()), a.y());
    }

    ASSERT_NOT_REACHED();
    return { };
}

- (void)stopAnimatedScroll
{
    if (!_currentScroll)
        return;

    // Determine the settling position of the spring, conserving the system's current energy.
    // Kinetic = elastic potential
    // 1/2 * m * v^2 = 1/2 * k * x^2
    // x = sqrt(v^2 * m / k)
    auto displacementMagnitudeSquared = (_velocity * _velocity).scaled(self.parameters.springMass / self.parameters.springStiffness);
    WebCore::FloatSize displacement = {
        std::copysign(sqrt(displacementMagnitudeSquared.width()), _velocity.width()),
        std::copysign(sqrt(displacementMagnitudeSquared.height()), _velocity.height())
    };

    // If the spring would settle before the minimum travel distance
    // for an instantaneous tap, move the settling position of the spring
    // out to that point.
    _idealPosition = [_scrollable boundedContentOffset:farthestPointInDirection(_currentPosition + displacement, _idealPositionForMinimumTravel, _currentScroll->direction)];

    _currentScroll = std::nullopt;
}

- (void)startDisplayLinkIfNeeded
{
    if (_displayLink)
        return;

    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkFired:)];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)stopDisplayLink
{
    [_displayLink invalidate];
    _displayLink = nil;
}

- (void)willStartInteractiveScroll
{
    // If the user touches the screen to start an interactive scroll, stop everything.
    _velocity = { };
    [self stopAnimatedScroll];
    [self stopDisplayLink];
}

- (void)displayLinkFired:(CADisplayLink *)sender
{
    WebCore::FloatSize force;
    WebCore::FloatSize axesToApplySpring = { 1, 1 };

    if (_currentScroll) {
        auto scrollableDirections = [_scrollable scrollableDirectionsFromOffset:_currentPosition];
        auto direction = _currentScroll->direction;

        if (scrollableDirections.at(boxSide(direction))) {
            // Apply the scrolling force. Only apply the spring in the perpendicular axis,
            // otherwise it drags against the direction of motion.
            axesToApplySpring = perpendicularAbsoluteUnitVector(direction);
            force = _currentScroll->force;
        } else {
            // The scroll view cannot scroll in this direction, and is rubber-banding.
            // Apply a constant and significant force; otherwise, the force for a
            // single-line increment is not strong enough to rubber-band perceptibly.
            force = unitVector(direction).scaled(self.parameters.rubberBandForce);
        }

        // If we've reached or exceeded the maximum velocity, stop applying any force.
        // However, we won't let the spring snap, we'll just keep going at the same
        // velocity until the user raises their finger or we hit an edge.
        if (fabs(_velocity.width()) >= fabs(_currentScroll->maximumVelocity.width()))
            force.setWidth(0);
        if (fabs(_velocity.height()) >= fabs(_currentScroll->maximumVelocity.height()))
            force.setHeight(0);
    }

    WebCore::FloatPoint idealPosition = [_scrollable boundedContentOffset:_currentScroll ? _currentPosition : _idealPosition];
    WebCore::FloatSize displacement = _currentPosition - idealPosition;

    // Compute the spring's force, and apply it in allowed directions.
    // F_spring = -k * x - c * v
    auto springForce = - displacement.scaled(self.parameters.springStiffness) - _velocity.scaled(self.parameters.springDamping);
    force += springForce * axesToApplySpring;

    // Integrate acceleration -> velocity -> position for this time step.
    CFTimeInterval frameDuration = sender.targetTimestamp - sender.timestamp;
    WebCore::FloatSize acceleration = force.scaled(1. / self.parameters.springMass);
    _velocity += acceleration.scaled(frameDuration);
    _currentPosition += _velocity.scaled(frameDuration);

    [_scrollable scrollToContentOffset:_currentPosition animated:NO];

    // If we've effectively stopped scrolling, and no key is pressed,
    // shut down the display link.
    if (!_hasPressedScrollingKey && _velocity.diagonalLengthSquared() < 1) {
        [_scrollable didFinishScrolling];
        [self stopDisplayLink];
        _velocity = { };
    }
}

@end

@interface WKKeyboardScrollViewAnimator () <WKKeyboardScrollableInternal>
@end

@implementation WKKeyboardScrollViewAnimator {
    WeakObjCPtr<UIScrollView> _scrollView;
    RetainPtr<WKKeyboardScrollingAnimator> _animator;

    BOOL _delegateRespondsToIsKeyboardScrollable;
    BOOL _delegateRespondsToDistanceForIncrement;
    BOOL _delegateRespondsToWillScroll;
    BOOL _delegateRespondsToDidFinishScrolling;
}

- (instancetype)init
{
    return nil;
}

- (instancetype)initWithScrollView:(UIScrollView *)scrollView
{
    self = [super init];
    if (!self)
        return nil;

    _scrollView = scrollView;
    _animator = adoptNS([[WKKeyboardScrollingAnimator alloc] initWithScrollable:self]);

    return self;
}

- (void)dealloc
{
    [_animator invalidate];
    [super dealloc];
}

- (void)invalidate
{
    _scrollView = nil;

    [_animator invalidate];
    _animator = nil;
}

- (void)setDelegate:(id <WKKeyboardScrollViewAnimatorDelegate>)delegate
{
    _delegate = delegate;

    _delegateRespondsToIsKeyboardScrollable = [_delegate respondsToSelector:@selector(isScrollableForKeyboardScrollViewAnimator:)];
    _delegateRespondsToDistanceForIncrement = [_delegate respondsToSelector:@selector(keyboardScrollViewAnimator:distanceForIncrement:)];
    _delegateRespondsToWillScroll = [_delegate respondsToSelector:@selector(keyboardScrollViewAnimatorWillScroll:)];
    _delegateRespondsToDidFinishScrolling = [_delegate respondsToSelector:@selector(keyboardScrollViewAnimatorDidFinishScrolling:)];
}

- (void)willStartInteractiveScroll
{
    [_animator willStartInteractiveScroll];
}

- (BOOL)beginWithEvent:(::WebEvent *)event
{
    return [_animator beginWithEvent:event];
}

- (void)handleKeyEvent:(::WebEvent *)event
{
    return [_animator handleKeyEvent:event];
}

- (BOOL)isKeyboardScrollable
{
    if (!_delegateRespondsToIsKeyboardScrollable)
        return YES;
    return [_delegate isScrollableForKeyboardScrollViewAnimator:self];
}

- (CGFloat)distanceForIncrement:(WebKit::ScrollingIncrement)increment
{
    auto scrollView = _scrollView.getAutoreleased();
    if (!scrollView)
        return 0;

    const CGFloat defaultPageScrollFraction = 0.8;
    const CGFloat defaultLineScrollHeight = 40;

    if (!_delegateRespondsToDistanceForIncrement) {
        switch (increment) {
        case WebKit::ScrollingIncrement::Document:
            return scrollView.contentSize.height;
        case WebKit::ScrollingIncrement::Page:
            return scrollView.frame.size.height * defaultPageScrollFraction;
        case WebKit::ScrollingIncrement::Line:
            return defaultLineScrollHeight * scrollView.zoomScale;
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    return [_delegate keyboardScrollViewAnimator:self distanceForIncrement:increment];
}

- (void)scrollToContentOffset:(WebCore::FloatPoint)contentOffsetDelta animated:(BOOL)animated
{
    auto scrollView = _scrollView.getAutoreleased();
    if (!scrollView)
        return;
    if (_delegateRespondsToWillScroll)
        [_delegate keyboardScrollViewAnimatorWillScroll:self];
    [scrollView setContentOffset:contentOffsetDelta animated:animated];
    [scrollView _flashScrollIndicatorsPersistingPreviousFlashes:YES];
}

- (void)scrollWithScrollToExtentAnimationTo:(CGPoint)offset
{
    auto scrollView = _scrollView.getAutoreleased();
    [scrollView _setContentOffsetWithDecelerationAnimation:offset];
    [scrollView flashScrollIndicators];
}

- (CGPoint)contentOffset
{
    auto scrollView = _scrollView.getAutoreleased();
    if (!scrollView)
        return CGPointZero;

    return [scrollView contentOffset];
}

- (CGPoint)boundedContentOffset:(CGPoint)offset
{
    auto scrollView = _scrollView.getAutoreleased();
    if (!scrollView)
        return CGPointZero;

    return [scrollView _adjustedContentOffsetForContentOffset:offset];
}

- (CGSize)interactiveScrollVelocity
{
    auto scrollView = _scrollView.getAutoreleased();
    if (!scrollView)
        return CGSizeZero;

    const NSTimeInterval millisecondsPerSecond = 1000;
    return CGSizeMake(scrollView._horizontalVelocity * millisecondsPerSecond, scrollView._verticalVelocity * millisecondsPerSecond);
}

- (WebCore::RectEdges<bool>)scrollableDirectionsFromOffset:(CGPoint)offset
{
    auto scrollView = _scrollView.getAutoreleased();
    if (!scrollView)
        return { };

    UIEdgeInsets contentInsets = scrollView.adjustedContentInset;

    CGSize contentSize = scrollView.contentSize;
    CGSize scrollViewSize = scrollView.bounds.size;

    CGPoint minimumContentOffset = CGPointMake(-contentInsets.left, -contentInsets.top);
    CGPoint maximumContentOffset = CGPointMake(std::max(minimumContentOffset.x, contentSize.width + contentInsets.right - scrollViewSize.width), std::max(minimumContentOffset.y, contentSize.height + contentInsets.bottom - scrollViewSize.height));

    WebCore::RectEdges<bool> edges;

    edges.setTop(offset.y > minimumContentOffset.y);
    edges.setBottom(offset.y < maximumContentOffset.y);
    edges.setLeft(offset.x > minimumContentOffset.x);
    edges.setRight(offset.x < maximumContentOffset.x);

    return edges;
}

- (WebCore::RectEdges<bool>)rubberbandableDirections
{
    auto scrollView = _scrollView.getAutoreleased();
    if (!scrollView)
        return { };

    WebCore::RectEdges<bool> edges;

    edges.setTop(scrollView._canScrollY);
    edges.setBottom(edges.top());
    edges.setLeft(scrollView._canScrollX);
    edges.setRight(edges.left());

    return edges;
}

- (void)didFinishScrolling
{
    if (_delegateRespondsToDidFinishScrolling)
        [_delegate keyboardScrollViewAnimatorDidFinishScrolling:self];
}

@end

#endif // PLATFORM(IOS_FAMILY)
