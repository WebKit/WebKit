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
#import "WKTouchEventsGestureRecognizer.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKContentViewInteraction.h"
#import "WKWebViewIOS.h"
#import <objc/message.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

static constexpr CGFloat singleTapMovementThreshold = 45;
static constexpr NSTimeInterval singleTapTimeThreshold = 0.75;

// Associated Object key used to get the identifier for a UITouch object.
static char associatedTouchIdentifierKey;
static unsigned incrementingTouchIdentifier = 1;

@implementation WKTouchEventsGestureRecognizer {
    BOOL _passedHitTest;
    BOOL _defaultPrevented;
    BOOL _dispatchingTouchEvents;
    BOOL _isPotentialTap;
    BOOL _wasExplicitlyCancelled;
    CGFloat _originalGestureDistance;
    CGFloat _originalGestureAngle;

    NSTimeInterval _lastTouchesBeganTime;
    std::optional<CGPoint> _lastTouchesBeganLocation;

    WebKit::WKTouchEvent _lastTouchEvent;
    RetainPtr<NSMapTable<NSNumber *, UITouch *>>_activeTouchesByIdentifier;
}

// UITouch's timestamp is a relative system time since startup, minus the time the device was suspended. This
// function converts it into an estimated wall time (seconds since Epoch).
static double approximateWallTime(NSTimeInterval timestamp)
{
    // mach_absolute_time() provides a relative system time since startup, minus the time the device was suspended.
    auto elapsedTimeSinceStartup = CACurrentMediaTime();

    auto elapsedTimeSinceTimestamp = elapsedTimeSinceStartup - timestamp;

    // CFAbsoluteTimeGetCurrent() provides the absolute time in seconds since 2001 so we need to add kCFAbsoluteTimeIntervalSince1970
    // to get a time since Epoch.
    return kCFAbsoluteTimeIntervalSince1970 + CFAbsoluteTimeGetCurrent() - elapsedTimeSinceTimestamp;
}

@synthesize defaultPrevented = _defaultPrevented;
@synthesize dispatchingTouchEvents = _dispatchingTouchEvents;

+ (void)initialize
{
    if (self == WKTouchEventsGestureRecognizer.class) {
        do {
            incrementingTouchIdentifier = arc4random();
        } while (!incrementingTouchIdentifier || incrementingTouchIdentifier == UINT_MAX);
    }
}

- (instancetype)initWithContentView:(WKContentView *)view
{
    self = [super initWithTarget:nil action:nil];
    if (!self)
        return nil;

    // Remove our target/action pair, and hold onto it.
    // This gesture recognizer calls its single target/action pair
    // itself, and is not called via the standard mechanism.
    _activeTouchesByIdentifier = NSMapTable.strongToWeakObjectsMapTable;
    _contentView = view;

    [self reset];

    return self;
}

- (NSMapTable<NSNumber *, UITouch *> *)activeTouchesByIdentifier
{
    return _activeTouchesByIdentifier.get();
}

- (void)reset
{
    if (_wasExplicitlyCancelled && (_lastTouchEvent.type == WebKit::WKTouchEventType::Begin || _lastTouchEvent.type == WebKit::WKTouchEventType::Change)) {
        _lastTouchEvent.type = WebKit::WKTouchEventType::Cancel;
        for (auto& touchPoint : _lastTouchEvent.touchPoints)
            touchPoint.phase = UITouchPhaseCancelled;
        [self performAction];
    }

    _wasExplicitlyCancelled = NO;
    _passedHitTest = NO;
    _defaultPrevented = NO;
    _dispatchingTouchEvents = NO;
    _wasExplicitlyCancelled = NO;

    _originalGestureDistance = NAN;
    _originalGestureAngle = NAN;

    _isPotentialTap = NO;

    _lastTouchEvent.type = WebKit::WKTouchEventType::Begin;
    _lastTouchEvent.timestamp = 0;
    _lastTouchEvent.locationInRootViewCoordinates = CGPointZero;
    _lastTouchEvent.scale = NAN;
    _lastTouchEvent.rotation = NAN;
    _lastTouchEvent.inJavaScriptGesture = false;
    _lastTouchEvent.isPotentialTap = false;
    _lastTouchEvent.coalescedEvents = { };
    _lastTouchEvent.predictedEvents = { };
    _lastTouchesBeganTime = 0;
    _lastTouchesBeganLocation = std::nullopt;
}

