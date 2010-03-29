/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc.  All rights reserved.
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

#import "config.h"
#import "KeyEventCocoa.h"

#import "Logging.h"
#import "PlatformString.h"
#import "WindowsKeyboardCodes.h"
#import <wtf/ASCIICType.h>

#if PLATFORM(IPHONE)
#import "KeyEventCodesIPhone.h"
#endif

using namespace WTF;

namespace WebCore {

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
            return "Alt";

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
            return "Clear";

        // "CodeInput"
        // "Compose"
        // "Control"
        // "Crsel"
        // "Convert"
        // "Copy"
        // "Cut"

        // "Down"
        case NSDownArrowFunctionKey:
            return "Down";
        // "End"
        case NSEndFunctionKey:
            return "End";
        // "Enter"
        case 0x3: case 0xA: case 0xD: // Macintosh calls the one on the main keyboard Return, but Windows calls it Enter, so we'll do the same for the DOM
            return "Enter";

        // "EraseEof"

        // "Execute"
        case NSExecuteFunctionKey:
            return "Execute";

        // "Exsel"

        // "F1"
        case NSF1FunctionKey:
            return "F1";
        // "F2"
        case NSF2FunctionKey:
            return "F2";
        // "F3"
        case NSF3FunctionKey:
            return "F3";
        // "F4"
        case NSF4FunctionKey:
            return "F4";
        // "F5"
        case NSF5FunctionKey:
            return "F5";
        // "F6"
        case NSF6FunctionKey:
            return "F6";
        // "F7"
        case NSF7FunctionKey:
            return "F7";
        // "F8"
        case NSF8FunctionKey:
            return "F8";
        // "F9"
        case NSF9FunctionKey:
            return "F9";
        // "F10"
        case NSF10FunctionKey:
            return "F10";
        // "F11"
        case NSF11FunctionKey:
            return "F11";
        // "F12"
        case NSF12FunctionKey:
            return "F12";
        // "F13"
        case NSF13FunctionKey:
            return "F13";
        // "F14"
        case NSF14FunctionKey:
            return "F14";
        // "F15"
        case NSF15FunctionKey:
            return "F15";
        // "F16"
        case NSF16FunctionKey:
            return "F16";
        // "F17"
        case NSF17FunctionKey:
            return "F17";
        // "F18"
        case NSF18FunctionKey:
            return "F18";
        // "F19"
        case NSF19FunctionKey:
            return "F19";
        // "F20"
        case NSF20FunctionKey:
            return "F20";
        // "F21"
        case NSF21FunctionKey:
            return "F21";
        // "F22"
        case NSF22FunctionKey:
            return "F22";
        // "F23"
        case NSF23FunctionKey:
            return "F23";
        // "F24"
        case NSF24FunctionKey:
            return "F24";

        // "FinalMode"

        // "Find"
        case NSFindFunctionKey:
            return "Find";

        // "FullWidth"
        // "HalfWidth"
        // "HangulMode"
        // "HanjaMode"

        // "Help"
        case NSHelpFunctionKey:
            return "Help";

        // "Hiragana"

        // "Home"
        case NSHomeFunctionKey:
            return "Home";
        // "Insert"
        case NSInsertFunctionKey:
            return "Insert";

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
            return "Left";

        // "Meta"
        // "MediaNextTrack"
        // "MediaPlayPause"
        // "MediaPreviousTrack"
        // "MediaStop"

        // "ModeChange"
        case NSModeSwitchFunctionKey:
            return "ModeChange";

        // "Nonconvert"
        // "NumLock"

        // "PageDown"
        case NSPageDownFunctionKey:
            return "PageDown";
        // "PageUp"
        case NSPageUpFunctionKey:
            return "PageUp";

        // "Paste"

        // "Pause"
        case NSPauseFunctionKey:
            return "Pause";

        // "Play"
        // "PreviousCandidate"

        // "PrintScreen"
        case NSPrintScreenFunctionKey:
            return "PrintScreen";

