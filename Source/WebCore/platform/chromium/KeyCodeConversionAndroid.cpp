/*
 * Copyright 2007, The Android Open Source Project
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#include "KeyboardCodes.h"

#include <android/keycodes.h>

// The Android NDK does not provide values for these yet:
#define AKEYCODE_MEDIA_PAUSE 127
#define AKEYCODE_VOLUME_MUTE 164

namespace WebCore {

int windowsKeyCodeForKeyEvent(unsigned int keyCode)
{
    // Does not provide all key codes, and does not handle all keys.
    switch (keyCode) {
    case AKEYCODE_DEL:
        return VKEY_BACK;
    case AKEYCODE_TAB:
        return VKEY_TAB;
    case AKEYCODE_CLEAR:
        return VKEY_CLEAR;
    case AKEYCODE_DPAD_CENTER:
    case AKEYCODE_ENTER:
        return VKEY_RETURN;
    case AKEYCODE_SHIFT_LEFT:
    case AKEYCODE_SHIFT_RIGHT:
        return VKEY_SHIFT;
    // Back will serve as escape, although we may not have access to it.
    case AKEYCODE_BACK:
        return VKEY_ESCAPE;
    case AKEYCODE_SPACE:
        return VKEY_SPACE;
    case AKEYCODE_HOME:
        return VKEY_HOME;
    case AKEYCODE_DPAD_LEFT:
        return VKEY_LEFT;
    case AKEYCODE_DPAD_UP:
        return VKEY_UP;
    case AKEYCODE_DPAD_RIGHT:
        return VKEY_RIGHT;
    case AKEYCODE_DPAD_DOWN:
        return VKEY_DOWN;
    case AKEYCODE_0:
        return VKEY_0;
    case AKEYCODE_1:
        return VKEY_1;
    case AKEYCODE_2:
        return VKEY_2;
    case AKEYCODE_3:
        return VKEY_3;
    case AKEYCODE_4:
        return VKEY_4;
    case AKEYCODE_5:
        return VKEY_5;
    case AKEYCODE_6:
        return VKEY_6;
    case AKEYCODE_7:
        return VKEY_7;
    case AKEYCODE_8:
        return VKEY_8;
    case AKEYCODE_9:
        return VKEY_9;
    case AKEYCODE_A:
        return VKEY_A;
    case AKEYCODE_B:
        return VKEY_B;
    case AKEYCODE_C:
        return VKEY_C;
    case AKEYCODE_D:
        return VKEY_D;
    case AKEYCODE_E:
        return VKEY_E;
    case AKEYCODE_F:
        return VKEY_F;
    case AKEYCODE_G:
        return VKEY_G;
    case AKEYCODE_H:
        return VKEY_H;
    case AKEYCODE_I:
        return VKEY_I;
    case AKEYCODE_J:
        return VKEY_J;
    case AKEYCODE_K:
        return VKEY_K;
    case AKEYCODE_L:
        return VKEY_L;
    case AKEYCODE_M:
        return VKEY_M;
    case AKEYCODE_N:
        return VKEY_N;
    case AKEYCODE_O:
        return VKEY_O;
    case AKEYCODE_P:
        return VKEY_P;
    case AKEYCODE_Q:
        return VKEY_Q;
    case AKEYCODE_R:
        return VKEY_R;
    case AKEYCODE_S:
        return VKEY_S;
    case AKEYCODE_T:
        return VKEY_T;
    case AKEYCODE_U:
        return VKEY_U;
    case AKEYCODE_V:
        return VKEY_V;
    case AKEYCODE_W:
        return VKEY_W;
    case AKEYCODE_X:
        return VKEY_X;
    case AKEYCODE_Y:
        return VKEY_Y;
    case AKEYCODE_Z:
        return VKEY_Z;
    case AKEYCODE_VOLUME_DOWN:
        return VKEY_VOLUME_DOWN;
    case AKEYCODE_VOLUME_UP:
        return VKEY_VOLUME_UP;
    case AKEYCODE_MEDIA_NEXT:
        return VKEY_MEDIA_NEXT_TRACK;
    case AKEYCODE_MEDIA_PREVIOUS:
        return VKEY_MEDIA_PREV_TRACK;
    case AKEYCODE_MEDIA_STOP:
        return VKEY_MEDIA_STOP;
    case AKEYCODE_MEDIA_PAUSE:
        return VKEY_MEDIA_PLAY_PAUSE;
    // Colon key.
    case AKEYCODE_SEMICOLON:
        return VKEY_OEM_1;
    case AKEYCODE_COMMA:
        return VKEY_OEM_COMMA;
    case AKEYCODE_MINUS:
        return VKEY_OEM_MINUS;
    case AKEYCODE_EQUALS:
        return VKEY_OEM_PLUS;
    case AKEYCODE_PERIOD:
        return VKEY_OEM_PERIOD;
    case AKEYCODE_SLASH:
        return VKEY_OEM_2;
    case AKEYCODE_LEFT_BRACKET:
        return VKEY_OEM_4;
    case AKEYCODE_BACKSLASH:
        return VKEY_OEM_5;
    case AKEYCODE_RIGHT_BRACKET:
        return VKEY_OEM_6;
    case AKEYCODE_MUTE:
    case AKEYCODE_VOLUME_MUTE:
        return VKEY_VOLUME_MUTE;
    default:
        return 0;
    }
}

} // namespace WebCore
