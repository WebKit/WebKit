/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "SharedBuffer.h"
#include "TextFlags.h"
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(WIN)
#include "COMPtr.h"
#include "SharedGDIObject.h"
#endif

#if USE(CAIRO)
#include "RefPtrCairo.h"
#endif

#if USE(FREETYPE)
#include "FcUniquePtr.h"
#include "HbUniquePtr.h"
#include "RefPtrFontconfig.h"
#include <memory>
#endif

#if PLATFORM(HAIKU)
#include <memory>

#include <interface/Font.h>

#include <wtf/text/AtomString.h>
#include <wtf/text/CString.h>
#endif

#if USE(APPKIT)
OBJC_CLASS NSFont;
#endif

#if USE(CORE_TEXT)
typedef const struct __CTFont* CTFontRef;
#endif

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if PLATFORM(WIN)
#include <wtf/win/GDIObject.h>
typedef struct HFONT__* HFONT;
interface IDWriteFont;
interface IDWriteFontFace;
#endif

#if USE(DIRECT2D)
#include <dwrite_3.h>
#endif

namespace WebCore {

class FontDescription;
class SharedBuffer;

// This class is conceptually immutable. Once created, no instances should ever change (in an observable way).
class FontPlatformData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct CreationData;
    
    struct FontVariationAxis {
        FontVariationAxis(const String& name, const String& tag, float defaultValue, float minimumValue, float maximumValue)
            : m_name(name)
            , m_tag(tag)
            , m_defaultValue(defaultValue)
            , m_minimumValue(minimumValue)
            , m_maximumValue(maximumValue)
        {
        }
        
        const String& name() const { return m_name; }
        const String& tag() const { return m_tag; }
        float defaultValue() const { return m_defaultValue; }
        float minimumValue() const { return m_minimumValue; }
        float maximumValue() const { return m_maximumValue; }

    private:
        const String m_name;
        const String m_tag;
        float m_defaultValue;
        float m_minimumValue;
        float m_maximumValue;
    };

    FontPlatformData(WTF::HashTableDeletedValueType);
    FontPlatformData();
#if PLATFORM(HAIKU)
    FontPlatformData(const FontPlatformData&);
#else
    FontPlatformData(FontPlatformData&&) = default;
#endif

    FontPlatformData(float size, bool syntheticBold, bool syntheticOblique, FontOrientation = FontOrientation::Horizontal, FontWidthVariant = FontWidthVariant::RegularWidth, TextRenderingMode = TextRenderingMode::AutoTextRendering, CreationData* = nullptr);

#if USE(CORE_TEXT)
    WEBCORE_EXPORT FontPlatformData(CTFontRef, float size, bool syntheticBold = false, bool syntheticOblique = false, FontOrientation = FontOrientation::Horizontal, FontWidthVariant = FontWidthVariant::RegularWidth, TextRenderingMode = TextRenderingMode::AutoTextRendering, CreationData* = nullptr);
#endif

#if PLATFORM(HAIKU)
    FontPlatformData(const FontDescription& fontDescription, const AtomString& familyName);
    static void SetFallBackSerifFont(const BString& font);
    static void SetFallBackSansSerifFont(const BString& font);
    static void SetFallBackFixedFont(const BString& font);
    static void SetFallBackStandardFont(const BString& font);
#endif

#if PLATFORM(WIN)
    FontPlatformData(GDIObject<HFONT>, float size, bool syntheticBold, bool syntheticOblique, bool useGDI);
#if USE(CORE_TEXT)
    FontPlatformData(GDIObject<HFONT>, CTFontRef, CGFontRef, float size, bool syntheticBold, bool syntheticOblique, bool useGDI);
#endif
#if USE(DIRECT2D)
    FontPlatformData(GDIObject<HFONT>&&, COMPtr<IDWriteFont>&&, float size, bool syntheticBold, bool syntheticOblique, bool useGDI);
#endif
#if USE(CAIRO)
    FontPlatformData(GDIObject<HFONT>, cairo_font_face_t*, float size, bool bold, bool italic);
#endif
#endif

#if USE(FREETYPE)
    FontPlatformData(cairo_font_face_t*, RefPtr<FcPattern>&&, float size, bool fixedWidth, bool syntheticBold, bool syntheticOblique, FontOrientation);
#endif

#if PLATFORM(HAIKU)
    const BFont* font() const { return m_font.get(); }
#endif

    static FontPlatformData cloneWithOrientation(const FontPlatformData&, FontOrientation);
    static FontPlatformData cloneWithSyntheticOblique(const FontPlatformData&, bool);
    static FontPlatformData cloneWithSize(const FontPlatformData&, float);

#if PLATFORM(WIN)
    HFONT hfont() const { return m_font ? m_font->get() : 0; }
    bool useGDI() const { return m_useGDI; }
#if USE(CG)
    CGFontRef cgFont() const { return m_cgFont.get(); }
#endif
#endif

#if USE(CORE_TEXT)
    WEBCORE_EXPORT CTFontRef registeredFont() const; // Returns nullptr iff the font is not registered, such as web fonts (otherwise returns font()).
    static RetainPtr<CFTypeRef> objectForEqualityCheck(CTFontRef);
    RetainPtr<CFTypeRef> objectForEqualityCheck() const;
    bool hasCustomTracking() const { return isSystemFont(); }

#if PLATFORM(WIN)
    CTFontRef ctFont() const { return m_ctFont.get(); }
#else
    CTFontRef font() const { return m_font.get(); }
    CTFontRef ctFont() const;
#endif
#endif

#if PLATFORM(WIN) || PLATFORM(COCOA)
    bool isSystemFont() const { return m_isSystemFont; }
#endif

