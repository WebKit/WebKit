/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "KWQDef.h"

class Qt {
public:

     enum ButtonState {
        NoButton        = 0x0000,
        LeftButton      = 0x0001,
        RightButton     = 0x0002,
        MidButton       = 0x0004,
        MouseButtonMask = 0x0007,
        ShiftButton     = 0x0008,
        ControlButton   = 0x0010,
        AltButton       = 0x0020,
        MetaButton      = 0x0040,
        KeyButtonMask   = 0x0078,
        Keypad          = 0x4000
    };

    enum AlignmentFlags {
        AlignAuto       = 0x0000,           // text alignment
        AlignLeft       = 0x0001,
        AlignRight      = 0x0002,
        AlignHCenter    = 0x0004,
        AlignVCenter    = 0x0020,
        AlignCenter     = AlignVCenter | AlignHCenter,

        DontClip        = 0x0080,           // misc. flags
        ShowPrefix      = 0x0200,
        WordBreak       = 0x0400,
    };

    enum LayoutDirection {
        LeftToRight     = 0,
        RightToLeft     = 1
    };

    enum Orientation {
        Horizontal,
        Vertical
    };

    enum BrushStyle {
        NoBrush,
        SolidPattern,
    };

    enum RasterOp { // raster op mode
        CopyROP,
        OrROP,
        XorROP,
    };
};

#endif
