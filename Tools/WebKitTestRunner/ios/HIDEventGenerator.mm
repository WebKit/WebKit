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

#import "config.h"
#import "HIDEventGenerator.h"

#import "GeneratedTouchesDebugWindow.h"
#import "UIKitTestSPI.h"
#import <mach/mach_time.h>
#import <pal/spi/cocoa/IOKitSPI.h>
#import <wtf/Assertions.h>
#import <wtf/BlockPtr.h>
#import <wtf/Optional.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(BackBoardServices)
SOFT_LINK(BackBoardServices, BKSHIDEventSetDigitizerInfo, void, (IOHIDEventRef digitizerEvent, uint32_t contextID, uint8_t systemGestureisPossible, uint8_t isSystemGestureStateChangeEvent, CFStringRef displayUUID, CFTimeInterval initialTouchTimestamp, float maxForce), (digitizerEvent, contextID, systemGestureisPossible, isSystemGestureStateChangeEvent, displayUUID, initialTouchTimestamp, maxForce));

NSString* const TopLevelEventInfoKey = @"events";
NSString* const HIDEventInputType = @"inputType";
NSString* const HIDEventTimeOffsetKey = @"timeOffset";
NSString* const HIDEventTouchesKey = @"touches";
NSString* const HIDEventPhaseKey = @"phase";
NSString* const HIDEventInterpolateKey = @"interpolate";
NSString* const HIDEventTimestepKey = @"timestep";
NSString* const HIDEventCoordinateSpaceKey = @"coordinateSpace";
NSString* const HIDEventStartEventKey = @"startEvent";
NSString* const HIDEventEndEventKey = @"endEvent";
NSString* const HIDEventTouchIDKey = @"id";
NSString* const HIDEventPressureKey = @"pressure";
NSString* const HIDEventXKey = @"x";
NSString* const HIDEventYKey = @"y";
NSString* const HIDEventTwistKey = @"twist";
NSString* const HIDEventMajorRadiusKey = @"majorRadius";
NSString* const HIDEventMinorRadiusKey = @"minorRadius";

NSString* const HIDEventInputTypeHand = @"hand";
NSString* const HIDEventInputTypeFinger = @"finger";
NSString* const HIDEventInputTypeStylus = @"stylus";

NSString* const HIDEventCoordinateSpaceTypeGlobal = @"global";
NSString* const HIDEventCoordinateSpaceTypeContent = @"content";

NSString* const HIDEventInterpolationTypeLinear = @"linear";
NSString* const HIDEventInterpolationTypeSimpleCurve = @"simpleCurve";

NSString* const HIDEventPhaseBegan = @"began";
NSString* const HIDEventPhaseStationary = @"stationary";
NSString* const HIDEventPhaseMoved = @"moved";
NSString* const HIDEventPhaseEnded = @"ended";
NSString* const HIDEventPhaseCanceled = @"canceled";

static const NSTimeInterval fingerLiftDelay = 0.05;
static const NSTimeInterval multiTapInterval = 0.15;
static const NSTimeInterval fingerMoveInterval = 0.016;
static const NSTimeInterval longPressHoldDelay = 2.0;
static const IOHIDFloat defaultMajorRadius = 5;
static const IOHIDFloat defaultPathPressure = 0;
static const long nanosecondsPerSecond = 1e9;

NSUInteger const HIDMaxTouchCount = 5;



static int fingerIdentifiers[HIDMaxTouchCount] = { 2, 3, 4, 5, 1 };

typedef enum {
    InterpolationTypeLinear,
    InterpolationTypeSimpleCurve,
} InterpolationType;

typedef enum {
    HandEventNull,
    HandEventTouched,
    HandEventMoved,
    HandEventChordChanged,
    HandEventLifted,
    HandEventCanceled,
    StylusEventTouched,
    StylusEventMoved,
    StylusEventLifted,
} HandEventType;

typedef struct {
    int identifier;
    CGPoint point;
    IOHIDFloat pathMajorRadius;
    IOHIDFloat pathPressure;
    UInt8 pathProximity;
    BOOL isStylus;
    IOHIDFloat azimuthAngle;
    IOHIDFloat altitudeAngle;
} SyntheticEventDigitizerInfo;

static CFTimeInterval secondsSinceAbsoluteTime(CFAbsoluteTime startTime)
{
    return (CFAbsoluteTimeGetCurrent() - startTime);
}

static double linearInterpolation(double a, double b, double t)
{
    return (a + (b - a) * t );
}

static double simpleCurveInterpolation(double a, double b, double t)
{
    return (a + (b - a) * sin(sin(t * M_PI / 2) * t * M_PI / 2));
}


static CGPoint calculateNextCurveLocation(CGPoint a, CGPoint b, CFTimeInterval t)
{
    return CGPointMake(simpleCurveInterpolation(a.x, b.x, t), simpleCurveInterpolation(a.y, b.y, t));
}

