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

#import "IOKitSPI.h"
#import "UIKitSPI.h"
#import <WebCore/SoftLinking.h>
#import <mach/mach_time.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>

SOFT_LINK_PRIVATE_FRAMEWORK(BackBoardServices)
SOFT_LINK(BackBoardServices, BKSHIDEventSetDigitizerInfo, void, (IOHIDEventRef digitizerEvent, uint32_t contextID, uint8_t systemGestureisPossible, uint8_t isSystemGestureStateChangeEvent, CFStringRef displayUUID, CFTimeInterval initialTouchTimestamp, float maxForce), (digitizerEvent, contextID, systemGestureisPossible, isSystemGestureStateChangeEvent, displayUUID, initialTouchTimestamp, maxForce));

static const NSTimeInterval fingerLiftDelay = 5e7;
static const NSTimeInterval multiTapInterval = 15e7;
static const NSTimeInterval fingerMoveInterval = 0.016;
static const IOHIDFloat defaultMajorRadius = 5;
static const IOHIDFloat defaultPathPressure = 0;
static const NSUInteger maxTouchCount = 5;
static const long nanosecondsPerSecond = 1e9;

static int fingerIdentifiers[maxTouchCount] = { 2, 3, 4, 5, 1 };

typedef enum {
    HandEventNull,
    HandEventTouched,
    HandEventMoved,
    HandEventChordChanged,
    HandEventLifted,
    HandEventCanceled,
    HandEventInRange,
    HandEventInRangeLift,
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
    return (CFAbsoluteTimeGetCurrent() - startTime);
}

static double simpleDragCurve(double a, double b, double t)
{
    return (a + (b - a) * sin(sin(t * M_PI / 2) * t * M_PI / 2));
}

static CGPoint calculateNextLocation(CGPoint a, CGPoint b, CFTimeInterval t)
{
    return CGPointMake(simpleDragCurve(a.x, b.x, t), simpleDragCurve(a.y, b.y, t));
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

@interface HIDEventGenerator ()
@property (nonatomic, strong) NSMutableDictionary *eventCallbacks;
@end

@implementation HIDEventGenerator {
    IOHIDEventSystemClientRef _ioSystemClient;
    SyntheticEventDigitizerInfo _activePoints[maxTouchCount];
    NSUInteger _activePointCount;
}

+ (HIDEventGenerator *)sharedHIDEventGenerator
{
    static HIDEventGenerator *eventGenerator = nil;
    if (!eventGenerator)
        eventGenerator = [[HIDEventGenerator alloc] init];

    return eventGenerator;
}

+ (unsigned)nextEventCallbackID
{
    static unsigned callbackID = 0;
    return ++callbackID;
}

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    for (NSUInteger i = 0; i < maxTouchCount; ++i)
        _activePoints[i].identifier = fingerIdentifiers[i];

    _eventCallbacks = [[NSMutableDictionary alloc] init];

    return self;
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
    } else if (eventType == HandEventTouched  || eventType == HandEventCanceled) {
        eventMask |= kIOHIDDigitizerEventIdentity;
    } else if (eventType == HandEventLifted)
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
    unsigned callbackID = [HIDEventGenerator nextEventCallbackID];
    void (^completionBlockCopy)() = Block_copy(completionBlock);
    [_eventCallbacks setObject:completionBlockCopy forKey:@(callbackID)];

    uint64_t machTime = mach_absolute_time();
    RetainPtr<IOHIDEventRef> markerEvent = adoptCF(IOHIDEventCreateVendorDefinedEvent(kCFAllocatorDefault,
        machTime,
        kHIDPage_VendorDefinedStart + 100,
        0,
        1,
        (uint8_t*)&callbackID,
        sizeof(unsigned),
        kIOHIDEventOptionNone));
    
    if (markerEvent) {
        markerEvent.get();
        dispatch_async(dispatch_get_main_queue(), ^{
            uint32_t contextID = [UIApplication sharedApplication].keyWindow._contextId;
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
    touchCount = MIN(touchCount, maxTouchCount);

    _activePointCount = touchCount;

    for (NSUInteger index = 0; index < touchCount; ++index)
        _activePoints[index].point = locations[index];

    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventType:HandEventTouched]);
    [self _sendHIDEvent:eventRef.get()];
}

- (void)touchDown:(CGPoint)location touchCount:(NSUInteger)touchCount
{
    touchCount = MIN(touchCount, maxTouchCount);

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
    touchCount = MIN(touchCount, maxTouchCount);
    touchCount = MIN(touchCount, _activePointCount);

    NSUInteger newPointCount = _activePointCount - touchCount;

    for (NSUInteger index = 0; index < touchCount; ++index)
        _activePoints[newPointCount + index].point = locations[index];
    
    RetainPtr<IOHIDEventRef> eventRef = adoptCF([self _createIOHIDEventType:HandEventLifted]);
    [self _sendHIDEvent:eventRef.get()];
    
    _activePointCount = newPointCount;
}

- (void)liftUp:(CGPoint)location touchCount:(NSUInteger)touchCount
{
    touchCount = MIN(touchCount, maxTouchCount);

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
    touchCount = MIN(touchCount, maxTouchCount);

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

            nextLocations[i] = calculateNextLocation(startLocations[i], newLocations[i], interval);
        }
        [self _updateTouchPoints:nextLocations count:touchCount];

        delayBetweenMove(eventIndex++, elapsed);
    }

    [self _updateTouchPoints:newLocations count:touchCount];
}

- (void)sendTaps:(int)tapCount location:(CGPoint)location withNumberOfTouches:(int)touchCount completionBlock:(void (^)(void))completionBlock
{
    struct timespec doubleDelay = { 0, static_cast<long>(multiTapInterval) };
    struct timespec pressDelay = { 0, static_cast<long>(fingerLiftDelay) };

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

- (void)dragWithStartPoint:(CGPoint)startLocation endPoint:(CGPoint)endLocation duration:(double)seconds completionBlock:(void (^)(void))completionBlock
{
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

static inline bool shouldWrapWithShiftKeyEventForCharacter(NSString *key)
{
    if (key.length != 1)
        return false;
    int keyCode = [key characterAtIndex:0];
    if (65 <= keyCode && keyCode <= 90)
        return true;
    switch (keyCode) {
    case '`':
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
    const int functionKeyOffset = kHIDUsage_KeyboardF1;
    for (int functionKeyIndex = 1; functionKeyIndex <= 12; ++functionKeyIndex) {
        if ([key isEqualToString:[NSString stringWithFormat:@"F%d", functionKeyIndex]])
            return functionKeyOffset + functionKeyIndex - 1;
    }
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
    // The simulator keyboard interprets both left and right modifier keys using the left version of the usage code.
    if ([key isEqualToString:@"leftControl"] || [key isEqualToString:@"rightControl"])
        return kHIDUsage_KeyboardLeftControl;
    if ([key isEqualToString:@"leftShift"] || [key isEqualToString:@"rightShift"])
        return kHIDUsage_KeyboardLeftShift;
    if ([key isEqualToString:@"leftAlt"] || [key isEqualToString:@"rightAlt"])
        return kHIDUsage_KeyboardLeftAlt;

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

@end
