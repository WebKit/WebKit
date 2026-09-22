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

#import "ModifierKeys.h"
#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "SyntheticNSEvent.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "WebKitTestRunnerWindow.h"
#import <Carbon/Carbon.h>
#import <WebCore/MouseEventTypes.h>
#import <WebKit/WKString.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

@interface NSApplication (Details)
- (void)_setCurrentEvent:(NSEvent *)event;
@end

@interface NSEvent (ForTestRunner)
- (void)_postDelayed;
@end

namespace WTR {

enum class MouseAction : uint8_t {
    Down,
    Up,
    Dragged,
};

static NSEventType eventTypeForMouseButtonAndAction(WebCore::MouseButton button, MouseAction action)
{
    using namespace WebCore;
    switch (button) {
    case MouseButton::Left:
        switch (action) {
        case MouseAction::Down:
            return NSEventTypeLeftMouseDown;
        case MouseAction::Up:
            return NSEventTypeLeftMouseUp;
        case MouseAction::Dragged:
            return NSEventTypeLeftMouseDragged;
        }
    case MouseButton::Right:
        switch (action) {
        case MouseAction::Down:
            return NSEventTypeRightMouseDown;
        case MouseAction::Up:
            return NSEventTypeRightMouseUp;
        case MouseAction::Dragged:
            return NSEventTypeRightMouseDragged;
        }
    default:
        switch (action) {
        case MouseAction::Down:
            return NSEventTypeOtherMouseDown;
        case MouseAction::Up:
            return NSEventTypeOtherMouseUp;
        case MouseAction::Dragged:
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
    m_mouseButtonsCurrentlyDown.reserveInitialCapacity(5);
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

static NSInteger swizzledEventButtonNumber()
{
    return TestController::singleton().eventSenderProxy()->lastButtonDown();
}

void EventSenderProxy::mouseDown(unsigned buttonNumber, WKEventModifiers modifiers, WKStringRef pointerType, CompletionHandler<void()>&& completionHandler)
{
    auto button = WebCore::buttonFromShort(static_cast<int16_t>(buttonNumber));
    m_mouseButtonsCurrentlyDown.set(button, true);
    m_lastButtonDown = nsEventButtonNumberFromWebCoreMouseButton(button);

    updateClickCountForButton(buttonNumber);

    auto eventType = eventTypeForMouseButtonAndAction(button, MouseAction::Down);
    RetainPtr event = [NSEvent mouseEventWithType:eventType
        location:NSMakePoint(m_position.x, m_position.y)
        modifierFlags:buildModifierFlags(modifiers)
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->targetView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        eventNumber:++m_eventNumber
        clickCount:m_clickCount
        pressure:WebCore::ForceAtClick];

    RetainPtr targetView = [m_testController->targetView()->platformView() hitTest:[event locationInWindow]];
    if (!targetView) {
        if (completionHandler)
            completionHandler();
        return;
    }

    if (button == WebCore::MouseButton::Left)
        m_leftMouseButtonDown = true;

    auto dispatchMouseDown = [event, targetView] {
        auto eventPressedMouseButtonsSwizzler = makeUnique<ClassMethodSwizzler>([NSEvent class], @selector(pressedMouseButtons), reinterpret_cast<IMP>(swizzledEventPressedMouseButtons));
        auto eventButtonNumberSwizzler = makeUnique<InstanceMethodSwizzler>([NSEvent class], @selector(buttonNumber), reinterpret_cast<IMP>(swizzledEventButtonNumber));
        [NSApp _setCurrentEvent:event];
        [targetView mouseDown:event];
        [NSApp _setCurrentEvent:nil];
    };

    if (!completionHandler) {
        dispatchMouseDown();
        return;
    }

    RetainPtr webView = m_testController->targetView()->platformView();
    dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([dispatchMouseDown = WTF::move(dispatchMouseDown), webView, completionHandler = WTF::move(completionHandler)] mutable {
        dispatchMouseDown();
        [webView _doAfterProcessingAllPendingMouseEvents:makeBlockPtr([completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler();
        }).get()];
    }).get());
}

void EventSenderProxy::mouseUp(unsigned buttonNumber, WKEventModifiers modifiers, WKStringRef pointerType, CompletionHandler<void()>&& completionHandler)
{
    auto button = WebCore::buttonFromShort(static_cast<int16_t>(buttonNumber));
    m_mouseButtonsCurrentlyDown.set(button, false);
    m_lastButtonDown = nsEventButtonNumberFromWebCoreMouseButton(button);

    auto eventType = eventTypeForMouseButtonAndAction(button, MouseAction::Up);
    RetainPtr event = [NSEvent mouseEventWithType:eventType
        location:NSMakePoint(m_position.x, m_position.y)
        modifierFlags:buildModifierFlags(modifiers)
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->targetView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        eventNumber:++m_eventNumber
        clickCount:m_clickCount
        pressure:0.0];

    // FIXME: Silly hack to teach WKTR to respect capturing mouse events outside the WKView.
    // The right solution is just to use NSApplication's built-in event sending methods,
    // instead of rolling our own algorithm for selecting an event target.
    RetainPtr targetView = [m_testController->targetView()->platformView() hitTest:[event locationInWindow]];
    if (!targetView)
        targetView = m_testController->targetView()->platformView();

    if (button == WebCore::MouseButton::Left)
        m_leftMouseButtonDown = false;
    m_clickTime = currentEventTime();
    m_clickPosition = m_position;

    auto dispatchMouseUp = [event, targetView] {
        auto eventPressedMouseButtonsSwizzler = makeUnique<ClassMethodSwizzler>([NSEvent class], @selector(pressedMouseButtons), reinterpret_cast<IMP>(swizzledEventPressedMouseButtons));
        auto eventButtonNumberSwizzler = makeUnique<InstanceMethodSwizzler>([NSEvent class], @selector(buttonNumber), reinterpret_cast<IMP>(swizzledEventButtonNumber));
        [NSApp _setCurrentEvent:event];
        [targetView mouseUp:event];
        [NSApp _setCurrentEvent:nil];
    };

    if (!completionHandler) {
        dispatchMouseUp();
        return;
    }

    RetainPtr webView = m_testController->targetView()->platformView();
    dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([dispatchMouseUp = WTF::move(dispatchMouseUp), webView, completionHandler = WTF::move(completionHandler)] mutable {
        dispatchMouseUp();
        [webView _doAfterProcessingAllPendingMouseEvents:makeBlockPtr([completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler();
        }).get()];
    }).get());
}

void EventSenderProxy::sendMouseDownToStartPressureEvents()
{
    updateClickCountForButton(std::to_underlying(WebCore::MouseButton::Left));

    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
        location:NSMakePoint(m_position.x, m_position.y)
        modifierFlags:NSEventMaskPressure
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        eventNumber:++m_eventNumber
        clickCount:m_clickCount
        pressure:WebCore::ForceAtClick];

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
    RetainPtr event = adoptNS([[SyntheticNSEvent alloc] initPressureEventAtLocation:NSMakePoint(m_position.x, m_position.y)
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
    RetainPtr event = adoptNS([[SyntheticNSEvent alloc] initPressureEventAtLocation:NSMakePoint(m_position.x, m_position.y)
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

void EventSenderProxy::mouseMoveTo(double x, double y, WKStringRef pointerType, CompletionHandler<void()>&& completionHandler)
{
    auto *view = m_testController->targetView()->platformView();
    auto newMousePosition = [view convertPoint:NSMakePoint(x, y) toView:nil];
    auto isDrag = m_leftMouseButtonDown;
    RetainPtr event = [NSEvent mouseEventWithType:(isDrag ? NSEventTypeLeftMouseDragged : NSEventTypeMouseMoved)
        location:newMousePosition
        modifierFlags:0
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:view.window.windowNumber
        context:[NSGraphicsContext currentContext]
        eventNumber:++m_eventNumber
        clickCount:(m_leftMouseButtonDown ? m_clickCount : 0)
        pressure:0];

    CGEventRef cgEvent = event.get().CGEvent;
    CGEventSetIntegerValueField(cgEvent, kCGMouseEventDeltaX, newMousePosition.x - m_position.x);
    CGEventSetIntegerValueField(cgEvent, kCGMouseEventDeltaY, -1 * (newMousePosition.y - m_position.y));
    event = [NSEvent eventWithCGEvent:cgEvent];
    m_position.x = newMousePosition.x;
    m_position.y = newMousePosition.y;

    m_testController->targetView()->setCursorOverlayPosition(x, y);

    RetainPtr webView = m_testController->targetView()->platformView();

    // Always target drags at the WKWebView to allow for drag-scrolling outside the view.
    // For non-drag moves, _simulateMouseMove: must be called on the WKWebView directly
    // (not on a hitTest: result, which may not be a WKWebView).
    auto dispatchMouseMove = [event, webView, isDrag] {
        if (isDrag) {
            auto eventPressedMouseButtonsSwizzler = makeUnique<ClassMethodSwizzler>([NSEvent class], @selector(pressedMouseButtons), reinterpret_cast<IMP>(swizzledEventPressedMouseButtons));
            [NSApp _setCurrentEvent:event];
            [webView mouseDragged:event];
            [NSApp _setCurrentEvent:nil];
        } else {
            [NSApp _setCurrentEvent:event];
            [webView _simulateMouseMove:event];
            [NSApp _setCurrentEvent:nil];
        }
    };

    if (!completionHandler) {
        dispatchMouseMove();
        return;
    }

    dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([dispatchMouseMove = WTF::move(dispatchMouseMove), webView, completionHandler = WTF::move(completionHandler)] mutable {
        dispatchMouseMove();
        [webView _doAfterProcessingAllPendingMouseEvents:makeBlockPtr([completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler();
        }).get()];
    }).get());
}

void EventSenderProxy::leapForward(int milliseconds)
{
    m_time += milliseconds / 1000.0;
}

void EventSenderProxy::keyDown(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation, CompletionHandler<void()>&& completionHandler)
{
    RetainPtr modifierKeys = [ModifierKeys modifierKeysWithKey:toWTFString(key).createNSString().get() modifiers:buildModifierFlags(modifiers) keyLocation:keyLocation];

    RetainPtr keyDownEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown
        location:NSMakePoint(5, 5)
        modifierFlags:modifierKeys->modifierFlags
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        characters:modifierKeys->eventCharacter.get()
        charactersIgnoringModifiers:modifierKeys->charactersIgnoringModifiers.get()
        isARepeat:NO
        keyCode:modifierKeys->keyCode];

    RetainPtr keyUpEvent = [NSEvent keyEventWithType:NSEventTypeKeyUp
        location:NSMakePoint(5, 5)
        modifierFlags:modifierKeys->modifierFlags
        timestamp:absoluteTimeForEventTime(currentEventTime())
        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
        context:[NSGraphicsContext currentContext]
        characters:modifierKeys->eventCharacter.get()
        charactersIgnoringModifiers:modifierKeys->charactersIgnoringModifiers.get()
        isARepeat:NO
        keyCode:modifierKeys->keyCode];

    RetainPtr firstResponder = [m_testController->mainWebView()->platformWindow() firstResponder];

    auto dispatchKeyEvents = [keyDownEvent, keyUpEvent, firstResponder] {
        [NSApp _setCurrentEvent:keyDownEvent];
        [firstResponder keyDown:keyDownEvent];
        [NSApp _setCurrentEvent:nil];

        [NSApp _setCurrentEvent:keyUpEvent];
        [firstResponder keyUp:keyUpEvent];
        [NSApp _setCurrentEvent:nil];
    };

    if (!completionHandler) {
        dispatchKeyEvents();
        return;
    }

    RetainPtr webView = m_testController->mainWebView()->platformView();

    dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([dispatchKeyEvents = WTF::move(dispatchKeyEvents), webView, completionHandler = WTF::move(completionHandler)] mutable {
        dispatchKeyEvents();

        [webView _doAfterProcessingAllPendingKeyEvents:makeBlockPtr([completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler();
        }).get()];
    }).get());
}

void EventSenderProxy::rawKeyDown(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
    RetainPtr<ModifierKeys> modifierKeys = [ModifierKeys modifierKeysWithKey:toWTFString(key).createNSString().get() modifiers:buildModifierFlags(modifiers) keyLocation:keyLocation];

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
    RetainPtr<ModifierKeys> modifierKeys = [ModifierKeys modifierKeysWithKey:toWTFString(key).createNSString().get() modifiers:buildModifierFlags(modifiers) keyLocation:keyLocation];

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
    dispatchSyntheticEvent(event, m_testController->mainWebView()->platformView(), @"mouseScrollBy", ^(NSView *targetView, NSEvent *syntheticEvent) {
        [targetView scrollWheel:syntheticEvent];
    });
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
    // Set a value that won't be interpreted as a falsy timestamp:
    CGEventSetTimestamp(cgScrollEvent.get(), 1);

    NSEvent* event = [NSEvent eventWithCGEvent:cgScrollEvent.get()];

    // Our event should have the correct settings:
    dispatchSyntheticEvent(event, m_testController->mainWebView()->platformView(), @"mouseScrollByWithWheelAndMomentumPhases", ^(NSView *targetView, NSEvent *syntheticEvent) {
        [targetView scrollWheel:syntheticEvent];
    });
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

    const char* markerMessage = nullptr;
    if (phase == WheelEventPhase::Ended || phase == WheelEventPhase::Cancelled)
        markerMessage = "SentWheelPhaseEndOrCancel";
    else if (momentumPhase == WheelEventPhase::Ended)
        markerMessage = "SentWheelMomentumPhaseEnd";

    if (markerMessage)
        WKPagePostMessageToInjectedBundle(m_testController->mainWebView()->page(), toWK("WheelEventMarker").get(), toWK(markerMessage).get());

    NSEvent* event = [NSEvent eventWithCGEvent:cgScrollEvent.get()];
    // Our event should have the correct settings:
    dispatchSyntheticEvent(event, m_testController->mainWebView()->platformView(), @"EventSenderProxy::sendWheelEvent", ^(NSView *targetView, NSEvent *syntheticEvent) {
        [targetView scrollWheel:syntheticEvent];
    });
}

void EventSenderProxy::smartMagnify()
{
    auto* mainWebView = m_testController->mainWebView();
    NSView *platformView = mainWebView->platformView();

    RetainPtr event = adoptNS([[SyntheticNSEvent alloc] initSmartMagnifyEventAtLocation:NSMakePoint(m_position.x, m_position.y)
        globalLocation:([mainWebView->platformWindow() convertRectToScreen:NSMakeRect(m_position.x, m_position.y, 1, 1)].origin)
        time:absoluteTimeForEventTime(currentEventTime())
        eventNumber:++m_eventNumber
        window:platformView.window]);

    dispatchSyntheticEvent(event, platformView, @"smartMagnify", ^(NSView *targetView, NSEvent *syntheticEvent) {
        [targetView smartMagnifyWithEvent:syntheticEvent];
    });
}

static void sendMagnifyEvent(TestController& testController, WKPoint position, NSInteger eventNumber, NSTimeInterval time, double scale, NSEventPhase phase, const String& description)
{
    auto* mainWebView = testController.mainWebView();
    RetainPtr platformView = mainWebView->platformView();

    RetainPtr event = adoptNS([[SyntheticNSEvent alloc] initMagnifyEventAtLocation:NSMakePoint(position.x, position.y)
        globalLocation:([mainWebView->platformWindow() convertRectToScreen:NSMakeRect(position.x, position.y, 1, 1)].origin)
        magnification:scale
        phase:phase
        time:time
        eventNumber:eventNumber
        window:[platformView window]]);

    dispatchSyntheticEvent(event, platformView, description.createNSString(), ^(NSView *targetView, NSEvent *syntheticEvent) {
        [targetView magnifyWithEvent:syntheticEvent];
    });
}

void EventSenderProxy::scaleGestureStart(double scale)
{
    sendMagnifyEvent(*m_testController, m_position, ++m_eventNumber, absoluteTimeForEventTime(currentEventTime()), scale, NSEventPhaseBegan, "scaleGestureStart"_s);
}

void EventSenderProxy::scaleGestureChange(double scale)
{
    sendMagnifyEvent(*m_testController, m_position, ++m_eventNumber, absoluteTimeForEventTime(currentEventTime()), scale, NSEventPhaseChanged, "scaleGestureChange"_s);
}

void EventSenderProxy::scaleGestureEnd(double scale)
{
    sendMagnifyEvent(*m_testController, m_position, ++m_eventNumber, absoluteTimeForEventTime(currentEventTime()), scale, NSEventPhaseEnded, "scaleGestureEnd"_s);
}

} // namespace WTR
