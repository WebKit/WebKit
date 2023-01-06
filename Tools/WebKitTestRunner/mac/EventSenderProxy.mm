/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
#import "ModifierKeys.h"
#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "WebKitTestRunnerWindow.h"
#import <Carbon/Carbon.h>
#import <WebKit/WKString.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <pal/spi/cocoa/IOKitSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

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
    CGFloat _eventSender_magnification;
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
- (id)initMagnifyEventAtLocation:(NSPoint)location globalLocation:(NSPoint)globalLocation magnification:(CGFloat)magnification phase:(NSEventPhase)phase time:(NSTimeInterval)time eventNumber:(NSInteger)eventNumber window:(NSWindow *)window;
- (NSTimeInterval)timestamp;
@end

static CGSGesturePhase EventSenderCGGesturePhaseFromNSEventPhase(NSEventPhase phase)
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

@implementation EventSenderSyntheticEvent

- (instancetype)initPressureEventAtLocation:(NSPoint)location globalLocation:(NSPoint)globalLocation stage:(NSInteger)stage pressure:(float)pressure stageTransition:(float)stageTransition phase:(NSEventPhase)phase time:(NSTimeInterval)time eventNumber:(NSInteger)eventNumber window:(NSWindow *)window
{
    auto cgEvent = adoptCF(CGEventCreate(nullptr));
    CGEventSetType(cgEvent.get(), (CGEventType)kCGSEventGesture);
    CGEventSetIntegerValueField(cgEvent.get(), kCGEventGestureHIDType, kIOHIDEventTypeForce);
    CGEventSetIntegerValueField(cgEvent.get(), kCGEventGesturePhase, EventSenderCGGesturePhaseFromNSEventPhase(phase));
    CGEventSetDoubleValueField(cgEvent.get(), kCGEventStagePressure, pressure);
    CGEventSetDoubleValueField(cgEvent.get(), kCGEventTransitionProgress, pressure);
    CGEventSetIntegerValueField(cgEvent.get(), kCGEventGestureStage, stageTransition);
    CGEventSetIntegerValueField(cgEvent.get(), kCGEventGestureBehavior, kCGSGestureBehaviorDeepPress);

    self = [super _initWithCGEvent:cgEvent.get() eventRef:nullptr];

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

- (id)initMagnifyEventAtLocation:(NSPoint)location globalLocation:(NSPoint)globalLocation magnification:(CGFloat)magnification phase:(NSEventPhase)phase time:(NSTimeInterval)time eventNumber:(NSInteger)eventNumber window:(NSWindow *)window
{
    auto cgEvent = adoptCF(CGEventCreate(nullptr));
    CGEventSetType(cgEvent.get(), (CGEventType)kCGSEventGesture);
    CGEventSetIntegerValueField(cgEvent.get(), kCGEventGestureHIDType, kIOHIDEventTypeZoom);
    CGEventSetIntegerValueField(cgEvent.get(), kCGEventGesturePhase, EventSenderCGGesturePhaseFromNSEventPhase(phase));
    CGEventSetDoubleValueField(cgEvent.get(), kCGEventGestureZoomValue, magnification);

    if (!(self = [super _initWithCGEvent:cgEvent.get() eventRef:nullptr]))
        return nil;

    _eventSender_location = location;
    _eventSender_locationInWindow = globalLocation;
    _eventSender_magnification = magnification;
    _eventSender_phase = phase;
    _eventSender_timestamp = time;
    _eventSender_eventNumber = eventNumber;
    _eventSender_window = window;
    _eventSender_type = NSEventTypeMagnify;

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

- (CGFloat)magnification
{
    return _eventSender_magnification;
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

static NSEventModifierFlags buildModifierFlags(WKEventModifiers modifiers)
{
    NSEventModifierFlags flags = 0;
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

void EventSenderProxy::mouseDown(unsigned buttonNumber, WKEventModifiers modifiers, WKStringRef pointerType)
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
                                     eventNumber:++m_eventNumber 
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

void EventSenderProxy::mouseUp(unsigned buttonNumber, WKEventModifiers modifiers, WKStringRef pointerType)
{
    m_mouseButtonsCurrentlyDown &= ~(1 << buttonNumber);

    NSEventType eventType = eventTypeForMouseButtonAndAction(buttonNumber, MouseUp);
    NSEvent *event = [NSEvent mouseEventWithType:eventType
                                        location:NSMakePoint(m_position.x, m_position.y)
                                   modifierFlags:buildModifierFlags(modifiers)
                                       timestamp:absoluteTimeForEventTime(currentEventTime())
                                    windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:++m_eventNumber 
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
        eventNumber:++m_eventNumber
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
        eventNumber:++m_eventNumber
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
        eventNumber:++m_eventNumber
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
        eventNumber:++m_eventNumber
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
        eventNumber:++m_eventNumber
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

void EventSenderProxy::mouseMoveTo(double x, double y, WKStringRef pointerType)
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
                                     eventNumber:++m_eventNumber 
                                      clickCount:(m_leftMouseButtonDown ? m_clickCount : 0) 
                                        pressure:0];

    CGEventRef cgEvent = event.CGEvent;
    CGEventSetIntegerValueField(cgEvent, kCGMouseEventDeltaX, newMousePosition.x - m_position.x);
    CGEventSetIntegerValueField(cgEvent, kCGMouseEventDeltaY, -1 * (newMousePosition.y - m_position.y));
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
            [checked_objc_cast<WKWebView>(targetView) _simulateMouseMove:event];
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
    RetainPtr<ModifierKeys> modifierKeys = [ModifierKeys modifierKeysWithKey:toWTFString(key) modifiers:buildModifierFlags(modifiers) keyLocation:keyLocation];

    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown
        location:NSMakePoint(5, 5)
        modifierFlags:modifierKeys->modifierFlags
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        characters:modifierKeys->eventCharacter.get()
        charactersIgnoringModifiers:modifierKeys->charactersIgnoringModifiers.get()
        isARepeat:NO
        keyCode:modifierKeys->keyCode];

    [NSApp _setCurrentEvent:event];
    [[m_testController->mainWebView()->platformWindow() firstResponder] keyDown:event];
    [NSApp _setCurrentEvent:nil];

    event = [NSEvent keyEventWithType:NSEventTypeKeyUp
                        location:NSMakePoint(5, 5)
                        modifierFlags:modifierKeys->modifierFlags
                        timestamp:absoluteTimeForEventTime(currentEventTime())
                        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
                        context:[NSGraphicsContext currentContext]
                        characters:modifierKeys->eventCharacter.get()
                        charactersIgnoringModifiers:modifierKeys->charactersIgnoringModifiers.get()
                        isARepeat:NO
                        keyCode:modifierKeys->keyCode];

    [NSApp _setCurrentEvent:event];
    [[m_testController->mainWebView()->platformWindow() firstResponder] keyUp:event];
    [NSApp _setCurrentEvent:nil];
}

void EventSenderProxy::rawKeyDown(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
    RetainPtr<ModifierKeys> modifierKeys = [ModifierKeys modifierKeysWithKey:toWTFString(key) modifiers:buildModifierFlags(modifiers) keyLocation:keyLocation];

    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown
        location:NSMakePoint(5, 5)
        modifierFlags:modifierKeys->modifierFlags
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        characters:modifierKeys->eventCharacter.get()
        charactersIgnoringModifiers:modifierKeys->charactersIgnoringModifiers.get()
        isARepeat:NO
        keyCode:modifierKeys->keyCode];

    [NSApp _setCurrentEvent:event];
    [[m_testController->mainWebView()->platformWindow() firstResponder] keyDown:event];
    [NSApp _setCurrentEvent:nil];
}

void EventSenderProxy::rawKeyUp(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
    RetainPtr<ModifierKeys> modifierKeys = [ModifierKeys modifierKeysWithKey:toWTFString(key) modifiers:buildModifierFlags(modifiers) keyLocation:keyLocation];

    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyUp
        location:NSMakePoint(5, 5)
        modifierFlags:modifierKeys->modifierFlags
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        characters:modifierKeys->eventCharacter.get()
        charactersIgnoringModifiers:modifierKeys->charactersIgnoringModifiers.get()
        isARepeat:NO
        keyCode:modifierKeys->keyCode];

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

static CGGesturePhase cgScrollPhaseFromPhase(EventSenderProxy::WheelEventPhase phase)
{
    switch (phase) {
    case EventSenderProxy::WheelEventPhase::None:
        return kCGGesturePhaseNone;
    case EventSenderProxy::WheelEventPhase::Began:
        return kCGGesturePhaseBegan;
    case EventSenderProxy::WheelEventPhase::Changed:
        return kCGGesturePhaseChanged;
    case EventSenderProxy::WheelEventPhase::Ended:
        return kCGGesturePhaseEnded;
    case EventSenderProxy::WheelEventPhase::Cancelled:
        return kCGGesturePhaseCancelled;
    case EventSenderProxy::WheelEventPhase::MayBegin:
        return kCGGesturePhaseMayBegin;
    }
    ASSERT_NOT_REACHED();
    return kCGGesturePhaseNone;
}

static CGMomentumScrollPhase cgMomentumPhaseFromPhase(EventSenderProxy::WheelEventPhase phase)
{
    switch (phase) {
    case EventSenderProxy::WheelEventPhase::None:
        return kCGMomentumScrollPhaseNone;
    case EventSenderProxy::WheelEventPhase::Began:
        return kCGMomentumScrollPhaseBegin;
    case EventSenderProxy::WheelEventPhase::Changed:
        return kCGMomentumScrollPhaseContinue;
    case EventSenderProxy::WheelEventPhase::Ended:
        return kCGMomentumScrollPhaseEnd;
    case EventSenderProxy::WheelEventPhase::Cancelled:
    case EventSenderProxy::WheelEventPhase::MayBegin:
        break;
    }

    ASSERT_NOT_REACHED();
    return kCGMomentumScrollPhaseNone;
}

void EventSenderProxy::sendWheelEvent(EventTimestamp timestamp, double windowX, double windowY, double deltaX, double deltaY, WheelEventPhase phase, WheelEventPhase momentumPhase)
{
    constexpr uint32_t wheelCount = 2;
    auto cgScrollEvent = adoptCF(CGEventCreateScrollWheelEvent2(nullptr, kCGScrollEventUnitPixel, wheelCount, deltaY, deltaX, 0));
    CGEventSetTimestamp(cgScrollEvent.get(), timestamp);

    // Set the CGEvent location in flipped coords relative to the first screen, which
    // compensates for the behavior of +[NSEvent eventWithCGEvent:] when the event has
    // no associated window. See <rdar://problem/17180591>.
    CGPoint flippedWindowMousePosition = CGPointMake(windowX, [[[NSScreen screens] objectAtIndex:0] frame].size.height - windowY);
    CGEventSetLocation(cgScrollEvent.get(), flippedWindowMousePosition);

    CGEventSetIntegerValueField(cgScrollEvent.get(), kCGScrollWheelEventIsContinuous, 1);
    CGEventSetIntegerValueField(cgScrollEvent.get(), kCGScrollWheelEventScrollPhase, cgScrollPhaseFromPhase(phase));
    CGEventSetIntegerValueField(cgScrollEvent.get(), kCGScrollWheelEventMomentumPhase, cgMomentumPhaseFromPhase(momentumPhase));

    NSEvent* event = [NSEvent eventWithCGEvent:cgScrollEvent.get()];
    // Our event should have the correct settings:
    if (NSView *targetView = [m_testController->mainWebView()->platformView() hitTest:[event locationInWindow]]) {
        [NSApp _setCurrentEvent:event];
        [targetView scrollWheel:event];
        [NSApp _setCurrentEvent:nil];
    } else {
        NSPoint windowLocation = [event locationInWindow];
        WTFLogAlways("EventSenderProxy::sendWheelEvent failed to find the target view at %f,%f\n", windowLocation.x, windowLocation.y);
    }
}

#if ENABLE(MAC_GESTURE_EVENTS)

void EventSenderProxy::scaleGestureStart(double scale)
{
    auto* mainWebView = m_testController->mainWebView();
    NSView *platformView = mainWebView->platformView();

    auto event = adoptNS([[EventSenderSyntheticEvent alloc] initMagnifyEventAtLocation:NSMakePoint(m_position.x, m_position.y)
        globalLocation:([mainWebView->platformWindow() convertRectToScreen:NSMakeRect(m_position.x, m_position.y, 1, 1)].origin)
        magnification:scale
        phase:NSEventPhaseBegan
        time:absoluteTimeForEventTime(currentEventTime())
        eventNumber:++m_eventNumber
        window:platformView.window]);

    if (NSView *targetView = [platformView hitTest:[event locationInWindow]]) {
        [NSApp _setCurrentEvent:event.get()];
        [targetView magnifyWithEvent:event.get()];
        [NSApp _setCurrentEvent:nil];
    } else {
        NSPoint windowLocation = [event locationInWindow];
        WTFLogAlways("gestureStart failed to find the target view at %f,%f\n", windowLocation.x, windowLocation.y);
    }
}

void EventSenderProxy::scaleGestureChange(double scale)
{
    auto* mainWebView = m_testController->mainWebView();
    NSView *platformView = mainWebView->platformView();

    auto event = adoptNS([[EventSenderSyntheticEvent alloc] initMagnifyEventAtLocation:NSMakePoint(m_position.x, m_position.y)
        globalLocation:([mainWebView->platformWindow() convertRectToScreen:NSMakeRect(m_position.x, m_position.y, 1, 1)].origin)
        magnification:scale
        phase:NSEventPhaseChanged
        time:absoluteTimeForEventTime(currentEventTime())
        eventNumber:++m_eventNumber
        window:platformView.window]);

    if (NSView *targetView = [platformView hitTest:[event locationInWindow]]) {
        [NSApp _setCurrentEvent:event.get()];
        [targetView magnifyWithEvent:event.get()];
        [NSApp _setCurrentEvent:nil];
    } else {
        NSPoint windowLocation = [event locationInWindow];
        WTFLogAlways("gestureStart failed to find the target view at %f,%f\n", windowLocation.x, windowLocation.y);
    }
}

void EventSenderProxy::scaleGestureEnd(double scale)
{
    auto* mainWebView = m_testController->mainWebView();
    NSView *platformView = mainWebView->platformView();

    auto event = adoptNS([[EventSenderSyntheticEvent alloc] initMagnifyEventAtLocation:NSMakePoint(m_position.x, m_position.y)
        globalLocation:([mainWebView->platformWindow() convertRectToScreen:NSMakeRect(m_position.x, m_position.y, 1, 1)].origin)
        magnification:scale
        phase:NSEventPhaseEnded
        time:absoluteTimeForEventTime(currentEventTime())
        eventNumber:++m_eventNumber
        window:platformView.window]);

    if (NSView *targetView = [platformView hitTest:[event locationInWindow]]) {
        [NSApp _setCurrentEvent:event.get()];
        [targetView magnifyWithEvent:event.get()];
        [NSApp _setCurrentEvent:nil];
    } else {
        NSPoint windowLocation = [event locationInWindow];
        WTFLogAlways("gestureStart failed to find the target view at %f,%f\n", windowLocation.x, windowLocation.y);
    }
}

#endif // ENABLE(MAC_GESTURE_EVENTS)

} // namespace WTR
