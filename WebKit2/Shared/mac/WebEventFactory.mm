/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "WebEventFactory.h"
#import <wtf/ASCIICType.h>

using namespace WebCore;

@interface NSEvent (Details)
- (CGFloat)deviceDeltaX;
- (CGFloat)deviceDeltaY;
- (BOOL)_continuousScroll;
@end

namespace WebKit {

static WebMouseEvent::Button mouseButtonForEvent(NSEvent *event)
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
            return WebMouseEvent::LeftButton;
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
            return WebMouseEvent::RightButton;
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
            return WebMouseEvent::MiddleButton;
        default:
            return WebMouseEvent::NoButton;
    }
}

static WebEvent::Type mouseEventTypeForEvent(NSEvent* event)
{
    switch ([event type]) {
        case NSLeftMouseDragged:
        case NSRightMouseDragged:
        case NSOtherMouseDragged:
        case NSMouseMoved:
            return WebEvent::MouseMove;
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
            return WebEvent::MouseDown;
        case NSLeftMouseUp:
        case NSRightMouseUp:
        case NSOtherMouseUp:
            return WebEvent::MouseUp;
        default:
            return WebEvent::MouseMove;
    }
}

static int clickCountForEvent(NSEvent *event)
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
            return [event clickCount];
        default:
            return 0;
    }
}

static NSScreen *screenForWindow(NSWindow *window)
{
    NSScreen *screen = [window screen]; // nil if the window is off-screen
    if (screen)
        return screen;
    
    NSArray *screens = [NSScreen screens];
    if ([screens count] > 0)
        return [screens objectAtIndex:0]; // screen containing the menubar
    
    return nil;
}

static NSPoint flipScreenPoint(const NSPoint& screenPoint, NSScreen *screen)
{
    NSPoint flippedPoint = screenPoint;
    flippedPoint.y = NSMaxY([screen frame]) - flippedPoint.y;
    return flippedPoint;
}

static NSPoint globalPoint(const NSPoint& windowPoint, NSWindow *window)
{
    return flipScreenPoint([window convertBaseToScreen:windowPoint], screenForWindow(window));
}

static NSPoint globalPointForEvent(NSEvent *event)
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
        case NSMouseMoved:
        case NSScrollWheel:
            return globalPoint([event locationInWindow], [event window]);
        default:
            return NSZeroPoint;
    }
}

static NSPoint pointForEvent(NSEvent *event, NSView *windowView)
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
        case NSMouseMoved:
        case NSScrollWheel: {
            // Note: This will have its origin at the bottom left of the window unless windowView is flipped.
            // In those cases, the Y coordinate gets flipped by Widget::convertFromContainingWindow.
            NSPoint location = [event locationInWindow];
            if (windowView)
                location = [windowView convertPoint:location fromView:nil];
            return location;
        }
        default:
            return NSZeroPoint;
    }
}

static inline String textFromEvent(NSEvent* event)
{
    if ([event type] == NSFlagsChanged)
        return String("");
    return String([event characters]);
}

static inline String unmodifiedTextFromEvent(NSEvent* event)
{
    if ([event type] == NSFlagsChanged)
        return String("");
    return String([event charactersIgnoringModifiers]);
}

