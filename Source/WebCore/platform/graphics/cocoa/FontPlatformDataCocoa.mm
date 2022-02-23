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

#import "FontCacheCoreText.h"
#import "SharedBuffer.h"
#import <pal/spi/cf/CoreTextSPI.h>
#import <wtf/Hasher.h>
#import <wtf/text/StringConcatenateNumbers.h>

#if PLATFORM(IOS_FAMILY)
#import <CoreText/CoreText.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#endif

namespace WebCore {

unsigned FontPlatformData::hash() const
{
    // FIXME: Hashing a CFHash is unfortunate here.
    return computeHash(CFHash(m_font.get()), m_widthVariant, m_isHashTableDeletedValue, m_textRenderingMode, m_orientation, m_orientation, m_syntheticBold, m_syntheticOblique);
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    if (!m_font || !other.m_font)
        return m_font == other.m_font;
    return CFEqual(m_font.get(), other.m_font.get());
}

RefPtr<SharedBuffer> FontPlatformData::platformOpenTypeTable(uint32_t) const
{
    return nullptr;
}

Vector<FontPlatformData::FontVariationAxis> FontPlatformData::variationAxes(ShouldLocalizeAxisNames shouldLocalizeAxisNames) const
{
    auto platformFont = ctFont();
    if (!platformFont)
        return { };
    
    return WTF::map(defaultVariationValues(platformFont, shouldLocalizeAxisNames), [](auto&& entry) {
        auto& [tag, values] = entry;
        return FontPlatformData::FontVariationAxis { values.axisName, String(tag.data(), tag.size()), values.defaultValue, values.minimumValue, values.maximumValue };
    });
}


} // namespace WebCore
