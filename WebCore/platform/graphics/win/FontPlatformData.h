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

#ifndef FontPlatformDataCairoWin_h
#define FontPlatformDataCairoWin_h

#include "FontDescription.h"
#include "GlyphBuffer.h"
#include "RefCountedGDIHandle.h"
#include "StringImpl.h"
#include <cairo-win32.h>
#include <cairo.h>
#include <wtf/Forward.h>

typedef struct HFONT__* HFONT;

namespace WebCore {

class FontPlatformData {
public:
    FontPlatformData(WTF::HashTableDeletedValueType)
        : m_fontFace(0)
        , m_useGDI(false)
        , m_font(WTF::HashTableDeletedValue)
        , m_size(0)
        , m_syntheticBold(false)
        , m_syntheticOblique(false)
        , m_scaledFont(0)
        { }

    FontPlatformData()
        : m_fontFace(0)
        , m_useGDI(false)
        , m_size(0)
        , m_syntheticBold(false)
        , m_syntheticOblique(false)
        , m_scaledFont(0)
        { }

    FontPlatformData(HFONT, float size, bool bold, bool oblique, bool useGDI);
    FontPlatformData(cairo_font_face_t* fontFace, float size, bool bold, bool italic);
    FontPlatformData(float size, bool bold, bool italic);
    FontPlatformData(const FontPlatformData&);
    ~FontPlatformData();

    HFONT hfont() const { return m_font->handle(); }
    bool useGDI() const { return m_useGDI; }
    cairo_font_face_t* fontFace() const { return m_fontFace; }

    bool isFixedPitch();
    float size() const { return m_size; }
    void setSize(float size) { m_size = size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }
    cairo_scaled_font_t* scaledFont() const { return m_scaledFont; }

    unsigned hash() const
    {
        return m_font->hash();
    }

    bool operator==(const FontPlatformData&) const;
    FontPlatformData& operator=(const FontPlatformData&);
    bool isHashTableDeletedValue() const
    {
        return m_font.isHashTableDeletedValue();
    }

#ifndef NDEBUG
    String description() const;
#endif

private:
    void platformDataInit(HFONT, float size, HDC, WCHAR* faceName);

    RefPtr<RefCountedGDIHandle<HFONT> > m_font;
    cairo_font_face_t* m_fontFace;
    bool m_useGDI;
    float m_size;
    bool m_syntheticBold;
    bool m_syntheticOblique;
    cairo_scaled_font_t* m_scaledFont;
};

}

#endif // FontPlatformDataCairoWin_h
