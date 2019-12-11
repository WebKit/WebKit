/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "EventSerializerMac.h"

#if PLATFORM(MAC)

#import "CoreGraphicsTestSPI.h"
#import <algorithm>
#import <mach/mach_time.h>
#import <pal/spi/cocoa/IOKitSPI.h>
#import <wtf/RetainPtr.h>

#define MOUSE_EVENT_TYPES \
    (CGSEventType)kCGEventLeftMouseDown, \
    (CGSEventType)kCGEventLeftMouseUp, \
    (CGSEventType)kCGEventRightMouseDown, \
    (CGSEventType)kCGEventRightMouseUp, \
    (CGSEventType)kCGEventMouseMoved, \
    (CGSEventType)kCGEventLeftMouseDragged, \
    (CGSEventType)kCGEventRightMouseDragged, \
    (CGSEventType)kCGEventOtherMouseDown, \
    (CGSEventType)kCGEventOtherMouseUp, \
    (CGSEventType)kCGEventOtherMouseDragged

#define KEY_EVENT_TYPES \
    (CGSEventType)kCGEventKeyDown, \
    (CGSEventType)kCGEventKeyUp, \
    (CGSEventType)kCGEventFlagsChanged

#define GESTURE_EVENT_TYPES \
    kCGSEventGesture, \
    kCGSEventFluidTouchGesture, \
    kCGSEventDockControl

bool eventIsOfType(CGEventRef event, CGSEventType type)
{
    return (CGSEventType)CGEventGetType(event) == type;
}

bool eventIsOfTypes(CGEventRef) { return false; }

template<typename ... Types>
bool eventIsOfTypes(CGEventRef event, CGSEventType first, Types ... rest)
{
    return eventIsOfType(event, first) || eventIsOfTypes(event, rest...);
}

bool eventIsOfGestureType(CGEventRef event, IOHIDEventType type)
{
    return (IOHIDEventType)CGEventGetIntegerValueField(event, kCGEventGestureHIDType) == type;
}

bool eventIsOfGestureTypes(CGEventRef) { return false; }

template<typename ... Types>
bool eventIsOfGestureTypes(CGEventRef event, IOHIDEventType first, Types ... rest)
{
    if (!eventIsOfTypes(event, GESTURE_EVENT_TYPES))
        return false;
    return eventIsOfGestureType(event, first) || eventIsOfGestureTypes(event, rest...);
}

#define FOR_EACH_CGEVENT_INTEGER_FIELD(macro) \
    macro(true, kCGSEventTypeField) \
    \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventDeltaAxis1) \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventDeltaAxis2) \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventDeltaAxis3) \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventIsContinuous) \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventMomentumPhase) \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventPointDeltaAxis1) \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventPointDeltaAxis2) \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventPointDeltaAxis3) \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventScrollCount) \
    macro(eventIsOfTypes(rawEvent, kCGSEventScrollWheel, kCGSEventZoom), kCGScrollWheelEventScrollPhase) \
    \
    macro(eventIsOfTypes(rawEvent, MOUSE_EVENT_TYPES), kCGMouseEventButtonNumber) \
    macro(eventIsOfTypes(rawEvent, MOUSE_EVENT_TYPES), kCGMouseEventClickState) \
    macro(eventIsOfTypes(rawEvent, MOUSE_EVENT_TYPES), kCGMouseEventDeltaX) \
    macro(eventIsOfTypes(rawEvent, MOUSE_EVENT_TYPES), kCGMouseEventDeltaY) \
    macro(eventIsOfTypes(rawEvent, MOUSE_EVENT_TYPES), kCGMouseEventSubtype) \
    macro(eventIsOfTypes(rawEvent, MOUSE_EVENT_TYPES), kCGMouseEventNumber) \
    \
    macro(eventIsOfTypes(rawEvent, KEY_EVENT_TYPES), kCGKeyboardEventAutorepeat) \
    macro(eventIsOfTypes(rawEvent, KEY_EVENT_TYPES), kCGKeyboardEventKeyboardType) \
    macro(eventIsOfTypes(rawEvent, KEY_EVENT_TYPES), kCGKeyboardEventKeycode) \
    \
    macro(eventIsOfTypes(rawEvent, GESTURE_EVENT_TYPES), kCGEventGestureHIDType) \
    macro(eventIsOfTypes(rawEvent, GESTURE_EVENT_TYPES), kCGEventGestureBehavior) \
    macro(eventIsOfTypes(rawEvent, GESTURE_EVENT_TYPES), kCGEventGestureFlavor) \
    macro(eventIsOfTypes(rawEvent, GESTURE_EVENT_TYPES), kCGEventGestureMask) \
    macro(eventIsOfTypes(rawEvent, GESTURE_EVENT_TYPES), kCGEventGesturePhase) \
    macro(eventIsOfTypes(rawEvent, GESTURE_EVENT_TYPES), kCGEventGestureStage) \
    \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeScroll), kCGEventScrollGestureFlagBits) \
    \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventSwipeGestureFlagBits) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureSwipeMask) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureSwipeMotion) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureSwipeValue) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureFlavor) \
    \
    macro(eventIsOfGestureTypes(rawEvent, kCGHIDEventTypeGestureStarted, kCGHIDEventTypeGestureEnded), kCGEventGestureStartEndSeriesType)

