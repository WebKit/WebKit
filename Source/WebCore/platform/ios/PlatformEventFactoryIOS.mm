/*
 * Copyright (C) 2004, 2006-2011, 2014, 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "IntPoint.h"
#import "KeyEventCocoa.h"
#import "KeyEventCodesIOS.h"
#import "Logging.h"
#import "WAKAppKitStubs.h"
#import "WebEvent.h"
#import "WindowsKeyboardCodes.h"
#import <wtf/Optional.h>
#import <wtf/WallTime.h>

namespace WebCore {

static OptionSet<PlatformEvent::Modifier> modifiersForEvent(WebEvent *event)
{
    OptionSet<PlatformEvent::Modifier> modifiers;

    if (event.modifierFlags & WebEventFlagMaskShiftKey)
        modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (event.modifierFlags & WebEventFlagMaskControlKey)
        modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (event.modifierFlags & WebEventFlagMaskOptionKey)
        modifiers.add(PlatformEvent::Modifier::AltKey);
    if (event.modifierFlags & WebEventFlagMaskCommandKey)
        modifiers.add(PlatformEvent::Modifier::MetaKey);
    if (event.modifierFlags & WebEventFlagMaskLeftCapsLockKey)
        modifiers.add(PlatformEvent::Modifier::CapsLockKey);

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
        m_modifiers = modifiersForEvent(event);
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
    if (event.keyboardFlags & WebEventKeyboardInputModifierFlagsChanged) {
        switch (event.keyCode) {
        case VK_LWIN: // Left Command
        case VK_APPS: // Right Command
            return "Meta"_s;

        case VK_CAPITAL: // Caps Lock
            return "CapsLock"_s;

        case VK_LSHIFT: // Left Shift
        case VK_RSHIFT: // Right Shift
            return "Shift"_s;

        case VK_LMENU: // Left Alt
        case VK_RMENU: // Right Alt
            return "Alt"_s;

        case VK_LCONTROL: // Left Ctrl
        case VK_RCONTROL: // Right Ctrl
            return "Control"_s;

        default:
            ASSERT_NOT_REACHED();
            return emptyString();
        }
    }
    NSString *characters = event.charactersIgnoringModifiers;
    if ([characters length] != 1) {
        LOG(Events, "received an unexpected number of characters in key event: %u", [characters length]);
        return "Unidentified"_s;
    }
    return keyIdentifierForCharCode([characters characterAtIndex:0]);
}

String keyForKeyEvent(WebEvent *event)
{
    if (event.keyboardFlags & WebEventKeyboardInputModifierFlagsChanged) {
        switch (event.keyCode) {
        case VK_LWIN: // Left Command
        case VK_APPS: // Right Command
            return "Meta"_s;

        case VK_CAPITAL: // Caps Lock
            return "CapsLock"_s;

        case VK_LSHIFT: // Left Shift
        case VK_RSHIFT: // Right Shift
            return "Shift"_s;

        case VK_LMENU: // Left Alt
        case VK_RMENU: // Right Alt
            return "Alt"_s;

        case VK_LCONTROL: // Left Ctrl
        case VK_RCONTROL: // Right Ctrl
            return "Control"_s;

        default:
            ASSERT_NOT_REACHED();
            return "Unidentified"_s;
        }
    }

    // If more than one key is being pressed and the key combination includes one or more modifier keys
    // that result in the key no longer producing a printable character (e.g., Control + a), then the
    // key value should be the printable key value that would have been produced if the key had been
    // typed with the default keyboard layout with no modifier keys except for Shift and AltGr applied.
    // See <https://www.w3.org/TR/2015/WD-uievents-20151215/#keys-guidelines>.
    bool isControlDown = event.modifierFlags & WebEventFlagMaskControlKey;
    NSString *characters = isControlDown ? event.charactersIgnoringModifiers : event.characters;
    auto length = [characters length];
    // characters return an empty string for dead keys.
    // https://developer.apple.com/reference/appkit/nsevent/1534183-characters
    // "Dead" is defined here https://w3c.github.io/uievents-key/#keys-composition.
    if (!length)
        return "Dead"_s;
    if (length > 1)
        return characters;
    return keyForCharCode([characters characterAtIndex:0]);
}

// https://w3c.github.io/uievents-code/
String codeForKeyEvent(WebEvent *event)
{
    switch (event.keyCode) {
    // Keys in the alphanumeric section.
    case VK_OEM_3: return "Backquote"_s;
    case VK_OEM_5: return "Backslash"_s;
    case VK_BACK: return "Backspace"_s;
    case VK_OEM_4: return "BracketLeft"_s;
    case VK_OEM_6: return "BracketRight"_s;
    case VK_OEM_COMMA: return "Comma"_s;
    case VK_0: return "Digit0"_s;
    case VK_1: return "Digit1"_s;
    case VK_2: return "Digit2"_s;
    case VK_3: return "Digit3"_s;
    case VK_4: return "Digit4"_s;
    case VK_5: return "Digit5"_s;
    case VK_6: return "Digit6"_s;
    case VK_7: return "Digit7"_s;
    case VK_8: return "Digit8"_s;
    case VK_9: return "Digit9"_s;
    case VK_OEM_PLUS: return "Equal"_s;
    case VK_OEM_102: return "IntlBackslash"_s;
    // IntlRo.
    // IntlYen.
    case VK_A: return "KeyA"_s;
    case VK_B: return "KeyB"_s;
    case VK_C: return "KeyC"_s;
    case VK_D: return "KeyD"_s;
    case VK_E: return "KeyE"_s;
    case VK_F: return "KeyF"_s;
    case VK_G: return "KeyG"_s;
    case VK_H: return "KeyH"_s;
    case VK_I: return "KeyI"_s;
    case VK_J: return "KeyJ"_s;
    case VK_K: return "KeyK"_s;
    case VK_L: return "KeyL"_s;
    case VK_M: return "KeyM"_s;
    case VK_N: return "KeyN"_s;
    case VK_O: return "KeyO"_s;
    case VK_P: return "KeyP"_s;
    case VK_Q: return "KeyQ"_s;
    case VK_R: return "KeyR"_s;
    case VK_S: return "KeyS"_s;
    case VK_T: return "KeyT"_s;
    case VK_U: return "KeyU"_s;
    case VK_V: return "KeyV"_s;
    case VK_W: return "KeyW"_s;
    case VK_X: return "KeyX"_s;
    case VK_Y: return "KeyY"_s;
    case VK_Z: return "KeyZ"_s;
    case VK_OEM_MINUS: return "Minus"_s;
    case VK_OEM_PERIOD: return "Period"_s;
    case VK_OEM_7: return "Quote"_s;
    case VK_OEM_1: return "Semicolon"_s;
    case VK_OEM_2: return "Slash"_s;

    // Functional keys in alphanumeric section.
    case VK_MENU: return "AltLeft"_s;
    // AltRight.
    case VK_CAPITAL: return "CapsLock"_s;
    // ContextMenu.
    case VK_LCONTROL: return "ControlLeft"_s;
    case VK_RCONTROL: return "ControlRight"_s;
    case VK_RETURN: return "Enter"_s; //  Labeled Return on Apple keyboards.
    case VK_LWIN: return "MetaLeft"_s;
    case VK_RWIN: return "MetaRight"_s;
    case VK_LSHIFT: return "ShiftLeft"_s;
    case VK_RSHIFT: return "ShiftRight"_s;
    case VK_SPACE: return "Space"_s;
    case VK_TAB: return "Tab"_s;

    // Functional keys found on Japanese and Korean keyboards.
    // Convert.
    case VK_KANA: return "KanaMode"_s;
    // Lang1.
    // Lang2.
    // Lang3.
    // Lang4.
    // Lang5.
    // NonConvert.

    // Keys in the ControlPad section.
    // Delete
    case VK_END: return "End"_s;
    case VK_HELP: return "Help"_s;
    case VK_HOME: return "Home"_s;
    // Insert: Not present on Apple keyboards.
    case VK_NEXT: return "PageDown"_s;
    case VK_PRIOR: return "PageUp"_s;

    // Keys in the ArrowPad section.
    case VK_DOWN: return "ArrowDown"_s;
    case VK_LEFT: return "ArrowLeft"_s;
    case VK_RIGHT: return "ArrowRight"_s;
    case VK_UP: return "ArrowUp"_s;

    // Keys in the Numpad section.
    case VK_NUMLOCK: return "NumLock"_s;
    case VK_NUMPAD0: return "Numpad0"_s;
    case VK_NUMPAD1: return "Numpad1"_s;
    case VK_NUMPAD2: return "Numpad2"_s;
    case VK_NUMPAD3: return "Numpad3"_s;
    case VK_NUMPAD4: return "Numpad4"_s;
    case VK_NUMPAD5: return "Numpad5"_s;
    case VK_NUMPAD6: return "Numpad6"_s;
    case VK_NUMPAD7: return "Numpad7"_s;
    case VK_NUMPAD8: return "Numpad8"_s;
    case VK_NUMPAD9: return "Numpad9"_s;
    case VK_ADD: return "NumpadAdd"_s;
    // NumpadBackspace.
    // NumpadClear.
    // NumpadClearEntry.
    case VK_SEPARATOR: return "NumpadComma"_s;
    case VK_DECIMAL: return "NumpadDecimal"_s;
    case VK_DIVIDE: return "NumpadDivide"_s;
    // NumpadEnter.
    case VK_CLEAR: return "NumpadEqual"_s;
    // NumpadHash.
    // NumpadMemoryAdd.
    // NumpadMemoryClear.
    // NumpadMemoryRecall.
    // NumpadMemoryStore.
    // NumpadMemorySubtract.
    case VK_MULTIPLY: return "NumpadMultiply"_s;
    // NumpadParenLeft.
    // NumpadParenRight.
    // NumpadStar: The specification says to use "NumpadMultiply" for the * key on numeric keypads.
    case VK_SUBTRACT: return "NumpadSubtract"_s;

    // Keys in the Function section.
    case VK_ESCAPE: return "Escape"_s;
    case VK_F1: return "F1"_s;
    case VK_F2: return "F2"_s;
    case VK_F3: return "F3"_s;
    case VK_F4: return "F4"_s;
    case VK_F5: return "F5"_s;
    case VK_F6: return "F6"_s;
    case VK_F7: return "F7"_s;
    case VK_F8: return "F8"_s;
    case VK_F9: return "F9"_s;
    case VK_F10: return "F10"_s;
    case VK_F11: return "F11"_s;
    case VK_F12: return "F12"_s;
    case VK_F13: return "F13"_s;
    case VK_F14: return "F14"_s;
    case VK_F15: return "F15"_s;
    case VK_F16: return "F16"_s;
    case VK_F17: return "F17"_s;
    case VK_F18: return "F18"_s;
    case VK_F19: return "F19"_s;
    case VK_F20: return "F20"_s;
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
    case VK_VOLUME_DOWN: return "AudioVolumeDown"_s;
    case VK_VOLUME_MUTE: return "AudioVolumeMute"_s;
    case VK_VOLUME_UP: return "AudioVolumeUp"_s;
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
        return "Unidentified"_s;
    }
}

static bool isKeypadEvent(WebEvent* event)
{
    // Check that this is the type of event that has a keyCode.
    if (event.type != WebEventKeyDown && event.type != WebEventKeyUp)
        return false;

    // With the exception of keypad comma, the following corresponds to the criterion for UIKeyModifierNumericPad.
    // FIXME: Recognize keypad comma.
    switch (event.keyCode) {
    case VK_CLEAR: // Num Pad Clear
    case VK_OEM_PLUS: // Num Pad =
    case VK_DIVIDE:
    case VK_MULTIPLY:
    case VK_SUBTRACT:
    case VK_ADD:
    case VK_RETURN: // Num Pad Enter
    case VK_DECIMAL: // Num Pad .
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
        return true;
    }
    return false;
}

int windowsKeyCodeForKeyEvent(WebEvent* event)
{
    if (event.keyboardFlags & WebEventKeyboardInputModifierFlagsChanged)
        return event.keyCode;

    // There are several kinds of characters for which we produce key code from char code:
    // 1. Roman letters. Windows keyboard layouts affect both virtual key codes and character codes for these,
    //    so e.g. 'A' gets the same keyCode on QWERTY, AZERTY or Dvorak layouts.
    // 2. Keys for which there is no known iOS virtual key codes, like PrintScreen.
    // 3. Certain punctuation keys. On Windows, these are also remapped depending on current keyboard layout,
    //    but see comment in windowsKeyCodeForCharCode().
    if (!isKeypadEvent(event) && (event.type == WebEventKeyDown || event.type == WebEventKeyUp)) {
        // Cmd switches Roman letters for Dvorak-QWERTY layout, so try modified characters first.
        NSString *string = event.characters;
        int code = string.length > 0 ? windowsKeyCodeForCharCode([string characterAtIndex:0]) : 0;
        if (code)
            return code;

        // Ctrl+A on an AZERTY keyboard would get VK_Q keyCode if we relied on -[WebEvent keyCode] below.
        string = event.charactersIgnoringModifiers;
        code = string.length > 0 ? windowsKeyCodeForCharCode([string characterAtIndex:0]) : 0;
        if (code)
            return code;
    }

    // Use iOS virtual key code directly for any keys not handled above.
    // E.g. the key next to Caps Lock has the same Event.keyCode on U.S. keyboard ('A') and on
    // Russian keyboard (CYRILLIC LETTER EF).
    return event.keyCode;
}

class PlatformKeyboardEventBuilder : public PlatformKeyboardEvent {
public:
    PlatformKeyboardEventBuilder(WebEvent *event)
    {
        ASSERT(event.type == WebEventKeyDown || event.type == WebEventKeyUp);

        m_type = (event.type == WebEventKeyUp ? PlatformEvent::KeyUp : PlatformEvent::KeyDown);
        m_modifiers = modifiersForEvent(event);
        m_timestamp = WallTime::now();

        if (event.keyboardFlags & WebEventKeyboardInputModifierFlagsChanged) {
            m_text = emptyString();
            m_unmodifiedText = emptyString();
            m_autoRepeat = false;
        } else {
            m_text = event.characters;
            m_unmodifiedText = event.charactersIgnoringModifiers;
            m_autoRepeat = event.isKeyRepeating;
        }
        m_key = keyForKeyEvent(event);
        m_code = codeForKeyEvent(event);
        m_keyIdentifier = keyIdentifierForKeyEvent(event);
        m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(event);
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

#endif // PLATFORM(IOS_FAMILY)
