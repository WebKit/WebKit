/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2011, 2014 Apple Inc. All rights reserved.
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
#import "PlatformEventFactoryIOS.h"

#import <Foundation/NSGeometry.h>
#import <IntPoint.h>
#import <KeyEventCocoa.h>
#import <Logging.h>
#import <WebEvent.h>

namespace WebCore {

static unsigned modifiersForEvent(WebEvent *event)
{
    unsigned modifiers = 0;

    if (event.modifierFlags & WebEventFlagMaskShift)
        modifiers |= PlatformEvent::ShiftKey;
    if (event.modifierFlags & WebEventFlagMaskControl)
        modifiers |= PlatformEvent::CtrlKey;
    if (event.modifierFlags & WebEventFlagMaskAlternate)
        modifiers |= PlatformEvent::AltKey;
    if (event.modifierFlags & WebEventFlagMaskCommand)
        modifiers |= PlatformEvent::MetaKey;

    return modifiers;
}

static inline IntPoint pointForEvent(WebEvent *event)
{
    return IntPoint(event.locationInWindow);
}

static inline IntPoint globalPointForEvent(WebEvent *event)
{
    // iOS WebKit works as if it is full screen. Therefore Web coords are Global coords.
    return pointForEvent(event);
}

static PlatformEvent::Type mouseEventType(WebEvent *event)
{
    switch (event.type) {
    case WebEventMouseDown:
        return PlatformEvent::MousePressed;
    case WebEventMouseUp:
        return PlatformEvent::MouseReleased;
    case WebEventMouseMoved:
        return PlatformEvent::MouseMoved;
    default:
        ASSERT_NOT_REACHED();
        return PlatformEvent::MousePressed;
    }
}

class PlatformMouseEventBuilder : public PlatformMouseEvent {
public:
    PlatformMouseEventBuilder(WebEvent *event)
    {
        m_type = mouseEventType(event);
        m_modifiers = 0;
        m_timestamp = currentTime();

        m_position = pointForEvent(event);
        m_globalPosition = globalPointForEvent(event);
        m_button = LeftButton; // This has always been the LeftButton on iOS.
        m_clickCount = 1; // This has always been 1 on iOS.
    }
};

PlatformMouseEvent PlatformEventFactory::createPlatformMouseEvent(WebEvent *event)
{
    return PlatformMouseEventBuilder(event);
}

class PlatformWheelEventBuilder : public PlatformWheelEvent {
public:
    PlatformWheelEventBuilder(WebEvent *event)
    {
        ASSERT(event.type == WebEventScrollWheel);

        m_type = PlatformEvent::Wheel;
        m_modifiers = 0;
        m_timestamp = currentTime();

        m_position = pointForEvent(event);
        m_globalPosition = globalPointForEvent(event);
        m_deltaX = event.deltaX;
        m_deltaY = event.deltaY;
        m_granularity = ScrollByPixelWheelEvent; // iOS only supports continuous (pixel-mode) scrolling.
    }
};

PlatformWheelEvent PlatformEventFactory::createPlatformWheelEvent(WebEvent *event)
{
    return PlatformWheelEventBuilder(event);
}

String keyIdentifierForKeyEvent(WebEvent *event)
{
    NSString *s = event.charactersIgnoringModifiers;
    if ([s length] != 1) {
        LOG(Events, "received an unexpected number of characters in key event: %u", [s length]);
        return "Unidentified";
    }

    return keyIdentifierForCharCode(CFStringGetCharacterAtIndex((CFStringRef)s, 0));
}

class PlatformKeyboardEventBuilder : public PlatformKeyboardEvent {
public:
    PlatformKeyboardEventBuilder(WebEvent *event)
    {
        ASSERT(event.type == WebEventKeyDown || event.type == WebEventKeyUp);

        m_type = (event.type == WebEventKeyUp ? PlatformEvent::KeyUp : PlatformEvent::KeyDown);
        m_modifiers = modifiersForEvent(event);
        m_timestamp = currentTime();

        m_text = event.characters;
        m_unmodifiedText = event.charactersIgnoringModifiers;
        m_keyIdentifier = keyIdentifierForKeyEvent(event);
        m_windowsVirtualKeyCode = event.keyCode;
        m_macCharCode = 0;
        m_autoRepeat = event.isKeyRepeating;
        m_isKeypad = false; // iOS does not distinguish the numpad. See <rdar://problem/7190835>.
        m_isSystemKey = false;
        m_Event = event;

        // Always use 13 for Enter/Return -- we don't want to use AppKit's different character for Enter.
        if (m_windowsVirtualKeyCode == '\r') {
            m_text = "\r";
            m_unmodifiedText = "\r";
        }

        // The adjustments below are only needed in backward compatibility mode, but we cannot tell what mode we are in from here.

        // Turn 0x7F into 8, because backspace needs to always be 8.
        if (m_text == "\x7F")
            m_text = "\x8";
        if (m_unmodifiedText == "\x7F")
            m_unmodifiedText = "\x8";
        // Always use 9 for tab -- we don't want to use AppKit's different character for shift-tab.
        if (m_windowsVirtualKeyCode == 9) {
            m_text = "\x9";
            m_unmodifiedText = "\x9";
        }
    }
};

PlatformKeyboardEvent PlatformEventFactory::createPlatformKeyboardEvent(WebEvent *event)
{
    return PlatformKeyboardEventBuilder(event);
}

#if ENABLE(TOUCH_EVENTS)
static PlatformTouchPoint::TouchPhaseType convertTouchPhase(NSNumber *touchPhaseNumber)
{
    WebEventTouchPhaseType touchPhase = static_cast<WebEventTouchPhaseType>([touchPhaseNumber unsignedIntValue]);
    switch (touchPhase) {
    case WebEventTouchPhaseBegan:
        return PlatformTouchPoint::TouchPhaseBegan;
    case WebEventTouchPhaseMoved:
        return PlatformTouchPoint::TouchPhaseMoved;
    case WebEventTouchPhaseStationary:
        return PlatformTouchPoint::TouchPhaseStationary;
    case WebEventTouchPhaseEnded:
        return PlatformTouchPoint::TouchPhaseEnded;
    case WebEventTouchPhaseCancelled:
        return PlatformTouchPoint::TouchPhaseCancelled;
    default:
        ASSERT_NOT_REACHED();
    }
    return PlatformTouchPoint::TouchPhaseBegan;
}

static PlatformEvent::Type touchEventType(WebEvent *event)
{
    switch (event.type) {
    case WebEventTouchBegin:
        return PlatformEvent::TouchStart;
    case WebEventTouchEnd:
        return PlatformEvent::TouchEnd;
    case WebEventTouchCancel:
        return PlatformEvent::TouchCancel;
    case WebEventTouchChange:
        return PlatformEvent::TouchMove;
    default:
        ASSERT_NOT_REACHED();
        return PlatformEvent::TouchCancel;
    }
}

class PlatformTouchPointBuilder : public PlatformTouchPoint {
public:
    PlatformTouchPointBuilder(unsigned identifier, const IntPoint& location, TouchPhaseType phase)
        : PlatformTouchPoint(identifier, location, phase)
    {
    }
};

class PlatformTouchEventBuilder : public PlatformTouchEvent {
public:
    PlatformTouchEventBuilder(WebEvent *event)
    {
        m_type = touchEventType(event);
        m_modifiers = modifiersForEvent(event);
        m_timestamp = event.timestamp;

        m_gestureScale = event.gestureScale;
        m_gestureRotation = event.gestureRotation;
        m_isGesture = event.isGesture;
        m_position = pointForEvent(event);
        m_globalPosition = globalPointForEvent(event);

        unsigned touchCount = event.touchCount;
        m_touchPoints.reserveInitialCapacity(touchCount);
        for (unsigned i = 0; i < touchCount; ++i) {
            unsigned identifier = [(NSNumber *)[event.touchIdentifiers objectAtIndex:i] unsignedIntValue];
            IntPoint location = IntPoint([(NSValue *)[event.touchLocations objectAtIndex:i] pointValue]);
            PlatformTouchPoint::TouchPhaseType touchPhase = convertTouchPhase([event.touchPhases objectAtIndex:i]);
            m_touchPoints.uncheckedAppend(PlatformTouchPointBuilder(identifier, location, touchPhase));
        }
    }
};

PlatformTouchEvent PlatformEventFactory::createPlatformTouchEvent(WebEvent *event)
{
    return PlatformTouchEventBuilder(event);
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebCore
