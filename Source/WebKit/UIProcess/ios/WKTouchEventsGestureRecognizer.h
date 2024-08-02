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

#pragma once

#if PLATFORM(IOS_FAMILY)

#import "WKTouchEventsGestureRecognizerTypes.h"
#import <UIKit/UIKit.h>
#import <wtf/Vector.h>

namespace WebKit {

struct WKTouchPoint {
    CGPoint locationInScreenCoordinates;
    CGPoint locationInDocumentCoordinates;
    unsigned identifier { 0 };
    UITouchPhase phase { UITouchPhaseBegan };
    CGFloat majorRadiusInScreenCoordinates { 0 };
    CGFloat force { 0 };
    CGFloat altitudeAngle { 0 };
    CGFloat azimuthAngle { 0 };
    WKTouchPointType touchType { WKTouchPointType::Direct };
};

struct WKTouchEvent {
    WKTouchEventType type { WKTouchEventType::Begin };
    NSTimeInterval timestamp { 0 };
    CGPoint locationInScreenCoordinates;
    CGPoint locationInDocumentCoordinates;
    CGFloat scale { 0 };
    CGFloat rotation { 0 };

    bool inJavaScriptGesture { false };

    Vector<WKTouchPoint> touchPoints;
    Vector<WKTouchEvent> coalescedEvents;
    Vector<WKTouchEvent> predictedEvents;
    bool isPotentialTap { false };
};

} // namespace WebKit

@class WKTouchEventsGestureRecognizer;

@protocol WKTouchEventsGestureRecognizerDelegate <NSObject>
- (BOOL)isAnyTouchOverActiveArea:(NSSet *)touches;
@optional
- (BOOL)shouldIgnoreTouchEvent;
- (BOOL)gestureRecognizer:(WKTouchEventsGestureRecognizer *)gestureRecognizer shouldIgnoreTouchEvent:(UIEvent *)event;
@end

@interface WKTouchEventsGestureRecognizer : UIGestureRecognizer
- (id)initWithTarget:(id)target action:(SEL)action touchDelegate:(id<WKTouchEventsGestureRecognizerDelegate>)delegate;
- (void)cancel;

@property (nonatomic, getter=isDefaultPrevented) BOOL defaultPrevented;

@property (nonatomic, readonly) const WebKit::WKTouchEvent& lastTouchEvent;
@property (nonatomic, readonly, getter=isDispatchingTouchEvents) BOOL dispatchingTouchEvents;
@property (nonatomic, readonly) NSMapTable<NSNumber *, UITouch *> *activeTouchesByIdentifier;

@end

#endif // PLATFORM(IOS_FAMILY)
