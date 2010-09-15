/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
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

#ifndef FontPlatformDataFreeType_h
#define FontPlatformDataFreeType_h

#include "FontDescription.h"
#include "GlyphBuffer.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fcfreetype.h>
#include <wtf/Forward.h>

namespace WebCore {

class FontPlatformData {
public:
    FontPlatformData(WTF::HashTableDeletedValueType)
        : m_pattern(hashTableDeletedFontValue())
        , m_fallbacks(0)
        , m_size(0)
        , m_syntheticBold(false)
        , m_syntheticOblique(false)
        , m_scaledFont(0)
        { }

    FontPlatformData()
        : m_pattern(0)
        , m_fallbacks(0)
        , m_size(0)
        , m_syntheticBold(false)
        , m_syntheticOblique(false)
        , m_scaledFont(0)
        { }

    FontPlatformData(const FontDescription&, const AtomicString& family);
    FontPlatformData(cairo_font_face_t* fontFace, float size, bool bold, bool italic);
    FontPlatformData(float size, bool bold, bool italic);
    FontPlatformData(const FontPlatformData&);

    ~FontPlatformData();

    static bool init();
    bool isFixedPitch();
    float size() const { return m_size; }
    void setSize(float size) { m_size = size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }

    cairo_scaled_font_t* scaledFont() const { return m_scaledFont; }

    unsigned hash() const
    {
        if (m_pattern)
            return FcPatternHash(m_pattern);
    }

    bool operator==(const FontPlatformData&) const;
    FontPlatformData& operator=(const FontPlatformData&);
    bool isHashTableDeletedValue() const
    {
        return m_pattern == hashTableDeletedFontValue();
    }

#ifndef NDEBUG
    String description() const;
#endif

    FcPattern* m_pattern;
    FcFontSet* m_fallbacks;
    float m_size;
    bool m_syntheticBold;
    bool m_syntheticOblique;
    cairo_scaled_font_t* m_scaledFont;

private:
    static FcPattern *hashTableDeletedFontValue() { return reinterpret_cast<FcPattern*>(-1); }
};

}

#endif // FontPlatformDataFreeType_h
