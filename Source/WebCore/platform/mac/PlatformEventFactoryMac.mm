/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PlatformEventFactoryMac.h"

#if PLATFORM(MAC)

#import "KeyEventCocoa.h"
#import "Logging.h"
#import "PlatformScreen.h"
#import "Scrollbar.h"
#import "WindowsKeyboardCodes.h"
#import <HIToolbox/CarbonEvents.h>
#import <HIToolbox/Events.h>
#import <mach/mach_time.h>
#import <pal/spi/mac/HIToolboxSPI.h>
#import <pal/spi/mac/NSEventSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/ASCIICType.h>
#import <wtf/WallTime.h>

namespace WebCore {

NSPoint globalPoint(const NSPoint& windowPoint, NSWindow *window)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return flipScreenPoint([window convertBaseToScreen:windowPoint], screen(window));
    ALLOW_DEPRECATED_DECLARATIONS_END
}

static NSPoint globalPointForEvent(NSEvent *event)
{
    switch ([event type]) {
#if defined(__LP64__)
    case NSEventTypePressure:
#endif
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
    case NSEventTypeMouseMoved:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeRightMouseUp:
    case NSEventTypeScrollWheel:
        return globalPoint([event locationInWindow], [event window]);
    default:
        return { 0, 0 };
    }
}

static IntPoint pointForEvent(NSEvent *event, NSView *windowView)
{
    switch ([event type]) {
#if defined(__LP64__)
    case NSEventTypePressure:
#endif
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
    case NSEventTypeMouseMoved:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeRightMouseUp:
    case NSEventTypeScrollWheel: {
        // Note: This will have its origin at the bottom left of the window unless windowView is flipped.
        // In those cases, the Y coordinate gets flipped by Widget::convertFromContainingWindow.
        NSPoint location = [event locationInWindow];
        if (windowView)
            location = [windowView convertPoint:location fromView:nil];
        return IntPoint(location);
    }
    default:
        return IntPoint();
    }
}

static MouseButton mouseButtonForEvent(NSEvent *event)
{
    switch ([event type]) {
#if defined(__LP64__)
    case NSEventTypePressure:
#endif
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
        return LeftButton;
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
        return RightButton;
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
        return MiddleButton;
    default:
        return NoButton;
    }
}

static unsigned short currentlyPressedMouseButtons()
{
    return static_cast<unsigned short>([NSEvent pressedMouseButtons]);
}

static PlatformEvent::Type mouseEventTypeForEvent(NSEvent* event)
{
    switch ([event type]) {
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
    case NSEventTypeMouseMoved:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeRightMouseDragged:
        return PlatformEvent::MouseMoved;
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
        return PlatformEvent::MousePressed;
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
        return PlatformEvent::MouseReleased;
    default:
        return PlatformEvent::MouseMoved;
    }
}

static int clickCountForEvent(NSEvent *event)
{
    switch ([event type]) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
        return [event clickCount];
    default:
        return 0;
    }
}

static PlatformWheelEventPhase momentumPhaseForEvent(NSEvent *event)
{
    uint32_t phase = PlatformWheelEventPhaseNone;

    if ([event momentumPhase] & NSEventPhaseBegan)
        phase |= PlatformWheelEventPhaseBegan;
    if ([event momentumPhase] & NSEventPhaseStationary)
        phase |= PlatformWheelEventPhaseStationary;
    if ([event momentumPhase] & NSEventPhaseChanged)
        phase |= PlatformWheelEventPhaseChanged;
    if ([event momentumPhase] & NSEventPhaseEnded)
        phase |= PlatformWheelEventPhaseEnded;
    if ([event momentumPhase] & NSEventPhaseCancelled)
        phase |= PlatformWheelEventPhaseCancelled;

    return static_cast<PlatformWheelEventPhase>(phase);
}

