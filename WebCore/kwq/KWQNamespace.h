/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#ifndef QNAMESPACE_H_
#define QNAMESPACE_H_

class QColor;
class QCursor;

class Qt {
public:
    enum ButtonState {
        LeftButton,
        MidButton,
        RightButton,
        ControlButton,
        AltButton,
        ShiftButton,
    };

    enum AlignmentFlags {
        AlignCenter,
        WordBreak,
        ShowPrefix,
    };

    enum PenStyle {
        NoPen,
        SolidLine,
        DotLine,
        DashLine,
    };

    enum BrushStyle {
        NoBrush,
        SolidPattern,
    };

    enum GUIStyle {
        MacStyle,
        WindowsStyle,
    };

    enum Key {
        Key_Escape = 0x1000,            // misc keys
        Key_Tab = 0x1001,
        Key_Backtab = 0x1002, Key_BackTab = Key_Backtab,
        Key_Backspace = 0x1003, Key_BackSpace = Key_Backspace,
        Key_Return = 0x1004,
        Key_Enter = 0x1005,
        Key_Insert = 0x1006,
        Key_Delete = 0x1007,
        Key_Pause = 0x1008,
        Key_Print = 0x1009,
        Key_SysReq = 0x100a,
        Key_Home = 0x1010,              // cursor movement
        Key_End = 0x1011,
        Key_Left = 0x1012,
        Key_Up = 0x1013,
        Key_Right = 0x1014,
        Key_Down = 0x1015,
    };

    enum RasterOp { // raster op mode
        CopyROP,
        OrROP,
        XorROP,
    };

    static const QColor &black;
    static const QColor &white;
    static const QColor &darkGray;

    static const QCursor &sizeAllCursor;
    static const QCursor &splitHCursor;
    static const QCursor &splitVCursor;
};

#endif
