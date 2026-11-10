/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#import "KeyEventCodesIOS.h"
#endif

namespace WebCore {

// https://w3c.github.io/uievents-key/
RefPtr<StringImpl> keyForCharCode(unichar charCode)
{
    switch (charCode) {
    case NSUpArrowFunctionKey:
        return "ArrowUp"_si;
    case NSDownArrowFunctionKey:
        return "ArrowDown"_si;
    case NSLeftArrowFunctionKey:
        return "ArrowLeft"_si;
    case NSRightArrowFunctionKey:
        return "ArrowRight"_si;
    case NSF1FunctionKey:
        return "F1"_si;
    case NSF2FunctionKey:
        return "F2"_si;
    case NSF3FunctionKey:
        return "F3"_si;
    case NSF4FunctionKey:
        return "F4"_si;
    case NSF5FunctionKey:
        return "F5"_si;
    case NSF6FunctionKey:
        return "F6"_si;
    case NSF7FunctionKey:
        return "F7"_si;
    case NSF8FunctionKey:
        return "F8"_si;
    case NSF9FunctionKey:
        return "F9"_si;
    case NSF10FunctionKey:
        return "F10"_si;
    case NSF11FunctionKey:
        return "F11"_si;
    case NSF12FunctionKey:
        return "F12"_si;
    case NSF13FunctionKey:
        return "F13"_si;
    case NSF14FunctionKey:
        return "F14"_si;
    case NSF15FunctionKey:
        return "F15"_si;
    case NSF16FunctionKey:
        return "F16"_si;
    case NSF17FunctionKey:
        return "F17"_si;
    case NSF18FunctionKey:
        return "F18"_si;
    case NSF19FunctionKey:
        return "F19"_si;
    case NSF20FunctionKey:
        return "F20"_si;
    case NSF21FunctionKey:
        return "F21"_si;
    case NSF22FunctionKey:
        return "F22"_si;
    case NSF23FunctionKey:
        return "F23"_si;
    case NSF24FunctionKey:
        return "F24"_si;
    case NSF25FunctionKey:
        return "F25"_si;
    case NSF26FunctionKey:
        return "F26"_si;
    case NSF27FunctionKey:
        return "F27"_si;
    case NSF28FunctionKey:
        return "F28"_si;
    case NSF29FunctionKey:
        return "F29"_si;
    case NSF30FunctionKey:
        return "F30"_si;
    case NSF31FunctionKey:
        return "F31"_si;
    case NSF32FunctionKey:
        return "F32"_si;
    case NSF33FunctionKey:
        return "F33"_si;
    case NSF34FunctionKey:
        return "F34"_si;
    case NSF35FunctionKey:
        return "F35"_si;
    case NSInsertFunctionKey:
        return "Insert"_si;
    case NSDeleteFunctionKey:
        return "Delete"_si;
    case NSHomeFunctionKey:
        return "Home"_si;
    case NSEndFunctionKey:
        return "End"_si;
    case NSPageUpFunctionKey:
        return "PageUp"_si;
    case NSPageDownFunctionKey:
        return "PageDown"_si;
    case NSPrintScreenFunctionKey:
        return "PrintScreen"_si;
    case NSScrollLockFunctionKey:
        return "ScrollLock"_si;
    case NSPauseFunctionKey:
        return "Pause"_si;
    case NSMenuFunctionKey:
        return "ContextMenu"_si;
    case NSPrintFunctionKey:
        return "Print"_si;
    case NSClearLineFunctionKey:
        return "Clear"_si;
    case NSSelectFunctionKey:
        return "Select"_si;
    case NSExecuteFunctionKey:
        return "Execute"_si;
    case NSUndoFunctionKey:
        return "Undo"_si;
    case NSRedoFunctionKey:
        return "Redo"_si;
    case NSFindFunctionKey:
        return "Find"_si;
    case NSHelpFunctionKey:
        return "Help"_si;
    case NSModeSwitchFunctionKey:
        return "ModeChange"_si;
    case NSEnterCharacter:
    case NSNewlineCharacter:
    case NSCarriageReturnCharacter:
        return "Enter"_si;
    case NSDeleteCharacter:
    case NSBackspaceCharacter:
        return "Backspace"_si;
    case NSBackTabCharacter:
    case NSTabCharacter:
        return "Tab"_si;
    case 0x1B:
        return "Escape"_si;
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
        return "Unidentified"_si;
    default:
        return StringImpl::create(singleElementSpan(*reinterpret_cast<const char16_t*>(&charCode)));
    }
}

RefPtr<StringImpl> keyIdentifierForCharCode(unichar charCode)
{
    switch (charCode) {
        // Each identifier listed in the DOM spec is listed here.
        // Many are simply commented out since they do not appear on standard Macintosh keyboards
        // or are on a key that doesn't have a corresponding character.

        // "Accept"
        // "AllCandidates"

        // "Alt"
        case NSMenuFunctionKey:
            return "Alt"_si;

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
            return "Clear"_si;

        // "CodeInput"
        // "Compose"
        // "Control"
        // "Crsel"
        // "Convert"
        // "Copy"
        // "Cut"

        // "Down"
        case NSDownArrowFunctionKey:
            return "Down"_si;
        // "End"
        case NSEndFunctionKey:
            return "End"_si;
        // "Enter"
        case 0x3: case 0xA: case 0xD: // Macintosh calls the one on the main keyboard Return, but Windows calls it Enter, so we'll do the same for the DOM
            return "Enter"_si;

        // "EraseEof"

        // "Execute"
        case NSExecuteFunctionKey:
            return "Execute"_si;

        // "Exsel"

        // "F1"
        case NSF1FunctionKey:
            return "F1"_si;
        // "F2"
        case NSF2FunctionKey:
            return "F2"_si;
        // "F3"
        case NSF3FunctionKey:
            return "F3"_si;
        // "F4"
        case NSF4FunctionKey:
            return "F4"_si;
        // "F5"
        case NSF5FunctionKey:
            return "F5"_si;
        // "F6"
        case NSF6FunctionKey:
            return "F6"_si;
        // "F7"
        case NSF7FunctionKey:
            return "F7"_si;
        // "F8"
        case NSF8FunctionKey:
            return "F8"_si;
        // "F9"
        case NSF9FunctionKey:
            return "F9"_si;
        // "F10"
        case NSF10FunctionKey:
            return "F10"_si;
        // "F11"
        case NSF11FunctionKey:
            return "F11"_si;
        // "F12"
        case NSF12FunctionKey:
            return "F12"_si;
        // "F13"
        case NSF13FunctionKey:
            return "F13"_si;
        // "F14"
        case NSF14FunctionKey:
            return "F14"_si;
        // "F15"
        case NSF15FunctionKey:
            return "F15"_si;
        // "F16"
        case NSF16FunctionKey:
            return "F16"_si;
        // "F17"
        case NSF17FunctionKey:
            return "F17"_si;
        // "F18"
        case NSF18FunctionKey:
            return "F18"_si;
        // "F19"
        case NSF19FunctionKey:
            return "F19"_si;
        // "F20"
        case NSF20FunctionKey:
            return "F20"_si;
        // "F21"
        case NSF21FunctionKey:
            return "F21"_si;
        // "F22"
        case NSF22FunctionKey:
            return "F22"_si;
        // "F23"
        case NSF23FunctionKey:
            return "F23"_si;
        // "F24"
        case NSF24FunctionKey:
            return "F24"_si;

        // "FinalMode"

        // "Find"
        case NSFindFunctionKey:
            return "Find"_si;

        // "FullWidth"
        // "HalfWidth"
        // "HangulMode"
        // "HanjaMode"

        // "Help"
        case NSHelpFunctionKey:
            return "Help"_si;

        // "Hiragana"

        // "Home"
        case NSHomeFunctionKey:
            return "Home"_si;
        // "Insert"
        case NSInsertFunctionKey:
            return "Insert"_si;

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
            return "Left"_si;

        // "Meta"
        // "MediaNextTrack"
        // "MediaPlayPause"
        // "MediaPreviousTrack"
        // "MediaStop"

        // "ModeChange"
        case NSModeSwitchFunctionKey:
            return "ModeChange"_si;

        // "Nonconvert"
        // "NumLock"

        // "PageDown"
        case NSPageDownFunctionKey:
            return "PageDown"_si;
        // "PageUp"
        case NSPageUpFunctionKey:
            return "PageUp"_si;

        // "Paste"

        // "Pause"
        case NSPauseFunctionKey:
            return "Pause"_si;

        // "Play"
        // "PreviousCandidate"

        // "PrintScreen"
        case NSPrintScreenFunctionKey:
            return "PrintScreen"_si;

        // "Process"
        // "Props"

        // "Right"
        case NSRightArrowFunctionKey:
            return "Right"_si;

        // "RomanCharacters"

        // "Scroll"
        case NSScrollLockFunctionKey:
            return "Scroll"_si;
        // "Select"
        case NSSelectFunctionKey:
            return "Select"_si;

        // "SelectMedia"
        // "Shift"

        // "Stop"
        case NSStopFunctionKey:
            return "Stop"_si;
        // "Up"
        case NSUpArrowFunctionKey:
            return "Up"_si;
        // "Undo"
        case NSUndoFunctionKey:
            return "Undo"_si;

        // "VolumeDown"
        // "VolumeMute"
        // "VolumeUp"
        // "Win"
        // "Zoom"

        // More function keys, not in the key identifier specification.
        case NSF25FunctionKey:
            return "F25"_si;
        case NSF26FunctionKey:
            return "F26"_si;
        case NSF27FunctionKey:
            return "F27"_si;
        case NSF28FunctionKey:
            return "F28"_si;
        case NSF29FunctionKey:
            return "F29"_si;
        case NSF30FunctionKey:
            return "F30"_si;
        case NSF31FunctionKey:
            return "F31"_si;
        case NSF32FunctionKey:
            return "F32"_si;
        case NSF33FunctionKey:
            return "F33"_si;
        case NSF34FunctionKey:
            return "F34"_si;
        case NSF35FunctionKey:
            return "F35"_si;

        // Turn 0x7F into 0x08, because backspace needs to always be 0x08.
        case 0x7F:
            return "U+0008"_si;
        // Standard says that DEL becomes U+007F.
        case NSDeleteFunctionKey:
            return "U+007F"_si;

        // Always use 0x09 for tab instead of AppKit's backtab character.
        case NSBackTabCharacter:
            return "U+0009"_si;

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
            return makeString("U+"_s, hex(toASCIIUpper(charCode), 4));
    }
}

}