typedef double(*pressureInterpolationFunction)(double, double, CFTimeInterval);
static pressureInterpolationFunction interpolations[] = {
    linearInterpolation,
    simpleCurveInterpolation,
};

static void delayBetweenMove(int eventIndex, double elapsed)
{
    // Delay next event until expected elapsed time.
    double delay = (eventIndex * fingerMoveInterval) - elapsed;
    if (delay > 0) {
        struct timespec moveDelay = { 0, static_cast<long>(delay * nanosecondsPerSecond) };
        nanosleep(&moveDelay, NULL);
    }   
}

@interface HIDEventGenerator ()
@property (nonatomic, strong) NSMutableDictionary *eventCallbacks;
@property (nonatomic, strong) NSArray<UIView *> *debugTouchViews;
@end

@implementation HIDEventGenerator {
    IOHIDEventSystemClientRef _ioSystemClient;
    SyntheticEventDigitizerInfo _activePoints[HIDMaxTouchCount];
    NSUInteger _activePointCount;
}

+ (HIDEventGenerator *)sharedHIDEventGenerator
{
    static HIDEventGenerator *eventGenerator = nil;
    if (!eventGenerator)
        eventGenerator = [[HIDEventGenerator alloc] init];

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
    [_eventCallbacks release];
    [_debugTouchViews release];
    [super dealloc];
}



- (void)_sendIOHIDKeyboardEvent:(uint64_t)timestamp usage:(uint32_t)usage isKeyDown:(bool)isKeyDown
{
    RetainPtr<IOHIDEventRef> eventRef = adoptCF(IOHIDEventCreateKeyboardEvent(kCFAllocatorDefault,
        timestamp,
        kHIDPage_KeyboardOrKeypad,
        usage,
        isKeyDown,
        kIOHIDEventOptionNone));
    [self _sendHIDEvent:eventRef.get()];
}

static IOHIDDigitizerTransducerType transducerTypeFromString(NSString * transducerTypeString)
{
    if ([transducerTypeString isEqualToString:HIDEventInputTypeHand])
        return kIOHIDDigitizerTransducerTypeHand;

    if ([transducerTypeString isEqualToString:HIDEventInputTypeFinger])
        return kIOHIDDigitizerTransducerTypeFinger;

    if ([transducerTypeString isEqualToString:HIDEventInputTypeStylus])
        return kIOHIDDigitizerTransducerTypeStylus;
    
    ASSERT_NOT_REACHED();
    return 0;
}

static UITouchPhase phaseFromString(NSString *string)
{
    if ([string isEqualToString:HIDEventPhaseBegan])
        return UITouchPhaseBegan;
    
    if ([string isEqualToString:HIDEventPhaseStationary])
        return UITouchPhaseStationary;

    if ([string isEqualToString:HIDEventPhaseMoved])
        return UITouchPhaseMoved;

    if ([string isEqualToString:HIDEventPhaseEnded])
        return UITouchPhaseEnded;

    if ([string isEqualToString:HIDEventPhaseCanceled])
        return UITouchPhaseCancelled;

    return UITouchPhaseStationary;
}

static InterpolationType interpolationFromString(NSString *string)
{
    if ([string isEqualToString:HIDEventInterpolationTypeLinear])
        return InterpolationTypeLinear;
    
    if ([string isEqualToString:HIDEventInterpolationTypeSimpleCurve])
        return InterpolationTypeSimpleCurve;
    
    return InterpolationTypeLinear;
}

- (IOHIDDigitizerEventMask)eventMaskFromEventInfo:(NSDictionary *)info
{
    IOHIDDigitizerEventMask eventMask = 0;
    NSArray *childEvents = info[HIDEventTouchesKey];
    for (NSDictionary *touchInfo in childEvents) {
        UITouchPhase phase = phaseFromString(touchInfo[HIDEventPhaseKey]);
        // If there are any new or ended events, mask includes touch.
        if (phase == UITouchPhaseBegan || phase == UITouchPhaseEnded || phase == UITouchPhaseCancelled)
            eventMask |= kIOHIDDigitizerEventTouch;
        // If there are any pressure readings, set mask must include attribute
        if ([touchInfo[HIDEventPressureKey] floatValue])
            eventMask |= kIOHIDDigitizerEventAttribute;
    }
    
    return eventMask;
}

// Returns 1 for all events where the fingers are on the glass (everything but ended and canceled).
- (CFIndex)touchFromEventInfo:(NSDictionary *)info
{
    NSArray *childEvents = info[HIDEventTouchesKey];
    for (NSDictionary *touchInfo in childEvents) {
        UITouchPhase phase = phaseFromString(touchInfo[HIDEventPhaseKey]);
        if (phase == UITouchPhaseBegan || phase == UITouchPhaseMoved || phase == UITouchPhaseStationary)
            return 1;
    }
    
    return 0;
}

