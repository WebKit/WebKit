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
// this needs to be included before fontprops.h for UChar* to be defined.
#include <wtf/unicode/Unicode.h>

#include <wx/defs.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/fontutil.h>
#include "fontprops.h"
#include "non-kerned-drawing.h"

#include <gdk/gdk.h>

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

wxFontProperties::wxFontProperties(wxFont* font):
m_ascent(0), m_descent(0), m_lineGap(0), m_lineSpacing(0), m_xHeight(0)
{
    ASSERT(font && font->Ok());

#if USE(WXGC)
    cairo_font_extents_t font_extents;
    cairo_text_extents_t text_extents;
    cairo_scaled_font_t* scaled_font = WebCore::createScaledFontForFont(font);
    
    cairo_scaled_font_extents(scaled_font, &font_extents);
    m_ascent = static_cast<int>(font_extents.ascent);
    m_descent = static_cast<int>(font_extents.descent);
    m_lineSpacing = static_cast<int>(font_extents.height);
    cairo_scaled_font_text_extents(scaled_font, "x", &text_extents);
    m_xHeight = text_extents.height;
    cairo_scaled_font_text_extents(scaled_font, " ", &text_extents);
    m_lineGap = m_lineSpacing - m_ascent - m_descent;
    
    cairo_scaled_font_destroy(scaled_font);
#else
    PangoContext* context = gdk_pango_context_get_for_screen( gdk_screen_get_default() );
    PangoLayout* layout = pango_layout_new(context);
    // and use it if it's valid
    if ( font && font->Ok() )
    {
        pango_layout_set_font_description
        (
            layout,
            font->GetNativeFontInfo()->description
        );
    }
    
    PangoFontMetrics* metrics = pango_context_get_metrics (context, font->GetNativeFontInfo()->description, NULL);

    int height = font->GetPixelSize().GetHeight();

    m_ascent = PANGO_PIXELS(pango_font_metrics_get_ascent(metrics)); 
    m_descent = PANGO_PIXELS(pango_font_metrics_get_descent(metrics));
    
    int h;

    const char* x = "x";
    pango_layout_set_text( layout, x, strlen(x) );
    pango_layout_get_pixel_size( layout, NULL, &h );
            
    m_xHeight = h;
    m_lineGap = (m_ascent + m_descent) / 4; // FIXME: How can we calculate this via Pango? 
    m_lineSpacing = m_ascent + m_descent;

    pango_font_metrics_unref(metrics);
#endif
}

bool wxFontContainsCharacters(const wxFont& font, const UChar* characters, int length)
{
    return true;
}

void GetTextExtent( const wxFont& font, const wxString& str, wxCoord *width, wxCoord *height,
                            wxCoord *descent, wxCoord *externalLeading )
{
    if ( width )
        *width = 0;
    if ( height )
        *height = 0;
    if ( descent )
        *descent = 0;
    if ( externalLeading )
        *externalLeading = 0;

    if (str.empty())
        return;

// FIXME: Doesn't support height, descent or external leading, though we don't need this for WebKit
// it will need to be implemented before merging into wx unless we craft a new API.
#if USE(WXGC)
    PangoFont* pangoFont = WebCore::createPangoFontForFont(&font);
    PangoContext* pangoContext = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(WebCore::pangoFontMap()));
    PangoGlyph pangoGlyph = WebCore::pango_font_get_glyph(pangoFont, pangoContext, (gunichar)g_utf8_get_char(str.ToUTF8()));
    cairo_glyph_t cglyph = { pangoGlyph, 0, 0 };
    cairo_text_extents_t extents;
    cairo_scaled_font_t* scaled_font = WebCore::createScaledFontForFont(&font);
    cairo_scaled_font_glyph_extents(scaled_font, &cglyph, 1, &extents);

    if (cairo_scaled_font_status(scaled_font) == CAIRO_STATUS_SUCCESS && extents.x_advance != 0)
        *width = (wxCoord)extents.x_advance;

    cairo_scaled_font_destroy(scaled_font);
    g_object_unref(pangoContext);
    g_object_unref(pangoFont);
#else
    PangoContext* context = gdk_pango_context_get_for_screen( gdk_screen_get_default() );
    PangoLayout* m_layout = pango_layout_new(context);
    // and use it if it's valid
    if ( font && font->IsOk() )
    {
        pango_layout_set_font_description
        (
            m_layout,
            font->GetNativeFontInfo()->description
        );
    }

    // Set layout's text
    const wxCharBuffer dataUTF8 = wxConvUTF8.cWX2MB(str);
    if ( !dataUTF8 )
    {
        // hardly ideal, but what else can we do if conversion failed?
        return;
    }

    pango_layout_set_text( m_layout, dataUTF8, strlen(dataUTF8) );

    int h = 0;
    pango_layout_get_pixel_size( m_layout, width, &h );

    if (descent)
    {
        PangoLayoutIter *iter = pango_layout_get_iter(m_layout);
        int baseline = pango_layout_iter_get_baseline(iter);
        pango_layout_iter_free(iter);
        *descent = h - PANGO_PIXELS(baseline);
    }

    if (height)
        *height = (wxCoord) h;
#endif
}

