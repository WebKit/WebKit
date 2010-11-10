/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebInputEvent_h
#define WebInputEvent_h

#include "WebCommon.h"
#include "WebTouchPoint.h"

#include <string.h>

namespace WebKit {

// The classes defined in this file are intended to be used with
// WebWidget's handleInputEvent method.  These event types are cross-
// platform and correspond closely to WebCore's Platform*Event classes.
//
// WARNING! These classes must remain PODs (plain old data).  They are
// intended to be "serializable" by copying their raw bytes, so they must
// not contain any non-bit-copyable member variables!

// WebInputEvent --------------------------------------------------------------

class WebInputEvent {
public:
    WebInputEvent(unsigned sizeParam = sizeof(WebInputEvent))
        : size(sizeParam)
        , type(Undefined)
        , modifiers(0)
        , timeStampSeconds(0.0) { }

    // When we use an input method (or an input method editor), we receive
    // two events for a keypress. The former event is a keydown, which
    // provides a keycode, and the latter is a textinput, which provides
    // a character processed by an input method. (The mapping from a
    // keycode to a character code is not trivial for non-English
    // keyboards.)
    // To support input methods, Safari sends keydown events to WebKit for
    // filtering. WebKit sends filtered keydown events back to Safari,
    // which sends them to input methods.
    // Unfortunately, it is hard to apply this design to Chrome because of
    // our multiprocess architecture. An input method is running in a
    // browser process. On the other hand, WebKit is running in a renderer
    // process. So, this design results in increasing IPC messages.
    // To support input methods without increasing IPC messages, Chrome
    // handles keyboard events in a browser process and send asynchronous
    // input events (to be translated to DOM events) to a renderer
    // process.
    // This design is mostly the same as the one of Windows and Mac Carbon.
    // So, for what it's worth, our Linux and Mac front-ends emulate our
    // Windows front-end. To emulate our Windows front-end, we can share
    // our back-end code among Windows, Linux, and Mac.
    // TODO(hbono): Issue 18064: remove the KeyDown type since it isn't
    // used in Chrome any longer.

    enum Type {
        Undefined = -1,

        // WebMouseEvent
        MouseDown,
        MouseUp,
        MouseMove,
        MouseEnter,
        MouseLeave,
        ContextMenu,

        // WebMouseWheelEvent
        MouseWheel,

        // WebKeyboardEvent
        RawKeyDown,
        KeyDown,
        KeyUp,
        Char,

        // WebTouchEvent
        TouchStart,
        TouchMove,
        TouchEnd,
        TouchCancel,
    };

    enum Modifiers {
        // modifiers for all events:
        ShiftKey         = 1 << 0,
        ControlKey       = 1 << 1,
        AltKey           = 1 << 2,
        MetaKey          = 1 << 3,

        // modifiers for keyboard events:
        IsKeyPad         = 1 << 4,
        IsAutoRepeat     = 1 << 5,

        // modifiers for mouse events:
        LeftButtonDown   = 1 << 6,
        MiddleButtonDown = 1 << 7,
        RightButtonDown  = 1 << 8,

        // Toggle modifiers for all events. Danger: these are not reflected
        // into WebCore, so round-tripping from WebInputEvent to a WebCore
        // event and back will not preserve these flags.
        CapsLockOn       = 1 << 9,
        NumLockOn        = 1 << 10,
    };

    static const int InputModifiers = ShiftKey | ControlKey | AltKey | MetaKey;

    unsigned size;   // The size of this structure, for serialization.
    Type type;
    int modifiers;
    double timeStampSeconds;   // Seconds since epoch.

    // Returns true if the WebInputEvent |type| is a mouse event.
    static bool isMouseEventType(int type)
    {
        return type == MouseDown
            || type == MouseUp
            || type == MouseMove
            || type == MouseEnter
            || type == MouseLeave
            || type == ContextMenu;
    }

    // Returns true if the WebInputEvent |type| is a keyboard event.
    static bool isKeyboardEventType(int type)
    {
        return type == RawKeyDown
            || type == KeyDown
            || type == KeyUp
            || type == Char;
    }

    // Returns true if the WebInputEvent |type| is a touch event.
    static bool isTouchEventType(int type)
    {
        return type == TouchStart
            || type == TouchMove
            || type == TouchEnd
            || type == TouchCancel;
    }
};

// WebKeyboardEvent -----------------------------------------------------------

class WebKeyboardEvent : public WebInputEvent {
public:
    // Caps on string lengths so we can make them static arrays and keep
    // them PODs.
    static const size_t textLengthCap = 4;

