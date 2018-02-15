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

#if PLATFORM(IOS)

#import "IntPoint.h"
#import "KeyEventCocoa.h"
#import "Logging.h"
#import "WAKAppKitStubs.h"
#import "WebEvent.h"
#import "WindowsKeyboardCodes.h"
#import <wtf/WallTime.h>

namespace WebCore {

static OptionSet<PlatformEvent::Modifier> modifiersForEvent(WebEvent *event)
{
    OptionSet<PlatformEvent::Modifier> modifiers;

    if (event.modifierFlags & WebEventFlagMaskShift)
        modifiers |= PlatformEvent::Modifier::ShiftKey;
    if (event.modifierFlags & WebEventFlagMaskControl)
        modifiers |= PlatformEvent::Modifier::CtrlKey;
    if (event.modifierFlags & WebEventFlagMaskAlternate)
        modifiers |= PlatformEvent::Modifier::AltKey;
    if (event.modifierFlags & WebEventFlagMaskCommand)
        modifiers |= PlatformEvent::Modifier::MetaKey;
    if (event.modifierFlags & WebEventFlagMaskAlphaShift)
        modifiers |= PlatformEvent::Modifier::CapsLockKey;

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
        m_timestamp = WallTime::now();

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
        m_timestamp = WallTime::now();

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

String keyForKeyEvent(WebEvent *event)
{
    NSString *characters = event.characters;
    auto length = [characters length];

    // characters return an empty string for dead keys.
    // https://developer.apple.com/reference/appkit/nsevent/1534183-characters
    // "Dead" is defined here https://w3c.github.io/uievents-key/#keys-composition.
    if (!length)
        return ASCIILiteral("Dead");

    if (length > 1)
        return characters;

    return keyForCharCode([characters characterAtIndex:0]);
}

// https://w3c.github.io/uievents-code/
String codeForKeyEvent(WebEvent *event)
{
    switch (event.keyCode) {
    // Keys in the alphanumeric section.
    case VK_OEM_3: return ASCIILiteral("Backquote");
    case VK_OEM_5: return ASCIILiteral("Backslash");
    case VK_BACK: return ASCIILiteral("Backspace");
    case VK_OEM_4: return ASCIILiteral("BracketLeft");
    case VK_OEM_6: return ASCIILiteral("BracketRight");
    case VK_OEM_COMMA: return ASCIILiteral("Comma");
    case VK_0: return ASCIILiteral("Digit0");
    case VK_1: return ASCIILiteral("Digit1");
    case VK_2: return ASCIILiteral("Digit2");
    case VK_3: return ASCIILiteral("Digit3");
    case VK_4: return ASCIILiteral("Digit4");
    case VK_5: return ASCIILiteral("Digit5");
    case VK_6: return ASCIILiteral("Digit6");
    case VK_7: return ASCIILiteral("Digit7");
    case VK_8: return ASCIILiteral("Digit8");
    case VK_9: return ASCIILiteral("Digit9");
    case VK_OEM_PLUS: return ASCIILiteral("Equal");
    case VK_OEM_102: return ASCIILiteral("IntlBackslash");
    // IntlRo.
    // IntlYen.
    case VK_A: return ASCIILiteral("KeyA");
    case VK_B: return ASCIILiteral("KeyB");
    case VK_C: return ASCIILiteral("KeyC");
    case VK_D: return ASCIILiteral("KeyD");
    case VK_E: return ASCIILiteral("KeyE");
    case VK_F: return ASCIILiteral("KeyF");
    case VK_G: return ASCIILiteral("KeyG");
    case VK_H: return ASCIILiteral("KeyH");
    case VK_I: return ASCIILiteral("KeyI");
    case VK_J: return ASCIILiteral("KeyJ");
    case VK_K: return ASCIILiteral("KeyK");
    case VK_L: return ASCIILiteral("KeyL");
    case VK_M: return ASCIILiteral("KeyM");
    case VK_N: return ASCIILiteral("KeyN");
    case VK_O: return ASCIILiteral("KeyO");
    case VK_P: return ASCIILiteral("KeyP");
    case VK_Q: return ASCIILiteral("KeyQ");
    case VK_R: return ASCIILiteral("KeyR");
    case VK_S: return ASCIILiteral("KeyS");
    case VK_T: return ASCIILiteral("KeyT");
    case VK_U: return ASCIILiteral("KeyU");
    case VK_V: return ASCIILiteral("KeyV");
    case VK_W: return ASCIILiteral("KeyW");
    case VK_X: return ASCIILiteral("KeyX");
    case VK_Y: return ASCIILiteral("KeyY");
    case VK_Z: return ASCIILiteral("KeyZ");
    case VK_OEM_MINUS: return ASCIILiteral("Minus");
    case VK_OEM_PERIOD: return ASCIILiteral("Period");
    case VK_OEM_7: return ASCIILiteral("Quote");
    case VK_OEM_1: return ASCIILiteral("Semicolon");
    case VK_OEM_2: return ASCIILiteral("Slash");

    // Functional keys in alphanumeric section.
    case VK_MENU: return ASCIILiteral("AltLeft");
    // AltRight.
    case VK_CAPITAL: return ASCIILiteral("CapsLock");
    // ContextMenu.
    case VK_LCONTROL: return ASCIILiteral("ControlLeft");
    case VK_RCONTROL: return ASCIILiteral("ControlRight");
    case VK_RETURN: return ASCIILiteral("Enter"); //  Labeled Return on Apple keyboards.
    case VK_LWIN: return ASCIILiteral("MetaLeft");
    case VK_RWIN: return ASCIILiteral("MetaRight");
    case VK_LSHIFT: return ASCIILiteral("ShiftLeft");
    case VK_RSHIFT: return ASCIILiteral("ShiftRight");
    case VK_SPACE: return ASCIILiteral("Space");
    case VK_TAB: return ASCIILiteral("Tab");

    // Functional keys found on Japanese and Korean keyboards.
    // Convert.
    case VK_KANA: return ASCIILiteral("KanaMode");
    // Lang1.
    // Lang2.
    // Lang3.
    // Lang4.
    // Lang5.
    // NonConvert.

    // Keys in the ControlPad section.
    // Delete
    case VK_END: return ASCIILiteral("End");
    case VK_HELP: return ASCIILiteral("Help");
    case VK_HOME: return ASCIILiteral("Home");
    // Insert: Not present on Apple keyboards.
    case VK_NEXT: return ASCIILiteral("PageDown");
    case VK_PRIOR: return ASCIILiteral("PageUp");

    // Keys in the ArrowPad section.
    case VK_DOWN: return ASCIILiteral("ArrowDown");
    case VK_LEFT: return ASCIILiteral("ArrowLeft");
    case VK_RIGHT: return ASCIILiteral("ArrowRight");
    case VK_UP: return ASCIILiteral("ArrowUp");

    // Keys in the Numpad section.
    case VK_NUMLOCK: return ASCIILiteral("NumLock");
    case VK_NUMPAD0: return ASCIILiteral("Numpad0");
    case VK_NUMPAD1: return ASCIILiteral("Numpad1");
    case VK_NUMPAD2: return ASCIILiteral("Numpad2");
    case VK_NUMPAD3: return ASCIILiteral("Numpad3");
    case VK_NUMPAD4: return ASCIILiteral("Numpad4");
    case VK_NUMPAD5: return ASCIILiteral("Numpad5");
    case VK_NUMPAD6: return ASCIILiteral("Numpad6");
    case VK_NUMPAD7: return ASCIILiteral("Numpad7");
    case VK_NUMPAD8: return ASCIILiteral("Numpad8");
    case VK_NUMPAD9: return ASCIILiteral("Numpad9");
    case VK_ADD: return ASCIILiteral("NumpadAdd");
    // NumpadBackspace.
    // NumpadClear.
    // NumpadClearEntry.
    case VK_SEPARATOR: return ASCIILiteral("NumpadComma");
    case VK_DECIMAL: return ASCIILiteral("NumpadDecimal");
    case VK_DIVIDE: return ASCIILiteral("NumpadDivide");
    // NumpadEnter.
    case VK_CLEAR: return ASCIILiteral("NumpadEqual");
    // NumpadHash.
    // NumpadMemoryAdd.
    // NumpadMemoryClear.
    // NumpadMemoryRecall.
    // NumpadMemoryStore.
    // NumpadMemorySubtract.
    case VK_MULTIPLY: return ASCIILiteral("NumpadMultiply");
    // NumpadParenLeft.
    // NumpadParenRight.
    // NumpadStar: The specification says to use "NumpadMultiply" for the * key on numeric keypads.
    case VK_SUBTRACT: return ASCIILiteral("NumpadSubtract");

    // Keys in the Function section.
    case VK_ESCAPE: return ASCIILiteral("Escape");
    case VK_F1: return ASCIILiteral("F1");
    case VK_F2: return ASCIILiteral("F2");
    case VK_F3: return ASCIILiteral("F3");
    case VK_F4: return ASCIILiteral("F4");
    case VK_F5: return ASCIILiteral("F5");
    case VK_F6: return ASCIILiteral("F6");
    case VK_F7: return ASCIILiteral("F7");
    case VK_F8: return ASCIILiteral("F8");
    case VK_F9: return ASCIILiteral("F9");
    case VK_F10: return ASCIILiteral("F10");
    case VK_F11: return ASCIILiteral("F11");
    case VK_F12: return ASCIILiteral("F12");
    case VK_F13: return ASCIILiteral("F13");
    case VK_F14: return ASCIILiteral("F14");
    case VK_F15: return ASCIILiteral("F15");
    case VK_F16: return ASCIILiteral("F16");
    case VK_F17: return ASCIILiteral("F17");
    case VK_F18: return ASCIILiteral("F18");
    case VK_F19: return ASCIILiteral("F19");
    case VK_F20: return ASCIILiteral("F20");
    // Fn: This is typically a hardware key that does not generate a separate code.
    // FnLock.
    // PrintScreen.
    // ScrollLock.
    // Pause.

    // Media keys.
    // BrowserBack.
    // BrowserFavorites.
    // BrowserForward.
    // BrowserHome.
    // BrowserRefresh.
    // BrowserSearch.
    // BrowserStop.
    // Eject.
    // LaunchApp1.
    // LaunchApp2.
    // LaunchMail.
    // MediaPlayPause.
    // MediaSelect.
    // MediaStop.
    // MediaTrackNext.
    // MediaTrackPrevious.
    // Power.
    // Sleep.
    case VK_VOLUME_DOWN: return ASCIILiteral("AudioVolumeDown");
    case VK_VOLUME_MUTE: return ASCIILiteral("AudioVolumeMute");
    case VK_VOLUME_UP: return ASCIILiteral("AudioVolumeUp");
    // WakeUp.

    // Legacy modifier keys.
    // Hyper.
    // Super.
    // Turbo.

    // Legacy process control keys.
    // Abort.
    // Resume.
    // Suspend.

    // Legacy editing keys.
    // Again.
    // Copy.
    // Cut.
    // Find.
    // Open.
    // Paste.
    // Props.
    // Select.
    // Undo.

    // Keys found on international keyboards.
    // Hiragana.
    // Katakana.

    default:
        return ASCIILiteral("Unidentified");
    }
}

class PlatformKeyboardEventBuilder : public PlatformKeyboardEvent {
public:
    PlatformKeyboardEventBuilder(WebEvent *event)
    {
        ASSERT(event.type == WebEventKeyDown || event.type == WebEventKeyUp);

        m_type = (event.type == WebEventKeyUp ? PlatformEvent::KeyUp : PlatformEvent::KeyDown);
        m_modifiers = modifiersForEvent(event);
        m_timestamp = WallTime::now();

        m_text = event.characters;
        m_unmodifiedText = event.charactersIgnoringModifiers;
        m_key = keyForKeyEvent(event);
        m_code = codeForKeyEvent(event);
        m_keyIdentifier = keyIdentifierForKeyEvent(event);
        m_windowsVirtualKeyCode = event.keyCode;
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
    
static PlatformTouchPoint::TouchPhaseType touchPhaseFromPlatformEventType(PlatformEvent::Type type)
{
    switch (type) {
    case PlatformEvent::TouchStart:
        return PlatformTouchPoint::TouchPhaseBegan;
    case PlatformEvent::TouchMove:
        return PlatformTouchPoint::TouchPhaseMoved;
    case PlatformEvent::TouchEnd:
        return PlatformTouchPoint::TouchPhaseEnded;
    default:
        ASSERT_NOT_REACHED();
        return PlatformTouchPoint::TouchPhaseCancelled;
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
        m_timestamp = WallTime::fromRawSeconds(event.timestamp);

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
    
    PlatformTouchEventBuilder(PlatformEvent::Type type, IntPoint location)
    {
        m_type = type;
        m_timestamp = WallTime::now();
        
        m_gestureScale = 1;
        m_gestureRotation = 0;
        m_isGesture = 0;
        m_position = location;
        m_globalPosition = location;
        m_isPotentialTap = true;
        
        unsigned touchCount = 1;
        m_touchPoints.reserveInitialCapacity(touchCount);
        for (unsigned i = 0; i < touchCount; ++i)
            m_touchPoints.uncheckedAppend(PlatformTouchPointBuilder(1, location, touchPhaseFromPlatformEventType(type)));
    }
};

PlatformTouchEvent PlatformEventFactory::createPlatformTouchEvent(WebEvent *event)
{
    return PlatformTouchEventBuilder(event);
}
    
PlatformTouchEvent PlatformEventFactory::createPlatformSimulatedTouchEvent(PlatformEvent::Type type, IntPoint location)
{
    return PlatformTouchEventBuilder(type, location);
}

#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebCore

#endif // PLATFORM(IOS)
