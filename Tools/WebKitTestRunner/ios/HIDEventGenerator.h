/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "UIKitSPI.h"

#import <CoreGraphics/CGGeometry.h>

// Keys for sendEventStream:completionBlock:.
extern NSString* const TopLevelEventInfoKey;
extern NSString* const HIDEventInputType;
extern NSString* const HIDEventTimeOffsetKey;
extern NSString* const HIDEventTouchesKey;
extern NSString* const HIDEventPhaseKey;
extern NSString* const HIDEventInterpolateKey;
extern NSString* const HIDEventTimestepKey;
extern NSString* const HIDEventCoordinateSpaceKey;
extern NSString* const HIDEventStartEventKey;
extern NSString* const HIDEventEndEventKey;
extern NSString* const HIDEventTouchIDKey;
extern NSString* const HIDEventPressureKey;
extern NSString* const HIDEventXKey;
extern NSString* const HIDEventYKey;
extern NSString* const HIDEventTwistKey;
extern NSString* const HIDEventMajorRadiusKey;
extern NSString* const HIDEventMinorRadiusKey;

// Values for HIDEventInputType.
extern NSString* const HIDEventInputTypeHand;
extern NSString* const HIDEventInputTypeFinger;
extern NSString* const HIDEventInputTypeStylus;

// Values for HIDEventCoordinateSpaceKey.
extern NSString* const HIDEventCoordinateSpaceTypeGlobal;
extern NSString* const HIDEventCoordinateSpaceTypeContent;

extern NSString* const HIDEventInterpolationTypeLinear;
extern NSString* const HIDEventInterpolationTypeSimpleCurve;

// Values for HIDEventPhaseKey.
extern NSString* const HIDEventPhaseBegan;
extern NSString* const HIDEventPhaseMoved;
extern NSString* const HIDEventPhaseEnded;
extern NSString* const HIDEventPhaseCanceled;

// Values for touch counts, etc, to keep debug code in sync

extern NSUInteger const HIDMaxTouchCount;

@interface HIDEventGenerator : NSObject

+ (HIDEventGenerator *)sharedHIDEventGenerator;

// Touches
- (void)touchDown:(CGPoint)location touchCount:(NSUInteger)count completionBlock:(void (^)(void))completionBlock;
- (void)liftUp:(CGPoint)location touchCount:(NSUInteger)count completionBlock:(void (^)(void))completionBlock;

// Stylus
- (void)stylusDownAtPoint:(CGPoint)location azimuthAngle:(CGFloat)azimuthAngle altitudeAngle:(CGFloat)altitudeAngle pressure:(CGFloat)pressure completionBlock:(void (^)(void))completionBlock;
- (void)stylusMoveToPoint:(CGPoint)location azimuthAngle:(CGFloat)azimuthAngle altitudeAngle:(CGFloat)altitudeAngle pressure:(CGFloat)pressure completionBlock:(void (^)(void))completionBlock;
- (void)stylusUpAtPoint:(CGPoint)location completionBlock:(void (^)(void))completionBlock;
- (void)stylusTapAtPoint:(CGPoint)location azimuthAngle:(CGFloat)azimuthAngle altitudeAngle:(CGFloat)altitudeAngle pressure:(CGFloat)pressure completionBlock:(void (^)(void))completionBlock;

// Taps
- (void)tap:(CGPoint)location completionBlock:(void (^)(void))completionBlock;
- (void)doubleTap:(CGPoint)location completionBlock:(void (^)(void))completionBlock;
- (void)twoFingerTap:(CGPoint)location completionBlock:(void (^)(void))completionBlock;

// Long Press
- (void)longPress:(CGPoint)location completionBlock:(void (^)(void))completionBlock;

// Drags
- (void)dragWithStartPoint:(CGPoint)startLocation endPoint:(CGPoint)endLocation duration:(double)seconds completionBlock:(void (^)(void))completionBlock;

// Pinches
- (void)pinchCloseWithStartPoint:(CGPoint)startLocation endPoint:(CGPoint)endLocation duration:(double)seconds completionBlock:(void (^)(void))completionBlock;
- (void)pinchOpenWithStartPoint:(CGPoint)startLocation endPoint:(CGPoint)endLocation duration:(double)seconds completionBlock:(void (^)(void))completionBlock;

// Event stream
- (void)sendEventStream:(NSDictionary *)eventInfo completionBlock:(void (^)(void))completionBlock;

- (void)markerEventReceived:(IOHIDEventRef)event;
- (BOOL)checkForOutstandingCallbacks;

// Keyboard
- (void)keyPress:(NSString *)character completionBlock:(void (^)(void))completionBlock;
- (void)keyDown:(NSString *)character completionBlock:(void (^)(void))completionBlock;
- (void)keyUp:(NSString *)character completionBlock:(void (^)(void))completionBlock;

@end