- (void)cancel
{
    _wasExplicitlyCancelled = NO;

    auto gestureState = [self state];
    switch (gestureState) {
    case UIGestureRecognizerStatePossible:
        _wasExplicitlyCancelled = YES;
        self.state = UIGestureRecognizerStateFailed;
        break;
    case UIGestureRecognizerStateBegan:
    case UIGestureRecognizerStateChanged:
        _wasExplicitlyCancelled = YES;
        self.state = UIGestureRecognizerStateCancelled;
        break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
        break;
    }
}

static unsigned nextTouchIdentifier()
{
    auto touchIdentifier = incrementingTouchIdentifier++;

    // Avoid the potential to overflow and skip the invalid 0 identifier.
    if (incrementingTouchIdentifier == UINT_MAX)
        incrementingTouchIdentifier = 1;

    return touchIdentifier;
}

- (void)_updateTapStateWithTouches:(NSSet<UITouch *> *)touches
{
    if (touches.count != 1) {
        _lastTouchEvent.isPotentialTap = false;
        _isPotentialTap = NO;
        return;
    }

    auto touch = touches.anyObject;
    auto currentTime = [NSDate timeIntervalSinceReferenceDate];
    auto currentTouchLocation = [touch locationInView:self.view];
    if (!_lastTouchesBeganLocation) {
        _lastTouchesBeganTime = currentTime;
        _lastTouchesBeganLocation = currentTouchLocation;
        _lastTouchEvent.isPotentialTap = YES;
        _isPotentialTap = YES;
        return;
    }

    BOOL isPotentialTap = currentTime - _lastTouchesBeganTime <= singleTapTimeThreshold
        && std::abs(_lastTouchesBeganLocation->x - currentTouchLocation.x) <= singleTapMovementThreshold
        && std::abs(_lastTouchesBeganLocation->y - currentTouchLocation.y) <= singleTapMovementThreshold;

    _lastTouchEvent.isPotentialTap = isPotentialTap;
    _isPotentialTap = isPotentialTap;
}

- (void)_updateTapStateWithTouches:(NSSet *)touches type:(WebKit::WKTouchEventType)type
{
    switch (type) {
    case WebKit::WKTouchEventType::Begin: {
        [self _updateTapStateWithTouches:touches];
        break;
    }
    case WebKit::WKTouchEventType::Change: {
        if (_isPotentialTap)
            [self _updateTapStateWithTouches:touches];
        break;
    }
    case WebKit::WKTouchEventType::End: {
        if (_isPotentialTap)
            [self _updateTapStateWithTouches:touches];
        break;
    }
    case WebKit::WKTouchEventType::Cancel: {
        _lastTouchEvent.isPotentialTap = false;
        _isPotentialTap = NO;
        break;
    }
    }
}

static CGPoint mapRootViewToViewport(CGPoint pointInRootView, WKContentView *contentView)
{
    RetainPtr webView = [contentView webView];
    CGPoint origin = [webView bounds].origin;
    auto inset = [webView _computedObscuredInset];
    auto offsetInRootView = [webView convertPoint:CGPointMake(origin.x + inset.left, origin.y + inset.top) toView:contentView];
    return CGPointMake(pointInRootView.x - offsetInRootView.x, pointInRootView.y - offsetInRootView.y);
}

- (WebKit::WKTouchEvent)_touchEventForTouch:(UITouch *)touch
{
    auto locationInWindow = [touch locationInView:nil];
    auto locationInRootView = [[self view] convertPoint:locationInWindow fromView:nil];
    RetainPtr contentView = [self contentView];

    WebKit::WKTouchPoint touchPoint;
    touchPoint.locationInRootViewCoordinates = locationInRootView;
    touchPoint.locationInViewport = mapRootViewToViewport(locationInRootView, contentView.get());
    touchPoint.identifier = 0;
    touchPoint.phase = touch.phase;
    touchPoint.majorRadiusInWindowCoordinates = touch.majorRadius;
    touchPoint.force = touch.maximumPossibleForce > 0 ? touch.force / touch.maximumPossibleForce : 0;

    if (touch.type == UITouchTypeStylus) {
        touchPoint.touchType = WebKit::WKTouchPointType::Stylus;
        touchPoint.altitudeAngle = touch.altitudeAngle;
        touchPoint.azimuthAngle = [touch azimuthAngleInView:self.view.window];
    } else {
        touchPoint.touchType = WebKit::WKTouchPointType::Direct;
        touchPoint.altitudeAngle = 0;
        touchPoint.azimuthAngle = 0;
    }

    WebKit::WKTouchEvent event;
    event.type = WebKit::WKTouchEventType::Change;
    event.timestamp = approximateWallTime(touch.timestamp);
    event.locationInRootViewCoordinates = locationInRootView;
    event.touchPoints = { touchPoint };

    return event;
}

