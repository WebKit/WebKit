/*
 * Copyright (C) 2015, 2019 Apple Inc. All rights reserved.
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
#import "_WKTouchEventGenerator.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import <mach/mach_time.h>
#import <pal/spi/cocoa/IOKitSPI.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(BackBoardServices)
SOFT_LINK(BackBoardServices, BKSHIDEventSetDigitizerInfo, void, (IOHIDEventRef digitizerEvent, uint32_t contextID, uint8_t systemGestureisPossible, uint8_t isSystemGestureStateChangeEvent, CFStringRef displayUUID, CFTimeInterval initialTouchTimestamp, float maxForce), (digitizerEvent, contextID, systemGestureisPossible, isSystemGestureStateChangeEvent, displayUUID, initialTouchTimestamp, maxForce));

static const NSTimeInterval fingerMoveInterval = 0.016;
static const IOHIDFloat defaultMajorRadius = 5;
static const IOHIDFloat defaultPathPressure = 0;
static const long nanosecondsPerSecond = 1e9;

NSUInteger const HIDMaxTouchCount = 5;

static int fingerIdentifiers[HIDMaxTouchCount] = { 2, 3, 4, 5, 1 };

typedef enum {
    HandEventNull,
    HandEventTouched,
    HandEventMoved,
    HandEventChordChanged,
    HandEventLifted,
    HandEventCanceled,
} HandEventType;

typedef struct {
    int identifier;
    CGPoint point;
    IOHIDFloat pathMajorRadius;
    IOHIDFloat pathPressure;
    UInt8 pathProximity;
} SyntheticEventDigitizerInfo;

static CFTimeInterval secondsSinceAbsoluteTime(CFAbsoluteTime startTime)
{
    return CFAbsoluteTimeGetCurrent() - startTime;
}

static double simpleCurveInterpolation(double a, double b, double t)
{
    return a + (b - a) * sin(sin(t * M_PI / 2) * t * M_PI / 2);
}

static CGPoint calculateNextCurveLocation(CGPoint a, CGPoint b, CFTimeInterval t)
{
    return CGPointMake(simpleCurveInterpolation(a.x, b.x, t), simpleCurveInterpolation(a.y, b.y, t));
}

static void delayBetweenMove(int eventIndex, double elapsed)
{
    // Delay next event until expected elapsed time.
    double delay = (eventIndex * fingerMoveInterval) - elapsed;
    if (delay > 0) {
        struct timespec moveDelay = { 0, static_cast<long>(delay * nanosecondsPerSecond) };
        nanosleep(&moveDelay, NULL);
    }   
}

// NOTE: this event synthesizer is derived from WebKitTestRunner code.
// Compared to that version, this lacks support for stylus event simulation,
// event stream, and only single touches are exposed via the touch/lift/move method calls.
@interface _WKTouchEventGenerator ()
@property (nonatomic, strong) NSMutableDictionary *eventCallbacks;
@end

@implementation _WKTouchEventGenerator {
    IOHIDEventSystemClientRef _ioSystemClient;
    SyntheticEventDigitizerInfo _activePoints[HIDMaxTouchCount];
    NSUInteger _activePointCount;
}

+ (_WKTouchEventGenerator *)sharedTouchEventGenerator
{
    static _WKTouchEventGenerator *eventGenerator = [[_WKTouchEventGenerator alloc] init];
    return eventGenerator;
}

+ (CFIndex)nextEventCallbackID
{
    static CFIndex callbackID = 0;
    return ++callbackID;
}

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    for (NSUInteger i = 0; i < HIDMaxTouchCount; ++i)
        _activePoints[i].identifier = fingerIdentifiers[i];

    _eventCallbacks = [[NSMutableDictionary alloc] init];

    return self;
}

- (void)dealloc
{
  if (_ioSystemClient)
        CFRelease(_ioSystemClient);

    [_eventCallbacks release];
    [super dealloc];
}

- (IOHIDEventRef)_createIOHIDEventType:(HandEventType)eventType
{
    BOOL isTouching = (eventType == HandEventTouched || eventType == HandEventMoved || eventType == HandEventChordChanged);

    IOHIDDigitizerEventMask eventMask = kIOHIDDigitizerEventTouch;
    if (eventType == HandEventMoved) {
        eventMask &= ~kIOHIDDigitizerEventTouch;
        eventMask |= kIOHIDDigitizerEventPosition;
        eventMask |= kIOHIDDigitizerEventAttribute;
    } else if (eventType == HandEventChordChanged) {
        eventMask |= kIOHIDDigitizerEventPosition;
        eventMask |= kIOHIDDigitizerEventAttribute;
    } else if (eventType == HandEventTouched || eventType == HandEventCanceled || eventType == HandEventLifted)
        eventMask |= kIOHIDDigitizerEventIdentity;

    uint64_t machTime = mach_absolute_time();
    RetainPtr<IOHIDEventRef> eventRef = adoptCF(IOHIDEventCreateDigitizerEvent(kCFAllocatorDefault, machTime,
        kIOHIDDigitizerTransducerTypeHand,
        0,
        0,
        eventMask,
        0,
        0, 0, 0,
        0,
        0,
        0,
        isTouching,
        kIOHIDEventOptionNone));

    IOHIDEventSetIntegerValue(eventRef.get(), kIOHIDEventFieldDigitizerIsDisplayIntegrated, 1);

    for (NSUInteger i = 0; i < _activePointCount; ++i) {
        SyntheticEventDigitizerInfo* pointInfo = &_activePoints[i];
        if (eventType == HandEventTouched) {
            if (!pointInfo->pathMajorRadius)
                pointInfo->pathMajorRadius = defaultMajorRadius;
            if (!pointInfo->pathPressure)
                pointInfo->pathPressure = defaultPathPressure;
            if (!pointInfo->pathProximity)
                pointInfo->pathProximity = kGSEventPathInfoInTouch | kGSEventPathInfoInRange;
        } else if (eventType == HandEventLifted || eventType == HandEventCanceled) {
            pointInfo->pathMajorRadius = 0;
            pointInfo->pathPressure = 0;
            pointInfo->pathProximity = 0;
        }

        CGPoint point = pointInfo->point;
        point = CGPointMake(roundf(point.x), roundf(point.y));
        
        RetainPtr<IOHIDEventRef> subEvent = adoptCF(IOHIDEventCreateDigitizerFingerEvent(kCFAllocatorDefault, machTime,
            pointInfo->identifier,
            pointInfo->identifier,
            eventMask,
            point.x, point.y, 0,
            pointInfo->pathPressure,
            0,
            pointInfo->pathProximity & kGSEventPathInfoInRange,
            pointInfo->pathProximity & kGSEventPathInfoInTouch,
            kIOHIDEventOptionNone));

        IOHIDEventSetFloatValue(subEvent.get(), kIOHIDEventFieldDigitizerMajorRadius, pointInfo->pathMajorRadius);
        IOHIDEventSetFloatValue(subEvent.get(), kIOHIDEventFieldDigitizerMinorRadius, pointInfo->pathMajorRadius);

        IOHIDEventAppendEvent(eventRef.get(), subEvent.get(), 0);
    }

    return eventRef.leakRef();
}

- (BOOL)_sendHIDEvent:(IOHIDEventRef)eventRef
{
    if (!_ioSystemClient)
        _ioSystemClient = IOHIDEventSystemClientCreate(kCFAllocatorDefault);

    if (eventRef) {
        RetainPtr<IOHIDEventRef> strongEvent = eventRef;
        dispatch_async(dispatch_get_main_queue(), ^{
            uint32_t contextID = [UIApplication sharedApplication].keyWindow._contextId;
            ASSERT(contextID);
            BKSHIDEventSetDigitizerInfo(strongEvent.get(), contextID, false, false, NULL, 0, 0);
            [[UIApplication sharedApplication] _enqueueHIDEvent:strongEvent.get()];
        });
    }
    return YES;
}

- (BOOL)_sendMarkerHIDEventWithCompletionBlock:(void (^)(void))completionBlock
{
    auto callbackID = [_WKTouchEventGenerator nextEventCallbackID];
    [_eventCallbacks setObject:Block_copy(completionBlock) forKey:@(callbackID)];

    auto markerEvent = adoptCF(IOHIDEventCreateVendorDefinedEvent(kCFAllocatorDefault,
        mach_absolute_time(),
        kHIDPage_VendorDefinedStart + 100,
        0,
        1,
        (uint8_t*)&callbackID,
        sizeof(CFIndex),
        kIOHIDEventOptionNone));
    
    if (markerEvent) {
        dispatch_async(dispatch_get_main_queue(), [markerEvent = WTFMove(markerEvent)] {
            auto contextID = [UIApplication sharedApplication].keyWindow._contextId;
            ASSERT(contextID);
            BKSHIDEventSetDigitizerInfo(markerEvent.get(), contextID, false, false, NULL, 0, 0);
            [[UIApplication sharedApplication] _enqueueHIDEvent:markerEvent.get()];
        });
    }
    return YES;
}

- (void)_updateTouchPoints:(CGPoint*)points count:(NSUInteger)count
{
    HandEventType handEventType;
    
    // The hand event type is based on previous state.
    if (!_activePointCount)
        handEventType = HandEventTouched;
    else if (!count)
        handEventType = HandEventLifted;
    else if (count == _activePointCount)
        handEventType = HandEventMoved;
    else
        handEventType = HandEventChordChanged;
    
    // Update previous count for next event.
    _activePointCount = count;

    // Update point locations.
    for (NSUInteger i = 0; i < count; ++i)
        _activePoints[i].point = points[i];
    
    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventType:handEventType]);
    [self _sendHIDEvent:eventRef.get()];
}

- (void)touchDownAtPoints:(CGPoint*)locations touchCount:(NSUInteger)touchCount
{
    touchCount = std::min(touchCount, HIDMaxTouchCount);

    _activePointCount = touchCount;

    for (NSUInteger index = 0; index < touchCount; ++index)
        _activePoints[index].point = locations[index];

    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventType:HandEventTouched]);
    [self _sendHIDEvent:eventRef.get()];
}

- (void)touchDown:(CGPoint)location touchCount:(NSUInteger)touchCount
{
    touchCount = std::min(touchCount, HIDMaxTouchCount);

    CGPoint locations[touchCount];

    for (NSUInteger index = 0; index < touchCount; ++index)
        locations[index] = location;
    
    [self touchDownAtPoints:locations touchCount:touchCount];
}

- (void)touchDown:(CGPoint)location
{
    [self touchDownAtPoints:&location touchCount:1];
}

- (void)liftUpAtPoints:(CGPoint*)locations touchCount:(NSUInteger)touchCount
{
    touchCount = std::min(touchCount, HIDMaxTouchCount);
    touchCount = std::min(touchCount, _activePointCount);

    NSUInteger newPointCount = _activePointCount - touchCount;

    for (NSUInteger index = 0; index < touchCount; ++index)
        _activePoints[newPointCount + index].point = locations[index];
    
    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventType:HandEventLifted]);
    [self _sendHIDEvent:eventRef.get()];
    
    _activePointCount = newPointCount;
}

- (void)liftUp:(CGPoint)location touchCount:(NSUInteger)touchCount
{
    touchCount = std::min(touchCount, HIDMaxTouchCount);

    CGPoint locations[touchCount];

    for (NSUInteger index = 0; index < touchCount; ++index)
        locations[index] = location;
    
    [self liftUpAtPoints:locations touchCount:touchCount];
}

- (void)liftUp:(CGPoint)location
{
    [self liftUp:location touchCount:1];
}

- (void)moveToPoints:(CGPoint*)newLocations touchCount:(NSUInteger)touchCount duration:(NSTimeInterval)seconds
{
    touchCount = std::min(touchCount, HIDMaxTouchCount);

    CGPoint startLocations[touchCount];
    CGPoint nextLocations[touchCount];

    CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
    CFTimeInterval elapsed = 0;

    int eventIndex = 0;
    while (elapsed < (seconds - fingerMoveInterval)) {
        elapsed = secondsSinceAbsoluteTime(startTime);
        CFTimeInterval interval = elapsed / seconds;
        
        for (NSUInteger i = 0; i < touchCount; ++i) {
            if (!eventIndex)
                startLocations[i] = _activePoints[i].point;

            nextLocations[i] = calculateNextCurveLocation(startLocations[i], newLocations[i], interval);
        }
        [self _updateTouchPoints:nextLocations count:touchCount];

        delayBetweenMove(eventIndex++, elapsed);
    }

    [self _updateTouchPoints:newLocations count:touchCount];
}

- (void)touchDown:(CGPoint)location completionBlock:(void (^)(void))completionBlock
{
    [self touchDown:location touchCount:1];
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)liftUp:(CGPoint)location completionBlock:(void (^)(void))completionBlock
{
    [self liftUp:location touchCount:1];
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)moveToPoint:(CGPoint)location duration:(NSTimeInterval)seconds completionBlock:(void (^)(void))completionBlock
{
    CGPoint locations[1];
    locations[0] = location;
    [self moveToPoints:locations touchCount:1 duration:seconds];
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)receivedHIDEvent:(IOHIDEventRef)event
{
    if (IOHIDEventGetType(event) != kIOHIDEventTypeVendorDefined)
        return;

    CFIndex callbackID = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedData);
    NSNumber *key = @(callbackID);
    void (^completionBlock)() = [_eventCallbacks objectForKey:key];
    if (completionBlock) {
        [_eventCallbacks removeObjectForKey:key];
        completionBlock();
        Block_release(completionBlock);
    }
}

@end

#endif // PLATFORM(IOS_FAMILY)