// FIXME: callers of _createIOHIDEventType could switch to this.
- (IOHIDEventRef)_createIOHIDEventWithInfo:(NSDictionary *)info
{
    uint64_t machTime = mach_absolute_time();

    IOHIDDigitizerEventMask eventMask = [self eventMaskFromEventInfo:info];

    CFIndex range = 0;
    // touch is 1 if a finger is down.
    CFIndex touch = [self touchFromEventInfo:info];

    IOHIDEventRef eventRef = IOHIDEventCreateDigitizerEvent(kCFAllocatorDefault, machTime,
        transducerTypeFromString(info[HIDEventInputType]),  // transducerType
        0,                                                  // index
        0,                                                  // identifier
        eventMask,                                          // event mask
        0,                                                  // button event
        0,                                                  // x
        0,                                                  // y
        0,                                                  // z
        0,                                                  // presure
        0,                                                  // twist
        range,                                              // range
        touch,                                              // touch
        kIOHIDEventOptionNone);

    IOHIDEventSetIntegerValue(eventRef, kIOHIDEventFieldDigitizerIsDisplayIntegrated, 1);

    NSArray *childEvents = info[HIDEventTouchesKey];
    for (NSDictionary *touchInfo in childEvents) {
        
        [[GeneratedTouchesDebugWindow sharedGeneratedTouchesDebugWindow] updateDebugIndicatorForTouch:[touchInfo[HIDEventTouchIDKey] intValue] withPointInWindowCoordinates:CGPointMake([touchInfo[HIDEventXKey] floatValue], [touchInfo[HIDEventYKey] floatValue]) isTouching:(BOOL)touch];
        
        IOHIDDigitizerEventMask childEventMask = 0;

        UITouchPhase phase = phaseFromString(touchInfo[HIDEventPhaseKey]);
        if (phase != UITouchPhaseCancelled && phase != UITouchPhaseBegan && phase != UITouchPhaseEnded && phase != UITouchPhaseStationary)
            childEventMask |= kIOHIDDigitizerEventPosition;

        if (phase == UITouchPhaseBegan || phase == UITouchPhaseEnded || phase == UITouchPhaseCancelled)
            childEventMask |= (kIOHIDDigitizerEventTouch | kIOHIDDigitizerEventRange);

        if (phase == UITouchPhaseCancelled)
            childEventMask |= kIOHIDDigitizerEventCancel;
        
        if ([touchInfo[HIDEventPressureKey] floatValue])
            childEventMask |= kIOHIDDigitizerEventAttribute;

        IOHIDEventRef subEvent = IOHIDEventCreateDigitizerFingerEvent(kCFAllocatorDefault, machTime,
            [touchInfo[HIDEventTouchIDKey] intValue],               // index
            2,                                                      // identifier (which finger we think it is). FIXME: this should come from the data.
            childEventMask,
            [touchInfo[HIDEventXKey] floatValue],
            [touchInfo[HIDEventYKey] floatValue],
            0, // z
            [touchInfo[HIDEventPressureKey] floatValue],
            [touchInfo[HIDEventTwistKey] floatValue],
            touch,                                                  // range
            touch,                                                  // touch
            kIOHIDEventOptionNone);

        IOHIDEventSetFloatValue(subEvent, kIOHIDEventFieldDigitizerMajorRadius, [touchInfo[HIDEventMajorRadiusKey] floatValue]);
        IOHIDEventSetFloatValue(subEvent, kIOHIDEventFieldDigitizerMinorRadius, [touchInfo[HIDEventMinorRadiusKey] floatValue]);

        IOHIDEventAppendEvent(eventRef, subEvent, 0);
        CFRelease(subEvent);
    }

    return eventRef;
}