        // "Process"
        // "Props"

        // "Right"
        case NSRightArrowFunctionKey:
            return "Right";

        // "RomanCharacters"

        // "Scroll"
        case NSScrollLockFunctionKey:
            return "Scroll";
        // "Select"
        case NSSelectFunctionKey:
            return "Select";

        // "SelectMedia"
        // "Shift"

        // "Stop"
        case NSStopFunctionKey:
            return "Stop";
        // "Up"
        case NSUpArrowFunctionKey:
            return "Up";
        // "Undo"
        case NSUndoFunctionKey:
            return "Undo";

        // "VolumeDown"
        // "VolumeMute"
        // "VolumeUp"
        // "Win"
        // "Zoom"

        // More function keys, not in the key identifier specification.
        case NSF25FunctionKey:
            return "F25";
        case NSF26FunctionKey:
            return "F26";
        case NSF27FunctionKey:
            return "F27";
        case NSF28FunctionKey:
            return "F28";
        case NSF29FunctionKey:
            return "F29";
        case NSF30FunctionKey:
            return "F30";
        case NSF31FunctionKey:
            return "F31";
        case NSF32FunctionKey:
            return "F32";
        case NSF33FunctionKey:
            return "F33";
        case NSF34FunctionKey:
            return "F34";
        case NSF35FunctionKey:
            return "F35";

        // Turn 0x7F into 0x08, because backspace needs to always be 0x08.
        case 0x7F:
            return "U+0008";
        // Standard says that DEL becomes U+007F.
        case NSDeleteFunctionKey:
            return "U+007F";

        // Always use 0x09 for tab instead of AppKit's backtab character.
        case NSBackTabCharacter:
            return "U+0009";

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
            return String::format("U+%04X", toASCIIUpper(charCode));
    }
}

int windowsKeyCodeForKeyCode(uint16_t keyCode)
{
    switch (keyCode) {
        case 48: return VK_TAB;

        case 54: // Right Command
            return VK_APPS;

        case 55: // Left Command
            return VK_LWIN;

        case 57: // Capslock
            return VK_CAPITAL;

        case 56: // Left Shift
        case 60: // Right Shift
            return VK_SHIFT;

        case 58: // Left Alt
        case 61: // Right Alt
            return VK_MENU;

        case 59: // Left Ctrl
        case 62: // Right Ctrl
            return VK_CONTROL;

        case 71: return VK_CLEAR;

        case 82: return VK_NUMPAD0;
        case 83: return VK_NUMPAD1;
        case 84: return VK_NUMPAD2;
        case 85: return VK_NUMPAD3;
        case 86: return VK_NUMPAD4;
        case 87: return VK_NUMPAD5;
        case 88: return VK_NUMPAD6;
        case 89: return VK_NUMPAD7;
        case 91: return VK_NUMPAD8;
        case 92: return VK_NUMPAD9;
        case 67: return VK_MULTIPLY;
        case 69: return VK_ADD;
        case 78: return VK_SUBTRACT;
        case 65: return VK_DECIMAL;
        case 75: return VK_DIVIDE;
     }

     return 0;
}

