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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <KWQDef.h>

class QColor;
class QCursor;

typedef unsigned int QRgb;

// class Qt ====================================================================

class Qt {
public:

    // typedefs ----------------------------------------------------------------

    // enums -------------------------------------------------------------------
 
     enum ButtonState {
        LeftButton,
        MidButton,
        RightButton,
        ControlButton,
        AltButton,
        ShiftButton,
    };

    enum AlignmentFlags {
        AlignLeft,
        AlignCenter,
        AlignRight,
        WordBreak,
        ShowPrefix,
        DontClip,
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
        Key_Escape = 0x1000,            
        Key_Tab = 0x1001,
        Key_Backtab = 0x1002, 
        Key_BackTab = Key_Backtab,
        Key_Backspace = 0x1003, 
        Key_BackSpace = Key_Backspace,
        Key_Return = 0x1004,
        Key_Enter = 0x1005,
        Key_Insert = 0x1006,
        Key_Delete = 0x1007,
        Key_Pause = 0x1008,
        Key_Print = 0x1009,
        Key_SysReq = 0x100a,
        Key_Home = 0x1010,              
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

    // constants ---------------------------------------------------------------

    QT_STATIC_CONST QColor &color0;
    QT_STATIC_CONST QColor &color1;
    QT_STATIC_CONST QColor &black;
    QT_STATIC_CONST QColor &white;
    QT_STATIC_CONST QColor &darkGray;
    QT_STATIC_CONST QColor &gray;
    QT_STATIC_CONST QColor &lightGray;
    QT_STATIC_CONST QColor &red;
    QT_STATIC_CONST QColor &green;
    QT_STATIC_CONST QColor &blue;
    QT_STATIC_CONST QColor &cyan;
    QT_STATIC_CONST QColor &magenta;
    QT_STATIC_CONST QColor &yellow;
    QT_STATIC_CONST QColor &darkRed;
    QT_STATIC_CONST QColor &darkGreen;
    QT_STATIC_CONST QColor &darkBlue;
    QT_STATIC_CONST QColor &darkCyan;
    QT_STATIC_CONST QColor &darkMagenta;
    QT_STATIC_CONST QColor &darkYellow;
                  
    static const QCursor &sizeAllCursor;
    static const QCursor &splitHCursor;
    static const QCursor &splitVCursor;

    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------
    
// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    Qt() {}
#endif

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~Qt() {}
#endif
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    Qt(const Qt &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    Qt &operator=(const Qt &);
#endif

}; // class Qt =================================================================

#endif
