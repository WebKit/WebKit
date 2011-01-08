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

#ifndef scrollbar_render_h
#define scrollbar_render_h

#include <wx/defs.h>
#include <wx/dc.h>
#include <wx/renderer.h>

enum wxScrollbarPart {
    wxSCROLLPART_NONE = 0,
    wxSCROLLPART_BACKBTNSTART = 1,
    wxSCROLLPART_FWDBTNSTART = 1 << 1,
    wxSCROLLPART_BACKTRACK = 1 << 2,
    wxSCROLLPART_THUMB = 1 << 3,
    wxSCROLLPART_FWDTRACK = 1 << 4,
    wxSCROLLPART_BACKBTNEND = 1 << 5,
    wxSCROLLPART_FWDBTNEND = 1 << 6,
    wxSCROLLPART_SCROLLBARBG = 1 << 7,
    wxSCROLLPART_TRACKBG = 1 << 8,
    wxSCROLLPART_ALL = 0xffffffff,
};

void wxRenderer_DrawScrollbar(wxWindow* window, wxDC& dc, 
                                       const wxRect& rect, 
                                       wxOrientation orientation, 
                                       int current, wxScrollbarPart focusPart, wxScrollbarPart hoverPart, 
                                       int max, int step, int flags=0);

inline void calcThumbStartAndLength(int physicalLength, int max, int current, 
                                int step, int *thumbStart, int *thumbLength)
{
        float proportion = ((float)physicalLength - 8)/ (max + step);
        float thumbSize =  proportion * (float)physicalLength;
        if (thumbSize < 20)
            thumbSize = 20;

        float thumbPos = ((float)current / (float)max) * ((float)physicalLength - thumbSize);
        if (thumbStart)
            *thumbStart = thumbPos;
        
        if (thumbLength)
            *thumbLength = thumbSize;
}
#endif