- (IOHIDEventRef)_createIOHIDEventType:(HandEventType)eventType
{
    BOOL isTouching = (eventType == HandEventTouched || eventType == HandEventMoved || eventType == HandEventChordChanged || eventType == StylusEventTouched || eventType == StylusEventMoved);

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
        } else if (eventType == HandEventLifted || eventType == HandEventCanceled || eventType == StylusEventLifted) {
            pointInfo->pathMajorRadius = 0;
            pointInfo->pathPressure = 0;
            pointInfo->pathProximity = 0;
        }

        CGPoint point = pointInfo->point;
        point = CGPointMake(roundf(point.x), roundf(point.y));
        
        [[GeneratedTouchesDebugWindow sharedGeneratedTouchesDebugWindow] updateDebugIndicatorForTouch:i withPointInWindowCoordinates:point isTouching:isTouching];

        RetainPtr<IOHIDEventRef> subEvent;
        if (pointInfo->isStylus) {
            if (eventType == StylusEventTouched) {
                eventMask |= kIOHIDDigitizerEventEstimatedAltitude;
                eventMask |= kIOHIDDigitizerEventEstimatedAzimuth;
                eventMask |= kIOHIDDigitizerEventEstimatedPressure;
            } else if (eventType == StylusEventMoved)
                eventMask = kIOHIDDigitizerEventPosition;

            subEvent = adoptCF(IOHIDEventCreateDigitizerStylusEventWithPolarOrientation(kCFAllocatorDefault, machTime,
                pointInfo->identifier,
                pointInfo->identifier,
                eventMask,
                0,
                point.x, point.y, 0,
                pointInfo->pathPressure,
                pointInfo->pathPressure,
                0,
                pointInfo->altitudeAngle,
                pointInfo->azimuthAngle,
                1,
                0,
                isTouching ? kIOHIDTransducerTouch : 0));

            if (eventType == StylusEventTouched)
                IOHIDEventSetIntegerValue(subEvent.get(), kIOHIDEventFieldDigitizerWillUpdateMask, 0x0400);
            else if (eventType == StylusEventMoved)
                IOHIDEventSetIntegerValue(subEvent.get(), kIOHIDEventFieldDigitizerDidUpdateMask, 0x0400);

        } else {
            subEvent = adoptCF(IOHIDEventCreateDigitizerFingerEvent(kCFAllocatorDefault, machTime,
                pointInfo->identifier,
                pointInfo->identifier,
                eventMask,
                point.x, point.y, 0,
                pointInfo->pathPressure,
                0,
                pointInfo->pathProximity & kGSEventPathInfoInRange,
                pointInfo->pathProximity & kGSEventPathInfoInTouch,
                kIOHIDEventOptionNone));
        }

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
    auto callbackID = [HIDEventGenerator nextEventCallbackID];
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
    for (NSUInteger i = 0; i < count; ++i) {
        _activePoints[i].point = points[i];
        [[GeneratedTouchesDebugWindow sharedGeneratedTouchesDebugWindow] updateDebugIndicatorForTouch:i withPointInWindowCoordinates:points[i] isTouching:YES];
    }
    
    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventType:handEventType]);
    [self _sendHIDEvent:eventRef.get()];
}

- (void)touchDownAtPoints:(CGPoint*)locations touchCount:(NSUInteger)touchCount
{
    touchCount = std::min(touchCount, HIDMaxTouchCount);

    _activePointCount = touchCount;

    for (NSUInteger index = 0; index < touchCount; ++index) {
        _activePoints[index].point = locations[index];
        _activePoints[index].isStylus = NO;
        
        [[GeneratedTouchesDebugWindow sharedGeneratedTouchesDebugWindow] updateDebugIndicatorForTouch:index withPointInWindowCoordinates:locations[index] isTouching:YES];
    }

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

    for (NSUInteger index = 0; index < touchCount; ++index) {
        _activePoints[newPointCount + index].point = locations[index];
        
        [[GeneratedTouchesDebugWindow sharedGeneratedTouchesDebugWindow] updateDebugIndicatorForTouch:index withPointInWindowCoordinates:CGPointZero isTouching:NO];
    }
    
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

- (void)touchDown:(CGPoint)location touchCount:(NSUInteger)count completionBlock:(void (^)(void))completionBlock
{
    [self touchDown:location touchCount:count];
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)liftUp:(CGPoint)location touchCount:(NSUInteger)count completionBlock:(void (^)(void))completionBlock
{
    [self liftUp:location touchCount:count];
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)stylusDownAtPoint:(CGPoint)location azimuthAngle:(CGFloat)azimuthAngle altitudeAngle:(CGFloat)altitudeAngle pressure:(CGFloat)pressure
{
    _activePointCount = 1;
    _activePoints[0].point = location;
    _activePoints[0].isStylus = YES;

    // At the time of writing, the IOKit documentation isn't always correct. For example
    // it says that pressure is a value [0,1], but in practice it is [0,500] for stylus
    // data. It does not mention that the azimuth angle is offset from a full rotation.
    // Also, UIKit and IOHID interpret the altitude as different adjacent angles.
    _activePoints[0].pathPressure = pressure * 500;
    _activePoints[0].azimuthAngle = M_PI * 2 - azimuthAngle;
    _activePoints[0].altitudeAngle = M_PI_2 - altitudeAngle;

    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventType:StylusEventTouched]);
    [self _sendHIDEvent:eventRef.get()];
}

- (void)stylusMoveToPoint:(CGPoint)location azimuthAngle:(CGFloat)azimuthAngle altitudeAngle:(CGFloat)altitudeAngle pressure:(CGFloat)pressure
{
    _activePointCount = 1;
    _activePoints[0].point = location;
    _activePoints[0].isStylus = YES;
    // See notes above for details on these calculations.
    _activePoints[0].pathPressure = pressure * 500;
    _activePoints[0].azimuthAngle = M_PI * 2 - azimuthAngle;
    _activePoints[0].altitudeAngle = M_PI_2 - altitudeAngle;

    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventType:StylusEventMoved]);
    [self _sendHIDEvent:eventRef.get()];
}