- (void)_recordTouches:(NSSet<UITouch *> *)touches type:(WebKit::WKTouchEventType)type coalescedTouches:(NSArray<UITouch *> *)coalescedTouches predictedTouches:(NSArray<UITouch *> *)predictedTouches
{
    _lastTouchEvent.type = type;
    _lastTouchEvent.inJavaScriptGesture = false;

    auto firstTouch = CGPointZero;
    auto secondTouch = CGPointZero;
    unsigned touchesDownCount = 0;
    auto centroidInWindowCoordinates = CGPointZero;
    auto centroidInRootViewCoordinates = CGPointZero;
    auto releasedTouchCentroidInWindowCoordinates = CGPointZero;
    auto releasedTouchCentroidInRootViewCoordinates = CGPointZero;

    _dispatchingTouchEvents = YES;

    NSUInteger touchCount = touches.count;

    if (_lastTouchEvent.touchPoints.size() != touchCount)
        _lastTouchEvent.touchPoints.resize(touchCount);

    _lastTouchEvent.timestamp = approximateWallTime(touches.anyObject.timestamp);

    _lastTouchEvent.coalescedEvents = { };
    _lastTouchEvent.predictedEvents = { };

    if (type == WebKit::WKTouchEventType::Change) {
        for (UITouch *coalescedTouch in coalescedTouches)
            _lastTouchEvent.coalescedEvents.append([self _touchEventForTouch:coalescedTouch]);

        for (UITouch *predictedTouch in predictedTouches)
            _lastTouchEvent.predictedEvents.append([self _touchEventForTouch:predictedTouch]);
    }

    RetainPtr contentView = [self contentView];
    NSUInteger touchIndex = 0;

    [_activeTouchesByIdentifier removeAllObjects];

    for (UITouch *touch in touches) {
        // Get the identifier of this touch. Or create one if one did not exist.
        auto associatedIdentifier = dynamic_objc_cast<NSNumber>(objc_getAssociatedObject(touch, &associatedTouchIdentifierKey));

        // Create a new identifier for trackpad events in the Begin phase regardless, because the UITouch
        // instance persists between trackpad clicks, and there is existing web content that does not expect subsequent
        // clicks to have the same touch identifier (62895732).
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        if (touch._isPointerTouch && type == WebKit::WKTouchEventType::Begin)
            associatedIdentifier = nil;
#endif

        if (!associatedIdentifier) {
            associatedIdentifier = [NSNumber numberWithUnsignedInt:nextTouchIdentifier()];
            objc_setAssociatedObject(touch, &associatedTouchIdentifierKey, associatedIdentifier, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        }

        [_activeTouchesByIdentifier setObject:touch forKey:associatedIdentifier];

        // iPhone WebKit works as if it is full screen. Therefore Web coords are Global coords.
        auto& touchPoint = _lastTouchEvent.touchPoints[touchIndex];
        auto locationInWindow = [touch locationInView:nil];
        auto locationInRootView = [[self view] convertPoint:locationInWindow fromView:nil];
        touchPoint.locationInRootViewCoordinates = locationInRootView;
        touchPoint.locationInViewport = mapRootViewToViewport(locationInRootView, contentView.get());
        touchPoint.identifier = [associatedIdentifier unsignedIntValue];
        touchPoint.phase = touch.phase;
        touchPoint.majorRadiusInWindowCoordinates = touch.majorRadius;

        if (touch.maximumPossibleForce > 0)
            touchPoint.force = touch.force / touch.maximumPossibleForce;
        else
            touchPoint.force = 0;

        if (touch.type == UITouchTypeStylus) {
            touchPoint.touchType = WebKit::WKTouchPointType::Stylus;
            touchPoint.altitudeAngle = touch.altitudeAngle;
            touchPoint.azimuthAngle = [touch azimuthAngleInView:self.view.window];
        } else {
            touchPoint.touchType = WebKit::WKTouchPointType::Direct;
            touchPoint.altitudeAngle = 0;
            touchPoint.azimuthAngle = 0;
        }

        ++touchIndex;

        if ((touchPoint.phase == UITouchPhaseEnded) || (touchPoint.phase == UITouchPhaseCancelled)) {
            releasedTouchCentroidInWindowCoordinates.x += locationInWindow.x;
            releasedTouchCentroidInWindowCoordinates.y += locationInWindow.y;
            releasedTouchCentroidInRootViewCoordinates.x += locationInRootView.x;
            releasedTouchCentroidInRootViewCoordinates.y += locationInRootView.y;
            continue;
        }

        touchesDownCount++;

        centroidInWindowCoordinates.x += locationInWindow.x;
        centroidInWindowCoordinates.y += locationInWindow.y;
        centroidInRootViewCoordinates.x += locationInRootView.x;
        centroidInRootViewCoordinates.y += locationInRootView.y;

        if (touchesDownCount == 1)
            firstTouch = locationInRootView;
        else if (touchesDownCount == 2)
            secondTouch = locationInRootView;
    }
    ASSERT(touchIndex == _lastTouchEvent.touchPoints.size());

    if (touchesDownCount > 0) {
        centroidInWindowCoordinates = CGPointMake(centroidInWindowCoordinates.x / touchesDownCount, centroidInWindowCoordinates.y / touchesDownCount);
        centroidInRootViewCoordinates = CGPointMake(centroidInRootViewCoordinates.x / touchesDownCount, centroidInRootViewCoordinates.y / touchesDownCount);
    } else {
        centroidInWindowCoordinates = CGPointMake(releasedTouchCentroidInWindowCoordinates.x / touchCount, releasedTouchCentroidInWindowCoordinates.y / touchCount);
        centroidInRootViewCoordinates = CGPointMake(releasedTouchCentroidInRootViewCoordinates.x / touchCount, releasedTouchCentroidInRootViewCoordinates.y / touchCount);
    }

    _lastTouchEvent.locationInRootViewCoordinates = centroidInRootViewCoordinates;

    _lastTouchEvent.scale = 0;
    _lastTouchEvent.rotation = 0;
    if (touchesDownCount > 1) {
        ASSERT(!CGPointEqualToPoint(firstTouch, CGPointZero));
        ASSERT(!CGPointEqualToPoint(secondTouch, CGPointZero));

        auto horizontalDistance = secondTouch.x - firstTouch.x;
        auto verticalDistance = secondTouch.y - firstTouch.y;

        auto currentDistance = sqrtf(horizontalDistance * horizontalDistance + verticalDistance * verticalDistance);
        if (isnan(_originalGestureDistance))
            _originalGestureDistance = currentDistance;

        _lastTouchEvent.scale = currentDistance / _originalGestureDistance;

        float currentRotation = atan2f(horizontalDistance, verticalDistance) * 180.0 * M_1_PI;
        if (isnan(_originalGestureAngle))
            _originalGestureAngle = currentRotation;

        _lastTouchEvent.rotation = _originalGestureAngle - currentRotation;

        _lastTouchEvent.inJavaScriptGesture = true;
    }

    [self _updateTapStateWithTouches:touches type:type];
}

static WebKit::WKTouchEventType eventTypeForTouchPhase(UITouchPhase phase)
{
    switch (phase) {
    case UITouchPhaseBegan:
        return WebKit::WKTouchEventType::Begin;
    case UITouchPhaseMoved:
        return WebKit::WKTouchEventType::Change;
    case UITouchPhaseStationary:
        return WebKit::WKTouchEventType::Change; // Process (Moved, Stationary) touch sets as TouchChange.
    case UITouchPhaseEnded:
        return WebKit::WKTouchEventType::End;
    case UITouchPhaseCancelled:
        return WebKit::WKTouchEventType::Cancel;
    case UITouchPhaseRegionEntered:
        return WebKit::WKTouchEventType::Begin;
    case UITouchPhaseRegionMoved:
        return WebKit::WKTouchEventType::Change;
    case UITouchPhaseRegionExited:
        return WebKit::WKTouchEventType::End;
    }
    return WebKit::WKTouchEventType::Change;
}

static WebKit::WKTouchEventType lastExpectedWKEventTypeForTouches(NSSet *touches)
{
    auto lastPhase = UITouchPhaseBegan;
    for (UITouch *touch in touches) {
        if (touch.phase != UITouchPhaseStationary)
            lastPhase = std::max(lastPhase, touch.phase);
    }
    return eventTypeForTouchPhase(lastPhase);
}

- (void)performAction
{
    [_contentView _touchEventsRecognized];
}

- (BOOL)_hasActiveTouchesForEvent:(UIEvent *)event
{
    for (UITouch *touch in [event touchesForGestureRecognizer:self]) {
        if (touch.phase < UITouchPhaseEnded)
            return YES;
    }
    return NO;
}

- (void)_processTouches:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event type:(WebKit::WKTouchEventType)type
{
    // WebCore expects only one event for each distinct set of touches. Gesture recognizer will call
    // all applicable touchesBegan:, touchesMoved: and so on. If two things happen simultaneously, like this:
    //     WebKit::WKTouchEventType::Change (UITouchPhaseMoved, UITouchPhaseEnded)
    //     WebKit::WKTouchEventType::End    (UITouchPhaseMoved, UITouchPhaseEnded)
    // we should only process and send the last of those into WebCore. Note touches is actually the set returned
    // by [event touchesForGestureRecognizer:self], not the set of touches passed in to touchesBegan:withEvent:.
    // See <rdar://8398098> for more detail.
    if (lastExpectedWKEventTypeForTouches(touches) != type)
        return;

    [self _recordTouches:touches type:type coalescedTouches:[event coalescedTouchesForTouch:touches.anyObject] predictedTouches:[event predictedTouchesForTouch:touches.anyObject]];

    [self performAction];

    // If default was prevented, set the state to cancel other recognizers.
    if (_defaultPrevented)
        self.state = (self.state == UIGestureRecognizerStatePossible) ? UIGestureRecognizerStateBegan : UIGestureRecognizerStateChanged;

    // Check if the events are done.
    if (type != WebKit::WKTouchEventType::End && type != WebKit::WKTouchEventType::Cancel)
        return;

    // Check if all the active touches are gone.
    if ([self _hasActiveTouchesForEvent:event])
        return;

    // Check if this gesture recognizer has ever set state.
    if (self.state == UIGestureRecognizerStatePossible) {
        // Setting gesture recognizer to Failed will result with
        // a call to -_resetGestureRecognizer
        self.state = UIGestureRecognizerStateFailed;
        return;
    }

    // Time to change the state.
    if (type == WebKit::WKTouchEventType::End)
        self.state = UIGestureRecognizerStateEnded;
    else if (type == WebKit::WKTouchEventType::Cancel)
        self.state = UIGestureRecognizerStateCancelled;
}

- (BOOL)canBePreventedByGestureRecognizer:(UIGestureRecognizer *)preventingGestureRecognizer
{
    // This allows this gesture recognizer to persist, even if other gesture recognizers are recognized.
    return NO;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    auto activeTouches = [event touchesForGestureRecognizer:self];

    RetainPtr contentView = [self contentView];
    // Only hitTest when when we haven't already passed the hitTest.
    if (!_passedHitTest) {
        if ([contentView _shouldIgnoreTouchEvent:event]) {
            self.state = UIGestureRecognizerStateFailed;
            return;
        }
        _passedHitTest = YES;
    }

    [self _processTouches:activeTouches withEvent:event type:WebKit::WKTouchEventType::Begin];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _processTouches:[event touchesForGestureRecognizer:self] withEvent:event type:WebKit::WKTouchEventType::Change];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _processTouches:[event touchesForGestureRecognizer:self] withEvent:event type:WebKit::WKTouchEventType::End];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _processTouches:[event touchesForGestureRecognizer:self] withEvent:event type:WebKit::WKTouchEventType::Cancel];
}

- (const WebKit::WKTouchEvent&)lastTouchEvent
{
    return _lastTouchEvent;
}

@end

#endif // PLATFORM(IOS_FAMILY)
