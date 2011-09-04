/*
 * Copyright (C) 2007 Kevin Watters, Kevin Ollivier.  All rights reserved.
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
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "SimpleFontData.h"

#include <wx/dc.h>
#include <wx/dcgraph.h>
#include <wx/defs.h>
#include <wx/dcclient.h>
#include <wx/gdicmn.h>
#include <vector>

#if USE(WXGC)
#include <cairo.h>
#if __WXMSW__
#include <cairo-win32.h>
#endif
#include <assert.h>

#if wxUSE_PANGO
#include <pango/pango.h>
#include <pango/pangocairo.h>

// Use cairo-ft if a recent enough Pango version isn't available
#if !PANGO_VERSION_CHECK(1,18,0)
#include <cairo-ft.h>
#include <pango/pangofc-fontmap.h>
#endif
#endif

#endif // USE(WXGC)

#if __WXGTK__
#include <gtk/gtk.h>
#endif

namespace WebCore {

#if wxUSE_PANGO
static PangoFontMap* g_fontMap;

PangoFontMap* pangoFontMap()
{
    if (!g_fontMap)
        g_fontMap = pango_cairo_font_map_get_default();

    return g_fontMap;
}

PangoFont* createPangoFontForFont(const wxFont* wxfont)
{
    ASSERT(wxfont && wxfont->Ok());

    const char* face = wxfont->GetFaceName().mb_str(wxConvUTF8);
    char const* families[] = {
        face,
        0
    };

    switch (wxfont->GetFamily()) {
    case wxFONTFAMILY_ROMAN:
        families[1] = "serif";
        break;
    case wxFONTFAMILY_SWISS:
        families[1] = "sans";
        break;
    case wxFONTFAMILY_MODERN:
        families[1] = "monospace";
        break;
    default:
        families[1] = "sans";
    }
    
    PangoFontDescription* description = pango_font_description_new();
    pango_font_description_set_absolute_size(description, wxfont->GetPointSize() * PANGO_SCALE);
 
    PangoFont* pangoFont = 0;
    PangoContext* pangoContext = 0;

    switch (wxfont->GetWeight()) {
    case wxFONTWEIGHT_LIGHT:
        pango_font_description_set_weight(description, PANGO_WEIGHT_LIGHT);
        break;
    case wxFONTWEIGHT_NORMAL:
        pango_font_description_set_weight(description, PANGO_WEIGHT_NORMAL);
        break;
    case wxFONTWEIGHT_BOLD:
        pango_font_description_set_weight(description, PANGO_WEIGHT_BOLD);
        break;
    }

    switch (wxfont->GetStyle()) {
    case wxFONTSTYLE_NORMAL:
        pango_font_description_set_style(description, PANGO_STYLE_NORMAL);
        break;
    case wxFONTSTYLE_ITALIC:
        pango_font_description_set_style(description, PANGO_STYLE_ITALIC);
        break;
    case wxFONTSTYLE_SLANT:
        pango_font_description_set_style(description, PANGO_STYLE_OBLIQUE);
        break;
    }

    PangoFontMap* fontMap = pangoFontMap();

    pangoContext = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(fontMap));
    for (unsigned i = 0; !pangoFont && i < G_N_ELEMENTS(families); i++) {
        pango_font_description_set_family(description, families[i]);
        pango_context_set_font_description(pangoContext, description);
        pangoFont = pango_font_map_load_font(fontMap, pangoContext, description);
    }
    pango_font_description_free(description);

    return pangoFont;
}

cairo_scaled_font_t* createScaledFontForFont(const wxFont* wxfont)
{
    ASSERT(wxfont && wxfont->Ok());

    cairo_scaled_font_t* scaledFont = NULL;
    PangoFont* pangoFont = createPangoFontForFont(wxfont);
    
#if PANGO_VERSION_CHECK(1,18,0)
    if (pangoFont)
        scaledFont = cairo_scaled_font_reference(pango_cairo_font_get_scaled_font(PANGO_CAIRO_FONT(pangoFont)));
#endif

    return scaledFont;
}

PangoGlyph pango_font_get_glyph(PangoFont* font, PangoContext* context, gunichar wc)
{
    PangoGlyph result = 0;
    gchar buffer[7];

    gint  length = g_unichar_to_utf8(wc, buffer);
    g_return_val_if_fail(length, 0);

    GList* items = pango_itemize(context, buffer, 0, length, NULL, NULL);

    if (g_list_length(items) == 1) {
        PangoItem* item = static_cast<PangoItem*>(items->data);
        PangoFont* tmpFont = item->analysis.font;
        item->analysis.font = font;

        PangoGlyphString* glyphs = pango_glyph_string_new();
        pango_shape(buffer, length, &item->analysis, glyphs);

        item->analysis.font = tmpFont;

        if (glyphs->num_glyphs == 1)
            result = glyphs->glyphs[0].glyph;
        else
            g_warning("didn't get 1 glyph but %d", glyphs->num_glyphs);

        pango_glyph_string_free(glyphs);
    }

    g_list_foreach(items, (GFunc)pango_item_free, NULL);
    g_list_free(items);

    return result;
}
#endif // wxUSE_PANGO


void drawTextWithSpacing(GraphicsContext* graphicsContext, const SimpleFontData* font, const wxColour& color, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point)
{
#if USE(WXGC)
    wxGCDC* dc = static_cast<wxGCDC*>(graphicsContext->platformContext());
    wxGraphicsContext* gc = dc->GetGraphicsContext();
    gc->PushState();
    cairo_t* cr = (cairo_t*)gc->GetNativeContext();

    wxFont* wxfont = font->getWxFont();
    cairo_scaled_font_t* scaled_font = 0;
#if wxUSE_PANGO
    PangoFont* pangoFont = createPangoFontForFont(wxfont);
    PangoFontMap* fontMap = pangoFontMap();
    PangoContext* pangoContext = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(fontMap));
    scaled_font = createScaledFontForFont(wxfont); 
#elif __WXMSW__
    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    int size = font->platformData().size();
    cairo_matrix_init_scale(&sizeMatrix, size, size);

    cairo_font_options_t* fontOptions = cairo_font_options_create();
    cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_SUBPIXEL);
    
    cairo_font_face_t* win_face = cairo_win32_font_face_create_for_hfont((HFONT)wxfont->GetHFONT());
    scaled_font = cairo_scaled_font_create(win_face, &sizeMatrix, &ctm, fontOptions); 
#endif
    ASSERT(scaled_font);

    GlyphBufferGlyph* glyphs = const_cast<GlyphBufferGlyph*>(glyphBuffer.glyphs(from));
    float offset = point.x();

    for (int i = 0; i < numGlyphs; i++) {
#if wxUSE_PANGO
        glyphs[i].index = pango_font_get_glyph(pangoFont, pangoContext, glyphBuffer.glyphAt(from + i));
#endif
        glyphs[i].x = offset;
        glyphs[i].y = point.y();
        offset += glyphBuffer.advanceAt(from + i);
    }

    cairo_set_source_rgba(cr, color.Red()/255.0, color.Green()/255.0, color.Blue()/255.0, color.Alpha()/255.0);
    cairo_set_scaled_font(cr, scaled_font);
    
    cairo_show_glyphs(cr, glyphs, numGlyphs);

    cairo_scaled_font_destroy(scaled_font);
    gc->PopState();
#else
    wxDC* dc = graphicsContext->platformContext();

    wxFont* wxfont = font->getWxFont();
    if (wxfont && wxfont->IsOk())
        dc->SetFont(*wxfont);
    dc->SetTextForeground(color);

    // convert glyphs to wxString
    GlyphBufferGlyph* glyphs = const_cast<GlyphBufferGlyph*>(glyphBuffer.glyphs(from));
    int offset = point.x();
    wxString text = wxEmptyString;
    for (unsigned i = 0; i < numGlyphs; i++) {
        text = text.Append((wxChar)glyphs[i]);
        offset += glyphBuffer.advanceAt(from + i);
    }
    
    // the y point is actually the bottom point of the text, turn it into the top
    float height = font->ascent() - font->descent();
    wxCoord ypoint = (wxCoord) (point.y() - height);
     
    dc->DrawText(text, (wxCoord)point.x(), ypoint);
#endif
}

}
