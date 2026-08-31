/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "SyntheticNSEvent.h"

#if PLATFORM(MAC)

#import "CoreGraphicsTestSPI.h"
#import <pal/spi/cocoa/IOKitSPI.h>
#import <wtf/Assertions.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

@interface NSApplication (SyntheticNSEventDetails)
- (void)_setCurrentEvent:(NSEvent *)event;
@end

@interface NSEvent (SyntheticNSEventDetails)
- (instancetype)_initWithCGEvent:(CGEventRef)event eventRef:(void *)eventRef;
@end

static CGSGesturePhase gesturePhaseFromNSEventPhase(NSEventPhase phase)
{
    switch (phase) {
    case NSEventPhaseMayBegin:
        return kCGSGesturePhaseMayBegin;

    case NSEventPhaseBegan:
        return kCGSGesturePhaseBegan;

    case NSEventPhaseChanged:
        return kCGSGesturePhaseChanged;

    case NSEventPhaseCancelled:
        return kCGSGesturePhaseCancelled;

    case NSEventPhaseEnded:
        return kCGSGesturePhaseEnded;

    case NSEventPhaseNone:
    default:
        return kCGSGesturePhaseNone;
    }
}

@implementation SyntheticNSEvent {
    NSPoint _syntheticGlobalLocation;
    NSPoint _syntheticLocationInWindow;
    NSInteger _syntheticStage;
    float _syntheticPressure;
    CGFloat _syntheticMagnification;
    CGFloat _syntheticStageTransition;
    NSEventPhase _syntheticPhase;
    NSEventPhase _syntheticMomentumPhase;
    NSTimeInterval _syntheticTimestamp;
    NSInteger _syntheticEventNumber;
    short _syntheticSubtype;
    NSEventType _syntheticType;
    NSWindow *_syntheticWindow;
}

- (instancetype)initPressureEventAtLocation:(NSPoint)location globalLocation:(NSPoint)globalLocation stage:(NSInteger)stage pressure:(float)pressure stageTransition:(float)stageTransition phase:(NSEventPhase)phase time:(NSTimeInterval)time eventNumber:(NSInteger)eventNumber window:(NSWindow *)window
{
    RetainPtr cgEvent = adoptCF(CGEventCreate(nullptr));
    CGEventSetType(cgEvent, (CGEventType)kCGSEventGesture);
    CGEventSetIntegerValueField(cgEvent, kCGEventGestureHIDType, kIOHIDEventTypeForce);
    CGEventSetIntegerValueField(cgEvent, kCGEventGesturePhase, gesturePhaseFromNSEventPhase(phase));
    CGEventSetDoubleValueField(cgEvent, kCGEventStagePressure, pressure);
    CGEventSetDoubleValueField(cgEvent, kCGEventTransitionProgress, pressure);
    CGEventSetIntegerValueField(cgEvent, kCGEventGestureStage, stageTransition);
    CGEventSetIntegerValueField(cgEvent, kCGEventGestureBehavior, kCGSGestureBehaviorDeepPress);

    self = [super _initWithCGEvent:cgEvent eventRef:nullptr];

    if (!self)
        return nil;

    _syntheticLocationInWindow = location;
    _syntheticGlobalLocation = globalLocation;
    _syntheticStage = stage;
    _syntheticPressure = pressure;
    _syntheticStageTransition = stageTransition;
    _syntheticPhase = phase;
    _syntheticTimestamp = time;
    _syntheticEventNumber = eventNumber;
    _syntheticWindow = window;
    _syntheticType = NSEventTypePressure;

    return self;
}

- (id)initMagnifyEventAtLocation:(NSPoint)location globalLocation:(NSPoint)globalLocation magnification:(CGFloat)magnification phase:(NSEventPhase)phase time:(NSTimeInterval)time eventNumber:(NSInteger)eventNumber window:(NSWindow *)window
{
    RetainPtr cgEvent = adoptCF(CGEventCreate(nullptr));
    CGEventSetType(cgEvent, (CGEventType)kCGSEventGesture);
    CGEventSetIntegerValueField(cgEvent, kCGEventGestureHIDType, kIOHIDEventTypeZoom);
    CGEventSetIntegerValueField(cgEvent, kCGEventGesturePhase, gesturePhaseFromNSEventPhase(phase));
    CGEventSetDoubleValueField(cgEvent, kCGEventGestureZoomValue, magnification);

    if (!(self = [super _initWithCGEvent:cgEvent eventRef:nullptr]))
        return nil;

    _syntheticLocationInWindow = location;
    _syntheticGlobalLocation = globalLocation;
    _syntheticMagnification = magnification;
    _syntheticPhase = phase;
    _syntheticTimestamp = time;
    _syntheticEventNumber = eventNumber;
    _syntheticWindow = window;
    _syntheticType = NSEventTypeMagnify;

    return self;
}

- (id)initSmartMagnifyEventAtLocation:(NSPoint)location globalLocation:(NSPoint)globalLocation time:(NSTimeInterval)time eventNumber:(NSInteger)eventNumber window:(NSWindow *)window
{
    RetainPtr cgEvent = adoptCF(CGEventCreate(nullptr));
    CGEventSetType(cgEvent, (CGEventType)kCGSEventGesture);
    CGEventSetIntegerValueField(cgEvent, kCGEventGestureHIDType, kIOHIDEventTypeZoomToggle);

    if (!(self = [super _initWithCGEvent:cgEvent eventRef:nullptr]))
        return nil;

    _syntheticType = NSEventTypeSmartMagnify;
    _syntheticLocationInWindow = location;
    _syntheticGlobalLocation = globalLocation;
    _syntheticTimestamp = time;
    _syntheticWindow = window;

    return self;
}


- (CGFloat)stageTransition
{
    return _syntheticStageTransition;
}

- (NSTimeInterval)timestamp
{
    return _syntheticTimestamp;
}

- (NSEventType)type
{
    return _syntheticType;
}

- (NSEventSubtype)subtype
{
    return (NSEventSubtype)_syntheticSubtype;
}

- (NSPoint)locationInWindow
{
    return _syntheticLocationInWindow;
}

- (NSPoint)location
{
    return _syntheticGlobalLocation;
}

- (NSInteger)stage
{
    return _syntheticStage;
}

- (float)pressure
{
    return _syntheticPressure;
}

- (CGFloat)magnification
{
    return _syntheticMagnification;
}

- (NSEventPhase)phase
{
    return _syntheticPhase;
}

- (NSEventPhase)momentumPhase
{
    return _syntheticMomentumPhase;
}

- (NSInteger)eventNumber
{
    return _syntheticEventNumber;
}

- (BOOL)_isTouchesEnded
{
    return false;
}

- (NSWindow *)window
{
    return _syntheticWindow;
}

@end

void dispatchSyntheticEvent(NSEvent *event, NSView *platformView, NSString *description, void (^dispatch)(NSView *, NSEvent *))
{
    NSPoint locationInWindow = [event locationInWindow];
    RetainPtr targetView = [platformView hitTest:locationInWindow];
    if (!targetView) {
        LOG_ERROR("%@ failed to find the target view at %@\n", description, NSStringFromPoint(locationInWindow));
        return;
    }

    [NSApp _setCurrentEvent:event];
    dispatch(targetView, event);
    [NSApp _setCurrentEvent:nil];
}

#endif