static String keyIdentifierForKeyEvent(NSEvent* event)
{
    if ([event type] == NSFlagsChanged) 
        switch ([event keyCode]) {
            case 54: // Right Command
            case 55: // Left Command
                return String("Meta");
                
            case 57: // Capslock
                return String("CapsLock");
                
            case 56: // Left Shift
            case 60: // Right Shift
                return String("Shift");
                
            case 58: // Left Alt
            case 61: // Right Alt
                return String("Alt");
                
            case 59: // Left Ctrl
            case 62: // Right Ctrl
                return String("Control");
                
            default:
                ASSERT_NOT_REACHED();
                return String("");
        }
    
    NSString *s = [event charactersIgnoringModifiers];
    if ([s length] != 1)
        return String("Unidentified");

    unichar c = [s characterAtIndex:0];
    switch (c) {
        // Each identifier listed in the DOM spec is listed here.
        // Many are simply commented out since they do not appear on standard Macintosh keyboards
        // or are on a key that doesn't have a corresponding character.

        // "Accept"
        // "AllCandidates"

        // "Alt"
        case NSMenuFunctionKey:
            return String("Alt");

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
            return String("Clear");

        // "CodeInput"
        // "Compose"
        // "Control"
        // "Crsel"
        // "Convert"
        // "Copy"
        // "Cut"

        // "Down"
        case NSDownArrowFunctionKey:
            return String("Down");
        // "End"
        case NSEndFunctionKey:
            return String("End");
        // "Enter"
        case 0x3: case 0xA: case 0xD: // Macintosh calls the one on the main keyboard Return, but Windows calls it Enter, so we'll do the same for the DOM
            return String("Enter");

        // "EraseEof"

        // "Execute"
        case NSExecuteFunctionKey:
            return String("Execute");

        // "Exsel"

        // "F1"
        case NSF1FunctionKey:
            return String("F1");
        // "F2"
        case NSF2FunctionKey:
            return String("F2");
        // "F3"
        case NSF3FunctionKey:
            return String("F3");
        // "F4"
        case NSF4FunctionKey:
            return String("F4");
        // "F5"
        case NSF5FunctionKey:
            return String("F5");
        // "F6"
        case NSF6FunctionKey:
            return String("F6");
        // "F7"
        case NSF7FunctionKey:
            return String("F7");
        // "F8"
        case NSF8FunctionKey:
            return String("F8");
        // "F9"
        case NSF9FunctionKey:
            return String("F9");
        // "F10"
        case NSF10FunctionKey:
            return String("F10");
        // "F11"
        case NSF11FunctionKey:
            return String("F11");
        // "F12"
        case NSF12FunctionKey:
            return String("F12");
        // "F13"
        case NSF13FunctionKey:
            return String("F13");
        // "F14"
        case NSF14FunctionKey:
            return String("F14");
        // "F15"
        case NSF15FunctionKey:
            return String("F15");
        // "F16"
        case NSF16FunctionKey:
            return String("F16");
        // "F17"
        case NSF17FunctionKey:
            return String("F17");
        // "F18"
        case NSF18FunctionKey:
            return String("F18");
        // "F19"
        case NSF19FunctionKey:
            return String("F19");
        // "F20"
        case NSF20FunctionKey:
            return String("F20");
        // "F21"
        case NSF21FunctionKey:
            return String("F21");
        // "F22"
        case NSF22FunctionKey:
            return String("F22");
        // "F23"
        case NSF23FunctionKey:
            return String("F23");
        // "F24"
        case NSF24FunctionKey:
            return String("F24");

        // "FinalMode"

        // "Find"
        case NSFindFunctionKey:
            return String("Find");

        // "FullWidth"
        // "HalfWidth"
        // "HangulMode"
        // "HanjaMode"

        // "Help"
        case NSHelpFunctionKey:
            return String("Help");

        // "Hiragana"

        // "Home"
        case NSHomeFunctionKey:
            return String("Home");
        // "Insert"
        case NSInsertFunctionKey:
            return String("Insert");

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
            return String("Left");

        // "Meta"
        // "MediaNextTrack"
        // "MediaPlayPause"
        // "MediaPreviousTrack"
        // "MediaStop"

        // "ModeChange"
        case NSModeSwitchFunctionKey:
            return String("ModeChange");

        // "Nonconvert"
        // "NumLock"

        // "PageDown"
        case NSPageDownFunctionKey:
            return String("PageDown");
        // "PageUp"
        case NSPageUpFunctionKey:
            return String("PageUp");

        // "Paste"

        // "Pause"
        case NSPauseFunctionKey:
            return String("Pause");

        // "Play"
        // "PreviousCandidate"

        // "PrintScreen"
        case NSPrintScreenFunctionKey:
            return String("PrintScreen");

        // "Process"
        // "Props"

        // "Right"
        case NSRightArrowFunctionKey:
            return String("Right");

        // "RomanCharacters"

        // "Scroll"
        case NSScrollLockFunctionKey:
            return String("Scroll");
        // "Select"
        case NSSelectFunctionKey:
            return String("Select");

        // "SelectMedia"
        // "Shift"

        // "Stop"
        case NSStopFunctionKey:
            return String("Stop");
        // "Up"
        case NSUpArrowFunctionKey:
            return String("Up");
        // "Undo"
        case NSUndoFunctionKey:
            return String("Undo");

        // "VolumeDown"
        // "VolumeMute"
        // "VolumeUp"
        // "Win"
        // "Zoom"

        // More function keys, not in the key identifier specification.
        case NSF25FunctionKey:
            return String("F25");
        case NSF26FunctionKey:
            return String("F26");
        case NSF27FunctionKey:
            return String("F27");
        case NSF28FunctionKey:
            return String("F28");
        case NSF29FunctionKey:
            return String("F29");
        case NSF30FunctionKey:
            return String("F30");
        case NSF31FunctionKey:
            return String("F31");
        case NSF32FunctionKey:
            return String("F32");
        case NSF33FunctionKey:
            return String("F33");
        case NSF34FunctionKey:
            return String("F34");
        case NSF35FunctionKey:
            return String("F35");

        // Turn 0x7F into 0x08, because backspace needs to always be 0x08.
        case 0x7F:
            return String("U+0008");
        // Standard says that DEL becomes U+007F.
        case NSDeleteFunctionKey:
            return String("U+007F");
            
        // Always use 0x09 for tab instead of AppKit's backtab character.
        case NSBackTabCharacter:
            return String("U+0009");

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
            return String::format("U+%04X", toASCIIUpper(c));
    }
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
    switch ([event keyCode]) {
        // VK_TAB (09) TAB key
        case 48: return 0x09;

        // VK_APPS (5D) Right windows/meta key
        case 54: // Right Command
            return 0x5D;
            
        // VK_LWIN (5B) Left windows/meta key
        case 55: // Left Command
            return 0x5B;
            
        // VK_CAPITAL (14) caps locks key
        case 57: // Capslock
            return 0x14;
            
        // VK_SHIFT (10) either shift key
        case 56: // Left Shift
        case 60: // Right Shift
            return 0x10;
            
        // VK_MENU (12) either alt key
        case 58: // Left Alt
        case 61: // Right Alt
            return 0x12;
            
        // VK_CONTROL (11) either ctrl key
        case 59: // Left Ctrl
        case 62: // Right Ctrl
            return 0x11;
            
        // VK_CLEAR (0C) CLEAR key
        case 71: return 0x0C;

        // VK_NUMPAD0 (60) Numeric keypad 0 key
        case 82: return 0x60;
        // VK_NUMPAD1 (61) Numeric keypad 1 key
        case 83: return 0x61;
        // VK_NUMPAD2 (62) Numeric keypad 2 key
        case 84: return 0x62;
        // VK_NUMPAD3 (63) Numeric keypad 3 key
        case 85: return 0x63;
        // VK_NUMPAD4 (64) Numeric keypad 4 key
        case 86: return 0x64;
        // VK_NUMPAD5 (65) Numeric keypad 5 key
        case 87: return 0x65;
        // VK_NUMPAD6 (66) Numeric keypad 6 key
        case 88: return 0x66;
        // VK_NUMPAD7 (67) Numeric keypad 7 key
        case 89: return 0x67;
        // VK_NUMPAD8 (68) Numeric keypad 8 key
        case 91: return 0x68;
        // VK_NUMPAD9 (69) Numeric keypad 9 key
        case 92: return 0x69;
        // VK_MULTIPLY (6A) Multiply key
        case 67: return 0x6A;
        // VK_ADD (6B) Add key
        case 69: return 0x6B;

        // VK_SUBTRACT (6D) Subtract key
        case 78: return 0x6D;
        // VK_DECIMAL (6E) Decimal key
        case 65: return 0x6E;
        // VK_DIVIDE (6F) Divide key
        case 75: return 0x6F;
     }

    NSString* s = [event charactersIgnoringModifiers];
    if ([s length] != 1)
        return 0;

    switch ([s characterAtIndex:0]) {
        // VK_LBUTTON (01) Left mouse button
        // VK_RBUTTON (02) Right mouse button
        // VK_CANCEL (03) Control-break processing
        // VK_MBUTTON (04) Middle mouse button (three-button mouse)
        // VK_XBUTTON1 (05)
        // VK_XBUTTON2 (06)

        // VK_BACK (08) BACKSPACE key
        case 8: case 0x7F: return 0x08;
        // VK_TAB (09) TAB key
        case 9: return 0x09;

        // VK_CLEAR (0C) CLEAR key
        // handled by key code above

        // VK_RETURN (0D)
        case 0xD: case 3: return 0x0D;

        // VK_SHIFT (10) SHIFT key
        // VK_CONTROL (11) CTRL key
        // VK_MENU (12) ALT key

        // VK_PAUSE (13) PAUSE key
        case NSPauseFunctionKey: return 0x13;

        // VK_CAPITAL (14) CAPS LOCK key
        // VK_KANA (15) Input Method Editor (IME) Kana mode
        // VK_HANGUEL (15) IME Hanguel mode (maintained for compatibility; use VK_HANGUL)
        // VK_HANGUL (15) IME Hangul mode
        // VK_JUNJA (17) IME Junja mode
        // VK_FINAL (18) IME final mode
        // VK_HANJA (19) IME Hanja mode
        // VK_KANJI (19) IME Kanji mode

        // VK_ESCAPE (1B) ESC key
        case 0x1B: return 0x1B;

        // VK_CONVERT (1C) IME convert
        // VK_NONCONVERT (1D) IME nonconvert
        // VK_ACCEPT (1E) IME accept
        // VK_MODECHANGE (1F) IME mode change request

        // VK_SPACE (20) SPACEBAR
        case ' ': return 0x20;
        // VK_PRIOR (21) PAGE UP key
        case NSPageUpFunctionKey: return 0x21;
        // VK_NEXT (22) PAGE DOWN key
        case NSPageDownFunctionKey: return 0x22;
        // VK_END (23) END key
        case NSEndFunctionKey: return 0x23;
        // VK_HOME (24) HOME key
        case NSHomeFunctionKey: return 0x24;
        // VK_LEFT (25) LEFT ARROW key
        case NSLeftArrowFunctionKey: return 0x25;
        // VK_UP (26) UP ARROW key
        case NSUpArrowFunctionKey: return 0x26;
        // VK_RIGHT (27) RIGHT ARROW key
        case NSRightArrowFunctionKey: return 0x27;
        // VK_DOWN (28) DOWN ARROW key
        case NSDownArrowFunctionKey: return 0x28;
        // VK_SELECT (29) SELECT key
        case NSSelectFunctionKey: return 0x29;
        // VK_PRINT (2A) PRINT key
        case NSPrintFunctionKey: return 0x2A;
        // VK_EXECUTE (2B) EXECUTE key
        case NSExecuteFunctionKey: return 0x2B;
        // VK_SNAPSHOT (2C) PRINT SCREEN key
        case NSPrintScreenFunctionKey: return 0x2C;
        // VK_INSERT (2D) INS key
        case NSInsertFunctionKey: case NSHelpFunctionKey: return 0x2D;
        // VK_DELETE (2E) DEL key
        case NSDeleteFunctionKey: return 0x2E;

        // VK_HELP (2F) HELP key

        //  (30) 0 key
        case '0': case ')': return 0x30;
        //  (31) 1 key
        case '1': case '!': return 0x31;
        //  (32) 2 key
        case '2': case '@': return 0x32;
        //  (33) 3 key
        case '3': case '#': return 0x33;
        //  (34) 4 key
        case '4': case '$': return 0x34;
        //  (35) 5 key
        case '5': case '%': return 0x35;
        //  (36) 6 key
        case '6': case '^': return 0x36;
        //  (37) 7 key
        case '7': case '&': return 0x37;
        //  (38) 8 key
        case '8': case '*': return 0x38;
        //  (39) 9 key
        case '9': case '(': return 0x39;
        //  (41) A key
        case 'a': case 'A': return 0x41;
        //  (42) B key
        case 'b': case 'B': return 0x42;
        //  (43) C key
        case 'c': case 'C': return 0x43;
        //  (44) D key
        case 'd': case 'D': return 0x44;
        //  (45) E key
        case 'e': case 'E': return 0x45;
        //  (46) F key
        case 'f': case 'F': return 0x46;
        //  (47) G key
        case 'g': case 'G': return 0x47;
        //  (48) H key
        case 'h': case 'H': return 0x48;
        //  (49) I key
        case 'i': case 'I': return 0x49;
        //  (4A) J key
        case 'j': case 'J': return 0x4A;
        //  (4B) K key
        case 'k': case 'K': return 0x4B;
        //  (4C) L key
        case 'l': case 'L': return 0x4C;
        //  (4D) M key
        case 'm': case 'M': return 0x4D;
        //  (4E) N key
        case 'n': case 'N': return 0x4E;
        //  (4F) O key
        case 'o': case 'O': return 0x4F;
        //  (50) P key
        case 'p': case 'P': return 0x50;
        //  (51) Q key
        case 'q': case 'Q': return 0x51;
        //  (52) R key
        case 'r': case 'R': return 0x52;
        //  (53) S key
        case 's': case 'S': return 0x53;
        //  (54) T key
        case 't': case 'T': return 0x54;
        //  (55) U key
        case 'u': case 'U': return 0x55;
        //  (56) V key
        case 'v': case 'V': return 0x56;
        //  (57) W key
        case 'w': case 'W': return 0x57;
        //  (58) X key
        case 'x': case 'X': return 0x58;
        //  (59) Y key
        case 'y': case 'Y': return 0x59;
        //  (5A) Z key
        case 'z': case 'Z': return 0x5A;

        // VK_LWIN (5B) Left Windows key (Microsoft Natural keyboard)
        // VK_RWIN (5C) Right Windows key (Natural keyboard)
        // VK_APPS (5D) Applications key (Natural keyboard)
        // VK_SLEEP (5F) Computer Sleep key

        // VK_NUMPAD0 (60) Numeric keypad 0 key
        // VK_NUMPAD1 (61) Numeric keypad 1 key
        // VK_NUMPAD2 (62) Numeric keypad 2 key
        // VK_NUMPAD3 (63) Numeric keypad 3 key
        // VK_NUMPAD4 (64) Numeric keypad 4 key
        // VK_NUMPAD5 (65) Numeric keypad 5 key
        // VK_NUMPAD6 (66) Numeric keypad 6 key
        // VK_NUMPAD7 (67) Numeric keypad 7 key
        // VK_NUMPAD8 (68) Numeric keypad 8 key
        // VK_NUMPAD9 (69) Numeric keypad 9 key
        // VK_MULTIPLY (6A) Multiply key
        // VK_ADD (6B) Add key
        // handled by key code above

        // VK_SEPARATOR (6C) Separator key

        // VK_SUBTRACT (6D) Subtract key
        // VK_DECIMAL (6E) Decimal key
        // VK_DIVIDE (6F) Divide key
        // handled by key code above

        // VK_F1 (70) F1 key
        case NSF1FunctionKey: return 0x70;
        // VK_F2 (71) F2 key
        case NSF2FunctionKey: return 0x71;
        // VK_F3 (72) F3 key
        case NSF3FunctionKey: return 0x72;
        // VK_F4 (73) F4 key
        case NSF4FunctionKey: return 0x73;
        // VK_F5 (74) F5 key
        case NSF5FunctionKey: return 0x74;
        // VK_F6 (75) F6 key
        case NSF6FunctionKey: return 0x75;
        // VK_F7 (76) F7 key
        case NSF7FunctionKey: return 0x76;
        // VK_F8 (77) F8 key
        case NSF8FunctionKey: return 0x77;
        // VK_F9 (78) F9 key
        case NSF9FunctionKey: return 0x78;
        // VK_F10 (79) F10 key
        case NSF10FunctionKey: return 0x79;
        // VK_F11 (7A) F11 key
        case NSF11FunctionKey: return 0x7A;
        // VK_F12 (7B) F12 key
        case NSF12FunctionKey: return 0x7B;
        // VK_F13 (7C) F13 key
        case NSF13FunctionKey: return 0x7C;
        // VK_F14 (7D) F14 key
        case NSF14FunctionKey: return 0x7D;
        // VK_F15 (7E) F15 key
        case NSF15FunctionKey: return 0x7E;
        // VK_F16 (7F) F16 key
        case NSF16FunctionKey: return 0x7F;
        // VK_F17 (80H) F17 key
        case NSF17FunctionKey: return 0x80;
        // VK_F18 (81H) F18 key
        case NSF18FunctionKey: return 0x81;
        // VK_F19 (82H) F19 key
        case NSF19FunctionKey: return 0x82;
        // VK_F20 (83H) F20 key
        case NSF20FunctionKey: return 0x83;
        // VK_F21 (84H) F21 key
        case NSF21FunctionKey: return 0x84;
        // VK_F22 (85H) F22 key
        case NSF22FunctionKey: return 0x85;
        // VK_F23 (86H) F23 key
        case NSF23FunctionKey: return 0x86;
        // VK_F24 (87H) F24 key
        case NSF24FunctionKey: return 0x87;

        // VK_NUMLOCK (90) NUM LOCK key

        // VK_SCROLL (91) SCROLL LOCK key
        case NSScrollLockFunctionKey: return 0x91;

        // VK_LSHIFT (A0) Left SHIFT key
        // VK_RSHIFT (A1) Right SHIFT key
        // VK_LCONTROL (A2) Left CONTROL key
        // VK_RCONTROL (A3) Right CONTROL key
        // VK_LMENU (A4) Left MENU key
        // VK_RMENU (A5) Right MENU key
        // VK_BROWSER_BACK (A6) Windows 2000/XP: Browser Back key
        // VK_BROWSER_FORWARD (A7) Windows 2000/XP: Browser Forward key
        // VK_BROWSER_REFRESH (A8) Windows 2000/XP: Browser Refresh key
        // VK_BROWSER_STOP (A9) Windows 2000/XP: Browser Stop key
        // VK_BROWSER_SEARCH (AA) Windows 2000/XP: Browser Search key
        // VK_BROWSER_FAVORITES (AB) Windows 2000/XP: Browser Favorites key
        // VK_BROWSER_HOME (AC) Windows 2000/XP: Browser Start and Home key
        // VK_VOLUME_MUTE (AD) Windows 2000/XP: Volume Mute key
        // VK_VOLUME_DOWN (AE) Windows 2000/XP: Volume Down key
        // VK_VOLUME_UP (AF) Windows 2000/XP: Volume Up key
        // VK_MEDIA_NEXT_TRACK (B0) Windows 2000/XP: Next Track key
        // VK_MEDIA_PREV_TRACK (B1) Windows 2000/XP: Previous Track key
        // VK_MEDIA_STOP (B2) Windows 2000/XP: Stop Media key
        // VK_MEDIA_PLAY_PAUSE (B3) Windows 2000/XP: Play/Pause Media key
        // VK_LAUNCH_MAIL (B4) Windows 2000/XP: Start Mail key
        // VK_LAUNCH_MEDIA_SELECT (B5) Windows 2000/XP: Select Media key
        // VK_LAUNCH_APP1 (B6) Windows 2000/XP: Start Application 1 key
        // VK_LAUNCH_APP2 (B7) Windows 2000/XP: Start Application 2 key

        // VK_OEM_1 (BA) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ';:' key
        case ';': case ':': return 0xBA;
        // VK_OEM_PLUS (BB) Windows 2000/XP: For any country/region, the '+' key
        case '=': case '+': return 0xBB;
        // VK_OEM_COMMA (BC) Windows 2000/XP: For any country/region, the ',' key
        case ',': case '<': return 0xBC;
        // VK_OEM_MINUS (BD) Windows 2000/XP: For any country/region, the '-' key
        case '-': case '_': return 0xBD;
        // VK_OEM_PERIOD (BE) Windows 2000/XP: For any country/region, the '.' key
        case '.': case '>': return 0xBE;
        // VK_OEM_2 (BF) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '/?' key
        case '/': case '?': return 0xBF;
        // VK_OEM_3 (C0) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '`~' key
        case '`': case '~': return 0xC0;
        // VK_OEM_4 (DB) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '[{' key
        case '[': case '{': return 0xDB;
        // VK_OEM_5 (DC) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '\|' key
        case '\\': case '|': return 0xDC;
        // VK_OEM_6 (DD) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ']}' key
        case ']': case '}': return 0xDD;
        // VK_OEM_7 (DE) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the 'single-quote/double-quote' key
        case '\'': case '"': return 0xDE;

        // VK_OEM_8 (DF) Used for miscellaneous characters; it can vary by keyboard.
        // VK_OEM_102 (E2) Windows 2000/XP: Either the angle bracket key or the backslash key on the RT 102-key keyboard
        // VK_PROCESSKEY (E5) Windows 95/98/Me, Windows NT 4.0, Windows 2000/XP: IME PROCESS key
        // VK_PACKET (E7) Windows 2000/XP: Used to pass Unicode characters as if they were keystrokes. The VK_PACKET key is the low word of a 32-bit Virtual Key value used for non-keyboard input methods. For more information, see Remark in KEYBDINPUT,SendInput, WM_KEYDOWN, and WM_KEYUP
        // VK_ATTN (F6) Attn key
        // VK_CRSEL (F7) CrSel key
        // VK_EXSEL (F8) ExSel key
        // VK_EREOF (F9) Erase EOF key
        // VK_PLAY (FA) Play key
        // VK_ZOOM (FB) Zoom key
        // VK_NONAME (FC) Reserved for future use
        // VK_PA1 (FD) PA1 key
        // VK_OEM_CLEAR (FE) Clear key
    }

    return 0;
}

