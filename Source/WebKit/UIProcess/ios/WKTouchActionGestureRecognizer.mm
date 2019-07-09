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
#import "WKTouchActionGestureRecognizer.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(POINTER_EVENTS)

#import <UIKit/UIGestureRecognizerSubclass.h>
#import <wtf/HashMap.h>

@implementation WKTouchActionGestureRecognizer {
    HashMap<unsigned, OptionSet<WebCore::TouchAction>> _touchActionsByTouchIdentifier;
    id <WKTouchActionGestureRecognizerDelegate> _touchActionDelegate;
}

- (id)initWithTouchActionDelegate:(id <WKTouchActionGestureRecognizerDelegate>)touchActionDelegate
{
    if (self = [super init])
        _touchActionDelegate = touchActionDelegate;
    return self;
}

- (void)setTouchActions:(OptionSet<WebCore::TouchAction>)touchActions forTouchIdentifier:(unsigned)touchIdentifier
{
    ASSERT(!touchActions.contains(WebCore::TouchAction::Auto));
    _touchActionsByTouchIdentifier.set(touchIdentifier, touchActions);
}

- (void)clearTouchActionsForTouchIdentifier:(unsigned)touchIdentifier
{
    _touchActionsByTouchIdentifier.remove(touchIdentifier);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _updateState];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _updateState];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _updateState];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _updateState];
}

- (void)_updateState
{
    // We always want to be in a recognized state so that we may always prevent another gesture recognizer.
    [self setState:UIGestureRecognizerStateRecognized];
}

- (BOOL)canBePreventedByGestureRecognizer:(UIGestureRecognizer *)preventingGestureRecognizer
{
    // This allows this gesture recognizer to persist, even if other gesture recognizers are recognized.
    return NO;
}

- (BOOL)canPreventGestureRecognizer:(UIGestureRecognizer *)preventedGestureRecognizer
{
    if (_touchActionsByTouchIdentifier.isEmpty())
        return NO;

    auto mayPan = [_touchActionDelegate gestureRecognizerMayPanWebView:preventedGestureRecognizer];
    auto mayPinchToZoom = [_touchActionDelegate gestureRecognizerMayPinchToZoomWebView:preventedGestureRecognizer];
    auto mayDoubleTapToZoom = [_touchActionDelegate gestureRecognizerMayDoubleTapToZoomWebView:preventedGestureRecognizer];

    if (!mayPan && !mayPinchToZoom && !mayDoubleTapToZoom)
        return NO;

    // Now that we've established that this gesture recognizer may yield an interaction that is preventable by the "touch-action"
    // CSS property we iterate over all active touches, check whether that touch matches the gesture recognizer, see if we have
    // any touch-action specified for it, and then check for each type of interaction whether the touch-action property has a
    // value that should prevent the interaction.
    auto* activeTouches = [_touchActionDelegate touchActionActiveTouches];
    for (NSNumber *touchIdentifier in activeTouches) {
        auto iterator = _touchActionsByTouchIdentifier.find([touchIdentifier unsignedIntegerValue]);
        if (iterator != _touchActionsByTouchIdentifier.end() && [[activeTouches objectForKey:touchIdentifier].gestureRecognizers containsObject:preventedGestureRecognizer]) {
            // Panning is only allowed if "pan-x", "pan-y" or "manipulation" is specified. Additional work is needed to respect individual values, but this takes
            // care of the case where no panning is allowed.
            if (mayPan && !iterator->value.containsAny({ WebCore::TouchAction::PanX, WebCore::TouchAction::PanY, WebCore::TouchAction::Manipulation }))
                return YES;
            // Pinch-to-zoom is only allowed if "pinch-zoom" or "manipulation" is specified.
            if (mayPinchToZoom && !iterator->value.containsAny({ WebCore::TouchAction::PinchZoom, WebCore::TouchAction::Manipulation }))
                return YES;
            // Double-tap-to-zoom is only disallowed if "none" is specified.
            if (mayDoubleTapToZoom && iterator->value.contains(WebCore::TouchAction::None))
                return YES;
        }
    }

    return NO;
}

@end

#endif