static PlatformWheelEventPhase phaseForEvent(NSEvent *event)
{
    uint32_t phase = PlatformWheelEventPhaseNone; 
    if ([event phase] & NSEventPhaseBegan)
        phase |= PlatformWheelEventPhaseBegan;
    if ([event phase] & NSEventPhaseStationary)
        phase |= PlatformWheelEventPhaseStationary;
    if ([event phase] & NSEventPhaseChanged)
        phase |= PlatformWheelEventPhaseChanged;
    if ([event phase] & NSEventPhaseEnded)
        phase |= PlatformWheelEventPhaseEnded;
    if ([event phase] & NSEventPhaseCancelled)
        phase |= PlatformWheelEventPhaseCancelled;
    if ([event momentumPhase] & NSEventPhaseMayBegin)
        phase |= PlatformWheelEventPhaseMayBegin;

    return static_cast<PlatformWheelEventPhase>(phase);
}

static inline String textFromEvent(NSEvent* event)
{
    if ([event type] == NSEventTypeFlagsChanged)
        return emptyString();
    return String([event characters]);
}

static inline String unmodifiedTextFromEvent(NSEvent* event)
{
    if ([event type] == NSEventTypeFlagsChanged)
        return emptyString();
    return String([event charactersIgnoringModifiers]);
}

String keyForKeyEvent(NSEvent *event)
{
    // This constant was missing before OS X Sierra.
#ifndef kVK_RightCommand
#define kVK_RightCommand 0x36
#endif
    switch ([event keyCode]) {
    case kVK_RightCommand:
    case kVK_Command:
        return "Meta"_s;
    case kVK_Shift:
    case kVK_RightShift:
        return "Shift"_s;
    case kVK_CapsLock:
        return "CapsLock"_s;
    case kVK_Option: // Left Alt.
    case kVK_RightOption: // Right Alt.
        return "Alt"_s;
    case kVK_Control:
    case kVK_RightControl:
        return "Control"_s;
    }

    // If the event is an NSEventTypeFlagsChanged events and we have not returned yet then this means we could not
    // identify the modifier key. We return now and report the key as "Unidentified".
    // Note that [event characters] below raises an exception if called on an NSEventTypeFlagsChanged event.
    if ([event type] == NSEventTypeFlagsChanged)
        return "Unidentified"_s;

    // If more than one key is being pressed and the key combination includes one or more modifier keys
    // that result in the key no longer producing a printable character (e.g., Control + a), then the
    // key value should be the printable key value that would have been produced if the key had been
    // typed with the default keyboard layout with no modifier keys except for Shift and AltGr applied.
    // https://w3c.github.io/uievents/#keys-guidelines
    bool isControlDown = ([event modifierFlags] & NSEventModifierFlagControl);
    NSString *s = isControlDown ? [event charactersIgnoringModifiers] : [event characters];
    auto length = [s length];
    // characters / charactersIgnoringModifiers return an empty string for dead keys.
    // https://developer.apple.com/reference/appkit/nsevent/1534183-characters
    if (!length)
        return "Dead"_s;
    // High unicode codepoints are coded with a character sequence in Mac OS X.
    if (length > 1)
        return s;
    return keyForCharCode([s characterAtIndex:0]);
}

