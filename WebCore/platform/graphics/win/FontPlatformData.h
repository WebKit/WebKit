/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006, 2007, 2008 Apple Inc.
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

#ifndef FontPlatformData_H
#define FontPlatformData_H

#include "StringImpl.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/RefCounted.h>

#if PLATFORM(CAIRO)
#include <cairo-win32.h>
#endif

typedef struct HFONT__* HFONT;
typedef struct CGFont* CGFontRef;

namespace WebCore {

class FontDescription;

class FontPlatformData {
public:
    FontPlatformData()
#if PLATFORM(CAIRO)
        : m_fontFace(0)
        , m_scaledFont(0)
        ,
#else
        :
#endif
          m_size(0)
        , m_syntheticBold(false)
        , m_syntheticOblique(false)
        , m_useGDI(false)
    {
    }

    FontPlatformData(HFONT, float size, bool bold, bool oblique, bool useGDI);
    FontPlatformData(float size, bool bold, bool oblique);

#if PLATFORM(CG)
    FontPlatformData(HFONT, CGFontRef, float size, bool bold, bool oblique, bool useGDI);
#elif PLATFORM(CAIRO)
    FontPlatformData(cairo_font_face_t*, float size, bool bold, bool oblique);
    FontPlatformData(const FontPlatformData&);

    FontPlatformData& operator=(const FontPlatformData&);
#endif
    ~FontPlatformData();

    FontPlatformData(WTF::HashTableDeletedValueType) : m_font(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_font.isHashTableDeletedValue(); }

    HFONT hfont() const { return m_font->hfont(); }
#if PLATFORM(CG)
    CGFontRef cgFont() const { return m_cgFont.get(); }
#elif PLATFORM(CAIRO)
    void setFont(cairo_t* ft) const;
    cairo_font_face_t* fontFace() const { return m_fontFace; }
    cairo_scaled_font_t* scaledFont() const { return m_scaledFont; }
#endif

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
        return m_font == other.m_font &&
#if PLATFORM(CG)
               m_cgFont == other.m_cgFont &&
#elif PLATFORM(CAIRO)
               m_fontFace == other.m_fontFace &&
               m_scaledFont == other.m_scaledFont &&
#endif
               m_size == other.m_size &&
               m_syntheticBold == other.m_syntheticBold && m_syntheticOblique == other.m_syntheticOblique &&
               m_useGDI == other.m_useGDI;
    }

private:
    class RefCountedHFONT : public RefCounted<RefCountedHFONT> {
    public:
        static PassRefPtr<RefCountedHFONT> create(HFONT hfont) { return adoptRef(new RefCountedHFONT(hfont)); }
        static PassRefPtr<RefCountedHFONT> createDeleted() { return adoptRef(new RefCountedHFONT(reinterpret_cast<HFONT>(-1))); }

            ~RefCountedHFONT() { if (m_hfont != reinterpret_cast<HFONT>(-1)) DeleteObject(m_hfont); }

        HFONT hfont() const { return m_hfont; }
        unsigned hash() const
        {
            return StringImpl::computeHash(reinterpret_cast<const UChar*>(&m_hfont), sizeof(HFONT) / sizeof(UChar));
        }

    private:
        RefCountedHFONT(HFONT hfont)
            : m_hfont(hfont)
        {
        }

        HFONT m_hfont;
    };

    void platformDataInit(HFONT font, float size, HDC hdc, WCHAR* faceName);

    RefPtr<RefCountedHFONT> m_font;
#if PLATFORM(CG)
    RetainPtr<CGFontRef> m_cgFont;
#elif PLATFORM(CAIRO)
    cairo_font_face_t* m_fontFace;
    cairo_scaled_font_t* m_scaledFont;
#endif

    float m_size;
    bool m_syntheticBold;
    bool m_syntheticOblique;
    bool m_useGDI;
};

}

#endif