static inline bool isKeyUpEvent(NSEvent *event)
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

static inline WebEvent::Modifiers modifiersForEvent(NSEvent *event)
{
    unsigned modifiers = 0;
    if ([event modifierFlags] & NSShiftKeyMask)
        modifiers |= WebEvent::ShiftKey;
    if ([event modifierFlags] & NSControlKeyMask)
        modifiers |= WebEvent::ControlKey;
    if ([event modifierFlags] & NSAlternateKeyMask)
        modifiers |= WebEvent::AltKey;
    if ([event modifierFlags] & NSCommandKeyMask)
        modifiers |= WebEvent::MetaKey;
    return (WebEvent::Modifiers)modifiers;
}

WebMouseEvent WebEventFactory::createWebMouseEvent(NSEvent *event, NSView *windowView)
{
    NSPoint position = pointForEvent(event, windowView);
    NSPoint globalPosition = globalPointForEvent(event);

    WebEvent::Type type                     = mouseEventTypeForEvent(event);
    WebMouseEvent::Button button            = mouseButtonForEvent(event);
    int positionX                           = position.x;
    int positionY                           = position.y;
    int globalPositionX                     = globalPosition.x;
    int globalPositionY                     = globalPosition.y;
    float deltaX                            = [event deltaX];
    float deltaY                            = [event deltaY];
    float deltaZ                            = [event deltaZ];
    int clickCount                          = clickCountForEvent(event);
    WebEvent::Modifiers modifiers           = modifiersForEvent(event);
    double timestamp                        = [event timestamp];

    return WebMouseEvent(type, button, positionX, positionY, globalPositionX, globalPositionY, deltaX, deltaY, deltaZ, clickCount, modifiers, timestamp);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(NSEvent *event, NSView *windowView)
{
    // Taken from WebCore
    const int cScrollbarPixelsPerLineStep = 40;

    NSPoint position = pointForEvent(event, windowView);
    NSPoint globalPosition = globalPointForEvent(event);

    int positionX                           = position.x;
    int positionY                           = position.y;
    int globalPositionX                     = globalPosition.x;
    int globalPositionY                     = globalPosition.y;
    WebWheelEvent::Granularity granularity  = WebWheelEvent::ScrollByPixelWheelEvent;

    float deltaX = 0;
    float deltaY = 0;
    float wheelTicksX = 0;
    float wheelTicksY = 0;
    if ([event _continuousScroll]) {
        // smooth scroll events
        deltaX = [event deviceDeltaX];
        deltaY = [event deviceDeltaY];
        wheelTicksX = deltaX / static_cast<float>(cScrollbarPixelsPerLineStep);
        wheelTicksY = deltaY / static_cast<float>(cScrollbarPixelsPerLineStep);
    } else {
        // plain old wheel events
        deltaX = [event deltaX];
        deltaY = [event deltaY];
        wheelTicksX = deltaX;
        wheelTicksY = deltaY;
        deltaX *= static_cast<float>(cScrollbarPixelsPerLineStep);
        deltaY *= static_cast<float>(cScrollbarPixelsPerLineStep);
    }

    WebEvent::Modifiers modifiers           = modifiersForEvent(event);
    double timestamp                        = [event timestamp];

    return WebWheelEvent(WebEvent::Wheel, positionX, positionY, globalPositionX, globalPositionY, deltaX, deltaY, wheelTicksX, wheelTicksY, granularity, modifiers, timestamp);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(NSEvent *event, NSView *)
{
    WebEvent::Type type             = isKeyUpEvent(event) ? WebEvent::KeyUp : WebEvent::KeyDown;
    String text                     = textFromEvent(event);
    String unmodifiedText           = unmodifiedTextFromEvent(event);
    String keyIdentifier            = keyIdentifierForKeyEvent(event);
    int windowsVirtualKeyCode       = windowsKeyCodeForKeyEvent(event);
    int nativeVirtualKeyCode        = [event keyCode];
    bool autoRepeat                 = ([event type] != NSFlagsChanged) && [event isARepeat];
    bool isKeypad                   = isKeypadEvent(event);
    bool isSystemKey                = false; // SystemKey is always false on the Mac.
    WebEvent::Modifiers modifiers   = modifiersForEvent(event);
    double timestamp                = [event timestamp];

    return WebKeyboardEvent(type, text, unmodifiedText, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, autoRepeat, isKeypad, isSystemKey, modifiers, timestamp);
}

} // namespace WebKit
