//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef SAMPLE_UTIL_KEYBOARD_H
#define SAMPLE_UTIL_KEYBOARD_H

namespace angle
{
enum class KeyType
{
    UNKNOWN,
    A,          // The A key
    B,          // The B key
    C,          // The C key
    D,          // The D key
    E,          // The E key
    F,          // The F key
    G,          // The G key
    H,          // The H key
    I,          // The I key
    J,          // The J key
    K,          // The K key
    L,          // The L key
    M,          // The M key
    N,          // The N key
    O,          // The O key
    P,          // The P key
    Q,          // The Q key
    R,          // The R key
    S,          // The S key
    T,          // The T key
    U,          // The U key
    V,          // The V key
    W,          // The W key
    X,          // The X key
    Y,          // The Y key
    Z,          // The Z key
    NUM0,       // The 0 key
    NUM1,       // The 1 key
    NUM2,       // The 2 key
    NUM3,       // The 3 key
    NUM4,       // The 4 key
    NUM5,       // The 5 key
    NUM6,       // The 6 key
    NUM7,       // The 7 key
    NUM8,       // The 8 key
    NUM9,       // The 9 key
    ESCAPE,     // The escape key
    LCONTROL,   // The left control key
    LSHIFT,     // The left shift key
    LALT,       // The left alt key
    LSYSTEM,    // The left OS specific key: Window (Windows and Linux), Apple (MacOS X), ...
    RCONTROL,   // The right control key
    RSHIFT,     // The right shift key
    RALT,       // The right alt key
    RSYSTEM,    // The right OS specific key: Window (Windows and Linux), Apple (MacOS X), ...
    MENU,       // The menu key
    LBRACKET,   // The [ key
    RBRACKET,   // The ] key
    SEMICOLON,  // The ; key
    COMMA,      // The , key
    PERIOD,     // The . key
    QUOTE,      // The ' key
    SLASH,      // The / key
    BACKSLASH,  // The \ key
    TILDE,      // The ~ key
    EQUAL,      // The = key
    DASH,       // The - key
    SPACE,      // The space key
    RETURN,     // The return key
    BACK,       // The backspace key
    TAB,        // The tabulation key
    PAGEUP,     // The page up key
    PAGEDOWN,   // The page down key
    END,        // The end key
    HOME,       // The home key
    INSERT,     // The insert key
    DEL,        // The delete key, Avoid Windows DELETE macro (#defined in <winnt.h>)
    ADD,        // +
    SUBTRACT,   // -
    MULTIPLY,   // *
    DIVIDE,     // /
    LEFT,       // Left arrow
    RIGHT,      // Right arrow
    UP,         // Up arrow
    DOWN,       // Down arrow
    NUMPAD0,    // The numpad 0 key
    NUMPAD1,    // The numpad 1 key
    NUMPAD2,    // The numpad 2 key
    NUMPAD3,    // The numpad 3 key
    NUMPAD4,    // The numpad 4 key
    NUMPAD5,    // The numpad 5 key
    NUMPAD6,    // The numpad 6 key
    NUMPAD7,    // The numpad 7 key
    NUMPAD8,    // The numpad 8 key
    NUMPAD9,    // The numpad 9 key
    F1,         // The F1 key
    F2,         // The F2 key
    F3,         // The F3 key
    F4,         // The F4 key
    F5,         // The F5 key
    F6,         // The F6 key
    F7,         // The F7 key
    F8,         // The F8 key
    F9,         // The F8 key
    F10,        // The F10 key
    F11,        // The F11 key
    F12,        // The F12 key
    F13,        // The F13 key
    F14,        // The F14 key
    F15,        // The F15 key
    PAUSE,      // The pause key
    COUNT,
};
}  // namespace angle

#endif  // SAMPLE_UTIL_KEYBOARD_H
