/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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
#import "EventSenderProxy.h"

#import "CoreGraphicsTestSPI.h"
#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "WebKitTestRunnerWindow.h"
#import <Carbon/Carbon.h>
#import <WebKit/WKString.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>

@interface NSApplication (Details)
- (void)_setCurrentEvent:(NSEvent *)event;
@end

@interface NSEvent (ForTestRunner)
- (void)_postDelayed;
- (instancetype)_initWithCGEvent:(CGEventRef)event eventRef:(void *)eventRef;
@end

@interface EventSenderSyntheticEvent : NSEvent {
@public
    NSPoint _eventSender_locationInWindow;
    NSPoint _eventSender_location;
    NSInteger _eventSender_stage;
    float _eventSender_pressure;
    CGFloat _eventSender_stageTransition;
    NSEventPhase _eventSender_phase;
    NSEventPhase _eventSender_momentumPhase;
    NSTimeInterval _eventSender_timestamp;
    NSInteger _eventSender_eventNumber;
    short _eventSender_subtype;
    NSEventType _eventSender_type;
    NSWindow *_eventSender_window;
}

- (id)initPressureEventAtLocation:(NSPoint)location globalLocation:(NSPoint)globalLocation stage:(NSInteger)stage pressure:(float)pressure stageTransition:(float)stageTransition phase:(NSEventPhase)phase time:(NSTimeInterval)time eventNumber:(NSInteger)eventNumber window:(NSWindow *)window;
- (NSTimeInterval)timestamp;
@end

@implementation EventSenderSyntheticEvent

- (instancetype)initPressureEventAtLocation:(NSPoint)location globalLocation:(NSPoint)globalLocation stage:(NSInteger)stage pressure:(float)pressure stageTransition:(float)stageTransition phase:(NSEventPhase)phase time:(NSTimeInterval)time eventNumber:(NSInteger)eventNumber window:(NSWindow *)window
{
    CGSGesturePhase gesturePhase;
    switch (phase) {
    case NSEventPhaseMayBegin:
        gesturePhase = kCGSGesturePhaseMayBegin;
        break;
    case NSEventPhaseBegan:
        gesturePhase = kCGSGesturePhaseBegan;
        break;
    case NSEventPhaseChanged:
        gesturePhase = kCGSGesturePhaseChanged;
        break;
    case NSEventPhaseCancelled:
        gesturePhase = kCGSGesturePhaseCancelled;
        break;
    case NSEventPhaseEnded:
        gesturePhase = kCGSGesturePhaseEnded;
        break;
    case NSEventPhaseNone:
    default:
        gesturePhase = kCGSGesturePhaseNone;
        break;
    }

    CGEventRef cgEvent = CGEventCreate(nullptr);
    CGEventSetType(cgEvent, (CGEventType)kCGSEventGesture);
    CGEventSetIntegerValueField(cgEvent, kCGEventGestureHIDType, 32);
    CGEventSetIntegerValueField(cgEvent, kCGEventGesturePhase, gesturePhase);
    CGEventSetDoubleValueField(cgEvent, kCGEventStagePressure, pressure);
    CGEventSetDoubleValueField(cgEvent, kCGEventTransitionProgress, pressure);
    CGEventSetIntegerValueField(cgEvent, kCGEventGestureStage, stageTransition);
    CGEventSetIntegerValueField(cgEvent, kCGEventGestureBehavior, kCGSGestureBehaviorDeepPress);

    self = [super _initWithCGEvent:cgEvent eventRef:nullptr];
    CFRelease(cgEvent);

    if (!self)
        return nil;

    _eventSender_location = location;
    _eventSender_locationInWindow = globalLocation;
    _eventSender_stage = stage;
    _eventSender_pressure = pressure;
    _eventSender_stageTransition = stageTransition;
    _eventSender_phase = phase;
    _eventSender_timestamp = time;
    _eventSender_eventNumber = eventNumber;
    _eventSender_window = window;
    _eventSender_type = NSEventTypePressure;

    return self;
}

- (CGFloat)stageTransition
{
    return _eventSender_stageTransition;
}

