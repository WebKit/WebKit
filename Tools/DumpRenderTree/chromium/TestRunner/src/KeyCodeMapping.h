/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef KeyCodeMapping_h
#define KeyCodeMapping_h

namespace WebTestRunner {

// The keycodes match the values of the virtual keycodes found here http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx
enum {
    VKEY_RETURN   = 0x0D,
    VKEY_ESCAPE   = 0x1B,
    VKEY_PRIOR    = 0x21,
    VKEY_NEXT     = 0x22,
    VKEY_END      = 0x23,
    VKEY_HOME     = 0x24,
    VKEY_LEFT     = 0x25,
    VKEY_UP       = 0x26,
    VKEY_RIGHT    = 0x27,
    VKEY_DOWN     = 0x28,
    VKEY_SNAPSHOT = 0x2C,
    VKEY_INSERT   = 0x2D,
    VKEY_DELETE   = 0x2E,
    VKEY_APPS     = 0x5D,
    VKEY_F1       = 0x70,
    VKEY_LSHIFT   = 0xA0,
    VKEY_RSHIFT   = 0xA1,
    VKEY_LCONTROL = 0xA2,
    VKEY_RCONTROL = 0xA3,
    VKEY_LMENU    = 0xA4,
    VKEY_RMENU    = 0xA5,
};

// Map a windows keycode to a native keycode on OS(LINUX) && USE(GTK).
int NativeKeyCodeForWindowsKeyCode(int keysym);

}

#endif // KeyCodeMapping_h