// https://w3c.github.io/uievents-code/
String codeForKeyEvent(NSEvent *event)
{
    switch ([event keyCode]) {
    // Keys in the alphanumeric section.
    case kVK_ANSI_Grave: return "Backquote"_s;
    case kVK_ANSI_Backslash: return "Backslash"_s;
    case kVK_Delete: return "Backspace"_s;
    case kVK_ANSI_LeftBracket: return "BracketLeft"_s;
    case kVK_ANSI_RightBracket: return "BracketRight"_s;
    case kVK_ANSI_Comma: return "Comma"_s;
    case kVK_ANSI_0: return "Digit0"_s;
    case kVK_ANSI_1: return "Digit1"_s;
    case kVK_ANSI_2: return "Digit2"_s;
    case kVK_ANSI_3: return "Digit3"_s;
    case kVK_ANSI_4: return "Digit4"_s;
    case kVK_ANSI_5: return "Digit5"_s;
    case kVK_ANSI_6: return "Digit6"_s;
    case kVK_ANSI_7: return "Digit7"_s;
    case kVK_ANSI_8: return "Digit8"_s;
    case kVK_ANSI_9: return "Digit9"_s;
    case kVK_ANSI_Equal: return "Equal"_s;
    case kVK_ISO_Section: return "IntlBackslash"_s;
    case kVK_JIS_Underscore: return "IntlRo"_s;
    case kVK_JIS_Yen: return "IntlYen"_s;
    case kVK_ANSI_A: return "KeyA"_s;
    case kVK_ANSI_B: return "KeyB"_s;
    case kVK_ANSI_C: return "KeyC"_s;
    case kVK_ANSI_D: return "KeyD"_s;
    case kVK_ANSI_E: return "KeyE"_s;
    case kVK_ANSI_F: return "KeyF"_s;
    case kVK_ANSI_G: return "KeyG"_s;
    case kVK_ANSI_H: return "KeyH"_s;
    case kVK_ANSI_I: return "KeyI"_s;
    case kVK_ANSI_J: return "KeyJ"_s;
    case kVK_ANSI_K: return "KeyK"_s;
    case kVK_ANSI_L: return "KeyL"_s;
    case kVK_ANSI_M: return "KeyM"_s;
    case kVK_ANSI_N: return "KeyN"_s;
    case kVK_ANSI_O: return "KeyO"_s;
    case kVK_ANSI_P: return "KeyP"_s;
    case kVK_ANSI_Q: return "KeyQ"_s;
    case kVK_ANSI_R: return "KeyR"_s;
    case kVK_ANSI_S: return "KeyS"_s;
    case kVK_ANSI_T: return "KeyT"_s;
    case kVK_ANSI_U: return "KeyU"_s;
    case kVK_ANSI_V: return "KeyV"_s;
    case kVK_ANSI_W: return "KeyW"_s;
    case kVK_ANSI_X: return "KeyX"_s;
    case kVK_ANSI_Y: return "KeyY"_s;
    case kVK_ANSI_Z: return "KeyZ"_s;
    case kVK_ANSI_Minus: return "Minus"_s;
    case kVK_ANSI_Period: return "Period"_s;
    case kVK_ANSI_Quote: return "Quote"_s;
    case kVK_ANSI_Semicolon: return "Semicolon"_s;
    case kVK_ANSI_Slash: return "Slash"_s;

    // Functional keys in alphanumeric section.
    case kVK_Option: return "AltLeft"_s;
    case kVK_RightOption: return "AltRight"_s;
    case kVK_CapsLock: return "CapsLock"_s;
    // ContextMenu.
    case kVK_Control: return "ControlLeft"_s;
    case kVK_RightControl: return "ControlRight"_s;
    case kVK_Return: return "Enter"_s; //  Labeled Return on Apple keyboards.
    case kVK_Command: return "MetaLeft"_s;
    case kVK_RightCommand: return "MetaRight"_s;
    case kVK_Shift: return "ShiftLeft"_s;
    case kVK_RightShift: return "ShiftRight"_s;
    case kVK_Space: return "Space"_s;
    case kVK_Tab: return "Tab"_s;

    // Functional keys found on Japanese and Korean keyboards.
    // Convert.
    case kVK_JIS_Kana: return "KanaMode"_s;
    // Lang1.
    case kVK_JIS_Eisu: return "Lang2"_s; // Japanese (Mac keyboard): eisu.
    // Lang3.
    // Lang4.
    // Lang5.
    // NonConvert.

    // Keys in the ControlPad section.
    case kVK_ForwardDelete: return "Delete"_s;
    case kVK_End: return "End"_s;
    case kVK_Help: return "Help"_s;
    case kVK_Home: return "Home"_s;
    // Insert: Not present on Apple keyboards.
    case kVK_PageDown: return "PageDown"_s;
    case kVK_PageUp: return "PageUp"_s;

    // Keys in the ArrowPad section.
    case kVK_DownArrow: return "ArrowDown"_s;
    case kVK_LeftArrow: return "ArrowLeft"_s;
    case kVK_RightArrow: return "ArrowRight"_s;
    case kVK_UpArrow: return "ArrowUp"_s;

    // Keys in the Numpad section.
    case kVK_ANSI_KeypadClear: return "NumLock"_s; // The specification says to use "NumLock" on Mac for the numpad Clear key.
    case kVK_ANSI_Keypad0: return "Numpad0"_s;
    case kVK_ANSI_Keypad1: return "Numpad1"_s;
    case kVK_ANSI_Keypad2: return "Numpad2"_s;
    case kVK_ANSI_Keypad3: return "Numpad3"_s;
    case kVK_ANSI_Keypad4: return "Numpad4"_s;
    case kVK_ANSI_Keypad5: return "Numpad5"_s;
    case kVK_ANSI_Keypad6: return "Numpad6"_s;
    case kVK_ANSI_Keypad7: return "Numpad7"_s;
    case kVK_ANSI_Keypad8: return "Numpad8"_s;
    case kVK_ANSI_Keypad9: return "Numpad9"_s;
    case kVK_ANSI_KeypadPlus: return "NumpadAdd"_s;
    // NumpadBackspace.
    // NumpadClear: The specification says that the numpad Clear key should always be encoded as "NumLock" on Mac.
    // NumpadClearEntry.
    case kVK_JIS_KeypadComma: return "NumpadComma"_s;
    case kVK_ANSI_KeypadDecimal: return "NumpadDecimal"_s;
    case kVK_ANSI_KeypadDivide: return "NumpadDivide"_s;
    case kVK_ANSI_KeypadEnter: return "NumpadEnter"_s;
    case kVK_ANSI_KeypadEquals: return "NumpadEqual"_s;
    // NumpadHash.
    // NumpadMemoryAdd.
    // NumpadMemoryClear.
    // NumpadMemoryRecall.
    // NumpadMemoryStore.
    // NumpadMemorySubtract.
    case kVK_ANSI_KeypadMultiply: return "NumpadMultiply"_s;
    // NumpadParenLeft.
    // NumpadParenRight.
    // NumpadStar: The specification says to use "NumpadMultiply" for the * key on numeric keypads.
    case kVK_ANSI_KeypadMinus: return "NumpadSubtract"_s;

    // Keys in the Function section.
    case kVK_Escape: return "Escape"_s;
    case kVK_F1: return "F1"_s;
    case kVK_F2: return "F2"_s;
    case kVK_F3: return "F3"_s;
    case kVK_F4: return "F4"_s;
    case kVK_F5: return "F5"_s;
    case kVK_F6: return "F6"_s;
    case kVK_F7: return "F7"_s;
    case kVK_F8: return "F8"_s;
    case kVK_F9: return "F9"_s;
    case kVK_F10: return "F10"_s;
    case kVK_F11: return "F11"_s;
    case kVK_F12: return "F12"_s;
    case kVK_F13: return "F13"_s;
    case kVK_F14: return "F14"_s;
    case kVK_F15: return "F15"_s;
    case kVK_F16: return "F16"_s;
    case kVK_F17: return "F17"_s;
    case kVK_F18: return "F18"_s;
    case kVK_F19: return "F19"_s;
    case kVK_F20: return "F20"_s;
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
    case kVK_VolumeDown: return "AudioVolumeDown"_s;
    case kVK_Mute: return "AudioVolumeMute"_s;
    case kVK_VolumeUp: return "AudioVolumeUp"_s;
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

String keyIdentifierForKeyEvent(NSEvent* event)
{
    if ([event type] == NSEventTypeFlagsChanged) {
        switch ([event keyCode]) {
        case 54: // Right Command
        case 55: // Left Command
            return "Meta"_str;

        case 57: // Capslock
            return "CapsLock"_str;

        case 56: // Left Shift
        case 60: // Right Shift
            return "Shift"_str;

        case 58: // Left Alt
        case 61: // Right Alt
            return "Alt"_str;

        case 59: // Left Ctrl
        case 62: // Right Ctrl
            return "Control"_str;

        default:
            ASSERT_NOT_REACHED();
            return emptyString();
        }
    }
    
    NSString *s = [event charactersIgnoringModifiers];
    if ([s length] != 1) {
        LOG(Events, "received an unexpected number of characters in key event: %u", [s length]);
        return "Unidentified";
    }
    return keyIdentifierForCharCode([s characterAtIndex:0]);
}

static bool isKeypadEvent(NSEvent* event)
{
    // Check that this is the type of event that has a keyCode.
    switch ([event type]) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    case NSEventTypeFlagsChanged:
        break;
    default:
        return false;
    }

    if ([event modifierFlags] & NSEventModifierFlagNumericPad)
        return true;

    switch ([event keyCode]) {
    case 71: // Clear
    case 81: // =
    case 75: // /
    case 67: // *
    case 78: // -
    case 69: // +
    case 76: // Enter
    case 65: // .
    case 82: // 0
    case 83: // 1
    case 84: // 2
    case 85: // 3
    case 86: // 4
    case 87: // 5
    case 88: // 6
    case 89: // 7
    case 91: // 8
    case 92: // 9
        return true;
    }
     
     return false;
}

int windowsKeyCodeForKeyEvent(NSEvent* event)
{
    int code = 0;
    // There are several kinds of characters for which we produce key code from char code:
    // 1. Roman letters. Windows keyboard layouts affect both virtual key codes and character codes for these,
    //    so e.g. 'A' gets the same keyCode on QWERTY, AZERTY or Dvorak layouts.
    // 2. Keys for which there is no known Mac virtual key codes, like PrintScreen.
    // 3. Certain punctuation keys. On Windows, these are also remapped depending on current keyboard layout,
    //    but see comment in windowsKeyCodeForCharCode().
    if (!isKeypadEvent(event) && ([event type] == NSEventTypeKeyDown || [event type] == NSEventTypeKeyUp)) {
        // Cmd switches Roman letters for Dvorak-QWERTY layout, so try modified characters first.
        NSString* s = [event characters];
        code = [s length] > 0 ? windowsKeyCodeForCharCode([s characterAtIndex:0]) : 0;
        if (code)
            return code;

        // Ctrl+A on an AZERTY keyboard would get VK_Q keyCode if we relied on -[NSEvent keyCode] below.
        s = [event charactersIgnoringModifiers];
        code = [s length] > 0 ? windowsKeyCodeForCharCode([s characterAtIndex:0]) : 0;
        if (code)
            return code;
    }

    // Map Mac virtual key code directly to Windows one for any keys not handled above.
    // E.g. the key next to Caps Lock has the same Event.keyCode on U.S. keyboard ('A') and on Russian keyboard (CYRILLIC LETTER EF).
    return windowsKeyCodeForKeyCode([event keyCode]);
}

static CFAbsoluteTime systemStartupTime;

static void updateSystemStartupTimeIntervalSince1970()
{
    // CFAbsoluteTimeGetCurrent() provides the absolute time in seconds since 2001.
    // mach_absolute_time() provides a relative system time since startup minus the time the computer was suspended.
    mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    double elapsedTimeSinceStartup = static_cast<double>(mach_absolute_time()) * timebase_info.numer / timebase_info.denom / 1e9;
    systemStartupTime = kCFAbsoluteTimeIntervalSince1970 + CFAbsoluteTimeGetCurrent() - elapsedTimeSinceStartup;
}

static CFTimeInterval cachedStartupTimeIntervalSince1970()
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        void (^updateBlock)(NSNotification *) = Block_copy(^(NSNotification *){ updateSystemStartupTimeIntervalSince1970(); });
        [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidWakeNotification
                                                                        object:nil
                                                                         queue:nil
                                                                    usingBlock:updateBlock];
        [[NSNotificationCenter defaultCenter] addObserverForName:NSSystemClockDidChangeNotification
                                                          object:nil
                                                           queue:nil
                                                      usingBlock:updateBlock];
        Block_release(updateBlock);

        updateSystemStartupTimeIntervalSince1970();
    });
    return systemStartupTime;
}

