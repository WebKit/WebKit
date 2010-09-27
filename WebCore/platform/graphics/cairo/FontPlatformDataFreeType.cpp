/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "PlatformString.h"
#include "FontDescription.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fcfreetype.h>

#if !PLATFORM(EFL) || ENABLE(GLIB_SUPPORT)
#include <gdk/gdk.h>
#endif

namespace WebCore {

FontPlatformData::FontPlatformData(FcPattern* pattern, const FontDescription& fontDescription)
    : m_pattern(pattern)
    , m_fallbacks(0)
    , m_size(fontDescription.computedPixelSize())
    , m_syntheticBold(false)
    , m_syntheticOblique(false)
{
    // FIXME: We should be getting these options from FontConfig.
    static const cairo_font_options_t* defaultOptions = cairo_font_options_create();
    const cairo_font_options_t* options = 0;
#if !PLATFORM(EFL) || ENABLE(GLIB_SUPPORT)
    if (GdkScreen* screen = gdk_screen_get_default())
        options = gdk_screen_get_font_options(screen);
#endif
    // gdk_screen_get_font_options() returns null if no default options are
    // set, so we always have to check.
    if (!options)
        options = defaultOptions;

    cairo_matrix_t fontMatrix;
    cairo_matrix_init_scale(&fontMatrix, m_size, m_size);
    cairo_matrix_t ctm;
    cairo_matrix_init_identity(&ctm);

    PlatformRefPtr<cairo_font_face_t> fontFace = adoptPlatformRef(cairo_ft_font_face_create_for_pattern(m_pattern.get()));
    m_scaledFont = adoptPlatformRef(cairo_scaled_font_create(fontFace.get(), &fontMatrix, &ctm, options));
}

FontPlatformData::FontPlatformData(float size, bool bold, bool italic)
    : m_fallbacks(0)
    , m_size(size)
    , m_syntheticBold(bold)
    , m_syntheticOblique(italic)
{
}

FontPlatformData::FontPlatformData(cairo_font_face_t* fontFace, float size, bool bold, bool italic)
    : m_fallbacks(0)
    , m_size(size)
    , m_syntheticBold(bold)
    , m_syntheticOblique(italic)
{
    cairo_matrix_t fontMatrix;
    cairo_matrix_init_scale(&fontMatrix, size, size);
    cairo_matrix_t ctm;
    cairo_matrix_init_identity(&ctm);
    static const cairo_font_options_t* defaultOptions = cairo_font_options_create();
    const cairo_font_options_t* options = NULL;

#if !PLATFORM(EFL) || ENABLE(GLIB_SUPPORT)
    if (GdkScreen* screen = gdk_screen_get_default())
        options = gdk_screen_get_font_options(screen);
#endif

    // gdk_screen_get_font_options() returns NULL if no default options are
    // set, so we always have to check.
    if (!options)
        options = defaultOptions;

    m_scaledFont = adoptPlatformRef(cairo_scaled_font_create(fontFace, &fontMatrix, &ctm, options));
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& other)
{
    // Check for self-assignment.
    if (this == &other)
        return *this;

    m_size = other.m_size;
    m_syntheticBold = other.m_syntheticBold;
    m_syntheticOblique = other.m_syntheticOblique;
    m_scaledFont = other.m_scaledFont;
    m_pattern = other.m_pattern;

    if (m_fallbacks) {
        FcFontSetDestroy(m_fallbacks);
        // This will be re-created on demand.
        m_fallbacks = 0;
    }

    return *this;
}

FontPlatformData::FontPlatformData(const FontPlatformData& other)
    : m_fallbacks(0)
{
    *this = other;
}

FontPlatformData::~FontPlatformData()
{
    if (m_fallbacks) {
        FcFontSetDestroy(m_fallbacks);
        m_fallbacks = 0;
    }
}

bool FontPlatformData::isFixedPitch()
{
    // TODO: Support isFixedPitch() for custom fonts.
    if (!m_pattern)
        return false;

    int spacing;
    if (FcPatternGetInteger(m_pattern.get(), FC_SPACING, 0, &spacing) == FcResultMatch)
        return spacing == FC_MONO;
    return false;
}

bool FontPlatformData::operator==(const FontPlatformData& other) const
{
    if (m_pattern == other.m_pattern)
        return true;
    if (!m_pattern || m_pattern.isHashTableDeletedValue() || !other.m_pattern || other.m_pattern.isHashTableDeletedValue())
        return false;
    return FcPatternEqual(m_pattern.get(), other.m_pattern.get());
}

#ifndef NDEBUG
String FontPlatformData::description() const
{
    return String();
}
#endif

}
