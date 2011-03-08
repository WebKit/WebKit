/*
 * This file is part of the internal font implementation.
 * It should not be included by source files outside of it.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "FontOrientation.h"
#include "FontWidthVariant.h"
#include "TextOrientation.h"
#include <wtf/text/StringImpl.h>

#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif

typedef struct CGFont* CGFontRef;
#ifndef BUILDING_ON_TIGER
typedef const struct __CTFont* CTFontRef;
#endif

#include <CoreFoundation/CFBase.h>
#include <objc/objc-auto.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(CHROMIUM) && OS(DARWIN)
#include "CrossProcessFontLoading.h"  
#endif


typedef UInt32 ATSUFontID;
typedef UInt32 ATSFontRef;

namespace WebCore {

#ifndef BUILDING_ON_TIGER
inline CTFontRef toCTFontRef(NSFont *nsFont) { return reinterpret_cast<CTFontRef>(nsFont); }
#endif

class FontPlatformData {
  public:
    FontPlatformData(float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation = Horizontal,
                     TextOrientation textOrientation = TextOrientationVerticalRight, FontWidthVariant widthVariant = RegularWidth)
        : m_syntheticBold(syntheticBold)
        , m_syntheticOblique(syntheticOblique)
        , m_orientation(orientation)
        , m_textOrientation(textOrientation)
        , m_size(size)
        , m_widthVariant(widthVariant)
        , m_font(0)
#ifdef BUILDING_ON_TIGER
        , m_cgFont(0)
#endif
        , m_isColorBitmapFont(false)
    {
    }

    FontPlatformData(NSFont*, float size, bool syntheticBold = false, bool syntheticOblique = false, FontOrientation = Horizontal,
                     TextOrientation = TextOrientationVerticalRight, FontWidthVariant = RegularWidth);
    
    FontPlatformData(CGFontRef cgFont, float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation,
                     TextOrientation textOrientation, FontWidthVariant widthVariant)
        : m_syntheticBold(syntheticBold)
        , m_syntheticOblique(syntheticOblique)
        , m_orientation(orientation)
        , m_textOrientation(textOrientation)
        , m_size(size)
        , m_widthVariant(widthVariant)
        , m_font(0)
        , m_cgFont(cgFont)
        , m_isColorBitmapFont(false)
    {
    }

    FontPlatformData(const FontPlatformData&);
    
    ~FontPlatformData();

    FontPlatformData(WTF::HashTableDeletedValueType) : m_font(hashTableDeletedFontValue()) { }
    bool isHashTableDeletedValue() const { return m_font == hashTableDeletedFontValue(); }

    float size() const { return m_size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }
    FontOrientation orientation() const { return m_orientation; }
    TextOrientation textOrientation() const { return m_textOrientation; }
    FontWidthVariant widthVariant() const { return m_widthVariant; }

    void setOrientation(FontOrientation orientation) { m_orientation = orientation; }

    bool m_syntheticBold;
    bool m_syntheticOblique;
    FontOrientation m_orientation;
    TextOrientation m_textOrientation;

    float m_size;
    
    FontWidthVariant m_widthVariant;

    unsigned hash() const
    {
        ASSERT(m_font != 0 || m_cgFont == 0);
        uintptr_t hashCodes[3] = { (uintptr_t)m_font, m_widthVariant, m_textOrientation << 3 | m_orientation << 2 | m_syntheticBold << 1 | m_syntheticOblique };
        return WTF::StringHasher::createBlobHash<sizeof(hashCodes)>(hashCodes);
    }

    const FontPlatformData& operator=(const FontPlatformData& f);

    bool operator==(const FontPlatformData& other) const
    { 
        return m_font == other.m_font && m_syntheticBold == other.m_syntheticBold && m_syntheticOblique == other.m_syntheticOblique
               && m_cgFont == other.m_cgFont && m_size == other.m_size && m_orientation == other.m_orientation
               && m_textOrientation == other.m_textOrientation && m_widthVariant == other.m_widthVariant;
    }

    NSFont *font() const { return m_font; }
    void setFont(NSFont *font);

    CTFontRef ctFont() const;

    bool roundsGlyphAdvances() const;
    bool allowsLigatures() const;
    bool isColorBitmapFont() const { return m_isColorBitmapFont; }

#ifndef BUILDING_ON_TIGER
    CGFontRef cgFont() const { return m_cgFont.get(); }
#else
    CGFontRef cgFont() const { return m_cgFont; }
#endif

#ifndef NDEBUG
    String description() const;
#endif

private:
    static NSFont *hashTableDeletedFontValue() { return reinterpret_cast<NSFont *>(-1); }
    
    // Load various data about the font specified by |nsFont| with the size fontSize into the following output paramters:
    // Note: Callers should always take into account that for the Chromium port, |outNSFont| isn't necessarily the same
    // font as |nsFont|.  This because the sandbox may block loading of the original font.
    // * outNSFont - The font that was actually loaded, for the Chromium port this may be different than nsFont.
    // The caller is responsible for calling CFRelease() on this parameter when done with it.
    // * cgFont - CGFontRef representing the input font at the specified point size.
    void loadFont(NSFont* nsFont, float fontSize, NSFont*& outNSFont, CGFontRef& cgFont);

    NSFont *m_font;

#ifndef BUILDING_ON_TIGER
    RetainPtr<CGFontRef> m_cgFont;
#else
    CGFontRef m_cgFont; // It is not necessary to refcount this, since either an NSFont owns it or some CachedFont has it referenced.
#endif

    mutable RetainPtr<CTFontRef> m_CTFont;

    bool m_isColorBitmapFont;
    
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    RefPtr<MemoryActivatedFont> m_inMemoryFont;
#endif
};

} // namespace WebCore

#endif