int windowsKeyCodeForCharCode(unichar charCode)
{
    switch (charCode) {
        case 8: case 0x7F: return VK_BACK;
        case 9: return VK_TAB;

        case 0xD: case 3: return VK_RETURN;

        case NSPauseFunctionKey: return VK_PAUSE;

        case 0x1B: return VK_ESCAPE;

        case ' ': return VK_SPACE;
        case NSPageUpFunctionKey: return VK_PRIOR;
        case NSPageDownFunctionKey: return VK_NEXT;
        case NSEndFunctionKey: return VK_END;
        case NSHomeFunctionKey: return VK_HOME;
        case NSLeftArrowFunctionKey: return VK_LEFT;
        case NSUpArrowFunctionKey: return VK_UP;
        case NSRightArrowFunctionKey: return VK_RIGHT;
        case NSDownArrowFunctionKey: return VK_DOWN;
        case NSSelectFunctionKey: return VK_SELECT;
        case NSPrintFunctionKey: return VK_PRINT;
        case NSExecuteFunctionKey: return VK_EXECUTE;
        case NSPrintScreenFunctionKey: return VK_SNAPSHOT;
        case NSInsertFunctionKey: case NSHelpFunctionKey: return VK_INSERT;
        case NSDeleteFunctionKey: return VK_DELETE;
        case '0': case ')': return VK_0;
        case '1': case '!': return VK_1;
        case '2': case '@': return VK_2;
        case '3': case '#': return VK_3;
        case '4': case '$': return VK_4;
        case '5': case '%': return VK_5;
        case '6': case '^': return VK_6;
        case '7': case '&': return VK_7;
        case '8': case '*': return VK_8;
        case '9': case '(': return VK_9;
        case 'a': case 'A': return VK_A;
        case 'b': case 'B': return VK_B;
        case 'c': case 'C': return VK_C;
        case 'd': case 'D': return VK_D;
        case 'e': case 'E': return VK_E;
        case 'f': case 'F': return VK_F;
        case 'g': case 'G': return VK_G;
        case 'h': case 'H': return VK_H;
        case 'i': case 'I': return VK_I;
        case 'j': case 'J': return VK_J;
        case 'k': case 'K': return VK_K;
        case 'l': case 'L': return VK_L;
        case 'm': case 'M': return VK_M;
        case 'n': case 'N': return VK_N;
        case 'o': case 'O': return VK_O;
        case 'p': case 'P': return VK_P;
        case 'q': case 'Q': return VK_Q;
        case 'r': case 'R': return VK_R;
        case 's': case 'S': return VK_S;
        case 't': case 'T': return VK_T;
        case 'u': case 'U': return VK_U;
        case 'v': case 'V': return VK_V;
        case 'w': case 'W': return VK_W;
        case 'x': case 'X': return VK_X;
        case 'y': case 'Y': return VK_Y;
        case 'z': case 'Z': return VK_Z;

        case NSF1FunctionKey: return VK_F1;
        case NSF2FunctionKey: return VK_F2;
        case NSF3FunctionKey: return VK_F3;
        case NSF4FunctionKey: return VK_F4;
        case NSF5FunctionKey: return VK_F5;
        case NSF6FunctionKey: return VK_F6;
        case NSF7FunctionKey: return VK_F7;
        case NSF8FunctionKey: return VK_F8;
        case NSF9FunctionKey: return VK_F9;
        case NSF10FunctionKey: return VK_F10;
        case NSF11FunctionKey: return VK_F11;
        case NSF12FunctionKey: return VK_F12;
        case NSF13FunctionKey: return VK_F13;
        case NSF14FunctionKey: return VK_F14;
        case NSF15FunctionKey: return VK_F15;
        case NSF16FunctionKey: return VK_F16;
        case NSF17FunctionKey: return VK_F17;
        case NSF18FunctionKey: return VK_F18;
        case NSF19FunctionKey: return VK_F19;
        case NSF20FunctionKey: return VK_F20;
        case NSF21FunctionKey: return VK_F21;
        case NSF22FunctionKey: return VK_F22;
        case NSF23FunctionKey: return VK_F23;
        case NSF24FunctionKey: return VK_F24;
        case NSScrollLockFunctionKey: return VK_SCROLL;

        case ';': case ':': return VK_OEM_1;
        case '=': case '+': return VK_OEM_PLUS;
        case ',': case '<': return VK_OEM_COMMA;
        case '-': case '_': return VK_OEM_MINUS;
        case '.': case '>': return VK_OEM_PERIOD;
        case '/': case '?': return VK_OEM_2;
        case '`': case '~': return VK_OEM_3;
        case '[': case '{': return VK_OEM_4;
        case '\\': case '|': return VK_OEM_5;
        case ']': case '}': return VK_OEM_6;
        case '\'': case '"': return VK_OEM_7;
    }

    return 0;
}

}