- (NSTimeInterval)timestamp
{
    return _eventSender_timestamp;
}

- (NSEventType)type
{
    return _eventSender_type;
}

- (NSEventSubtype)subtype
{
    return (NSEventSubtype)_eventSender_subtype;
}

- (NSPoint)locationInWindow
{
    return _eventSender_location;
}

- (NSPoint)location
{
    return _eventSender_locationInWindow;
}

- (NSInteger)stage
{
    return _eventSender_stage;
}

- (float)pressure
{
    return _eventSender_pressure;
}

- (NSEventPhase)phase
{
    return _eventSender_phase;
}

- (NSEventPhase)momentumPhase
{
    return _eventSender_momentumPhase;
}

- (NSInteger)eventNumber
{
    return _eventSender_eventNumber;
}

- (BOOL)_isTouchesEnded
{
    return false;
}

- (NSWindow *)window
{
    return _eventSender_window;
}

@end

namespace WTR {

enum MouseAction {
    MouseDown,
    MouseUp,
    MouseDragged
};

// Match the DOM spec (sadly the DOM spec does not provide an enum)
enum MouseButton {
    LeftMouseButton = 0,
    MiddleMouseButton = 1,
    RightMouseButton = 2,
    NoMouseButton = -2
};

struct KeyMappingEntry {
    int macKeyCode;
    int macNumpadKeyCode;
    unichar character;
    NSString* characterName;
};

static NSEventType eventTypeForMouseButtonAndAction(int button, MouseAction action)
{
    switch (button) {
    case LeftMouseButton:
        switch (action) {
        case MouseDown:
            return NSEventTypeLeftMouseDown;
        case MouseUp:
            return NSEventTypeLeftMouseUp;
        case MouseDragged:
            return NSEventTypeLeftMouseDragged;
        }
    case RightMouseButton:
        switch (action) {
        case MouseDown:
            return NSEventTypeRightMouseDown;
        case MouseUp:
            return NSEventTypeRightMouseUp;
        case MouseDragged:
            return NSEventTypeRightMouseDragged;
        }
    default:
        switch (action) {
        case MouseDown:
            return NSEventTypeOtherMouseDown;
        case MouseUp:
            return NSEventTypeOtherMouseUp;
        case MouseDragged:
            return NSEventTypeOtherMouseDragged;
        }
    }
    assert(0);
    return static_cast<NSEventType>(0);
}

static int buildModifierFlags(WKEventModifiers modifiers)
{
    int flags = 0;
    if (modifiers & kWKEventModifiersControlKey)
        flags |= NSEventModifierFlagControl;
    if (modifiers & kWKEventModifiersShiftKey)
        flags |= NSEventModifierFlagShift;
    if (modifiers & kWKEventModifiersAltKey)
        flags |= NSEventModifierFlagOption;
    if (modifiers & kWKEventModifiersMetaKey)
        flags |= NSEventModifierFlagCommand;
    if (modifiers & kWKEventModifiersCapsLockKey)
        flags |= NSEventModifierFlagCapsLock;
    return flags;
}

static NSTimeInterval absoluteTimeForEventTime(double currentEventTime)
{
    return GetCurrentEventTime() + currentEventTime;
}

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
{
}

EventSenderProxy::~EventSenderProxy() = default;

void EventSenderProxy::updateClickCountForButton(int button)
{
    if (m_time - m_clickTime < 1 && m_position == m_clickPosition && button == m_clickButton) {
        ++m_clickCount;
        m_clickTime = m_time;
        return;
    }

    m_clickCount = 1;
    m_clickTime = m_time;
    m_clickPosition = m_position;
    m_clickButton = button;
}

static NSUInteger swizzledEventPressedMouseButtons()
{
    return TestController::singleton().eventSenderProxy()->mouseButtonsCurrentlyDown();
}

void EventSenderProxy::mouseDown(unsigned buttonNumber, WKEventModifiers modifiers)
{
    m_mouseButtonsCurrentlyDown |= (1 << buttonNumber);

    updateClickCountForButton(buttonNumber);

    NSEventType eventType = eventTypeForMouseButtonAndAction(buttonNumber, MouseDown);
    NSEvent *event = [NSEvent mouseEventWithType:eventType
                                        location:NSMakePoint(m_position.x, m_position.y)
                                   modifierFlags:buildModifierFlags(modifiers)
                                       timestamp:absoluteTimeForEventTime(currentEventTime())
                                    windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:m_clickCount 
                                        pressure:0.0];

    NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[event locationInWindow]];
    if (targetView) {
        auto eventPressedMouseButtonsSwizzler = makeUnique<ClassMethodSwizzler>([NSEvent class], @selector(pressedMouseButtons), reinterpret_cast<IMP>(swizzledEventPressedMouseButtons));
        [NSApp _setCurrentEvent:event];
        [targetView mouseDown:event];
        [NSApp _setCurrentEvent:nil];
        if (buttonNumber == LeftMouseButton)
            m_leftMouseButtonDown = true;
    }
}

