/*
 * Copyright (C) 2004-2022 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "KeyEventCocoa.h"

#import "Logging.h"
#import "PlatformKeyboardEvent.h"
#import "WindowsKeyboardCodes.h"
#import <wtf/ASCIICType.h>
#import <wtf/HexNumber.h>
#import <wtf/MainThread.h>

#if PLATFORM(IOS_FAMILY)
#import "KeyEventCodesIOS.h"
#endif

namespace WebCore {

// https://w3c.github.io/uievents-key/
String keyForCharCode(unichar charCode)
{
    switch (charCode) {
    case NSUpArrowFunctionKey:
        return "ArrowUp"_s;
    case NSDownArrowFunctionKey:
        return "ArrowDown"_s;
    case NSLeftArrowFunctionKey:
        return "ArrowLeft"_s;
    case NSRightArrowFunctionKey:
        return "ArrowRight"_s;
    case NSF1FunctionKey:
        return "F1"_s;
    case NSF2FunctionKey:
        return "F2"_s;
    case NSF3FunctionKey:
        return "F3"_s;
    case NSF4FunctionKey:
        return "F4"_s;
    case NSF5FunctionKey:
        return "F5"_s;
    case NSF6FunctionKey:
        return "F6"_s;
    case NSF7FunctionKey:
        return "F7"_s;
    case NSF8FunctionKey:
        return "F8"_s;
    case NSF9FunctionKey:
        return "F9"_s;
    case NSF10FunctionKey:
        return "F10"_s;
    case NSF11FunctionKey:
        return "F11"_s;
    case NSF12FunctionKey:
        return "F12"_s;
    case NSF13FunctionKey:
        return "F13"_s;
    case NSF14FunctionKey:
        return "F14"_s;
    case NSF15FunctionKey:
        return "F15"_s;
    case NSF16FunctionKey:
        return "F16"_s;
    case NSF17FunctionKey:
        return "F17"_s;
    case NSF18FunctionKey:
        return "F18"_s;
    case NSF19FunctionKey:
        return "F19"_s;
    case NSF20FunctionKey:
        return "F20"_s;
    case NSF21FunctionKey:
        return "F21"_s;
    case NSF22FunctionKey:
        return "F22"_s;
    case NSF23FunctionKey:
        return "F23"_s;
    case NSF24FunctionKey:
        return "F24"_s;
    case NSF25FunctionKey:
        return "F25"_s;
    case NSF26FunctionKey:
        return "F26"_s;
    case NSF27FunctionKey:
        return "F27"_s;
    case NSF28FunctionKey:
        return "F28"_s;
    case NSF29FunctionKey:
        return "F29"_s;
    case NSF30FunctionKey:
        return "F30"_s;
    case NSF31FunctionKey:
        return "F31"_s;
    case NSF32FunctionKey:
        return "F32"_s;
    case NSF33FunctionKey:
        return "F33"_s;
    case NSF34FunctionKey:
        return "F34"_s;
    case NSF35FunctionKey:
        return "F35"_s;
    case NSInsertFunctionKey:
        return "Insert"_s;
    case NSDeleteFunctionKey:
        return "Delete"_s;
    case NSHomeFunctionKey:
        return "Home"_s;
    case NSEndFunctionKey:
        return "End"_s;
    case NSPageUpFunctionKey:
        return "PageUp"_s;
    case NSPageDownFunctionKey:
        return "PageDown"_s;
    case NSPrintScreenFunctionKey:
        return "PrintScreen"_s;
    case NSScrollLockFunctionKey:
        return "ScrollLock"_s;
    case NSPauseFunctionKey:
        return "Pause"_s;
    case NSMenuFunctionKey:
        return "ContextMenu"_s;
    case NSPrintFunctionKey:
        return "Print"_s;
    case NSClearLineFunctionKey:
        return "Clear"_s;
    case NSSelectFunctionKey:
        return "Select"_s;
    case NSExecuteFunctionKey:
        return "Execute"_s;
    case NSUndoFunctionKey:
        return "Undo"_s;
    case NSRedoFunctionKey:
        return "Redo"_s;
    case NSFindFunctionKey:
        return "Find"_s;
    case NSHelpFunctionKey:
        return "Help"_s;
    case NSModeSwitchFunctionKey:
        return "ModeChange"_s;
    case NSEnterCharacter:
    case NSNewlineCharacter:
    case NSCarriageReturnCharacter:
        return "Enter"_s;
    case NSDeleteCharacter:
    case NSBackspaceCharacter:
        return "Backspace"_s;
    case NSBackTabCharacter:
    case NSTabCharacter:
        return "Tab"_s;
    case 0x1B:
        return "Escape"_s;
    case NSFormFeedCharacter:
    case NSParagraphSeparatorCharacter:
    case NSLineSeparatorCharacter:
    case NSBeginFunctionKey:
    case NSSysReqFunctionKey:
    case NSBreakFunctionKey:
    case NSResetFunctionKey:
    case NSStopFunctionKey:
    case NSUserFunctionKey:
    case NSSystemFunctionKey:
    case NSClearDisplayFunctionKey:
    case NSInsertLineFunctionKey:
    case NSDeleteLineFunctionKey:
    case NSInsertCharFunctionKey:
    case NSDeleteCharFunctionKey:
    case NSPrevFunctionKey:
    case NSNextFunctionKey:
        return "Unidentified"_s;
    default:
        return String(reinterpret_cast<UChar*>(&charCode), 1);
    }
}

String keyIdentifierForCharCode(unichar charCode)
{
    switch (charCode) {
        // Each identifier listed in the DOM spec is listed here.
        // Many are simply commented out since they do not appear on standard Macintosh keyboards
        // or are on a key that doesn't have a corresponding character.

        // "Accept"
        // "AllCandidates"

        // "Alt"
        case NSMenuFunctionKey:
            return "Alt"_s;

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
            return "Clear"_s;

        // "CodeInput"
        // "Compose"
        // "Control"
        // "Crsel"
        // "Convert"
        // "Copy"
        // "Cut"

        // "Down"
        case NSDownArrowFunctionKey:
            return "Down"_s;
        // "End"
        case NSEndFunctionKey:
            return "End"_s;
        // "Enter"
        case 0x3: case 0xA: case 0xD: // Macintosh calls the one on the main keyboard Return, but Windows calls it Enter, so we'll do the same for the DOM
            return "Enter"_s;

        // "EraseEof"

        // "Execute"
        case NSExecuteFunctionKey:
            return "Execute"_s;

        // "Exsel"

        // "F1"
        case NSF1FunctionKey:
            return "F1"_s;
        // "F2"
        case NSF2FunctionKey:
            return "F2"_s;
        // "F3"
        case NSF3FunctionKey:
            return "F3"_s;
        // "F4"
        case NSF4FunctionKey:
            return "F4"_s;
        // "F5"
        case NSF5FunctionKey:
            return "F5"_s;
        // "F6"
        case NSF6FunctionKey:
            return "F6"_s;
        // "F7"
        case NSF7FunctionKey:
            return "F7"_s;
        // "F8"
        case NSF8FunctionKey:
            return "F8"_s;
        // "F9"
        case NSF9FunctionKey:
            return "F9"_s;
        // "F10"
        case NSF10FunctionKey:
            return "F10"_s;
        // "F11"
        case NSF11FunctionKey:
            return "F11"_s;
        // "F12"
        case NSF12FunctionKey:
            return "F12"_s;
        // "F13"
        case NSF13FunctionKey:
            return "F13"_s;
        // "F14"
        case NSF14FunctionKey:
            return "F14"_s;
        // "F15"
        case NSF15FunctionKey:
            return "F15"_s;
        // "F16"
        case NSF16FunctionKey:
            return "F16"_s;
        // "F17"
        case NSF17FunctionKey:
            return "F17"_s;
        // "F18"
        case NSF18FunctionKey:
            return "F18"_s;
        // "F19"
        case NSF19FunctionKey:
            return "F19"_s;
        // "F20"
        case NSF20FunctionKey:
            return "F20"_s;
        // "F21"
        case NSF21FunctionKey:
            return "F21"_s;
        // "F22"
        case NSF22FunctionKey:
            return "F22"_s;
        // "F23"
        case NSF23FunctionKey:
            return "F23"_s;
        // "F24"
        case NSF24FunctionKey:
            return "F24"_s;

        // "FinalMode"

        // "Find"
        case NSFindFunctionKey:
            return "Find"_s;

        // "FullWidth"
        // "HalfWidth"
        // "HangulMode"
        // "HanjaMode"

        // "Help"
        case NSHelpFunctionKey:
            return "Help"_s;

        // "Hiragana"

        // "Home"
        case NSHomeFunctionKey:
            return "Home"_s;
        // "Insert"
        case NSInsertFunctionKey:
            return "Insert"_s;

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
            return "Left"_s;

        // "Meta"
        // "MediaNextTrack"
        // "MediaPlayPause"
        // "MediaPreviousTrack"
        // "MediaStop"

        // "ModeChange"
        case NSModeSwitchFunctionKey:
            return "ModeChange"_s;

        // "Nonconvert"
        // "NumLock"

        // "PageDown"
        case NSPageDownFunctionKey:
            return "PageDown"_s;
        // "PageUp"
        case NSPageUpFunctionKey:
            return "PageUp"_s;

        // "Paste"

        // "Pause"
        case NSPauseFunctionKey:
            return "Pause"_s;

        // "Play"
        // "PreviousCandidate"

        // "PrintScreen"
        case NSPrintScreenFunctionKey:
            return "PrintScreen"_s;

        // "Process"
        // "Props"

        // "Right"
        case NSRightArrowFunctionKey:
            return "Right"_s;

        // "RomanCharacters"

        // "Scroll"
        case NSScrollLockFunctionKey:
            return "Scroll"_s;
        // "Select"
        case NSSelectFunctionKey:
            return "Select"_s;

        // "SelectMedia"
        // "Shift"

        // "Stop"
        case NSStopFunctionKey:
            return "Stop"_s;
        // "Up"
        case NSUpArrowFunctionKey:
            return "Up"_s;
        // "Undo"
        case NSUndoFunctionKey:
            return "Undo"_s;

        // "VolumeDown"
        // "VolumeMute"
        // "VolumeUp"
        // "Win"
        // "Zoom"

        // More function keys, not in the key identifier specification.
        case NSF25FunctionKey:
            return "F25"_s;
        case NSF26FunctionKey:
            return "F26"_s;
        case NSF27FunctionKey:
            return "F27"_s;
        case NSF28FunctionKey:
            return "F28"_s;
        case NSF29FunctionKey:
            return "F29"_s;
        case NSF30FunctionKey:
            return "F30"_s;
        case NSF31FunctionKey:
            return "F31"_s;
        case NSF32FunctionKey:
            return "F32"_s;
        case NSF33FunctionKey:
            return "F33"_s;
        case NSF34FunctionKey:
            return "F34"_s;
        case NSF35FunctionKey:
            return "F35"_s;

        // Turn 0x7F into 0x08, because backspace needs to always be 0x08.
        case 0x7F:
            return "U+0008"_s;
        // Standard says that DEL becomes U+007F.
        case NSDeleteFunctionKey:
            return "U+007F"_s;

        // Always use 0x09 for tab instead of AppKit's backtab character.
        case NSBackTabCharacter:
            return "U+0009"_s;

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
            return makeString("U+", hex(toASCIIUpper(charCode), 4));
    }
}

}
