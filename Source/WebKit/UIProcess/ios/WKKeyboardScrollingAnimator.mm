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

#import "AccessibilitySupportSPI.h"
#import "UIKitSPI.h"
#import "UIKitUtilities.h"
#import "WKVelocityTrackingScrollView.h"
#import <QuartzCore/CADisplayLink.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/KeyEventCodesIOS.h>
#import <WebCore/KeyboardScroll.h>
#import <WebCore/RectEdges.h>
#import <WebCore/WebEvent.h>
#import <WebKit/UIKitSPI.h>
#import <algorithm>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

@protocol WKKeyboardScrollableInternal <NSObject>
@required
- (BOOL)isKeyboardScrollable;
- (CGFloat)distanceForIncrement:(WebCore::ScrollGranularity)increment inDirection:(WebCore::ScrollDirection)direction;
- (void)scrollToContentOffset:(WebCore::FloatPoint)offset animated:(BOOL)animated;
- (void)willBeginScrollingToExtentWithAnimationInTrackingView:(UIView *)view;
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

- (void)stopScrollingImmediately;

@end

@implementation WKKeyboardScrollingAnimator {
    id <WKKeyboardScrollableInternal> _scrollable;
    RetainPtr<CADisplayLink> _displayLink;
    RetainPtr<UIView> _viewForTrackingScrollToExtentAnimation;

    std::optional<WebCore::KeyboardScroll> _currentScroll;

    BOOL _scrollTriggeringKeyIsPressed;

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

- (void)invalidate
{
    [self resetViewForScrollToExtentAnimation];
    [self stopAnimatedScroll];
    [self stopDisplayLink];
    _scrollable = nil;
}

static WebCore::FloatSize perpendicularAbsoluteUnitVector(WebCore::ScrollDirection direction)
{
    switch (direction) {
    case WebCore::ScrollDirection::ScrollUp:
    case WebCore::ScrollDirection::ScrollDown:
        return { 1, 0 };
    case WebCore::ScrollDirection::ScrollLeft:
    case WebCore::ScrollDirection::ScrollRight:
        return { 0, 1 };
    }
}

static WebCore::BoxSide boxSide(WebCore::ScrollDirection direction)
{
    switch (direction) {
    case WebCore::ScrollDirection::ScrollUp:
        return WebCore::BoxSide::Top;
    case WebCore::ScrollDirection::ScrollDown:
        return WebCore::BoxSide::Bottom;
    case WebCore::ScrollDirection::ScrollLeft:
        return WebCore::BoxSide::Left;
    case WebCore::ScrollDirection::ScrollRight:
        return WebCore::BoxSide::Right;
    }
}

- (std::optional<WebCore::KeyboardScroll>)keyboardScrollForEvent:(::WebEvent *)event
{
    static const unsigned kWebSpaceKey = 0x20;

    if (![_scrollable isKeyboardScrollable])
        return std::nullopt;

    if (event.keyboardFlags & WebEventKeyboardInputModifierFlagsChanged)
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

    // No shortcuts include more than one modifier; we should not eat key events
    // that contain more than one modifier because they might be used for other shortcuts.
    if (shiftPressed + altPressed + cmdPressed > 1)
        return std::nullopt;

    auto allowedModifiers = ^ WebEventFlags {
        switch (key) {
        case Key::LeftArrow:
        case Key::RightArrow:
            return WebEventFlagMaskOptionKey;
        case Key::UpArrow:
        case Key::DownArrow:
            return WebEventFlagMaskOptionKey | WebEventFlagMaskCommandKey;
        case Key::PageUp:
        case Key::PageDown:
            return 0;
        case Key::Space:
            return WebEventFlagMaskShiftKey;
        case Key::Other:
            ASSERT_NOT_REACHED();
            return 0;
        };
    }();

    auto relevantModifierFlags = WebEventFlagMaskOptionKey | WebEventFlagMaskCommandKey | WebEventFlagMaskShiftKey;
    if (event.modifierFlags & relevantModifierFlags & ~allowedModifiers)
        return std::nullopt;

    auto increment = ^{
        switch (key) {
        case Key::LeftArrow:
        case Key::RightArrow:
            if (altPressed)
                return WebCore::ScrollGranularity::Page;
            return WebCore::ScrollGranularity::Line;
        case Key::UpArrow:
        case Key::DownArrow:
            if (altPressed)
                return WebCore::ScrollGranularity::Page;
            if (cmdPressed)
                return WebCore::ScrollGranularity::Document;
            return WebCore::ScrollGranularity::Line;
        case Key::PageUp:
        case Key::PageDown:
        case Key::Space:
            return WebCore::ScrollGranularity::Page;
        case Key::Other:
            ASSERT_NOT_REACHED();
            return WebCore::ScrollGranularity::Line;
        };
    }();

    auto direction = ^() {
        switch (key) {
        case Key::LeftArrow:
            return WebCore::ScrollDirection::ScrollLeft;
        case Key::RightArrow:
            return WebCore::ScrollDirection::ScrollRight;
        case Key::UpArrow:
        case Key::PageUp:
            return WebCore::ScrollDirection::ScrollUp;
        case Key::DownArrow:
        case Key::PageDown:
            return WebCore::ScrollDirection::ScrollDown;
        case Key::Space:
            return shiftPressed ? WebCore::ScrollDirection::ScrollUp : WebCore::ScrollDirection::ScrollDown;
        case Key::Other:
            ASSERT_NOT_REACHED();
            return WebCore::ScrollDirection::ScrollDown;
        };
    }();

    // FIXME (227461): Replace with call to WebCore::KeyboardScroll constructor.
    // FIXME (245749): Use `ScrollableArea::adjustVerticalPageScrollStepForFixedContent` to account for fixed content

    constexpr auto parameters = WebCore::KeyboardScrollParameters::parameters();

    CGFloat scrollDistance = [_scrollable distanceForIncrement:increment inDirection:direction];

    WebCore::KeyboardScroll scroll;
    scroll.offset = WebCore::unitVectorForScrollDirection(direction).scaled(scrollDistance);
    scroll.granularity = increment;
    scroll.direction = direction;
    scroll.maximumVelocity = scroll.offset.scaled(parameters.maximumVelocityMultiplier);

    // Apply a constant force to achieve Vmax in timeToMaximumVelocity seconds.
    // F_constant = m * Vmax / t
    scroll.force = scroll.maximumVelocity.scaled(parameters.springMass / parameters.timeToMaximumVelocity);
    
    return scroll;
}

static NSString * const scrollToExtentWithAnimationKey = @"ScrollToExtentAnimation";

- (BOOL)beginWithEvent:(::WebEvent *)event
{
    if (event.type != WebEventKeyDown)
        return NO;

    auto scroll = [self keyboardScrollForEvent:event];
    if (!scroll)
        return NO;

    if (_scrollTriggeringKeyIsPressed)
        return NO;

    if (![_scrollable rubberbandableDirections].at(boxSide(scroll->direction)))
        return NO;

    _scrollTriggeringKeyIsPressed = YES;
    _currentScroll = scroll;

    [self resetViewForScrollToExtentAnimation];

    if (scroll->granularity == WebCore::ScrollGranularity::Document) {
        _velocity = { };
        [self stopAnimatedScroll];
        auto currentOffset = _scrollable.contentOffset;
        auto targetOffset = [_scrollable boundedContentOffset:_currentPosition + scroll->offset];
        _viewForTrackingScrollToExtentAnimation = adoptNS([UIView new]);
        [_viewForTrackingScrollToExtentAnimation setHidden:YES];
        [_viewForTrackingScrollToExtentAnimation setUserInteractionEnabled:NO];
        [_scrollable willBeginScrollingToExtentWithAnimationInTrackingView:_viewForTrackingScrollToExtentAnimation.get()];

        // See also: _smoothDecelerationAnimation() in UIKit.
        auto animation = [CASpringAnimation animationWithKeyPath:@"position"];
        static constexpr auto angularFrequency = 2 * M_PI / 0.6;
        animation.mass = 1;
        animation.stiffness = angularFrequency * angularFrequency;
        animation.damping = 2 * angularFrequency;
        animation.duration = 1.6;
        animation.timingFunction = [CAMediaTimingFunction functionWithControlPoints:0.0 :0.2 :1.0 :1.0];
        animation.fromValue = [NSValue valueWithCGPoint:currentOffset];
        animation.toValue = [NSValue valueWithCGPoint:targetOffset];

        auto layer = [_viewForTrackingScrollToExtentAnimation layer];
        layer.position = currentOffset;
        [layer removeAllAnimations];
        [layer addAnimation:animation forKey:scrollToExtentWithAnimationKey];
        layer.position = targetOffset;

        [self startDisplayLinkIfNeeded];

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
    if (!_scrollTriggeringKeyIsPressed)
        return;

    auto scroll = [self keyboardScrollForEvent:event];

    // UIKit does not emit a keyup event when the Command key is down. See <rdar://problem/49523065>.
    // For recognized key commands that include the Command key (e.g. Command + Arrow Up) we reset our
    // state on keydown.
    if (!scroll || event.type == WebEventKeyUp || (event.modifierFlags & WebEventFlagMaskCommandKey)) {
        [self stopAnimatedScroll];
        _scrollTriggeringKeyIsPressed = NO;
    }
}

static WebCore::FloatPoint farthestPointInDirection(WebCore::FloatPoint a, WebCore::FloatPoint b, WebCore::ScrollDirection direction)
{
    switch (direction) {
    case WebCore::ScrollDirection::ScrollUp:
        return WebCore::FloatPoint(a.x(), std::min(a.y(), b.y()));
    case WebCore::ScrollDirection::ScrollDown:
        return WebCore::FloatPoint(a.x(), std::max(a.y(), b.y()));
    case WebCore::ScrollDirection::ScrollLeft:
        return WebCore::FloatPoint(std::min(a.x(), b.x()), a.y());
    case WebCore::ScrollDirection::ScrollRight:
        return WebCore::FloatPoint(std::max(a.x(), b.x()), a.y());
    }

    ASSERT_NOT_REACHED();
    return { };
}

- (void)stopAnimatedScroll
{
    if (!_currentScroll)
        return;

    constexpr auto parameters = WebCore::KeyboardScrollParameters::parameters();

    // Determine the settling position of the spring, conserving the system's current energy.
    // Kinetic = elastic potential
    // 1/2 * m * v^2 = 1/2 * k * x^2
    // x = sqrt(v^2 * m / k)
    auto displacementMagnitudeSquared = (_velocity * _velocity).scaled(parameters.springMass / parameters.springStiffness);
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

- (BOOL)scrollTriggeringKeyIsPressed
{
    return _scrollTriggeringKeyIsPressed;
}

- (void)willStartInteractiveScroll
{
    // If the user touches the screen to start an interactive scroll, stop everything.
    _velocity = { };
    [self resetViewForScrollToExtentAnimation];
    [self stopAnimatedScroll];
    [self stopDisplayLink];
}

- (void)startDisplayLinkIfNeeded
{
    if (_displayLink)
        return;

    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkFired:)];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)resetViewForScrollToExtentAnimation
{
    [[_viewForTrackingScrollToExtentAnimation layer] removeAllAnimations];
    [_viewForTrackingScrollToExtentAnimation removeFromSuperview];
    _viewForTrackingScrollToExtentAnimation = nil;
}

- (void)stopDisplayLink
{
    [_displayLink invalidate];
    _displayLink = nil;
}

- (void)stopScrollingImmediately
{
    [self resetViewForScrollToExtentAnimation];
    [_scrollable didFinishScrolling];
    [self stopDisplayLink];
    _velocity = { };
}

- (void)displayLinkFired:(CADisplayLink *)sender
{
    if (_viewForTrackingScrollToExtentAnimation) {
        if (![[_viewForTrackingScrollToExtentAnimation layer].animationKeys containsObject:scrollToExtentWithAnimationKey]) {
            [_scrollable didFinishScrolling];
            [self resetViewForScrollToExtentAnimation];
            [self stopDisplayLink];
        } else if (auto presentationLayer = [_viewForTrackingScrollToExtentAnimation layer].presentationLayer)
            [_scrollable scrollToContentOffset:presentationLayer.position animated:NO];
        return;
    }

    WebCore::FloatSize force;
    WebCore::FloatSize axesToApplySpring = { 1, 1 };

    constexpr auto parameters = WebCore::KeyboardScrollParameters::parameters();

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
            force = WebCore::unitVectorForScrollDirection(direction).scaled(parameters.rubberBandForce);
        }

        // If we've reached or exceeded the maximum velocity, stop applying any force.
        // However, we won't let the spring snap, we'll just keep going at the same
        // velocity until the user raises their finger or we hit an edge.
        if (std::abs(_velocity.width()) >= std::abs(_currentScroll->maximumVelocity.width()))
            force.setWidth(0);
        if (std::abs(_velocity.height()) >= std::abs(_currentScroll->maximumVelocity.height()))
            force.setHeight(0);
    }

    WebCore::FloatPoint idealPosition = [_scrollable boundedContentOffset:_currentScroll ? _currentPosition : _idealPosition];
    WebCore::FloatSize displacement = _currentPosition - idealPosition;

    // Compute the spring's force, and apply it in allowed directions.
    // F_spring = -k * x - c * v
    auto springForce = - displacement.scaled(parameters.springStiffness) - _velocity.scaled(parameters.springDamping);
    force += springForce * axesToApplySpring;

    // Integrate acceleration -> velocity -> position for this time step.
    CFTimeInterval frameDuration = sender.targetTimestamp - sender.timestamp;
    WebCore::FloatSize acceleration = force.scaled(1. / parameters.springMass);
    _velocity += acceleration.scaled(frameDuration);
    _currentPosition += _velocity.scaled(frameDuration);

    [_scrollable scrollToContentOffset:_currentPosition animated:NO];

    // If we've effectively stopped scrolling, and no key is pressed,
    // shut down the display link.
    if (!_scrollTriggeringKeyIsPressed && _velocity.diagonalLengthSquared() < 1) {
        [_scrollable didFinishScrolling];
        [self stopDisplayLink];
        _velocity = { };
    }
}