WallTime eventTimeStampSince1970(NSEvent* event)
{
    return WallTime::fromRawSeconds(static_cast<double>(cachedStartupTimeIntervalSince1970() + [event timestamp]));
}

static inline bool isKeyUpEvent(NSEvent *event)
{
    if ([event type] != NSEventTypeFlagsChanged)
        return [event type] == NSEventTypeKeyUp;
    // FIXME: This logic fails if the user presses both Shift keys at once, for example:
    // we treat releasing one of them as keyDown.
    switch ([event keyCode]) {
    case 54: // Right Command
    case 55: // Left Command
        return !([event modifierFlags] & NSEventModifierFlagCommand);

    case 57: // Capslock
        return !([event modifierFlags] & NSEventModifierFlagCapsLock);

    case 56: // Left Shift
    case 60: // Right Shift
        return !([event modifierFlags] & NSEventModifierFlagShift);

    case 58: // Left Alt
    case 61: // Right Alt
        return !([event modifierFlags] & NSEventModifierFlagOption);

    case 59: // Left Ctrl
    case 62: // Right Ctrl
        return !([event modifierFlags] & NSEventModifierFlagControl);

    case 63: // Function
        return !([event modifierFlags] & NSEventModifierFlagFunction);
    }
    return false;
}

OptionSet<PlatformEvent::Modifier> modifiersForEvent(NSEvent *event)
{
    OptionSet<PlatformEvent::Modifier> modifiers;

    if (event.modifierFlags & NSEventModifierFlagShift)
        modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (event.modifierFlags & NSEventModifierFlagControl)
        modifiers.add(PlatformEvent::Modifier::CtrlKey);
    if (event.modifierFlags & NSEventModifierFlagOption)
        modifiers.add(PlatformEvent::Modifier::AltKey);
    if (event.modifierFlags & NSEventModifierFlagCommand)
        modifiers.add(PlatformEvent::Modifier::MetaKey);
    if (event.modifierFlags & NSEventModifierFlagCapsLock)
        modifiers.add(PlatformEvent::Modifier::CapsLockKey);

    return modifiers;
}

