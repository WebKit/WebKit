/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQEvent.h"
#import "KWQLogging.h"

QEvent::~QEvent()
{
}


QMouseEvent::QMouseEvent( Type t, const QPoint &pos, int b, int s )
    : QEvent(t), _position(pos), _button((ButtonState)b), _state((ButtonState)s)
{
}

QMouseEvent::QMouseEvent( Type t, const QPoint &pos, int b, int s, int cs )
    : QEvent(t), _position(pos), _button((ButtonState)b), _state((ButtonState)s), _clickCount(cs)
{
}

QMouseEvent::QMouseEvent(Type t, const QPoint &pos, const QPoint &, int b, int s)
    : QEvent(t), _position(pos), _button((ButtonState)b), _state((ButtonState)s)
{
}

Qt::ButtonState QMouseEvent::stateAfter()
{
    return _state;
}


QTimerEvent::QTimerEvent(int t)
    : QEvent(Timer)
{
    _timerId = t;
}

static char hexDigit(int i) {
    if (i < 0 || i > 16) {
        ERROR("illegal hex digit");
        return '0';
    }
    int h = i;
    if (h >= 10) {
        h = h - 10 + 'a'; 
    }
    else {
        h += '0';
    }
    return h;
}

static QString identifierForKeyText(const QString &text)
{
    int count = text.length();
    if (count == 0 || count > 1) {
#ifdef APPLE_CHANGES
        LOG(Events, "received an unexpected number of characters in key event: %d", count);
#endif
        return "Unidentified";
    }
    ushort c = text[0].unicode();
    switch (c) {
        // Each identifier listed in the DOM spec is listed here.
        // Many are simply commented out since they do not appear on standard Macintosh keyboards.
        // "Accept"
        // "AllCandidates"
        // "Alt"
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
            break;
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
            break;
        // "End"
        case NSEndFunctionKey:
            return "End";
            break;
        // "Enter"
        case 0x3:
            return "Enter";
            break;
        // "EraseEof"
        // "Execute"
        case NSExecuteFunctionKey:
            return "Execute";
            break;
        // "Exsel"
        // "F1"
        case NSF1FunctionKey:
            return "F1";
            break;
        // "F2"
        case NSF2FunctionKey:
            return "F2";
            break;
        // "F3"
        case NSF3FunctionKey:
            return "F3";
            break;
        // "F4"
        case NSF4FunctionKey:
            return "F4";
            break;
        // "F5"
        case NSF5FunctionKey:
            return "F5";
            break;
        // "F6"
        case NSF6FunctionKey:
            return "F6";
            break;
        // "F7"
        case NSF7FunctionKey:
            return "F7";
            break;
        // "F8"
        case NSF8FunctionKey:
            return "F8";
            break;
        // "F9"
        case NSF9FunctionKey:
            return "F9";
            break;
        // "F10"
        case NSF10FunctionKey:
            return "F10";
            break;
        // "F11"
        case NSF11FunctionKey:
            return "F11";
            break;
        // "F12"
        case NSF12FunctionKey:
            return "F12";
            break;
        // "F13"
        case NSF13FunctionKey:
            return "F13";
            break;
        // "F14"
        case NSF14FunctionKey:
            return "F14";
            break;
        // "F15"
        case NSF15FunctionKey:
            return "F15";
            break;
        // "F16"
        case NSF16FunctionKey:
            return "F16";
            break;
        // "F17"
        case NSF17FunctionKey:
            return "F17";
            break;
        // "F18"
        case NSF18FunctionKey:
            return "F18";
            break;
        // "F19"
        case NSF19FunctionKey:
            return "F19";
            break;
        // "F20"
        case NSF20FunctionKey:
            return "F20";
            break;
        // "F21"
        case NSF21FunctionKey:
            return "F21";
            break;
        // "F22"
        case NSF22FunctionKey:
            return "F22";
            break;
        // "F23"
        case NSF23FunctionKey:
            return "F23";
            break;
        // "F24"
        case NSF24FunctionKey:
            return "F24";
            break;
        // "FinalMode"
        // "Find"
        case NSFindFunctionKey:
            return "Find";
            break;
        // "ForwardDelete" (Non-standard)
        case NSDeleteFunctionKey:
            return "ForwardDelete";
            break;
        // "FullWidth"
        // "HalfWidth"
        // "HangulMode"
        // "HanjaMode"
        // "Help"
        case NSHelpFunctionKey:
            return "Help";
            break;
        // "Hiragana"
        // "Home"
        case NSHomeFunctionKey:
            return "Home";
            break;
        // "Insert"
        case NSInsertFunctionKey:
            return "Left";
            break;
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
            break;
        // "Meta"
        // "MediaNextTrack"
        // "MediaPlayPause"
        // "MediaPreviousTrack"
        // "MediaStop"
        // "ModeChange"
        case NSModeSwitchFunctionKey:
            return "ModeChange";
            break;
        // "Nonconvert"
        // "NumLock"
        // "PageDown"
        case NSPageDownFunctionKey:
            return "PageDown";
            break;
        // "PageUp"
        case NSPageUpFunctionKey:
            return "PageUp";
            break;
        // "Paste"
        // "Pause"
        case NSPauseFunctionKey:
            return "Pause";
            break;
        // "Play"
        // "PreviousCandidate"
        // "PrintScreen"
        case NSPrintScreenFunctionKey:
            return "PrintScreen";
            break;
        // "Process"
        // "Props"
        // "Right"
        case NSRightArrowFunctionKey:
            return "Right";
            break;
        // "RomanCharacters"
        // "Scroll"
        // "Select"
        // "SelectMedia"
        // "Shift"
        // "Stop"
        case NSStopFunctionKey:
            return "Stop";
            break;
        // "Up"
        case NSUpArrowFunctionKey:
            return "Up";
            break;
        // "Undo"
        case NSUndoFunctionKey:
            return "Undo";
            break;
        // "VolumeDown"
        // "VolumeMute"
        // "VolumeUp"
        // "Win"
        // "Zoom"
        default:
            char escaped[5];
            escaped[0] = hexDigit((c >> 12) & 0xf);
            escaped[1] = hexDigit((c >> 8) & 0xf);
            escaped[2] = hexDigit((c >> 4) & 0xf);
            escaped[3] = hexDigit(c & 0xf);
            escaped[4] = '\0';
            NSString *nsstring = [[NSString alloc] initWithFormat:@"U+00%s", escaped];
            QString qstring = QString::fromNSString(nsstring);
            [nsstring release];
            return qstring;
    }
}

QKeyEvent::QKeyEvent(Type t, int key, int ascii, int buttonState, const QString &text, bool autoRepeat, ushort count)
    : QEvent(t),
      _key(key),
      _ascii(ascii),
      _state((ButtonState)buttonState),
      _text(text),
      _autoRepeat(autoRepeat),
      _count(count),
      _isAccepted(false)
{
    _identifier = identifierForKeyText(text);
}

int QKeyEvent::key() const
{
    return _key;
}

Qt::ButtonState QKeyEvent::state() const
{
    return _state;
}

void QKeyEvent::accept()
{
    _isAccepted = true;
}

void QKeyEvent::ignore()
{
    _isAccepted = false;
}

bool QKeyEvent::isAutoRepeat() const
{
    return _autoRepeat;
}

QString QKeyEvent::text(void) const
{
    return _text;
}

int QKeyEvent::ascii(void) const
{
    return _ascii;
}

int QKeyEvent::count(void) const
{
    return _count;
}

bool QKeyEvent::isAccepted(void) const
{
    return _isAccepted;
}

QString QKeyEvent::identifier() const
{
    return _identifier;
}
