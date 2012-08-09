/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006-2009 Google Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebInputEventFactory.h"

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#import "KeyEventCocoa.h"
#include "WebInputEvent.h"
#include <wtf/ASCIICType.h>

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 1070

// Additional Lion APIs.
enum {
    NSEventPhaseNone        = 0,
    NSEventPhaseBegan       = 0x1 << 0,
    NSEventPhaseStationary  = 0x1 << 1,
    NSEventPhaseChanged     = 0x1 << 2,
    NSEventPhaseEnded       = 0x1 << 3,
    NSEventPhaseCancelled   = 0x1 << 4
};
typedef NSUInteger NSEventPhase;

@interface NSEvent (LionSDKDeclarations)
- (NSEventPhase)phase;
- (NSEventPhase)momentumPhase;
@end

#endif  // __MAC_OS_X_VERSION_MAX_ALLOWED < 1070

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 1080

// Additional Mountain Lion APIs.
enum {
    NSEventPhaseMayBegin    = 0x1 << 5
};

#endif  // __MAC_OS_X_VERSION_MAX_ALLOWED < 1080

// Do not __MAC_OS_X_VERSION_MAX_ALLOWED here because of a bug in the 10.5 SDK,
// see <http://lists.webkit.org/pipermail/webkit-dev/2012-July/021442.html>.
#if MAC_OS_X_VERSION_MAX_ALLOWED <= 1050

// These are not defined in the 10.5 SDK but are defined in later SDKs inside
// a MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5 #ifdef.
enum {
    NSEventTypeBeginGesture     = 19,
    NSEventTypeEndGesture       = 20
};

#endif  // MAC_OS_X_VERSION_MAX_ALLOWED <= 1050