void EventSenderProxy::mouseUp(unsigned buttonNumber, WKEventModifiers modifiers)
{
    m_mouseButtonsCurrentlyDown &= ~(1 << buttonNumber);

    NSEventType eventType = eventTypeForMouseButtonAndAction(buttonNumber, MouseUp);
    NSEvent *event = [NSEvent mouseEventWithType:eventType
                                        location:NSMakePoint(m_position.x, m_position.y)
                                   modifierFlags:buildModifierFlags(modifiers)
                                       timestamp:absoluteTimeForEventTime(currentEventTime())
                                    windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:m_clickCount 
                                        pressure:0.0];

    NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[event locationInWindow]];
    // FIXME: Silly hack to teach WKTR to respect capturing mouse events outside the WKView.
    // The right solution is just to use NSApplication's built-in event sending methods, 
    // instead of rolling our own algorithm for selecting an event target.
    if (!targetView)
        targetView = m_testController->mainWebView()->platformView();

    ASSERT(targetView);
    auto eventPressedMouseButtonsSwizzler = makeUnique<ClassMethodSwizzler>([NSEvent class], @selector(pressedMouseButtons), reinterpret_cast<IMP>(swizzledEventPressedMouseButtons));
    [NSApp _setCurrentEvent:event];
    [targetView mouseUp:event];
    [NSApp _setCurrentEvent:nil];
    if (buttonNumber == LeftMouseButton)
        m_leftMouseButtonDown = false;
    m_clickTime = currentEventTime();
    m_clickPosition = m_position;
}

void EventSenderProxy::sendMouseDownToStartPressureEvents()
{
    updateClickCountForButton(0);

    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
        location:NSMakePoint(m_position.x, m_position.y)
        modifierFlags:NSEventMaskPressure
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        eventNumber:++eventNumber
        clickCount:m_clickCount
        pressure:0.0];

    [NSApp sendEvent:event];
}

static void handleForceEventSynchronously(NSEvent *event)
{
    // Force events have to be pushed onto the queue, then popped off right away and handled synchronously in order
    // to get the NSImmediateActionGestureRecognizer to do the right thing.
    [event _postDelayed];
    [NSApp sendEvent:[NSApp nextEventMatchingMask:NSEventMaskPressure untilDate:[NSDate dateWithTimeIntervalSinceNow:0.05] inMode:NSDefaultRunLoopMode dequeue:YES]];
}

RetainPtr<NSEvent> EventSenderProxy::beginPressureEvent(int stage)
{
    auto event = adoptNS([[EventSenderSyntheticEvent alloc] initPressureEventAtLocation:NSMakePoint(m_position.x, m_position.y)
        globalLocation:([m_testController->mainWebView()->platformWindow() convertRectToScreen:NSMakeRect(m_position.x, m_position.y, 1, 1)].origin)
        stage:stage
        pressure:0.5
        stageTransition:0
        phase:NSEventPhaseBegan
        time:absoluteTimeForEventTime(currentEventTime())
        eventNumber:++eventNumber
        window:[m_testController->mainWebView()->platformView() window]]);

    return event;
}