    // http://www.w3.org/TR/DOM-Level-3-Events/keyset.html lists the
    // identifiers.  The longest is 18 characters, so we round up to the
    // next multiple of 4.
    static const size_t keyIdentifierLengthCap = 20;

    // |windowsKeyCode| is the Windows key code associated with this key
    // event.  Sometimes it's direct from the event (i.e. on Windows),
    // sometimes it's via a mapping function.  If you want a list, see
    // WebCore/platform/chromium/KeyboardCodes* .
    int windowsKeyCode;

    // The actual key code genenerated by the platform.  The DOM spec runs
    // on Windows-equivalent codes (thus |windowsKeyCode| above) but it
    // doesn't hurt to have this one around.
    int nativeKeyCode;

    // |text| is the text generated by this keystroke.  |unmodifiedText| is
    // |text|, but unmodified by an concurrently-held modifiers (except
    // shift).  This is useful for working out shortcut keys.  Linux and
    // Windows guarantee one character per event.  The Mac does not, but in
    // reality that's all it ever gives.  We're generous, and cap it a bit
    // longer.
    WebUChar text[textLengthCap];
    WebUChar unmodifiedText[textLengthCap];

    // This is a string identifying the key pressed.
    char keyIdentifier[keyIdentifierLengthCap];

    // This identifies whether this event was tagged by the system as being
    // a "system key" event (see
    // http://msdn.microsoft.com/en-us/library/ms646286(VS.85).aspx for
    // details).  Other platforms don't have this concept, but it's just
    // easier to leave it always false than ifdef.
    // int is used instead of bool to ensure the size of this structure is
    // strictly aligned to a factor of 4 bytes, otherwise memory check tools
    // like valgrind may complain about uninitialized memory usage when
    // transfering it over the wire.
    int isSystemKey;

    WebKeyboardEvent(unsigned sizeParam = sizeof(WebKeyboardEvent))
        : WebInputEvent(sizeParam)
        , windowsKeyCode(0)
        , nativeKeyCode(0)
        , isSystemKey(false)
    {
        memset(&text, 0, sizeof(text));
        memset(&unmodifiedText, 0, sizeof(unmodifiedText));
        memset(&keyIdentifier, 0, sizeof(keyIdentifier));
    }

    // Sets keyIdentifier based on the value of windowsKeyCode.  This is
    // handy for generating synthetic keyboard events.
    WEBKIT_API void setKeyIdentifierFromWindowsKeyCode();
};

// WebMouseEvent --------------------------------------------------------------

class WebMouseEvent : public WebInputEvent {
public:
    // These values defined for WebCore::MouseButton
    enum Button {
        ButtonNone = -1,
        ButtonLeft,
        ButtonMiddle,
        ButtonRight
    };

    Button button;
    int x;
    int y;
    int windowX;
    int windowY;
    int globalX;
    int globalY;
    int clickCount;

    WebMouseEvent(unsigned sizeParam = sizeof(WebMouseEvent))
        : WebInputEvent(sizeParam)
        , button(ButtonNone)
        , x(0)
        , y(0)
        , windowX(0)
        , windowY(0)
        , globalX(0)
        , globalY(0)
        , clickCount(0)
    {
    }
};

// WebMouseWheelEvent ---------------------------------------------------------

class WebMouseWheelEvent : public WebMouseEvent {
public:
    float deltaX;
    float deltaY;
    float wheelTicksX;
    float wheelTicksY;

    // int is used instead of bool to ensure the size of this structure is
    // strictly aligned to a factor of 4 bytes, otherwise memory check tools
    // like valgrind may complain about uninitialized memory usage when
    // transfering it over the wire.
    int scrollByPage;

    WebMouseWheelEvent(unsigned sizeParam = sizeof(WebMouseWheelEvent))
        : WebMouseEvent(sizeParam)
        , deltaX(0.0f)
        , deltaY(0.0f)
        , wheelTicksX(0.0f)
        , wheelTicksY(0.0f)
        , scrollByPage(false)
    {
    }
};

// WebTouchEvent --------------------------------------------------------------

class WebTouchEvent : public WebInputEvent {
public:
    static const int touchPointsLengthCap = 4;

    int touchPointsLength;
    WebTouchPoint touchPoints[touchPointsLengthCap];

    WebTouchEvent(unsigned sizeParam = sizeof(WebTouchEvent))
        : WebInputEvent(sizeParam)
        , touchPointsLength(0)
    {
    }
};

} // namespace WebKit

#endif
