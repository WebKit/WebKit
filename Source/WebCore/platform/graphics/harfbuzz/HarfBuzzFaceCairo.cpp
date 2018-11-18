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
#include "HarfBuzzFace.h"

#include "CairoUtilities.h"
#include "Font.h"
#include "FontCascade.h"
#include "FontPlatformData.h"
#include "GlyphBuffer.h"
#include "TextEncoding.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include <hb.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringView.h>

namespace WebCore {

struct HarfBuzzFontData {
    WTF::HashMap<uint32_t, uint32_t>& glyphCacheForFaceCacheEntry;
    RefPtr<cairo_scaled_font_t> cairoScaledFont;
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

    bool hasVerticalGlyphs = glyphExtents.y_advance;
    if (advance)
        *advance = doubleToHarfBuzzPosition(hasVerticalGlyphs ? -glyphExtents.y_advance : glyphExtents.x_advance);

    if (extents) {
        extents->x_bearing = doubleToHarfBuzzPosition(glyphExtents.x_bearing);
        extents->y_bearing = doubleToHarfBuzzPosition(hasVerticalGlyphs ? -glyphExtents.y_bearing : glyphExtents.y_bearing);
        extents->width = doubleToHarfBuzzPosition(hasVerticalGlyphs ? -glyphExtents.height : glyphExtents.width);
        extents->height = doubleToHarfBuzzPosition(hasVerticalGlyphs ? glyphExtents.width : glyphExtents.height);
    }
}

static hb_bool_t harfBuzzGetGlyph(hb_font_t*, void* fontData, hb_codepoint_t unicode, hb_codepoint_t, hb_codepoint_t* glyph, void*)
{
    auto& hbFontData = *static_cast<HarfBuzzFontData*>(fontData);
    auto* scaledFont = hbFontData.cairoScaledFont.get();
    ASSERT(scaledFont);

    auto result = hbFontData.glyphCacheForFaceCacheEntry.add(unicode, 0);
    if (result.isNewEntry) {
        cairo_glyph_t* glyphs = 0;
        int numGlyphs = 0;
        char buffer[U8_MAX_LENGTH];
        size_t bufferLength = 0;
        if (FontCascade::treatAsSpace(unicode) && unicode != '\t')
            unicode = ' ';
        else if (FontCascade::treatAsZeroWidthSpaceInComplexScript(unicode))
            unicode = zeroWidthSpace;
        U8_APPEND_UNSAFE(buffer, bufferLength, unicode);
        if (cairo_scaled_font_text_to_glyphs(scaledFont, 0, 0, buffer, bufferLength, &glyphs, &numGlyphs, nullptr, nullptr, nullptr) != CAIRO_STATUS_SUCCESS || !numGlyphs)
            return false;
        result.iterator->value = glyphs[0].index;
        cairo_glyph_free(glyphs);
    }
    *glyph = result.iterator->value;
    return !!*glyph;
}

static hb_position_t harfBuzzGetGlyphHorizontalAdvance(hb_font_t*, void* fontData, hb_codepoint_t glyph, void*)
{
    auto& hbFontData = *static_cast<HarfBuzzFontData*>(fontData);
    auto* scaledFont = hbFontData.cairoScaledFont.get();
    ASSERT(scaledFont);

    hb_position_t advance = 0;
    CairoGetGlyphWidthAndExtents(scaledFont, glyph, &advance, 0);
    return advance;
}

static hb_bool_t harfBuzzGetGlyphHorizontalOrigin(hb_font_t*, void*, hb_codepoint_t, hb_position_t*, hb_position_t*, void*)
{
    // Just return true, following the way that Harfbuzz-FreeType
    // implementation does.
    return true;
}

static hb_bool_t harfBuzzGetGlyphExtents(hb_font_t*, void* fontData, hb_codepoint_t glyph, hb_glyph_extents_t* extents, void*)
{
    auto& hbFontData = *static_cast<HarfBuzzFontData*>(fontData);
    auto* scaledFont = hbFontData.cairoScaledFont.get();
    ASSERT(scaledFont);

    CairoGetGlyphWidthAndExtents(scaledFont, glyph, 0, extents);
    return true;
}

static hb_font_funcs_t* harfBuzzCairoTextGetFontFuncs()
{
    static hb_font_funcs_t* harfBuzzCairoFontFuncs = 0;

    // We don't set callback functions which we can't support.
    // Harfbuzz will use the fallback implementation if they aren't set.
    if (!harfBuzzCairoFontFuncs) {
        harfBuzzCairoFontFuncs = hb_font_funcs_create();
        hb_font_funcs_set_glyph_func(harfBuzzCairoFontFuncs, harfBuzzGetGlyph, 0, 0);
        hb_font_funcs_set_glyph_h_advance_func(harfBuzzCairoFontFuncs, harfBuzzGetGlyphHorizontalAdvance, 0, 0);
        hb_font_funcs_set_glyph_h_origin_func(harfBuzzCairoFontFuncs, harfBuzzGetGlyphHorizontalOrigin, 0, 0);
        hb_font_funcs_set_glyph_extents_func(harfBuzzCairoFontFuncs, harfBuzzGetGlyphExtents, 0, 0);
        hb_font_funcs_make_immutable(harfBuzzCairoFontFuncs);
    }
    return harfBuzzCairoFontFuncs;
}

static hb_blob_t* harfBuzzCairoGetTable(hb_face_t*, hb_tag_t tag, void* userData)
{
    auto* scaledFont = static_cast<cairo_scaled_font_t*>(userData);
    if (!scaledFont)
        return 0;

    CairoFtFaceLocker cairoFtFaceLocker(scaledFont);
    FT_Face ftFont = cairoFtFaceLocker.ftFace();
    if (!ftFont)
        return nullptr;

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

hb_face_t* HarfBuzzFace::createFace()
{
    auto* scaledFont = m_platformData.scaledFont();
    cairo_scaled_font_reference(scaledFont);

    hb_face_t* face = hb_face_create_for_tables(harfBuzzCairoGetTable, scaledFont,
        [](void* data)
        {
            cairo_scaled_font_destroy(static_cast<cairo_scaled_font_t*>(data));
        });
    ASSERT(face);
    return face;
}

hb_font_t* HarfBuzzFace::createFont()
{
    hb_font_t* font = hb_font_create(m_cacheEntry->face());
    hb_font_set_funcs(font, harfBuzzCairoTextGetFontFuncs(), new HarfBuzzFontData { m_cacheEntry->glyphCache(), m_platformData.scaledFont() },
        [](void* data)
        {
            delete static_cast<HarfBuzzFontData*>(data);
        });

    const float size = m_platformData.size();
    if (floorf(size) == size)
        hb_font_set_ppem(font, size, size);
    int scale = floatToHarfBuzzPosition(size);
    hb_font_set_scale(font, scale, scale);
    hb_font_make_immutable(font);
    return font;
}

} // namespace WebCore
