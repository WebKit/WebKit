/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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

#include "config.h"
#include "Pen.h"

#include <wx/defs.h>
#include <wx/pen.h>
#include <wx/colour.h>
 
namespace WebCore {

// Pen style conversion functions
static int penStyleToWxPenStyle(int p)
{
    if (p == Pen::SolidLine)
        return wxSOLID;
    if (p == Pen::DotLine)
        return wxDOT;
    if (p == Pen::DashLine)
        return wxLONG_DASH;
    if (p == Pen::NoPen)
        return wxTRANSPARENT;
    
    return wxSOLID;
}

static Pen::PenStyle wxPenStyleToPenStyle(int p)
{
    if (p == wxSOLID)
        return Pen::SolidLine;
    if (p == wxDOT)
        return Pen::DotLine;
    if (p == wxLONG_DASH || p == wxSHORT_DASH || p == wxDOT_DASH || p == wxUSER_DASH)
        return Pen::DashLine;
    if (p == wxTRANSPARENT)
        return Pen::NoPen; 
    
    return Pen::SolidLine;
}

Pen::Pen(const wxPen& p)
{
    wxColour color = p.GetColour();
    setColor(Color(color.Red(), color.Green(), color.Blue()));
    setWidth(p.GetWidth());
    setStyle(wxPenStyleToPenStyle(p.GetStyle()));
}

Pen::operator wxPen() const
{
    return wxPen(wxColour(m_color.red(), m_color.blue(), m_color.green()), width(), penStyleToWxPenStyle(style()));
}

}