@end

@interface WKKeyboardScrollViewAnimator () <WKKeyboardScrollableInternal>
@end

@implementation WKKeyboardScrollViewAnimator {
    __weak WKVelocityTrackingScrollView *_scrollView;
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

- (instancetype)initWithScrollView:(WKVelocityTrackingScrollView *)scrollView
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

- (void)stopScrollingImmediately
{
    [_animator stopScrollingImmediately];
}

- (void)setDelegate:(id <WKKeyboardScrollViewAnimatorDelegate>)delegate
{
    _delegate = delegate;

    _delegateRespondsToIsKeyboardScrollable = [_delegate respondsToSelector:@selector(isScrollableForKeyboardScrollViewAnimator:)];
    _delegateRespondsToDistanceForIncrement = [_delegate respondsToSelector:@selector(keyboardScrollViewAnimator:distanceForIncrement:inDirection:)];
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

- (BOOL)scrollTriggeringKeyIsPressed
{
    return [_animator scrollTriggeringKeyIsPressed];
}

- (BOOL)isKeyboardScrollable
{
    if (!_delegateRespondsToIsKeyboardScrollable)
        return YES;
    return [_delegate isScrollableForKeyboardScrollViewAnimator:self];
}

- (CGFloat)distanceForIncrement:(WebCore::ScrollGranularity)increment inDirection:(WebCore::ScrollDirection)direction
{
    if (!_scrollView)
        return 0;

    const CGFloat defaultPageScrollFraction = 0.8;
    const CGFloat defaultLineScrollHeight = 40;

    BOOL directionIsHorizontal = direction == WebCore::ScrollDirection::ScrollLeft || direction == WebCore::ScrollDirection::ScrollRight;

    if (!_delegateRespondsToDistanceForIncrement) {
        switch (increment) {
        case WebCore::ScrollGranularity::Document:
            return directionIsHorizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height;
        case WebCore::ScrollGranularity::Page:
            return (directionIsHorizontal ? _scrollView.frame.size.width : _scrollView.frame.size.height) * defaultPageScrollFraction;
        case WebCore::ScrollGranularity::Line:
            return defaultLineScrollHeight * _scrollView.zoomScale;
        case WebCore::ScrollGranularity::Pixel:
            return 0;
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    return [_delegate keyboardScrollViewAnimator:self distanceForIncrement:increment inDirection:direction];
}

static UIAxis axesForDelta(WebCore::FloatSize delta)
{
    UIAxis axes = UIAxisNeither;
    if (delta.width())
        axes = static_cast<UIAxis>(axes | UIAxisHorizontal);
    if (delta.height())
        axes = static_cast<UIAxis>(axes | UIAxisVertical);
    return axes;
}

- (void)scrollToContentOffset:(WebCore::FloatPoint)contentOffset animated:(BOOL)animated
{
    if (!_scrollView)
        return;
    if (_delegateRespondsToWillScroll)
        [_delegate keyboardScrollViewAnimatorWillScroll:self];
    [_scrollView setContentOffset:contentOffset animated:animated];
    [_scrollView _flashScrollIndicatorsForAxes:axesForDelta(WebCore::FloatPoint(_scrollView.contentOffset) - contentOffset) persistingPreviousFlashes:YES];
}

- (void)willBeginScrollingToExtentWithAnimationInTrackingView:(UIView *)view
{
    [_scrollView addSubview:view];
    [_scrollView flashScrollIndicators];
}

- (CGPoint)contentOffset
{
    if (!_scrollView)
        return CGPointZero;

    return _scrollView.contentOffset;
}

- (CGPoint)boundedContentOffset:(CGPoint)offset
{
    return [_scrollView _wk_clampToScrollExtents:offset];
}

- (CGSize)interactiveScrollVelocity
{
    return _scrollView.interactiveScrollVelocityInPointsPerSecond;
}

- (WebCore::RectEdges<bool>)scrollableDirectionsFromOffset:(CGPoint)offset
{
    if (!_scrollView)
        return { };

    UIEdgeInsets contentInsets = _scrollView.adjustedContentInset;

    CGSize contentSize = _scrollView.contentSize;
    CGSize scrollViewSize = _scrollView.bounds.size;

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
    if (!_scrollView)
        return { };

    WebCore::RectEdges<bool> edges;

    edges.setTop(_scrollView._wk_canScrollVerticallyWithoutBouncing);
    edges.setBottom(edges.top());
    edges.setLeft(_scrollView._wk_canScrollHorizontallyWithoutBouncing);
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
