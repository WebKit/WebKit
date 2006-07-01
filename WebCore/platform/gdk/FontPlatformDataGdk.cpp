/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "FontPlatformData.h"

#include "PlatformString.h"
#include "DeprecatedString.h"
#include "FontDescription.h"
#include "cairo-ft.h"
#include "cairo.h"
#include <fontconfig/fcfreetype.h>
#include <assert.h>

namespace WebCore {

FontPlatformData::FontPlatformData(const FontDescription& fontDescription, const AtomicString& familyName)
    : m_fontDescription(fontDescription)
{
    init();

    FcChar8* family = (FcChar8*)((const char*)(familyName.deprecatedString().utf8()));
    int fcslant = FC_SLANT_ROMAN;
    int fcweight = FC_WEIGHT_NORMAL;
    int fcsize = (int)fontDescription.computedSize();
    if (fontDescription.italic())
        fcslant = FC_SLANT_ITALIC;
    if (fontDescription.weight() == cBoldWeight)
        fcweight = FC_WEIGHT_BOLD;

    int type = fontDescription.genericFamily();
    switch (type) {
        case FontDescription::SerifFamily:
            family = (FcChar8*)"sans-serif";
            break;
        case FontDescription::SansSerifFamily:
            family = (FcChar8*)"sans-serif";
            break;
        case FontDescription::NoFamily:
        case FontDescription::StandardFamily:
        default:
            family = (FcChar8*)"sans-serif";
    }
    FcPattern* pattern = FcPatternCreate();
    assert(pattern != 0);
    if (!FcPatternAddString(pattern, FC_FAMILY, family))
        goto freePattern;
    if (!FcPatternAddInteger(pattern, FC_SLANT, fcslant))
        goto freePattern;
    if (!FcPatternAddInteger(pattern, FC_WEIGHT, fcweight))
        goto freePattern;
    if (!FcPatternAddInteger(pattern, FC_PIXEL_SIZE, fcsize))
        goto freePattern;


    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult fcresult;
    m_pattern = FcFontMatch(NULL, pattern, &fcresult);
    if (!m_pattern)
        goto freePattern;

    m_fontFace = cairo_ft_font_face_create_for_pattern(m_pattern);
    m_fontMatrix = (cairo_matrix_t*)malloc(sizeof(cairo_matrix_t));
    cairo_matrix_t ctm;
    cairo_font_extents_t font_extents;
    cairo_text_extents_t text_extents;
    cairo_matrix_init_scale(m_fontMatrix, m_fontDescription.computedPixelSize(), m_fontDescription.computedPixelSize());
    cairo_matrix_init_identity(&ctm);
    m_options = cairo_font_options_create();
    m_scaledFont = cairo_scaled_font_create(m_fontFace, m_fontMatrix, &ctm, m_options);

    assert(m_scaledFont != 0);

freePattern:
    FcPatternDestroy(pattern);
}

bool FontPlatformData::init()
{
    static FcBool FCInitialized;
    if (FCInitialized)
        return true;
    FCInitialized = FcTrue;
    if (!FcInit()) {
        fprintf(stderr, "Can't init font config library\n");
        return false;
    }
    return true;
}

FontPlatformData::~FontPlatformData()
{
    FcPatternDestroy(m_pattern);
    cairo_font_face_destroy(m_fontFace);
    cairo_scaled_font_destroy(m_scaledFont);
    cairo_font_options_destroy(m_options);
}

bool FontPlatformData::isFixedPitch()
{
    int spacing;
    if (FcPatternGetInteger(m_pattern, FC_SPACING, 0, &spacing) == FcResultMatch)
        return spacing == FC_MONO;
    return false;
}

// length is a out value giving the length of the list
cairo_font_face_t** FontPlatformData::list(FontDescription& fontDescription, const AtomicString& familyName, int* length)
{
    init();

    FcPattern* pattern = FcPatternCreate();
    if (!pattern)
        return NULL;

    FcObjectSet* os = FcObjectSetCreate();
    FcObjectSetAdd(os, FC_FAMILY);
    FcObjectSetAdd(os, FC_STYLE);
    FcFontSet* fs = FcFontList(0, pattern, os);
    if (length)
        *length = fs->nfont;
    if (pattern)
        FcPatternDestroy (pattern);
    if (os)
        FcObjectSetDestroy (os);
    cairo_font_face_t** result = (cairo_font_face_t**)malloc((fs->nfont + 1) * sizeof(cairo_font_face_t*));
    for (int i = 0; i < fs->nfont; i++) {
        FcChar8* font = FcNameUnparse(fs->fonts[i]);
        printf("%s\n", font);
        result[i] = cairo_ft_font_face_create_for_pattern(fs->fonts[i]);
    }
    result[fs->nfont] = NULL;
    return result;
}

Glyph FontPlatformData::index(unsigned ucs4) const
{
    ucs4 = (0xff & ucs4);
    FT_Face face = cairo_ft_scaled_font_lock_face(m_scaledFont);
    assert(face != 0);
    int index = FcFreeTypeCharIndex(face, ucs4);
    cairo_ft_scaled_font_unlock_face(m_scaledFont);
    return index;
}


void FontPlatformData::setFont(cairo_t* cr) const
{
    cairo_set_font_face(cr, m_fontFace);
    cairo_set_font_matrix(cr, m_fontMatrix);
    cairo_set_font_options(cr, m_options);
}

bool FontPlatformData::operator==(const FontPlatformData& other) const
{
    if (m_pattern == other.m_pattern)
        return true;
    if (m_pattern == 0 || m_pattern == (FcPattern*)-1
            || other.m_pattern == 0 || other.m_pattern == (FcPattern*)-1)
        return false;
    return FcPatternEqual(m_pattern, other.m_pattern);
}

}