RetainPtr<NSEvent> EventSenderProxy::pressureChangeEvent(int stage, float pressure, EventSenderProxy::PressureChangeDirection direction)
{
    auto event = adoptNS([[EventSenderSyntheticEvent alloc] initPressureEventAtLocation:NSMakePoint(m_position.x, m_position.y)
        globalLocation:([m_testController->mainWebView()->platformWindow() convertRectToScreen:NSMakeRect(m_position.x, m_position.y, 1, 1)].origin)
        stage:stage
        pressure:pressure
        stageTransition:direction == PressureChangeDirection::Increasing ? 0.5 : -0.5
        phase:NSEventPhaseChanged
        time:absoluteTimeForEventTime(currentEventTime())
        eventNumber:++eventNumber
        window:[m_testController->mainWebView()->platformView() window]]);

    return event;
}

RetainPtr<NSEvent> EventSenderProxy::pressureChangeEvent(int stage, EventSenderProxy::PressureChangeDirection direction)
{
    return pressureChangeEvent(stage, 0.5, direction);
}

void EventSenderProxy::mouseForceClick()
{
    sendMouseDownToStartPressureEvents();

    auto beginPressure = beginPressureEvent(1);
    auto preForceClick = pressureChangeEvent(1, PressureChangeDirection::Increasing);
    auto forceClick = pressureChangeEvent(2, PressureChangeDirection::Increasing);
    auto releasingPressure = pressureChangeEvent(1, PressureChangeDirection::Decreasing);
    NSEvent *mouseUp = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
        location:NSMakePoint(m_position.x, m_position.y)
        modifierFlags:0
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        eventNumber:++eventNumber
        clickCount:m_clickCount
        pressure:0.0];

    NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[preForceClick.get() locationInWindow]];
    targetView = targetView ? targetView : m_testController->mainWebView()->platformView();
    ASSERT(targetView);

    // Since AppKit does not implement forceup/down as mouse events, we need to send two pressure events to detect
    // the change in stage that marks those moments.
    handleForceEventSynchronously(beginPressure.get());
    handleForceEventSynchronously(preForceClick.get());
    handleForceEventSynchronously(forceClick.get());
    handleForceEventSynchronously(releasingPressure.get());
    [NSApp sendEvent:mouseUp];

    [NSApp _setCurrentEvent:nil];
    // WKView caches the most recent pressure event, so send it a nil event to clear the cache.
    IGNORE_NULL_CHECK_WARNINGS_BEGIN
    [targetView pressureChangeWithEvent:nil];
    IGNORE_NULL_CHECK_WARNINGS_END
}

void EventSenderProxy::startAndCancelMouseForceClick()
{
    sendMouseDownToStartPressureEvents();

    auto beginPressure = beginPressureEvent(1);
    auto increasingPressure = pressureChangeEvent(1, PressureChangeDirection::Increasing);
    auto releasingPressure = pressureChangeEvent(1, PressureChangeDirection::Decreasing);
    NSEvent *mouseUp = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
        location:NSMakePoint(m_position.x, m_position.y)
        modifierFlags:0
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        eventNumber:++eventNumber
        clickCount:m_clickCount
        pressure:0.0];

    NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[beginPressure.get() locationInWindow]];
    targetView = targetView ? targetView : m_testController->mainWebView()->platformView();
    ASSERT(targetView);

    // Since AppKit does not implement forceup/down as mouse events, we need to send two pressure events to detect
    // the change in stage that marks those moments.
    handleForceEventSynchronously(beginPressure.get());
    handleForceEventSynchronously(increasingPressure.get());
    handleForceEventSynchronously(releasingPressure.get());
    [NSApp sendEvent:mouseUp];

    [NSApp _setCurrentEvent:nil];
    // WKView caches the most recent pressure event, so send it a nil event to clear the cache.
    IGNORE_NULL_CHECK_WARNINGS_BEGIN
    [targetView pressureChangeWithEvent:nil];
    IGNORE_NULL_CHECK_WARNINGS_END
}

