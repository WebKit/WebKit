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
#include <wx/renderer.h>

#include <Carbon/Carbon.h>

static int wxScrollbarPartToHIPressedState(wxScrollbarPart part)
{
    switch (part) {
        case wxSCROLLPART_BACKBTNSTART:
            return kThemeTopOutsideArrowPressed;
        case wxSCROLLPART_BACKBTNEND:
            return kThemeTopOutsideArrowPressed; // This does not make much sense.  For some reason the outside constant is required.
        case wxSCROLLPART_FWDBTNSTART:
            return kThemeTopInsideArrowPressed;
        case wxSCROLLPART_FWDBTNEND:
            return kThemeBottomOutsideArrowPressed;
        case wxSCROLLPART_THUMB:
            return kThemeThumbPressed;
        default:
            return 0;
    }
}

void wxRenderer_DrawScrollbar(wxWindow* WXUNUSED(window), wxDC& dc,
                             const wxRect& rect, wxOrientation orient, int current,
                             wxScrollbarPart focusPart, wxScrollbarPart hoverPart, int max, int step, int flags)
{
    const wxCoord x = rect.x;
    const wxCoord y = rect.y;
    const wxCoord w = rect.width;
    const wxCoord h = rect.height;

    dc.SetBrush( *wxWHITE_BRUSH );
    dc.SetPen( *wxTRANSPARENT_PEN );
    dc.DrawRectangle(rect);
    
    dc.SetBrush( *wxTRANSPARENT_BRUSH );

    HIRect hiRect = CGRectMake( x, y, w, h );

    CGContextRef cgContext = NULL;
    wxGraphicsContext* gc = NULL;
#if wxCHECK_VERSION(2,9,0)
    wxGCDCImpl *impl = dynamic_cast<wxGCDCImpl*> (dc.GetImpl());
    if (impl)
        gc = impl->GetGraphicsContext();
#else
    gc = dc.GetGraphicsContext();
#endif
    if (gc)
        cgContext = (CGContextRef) gc->GetNativeContext();
    
    if (cgContext)
    {
        HIThemeTrackDrawInfo trackInfo;
        trackInfo.version = 0;
        trackInfo.kind = kThemeMediumScrollBar;
        trackInfo.bounds = hiRect;
        trackInfo.min = 0;
        trackInfo.max = max;
        trackInfo.value = current;
        trackInfo.trackInfo.scrollbar.viewsize = step;
        trackInfo.attributes = 0;
        if (orient == wxHORIZONTAL)
            trackInfo.attributes |= kThemeTrackHorizontal;
        trackInfo.enableState = (flags & wxCONTROL_FOCUSED) ? kThemeTrackActive : kThemeTrackInactive;
        trackInfo.trackInfo.scrollbar.pressState = wxScrollbarPartToHIPressedState(focusPart);
        trackInfo.attributes |= kThemeTrackShowThumb;
        
        if (flags & wxCONTROL_DISABLED)
            trackInfo.enableState = kThemeTrackDisabled;
        
        HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);
    }
}