static int typeForEvent(NSEvent *event)
{
    return static_cast<int>([NSMenu menuTypeForEvent:event]);
}

void getWheelEventDeltas(NSEvent *event, float& deltaX, float& deltaY, BOOL& continuous)
{
    ASSERT(event);
    if (event.hasPreciseScrollingDeltas) {
        deltaX = event.scrollingDeltaX;
        deltaY = event.scrollingDeltaY;
        continuous = YES;
    } else {
        deltaX = event.deltaX;
        deltaY = event.deltaY;
        continuous = NO;
    }
}

UInt8 keyCharForEvent(NSEvent *event)
{
    EventRef eventRef = (EventRef)[event _eventRef];
    if (!eventRef)
        return 0;

    ByteCount keyCharCount = 0;
    if (GetEventParameter(eventRef, kEventParamKeyMacCharCodes, typeChar, 0, 0, &keyCharCount, 0) != noErr)
        return 0;
    if (keyCharCount != 1)
        return 0;

    UInt8 keyChar = 0;
    if (GetEventParameter(eventRef, kEventParamKeyMacCharCodes, typeChar, 0, sizeof(keyChar), &keyCharCount, &keyChar) != noErr)
        return 0;
    
    return keyChar;
}

class PlatformMouseEventBuilder : public PlatformMouseEvent {
public:
    PlatformMouseEventBuilder(NSEvent *event, NSEvent *correspondingPressureEvent, NSView *windowView)
    {
        // PlatformEvent
        m_type = mouseEventTypeForEvent(event);

#if defined(__LP64__)
        BOOL eventIsPressureEvent = [event type] == NSEventTypePressure;
        if (eventIsPressureEvent) {
            // Since AppKit doesn't send mouse events for force down or force up, we have to use the current pressure
            // event and correspondingPressureEvent to detect if this is MouseForceDown, MouseForceUp, or just MouseForceChanged.
            if (correspondingPressureEvent.stage == 1 && event.stage == 2)
                m_type = PlatformEvent::MouseForceDown;
            else if (correspondingPressureEvent.stage == 2 && event.stage == 1)
                m_type = PlatformEvent::MouseForceUp;
            else
                m_type = PlatformEvent::MouseForceChanged;
        }
#else
        UNUSED_PARAM(correspondingPressureEvent);
#endif

        m_modifiers = modifiersForEvent(event);
        m_timestamp = eventTimeStampSince1970(event);

        // PlatformMouseEvent
        m_position = pointForEvent(event, windowView);
        m_globalPosition = IntPoint(globalPointForEvent(event));
        m_button = mouseButtonForEvent(event);
        m_buttons = currentlyPressedMouseButtons();
        m_clickCount = clickCountForEvent(event);
#if ENABLE(POINTER_LOCK)
        m_movementDelta = IntPoint(event.deltaX, event.deltaY);
#endif

        m_force = 0;
#if defined(__LP64__)
        int stage = eventIsPressureEvent ? event.stage : correspondingPressureEvent.stage;
        double pressure = eventIsPressureEvent ? event.pressure : correspondingPressureEvent.pressure;
        m_force = pressure + stage;
#endif

        // Mac specific
        m_modifierFlags = [event modifierFlags];
        m_eventNumber = [event eventNumber];
        m_menuTypeForEvent = typeForEvent(event);
    }
};