- (void)stylusUpAtPoint:(CGPoint)location
{
    _activePointCount = 1;
    _activePoints[0].point = location;
    _activePoints[0].isStylus = YES;
    _activePoints[0].pathPressure = 0;
    _activePoints[0].azimuthAngle = 0;
    _activePoints[0].altitudeAngle = 0;

    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventType:StylusEventLifted]);
    [self _sendHIDEvent:eventRef.get()];
}

- (void)stylusDownAtPoint:(CGPoint)location azimuthAngle:(CGFloat)azimuthAngle altitudeAngle:(CGFloat)altitudeAngle pressure:(CGFloat)pressure completionBlock:(void (^)(void))completionBlock
{
    [self stylusDownAtPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure];
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)stylusMoveToPoint:(CGPoint)location azimuthAngle:(CGFloat)azimuthAngle altitudeAngle:(CGFloat)altitudeAngle pressure:(CGFloat)pressure completionBlock:(void (^)(void))completionBlock
{
    [self stylusMoveToPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure];
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)stylusUpAtPoint:(CGPoint)location completionBlock:(void (^)(void))completionBlock
{
    [self stylusUpAtPoint:location];
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)stylusTapAtPoint:(CGPoint)location azimuthAngle:(CGFloat)azimuthAngle altitudeAngle:(CGFloat)altitudeAngle pressure:(CGFloat)pressure completionBlock:(void (^)(void))completionBlock
{
    struct timespec pressDelay = { 0, static_cast<long>(fingerLiftDelay * nanosecondsPerSecond) };

    [self stylusDownAtPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure];
    nanosleep(&pressDelay, 0);
    [self stylusUpAtPoint:location];

    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)sendTaps:(int)tapCount location:(CGPoint)location withNumberOfTouches:(int)touchCount completionBlock:(void (^)(void))completionBlock
{
    struct timespec doubleDelay = { 0, static_cast<long>(multiTapInterval * nanosecondsPerSecond) };
    struct timespec pressDelay = { 0, static_cast<long>(fingerLiftDelay * nanosecondsPerSecond) };

    for (int i = 0; i < tapCount; i++) {
        [self touchDown:location touchCount:touchCount];
        nanosleep(&pressDelay, 0);
        [self liftUp:location touchCount:touchCount];
        if (i + 1 != tapCount) 
            nanosleep(&doubleDelay, 0);
    }
    
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)tap:(CGPoint)location completionBlock:(void (^)(void))completionBlock
{
    [self sendTaps:1 location:location withNumberOfTouches:1 completionBlock:completionBlock];
}

- (void)doubleTap:(CGPoint)location completionBlock:(void (^)(void))completionBlock
{
    [self sendTaps:2 location:location withNumberOfTouches:1 completionBlock:completionBlock];
}

- (void)twoFingerTap:(CGPoint)location completionBlock:(void (^)(void))completionBlock
{
    [self sendTaps:1 location:location withNumberOfTouches:2 completionBlock:completionBlock];
}

- (void)longPress:(CGPoint)location completionBlock:(void (^)(void))completionBlock
{
    [self touchDown:location touchCount:1];
    auto completionBlockCopy = makeBlockPtr(completionBlock);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, longPressHoldDelay * nanosecondsPerSecond), dispatch_get_main_queue(), ^ {
        [self liftUp:location];
        [self _sendMarkerHIDEventWithCompletionBlock:completionBlockCopy.get()];
    });
}

- (void)dragWithStartPoint:(CGPoint)startLocation endPoint:(CGPoint)endLocation duration:(double)seconds completionBlock:(void (^)(void))completionBlock
{
    [self touchDown:startLocation touchCount:1];
    [self moveToPoints:&endLocation touchCount:1 duration:seconds];
    [self liftUp:endLocation];
    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)pinchCloseWithStartPoint:(CGPoint)startLocation endPoint:(CGPoint)endLocation duration:(double)seconds completionBlock:(void (^)(void))completionBlock
{
}

- (void)pinchOpenWithStartPoint:(CGPoint)startLocation endPoint:(CGPoint)endLocation duration:(double)seconds completionBlock:(void (^)(void))completionBlock
{
}

- (void)markerEventReceived:(IOHIDEventRef)event
{
    if (IOHIDEventGetType(event) != kIOHIDEventTypeVendorDefined)
        return;

    CFIndex callbackID = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedData);
    void (^completionBlock)() = [_eventCallbacks objectForKey:@(callbackID)];
    if (completionBlock) {
        [_eventCallbacks removeObjectForKey:@(callbackID)];
        completionBlock();
        Block_release(completionBlock);
    }
}

- (BOOL)checkForOutstandingCallbacks
{
    return !([_eventCallbacks count] > 0);
}

