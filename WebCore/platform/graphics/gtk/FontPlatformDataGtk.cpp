/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Pioneer Research Center USA, Inc.
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
#include <cairo.h>
#include <assert.h>

namespace WebCore {

PangoFontMap* FontPlatformData::m_fontMap = 0;
GHashTable* FontPlatformData::m_hashTable = 0;

FontPlatformData::FontPlatformData(const FontDescription& fontDescription, const AtomicString& familyName)
    : m_context(0)
    , m_font(0)
    , m_fontDescription(fontDescription)
    , m_scaledFont(0)
{
    FontPlatformData::init();

    CString  stored_family = familyName.domString().utf8();
    gchar const* families[] = {
        stored_family.data(),
        NULL
    };

    switch (fontDescription.genericFamily()) {
    case FontDescription::SerifFamily:
        families[1] = "serif";
        break;
    case FontDescription::SansSerifFamily:
        families[1] = "sans";
        break;
    case FontDescription::MonospaceFamily:
        families[1] = "monospace";
        break;
    case FontDescription::NoFamily:
    case FontDescription::StandardFamily:
    default:
        families[1] = "sans";
        break;
    }

    PangoFontDescription* description = pango_font_description_new();
    pango_font_description_set_absolute_size(description, fontDescription.computedSize() * PANGO_SCALE);

    if (fontDescription.bold())
        pango_font_description_set_weight(description, PANGO_WEIGHT_BOLD);
    if (fontDescription.italic())
        pango_font_description_set_style(description, PANGO_STYLE_ITALIC);

    m_context = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(m_fontMap));

    for (unsigned int i = 0; !m_font && i < G_N_ELEMENTS(families); i++) {
        pango_font_description_set_family(description, families[i]);
        m_font = pango_font_map_load_font(m_fontMap, m_context, description);
    }

    // FIXME: should we set some default font?
    if (m_font)
        m_scaledFont = pango_cairo_font_get_scaled_font(PANGO_CAIRO_FONT(m_font));

    pango_font_description_free(description);
}

bool FontPlatformData::init()
{
    static bool initialized;
    if (initialized)
        return true;
    initialized = true;

    if (!m_fontMap)
        m_fontMap = pango_cairo_font_map_new();
    if (!m_hashTable) {
        PangoFontFamily**families = 0;
        int n_families = 0;

        m_hashTable = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);

        pango_font_map_list_families(m_fontMap, &families, &n_families);

        for (int family = 0; family < n_families; family++)
                g_hash_table_insert(m_hashTable,
                                    g_strdup(pango_font_family_get_name(families[family])),
                                    g_object_ref(families[family]));

        g_free(families);
    }

    return true;
}

FontPlatformData::~FontPlatformData()
{
}

bool FontPlatformData::isFixedPitch()
{
    PangoFontDescription* description = pango_font_describe_with_absolute_size(m_font);
    PangoFontFamily* family = reinterpret_cast<PangoFontFamily*>(g_hash_table_lookup(m_hashTable, pango_font_description_get_family(description)));
    pango_font_description_free(description);

    return pango_font_family_is_monospace(family);
}

void FontPlatformData::setFont(cairo_t* cr) const
{
    ASSERT(m_scaledFont);

    cairo_set_scaled_font(cr, m_scaledFont);
}

bool FontPlatformData::operator==(const FontPlatformData& other) const
{
    if (m_font == other.m_font)
        return true;
    if (m_font == 0 || m_font == reinterpret_cast<PangoFont*>(-1)
            || other.m_font == 0 || other.m_font == reinterpret_cast<PangoFont*>(-1))
        return false;

    PangoFontDescription* thisDesc = pango_font_describe(m_font);
    PangoFontDescription* otherDesc = pango_font_describe(other.m_font);
    bool result = pango_font_description_equal(thisDesc, otherDesc);
    pango_font_description_free(otherDesc);
    pango_font_description_free(thisDesc);
    return result;
}

}
