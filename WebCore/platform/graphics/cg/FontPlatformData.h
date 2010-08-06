/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc.
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

#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "RefCountedGDIHandle.h"
#include "StringImpl.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>

typedef struct HFONT__* HFONT;
typedef struct CGFont* CGFontRef;

namespace WebCore {

class FontDescription;
class String;

class FontPlatformData {
public:
    FontPlatformData()
        : m_size(0)
        , m_syntheticBold(false)
        , m_syntheticOblique(false)
        , m_useGDI(false)
    {
    }

    FontPlatformData(HFONT, float size, bool bold, bool oblique, bool useGDI);
    FontPlatformData(float size, bool bold, bool oblique);

    FontPlatformData(HFONT, CGFontRef, float size, bool bold, bool oblique, bool useGDI);
    ~FontPlatformData();

    FontPlatformData(WTF::HashTableDeletedValueType) : m_font(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_font.isHashTableDeletedValue(); }

    HFONT hfont() const { return m_font->handle(); }
    CGFontRef cgFont() const { return m_cgFont.get(); }

    float size() const { return m_size; }
    void setSize(float size) { m_size = size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }
    bool useGDI() const { return m_useGDI; }

    unsigned hash() const
    {
        return m_font->hash();
    }

    bool operator==(const FontPlatformData& other) const
    { 
        return m_font == other.m_font
            && m_cgFont == other.m_cgFont
            && m_size == other.m_size
            && m_syntheticBold == other.m_syntheticBold
            && m_syntheticOblique == other.m_syntheticOblique
            && m_useGDI == other.m_useGDI;
    }

#ifndef NDEBUG
    String description() const;
#endif

private:
    void platformDataInit(HFONT, float size, HDC, WCHAR* faceName);

    RefPtr<RefCountedGDIHandle<HFONT> > m_font;
    RetainPtr<CGFontRef> m_cgFont;

    float m_size;
    bool m_syntheticBold;
    bool m_syntheticOblique;
    bool m_useGDI;
};

}

#endif
