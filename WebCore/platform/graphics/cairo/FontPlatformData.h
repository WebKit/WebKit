/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
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

#ifndef FontPlatformData_h
#define FontPlatformData_h

#include <cairo.h>
#include "FontDescription.h"
#include "GlyphBuffer.h"

#if defined(USE_FREETYPE)
#include <cairo-ft.h>
#include <fontconfig/fcfreetype.h>
#elif defined(USE_PANGO)
#include <pango/pangocairo.h>
#elif PLATFORM(WIN)
#include <cairo-win32.h>
#include "RefCountedGDIHandle.h"
#include "StringImpl.h"
#else
#error "Must defined a font backend"
#endif

#if PLATFORM(WIN)
typedef struct HFONT__* HFONT;
#endif
namespace WebCore {

class String;

class FontPlatformData {
public:
    FontPlatformData(WTF::HashTableDeletedValueType)
#if defined(USE_FREETYPE)
        : m_pattern(hashTableDeletedFontValue())
        , m_fallbacks(0)
#elif defined(USE_PANGO)
        : m_context(0)
        , m_font(hashTableDeletedFontValue())
#elif PLATFORM(WIN)
        : m_fontFace(0)
        , m_useGDI(false)
        , m_font(WTF::HashTableDeletedValue)
#else
#error "Must defined a font backend"
#endif
        , m_size(0)
        , m_syntheticBold(false)
        , m_syntheticOblique(false)
        , m_scaledFont(0)
        { }

    FontPlatformData()
#if defined(USE_FREETYPE)
        : m_pattern(0)
        , m_fallbacks(0)
#elif defined(USE_PANGO)
        : m_context(0)
        , m_font(0)
#elif PLATFORM(WIN)
        : m_fontFace(0)
        , m_useGDI(false)
#else
#error "Must defined a font backend"
#endif
        , m_size(0)
        , m_syntheticBold(false)
        , m_syntheticOblique(false)
        , m_scaledFont(0)
        { }

#if PLATFORM(WIN)
    FontPlatformData(HFONT, float size, bool bold, bool oblique, bool useGDI);
#else
    FontPlatformData(const FontDescription&, const AtomicString& family);
#endif

    FontPlatformData(cairo_font_face_t* fontFace, float size, bool bold, bool italic);
    FontPlatformData(float size, bool bold, bool italic);
    FontPlatformData(const FontPlatformData&);

    ~FontPlatformData();

#if !PLATFORM(WIN)
    static bool init();
#else
    HFONT hfont() const { return m_font->handle(); }
    bool useGDI() const { return m_useGDI; }
    cairo_font_face_t* fontFace() const { return m_fontFace; }
#endif

    bool isFixedPitch();
    float size() const { return m_size; }
    void setSize(float size) { m_size = size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }

    cairo_scaled_font_t* scaledFont() const { return m_scaledFont; }

    unsigned hash() const
    {
#if PLATFORM(WIN)
        return m_font->hash();
#else
#if defined(USE_FREETYPE)
        if (m_pattern)
            return FcPatternHash(m_pattern);
#endif
        uintptr_t hashCodes[1] = { reinterpret_cast<uintptr_t>(m_scaledFont) };
        return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), sizeof(hashCodes) / sizeof(UChar));
#endif
    }

    bool operator==(const FontPlatformData&) const;
    FontPlatformData& operator=(const FontPlatformData&);
    bool isHashTableDeletedValue() const
    {
#if defined(USE_FREETYPE)
        return m_pattern == hashTableDeletedFontValue();
#elif defined(USE_PANGO)
        return m_font == hashTableDeletedFontValue();
#elif PLATFORM(WIN)
        return m_font.isHashTableDeletedValue();
#endif
    }

#ifndef NDEBUG
    String description() const;
#endif

#if defined(USE_FREETYPE)
    FcPattern* m_pattern;
    FcFontSet* m_fallbacks;
#elif defined(USE_PANGO)
    static PangoFontMap* m_fontMap;
    static GHashTable* m_hashTable;

    PangoContext* m_context;
    PangoFont* m_font;
#elif PLATFORM(WIN)
private:
    void platformDataInit(HFONT, float size, HDC, WCHAR* faceName);

    RefPtr<RefCountedGDIHandle<HFONT> > m_font;
    cairo_font_face_t* m_fontFace;
    bool m_useGDI;
#else
#error "Must defined a font backend"
#endif
    float m_size;
    bool m_syntheticBold;
    bool m_syntheticOblique;
    cairo_scaled_font_t* m_scaledFont;
private:
#if defined(USE_FREETYPE)
    static FcPattern *hashTableDeletedFontValue() { return reinterpret_cast<FcPattern*>(-1); }
#elif defined(USE_PANGO)
    static PangoFont *hashTableDeletedFontValue() { return reinterpret_cast<PangoFont*>(-1); }
#endif
};

}

#endif
