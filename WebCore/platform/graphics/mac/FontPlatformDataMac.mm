/*
 * This file is part of the internal font implementation.
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

#import "config.h"
#import "FontPlatformData.h"

#import "PlatformString.h"
#import "WebCoreSystemInterface.h"
#import <AppKit/NSFont.h>

namespace WebCore {

FontPlatformData::FontPlatformData(NSFont *nsFont, bool syntheticBold, bool syntheticOblique)
    : m_syntheticBold(syntheticBold)
    , m_syntheticOblique(syntheticOblique)
    , m_font(nsFont)
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    , m_isColorBitmapFont(CTFontGetSymbolicTraits(toCTFontRef(nsFont)) & kCTFontColorGlyphsTrait)
#else
    , m_isColorBitmapFont(false)
#endif
{
    if (nsFont)
        CFRetain(nsFont);
    m_size = nsFont ? [nsFont pointSize] : 0.0f;
#ifndef BUILDING_ON_TIGER
    m_cgFont.adoptCF(CTFontCopyGraphicsFont(toCTFontRef(nsFont), 0));
    m_atsuFontID = CTFontGetPlatformFont(toCTFontRef(nsFont), 0);
#else
    m_cgFont = wkGetCGFontFromNSFont(nsFont);
    m_atsuFontID = wkGetNSFontATSUFontId(nsFont);
#endif
}

FontPlatformData::FontPlatformData(const FontPlatformData& f)
{
    m_font = f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1) ? static_cast<const NSFont *>(CFRetain(f.m_font)) : f.m_font;
    m_syntheticBold = f.m_syntheticBold;
    m_syntheticOblique = f.m_syntheticOblique;
    m_size = f.m_size;
    m_cgFont = f.m_cgFont;
    m_atsuFontID = f.m_atsuFontID;
    m_isColorBitmapFont = f.m_isColorBitmapFont;
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
    return *this;
}

void FontPlatformData::setFont(NSFont *font)
{
    if (m_font == font)
        return;
    if (font)
        CFRetain(font);
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
    m_font = font;
    m_size = font ? [font pointSize] : 0.0f;
#ifndef BUILDING_ON_TIGER
    m_cgFont.adoptCF(CTFontCopyGraphicsFont(toCTFontRef(font), 0));
    m_atsuFontID = CTFontGetPlatformFont(toCTFontRef(font), 0);
#else
    m_cgFont = wkGetCGFontFromNSFont(font);
    m_atsuFontID = wkGetNSFontATSUFontId(font);
#endif
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    m_isColorBitmapFont = CTFontGetSymbolicTraits(toCTFontRef(font)) & kCTFontColorGlyphsTrait;
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

#ifndef NDEBUG
String FontPlatformData::description() const
{
    RetainPtr<CFStringRef> cgFontDescription(AdoptCF, CFCopyDescription(cgFont()));
    return String(cgFontDescription.get()) + " " + String::number(m_size) + (m_syntheticBold ? " synthetic bold" : "") + (m_syntheticOblique ? " syntheitic oblique" : "");
}
#endif

} // namespace WebCore
