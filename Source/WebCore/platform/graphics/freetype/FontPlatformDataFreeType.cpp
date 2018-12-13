/*
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2009, 2010 Igalia S.L.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FontPlatformData.h"

#include "CairoUniquePtr.h"
#include "CairoUtilities.h"
#include "FontCache.h"
#include "SharedBuffer.h"
#include <cairo-ft.h>
#include <fontconfig/fcfreetype.h>
#include <ft2build.h>
#include FT_TRUETYPE_TABLES_H
#include <hb-ft.h>
#include <hb-ot.h>
#include <wtf/MathExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static cairo_subpixel_order_t convertFontConfigSubpixelOrder(int fontConfigOrder)
{
    switch (fontConfigOrder) {
    case FC_RGBA_RGB:
        return CAIRO_SUBPIXEL_ORDER_RGB;
    case FC_RGBA_BGR:
        return CAIRO_SUBPIXEL_ORDER_BGR;
    case FC_RGBA_VRGB:
        return CAIRO_SUBPIXEL_ORDER_VRGB;
    case FC_RGBA_VBGR:
        return CAIRO_SUBPIXEL_ORDER_VBGR;
    case FC_RGBA_NONE:
    case FC_RGBA_UNKNOWN:
        return CAIRO_SUBPIXEL_ORDER_DEFAULT;
    }
    return CAIRO_SUBPIXEL_ORDER_DEFAULT;
}

static cairo_hint_style_t convertFontConfigHintStyle(int fontConfigStyle)
{
    switch (fontConfigStyle) {
    case FC_HINT_NONE:
        return CAIRO_HINT_STYLE_NONE;
    case FC_HINT_SLIGHT:
        return CAIRO_HINT_STYLE_SLIGHT;
    case FC_HINT_MEDIUM:
        return CAIRO_HINT_STYLE_MEDIUM;
    case FC_HINT_FULL:
        return CAIRO_HINT_STYLE_FULL;
    }
    return CAIRO_HINT_STYLE_NONE;
}

static void setCairoFontOptionsFromFontConfigPattern(cairo_font_options_t* options, FcPattern* pattern)
{
    FcBool booleanResult;
    int integerResult;

    if (FcPatternGetInteger(pattern, FC_RGBA, 0, &integerResult) == FcResultMatch) {
        cairo_font_options_set_subpixel_order(options, convertFontConfigSubpixelOrder(integerResult));

        // Based on the logic in cairo-ft-font.c in the cairo source, a font with
        // a subpixel order implies that is uses subpixel antialiasing.
        if (integerResult != FC_RGBA_NONE)
            cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_SUBPIXEL);
    }

    if (FcPatternGetBool(pattern, FC_ANTIALIAS, 0, &booleanResult) == FcResultMatch) {
        // Only override the anti-aliasing setting if was previously turned off. Otherwise
        // we'll override the preference which decides between gray anti-aliasing and
        // subpixel anti-aliasing.
        if (!booleanResult)
            cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_NONE);
        else if (cairo_font_options_get_antialias(options) == CAIRO_ANTIALIAS_NONE)
            cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    }

    if (FcPatternGetInteger(pattern, FC_HINT_STYLE, 0, &integerResult) == FcResultMatch)
        cairo_font_options_set_hint_style(options, convertFontConfigHintStyle(integerResult));
    if (FcPatternGetBool(pattern, FC_HINTING, 0, &booleanResult) == FcResultMatch && !booleanResult)
        cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);

#if ENABLE(VARIATION_FONTS)
    FcChar8* variations;
    if (FcPatternGetString(pattern, FC_FONT_VARIATIONS, 0, &variations) == FcResultMatch) {
        cairo_font_options_set_variations(options, reinterpret_cast<char*>(variations));
    }
#endif
}

FontPlatformData::FontPlatformData(cairo_font_face_t* fontFace, FcPattern* pattern, float size, bool fixedWidth, bool syntheticBold, bool syntheticOblique, FontOrientation orientation)
    : FontPlatformData(size, syntheticBold, syntheticOblique, orientation)
{
    m_pattern = pattern;
    m_fixedWidth = fixedWidth;

    buildScaledFont(fontFace);
}

FontPlatformData::FontPlatformData(const FontPlatformData& other)
{
    *this = other;
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& other)
{
    // Check for self-assignment.
    if (this == &other)
        return *this;

    m_size = other.m_size;
    m_orientation = other.m_orientation;
    m_widthVariant = other.m_widthVariant;
    m_textRenderingMode = other.m_textRenderingMode;

    m_syntheticBold = other.m_syntheticBold;
    m_syntheticOblique = other.m_syntheticOblique;
    m_isColorBitmapFont = other.m_isColorBitmapFont;
    m_isHashTableDeletedValue = other.m_isHashTableDeletedValue;
    m_isSystemFont = other.m_isSystemFont;

    m_fixedWidth = other.m_fixedWidth;
    m_pattern = other.m_pattern;

    m_scaledFont = other.m_scaledFont;

    return *this;
}

FontPlatformData::~FontPlatformData() = default;

FontPlatformData FontPlatformData::cloneWithOrientation(const FontPlatformData& source, FontOrientation orientation)
{
    FontPlatformData copy(source);
    if (copy.m_scaledFont && copy.m_orientation != orientation) {
        copy.m_orientation = orientation;
        copy.buildScaledFont(cairo_scaled_font_get_font_face(copy.m_scaledFont.get()));
    }
    return copy;
}

FontPlatformData FontPlatformData::cloneWithSyntheticOblique(const FontPlatformData& source, bool syntheticOblique)
{
    FontPlatformData copy(source);
    if (copy.m_syntheticOblique != syntheticOblique) {
        copy.m_syntheticOblique = syntheticOblique;
        ASSERT(copy.m_scaledFont.get());
        copy.buildScaledFont(cairo_scaled_font_get_font_face(copy.m_scaledFont.get()));
    }
    return copy;
}

FontPlatformData FontPlatformData::cloneWithSize(const FontPlatformData& source, float size)
{
    FontPlatformData copy(source);
    copy.m_size = size;
    // We need to reinitialize the instance, because the difference in size
    // necessitates a new scaled font instance.
    ASSERT(copy.m_scaledFont.get());
    copy.buildScaledFont(cairo_scaled_font_get_font_face(copy.m_scaledFont.get()));
    return copy;
}

FcPattern* FontPlatformData::fcPattern() const
{
    return m_pattern.get();
}

bool FontPlatformData::isFixedPitch() const
{
    return m_fixedWidth;
}

unsigned FontPlatformData::hash() const
{
    return PtrHash<cairo_scaled_font_t*>::hash(m_scaledFont.get());
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    // FcPatternEqual does not support null pointers as arguments.
    if ((m_pattern && !other.m_pattern)
        || (!m_pattern && other.m_pattern)
        || (m_pattern != other.m_pattern && !FcPatternEqual(m_pattern.get(), other.m_pattern.get())))
        return false;

    return m_scaledFont == other.m_scaledFont;
}

#if !LOG_DISABLED
String FontPlatformData::description() const
{
    return String();
}
#endif

void FontPlatformData::buildScaledFont(cairo_font_face_t* fontFace)
{
    ASSERT(m_pattern);
    CairoUniquePtr<cairo_font_options_t> options(cairo_font_options_copy(getDefaultCairoFontOptions()));
    setCairoFontOptionsFromFontConfigPattern(options.get(), m_pattern.get());

    cairo_matrix_t ctm;
    cairo_matrix_init_identity(&ctm);

    // FontConfig may return a list of transformation matrices with the pattern, for instance,
    // for fonts that are oblique. We use that to initialize the cairo font matrix.
    cairo_matrix_t fontMatrix;
    FcMatrix fontConfigMatrix, *tempFontConfigMatrix;
    FcMatrixInit(&fontConfigMatrix);

    // These matrices may be stacked in the pattern, so it's our job to get them all and multiply them.
    for (int i = 0; FcPatternGetMatrix(m_pattern.get(), FC_MATRIX, i, &tempFontConfigMatrix) == FcResultMatch; i++)
        FcMatrixMultiply(&fontConfigMatrix, &fontConfigMatrix, tempFontConfigMatrix);

    cairo_matrix_init(&fontMatrix, 1, -fontConfigMatrix.yx, -fontConfigMatrix.xy, 1, 0, 0);

    // The matrix from FontConfig does not include the scale. Scaling a font with width zero size leads
    // to a failed cairo_scaled_font_t instantiations. Instead we scale the font to a very tiny
    // size and just abort rendering later on.
    float realSize = m_size ? m_size : 1;
    cairo_matrix_scale(&fontMatrix, realSize, realSize);

    if (syntheticOblique()) {
        static const float syntheticObliqueSkew = -tanf(14 * acosf(0) / 90);
        static const cairo_matrix_t skew = {1, 0, syntheticObliqueSkew, 1, 0, 0};
        static const cairo_matrix_t verticalSkew = {1, -syntheticObliqueSkew, 0, 1, 0, 0};
        cairo_matrix_multiply(&fontMatrix, m_orientation == FontOrientation::Vertical ? &verticalSkew : &skew, &fontMatrix);
    }

    if (m_orientation == FontOrientation::Vertical) {
        // The resulting transformation matrix for vertical glyphs (V) is a
        // combination of rotation (R) and translation (T) applied on the
        // horizontal matrix (H). V = H . R . T, where R rotates by -90 degrees
        // and T translates by font size towards y axis.
        cairo_matrix_rotate(&fontMatrix, -piOverTwoDouble);
        cairo_matrix_translate(&fontMatrix, 0.0, 1.0);
    }

    m_scaledFont = adoptRef(cairo_scaled_font_create(fontFace, &fontMatrix, &ctm, options.get()));
}

bool FontPlatformData::hasCompatibleCharmap() const
{
    ASSERT(m_scaledFont.get());
    CairoFtFaceLocker cairoFtFaceLocker(m_scaledFont.get());
    FT_Face freeTypeFace = cairoFtFaceLocker.ftFace();
    if (!freeTypeFace)
        return false;
    return !(FT_Select_Charmap(freeTypeFace, ft_encoding_unicode)
        && FT_Select_Charmap(freeTypeFace, ft_encoding_symbol)
        && FT_Select_Charmap(freeTypeFace, ft_encoding_apple_roman));
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    CairoFtFaceLocker cairoFtFaceLocker(m_scaledFont.get());
    FT_Face freeTypeFace = cairoFtFaceLocker.ftFace();
    if (!freeTypeFace)
        return nullptr;

    FT_ULong tableSize = 0;
    // Tag bytes need to be reversed because OT_MAKE_TAG uses big-endian order.
    uint32_t tag = FT_MAKE_TAG((table & 0xff), (table & 0xff00) >> 8, (table & 0xff0000) >> 16, table >> 24);
    if (FT_Load_Sfnt_Table(freeTypeFace, tag, 0, 0, &tableSize))
        return nullptr;

    Vector<char> data(tableSize);
    FT_ULong expectedTableSize = tableSize;
    FT_Error error = FT_Load_Sfnt_Table(freeTypeFace, tag, 0, reinterpret_cast<FT_Byte*>(data.data()), &tableSize);
    if (error || tableSize != expectedTableSize)
        return nullptr;

    return SharedBuffer::create(WTFMove(data));
}

HbUniquePtr<hb_font_t> FontPlatformData::createOpenTypeMathHarfBuzzFont() const
{
    CairoFtFaceLocker cairoFtFaceLocker(m_scaledFont.get());
    FT_Face ftFace = cairoFtFaceLocker.ftFace();
    if (!ftFace)
        return nullptr;

    HbUniquePtr<hb_face_t> face(hb_ft_face_create_cached(ftFace));
    if (!hb_ot_math_has_data(face.get()))
        return nullptr;

    return HbUniquePtr<hb_font_t>(hb_font_create(face.get()));
}

} // namespace WebCore
