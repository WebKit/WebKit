/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
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

#import "config.h"
#import "FontPlatformData.h"

#import "PlatformString.h"
#import "WebCoreSystemInterface.h"
#import <AppKit/NSFont.h>

namespace WebCore {

#if PLATFORM(MAC)
void FontPlatformData::loadFont(NSFont* nsFont, float, NSFont*& outNSFont, CGFontRef& cgFont, ATSUFontID& fontID)
{
    outNSFont = nsFont;
#ifndef BUILDING_ON_TIGER
    cgFont = CTFontCopyGraphicsFont(toCTFontRef(nsFont), 0);
    fontID = CTFontGetPlatformFont(toCTFontRef(nsFont), 0);
#else
    cgFont = wkGetCGFontFromNSFont(nsFont);
    fontID = wkGetNSFontATSUFontId(nsFont);
#endif
}
#endif  // PLATFORM(MAC)

FontPlatformData::FontPlatformData(NSFont *nsFont, bool syntheticBold, bool syntheticOblique)
    : m_syntheticBold(syntheticBold)
    , m_syntheticOblique(syntheticOblique)
    , m_font(nsFont)
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    // FIXME: Chromium: The following code isn't correct for the Chromium port since the sandbox might
    // have blocked font loading, in which case we'll only have the real loaded font file after the call to loadFont().
    , m_isColorBitmapFont(CTFontGetSymbolicTraits(toCTFontRef(nsFont)) & kCTFontColorGlyphsTrait)
#else
    , m_isColorBitmapFont(false)
#endif
{
    ASSERT_ARG(nsFont, nsFont);

    m_size = [nsFont pointSize];
    
    CGFontRef cgFont = 0;
    loadFont(nsFont, m_size, m_font, cgFont, m_atsuFontID);
    
    if (m_font)
        CFRetain(m_font);

#ifndef BUILDING_ON_TIGER
    m_cgFont.adoptCF(cgFont);
#else
    m_cgFont = cgFont;
#endif
}

FontPlatformData::FontPlatformData(const FontPlatformData& f)
{
    m_font = f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1) ? const_cast<NSFont *>(static_cast<const NSFont *>(CFRetain(f.m_font))) : f.m_font;

    m_syntheticBold = f.m_syntheticBold;
    m_syntheticOblique = f.m_syntheticOblique;
    m_size = f.m_size;
    m_cgFont = f.m_cgFont;
    m_atsuFontID = f.m_atsuFontID;
    m_isColorBitmapFont = f.m_isColorBitmapFont;
#if USE(CORE_TEXT)
    m_CTFont = f.m_CTFont;
#endif
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    m_inMemoryFont = f.m_inMemoryFont;
#endif
}

FontPlatformData:: ~FontPlatformData()
{
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
}

const FontPlatformData& FontPlatformData::operator=(const FontPlatformData& f)
{
    m_syntheticBold = f.m_syntheticBold;
    m_syntheticOblique = f.m_syntheticOblique;
    m_size = f.m_size;
    m_cgFont = f.m_cgFont;
    m_atsuFontID = f.m_atsuFontID;
    if (m_font == f.m_font)
        return *this;
    if (f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1))
        CFRetain(f.m_font);
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
    m_font = f.m_font;
    m_isColorBitmapFont = f.m_isColorBitmapFont;
#if USE(CORE_TEXT)
    m_CTFont = f.m_CTFont;
#endif
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    m_inMemoryFont = f.m_inMemoryFont;
#endif
    return *this;
}

void FontPlatformData::setFont(NSFont *font)
{
    ASSERT_ARG(font, font);
    ASSERT(m_font != reinterpret_cast<NSFont *>(-1));

    if (m_font == font)
        return;

    CFRetain(font);
    if (m_font)
        CFRelease(m_font);
    m_font = font;
    m_size = [font pointSize];
    
    CGFontRef cgFont = 0;
    NSFont* loadedFont = 0;
    loadFont(m_font, m_size, loadedFont, cgFont, m_atsuFontID);
    
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    // If loadFont replaced m_font with a fallback font, then release the
    // previous font to counter the retain above. Then retain the new font.
    if (loadedFont != m_font) {
        CFRelease(m_font);
        CFRetain(loadedFont);
        m_font = loadedFont;
    }
#endif
    
#ifndef BUILDING_ON_TIGER
    m_cgFont.adoptCF(cgFont);
#else
    m_cgFont = cgFont;
#endif
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    m_isColorBitmapFont = CTFontGetSymbolicTraits(toCTFontRef(m_font)) & kCTFontColorGlyphsTrait;
#endif
#if USE(CORE_TEXT)
    m_CTFont = 0;
#endif
}

bool FontPlatformData::roundsGlyphAdvances() const
{
    return [m_font renderingMode] == NSFontAntialiasedIntegerAdvancementsRenderingMode;
}

bool FontPlatformData::allowsLigatures() const
{
    return ![[m_font coveredCharacterSet] characterIsMember:'a'];
}

#if USE(CORE_TEXT)
CTFontRef FontPlatformData::ctFont() const
{
    if (m_font)
        return toCTFontRef(m_font);
    if (!m_CTFont)
        m_CTFont.adoptCF(CTFontCreateWithGraphicsFont(m_cgFont.get(), m_size, 0, 0));
    return m_CTFont.get();
}
#endif // USE(CORE_TEXT)

#ifndef NDEBUG
String FontPlatformData::description() const
{
    RetainPtr<CFStringRef> cgFontDescription(AdoptCF, CFCopyDescription(cgFont()));
    return String(cgFontDescription.get()) + " " + String::number(m_size) + (m_syntheticBold ? " synthetic bold" : "") + (m_syntheticOblique ? " synthetic oblique" : "");
}
#endif

} // namespace WebCore