    bool hasVariations() const { return m_hasVariations; }

#if USE(DIRECT2D)
    IDWriteFont* dwFont() const { return m_dwFont.get(); }
    IDWriteFontFace* dwFontFace() const { return m_dwFontFace.get(); }
#endif

    bool isFixedPitch() const;
    float size() const { return m_size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }
    bool isColorBitmapFont() const { return m_isColorBitmapFont; }
    FontOrientation orientation() const { return m_orientation; }
    FontWidthVariant widthVariant() const { return m_widthVariant; }
    TextRenderingMode textRenderingMode() const { return m_textRenderingMode; }
    bool isForTextCombine() const { return widthVariant() != FontWidthVariant::RegularWidth; } // Keep in sync with callers of FontDescription::setWidthVariant().

    String familyName() const;
    Vector<FontVariationAxis> variationAxes() const;

#if USE(CAIRO)
    cairo_scaled_font_t* scaledFont() const { return m_scaledFont.get(); }
#endif

#if USE(FREETYPE)
#if USE(HARFBUZZ) && !ENABLE(OPENTYPE_MATH)
    HbUniquePtr<hb_font_t> createOpenTypeMathHarfBuzzFont() const;
#endif
    bool hasCompatibleCharmap() const;
    FcPattern* fcPattern() const;
    bool isFixedWidth() const { return m_fixedWidth; }
#endif

    unsigned hash() const;

#if USE(HAIKU)
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
#if PLATFORM(IOS_FAMILY)
        return m_isEmoji;
#else
        return false;
#endif
    }

    RefPtr<SharedBuffer> openTypeTable(uint32_t table) const;
    RefPtr<SharedBuffer> platformOpenTypeTable(uint32_t table) const;

    String description() const;

    struct CreationData {
        Ref<SharedBuffer> fontFaceData;
        String itemInCollection;
    };

    const Optional<CreationData>& creationData() const
    {
        return m_creationData;
    }

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
#if PLATFORM(HAIKU)
    static void findMatchingFontFamily(const AtomString& familyName,
		font_family& fontFamily);
#endif

#if PLATFORM(WIN)
    RefPtr<SharedGDIObject<HFONT>> m_font; // FIXME: Delete this in favor of m_ctFont or m_dwFont or m_scaledFont.
#if USE(CORE_TEXT)
    RetainPtr<CGFontRef> m_cgFont; // FIXME: Delete this in favor of m_ctFont.
    RetainPtr<CTFontRef> m_ctFont;
#endif
#elif USE(CORE_TEXT)
    // FIXME: Get rid of one of these. These two fonts are subtly different, and it is not obvious which one to use where.
    RetainPtr<CTFontRef> m_font;
    mutable RetainPtr<CTFontRef> m_ctFont;
#elif PLATFORM(HAIKU)
    std::unique_ptr<BFont> m_font;
#endif

#if USE(CG) && PLATFORM(WIN)
    RetainPtr<CGFontRef> m_cgFont;
#endif

#if USE(DIRECT2D)
    COMPtr<IDWriteFont> m_dwFont;
    COMPtr<IDWriteFontFace> m_dwFontFace;
#endif

#if USE(CAIRO)
    RefPtr<cairo_scaled_font_t> m_scaledFont;
#endif

#if USE(FREETYPE)
#if USE(CAIRO)
    RefPtr<FcPattern> m_pattern;
#endif
#endif

    float m_size { 0 };

    FontOrientation m_orientation { FontOrientation::Horizontal };
    FontWidthVariant m_widthVariant { FontWidthVariant::RegularWidth };
    TextRenderingMode m_textRenderingMode { TextRenderingMode::AutoTextRendering };

    Optional<CreationData> m_creationData;

    bool m_syntheticBold { false };
    bool m_syntheticOblique { false };
    bool m_isColorBitmapFont { false };
    bool m_isHashTableDeletedValue { false };
    bool m_isSystemFont { false };
    bool m_hasVariations { false };
    // The values above are common to all ports

#if PLATFORM(HAIKU)
    static font_family m_FallbackSerifFontFamily;
    static font_family m_FallbackSansSerifFontFamily;
    static font_family m_FallbackFixedFontFamily;
    static font_family m_FallbackStandardFontFamily;
#endif

#if PLATFORM(IOS_FAMILY)
    bool m_isEmoji { false };
#endif

#if PLATFORM(WIN)
    bool m_useGDI { false };
#endif

#if USE(FREETYPE)
    bool m_fixedWidth { false };
#endif

    // Adding any non-derived information to FontPlatformData needs a parallel change in WebCoreArgumentCodersCocoa.cpp.
};

#if USE(CORE_TEXT)
bool isSystemFont(CTFontRef);
#endif

#if USE(APPKIT) && defined(__OBJC__)

// NSFonts and CTFontRefs are toll-free-bridged.
inline CTFontRef toCTFont(NSFont *font)
{
    return (__bridge CTFontRef)font;
}

inline NSFont *toNSFont(CTFontRef font)
{
    return (__bridge NSFont *)font;
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