static inline bool shouldWrapWithShiftKeyEventForCharacter(NSString *key)
{
    if (key.length != 1)
        return false;
    int keyCode = [key characterAtIndex:0];
    if (65 <= keyCode && keyCode <= 90)
        return true;
    switch (keyCode) {
    case '!':
    case '@':
    case '#':
    case '$':
    case '%':
    case '^':
    case '&':
    case '*':
    case '(':
    case ')':
    case '_':
    case '+':
    case '{':
    case '}':
    case '|':
    case ':':
    case '"':
    case '<':
    case '>':
    case '?':
    case '~':
        return true;
    }
    return false;
}

static std::optional<uint32_t> keyCodeForDOMFunctionKey(NSString *key)
{
    // Compare the input string with the function-key names defined by the DOM spec (i.e. "F1",...,"F24").
    // If the input string is a function-key name, set its key code. On iOS the key codes for the first 12
    // function keys are disjoint from the key codes of the last 12 function keys.
    for (int i = 1; i <= 12; ++i) {
        if ([key isEqualToString:[NSString stringWithFormat:@"F%d", i]])
            return kHIDUsage_KeyboardF1 + i - 1;
    }
    for (int i = 13; i <= 24; ++i) {
        if ([key isEqualToString:[NSString stringWithFormat:@"F%d", i]])
            return kHIDUsage_KeyboardF13 + i - 1;
    }
    return std::nullopt;
}

static inline uint32_t hidUsageCodeForCharacter(NSString *key)
{
    const int uppercaseAlphabeticOffset = 'A' - kHIDUsage_KeyboardA;
    const int lowercaseAlphabeticOffset = 'a' - kHIDUsage_KeyboardA;
    const int numericNonZeroOffset = '1' - kHIDUsage_Keyboard1;
    if (key.length == 1) {
        // Handle alphanumeric characters and basic symbols.
        int keyCode = [key characterAtIndex:0];
        if (97 <= keyCode && keyCode <= 122) // Handle a-z.
            return keyCode - lowercaseAlphabeticOffset;

        if (65 <= keyCode && keyCode <= 90) // Handle A-Z.
            return keyCode - uppercaseAlphabeticOffset;

        if (49 <= keyCode && keyCode <= 57) // Handle 1-9.
            return keyCode - numericNonZeroOffset;

        // Handle all other cases.
        switch (keyCode) {
        case '`':
        case '~':
            return kHIDUsage_KeyboardGraveAccentAndTilde;
        case '!':
            return kHIDUsage_Keyboard1;
        case '@':
            return kHIDUsage_Keyboard2;
        case '#':
            return kHIDUsage_Keyboard3;
        case '$':
            return kHIDUsage_Keyboard4;
        case '%':
            return kHIDUsage_Keyboard5;
        case '^':
            return kHIDUsage_Keyboard6;
        case '&':
            return kHIDUsage_Keyboard7;
        case '*':
            return kHIDUsage_Keyboard8;
        case '(':
            return kHIDUsage_Keyboard9;
        case ')':
        case '0':
            return kHIDUsage_Keyboard0;
        case '-':
        case '_':
            return kHIDUsage_KeyboardHyphen;
        case '=':
        case '+':
            return kHIDUsage_KeyboardEqualSign;
        case '\t':
            return kHIDUsage_KeyboardTab;
        case '[':
        case '{':
            return kHIDUsage_KeyboardOpenBracket;
        case ']':
        case '}':
            return kHIDUsage_KeyboardCloseBracket;
        case '\\':
        case '|':
            return kHIDUsage_KeyboardBackslash;
        case ';':
        case ':':
            return kHIDUsage_KeyboardSemicolon;
        case '\'':
        case '"':
            return kHIDUsage_KeyboardQuote;
        case '\r':
        case '\n':
            return kHIDUsage_KeyboardReturnOrEnter;
        case ',':
        case '<':
            return kHIDUsage_KeyboardComma;
        case '.':
        case '>':
            return kHIDUsage_KeyboardPeriod;
        case '/':
        case '?':
            return kHIDUsage_KeyboardSlash;
        case ' ':
            return kHIDUsage_KeyboardSpacebar;
        }
    }

    if (auto keyCode = keyCodeForDOMFunctionKey(key))
        return *keyCode;

    if ([key isEqualToString:@"pageUp"])
        return kHIDUsage_KeyboardPageUp;
    if ([key isEqualToString:@"pageDown"])
        return kHIDUsage_KeyboardPageDown;
    if ([key isEqualToString:@"home"])
        return kHIDUsage_KeyboardHome;
    if ([key isEqualToString:@"insert"])
        return kHIDUsage_KeyboardInsert;
    if ([key isEqualToString:@"end"])
        return kHIDUsage_KeyboardEnd;
    if ([key isEqualToString:@"escape"])
        return kHIDUsage_KeyboardEscape;
    if ([key isEqualToString:@"return"] || [key isEqualToString:@"enter"])
        return kHIDUsage_KeyboardReturnOrEnter;
    if ([key isEqualToString:@"leftArrow"])
        return kHIDUsage_KeyboardLeftArrow;
    if ([key isEqualToString:@"rightArrow"])
        return kHIDUsage_KeyboardRightArrow;
    if ([key isEqualToString:@"upArrow"])
        return kHIDUsage_KeyboardUpArrow;
    if ([key isEqualToString:@"downArrow"])
        return kHIDUsage_KeyboardDownArrow;
    if ([key isEqualToString:@"delete"])
        return kHIDUsage_KeyboardDeleteOrBackspace;
    if ([key isEqualToString:@"forwardDelete"])
        return kHIDUsage_KeyboardDeleteForward;
    if ([key isEqualToString:@"leftCommand"])
        return kHIDUsage_KeyboardLeftGUI;
    if ([key isEqualToString:@"rightCommand"])
        return kHIDUsage_KeyboardRightGUI;
    if ([key isEqualToString:@"clear"]) // Num Lock / Clear
        return kHIDUsage_KeypadNumLock;
    if ([key isEqualToString:@"leftControl"])
        return kHIDUsage_KeyboardLeftControl;
    if ([key isEqualToString:@"rightControl"])
        return kHIDUsage_KeyboardRightControl;
    if ([key isEqualToString:@"leftShift"])
        return kHIDUsage_KeyboardLeftShift;
    if ([key isEqualToString:@"rightShift"])
        return kHIDUsage_KeyboardRightShift;
    if ([key isEqualToString:@"leftAlt"])
        return kHIDUsage_KeyboardLeftAlt;
    if ([key isEqualToString:@"rightAlt"])
        return kHIDUsage_KeyboardRightAlt;

    return 0;
}