void EventSenderProxy::mouseForceDown()
{
    sendMouseDownToStartPressureEvents();

    auto beginPressure = beginPressureEvent(1);
    auto preForceClick = pressureChangeEvent(1, PressureChangeDirection::Increasing);
    auto forceMouseDown = pressureChangeEvent(2, PressureChangeDirection::Increasing);

    NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[beginPressure locationInWindow]];
    targetView = targetView ? targetView : m_testController->mainWebView()->platformView();
    ASSERT(targetView);

    // Since AppKit does not implement forceup/down as mouse events, we need to send two pressure events to detect
    // the change in stage that marks those moments.
    handleForceEventSynchronously(beginPressure.get());
    handleForceEventSynchronously(preForceClick.get());
    [forceMouseDown _postDelayed];

    [NSApp _setCurrentEvent:nil];
    // WKView caches the most recent pressure event, so send it a nil event to clear the cache.
    IGNORE_NULL_CHECK_WARNINGS_BEGIN
    [targetView pressureChangeWithEvent:nil];
    IGNORE_NULL_CHECK_WARNINGS_END
}

void EventSenderProxy::mouseForceUp()
{
    auto beginPressure = beginPressureEvent(2);
    auto stageTwoEvent = pressureChangeEvent(2, PressureChangeDirection::Decreasing);
    auto stageOneEvent = pressureChangeEvent(1, PressureChangeDirection::Decreasing);

    // Since AppKit does not implement forceup/down as mouse events, we need to send two pressure events to detect
    // the change in stage that marks those moments.
    [NSApp sendEvent:beginPressure.get()];
    [NSApp sendEvent:stageTwoEvent.get()];
    [NSApp sendEvent:stageOneEvent.get()];

    NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[beginPressure locationInWindow]];
    targetView = targetView ? targetView : m_testController->mainWebView()->platformView();
    ASSERT(targetView);

    [NSApp _setCurrentEvent:nil];
    // WKView caches the most recent pressure event, so send it a nil event to clear the cache.
    IGNORE_NULL_CHECK_WARNINGS_BEGIN
    [targetView pressureChangeWithEvent:nil];
    IGNORE_NULL_CHECK_WARNINGS_END
}

void EventSenderProxy::mouseForceChanged(float force)
{
    int stage = force < 1 ? 1 : 2;
    float pressure = force < 1 ? force : force - 1;
    auto beginPressure = beginPressureEvent(stage);
    auto pressureChangedEvent = pressureChangeEvent(stage, pressure, PressureChangeDirection::Increasing);

    NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[beginPressure locationInWindow]];
    targetView = targetView ? targetView : m_testController->mainWebView()->platformView();
    ASSERT(targetView);

    [NSApp sendEvent:beginPressure.get()];
    [NSApp sendEvent:pressureChangedEvent.get()];

    // WKView caches the most recent pressure event, so send it a nil event to clear the cache.
    IGNORE_NULL_CHECK_WARNINGS_BEGIN
    [targetView pressureChangeWithEvent:nil];
    IGNORE_NULL_CHECK_WARNINGS_END
}

void EventSenderProxy::mouseMoveTo(double x, double y)
{
    NSView *view = m_testController->mainWebView()->platformView();
    NSPoint newMousePosition = [view convertPoint:NSMakePoint(x, y) toView:nil];
    bool isDrag = m_leftMouseButtonDown;
    NSEvent *event = [NSEvent mouseEventWithType:(isDrag ? NSEventTypeLeftMouseDragged : NSEventTypeMouseMoved)
                                        location:newMousePosition
                                   modifierFlags:0 
                                       timestamp:absoluteTimeForEventTime(currentEventTime())
                                    windowNumber:view.window.windowNumber
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++eventNumber 
                                      clickCount:(m_leftMouseButtonDown ? m_clickCount : 0) 
                                        pressure:0];

    CGEventRef cgEvent = event.CGEvent;
    CGEventSetIntegerValueField(cgEvent, kCGMouseEventDeltaX, newMousePosition.x - m_position.x);
    CGEventSetIntegerValueField(cgEvent, kCGMouseEventDeltaY, newMousePosition.y - m_position.y);
    event = [NSEvent eventWithCGEvent:cgEvent];
    m_position.x = newMousePosition.x;
    m_position.y = newMousePosition.y;

    NSPoint windowLocation = event.locationInWindow;
    // Always target drags at the WKWebView to allow for drag-scrolling outside the view.
    NSView *targetView = isDrag ? m_testController->mainWebView()->platformView() : [m_testController->mainWebView()->platformView() hitTest:windowLocation];
    if (targetView) {
        auto eventPressedMouseButtonsSwizzler = makeUnique<ClassMethodSwizzler>([NSEvent class], @selector(pressedMouseButtons), reinterpret_cast<IMP>(swizzledEventPressedMouseButtons));
        [NSApp _setCurrentEvent:event];
        if (isDrag)
            [targetView mouseDragged:event];
        else
            [targetView mouseMoved:event];
        [NSApp _setCurrentEvent:nil];
    } else
        WTFLogAlways("mouseMoveTo failed to find a target view at %f,%f\n", windowLocation.x, windowLocation.y);
}

