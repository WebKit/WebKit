/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
 *
 */

#include "config.h"
#include "FontPlatformData.h"

#include "CString.h"
#include "PlatformString.h"
#include "FontDescription.h"
#include "cairo-ft.h"
#include "cairo.h"
#include <fontconfig/fcfreetype.h>
#include <assert.h>

namespace WebCore {

FontPlatformData::FontPlatformData(const FontDescription& fontDescription, const AtomicString& familyName)
    : m_pattern(0)
    , m_fontDescription(fontDescription)
    , m_scaledFont(0)
{
    FontPlatformData::init();

    CString familyNameString = familyName.domString().utf8();
    const char* fcfamily = familyNameString.data();
    int fcslant = FC_SLANT_ROMAN;
    int fcweight = FC_WEIGHT_NORMAL;
    float fcsize = fontDescription.computedSize();
    if (fontDescription.italic())
        fcslant = FC_SLANT_ITALIC;
    if (fontDescription.bold())
        fcweight = FC_WEIGHT_BOLD;

    int type = fontDescription.genericFamily();

    FcPattern* pattern = FcPatternCreate();
    cairo_font_face_t* fontFace;
    cairo_font_options_t* options;
    cairo_matrix_t* fontMatrix;

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
    fontFace = cairo_ft_font_face_create_for_pattern(m_pattern);
    fontMatrix = reinterpret_cast<cairo_matrix_t*>(malloc(sizeof(cairo_matrix_t)));
    cairo_matrix_t ctm;
    cairo_matrix_init_scale(fontMatrix, m_fontDescription.computedSize(), m_fontDescription.computedSize());
    cairo_matrix_init_identity(&ctm);
    options = cairo_font_options_create();
    m_scaledFont = cairo_scaled_font_create(fontFace, fontMatrix, &ctm, options);
    cairo_font_face_destroy(fontFace);
    cairo_font_options_destroy(options);
    free(fontMatrix);

freePattern:
    FcPatternDestroy(pattern);
}

bool FontPlatformData::init()
{
    static bool initialized;
    if (initialized)
        return true;
    initialized = true;
    if (!FcInit()) {
        fprintf(stderr, "Can't init font config library\n");
        return false;
    }
    return true;
}

FontPlatformData::~FontPlatformData()
{
}

bool FontPlatformData::isFixedPitch()
{
    int spacing;
    if (FcPatternGetInteger(m_pattern, FC_SPACING, 0, &spacing) == FcResultMatch)
        return spacing == FC_MONO;
    return false;
}

void FontPlatformData::setFont(cairo_t* cr) const
{
    ASSERT(m_scaledFont);

    cairo_set_scaled_font(cr, m_scaledFont);
}

bool FontPlatformData::operator==(const FontPlatformData& other) const
{
    if (m_pattern == other.m_pattern)
        return true;
    if (m_pattern == 0 || m_pattern == reinterpret_cast<FcPattern*>(-1)
            || other.m_pattern == 0 || other.m_pattern == reinterpret_cast<FcPattern*>(-1))
        return false;
    return FcPatternEqual(m_pattern, other.m_pattern);
}

}
