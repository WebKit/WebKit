/*
 * Copyright (C) 2003-6 Apple Computer, Inc.  All rights reserved.
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

#ifndef Pen_h
#define Pen_h

#include "Color.h"

#if PLATFORM(WX)
class wxPen;
#endif

namespace WebCore {

class Pen {
public:
    enum PenStyle {
        NoPen,
        SolidLine,
        DotLine,
        DashLine
    };

    Pen(const Color &c = Color::black, unsigned w = 0, PenStyle ps = SolidLine);

    const Color &color() const;
    unsigned width() const;
    PenStyle style() const;

    void setColor(const Color &);
    void setWidth(unsigned);
    void setStyle(PenStyle);

    bool operator==(const Pen &) const;
    bool operator!=(const Pen &) const;
    
#if PLATFORM(WX)
    Pen(const wxPen&);
    operator wxPen() const;
#endif

private:
    PenStyle  m_style;
    unsigned  m_width;
    Color     m_color;
};

}

#endif