PlatformMouseEvent PlatformEventFactory::createPlatformMouseEvent(NSEvent *event, NSEvent *correspondingPressureEvent, NSView *windowView)
{
    return PlatformMouseEventBuilder(event, correspondingPressureEvent, windowView);
}


class PlatformWheelEventBuilder : public PlatformWheelEvent {
public:
    PlatformWheelEventBuilder(NSEvent *event, NSView *windowView)
    {
        // PlatformEvent
        m_type = PlatformEvent::Wheel;
        m_modifiers = modifiersForEvent(event);
        m_timestamp = eventTimeStampSince1970(event);

        // PlatformWheelEvent
        m_position = pointForEvent(event, windowView);
        m_globalPosition = IntPoint(globalPointForEvent(event));
        m_granularity = ScrollByPixelWheelEvent;

        BOOL continuous;
        getWheelEventDeltas(event, m_deltaX, m_deltaY, continuous);
        if (continuous) {
            m_wheelTicksX = m_deltaX / static_cast<float>(Scrollbar::pixelsPerLineStep());
            m_wheelTicksY = m_deltaY / static_cast<float>(Scrollbar::pixelsPerLineStep());
        } else {
            m_wheelTicksX = m_deltaX;
            m_wheelTicksY = m_deltaY;
            m_deltaX *= static_cast<float>(Scrollbar::pixelsPerLineStep());
            m_deltaY *= static_cast<float>(Scrollbar::pixelsPerLineStep());
        }

        m_phase = phaseForEvent(event);
        m_momentumPhase = momentumPhaseForEvent(event);
        m_hasPreciseScrollingDeltas = continuous;
        m_directionInvertedFromDevice = [event isDirectionInvertedFromDevice];
    }
};