- (void)keyPress:(NSString *)character completionBlock:(void (^)(void))completionBlock
{
    bool shouldWrapWithShift = shouldWrapWithShiftKeyEventForCharacter(character);
    uint32_t usage = hidUsageCodeForCharacter(character);
    uint64_t absoluteMachTime = mach_absolute_time();

    if (shouldWrapWithShift)
        [self _sendIOHIDKeyboardEvent:absoluteMachTime usage:kHIDUsage_KeyboardLeftShift isKeyDown:true];

    [self _sendIOHIDKeyboardEvent:absoluteMachTime usage:usage isKeyDown:true];
    [self _sendIOHIDKeyboardEvent:absoluteMachTime usage:usage isKeyDown:false];

    if (shouldWrapWithShift)
        [self _sendIOHIDKeyboardEvent:absoluteMachTime usage:kHIDUsage_KeyboardLeftShift isKeyDown:false];

    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)keyDown:(NSString *)character completionBlock:(void (^)(void))completionBlock
{
    bool shouldWrapWithShift = shouldWrapWithShiftKeyEventForCharacter(character);
    uint32_t usage = hidUsageCodeForCharacter(character);
    uint64_t absoluteMachTime = mach_absolute_time();

    if (shouldWrapWithShift)
        [self _sendIOHIDKeyboardEvent:absoluteMachTime usage:kHIDUsage_KeyboardLeftShift isKeyDown:true];

    [self _sendIOHIDKeyboardEvent:absoluteMachTime usage:usage isKeyDown:true];

    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)keyUp:(NSString *)character completionBlock:(void (^)(void))completionBlock
{
    bool shouldWrapWithShift = shouldWrapWithShiftKeyEventForCharacter(character);
    uint32_t usage = hidUsageCodeForCharacter(character);
    uint64_t absoluteMachTime = mach_absolute_time();

    [self _sendIOHIDKeyboardEvent:absoluteMachTime usage:usage isKeyDown:false];

    if (shouldWrapWithShift)
        [self _sendIOHIDKeyboardEvent:absoluteMachTime usage:kHIDUsage_KeyboardLeftShift isKeyDown:false];

    [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
}

- (void)dispatchEventWithInfo:(NSDictionary *)eventInfo
{
    ASSERT([NSThread isMainThread]);

    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventWithInfo:eventInfo]);
    [self _sendHIDEvent:eventRef.get()];
}

