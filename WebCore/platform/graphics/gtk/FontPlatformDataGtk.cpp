/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2009 Igalia S.L.
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

#include "CString.h"
#include "PlatformString.h"
#include "FontDescription.h"

#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fcfreetype.h>
#include <gtk/gtk.h>

namespace WebCore {

FontPlatformData::FontPlatformData(const FontDescription& fontDescription, const AtomicString& familyName)
    : m_pattern(0)
    , m_fallbacks(0)
    , m_size(fontDescription.computedPixelSize())
    , m_syntheticBold(false)
    , m_syntheticOblique(false)
    , m_scaledFont(0)
{
    FontPlatformData::init();

    CString familyNameString = familyName.string().utf8();
    const char* fcfamily = familyNameString.data();
    int fcslant = FC_SLANT_ROMAN;
    // FIXME: Map all FontWeight values to fontconfig weights.
    int fcweight = FC_WEIGHT_NORMAL;
    double fcsize = fontDescription.computedPixelSize();
    if (fontDescription.italic())
        fcslant = FC_SLANT_ITALIC;
    if (fontDescription.weight() >= FontWeight600)
        fcweight = FC_WEIGHT_BOLD;

    int type = fontDescription.genericFamily();

    FcPattern* pattern = FcPatternCreate();
    cairo_font_face_t* fontFace;
    static const cairo_font_options_t* defaultOptions = cairo_font_options_create();
    const cairo_font_options_t* options = NULL;
    cairo_matrix_t fontMatrix;

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
    case FontDescription::StandardFamily:
        fcfamily = "sans-serif";
        break;
    case FontDescription::NoFamily:
    default:
        fcfamily = NULL;
        break;
    }

    if (fcfamily && !FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(fcfamily)))
        goto freePattern;
    if (!FcPatternAddInteger(pattern, FC_WEIGHT, fcweight))
        goto freePattern;
    if (!FcPatternAddInteger(pattern, FC_SLANT, fcslant))
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
    cairo_matrix_t ctm;
    cairo_matrix_init_scale(&fontMatrix, fontDescription.computedPixelSize(), fontDescription.computedPixelSize());
    cairo_matrix_init_identity(&ctm);

    if (GdkScreen* screen = gdk_screen_get_default())
        options = gdk_screen_get_font_options(screen);

    // gdk_screen_get_font_options() returns NULL if no default options are
    // set, so we always have to check.
    if (!options)
        options = defaultOptions;

    m_scaledFont = cairo_scaled_font_create(fontFace, &fontMatrix, &ctm, options);
    cairo_font_face_destroy(fontFace);

freePattern:
    FcPatternDestroy(pattern);
}

FontPlatformData::FontPlatformData(float size, bool bold, bool italic)
    : m_pattern(0)
    , m_fallbacks(0)
    , m_size(size)
    , m_syntheticBold(bold)
    , m_syntheticOblique(italic)
    , m_scaledFont(0)
{
}

FontPlatformData::FontPlatformData(cairo_font_face_t* fontFace, int size, bool bold, bool italic)
    : m_pattern(0)
    , m_fallbacks(0)
    , m_size(size)
    , m_syntheticBold(bold)
    , m_syntheticOblique(italic)
    , m_scaledFont(0)
{
    cairo_matrix_t fontMatrix;
    cairo_matrix_init_scale(&fontMatrix, size, size);
    cairo_matrix_t ctm;
    cairo_matrix_init_identity(&ctm);
    static const cairo_font_options_t* defaultOptions = cairo_font_options_create();
    const cairo_font_options_t* options = NULL;

    if (GdkScreen* screen = gdk_screen_get_default())
        options = gdk_screen_get_font_options(screen);

    // gdk_screen_get_font_options() returns NULL if no default options are
    // set, so we always have to check.
    if (!options)
        options = defaultOptions;

    m_scaledFont = cairo_scaled_font_create(fontFace, &fontMatrix, &ctm, options);
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& other)
{
    // Check for self-assignment.
    if (this == &other)
        return *this;

    m_size = other.m_size;
    m_syntheticBold = other.m_syntheticBold;
    m_syntheticOblique = other.m_syntheticOblique;

    if (other.m_scaledFont)
        cairo_scaled_font_reference(other.m_scaledFont);
    if (m_scaledFont)
        cairo_scaled_font_destroy(m_scaledFont);
    m_scaledFont = other.m_scaledFont;

    if (other.m_pattern)
        FcPatternReference(other.m_pattern);
    if (m_pattern)
        FcPatternDestroy(m_pattern);
    m_pattern = other.m_pattern;

    if (m_fallbacks) {
        FcFontSetDestroy(m_fallbacks);
        // This will be re-created on demand.
        m_fallbacks = 0;
    }

    return *this;
}

FontPlatformData::FontPlatformData(const FontPlatformData& other)
    : m_pattern(0)
    , m_fallbacks(0)
    , m_scaledFont(0)
{
    *this = other;
}

bool FontPlatformData::init()
{
    static bool initialized = false;
    if (initialized)
        return true;
    if (!FcInit()) {
        fprintf(stderr, "Can't init font config library\n");
        return false;
    }
    initialized = true;
    return true;
}

FontPlatformData::~FontPlatformData()
{
    if (m_pattern && ((FcPattern*)-1 != m_pattern)) {
        FcPatternDestroy(m_pattern);
        m_pattern = 0;
    }

    if (m_fallbacks) {
        FcFontSetDestroy(m_fallbacks);
        m_fallbacks = 0;
    }

    if (m_scaledFont)
        cairo_scaled_font_destroy(m_scaledFont);
}

bool FontPlatformData::isFixedPitch()
{
    // TODO: Support isFixedPitch() for custom fonts.
    if (!m_pattern)
        return false;

    int spacing;
    if (FcPatternGetInteger(m_pattern, FC_SPACING, 0, &spacing) == FcResultMatch)
        return spacing == FC_MONO;
    return false;
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

#ifndef NDEBUG
String FontPlatformData::description() const
{
    return String();
}
#endif

}
