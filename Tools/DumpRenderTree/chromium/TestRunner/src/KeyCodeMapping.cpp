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

#include "config.h"
#include "KeyCodeMapping.h"

namespace WebTestRunner {

int NativeKeyCodeForWindowsKeyCode(int keysym)
{
#if defined(__linux__) && USE(GTK)
    // See /usr/share/X11/xkb/keycodes/*
    static const int asciiToKeyCode[] = {
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        22,
        23,
        0,
        0,
        0,
        36,
        0,
        0,
        50,
        37,
        64,
        127,
        66,
        0,
        0,
        0,
        0,
        131,
        0,
        9,
        100,
        102,
        0,
        0,
        65, // ' '
        112, // '!'
        117, // '"'
        115, // '#'
        110, // '$'
        113, // '%'
        111, // '&'
        114, // '''
        116, // '('
        0, // ')'
        107, // '*'
        0, // '+'
        0, // ','
        118, // '-'
        119, // '.'
        146, // '/'
        19, // '0'
        10, // '1'
        11, // '2'
        12, // '3'
        13, // '4'
        14, // '5'
        15, // '6'
        16, // '7'
        17, // '8'
        18, // '9'
        0, // ':'
        0, // ';'
        0, // '<'
        0, // '='
        0, // '>'
        0, // '?'
        0, // '@'
        38, // 'A'
        56, // 'B'
        54, // 'C'
        40, // 'D'
        26, // 'E'
        41, // 'F'
        42, // 'G'
        43, // 'H'
        31, // 'I'
        44, // 'J'
        45, // 'K'
        46, // 'L'
        58, // 'M'
        57, // 'N'
        32, // 'O'
        33, // 'P'
        24, // 'Q'
        27, // 'R'
        39, // 'S'
        28, // 'T'
        30, // 'U'
        55, // 'V'
        25, // 'W'
        53, // 'X'
        29, // 'Y'
        52, // 'Z'
        133, // '['
        134, // '\'
        135, // ']'
        0, // '^'
        0, // '_'
        90, // '`'
        38, // 'a'
        56, // 'b'
        54, // 'c'
        40, // 'd'
        26, // 'e'
        41, // 'f'
        42, // 'g'
        43, // 'h'
        31, // 'i'
        44, // 'j'
        45, // 'k'
        46, // 'l'
        58, // 'm'
        57, // 'n'
        32, // 'o'
        33, // 'p'
        24, // 'q'
        27, // 'r'
        39, // 's'
        28, // 't'
        30, // 'u'
        55, // 'v'
        25, // 'w'
        53, // 'x'
        29, // 'y'
        52, // 'z'
        96, // '{'
        0, // '|'
        0, // '}'
        0, // '~'
        0, // DEL
    };

    if (keysym <= 127)
        return asciiToKeyCode[keysym];

    switch (keysym) {
    case VKEY_PRIOR:
        return 112;
    case VKEY_NEXT:
        return 117;
    case VKEY_END:
        return 115;
    case VKEY_HOME:
        return 110;
    case VKEY_LEFT:
        return 113;
    case VKEY_UP:
        return 111;
    case VKEY_RIGHT:
        return 114;
    case VKEY_DOWN:
        return 116;
    case VKEY_SNAPSHOT:
        return 107;
    case VKEY_INSERT:
        return 118;
    case VKEY_DELETE:
        return 119;
    case VKEY_APPS:
        return 135;
    case VKEY_F1:
    case VKEY_F1 + 1:
    case VKEY_F1 + 2:
    case VKEY_F1 + 3:
    case VKEY_F1 + 4:
    case VKEY_F1 + 5:
    case VKEY_F1 + 6:
    case VKEY_F1 + 7:
    case VKEY_F1 + 8:
    case VKEY_F1 + 9:
    case VKEY_F1 + 10:
    case VKEY_F1 + 11:
    case VKEY_F1 + 12:
    case VKEY_F1 + 13:
    case VKEY_F1 + 14:
    case VKEY_F1 + 15:
    case VKEY_F1 + 16:
    case VKEY_F1 + 17:
    case VKEY_F1 + 18:
    case VKEY_F1 + 19:
    case VKEY_F1 + 20:
    case VKEY_F1 + 21:
    case VKEY_F1 + 22:
    case VKEY_F1 + 23:
        return 67 + (keysym - VKEY_F1);
    case VKEY_LSHIFT:
        return 50;
    case VKEY_RSHIFT:
        return 62;
    case VKEY_LCONTROL:
        return 37;
    case VKEY_RCONTROL:
        return 105;
    case VKEY_LMENU:
        return 64;
    case VKEY_RMENU:
        return 108;

    default:
        return 0;
    }
#else
    return keysym - keysym;
#endif
}

}