PlatformWheelEvent PlatformEventFactory::createPlatformWheelEvent(NSEvent *event, NSView *windowView)
{
    return PlatformWheelEventBuilder(event, windowView);
}


class PlatformKeyboardEventBuilder : public PlatformKeyboardEvent {
public:
    PlatformKeyboardEventBuilder(NSEvent *event)
    {
        // PlatformEvent
        m_type = isKeyUpEvent(event) ? PlatformEvent::KeyUp : PlatformEvent::KeyDown;
        m_modifiers = modifiersForEvent(event);
        m_timestamp = eventTimeStampSince1970(event);

        // PlatformKeyboardEvent
        m_text = textFromEvent(event);
        m_unmodifiedText = unmodifiedTextFromEvent(event);
        m_keyIdentifier = keyIdentifierForKeyEvent(event);
        m_key = keyForKeyEvent(event);
        m_code = codeForKeyEvent(event);
        m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(event);
        m_autoRepeat = [event type] != NSEventTypeFlagsChanged && [event isARepeat];
        m_isKeypad = isKeypadEvent(event);
        m_isSystemKey = false; // SystemKey is always false on the Mac.

        // Always use 13 for Enter/Return -- we don't want to use AppKit's different character for Enter.
        if (m_windowsVirtualKeyCode == VK_RETURN) {
            m_text = "\r";
            m_unmodifiedText = "\r";
        }

        // AppKit sets text to "\x7F" for backspace, but the correct KeyboardEvent character code is 8.
        if (m_windowsVirtualKeyCode == VK_BACK) {
            m_text = "\x8";
            m_unmodifiedText = "\x8";
        }

        // Always use 9 for Tab -- we don't want to use AppKit's different character for shift-tab.
        if (m_windowsVirtualKeyCode == VK_TAB) {
            m_text = "\x9";
            m_unmodifiedText = "\x9";
        }

        // Mac specific.
        m_macEvent = event;
    }
};

PlatformKeyboardEvent PlatformEventFactory::createPlatformKeyboardEvent(NSEvent *event)
{
    return PlatformKeyboardEventBuilder(event);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