#define FOR_EACH_CGEVENT_DOUBLE_FIELD(macro) \
    macro(eventIsOfTypes(rawEvent, GESTURE_EVENT_TYPES), kCGEventGestureProgress) \
    \
    macro(eventIsOfTypes(rawEvent, MOUSE_EVENT_TYPES), kCGMouseEventPressure) \
    \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeZoom), kCGEventGestureZoomDeltaX) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeZoom), kCGEventGestureZoomDeltaY) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeZoom), kCGEventGestureZoomValue) \
    \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeRotation), kCGEventGestureRotationValue) \
    \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeScroll), kCGEventGestureScrollX) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeScroll), kCGEventGestureScrollY) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeScroll), kCGEventGestureScrollZ) \
    \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureSwipePositionX) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureSwipePositionY) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureSwipeProgress) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureSwipeVelocityX) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureSwipeVelocityY) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeNavigationSwipe), kCGEventGestureSwipeVelocityZ) \
    \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeForce), kCGEventTransitionProgress) \
    macro(eventIsOfGestureType(rawEvent, kIOHIDEventTypeForce), kCGEventStagePressure)

#define LOAD_INTEGER_FIELD_FROM_EVENT(eventTypeFilter, field) \
^ { \
    if (!(eventTypeFilter)) \
        return; \
    int64_t value = CGEventGetIntegerValueField(rawEvent, field); \
    int64_t plainValue = CGEventGetIntegerValueField(rawPlainEvent, field); \
    if (value != plainValue) \
        dict[@#field] = @(value); \
}();

#define LOAD_DOUBLE_FIELD_FROM_EVENT(eventTypeFilter, field) \
^ { \
    if (!(eventTypeFilter)) \
        return; \
    double value = CGEventGetDoubleValueField(rawEvent, field); \
    if (!isnan(value)) { \
        double plainValue = CGEventGetDoubleValueField(rawPlainEvent, field); \
        if (fabs(value - plainValue) >= FLT_EPSILON) \
            dict[@#field] = @(value); \
    } \
}();

#define STORE_INTEGER_FIELD_TO_EVENT(eventTypeFilter, field) \
^ { \
    if (!(eventTypeFilter)) \
        return; \
    NSNumber *value = dict[@#field]; \
    if (value) \
        CGEventSetIntegerValueField(rawEvent, field, value.unsignedLongLongValue); \
}();

#define STORE_DOUBLE_FIELD_TO_EVENT(eventTypeFilter, field) \
^ { \
    if (!(eventTypeFilter)) \
        return; \
    NSNumber *value = dict[@#field]; \
    if (value) \
        CGEventSetDoubleValueField(rawEvent, field, value.doubleValue); \
}();

@implementation EventSerializer

+ (NSDictionary *)dictionaryForEvent:(CGEventRef)rawEvent relativeToTime:(CGEventTimestamp)referenceTimestamp
{
    RetainPtr<CGEventRef> plainEvent = adoptCF(CGEventCreate(NULL));
    CGEventRef rawPlainEvent = plainEvent.get();

    NSMutableDictionary *dict = [[[NSMutableDictionary alloc] init] autorelease];

    FOR_EACH_CGEVENT_INTEGER_FIELD(LOAD_INTEGER_FIELD_FROM_EVENT);
    FOR_EACH_CGEVENT_DOUBLE_FIELD(LOAD_DOUBLE_FIELD_FROM_EVENT);

    CGEventTimestamp timestamp = CGEventGetTimestamp(rawEvent);
    dict[@"relativeTimeMS"] = @(std::max(static_cast<double>(timestamp - referenceTimestamp) / NSEC_PER_MSEC, 0.0));

    CGSEventType eventType = (CGSEventType)CGEventGetIntegerValueField(rawEvent, kCGSEventTypeField);
    if (eventType == kCGSEventGesture || eventType == kCGSEventFluidTouchGesture || eventType == kCGSEventDockControl) {
        if (CGEventGetIntegerValueField(rawEvent, kCGEventGestureIsPreflight)) {
            dict[@"kCGEventGestureIsPreflight"] = @YES;
            dict[@"kCGEventGesturePreflightProgress"] = @(CGEventGetDoubleValueField(rawEvent, kCGEventGesturePreflightProgress));
        }
    }

    dict[@"windowLocation"] = NSStringFromPoint(NSPointFromCGPoint(CGEventGetWindowLocation(rawEvent)));

    auto flags = static_cast<CGEventFlags>(CGEventGetFlags(rawEvent) & ~NX_NONCOALSESCEDMASK);
    auto plainFlags = static_cast<CGEventFlags>(CGEventGetFlags(rawPlainEvent) & ~NX_NONCOALSESCEDMASK);
    if (flags != plainFlags)
        dict[@"flags"] = @(flags);

    return dict;
}

+ (RetainPtr<CGEventRef>)createEventForDictionary:(NSDictionary *)dict inWindow:(NSWindow *)window relativeToTime:(CGEventTimestamp)referenceTimestamp
{
    RetainPtr<CGEventRef> event = adoptCF(CGEventCreate(NULL));
    CGEventRef rawEvent = event.get();

    FOR_EACH_CGEVENT_INTEGER_FIELD(STORE_INTEGER_FIELD_TO_EVENT);
    FOR_EACH_CGEVENT_DOUBLE_FIELD(STORE_DOUBLE_FIELD_TO_EVENT);

    if (dict[@"relativeTimeMS"])
        CGEventSetTimestamp(event.get(), referenceTimestamp + static_cast<CGEventTimestamp>(([dict[@"relativeTimeMS"] doubleValue] * NSEC_PER_MSEC)));

    if ([dict[@"kCGEventGestureIsPreflight"] boolValue]) {
        CGEventSetIntegerValueField(rawEvent, kCGEventGestureIsPreflight, 1);
        CGEventSetDoubleValueField(rawEvent, kCGEventGesturePreflightProgress, [dict[@"kCGEventGesturePreflightProgress"] doubleValue]);
    }

    if (dict[@"windowLocation"]) {
        CGPoint windowLocation = NSPointToCGPoint(NSPointFromString(dict[@"windowLocation"]));
        CGEventSetWindowLocation(rawEvent, windowLocation);

        NSPoint screenPoint = [window convertRectToScreen:NSMakeRect(windowLocation.x, windowLocation.y, 1, 1)].origin;
        CGEventSetLocation(rawEvent, CGPointMake(screenPoint.x, NSScreen.screens.firstObject.frame.size.height - screenPoint.y));
    }

    if (dict[@"flags"])
        CGEventSetFlags(rawEvent, static_cast<CGEventFlags>([dict[@"flags"] unsignedLongLongValue]));

    CGEventSetIntegerValueField(rawEvent, kCGSEventWindowIDField, window.windowNumber);

    return event;
}

@end

@implementation EventStreamPlayer

const float eventDispatchTimerRate = 1. / 120.;

+ (void)playStream:(NSArray<NSDictionary *> *)eventDicts window:(NSWindow *)window completionHandler:(void(^)())completionHandler
{
    RetainPtr<EventStreamPlayer> player = adoptNS([[EventStreamPlayer alloc] init]);

    player->_remainingEventDictionaries = adoptNS([eventDicts mutableCopy]);
    player->_window = window;

    if (completionHandler)
        player->_completionHandler = makeBlockPtr(completionHandler);

    player->_startTime = mach_absolute_time();

    [NSTimer scheduledTimerWithTimeInterval:eventDispatchTimerRate target:player.get() selector:@selector(playbackTimerFired:) userInfo:nil repeats:YES];
}

- (void)playbackTimerFired:(NSTimer *)timer
{
    auto removeList = adoptNS([[NSMutableArray alloc] init]);
    NSEvent *nsEvent = nil;
    for (id eventDict in _remainingEventDictionaries.get()) {
        RetainPtr<CGEventRef> event = [EventSerializer createEventForDictionary:eventDict inWindow:_window.get() relativeToTime:_startTime];
        if (CGEventGetTimestamp(event.get()) < mach_absolute_time()) {
            nsEvent = [NSEvent eventWithCGEvent:event.get()];
            [NSApp postEvent:nsEvent atStart:NO];
            [removeList addObject:eventDict];
        }
    }
    [_remainingEventDictionaries removeObjectsInArray:removeList.get()];

    if ([_remainingEventDictionaries count])
        return;

    NSEventType applicationDefinedEventType = NSEventTypeApplicationDefined;
    [NSApp postEvent:[NSEvent otherEventWithType:applicationDefinedEventType location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:0 subtype:0 data1:42 data2:0] atStart:NO];
    // Block until we send the last event we posted.
    while (true) {
        NSEvent *nextEvent = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate dateWithTimeIntervalSinceNow:eventDispatchTimerRate] inMode:NSDefaultRunLoopMode dequeue:YES];
        if (nextEvent.type == applicationDefinedEventType && nextEvent.data1 == 42)
            break;
        if (nextEvent)
            [NSApp sendEvent:nextEvent];
    }

    if (_completionHandler)
        _completionHandler();

    [timer invalidate];
}

@end

#endif
