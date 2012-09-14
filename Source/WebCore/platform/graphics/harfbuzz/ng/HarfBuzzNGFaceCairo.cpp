/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (c) 2012 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HarfBuzzNGFace.h"

#include "FontPlatformData.h"
#include "GlyphBuffer.h"
#include "HarfBuzzShaper.h"
#include "SimpleFontData.h"
#include "TextEncoding.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <freetype/freetype.h>
#include <freetype/tttables.h>
#include <hb.h>
#include <wtf/text/CString.h>

namespace WebCore {

class CairoFtFaceLocker {
public:
    CairoFtFaceLocker(cairo_scaled_font_t* cairoScaledFont) : m_scaledFont(cairoScaledFont) { };
    FT_Face lock()
    {
        return cairo_ft_scaled_font_lock_face(m_scaledFont);
    };
    ~CairoFtFaceLocker()
    {
        cairo_ft_scaled_font_unlock_face(m_scaledFont);
    }
private:
    cairo_scaled_font_t* m_scaledFont;
};

static hb_position_t floatToHarfBuzzPosition(float value)
{
    return static_cast<hb_position_t>(value * (1 << 16));
}

static hb_position_t doubleToHarfBuzzPosition(double value)
{
    return static_cast<hb_position_t>(value * (1 << 16));
}

static void CairoGetGlyphWidthAndExtents(cairo_scaled_font_t* scaledFont, hb_codepoint_t codepoint, hb_position_t* advance, hb_glyph_extents_t* extents)
{
    cairo_text_extents_t glyphExtents;
    cairo_glyph_t glyph;
    glyph.index = codepoint;
    glyph.x = 0;
    glyph.y = 0;
    cairo_scaled_font_glyph_extents(scaledFont, &glyph, 1, &glyphExtents);

    if (advance)
        *advance = doubleToHarfBuzzPosition(glyphExtents.x_advance);
    if (extents) {
        extents->x_bearing = doubleToHarfBuzzPosition(glyphExtents.x_bearing);
        extents->y_bearing = doubleToHarfBuzzPosition(glyphExtents.y_bearing);
        extents->width = doubleToHarfBuzzPosition(glyphExtents.width);
        extents->height = doubleToHarfBuzzPosition(glyphExtents.height);
    }
}

static hb_bool_t harfbuzzGetGlyph(hb_font_t*, void* fontData, hb_codepoint_t unicode, hb_codepoint_t, hb_codepoint_t* glyph, void*)
{
    FontPlatformData* platformData = reinterpret_cast<FontPlatformData*>(fontData);
    cairo_scaled_font_t* scaledFont = platformData->scaledFont();
    ASSERT(scaledFont);

    cairo_glyph_t* glyphs = 0;
    int numGlyphs = 0;
    CString utf8Codepoint = UTF8Encoding().encode(reinterpret_cast<UChar*>(&unicode), 1, QuestionMarksForUnencodables);
    if (CAIRO_STATUS_SUCCESS != cairo_scaled_font_text_to_glyphs(scaledFont, 0, 0, utf8Codepoint.data(), utf8Codepoint.length(), &glyphs, &numGlyphs, 0, 0, 0))
        return false;
    if (!numGlyphs)
        return false;
    *glyph = glyphs[0].index;
    cairo_glyph_free(glyphs);
    return true;
}

static hb_position_t harfbuzzGetGlyphHorizontalAdvance(hb_font_t*, void* fontData, hb_codepoint_t glyph, void*)
{
    FontPlatformData* platformData = reinterpret_cast<FontPlatformData*>(fontData);
    cairo_scaled_font_t* scaledFont = platformData->scaledFont();
    ASSERT(scaledFont);

    hb_position_t advance = 0;
    CairoGetGlyphWidthAndExtents(scaledFont, glyph, &advance, 0);
    return advance;
}

static hb_bool_t harfbuzzGetGlyphHorizontalOrigin(hb_font_t*, void*, hb_codepoint_t, hb_position_t*, hb_position_t*, void*)
{
    // Just return true, following the way that Harfbuzz-FreeType
    // implementation does.
    return true;
}

static hb_bool_t harfbuzzGetGlyphExtents(hb_font_t*, void* fontData, hb_codepoint_t glyph, hb_glyph_extents_t* extents, void*)
{
    FontPlatformData* platformData = reinterpret_cast<FontPlatformData*>(fontData);
    cairo_scaled_font_t* scaledFont = platformData->scaledFont();
    ASSERT(scaledFont);

    CairoGetGlyphWidthAndExtents(scaledFont, glyph, 0, extents);
    return true;
}

static hb_font_funcs_t* harfbuzzCairoTextGetFontFuncs()
{
    static hb_font_funcs_t* harfbuzzCairoFontFuncs = 0;

    // We don't set callback functions which we can't support.
    // Harfbuzz will use the fallback implementation if they aren't set.
    if (!harfbuzzCairoFontFuncs) {
        harfbuzzCairoFontFuncs = hb_font_funcs_create();
        hb_font_funcs_set_glyph_func(harfbuzzCairoFontFuncs, harfbuzzGetGlyph, 0, 0);
        hb_font_funcs_set_glyph_h_advance_func(harfbuzzCairoFontFuncs, harfbuzzGetGlyphHorizontalAdvance, 0, 0);
        hb_font_funcs_set_glyph_h_origin_func(harfbuzzCairoFontFuncs, harfbuzzGetGlyphHorizontalOrigin, 0, 0);
        hb_font_funcs_set_glyph_extents_func(harfbuzzCairoFontFuncs, harfbuzzGetGlyphExtents, 0, 0);
        hb_font_funcs_make_immutable(harfbuzzCairoFontFuncs);
    }
    return harfbuzzCairoFontFuncs;
}

static hb_blob_t* harfbuzzCairoGetTable(hb_face_t*, hb_tag_t tag, void* userData)
{
    FontPlatformData* font = reinterpret_cast<FontPlatformData*>(userData);

    cairo_scaled_font_t* scaledFont = font->scaledFont();
    if (!scaledFont)
        return 0;

    CairoFtFaceLocker cairoFtFaceLocker(scaledFont);
    FT_Face ftFont = cairoFtFaceLocker.lock();
    if (!ftFont)
        return 0;

    FT_ULong tableSize = 0;
    FT_Error error = FT_Load_Sfnt_Table(ftFont, tag, 0, 0, &tableSize);
    if (error)
        return 0;

    FT_Byte* buffer = reinterpret_cast<FT_Byte*>(fastMalloc(tableSize));
    if (!buffer)
        return 0;
    FT_ULong expectedTableSize = tableSize;
    error = FT_Load_Sfnt_Table(ftFont, tag, 0, buffer, &tableSize);
    if (error || tableSize != expectedTableSize) {
        fastFree(buffer);
        return 0;
    }

    return hb_blob_create(reinterpret_cast<const char*>(buffer), tableSize, HB_MEMORY_MODE_WRITABLE, buffer, fastFree);
}

hb_face_t* HarfBuzzNGFace::createFace()
{
    hb_face_t* face = hb_face_create_for_tables(harfbuzzCairoGetTable, m_platformData, 0);
    ASSERT(face);
    return face;
}

hb_font_t* HarfBuzzNGFace::createFont()
{
    hb_font_t* font = hb_font_create(m_face);
    hb_font_set_funcs(font, harfbuzzCairoTextGetFontFuncs(), m_platformData, 0);
    const float size = m_platformData->size();
    if (floorf(size) == size)
        hb_font_set_ppem(font, size, size);
    int scale = floatToHarfBuzzPosition(size);
    hb_font_set_scale(font, scale, scale);
    hb_font_make_immutable(font);
    return font;
}

GlyphBufferAdvance HarfBuzzShaper::createGlyphBufferAdvance(float width, float height)
{
    return GlyphBufferAdvance(width, height);
}

} // namespace WebCore