void EventSenderProxy::leapForward(int milliseconds)
{
    m_time += milliseconds / 1000.0;
}

void EventSenderProxy::keyDown(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
    NSString* character = toWTFString(key);

    NSString *eventCharacter = character;
    unsigned short keyCode = 0;
    if ([character isEqualToString:@"leftArrow"]) {
        const unichar ch = NSLeftArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7B;
    } else if ([character isEqualToString:@"rightArrow"]) {
        const unichar ch = NSRightArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7C;
    } else if ([character isEqualToString:@"upArrow"]) {
        const unichar ch = NSUpArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7E;
    } else if ([character isEqualToString:@"downArrow"]) {
        const unichar ch = NSDownArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7D;
    } else if ([character isEqualToString:@"pageUp"]) {
        const unichar ch = NSPageUpFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x74;
    } else if ([character isEqualToString:@"pageDown"]) {
        const unichar ch = NSPageDownFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x79;
    } else if ([character isEqualToString:@"home"]) {
        const unichar ch = NSHomeFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x73;
    } else if ([character isEqualToString:@"end"]) {
        const unichar ch = NSEndFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x77;
    } else if ([character isEqualToString:@"insert"]) {
        const unichar ch = NSInsertFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x72;
    } else if ([character isEqualToString:@"delete"]) {
        const unichar ch = NSDeleteFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x75;
    } else if ([character isEqualToString:@"printScreen"]) {
        const unichar ch = NSPrintScreenFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x0; // There is no known virtual key code for PrintScreen.
    } else if ([character isEqualToString:@"cyrillicSmallLetterA"]) {
        const unichar ch = 0x0430;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3; // Shares key with "F" on Russian layout.
    } else if ([character isEqualToString:@"leftControl"]) {
        const unichar ch = 0xFFE3;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3B;
    } else if ([character isEqualToString:@"leftShift"]) {
        const unichar ch = 0xFFE1;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x38;
    } else if ([character isEqualToString:@"leftAlt"]) {
        const unichar ch = 0xFFE7;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3A;
    } else if ([character isEqualToString:@"rightControl"]) {
        const unichar ch = 0xFFE4;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3E;
    } else if ([character isEqualToString:@"rightShift"]) {
        const unichar ch = 0xFFE2;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3C;
    } else if ([character isEqualToString:@"rightAlt"]) {
        const unichar ch = 0xFFE8;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x3D;
    } else if ([character isEqualToString:@"escape"]) {
        const unichar ch = 0x1B;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x35;
    }

    // Compare the input string with the function-key names defined by the DOM spec (i.e. "F1",...,"F24").
    // If the input string is a function-key name, set its key code.
    for (unsigned i = 1; i <= 24; i++) {
        if ([character isEqualToString:[NSString stringWithFormat:@"F%u", i]]) {
            const unichar ch = NSF1FunctionKey + (i - 1);
            eventCharacter = [NSString stringWithCharacters:&ch length:1];
            switch (i) {
                case 1: keyCode = 0x7A; break;
                case 2: keyCode = 0x78; break;
                case 3: keyCode = 0x63; break;
                case 4: keyCode = 0x76; break;
                case 5: keyCode = 0x60; break;
                case 6: keyCode = 0x61; break;
                case 7: keyCode = 0x62; break;
                case 8: keyCode = 0x64; break;
                case 9: keyCode = 0x65; break;
                case 10: keyCode = 0x6D; break;
                case 11: keyCode = 0x67; break;
                case 12: keyCode = 0x6F; break;
                case 13: keyCode = 0x69; break;
                case 14: keyCode = 0x6B; break;
                case 15: keyCode = 0x71; break;
                case 16: keyCode = 0x6A; break;
                case 17: keyCode = 0x40; break;
                case 18: keyCode = 0x4F; break;
                case 19: keyCode = 0x50; break;
                case 20: keyCode = 0x5A; break;
            }
        }
    }

    // FIXME: No keyCode is set for most keys.
    if ([character isEqualToString:@"\t"])
        keyCode = 0x30;
    else if ([character isEqualToString:@" "])
        keyCode = 0x31;
    else if ([character isEqualToString:@"\r"])
        keyCode = 0x24;
    else if ([character isEqualToString:@"\n"])
        keyCode = 0x4C;
    else if ([character isEqualToString:@"\x8"])
        keyCode = 0x33;
    else if ([character isEqualToString:@"a"])
        keyCode = 0x00;
    else if ([character isEqualToString:@"b"])
        keyCode = 0x0B;
    else if ([character isEqualToString:@"d"])
        keyCode = 0x02;
    else if ([character isEqualToString:@"e"])
        keyCode = 0x0E;
    else if ([character isEqualToString:@"\x1b"])
        keyCode = 0x1B;
    else if ([character isEqualToString:@":"])
        keyCode = 0x29;
    else if ([character isEqualToString:@";"])
        keyCode = 0x29;
    else if ([character isEqualToString:@","])
        keyCode = 0x2B;

    KeyMappingEntry table[] = {
        {0x2F, 0x41, '.', nil},
        {0,    0x43, '*', nil},
        {0,    0x45, '+', nil},
        {0,    0x47, NSClearLineFunctionKey, @"clear"},
        {0x2C, 0x4B, '/', nil},
        {0,    0x4C, 3, @"enter" },
        {0x1B, 0x4E, '-', nil},
        {0x18, 0x51, '=', nil},
        {0x1D, 0x52, '0', nil},
        {0x12, 0x53, '1', nil},
        {0x13, 0x54, '2', nil},
        {0x14, 0x55, '3', nil},
        {0x15, 0x56, '4', nil},
        {0x17, 0x57, '5', nil},
        {0x16, 0x58, '6', nil},
        {0x1A, 0x59, '7', nil},
        {0x1C, 0x5B, '8', nil},
        {0x19, 0x5C, '9', nil},
    };
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(table); ++i) {
        NSString* currentCharacterString = [NSString stringWithCharacters:&table[i].character length:1];
        if ([character isEqualToString:currentCharacterString] || [character isEqualToString:table[i].characterName]) {
            if (keyLocation == 0x03 /*DOM_KEY_LOCATION_NUMPAD*/)
                keyCode = table[i].macNumpadKeyCode;
            else
                keyCode = table[i].macKeyCode;
            eventCharacter = currentCharacterString;
            break;
        }
    }

    NSString *charactersIgnoringModifiers = eventCharacter;

    int modifierFlags = 0;

    if ([character length] == 1 && [character characterAtIndex:0] >= 'A' && [character characterAtIndex:0] <= 'Z') {
        modifierFlags |= NSEventModifierFlagShift;
        charactersIgnoringModifiers = [character lowercaseString];
    }

    modifierFlags |= buildModifierFlags(modifiers);

    if (keyLocation == 0x03 /*DOM_KEY_LOCATION_NUMPAD*/)
        modifierFlags |= NSEventModifierFlagNumericPad;

    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                        location:NSMakePoint(5, 5)
                        modifierFlags:modifierFlags
                        timestamp:absoluteTimeForEventTime(currentEventTime())
                        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
                        context:[NSGraphicsContext currentContext]
                        characters:eventCharacter
                        charactersIgnoringModifiers:charactersIgnoringModifiers
                        isARepeat:NO
                        keyCode:keyCode];

    [NSApp _setCurrentEvent:event];
    [[m_testController->mainWebView()->platformWindow() firstResponder] keyDown:event];
    [NSApp _setCurrentEvent:nil];

    event = [NSEvent keyEventWithType:NSEventTypeKeyUp
                        location:NSMakePoint(5, 5)
                        modifierFlags:modifierFlags
                        timestamp:absoluteTimeForEventTime(currentEventTime())
                        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
                        context:[NSGraphicsContext currentContext]
                        characters:eventCharacter
                        charactersIgnoringModifiers:charactersIgnoringModifiers
                        isARepeat:NO
                        keyCode:keyCode];

    [NSApp _setCurrentEvent:event];
    [[m_testController->mainWebView()->platformWindow() firstResponder] keyUp:event];
    [NSApp _setCurrentEvent:nil];
}

