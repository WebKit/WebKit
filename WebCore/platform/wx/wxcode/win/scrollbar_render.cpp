/*
 * Copyright (C) 2009 Kevin Ollivier  All rights reserved.
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

#include "scrollbar_render.h"

#include <wx/defs.h>

#include <wx/dc.h>
#include <wx/dcgraph.h>
#include <wx/graphics.h>
#include <wx/renderer.h>
#include <wx/window.h>

#include <wx/msw/private.h>
#include <wx/msw/uxtheme.h>

// constants
#define SP_BUTTON          1
#define SP_THUMBHOR        2
#define SP_THUMBVERT       3
#define SP_TRACKENDHOR      4
#define SP_TRACKENDVERT     7
#define SP_GRIPPERHOR       8
#define SP_GRIPPERVERT      9

#define TS_NORMAL           1
#define TS_HOVER            2
#define TS_ACTIVE           3
#define TS_DISABLED         4

#define TS_UP_BUTTON       0
#define TS_DOWN_BUTTON     4
#define TS_LEFT_BUTTON     8
#define TS_RIGHT_BUTTON    12

#if wxUSE_GRAPHICS_CONTEXT
// TODO remove this dependency (gdiplus needs the macros)
// we need to undef because the macros are being defined in WebCorePrefix.h
// but GdiPlus.h is not accepting them
#undef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))

#undef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))

#include <wx/dcgraph.h>
#include "gdiplus.h"
using namespace Gdiplus;
#endif // wxUSE_GRAPHICS_CONTEXT

class GraphicsHDC
{
public:
    GraphicsHDC(wxDC* dc)
    {
#if wxUSE_GRAPHICS_CONTEXT
        m_graphics = NULL;
        wxGCDC* gcdc = wxDynamicCast(dc, wxGCDC);
        if (gcdc) {
            m_graphics = (Graphics*)gcdc->GetGraphicsContext()->GetNativeContext();
            m_hdc = m_graphics->GetHDC();
        }
        else
#endif
            m_hdc = GetHdcOf(*dc);
    }

    ~GraphicsHDC()
    {
#if wxUSE_GRAPHICS_CONTEXT
        if (m_graphics)
            m_graphics->ReleaseHDC(m_hdc);
#endif
    }

    operator HDC() const { return m_hdc; }

private:
    HDC         m_hdc;
#if wxUSE_GRAPHICS_CONTEXT
    Graphics*   m_graphics;
#endif
};

int getTSStateForPart(wxScrollbarPart part, wxScrollbarPart focusPart, wxScrollbarPart hoverPart, int flags = 0)
{
    int xpState = TS_NORMAL;
    if (flags & wxCONTROL_DISABLED)
        xpState = TS_DISABLED;
    else if (part == focusPart)
        xpState = TS_ACTIVE;
    else if (part == hoverPart)
        xpState = TS_HOVER;
        
    return xpState;
}

void wxRenderer_DrawScrollbar(wxWindow* window, wxDC& dc,
                             const wxRect& rect, wxOrientation orient, int current, wxScrollbarPart focusPart, wxScrollbarPart hoverPart, int max, int step, int flags)
{
    wxUxThemeEngine *engine = wxUxThemeEngine::Get();
    HTHEME hTheme = (HTHEME)engine->OpenThemeData(0, L"SCROLLBAR");

    bool horiz = orient == wxHORIZONTAL;
    int part = 0;
    if (horiz)
        part = SP_TRACKENDHOR;
    else
        part = SP_TRACKENDVERT;

    int xpState = TS_NORMAL;
    wxRect transRect = rect;

#if USE(WXGC)
    // when going from GdiPlus -> Gdi, any GdiPlus transformations are lost
    // so we need to alter the coordinates to reflect their transformed point.
    double xtrans = 0;
    double ytrans = 0;
    
    wxGCDC* gcdc = wxDynamicCast(&dc, wxGCDC);
    wxGraphicsContext* gc = gcdc->GetGraphicsContext();
    gc->GetTransform().TransformPoint(&xtrans, &ytrans);

    transRect.x += (int)xtrans;
    transRect.y += (int)ytrans;
#endif

    RECT r;
    wxCopyRectToRECT(transRect, r);

    // Unlike Mac, on MSW you draw the scrollbar piece by piece.
    // so we draw the track first, then the buttons
    if (hTheme)
    {
        engine->DrawThemeBackground(hTheme, GraphicsHDC(&dc), part, xpState, &r, 0);

        int buttonSize = 16;
            
        part = SP_BUTTON;
        xpState = getTSStateForPart(wxSCROLLPART_BACKBTNSTART, focusPart, hoverPart, flags);
        xpState += horiz ? TS_LEFT_BUTTON : TS_UP_BUTTON;
        RECT buttonRect = r;
        buttonRect.bottom = buttonRect.top + buttonSize;
        buttonRect.right = buttonRect.left + buttonSize;
        engine->DrawThemeBackground(hTheme, GraphicsHDC(&dc), part, xpState, &buttonRect, 0);

        xpState = getTSStateForPart(wxSCROLLPART_FWDBTNEND, focusPart, hoverPart, flags);
        xpState += horiz ? TS_RIGHT_BUTTON : TS_DOWN_BUTTON;
        buttonRect = r;
        buttonRect.top = buttonRect.bottom - buttonSize;
        buttonRect.left = buttonRect.right - buttonSize;
        engine->DrawThemeBackground(hTheme, GraphicsHDC(&dc), part, xpState, &buttonRect, 0);

        part = horiz ? SP_THUMBHOR : SP_THUMBVERT;

        int physicalLength = horiz ? rect.width : rect.height;
        physicalLength -= buttonSize*2;
        int thumbStart = 0;
        int thumbLength = 0;
        calcThumbStartAndLength(physicalLength, max, 
                            current, step, &thumbStart, &thumbLength);
        buttonRect = r;
        if (horiz) {
            buttonRect.left = buttonRect.left + thumbStart + buttonSize;
            buttonRect.right = buttonRect.left + thumbLength;
        } else {
            buttonRect.top = buttonRect.top + thumbStart + buttonSize;
            buttonRect.bottom = buttonRect.top + thumbLength;
        }

        xpState = getTSStateForPart(wxSCROLLPART_THUMB, focusPart, hoverPart, flags);
        engine->DrawThemeBackground(hTheme, GraphicsHDC(&dc), part, xpState, &buttonRect, 0);
        
        // draw the gripper
        int thickness = ::GetSystemMetrics(SM_CXVSCROLL) / 2;
        
        buttonRect.left += ((buttonRect.right - buttonRect.left) - thickness) / 2;
        buttonRect.top += ((buttonRect.bottom - buttonRect.top) - thickness) / 2;
        buttonRect.right = buttonRect.left + thickness;
        buttonRect.bottom = buttonRect.top + thickness;
        
        if (horiz)
            part = SP_GRIPPERHOR;
        else
            part = SP_GRIPPERVERT;
        
        engine->DrawThemeBackground(hTheme, GraphicsHDC(&dc), part, xpState, &buttonRect, 0);
    }
}