- (NSArray *)interpolatedEvents:(NSDictionary *)interpolationsDictionary
{
    NSDictionary *startEvent = interpolationsDictionary[HIDEventStartEventKey];
    NSDictionary *endEvent = interpolationsDictionary[HIDEventEndEventKey];
    NSTimeInterval timeStep = [interpolationsDictionary[HIDEventTimestepKey] doubleValue];
    InterpolationType interpolationType = interpolationFromString(interpolationsDictionary[HIDEventInterpolateKey]);
    
    NSMutableArray *interpolatedEvents = [NSMutableArray arrayWithObject:startEvent];
    
    NSTimeInterval startTime = [startEvent[HIDEventTimeOffsetKey] doubleValue];
    NSTimeInterval endTime = [endEvent[HIDEventTimeOffsetKey] doubleValue];
    NSTimeInterval time = startTime + timeStep;
    
    NSArray *startTouches = startEvent[HIDEventTouchesKey];
    NSArray *endTouches = endEvent[HIDEventTouchesKey];
    
    while (time < endTime) {
        NSMutableDictionary *newEvent = [endEvent mutableCopy];
        double timeRatio = (time - startTime) / (endTime - startTime);
        newEvent[HIDEventTimeOffsetKey] = [NSNumber numberWithDouble:(time)];
        
        NSEnumerator *startEnumerator = [startTouches objectEnumerator];
        NSDictionary *startTouch;
        NSMutableArray *newTouches = [NSMutableArray arrayWithCapacity:[endTouches count]];
        while (startTouch = [startEnumerator nextObject])  {
            NSEnumerator *endEnumerator = [endTouches objectEnumerator];
            NSDictionary *endTouch = [endEnumerator nextObject];
            NSInteger startTouchID = [startTouch[HIDEventTouchIDKey] integerValue];
            
            while (endTouch && ([endTouch[HIDEventTouchIDKey] integerValue] != startTouchID))
                endTouch = [endEnumerator nextObject];
            
            if (endTouch) {
                NSMutableDictionary *newTouch = [endTouch mutableCopy];
                
                if (newTouch[HIDEventXKey] != startTouch[HIDEventXKey])
                    newTouch[HIDEventXKey] = @(interpolations[interpolationType]([startTouch[HIDEventXKey] doubleValue], [endTouch[HIDEventXKey] doubleValue], timeRatio));
                
                if (newTouch[HIDEventYKey] != startTouch[HIDEventYKey])
                    newTouch[HIDEventYKey] = @(interpolations[interpolationType]([startTouch[HIDEventYKey] doubleValue], [endTouch[HIDEventYKey] doubleValue], timeRatio));
                
                if (newTouch[HIDEventPressureKey] != startTouch[HIDEventPressureKey])
                    newTouch[HIDEventPressureKey] = @(interpolations[interpolationType]([startTouch[HIDEventPressureKey] doubleValue], [endTouch[HIDEventPressureKey] doubleValue], timeRatio));
                
                [newTouches addObject:newTouch];
                [newTouch release];
            } else
                NSLog(@"Missing End Touch with ID: %ld", (long)startTouchID);
        }
        
        newEvent[HIDEventTouchesKey] = newTouches;
        
        [interpolatedEvents addObject:newEvent];
        [newEvent release];
        time += timeStep;
    }
    
    [interpolatedEvents addObject:endEvent];

    return interpolatedEvents;
}

- (NSArray *)expandEvents:(NSArray *)events withStartTime:(CFAbsoluteTime)startTime
{
    NSMutableArray *expandedEvents = [NSMutableArray array];
    for (NSDictionary *event in events) {
        NSString *interpolate = event[HIDEventInterpolateKey];
        // we have key events that we need to generate
        if (interpolate) {
            NSArray *newEvents = [self interpolatedEvents:event];
            [expandedEvents addObjectsFromArray:[self expandEvents:newEvents withStartTime:startTime]];
        } else
            [expandedEvents addObject:event];
    }
    return expandedEvents;
}

- (void)eventDispatchThreadEntry:(NSDictionary *)threadData
{
    NSDictionary *eventStream = threadData[@"eventInfo"];
    void (^completionBlock)() = threadData[@"completionBlock"];
    
    NSArray *events = eventStream[TopLevelEventInfoKey];
    if (!events.count) {
        NSLog(@"No events found in event stream");
        return;
    }
    
    CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
    
    NSArray *expandedEvents = [self expandEvents:events withStartTime:startTime];
    
    for (NSDictionary *eventInfo in expandedEvents) {
        NSTimeInterval eventRelativeTime = [eventInfo[HIDEventTimeOffsetKey] doubleValue];
        CFAbsoluteTime targetTime = startTime + eventRelativeTime;
        
        CFTimeInterval waitTime = targetTime - CFAbsoluteTimeGetCurrent();
        if (waitTime > 0)
            [NSThread sleepForTimeInterval:waitTime];
        
        dispatch_async(dispatch_get_main_queue(), ^ {
            [self dispatchEventWithInfo:eventInfo];
        });
    }
    
    dispatch_async(dispatch_get_main_queue(), ^ {
        [self _sendMarkerHIDEventWithCompletionBlock:completionBlock];
    });
}

- (void)sendEventStream:(NSDictionary *)eventInfo completionBlock:(void (^)(void))completionBlock
{
    if (!eventInfo) {
        NSLog(@"eventInfo is nil");
        if (completionBlock)
            completionBlock();
        return;
    }
    
    NSDictionary* threadData = @{
        @"eventInfo": [eventInfo copy],
        @"completionBlock": [[completionBlock copy] autorelease]
    };
    
    NSThread *eventDispatchThread = [[[NSThread alloc] initWithTarget:self selector:@selector(eventDispatchThreadEntry:) object:threadData] autorelease];
    eventDispatchThread.qualityOfService = NSQualityOfServiceUserInteractive;
    [eventDispatchThread start];
}

@end