void EventSenderProxy::mouseScrollBy(int x, int y)
{
    auto cgScrollEvent = adoptCF(CGEventCreateScrollWheelEvent2(0, kCGScrollEventUnitLine, 2, y, x, 0));

    // Set the CGEvent location in flipped coords relative to the first screen, which
    // compensates for the behavior of +[NSEvent eventWithCGEvent:] when the event has
    // no associated window. See <rdar://problem/17180591>.
    CGPoint lastGlobalMousePosition = CGPointMake(m_position.x, [[[NSScreen screens] objectAtIndex:0] frame].size.height - m_position.y);
    CGEventSetLocation(cgScrollEvent.get(), lastGlobalMousePosition);

    NSEvent *event = [NSEvent eventWithCGEvent:cgScrollEvent.get()];
    if (NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[event locationInWindow]]) {
        [NSApp _setCurrentEvent:event];
        [targetView scrollWheel:event];
        [NSApp _setCurrentEvent:nil];
    } else {
        NSPoint location = [event locationInWindow];
        WTFLogAlways("mouseScrollBy failed to find the target view at %f,%f\n", location.x, location.y);
    }
}

void EventSenderProxy::continuousMouseScrollBy(int x, int y, bool paged)
{
    WTFLogAlways("EventSenderProxy::continuousMouseScrollBy is not implemented\n");
    return;
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int x, int y, int phase, int momentum)
{
    auto cgScrollEvent = adoptCF(CGEventCreateScrollWheelEvent2(0, kCGScrollEventUnitLine, 2, y, x, 0));

    // Set the CGEvent location in flipped coords relative to the first screen, which
    // compensates for the behavior of +[NSEvent eventWithCGEvent:] when the event has
    // no associated window. See <rdar://problem/17180591>.
    CGPoint lastGlobalMousePosition = CGPointMake(m_position.x, [[[NSScreen screens] objectAtIndex:0] frame].size.height - m_position.y);
    CGEventSetLocation(cgScrollEvent.get(), lastGlobalMousePosition);

    CGEventSetIntegerValueField(cgScrollEvent.get(), kCGScrollWheelEventIsContinuous, 1);
    CGEventSetIntegerValueField(cgScrollEvent.get(), kCGScrollWheelEventScrollPhase, phase);
    CGEventSetIntegerValueField(cgScrollEvent.get(), kCGScrollWheelEventMomentumPhase, momentum);

    NSEvent* event = [NSEvent eventWithCGEvent:cgScrollEvent.get()];

    // Our event should have the correct settings:
    if (NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[event locationInWindow]]) {
        [NSApp _setCurrentEvent:event];
        [targetView scrollWheel:event];
        [NSApp _setCurrentEvent:nil];
    } else {
        NSPoint windowLocation = [event locationInWindow];
        WTFLogAlways("mouseScrollByWithWheelAndMomentumPhases failed to find the target view at %f,%f\n", windowLocation.x, windowLocation.y);
    }
}

} // namespace WTR
