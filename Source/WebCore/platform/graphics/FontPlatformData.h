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

#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "TextFlags.h"

#if PLATFORM(WIN)
#include "SharedGDIObject.h"
#endif

#if USE(CAIRO)
#include "RefPtrCairo.h"
#include <wtf/HashFunctions.h>
#include <cairo.h>
#endif

#if USE(FREETYPE)
#include "FcUniquePtr.h"
#include "HarfBuzzFace.h"
#include "OpenTypeVerticalData.h"
#endif

#if PLATFORM(COCOA)
#if USE(APPKIT)
OBJC_CLASS NSFont;
#endif

typedef const struct __CTFont* CTFontRef;

#endif

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
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

// This class is conceptually immutable. Once created, no instances should ever change (in an observable way).
class FontPlatformData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontPlatformData(WTF::HashTableDeletedValueType);
    FontPlatformData();
#if !USE(FREETYPE)
    FontPlatformData(const FontPlatformData&) = default;
#endif
    FontPlatformData(FontPlatformData&&) = default;
    FontPlatformData(const FontDescription&, const AtomicString& family);
    FontPlatformData(float size, bool syntheticBold, bool syntheticOblique, FontOrientation = Horizontal, FontWidthVariant = RegularWidth, TextRenderingMode = AutoTextRendering);

#if PLATFORM(COCOA)
    WEBCORE_EXPORT FontPlatformData(CTFontRef, float size, bool syntheticBold = false, bool syntheticOblique = false, FontOrientation = Horizontal, FontWidthVariant = RegularWidth, TextRenderingMode = AutoTextRendering);
#endif

    static FontPlatformData cloneWithOrientation(const FontPlatformData&, FontOrientation);
    static FontPlatformData cloneWithSyntheticOblique(const FontPlatformData&, bool);
    static FontPlatformData cloneWithSize(const FontPlatformData&, float);

#if USE(CG)
    FontPlatformData(CGFontRef, float size, bool syntheticBold, bool syntheticOblique, FontOrientation, FontWidthVariant, TextRenderingMode);
#endif

#if PLATFORM(WIN)
    FontPlatformData(GDIObject<HFONT>, float size, bool syntheticBold, bool syntheticOblique, bool useGDI);

#if USE(CG)
    FontPlatformData(GDIObject<HFONT>, CGFontRef, float size, bool syntheticBold, bool syntheticOblique, bool useGDI);
#elif USE(CAIRO)
    FontPlatformData(GDIObject<HFONT>, cairo_font_face_t*, float size, bool bold, bool italic);
#endif
#endif

#if USE(FREETYPE)
    FontPlatformData(FcPattern*, const FontDescription&);
    FontPlatformData(cairo_font_face_t*, const FontDescription&, bool syntheticBold, bool syntheticOblique);
    FontPlatformData(const FontPlatformData&);
    ~FontPlatformData();
#endif

#if PLATFORM(WIN)
    HFONT hfont() const { return m_font ? m_font->get() : 0; }
    bool useGDI() const { return m_useGDI; }
#elif PLATFORM(COCOA)
    CTFontRef font() const { return m_font.get(); }
    WEBCORE_EXPORT CTFontRef registeredFont() const; // Returns nullptr iff the font is not registered, such as web fonts (otherwise returns font()).

    CTFontRef ctFont() const;
    static RetainPtr<CFTypeRef> objectForEqualityCheck(CTFontRef);
    RetainPtr<CFTypeRef> objectForEqualityCheck() const;

    bool hasCustomTracking() const { return isSystemFont(); }
#endif

#if PLATFORM(WIN) || PLATFORM(COCOA)
    bool isSystemFont() const { return m_isSystemFont; }
#endif

#if USE(CG)
    CGFontRef cgFont() const { return m_cgFont.get(); }
#endif

    bool isFixedPitch() const;
    float size() const { return m_size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }
    bool isColorBitmapFont() const { return m_isColorBitmapFont; }
    FontOrientation orientation() const { return m_orientation; }
    FontWidthVariant widthVariant() const { return m_widthVariant; }
    TextRenderingMode textRenderingMode() const { return m_textRenderingMode; }
    bool isForTextCombine() const { return widthVariant() != RegularWidth; } // Keep in sync with callers of FontDescription::setWidthVariant().

#if USE(CAIRO)
    cairo_scaled_font_t* scaledFont() const { return m_scaledFont.get(); }
#endif

#if USE(FREETYPE)
    HarfBuzzFace* harfBuzzFace() const;
    bool hasCompatibleCharmap() const;
    FcFontSet* fallbacks() const;
