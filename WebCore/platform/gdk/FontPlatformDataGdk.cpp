/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

    const char* fcfamily = familyName.deprecatedString().ascii();
    int fcslant = FC_SLANT_ROMAN;
    int fcweight = FC_WEIGHT_NORMAL;
    float fcsize = fontDescription.computedSize();
    if (fontDescription.italic())
        fcslant = FC_SLANT_ITALIC;
    if (fontDescription.bold())
        fcweight = FC_WEIGHT_BOLD;

    int type = fontDescription.genericFamily();

    FcPattern* pattern = FcPatternCreate();

    if (!FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(fcfamily)))
        goto freePattern;

    switch (type) {
        case FontDescription::SerifFamily:
            fcfamily = "serif";
            break;
        case FontDescription::SansSerifFamily:
            fcfamily = "sans-serif";
            break;
        case FontDescription::MonospaceFamily:
            fcfamily = "monospace";
            break;
        case FontDescription::NoFamily:
        case FontDescription::StandardFamily:
        default:
            fcfamily = "sans-serif";
    }

    if (!FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(fcfamily)))
        goto freePattern;
    if (!FcPatternAddInteger(pattern, FC_SLANT, fcslant))
        goto freePattern;
    if (!FcPatternAddInteger(pattern, FC_WEIGHT, fcweight))
        goto freePattern;
    if (!FcPatternAddDouble(pattern, FC_PIXEL_SIZE, fcsize))
        goto freePattern;

    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult fcresult;
    m_pattern = FcFontMatch(NULL, pattern, &fcresult);
    // FIXME: should we set some default font?
    if (!m_pattern)
        goto freePattern;
    m_fontFace = cairo_ft_font_face_create_for_pattern(m_pattern);
    m_fontMatrix = (cairo_matrix_t*)malloc(sizeof(cairo_matrix_t));
    cairo_matrix_t ctm;
    cairo_matrix_init_scale(m_fontMatrix, m_fontDescription.computedSize(), m_fontDescription.computedSize());
    cairo_matrix_init_identity(&ctm);
    m_options = cairo_font_options_create();
    m_scaledFont = cairo_scaled_font_create(m_fontFace, m_fontMatrix, &ctm, m_options);

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
    if (m_pattern && ((FcPattern*)-1 != m_pattern))
        FcPatternDestroy(m_pattern);
    free(m_fontMatrix);
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
