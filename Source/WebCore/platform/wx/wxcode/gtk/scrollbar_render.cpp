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
#include <wx/settings.h>
#include <wx/window.h>

#include <gtk/gtk.h>

#if wxCHECK_VERSION(2, 9, 0)
    #include <wx/gtk/dc.h>
#else
    #include "wx/gtk/win_gtk.h"
#endif

int wxStyleForPart(wxScrollbarPart part, wxScrollbarPart focusPart, wxScrollbarPart hoverPart, int flags)
{
    int style = 0;
    if (flags == wxCONTROL_DISABLED)
        style = wxCONTROL_DISABLED;
    else if (part == focusPart)
        style = wxCONTROL_PRESSED;
    else if (part == hoverPart)
        style = wxCONTROL_CURRENT;

    return style;
}

GtkWidget* GetButtonWidget()
{
    static GtkWidget *s_button = NULL;
    static GtkWidget *s_window = NULL;

    if ( !s_button )
    {
        s_window = gtk_window_new( GTK_WINDOW_POPUP );
        gtk_widget_realize( s_window );
        s_button = gtk_button_new();
        gtk_container_add( GTK_CONTAINER(s_window), s_button );
        gtk_widget_realize( s_button );
    }

    return s_button;
}


GdkWindow* wxGetGdkWindowForDC(wxWindow* win, wxDC& dc)
{
    GdkWindow* gdk_window = NULL;
#if wxCHECK_VERSION(2, 9, 0)
    if ( dc.IsKindOf( CLASSINFO(wxGCDC) ) )
        gdk_window = win->GTKGetDrawingWindow();
    else
    {
        wxGTKDCImpl *impl = wxDynamicCast(dc.GetImpl(), wxGTKDCImpl);
        if ( impl )
            gdk_window = impl->GetGDKWindow();
    }
#else // wx < 2.9
    // The way to get a GdkWindow* from a wxWindow is to use 
    // GTK_PIZZA(win->m_wxwindow)->bin_window, but this approach
    // won't work when drawing to a wxMemoryDC as it has its own
    // GdkWindow* for its bitmap. wxWindowDC's GetGDKWindow() was
    // designed to create a solution for all DCs, but we can't
    // implement it with wxGCDC since it doesn't retain its wxWindow. 
    // So, to work around this, we use GetGDKWindow whenever possible
    // and use bin_window for wxGCDC. 
#if wxUSE_GRAPHICS_CONTEXT
    if ( dc.IsKindOf( CLASSINFO(wxGCDC) ) )
        gdk_window = GTK_PIZZA(win->m_wxwindow)->bin_window;    
    else
#endif
       gdk_window = dc.GetGDKWindow();
    wxASSERT_MSG( gdk_window,
                  wxT("cannot use wxRendererNative on wxDC of this type") );
#endif // wx 2.9/2.8
    
    return gdk_window;
}

void wxRenderer_DrawScrollbar(wxWindow* window, wxDC& dc, const wxRect& rect, wxOrientation orient, 
                        int current, wxScrollbarPart focusPart, wxScrollbarPart hoverPart, int max, int step, int flags)
{
    bool horiz = orient == wxHORIZONTAL;
    wxColour scrollbar_colour = wxSystemSettings::GetColour(wxSYS_COLOUR_SCROLLBAR);
    dc.SetBrush(wxBrush(scrollbar_colour));
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW)));
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);

    // when going from Cairo -> Gdk, any Cairo context transformations are lost
    // so we need to alter the coordinates to reflect their transformed point.
    double xtrans = 0;
    double ytrans = 0;
    
    wxGCDC* gcdc = wxDynamicCast(&dc, wxGCDC);
    wxGraphicsContext* gc = gcdc->GetGraphicsContext();
    gc->GetTransform().TransformPoint(&xtrans, &ytrans);

    wxRendererNative& renderer = wxRendererNative::Get();
    int x = rect.x + (int)xtrans;
    int y = rect.y + (int)ytrans;

    int buttonLength = 16;

    renderer.DrawPushButton(window, dc, wxRect(x,y,buttonLength,buttonLength), wxStyleForPart(wxSCROLLPART_BACKBTNSTART, focusPart, hoverPart, flags));

    GtkWidget* button = GetButtonWidget();
    GdkWindow* gdk_window = wxGetGdkWindowForDC(window, dc);

    GtkArrowType arrowType = GTK_ARROW_UP;
    if (horiz)
        arrowType = GTK_ARROW_LEFT;

    gtk_paint_arrow( button->style, gdk_window, GTK_STATE_NORMAL, GTK_SHADOW_OUT, NULL, button, "arrow", arrowType, false, x + 4, y + 4, 8, 8);

    wxRect buttonRect = rect;
    int physicalLength = horiz ? rect.width : rect.height;
    physicalLength -= buttonLength*2;
    int thumbStart = 0;
    int thumbLength = 0;
    calcThumbStartAndLength(physicalLength, max, current, step, &thumbStart, &thumbLength);

    if (horiz) {
        buttonRect.x = x + thumbStart + buttonLength;
        buttonRect.y = y;
        buttonRect.width = thumbLength;
    } else {
        buttonRect.x = x;
        buttonRect.y = y + thumbStart + buttonLength;
        buttonRect.height = thumbLength;
    }

    renderer.DrawPushButton(window, dc, buttonRect, wxStyleForPart(wxSCROLLPART_THUMB, focusPart, hoverPart, flags));

    if (horiz)
        x += rect.width - buttonLength;
    else
        y += rect.height - buttonLength;

    renderer.DrawPushButton(window, dc, wxRect(x,y,buttonLength,buttonLength), wxStyleForPart(wxSCROLLPART_FWDBTNEND, focusPart, hoverPart, flags));
    
    if (horiz)
        arrowType = GTK_ARROW_RIGHT;
    else
        arrowType = GTK_ARROW_DOWN;

    gtk_paint_arrow( button->style, gdk_window, GTK_STATE_NORMAL, GTK_SHADOW_OUT, NULL, button, "arrow", arrowType, false, x + 4, y + 4, 8, 8);
}