#endif

    unsigned hash() const
    {
#if USE(CAIRO)
        return PtrHash<cairo_scaled_font_t*>::hash(m_scaledFont.get());
#elif PLATFORM(WIN)
        return m_font ? m_font->hash() : 0;
#elif PLATFORM(COCOA)
        uintptr_t flags = static_cast<uintptr_t>(m_isHashTableDeletedValue << 5 | m_textRenderingMode << 3 | m_orientation << 2 | m_syntheticBold << 1 | m_syntheticOblique);
#if USE(APPKIT)
        uintptr_t fontHash = (uintptr_t)m_font.get();
#else
        uintptr_t fontHash = reinterpret_cast<uintptr_t>(CFHash(m_font.get()));
#endif
        uintptr_t hashCodes[3] = { fontHash, m_widthVariant, flags };
        return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
#else
#error "Unsupported configuration"
#endif
    }

#if USE(FREETYPE)
    FontPlatformData& operator=(const FontPlatformData&);
#else
    FontPlatformData& operator=(const FontPlatformData&) = default;
#endif

    bool operator==(const FontPlatformData& other) const
    {
        return platformIsEqual(other)
            && m_isHashTableDeletedValue == other.m_isHashTableDeletedValue
            && m_size == other.m_size
            && m_syntheticBold == other.m_syntheticBold
            && m_syntheticOblique == other.m_syntheticOblique
            && m_isColorBitmapFont == other.m_isColorBitmapFont
            && m_orientation == other.m_orientation
            && m_widthVariant == other.m_widthVariant
            && m_textRenderingMode == other.m_textRenderingMode;
    }

    bool isHashTableDeletedValue() const
    {
        return m_isHashTableDeletedValue;
    }

    bool isEmoji() const
    {
#if PLATFORM(IOS)
        return m_isEmoji;
#else
        return false;
#endif
    }

#if PLATFORM(COCOA) || PLATFORM(WIN) || USE(FREETYPE)
    RefPtr<SharedBuffer> openTypeTable(uint32_t table) const;
#endif

#ifndef NDEBUG
    String description() const;
#endif

private:
    bool platformIsEqual(const FontPlatformData&) const;
#if PLATFORM(COCOA)
    CGFloat ctFontSize() const;
#endif
#if PLATFORM(WIN)
    void platformDataInit(HFONT, float size, HDC, WCHAR* faceName);
#endif
#if USE(FREETYPE)
    void buildScaledFont(cairo_font_face_t*);
#endif

#if PLATFORM(COCOA)
    // FIXME: Get rid of one of these. These two fonts are subtly different, and it is not obvious which one to use where.
    RetainPtr<CTFontRef> m_font;
    mutable RetainPtr<CTFontRef> m_ctFont;
#elif PLATFORM(WIN)
    RefPtr<SharedGDIObject<HFONT>> m_font;
#endif

#if USE(CG)
    RetainPtr<CGFontRef> m_cgFont;
#endif
#if USE(CAIRO)
    RefPtr<cairo_scaled_font_t> m_scaledFont;
#endif
#if USE(FREETYPE)
    RefPtr<FcPattern> m_pattern;
    mutable FcUniquePtr<FcFontSet> m_fallbacks;
    mutable RefPtr<HarfBuzzFace> m_harfBuzzFace;
#endif

    // The values below are common to all ports
    // FIXME: If they're common to all ports, they should move to Font
    float m_size { 0 };

    FontOrientation m_orientation { Horizontal };
    FontWidthVariant m_widthVariant { RegularWidth };
    TextRenderingMode m_textRenderingMode { AutoTextRendering };

    bool m_syntheticBold { false };
    bool m_syntheticOblique { false };
    bool m_isColorBitmapFont { false };
    bool m_isHashTableDeletedValue { false };
    bool m_isSystemFont { false };
    // The values above are common to all ports

#if PLATFORM(IOS)
    bool m_isEmoji { false };
#endif
#if PLATFORM(WIN)
    bool m_useGDI { false };
#endif
#if USE(FREETYPE)
    bool m_fixedWidth { false };
#endif
};

#if USE(APPKIT)
// NSFonts and CTFontRefs are toll-free-bridged.
inline CTFontRef toCTFont(NSFont *font)
{
    return (CTFontRef)font;
}

inline NSFont *toNSFont(CTFontRef font)
{
    return (NSFont *)font;
}
#endif

#if USE(CG)
class ScopedTextMatrix {
public:
    ScopedTextMatrix(CGAffineTransform newMatrix, CGContextRef context)
        : m_context(context)
        , m_textMatrix(CGContextGetTextMatrix(context))
    {
        CGContextSetTextMatrix(m_context, newMatrix);
    }

    ~ScopedTextMatrix()
    {
        CGContextSetTextMatrix(m_context, m_textMatrix);
    }

    CGAffineTransform savedMatrix() const
    {
        return m_textMatrix;
    }

private:
    CGContextRef m_context;
    CGAffineTransform m_textMatrix;
};
#endif

} // namespace WebCore

#endif // FontPlatformData_h