namespace WebKit {

// WebKeyboardEvent -----------------------------------------------------------

// ----------------------------------------------------------------------------
// Begin Apple code, copied from KeyEventMac.mm
//
// We can share some of this code if we factored it out of KeyEventMac, but
// the main problem is that it relies on the NSString ctor on String for
// conversions, and since we're building without PLATFORM(MAC), we don't have
// that. As a result we have to use NSString here exclusively and thus tweak
// the code so it's not re-usable as-is. One possiblity would be to make the
// upstream code only use NSString, but I'm not certain how far that change
// would propagate.

static inline bool isKeyUpEvent(NSEvent* event)
{
    if ([event type] != NSFlagsChanged)
        return [event type] == NSKeyUp;
    // FIXME: This logic fails if the user presses both Shift keys at once, for example:
    // we treat releasing one of them as keyDown.
    switch ([event keyCode]) {
    case 54: // Right Command
    case 55: // Left Command
        return ([event modifierFlags] & NSCommandKeyMask) == 0;

    case 57: // Capslock
        return ([event modifierFlags] & NSAlphaShiftKeyMask) == 0;

    case 56: // Left Shift
    case 60: // Right Shift
        return ([event modifierFlags] & NSShiftKeyMask) == 0;

    case 58: // Left Alt
    case 61: // Right Alt
        return ([event modifierFlags] & NSAlternateKeyMask) == 0;

    case 59: // Left Ctrl
    case 62: // Right Ctrl
        return ([event modifierFlags] & NSControlKeyMask) == 0;

    case 63: // Function
        return ([event modifierFlags] & NSFunctionKeyMask) == 0;
    }
    return false;
}

static bool isKeypadEvent(NSEvent* event)
{
    // Check that this is the type of event that has a keyCode.
    switch ([event type]) {
    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
        break;
    default:
        return false;
    }

    if ([event modifierFlags] & NSNumericPadKeyMask)
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

static int windowsKeyCodeForKeyEvent(NSEvent* event)
{
    int code = 0;
    // There are several kinds of characters for which we produce key code from char code:
    // 1. Roman letters. Windows keyboard layouts affect both virtual key codes and character codes for these,
    //    so e.g. 'A' gets the same keyCode on QWERTY, AZERTY or Dvorak layouts.
    // 2. Keys for which there is no known Mac virtual key codes, like PrintScreen.
    // 3. Certain punctuation keys. On Windows, these are also remapped depending on current keyboard layout,
    //    but see comment in windowsKeyCodeForCharCode().
    if ([event type] == NSKeyDown || [event type] == NSKeyUp) {
        // Cmd switches Roman letters for Dvorak-QWERTY layout, so try modified characters first.
        NSString* s = [event characters];
        code = [s length] > 0 ? WebCore::windowsKeyCodeForCharCode([s characterAtIndex:0]) : 0;
        if (code)
            return code;

        // Ctrl+A on an AZERTY keyboard would get VK_Q keyCode if we relied on -[NSEvent keyCode] below.
        s = [event charactersIgnoringModifiers];
        code = [s length] > 0 ? WebCore::windowsKeyCodeForCharCode([s characterAtIndex:0]) : 0;
        if (code)
            return code;
    }

    // Map Mac virtual key code directly to Windows one for any keys not handled above.
    // E.g. the key next to Caps Lock has the same Event.keyCode on U.S. keyboard ('A') and on Russian keyboard (CYRILLIC LETTER EF).
    return WebCore::windowsKeyCodeForKeyCode([event keyCode]);
}

static WebInputEvent::Type gestureEventTypeForEvent(NSEvent *event)
{
    switch ([event type]) {
    case NSEventTypeBeginGesture:
        return WebInputEvent::GestureScrollBegin;
    case NSEventTypeEndGesture:
        return WebInputEvent::GestureScrollEnd;
    default:
        ASSERT_NOT_REACHED();
        return WebInputEvent::GestureScrollEnd;
    }
}

static inline NSString* textFromEvent(NSEvent* event)
{
    if ([event type] == NSFlagsChanged)
        return @"";
    return [event characters];
}

static inline NSString* unmodifiedTextFromEvent(NSEvent* event)
{
    if ([event type] == NSFlagsChanged)
        return @"";
    return [event charactersIgnoringModifiers];
}

static NSString* keyIdentifierForKeyEvent(NSEvent* event)
{
    if ([event type] == NSFlagsChanged) {
        switch ([event keyCode]) {
        case 54: // Right Command
        case 55: // Left Command
            return @"Meta";

        case 57: // Capslock
            return @"CapsLock";

        case 56: // Left Shift
        case 60: // Right Shift
            return @"Shift";

        case 58: // Left Alt
        case 61: // Right Alt
            return @"Alt";

        case 59: // Left Ctrl
        case 62: // Right Ctrl
            return @"Control";

// Begin non-Apple addition/modification --------------------------------------
        case 63: // Function
            return @"Function";

        default: // Unknown, but this may be a strange/new keyboard.
            return @"Unidentified";
// End non-Apple addition/modification ----------------------------------------
        }
    }

    NSString* s = [event charactersIgnoringModifiers];
    if ([s length] != 1)
        return @"Unidentified";

    unichar c = [s characterAtIndex:0];
    switch (c) {
    // Each identifier listed in the DOM spec is listed here.
    // Many are simply commented out since they do not appear on standard Macintosh keyboards
    // or are on a key that doesn't have a corresponding character.

    // "Accept"
    // "AllCandidates"

    // "Alt"
    case NSMenuFunctionKey:
        return @"Alt";

    // "Apps"
    // "BrowserBack"
    // "BrowserForward"
    // "BrowserHome"
    // "BrowserRefresh"
    // "BrowserSearch"
    // "BrowserStop"
    // "CapsLock"

    // "Clear"
    case NSClearLineFunctionKey:
        return @"Clear";

    // "CodeInput"
    // "Compose"
    // "Control"
    // "Crsel"
    // "Convert"
    // "Copy"
    // "Cut"

    // "Down"
    case NSDownArrowFunctionKey:
        return @"Down";
    // "End"
    case NSEndFunctionKey:
        return @"End";
    // "Enter"
    case 0x3: case 0xA: case 0xD: // Macintosh calls the one on the main keyboard Return, but Windows calls it Enter, so we'll do the same for the DOM
        return @"Enter";

    // "EraseEof"

    // "Execute"
    case NSExecuteFunctionKey:
        return @"Execute";

    // "Exsel"

    // "F1"
    case NSF1FunctionKey:
        return @"F1";
    // "F2"
    case NSF2FunctionKey:
        return @"F2";
    // "F3"
    case NSF3FunctionKey:
        return @"F3";
    // "F4"
    case NSF4FunctionKey:
        return @"F4";
    // "F5"
    case NSF5FunctionKey:
        return @"F5";
    // "F6"
    case NSF6FunctionKey:
        return @"F6";
    // "F7"
    case NSF7FunctionKey:
        return @"F7";
    // "F8"
    case NSF8FunctionKey:
        return @"F8";
    // "F9"
    case NSF9FunctionKey:
        return @"F9";
    // "F10"
    case NSF10FunctionKey:
        return @"F10";
    // "F11"
    case NSF11FunctionKey:
        return @"F11";
    // "F12"
    case NSF12FunctionKey:
        return @"F12";
    // "F13"
    case NSF13FunctionKey:
        return @"F13";
    // "F14"
    case NSF14FunctionKey:
        return @"F14";
    // "F15"
    case NSF15FunctionKey:
        return @"F15";
    // "F16"
    case NSF16FunctionKey:
        return @"F16";
    // "F17"
    case NSF17FunctionKey:
        return @"F17";
    // "F18"
    case NSF18FunctionKey:
        return @"F18";
    // "F19"
    case NSF19FunctionKey:
        return @"F19";
    // "F20"
    case NSF20FunctionKey:
        return @"F20";
    // "F21"
    case NSF21FunctionKey:
        return @"F21";
    // "F22"
    case NSF22FunctionKey:
        return @"F22";
    // "F23"
    case NSF23FunctionKey:
        return @"F23";
    // "F24"
    case NSF24FunctionKey:
        return @"F24";

    // "FinalMode"

    // "Find"
    case NSFindFunctionKey:
        return @"Find";

    // "FullWidth"
    // "HalfWidth"
    // "HangulMode"
    // "HanjaMode"

    // "Help"
    case NSHelpFunctionKey:
        return @"Help";

    // "Hiragana"

    // "Home"
    case NSHomeFunctionKey:
        return @"Home";
    // "Insert"
    case NSInsertFunctionKey:
        return @"Insert";

    // "JapaneseHiragana"
    // "JapaneseKatakana"
    // "JapaneseRomaji"
    // "JunjaMode"
    // "KanaMode"
    // "KanjiMode"
    // "Katakana"
    // "LaunchApplication1"
    // "LaunchApplication2"
    // "LaunchMail"

    // "Left"
    case NSLeftArrowFunctionKey:
        return @"Left";

    // "Meta"
    // "MediaNextTrack"
    // "MediaPlayPause"
    // "MediaPreviousTrack"
    // "MediaStop"

    // "ModeChange"
    case NSModeSwitchFunctionKey:
        return @"ModeChange";

    // "Nonconvert"
    // "NumLock"

    // "PageDown"
    case NSPageDownFunctionKey:
        return @"PageDown";
    // "PageUp"
    case NSPageUpFunctionKey:
        return @"PageUp";

    // "Paste"

    // "Pause"
    case NSPauseFunctionKey:
        return @"Pause";

    // "Play"
    // "PreviousCandidate"

    // "PrintScreen"
    case NSPrintScreenFunctionKey:
        return @"PrintScreen";

    // "Process"
    // "Props"

    // "Right"
    case NSRightArrowFunctionKey:
        return @"Right";

    // "RomanCharacters"

    // "Scroll"
    case NSScrollLockFunctionKey:
        return @"Scroll";
    // "Select"
    case NSSelectFunctionKey:
        return @"Select";

    // "SelectMedia"
    // "Shift"

    // "Stop"
    case NSStopFunctionKey:
        return @"Stop";
    // "Up"
    case NSUpArrowFunctionKey:
        return @"Up";
    // "Undo"
    case NSUndoFunctionKey:
        return @"Undo";

    // "VolumeDown"
    // "VolumeMute"
    // "VolumeUp"
    // "Win"
    // "Zoom"

    // More function keys, not in the key identifier specification.
    case NSF25FunctionKey:
        return @"F25";
    case NSF26FunctionKey:
        return @"F26";
    case NSF27FunctionKey:
        return @"F27";
    case NSF28FunctionKey:
        return @"F28";
    case NSF29FunctionKey:
        return @"F29";
    case NSF30FunctionKey:
        return @"F30";
    case NSF31FunctionKey:
        return @"F31";
    case NSF32FunctionKey:
        return @"F32";
    case NSF33FunctionKey:
        return @"F33";
    case NSF34FunctionKey:
        return @"F34";
    case NSF35FunctionKey:
        return @"F35";

    // Turn 0x7F into 0x08, because backspace needs to always be 0x08.
    case 0x7F:
        return @"U+0008";
    // Standard says that DEL becomes U+007F.
    case NSDeleteFunctionKey:
        return @"U+007F";

    // Always use 0x09 for tab instead of AppKit's backtab character.
    case NSBackTabCharacter:
        return @"U+0009";

    case NSBeginFunctionKey:
    case NSBreakFunctionKey:
    case NSClearDisplayFunctionKey:
    case NSDeleteCharFunctionKey:
    case NSDeleteLineFunctionKey:
    case NSInsertCharFunctionKey:
    case NSInsertLineFunctionKey:
    case NSNextFunctionKey:
    case NSPrevFunctionKey:
    case NSPrintFunctionKey:
    case NSRedoFunctionKey:
    case NSResetFunctionKey:
    case NSSysReqFunctionKey:
    case NSSystemFunctionKey:
    case NSUserFunctionKey:
        // FIXME: We should use something other than the vendor-area Unicode values for the above keys.
        // For now, just fall through to the default.
    default:
        return [NSString stringWithFormat:@"U+%04X", WTF::toASCIIUpper(c)];
    }
}

// End Apple code.
// ----------------------------------------------------------------------------

static inline int modifiersFromEvent(NSEvent* event) {
    int modifiers = 0;

    if ([event modifierFlags] & NSControlKeyMask)
        modifiers |= WebInputEvent::ControlKey;
    if ([event modifierFlags] & NSShiftKeyMask)
        modifiers |= WebInputEvent::ShiftKey;
    if ([event modifierFlags] & NSAlternateKeyMask)
        modifiers |= WebInputEvent::AltKey;
    if ([event modifierFlags] & NSCommandKeyMask)
        modifiers |= WebInputEvent::MetaKey;
    if ([event modifierFlags] & NSAlphaShiftKeyMask)
        modifiers |= WebInputEvent::CapsLockOn;
    // TODO(port): Set mouse button states

    return modifiers;
}

static inline void setWebEventLocationFromEventInView(WebMouseEvent* result,
                                                      NSEvent* event,
                                                      NSView* view) {
    NSPoint windowLocal = [event locationInWindow];

    NSPoint screenLocal = [[view window] convertBaseToScreen:windowLocal];
    result->globalX = screenLocal.x;
    // Flip y.
    NSScreen* primaryScreen = ([[NSScreen screens] count] > 0) ?
        [[NSScreen screens] objectAtIndex:0] : nil;
    if (primaryScreen)
        result->globalY = [primaryScreen frame].size.height - screenLocal.y;
    else
        result->globalY = screenLocal.y;

    NSPoint contentLocal = [view convertPoint:windowLocal fromView:nil];
    result->x = contentLocal.x;
    result->y = [view frame].size.height - contentLocal.y;  // Flip y.

    result->windowX = result->x;
    result->windowY = result->y;

    result->movementX = [event deltaX];
    result->movementY = [event deltaY];
}

WebKeyboardEvent WebInputEventFactory::keyboardEvent(NSEvent* event)
{
    WebKeyboardEvent result;

    result.type =
        isKeyUpEvent(event) ? WebInputEvent::KeyUp : WebInputEvent::RawKeyDown;

    result.modifiers = modifiersFromEvent(event);

    if (isKeypadEvent(event))
        result.modifiers |= WebInputEvent::IsKeyPad;

    if (([event type] != NSFlagsChanged) && [event isARepeat])
        result.modifiers |= WebInputEvent::IsAutoRepeat;

    result.windowsKeyCode = windowsKeyCodeForKeyEvent(event);
    result.nativeKeyCode = [event keyCode];

    NSString* textStr = textFromEvent(event);
    NSString* unmodifiedStr = unmodifiedTextFromEvent(event);
    NSString* identifierStr = keyIdentifierForKeyEvent(event);

    // Begin Apple code, copied from KeyEventMac.mm

    // Always use 13 for Enter/Return -- we don't want to use AppKit's
    // different character for Enter.
    if (result.windowsKeyCode == '\r') {
        textStr = @"\r";
        unmodifiedStr = @"\r";
    }

    // The adjustments below are only needed in backward compatibility mode,
    // but we cannot tell what mode we are in from here.

    // Turn 0x7F into 8, because backspace needs to always be 8.
    if ([textStr isEqualToString:@"\x7F"])
        textStr = @"\x8";
    if ([unmodifiedStr isEqualToString:@"\x7F"])
        unmodifiedStr = @"\x8";
    // Always use 9 for tab -- we don't want to use AppKit's different character
    // for shift-tab.
    if (result.windowsKeyCode == 9) {
        textStr = @"\x9";
        unmodifiedStr = @"\x9";
    }

    // End Apple code.

    if ([textStr length] < WebKeyboardEvent::textLengthCap &&
        [unmodifiedStr length] < WebKeyboardEvent::textLengthCap) {
        [textStr getCharacters:&result.text[0]];
        [unmodifiedStr getCharacters:&result.unmodifiedText[0]];
    } else
        ASSERT_NOT_REACHED();

    [identifierStr getCString:&result.keyIdentifier[0]
                    maxLength:sizeof(result.keyIdentifier)
                     encoding:NSASCIIStringEncoding];

    result.timeStampSeconds = [event timestamp];

    // Windows and Linux set |isSystemKey| if alt is down. WebKit looks at this
    // flag to decide if it should handle a key or not. E.g. alt-left/right
    // shouldn't be used by WebKit to scroll the current page, because we want
    // to get that key back for it to do history navigation. Hence, the
    // corresponding situation on OS X is to set this for cmd key presses.
    if (result.modifiers & WebInputEvent::MetaKey)
        result.isSystemKey = true;

    return result;
}

WebKeyboardEvent WebInputEventFactory::keyboardEvent(wchar_t character,
                                                     int modifiers,
                                                     double timeStampSeconds)
{
    // keyboardEvent(NSEvent*) depends on the NSEvent object and
    // it is hard to use it from methods of the NSTextInput protocol. For
    // such methods, this function creates a WebInputEvent::Char event without
    // using a NSEvent object.
    WebKeyboardEvent result;
    result.type = WebKit::WebInputEvent::Char;
    result.timeStampSeconds = timeStampSeconds;
    result.modifiers = modifiers;
    result.windowsKeyCode = character;
    result.nativeKeyCode = character;
    result.text[0] = character;
    result.unmodifiedText[0] = character;

    // Windows and Linux set |isSystemKey| if alt is down. WebKit looks at this
    // flag to decide if it should handle a key or not. E.g. alt-left/right
    // shouldn't be used by WebKit to scroll the current page, because we want
    // to get that key back for it to do history navigation. Hence, the
    // corresponding situation on OS X is to set this for cmd key presses.
    if (result.modifiers & WebInputEvent::MetaKey)
        result.isSystemKey = true;

    return result;
}

// WebMouseEvent --------------------------------------------------------------

WebMouseEvent WebInputEventFactory::mouseEvent(NSEvent* event, NSView* view)
{
    WebMouseEvent result;

    result.clickCount = 0;

    switch ([event type]) {
    case NSMouseExited:
        result.type = WebInputEvent::MouseLeave;
        result.button = WebMouseEvent::ButtonNone;
        break;
    case NSLeftMouseDown:
        result.type = WebInputEvent::MouseDown;
        result.clickCount = [event clickCount];
        result.button = WebMouseEvent::ButtonLeft;
        break;
    case NSOtherMouseDown:
        result.type = WebInputEvent::MouseDown;
        result.clickCount = [event clickCount];
        result.button = WebMouseEvent::ButtonMiddle;
        break;
    case NSRightMouseDown:
        result.type = WebInputEvent::MouseDown;
        result.clickCount = [event clickCount];
        result.button = WebMouseEvent::ButtonRight;
        break;
    case NSLeftMouseUp:
        result.type = WebInputEvent::MouseUp;
        result.clickCount = [event clickCount];
        result.button = WebMouseEvent::ButtonLeft;
        break;
    case NSOtherMouseUp:
        result.type = WebInputEvent::MouseUp;
        result.clickCount = [event clickCount];
        result.button = WebMouseEvent::ButtonMiddle;
        break;
    case NSRightMouseUp:
        result.type = WebInputEvent::MouseUp;
        result.clickCount = [event clickCount];
        result.button = WebMouseEvent::ButtonRight;
        break;
    case NSMouseMoved:
    case NSMouseEntered:
        result.type = WebInputEvent::MouseMove;
        break;
    case NSLeftMouseDragged:
        result.type = WebInputEvent::MouseMove;
        result.button = WebMouseEvent::ButtonLeft;
        break;
    case NSOtherMouseDragged:
        result.type = WebInputEvent::MouseMove;
        result.button = WebMouseEvent::ButtonMiddle;
        break;
    case NSRightMouseDragged:
        result.type = WebInputEvent::MouseMove;
        result.button = WebMouseEvent::ButtonRight;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    setWebEventLocationFromEventInView(&result, event, view);

    result.modifiers = modifiersFromEvent(event);

    result.timeStampSeconds = [event timestamp];

    return result;
}

// WebMouseWheelEvent ---------------------------------------------------------

static WebMouseWheelEvent::Phase phaseForNSEventPhase(NSEventPhase eventPhase)
{
    uint32_t phase = WebMouseWheelEvent::PhaseNone;
    if (eventPhase & NSEventPhaseBegan)
        phase |= WebMouseWheelEvent::PhaseBegan;
    if (eventPhase & NSEventPhaseStationary)
        phase |= WebMouseWheelEvent::PhaseStationary;
    if (eventPhase & NSEventPhaseChanged)
        phase |= WebMouseWheelEvent::PhaseChanged;
    if (eventPhase & NSEventPhaseEnded)
        phase |= WebMouseWheelEvent::PhaseEnded;
    if (eventPhase & NSEventPhaseCancelled)
        phase |= WebMouseWheelEvent::PhaseCancelled;
    if (eventPhase & NSEventPhaseMayBegin)
        phase |= WebMouseWheelEvent::PhaseMayBegin;
    return static_cast<WebMouseWheelEvent::Phase>(phase);
}

static WebMouseWheelEvent::Phase phaseForEvent(NSEvent *event)
{
    if (![event respondsToSelector:@selector(phase)])
        return WebMouseWheelEvent::PhaseNone;

    NSEventPhase eventPhase = [event phase];
    return phaseForNSEventPhase(eventPhase);
}

static WebMouseWheelEvent::Phase momentumPhaseForEvent(NSEvent *event)
{
    if (![event respondsToSelector:@selector(momentumPhase)])
        return WebMouseWheelEvent::PhaseNone;

    NSEventPhase eventMomentumPhase = [event momentumPhase];
    return phaseForNSEventPhase(eventMomentumPhase);
}

WebMouseWheelEvent WebInputEventFactory::mouseWheelEvent(NSEvent* event, NSView* view)
{
    WebMouseWheelEvent result;

    result.type = WebInputEvent::MouseWheel;
    result.button = WebMouseEvent::ButtonNone;

    result.modifiers = modifiersFromEvent(event);

    setWebEventLocationFromEventInView(&result, event, view);

    // Of Mice and Men
    // ---------------
    //
    // There are three types of scroll data available on a scroll wheel CGEvent.
    // Apple's documentation ([1]) is rather vague in their differences, and not
    // terribly helpful in deciding which to use. This is what's really going on.
    //
    // First, these events behave very differently depending on whether a standard
    // wheel mouse is used (one that scrolls in discrete units) or a
    // trackpad/Mighty Mouse is used (which both provide continuous scrolling).
    // You must check to see which was used for the event by testing the
    // kCGScrollWheelEventIsContinuous field.
    //
    // Second, these events refer to "axes". Axis 1 is the y-axis, and axis 2 is
    // the x-axis.
    //
    // Third, there is a concept of mouse acceleration. Scrolling the same amount
    // of physical distance will give you different results logically depending on
    // whether you scrolled a little at a time or in one continuous motion. Some
    // fields account for this while others do not.
    //
    // Fourth, for trackpads there is a concept of chunkiness. When scrolling
    // continuously, events can be delivered in chunks. That is to say, lots of
    // scroll events with delta 0 will be delivered, and every so often an event
    // with a non-zero delta will be delivered, containing the accumulated deltas
    // from all the intermediate moves. [2]
    //
    // For notchy wheel mice (kCGScrollWheelEventIsContinuous == 0)
    // ------------------------------------------------------------
    //
    // kCGScrollWheelEventDeltaAxis*
    //   This is the rawest of raw events. For each mouse notch you get a value of
    //   +1/-1. This does not take acceleration into account and thus is less
    //   useful for building UIs.
    //
    // kCGScrollWheelEventPointDeltaAxis*
    //   This is smarter. In general, for each mouse notch you get a value of
    //   +1/-1, but this _does_ take acceleration into account, so you will get
    //   larger values on longer scrolls. This field would be ideal for building
    //   UIs except for one nasty bug: when the shift key is pressed, this set of
    //   fields fails to move the value into the axis2 field (the other two types
    //   of data do). This wouldn't be so bad except for the fact that while the
    //   number of axes is used in the creation of a CGScrollWheelEvent, there is
    //   no way to get that information out of the event once created.
    //
    // kCGScrollWheelEventFixedPtDeltaAxis*
    //   This is a fixed value, and for each mouse notch you get a value of
    //   +0.1/-0.1 (but, like above, scaled appropriately for acceleration). This
    //   value takes acceleration into account, and in fact is identical to the
    //   results you get from -[NSEvent delta*]. (That is, if you linked on Tiger
    //   or greater; see [2] for details.)
    //
    // A note about continuous devices
    // -------------------------------
    //
    // There are two devices that provide continuous scrolling events (trackpads
    // and Mighty Mouses) and they behave rather differently. The Mighty Mouse
    // behaves a lot like a regular mouse. There is no chunking, and the
    // FixedPtDelta values are the PointDelta values multiplied by 0.1. With the
    // trackpad, though, there is chunking. While the FixedPtDelta values are
    // reasonable (they occur about every fifth event but have values five times
    // larger than usual) the Delta values are unreasonable. They don't appear to
    // accumulate properly.
    //
    // For continuous devices (kCGScrollWheelEventIsContinuous != 0)
    // -------------------------------------------------------------
    //
    // kCGScrollWheelEventDeltaAxis*
    //   This provides values with no acceleration. With a trackpad, these values
    //   are chunked but each non-zero value does not appear to be cumulative.
    //   This seems to be a bug.
    //
    // kCGScrollWheelEventPointDeltaAxis*
    //   This provides values with acceleration. With a trackpad, these values are
    //   not chunked and are highly accurate.
    //
    // kCGScrollWheelEventFixedPtDeltaAxis*
    //   This provides values with acceleration. With a trackpad, these values are
    //   chunked but unlike Delta events are properly cumulative.
    //
    // Summary
    // -------
    //
    // In general the best approach to take is: determine if the event is
    // continuous. If it is not, then use the FixedPtDelta events (or just stick
    // with Cocoa events). They provide both acceleration and proper horizontal
    // scrolling. If the event is continuous, then doing pixel scrolling with the
    // PointDelta is the way to go. In general, avoid the Delta events. They're
    // the oldest (dating back to 10.4, before CGEvents were public) but they lack
    // acceleration and precision, making them useful only in specific edge cases.
    //
    // References
    // ----------
    //
    // [1] <http://developer.apple.com/documentation/Carbon/Reference/QuartzEventServicesRef/Reference/reference.html>
    // [2] <http://developer.apple.com/releasenotes/Cocoa/AppKitOlderNotes.html>
    //     Scroll to the section headed "NSScrollWheel events".
    //
    // P.S. The "smooth scrolling" option in the system preferences is utterly
    // unrelated to any of this.

    CGEventRef cgEvent = [event CGEvent];
    ASSERT(cgEvent);

    // Wheel ticks are supposed to be raw, unaccelerated values, one per physical
    // mouse wheel notch. The delta event is perfect for this (being a good
    // "specific edge case" as mentioned above). Trackpads, unfortunately, do
    // event chunking, and sending mousewheel events with 0 ticks causes some
    // websites to malfunction. Therefore, for all continuous input devices we use
    // the point delta data instead, since we cannot distinguish trackpad data
    // from data from any other continuous device.

    // Conversion between wheel delta amounts and number of pixels to scroll.
    static const double scrollbarPixelsPerCocoaTick = 40.0;

    if (CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventIsContinuous)) {
        result.deltaX = CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventPointDeltaAxis2);
        result.deltaY = CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventPointDeltaAxis1);
        result.wheelTicksX = result.deltaX / scrollbarPixelsPerCocoaTick;
        result.wheelTicksY = result.deltaY / scrollbarPixelsPerCocoaTick;
        result.hasPreciseScrollingDeltas = true;
    } else {
        result.deltaX = [event deltaX] * scrollbarPixelsPerCocoaTick;
        result.deltaY = [event deltaY] * scrollbarPixelsPerCocoaTick;
        result.wheelTicksY = CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventDeltaAxis1);
        result.wheelTicksX = CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventDeltaAxis2);
    }

    result.timeStampSeconds = [event timestamp];

    result.phase              = phaseForEvent(event);
    result.momentumPhase      = momentumPhaseForEvent(event);

    return result;
}

WebGestureEvent WebInputEventFactory::gestureEvent(NSEvent *event, NSView *view)
{
    WebGestureEvent result;

    // Use a temporary WebMouseEvent to get the location.
    WebMouseEvent temp;

    setWebEventLocationFromEventInView(&temp, event, view);
    result.x = temp.x;
    result.y = temp.y;
    result.globalX = temp.globalX;
    result.globalY = temp.globalY;

    result.type = gestureEventTypeForEvent(event);
    result.modifiers = modifiersFromEvent(event);
    result.timeStampSeconds = [event timestamp];

    return result;
}

} // namespace WebKit
