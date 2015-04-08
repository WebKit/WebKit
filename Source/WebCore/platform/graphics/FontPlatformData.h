/*
 * Copyright (C) 2006, 2007, 2008, 2010, 2013 Apple Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Pioneer Research Center USA, Inc.
 * Copyright (C) 2010, 2011 Brent Fulgham <bfulgham@webkit.org>
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

// FIXME: This is temporary until all ports switch to using this file.
#if PLATFORM(EFL) || PLATFORM(GTK)
#include "freetype/FontPlatformData.h"
#else

#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "FontOrientation.h"
#include "FontWidthVariant.h"

#if PLATFORM(WIN)
#include "SharedGDIObject.h"
#endif

#if USE(CAIRO)
#include <wtf/HashFunctions.h>
#include <cairo.h>
#endif

#if PLATFORM(COCOA)
#if PLATFORM(IOS)
#import <CoreGraphics/CoreGraphics.h>
#endif
#if USE(APPKIT)
OBJC_CLASS NSFont;
#endif

typedef const struct __CTFont* CTFontRef;

#endif

#if USE(CG)
typedef struct CGFont* CGFontRef;
#endif

#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/StringImpl.h>

#if PLATFORM(WIN)
#include <wtf/win/GDIObject.h>
typedef struct HFONT__* HFONT;
#endif


namespace WebCore {

class FontDescription;
class SharedBuffer;

class FontPlatformData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontPlatformData(WTF::HashTableDeletedValueType);
    FontPlatformData();
    FontPlatformData(const FontPlatformData&);
    FontPlatformData(const FontDescription&, const AtomicString& family);
    FontPlatformData(float size, bool syntheticBold, bool syntheticOblique, FontOrientation = Horizontal, FontWidthVariant = RegularWidth);

#if PLATFORM(COCOA)
    WEBCORE_EXPORT FontPlatformData(CTFontRef, float size, bool syntheticBold = false, bool syntheticOblique = false, FontOrientation = Horizontal, FontWidthVariant = RegularWidth);
#if USE(APPKIT)
    // FIXME: Remove this when all NSFont usage is removed.
    WEBCORE_EXPORT FontPlatformData(NSFont *, float size, bool syntheticBold = false, bool syntheticOblique = false, FontOrientation = Horizontal, FontWidthVariant = RegularWidth);
#endif
#endif

#if USE(CG)
    FontPlatformData(CGFontRef, float size, bool syntheticBold, bool syntheticOblique, FontOrientation, FontWidthVariant);
#endif

#if PLATFORM(WIN)
    FontPlatformData(GDIObject<HFONT>, float size, bool syntheticBold, bool syntheticOblique, bool useGDI);

#if USE(CG)
    FontPlatformData(GDIObject<HFONT>, CGFontRef, float size, bool syntheticBold, bool syntheticOblique, bool useGDI);
#elif USE(CAIRO)
    FontPlatformData(GDIObject<HFONT>, cairo_font_face_t*, float size, bool bold, bool italic);
#endif
#endif

    WEBCORE_EXPORT ~FontPlatformData();

#if PLATFORM(WIN)
    HFONT hfont() const { return m_font ? m_font->get() : 0; }
    bool useGDI() const { return m_useGDI; }
#elif PLATFORM(COCOA)
    CTFontRef font() const { return m_font; }
    void setFont(CTFontRef);

    CTFontRef ctFont() const;
    static RetainPtr<CFTypeRef> objectForEqualityCheck(CTFontRef);
    RetainPtr<CFTypeRef> objectForEqualityCheck() const;

    bool allowsLigatures() const;
    bool roundsGlyphAdvances() const;

#if USE(APPKIT)
    // FIXME: Remove this when all NSFont usage is removed.
    NSFont *nsFont() const { return (NSFont *)m_font; }
    void setNSFont(NSFont *font) { setFont((CTFontRef)font); }
#endif
#endif

#if USE(CG)
    CGFontRef cgFont() const { return m_cgFont.get(); }
#endif

    bool isFixedPitch() const;
    float size() const { return m_size; }
    void setSize(float size) { m_size = size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }
    bool isColorBitmapFont() const { return m_isColorBitmapFont; }
    bool isCompositeFontReference() const { return m_isCompositeFontReference; }
    FontOrientation orientation() const { return m_orientation; }
    FontWidthVariant widthVariant() const { return m_widthVariant; }

    void setOrientation(FontOrientation orientation) { m_orientation = orientation; }

#if USE(CAIRO)
    cairo_scaled_font_t* scaledFont() const { return m_scaledFont; }
#endif

    unsigned hash() const
    {
#if PLATFORM(WIN) && !USE(CAIRO)
        return m_font ? m_font->hash() : 0;
#elif OS(DARWIN)
#if USE(APPKIT)
        ASSERT(m_font || !m_cgFont);
        uintptr_t hashCodes[3] = { (uintptr_t)m_font, m_widthVariant, static_cast<uintptr_t>(m_orientation << 2 | m_syntheticBold << 1 | m_syntheticOblique) };
#else
        ASSERT(m_font || !m_cgFont || m_isEmoji);
        uintptr_t hashCodes[3] = { static_cast<uintptr_t>(CFHash(m_font)), m_widthVariant, static_cast<uintptr_t>(m_isEmoji << 3 | m_orientation << 2 | m_syntheticBold << 1 | m_syntheticOblique) };
#endif // !PLATFORM(IOS)
        return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
#elif USE(CAIRO)
        return PtrHash<cairo_scaled_font_t*>::hash(m_scaledFont);
#endif
    }

    const FontPlatformData& operator=(const FontPlatformData&);

    bool operator==(const FontPlatformData& other) const
    {
        return platformIsEqual(other)
            && m_size == other.m_size
            && m_syntheticBold == other.m_syntheticBold
            && m_syntheticOblique == other.m_syntheticOblique
            && m_isColorBitmapFont == other.m_isColorBitmapFont
            && m_isCompositeFontReference == other.m_isCompositeFontReference
            && m_orientation == other.m_orientation
            && m_widthVariant == other.m_widthVariant;
    }

    bool isHashTableDeletedValue() const
    {
#if PLATFORM(WIN) && !USE(CAIRO)
        return m_font.isHashTableDeletedValue();
#elif PLATFORM(COCOA)
        return m_font == hashTableDeletedFontValue();
#elif USE(CAIRO)
        return m_scaledFont == hashTableDeletedFontValue();
#endif
    }

#if PLATFORM(COCOA) || PLATFORM(WIN)
    PassRefPtr<SharedBuffer> openTypeTable(uint32_t table) const;
#endif

#ifndef NDEBUG
    String description() const;
#endif

private:
    bool platformIsEqual(const FontPlatformData&) const;
    void platformDataInit(const FontPlatformData&);
    const FontPlatformData& platformDataAssign(const FontPlatformData&);
#if PLATFORM(COCOA)
    static CTFontRef hashTableDeletedFontValue() { return reinterpret_cast<CTFontRef>(-1); }
#endif
#if PLATFORM(WIN)
    void platformDataInit(HFONT, float size, HDC, WCHAR* faceName);
#endif
#if USE(CAIRO)
    static cairo_scaled_font_t* hashTableDeletedFontValue() { return reinterpret_cast<cairo_scaled_font_t*>(-1); }
#endif

public:
    bool m_syntheticBold { false };
    bool m_syntheticOblique { false };
    FontOrientation m_orientation { Horizontal };
#if PLATFORM(IOS)
    bool m_isEmoji { false };
#endif
    float m_size { 0 };
    FontWidthVariant m_widthVariant { RegularWidth };

private:
#if PLATFORM(COCOA)
    // FIXME: Get rid of one of these. These two fonts are subtly different, and it is not obvious which one to use where.
    CTFontRef m_font { nullptr };
    mutable RetainPtr<CTFontRef> m_ctFont;
#elif PLATFORM(WIN)
    RefPtr<SharedGDIObject<HFONT>> m_font;
#endif

#if USE(CG)
    RetainPtr<CGFontRef> m_cgFont;
#endif
#if USE(CAIRO)
    cairo_scaled_font_t* m_scaledFont { nullptr };
#endif

    bool m_isColorBitmapFont { false };
    bool m_isCompositeFontReference { false };

#if PLATFORM(WIN)
    bool m_useGDI { false };
#endif
};

} // namespace WebCore

#endif // FontPlatformData_h

#endif
