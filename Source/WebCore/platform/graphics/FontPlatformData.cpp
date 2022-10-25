/*
 * Copyright (C) 2011 Brent Fulgham
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

#include "config.h"
#include "FontPlatformData.h"

#include "FontCache.h"
#include "FontDescription.h"
#include "RenderStyleConstants.h"
#include "StyleFontSizeFunctions.h"

#include <wtf/SortedArrayMap.h>

namespace WebCore {

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : m_isHashTableDeletedValue(true)
{
}

FontPlatformData::FontPlatformData()
{
}

FontPlatformData::FontPlatformData(float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode, const CreationData* creationData)
    : m_size(size)
    , m_orientation(orientation)
    , m_widthVariant(widthVariant)
    , m_textRenderingMode(textRenderingMode)
    , m_creationData(makeOptionalFromPointer(creationData))
    , m_syntheticBold(syntheticBold)
    , m_syntheticOblique(syntheticOblique)
{
}

#if !USE(FREETYPE)
FontPlatformData FontPlatformData::cloneWithOrientation(const FontPlatformData& source, FontOrientation orientation)
{
    FontPlatformData copy(source);
    copy.m_orientation = orientation;
    return copy;
}

FontPlatformData FontPlatformData::cloneWithSyntheticOblique(const FontPlatformData& source, bool syntheticOblique)
{
    FontPlatformData copy(source);
    copy.m_syntheticOblique = syntheticOblique;
    return copy;
}
#endif

#if !USE(FREETYPE) && (!USE(CORE_TEXT) || !PLATFORM(COCOA))
// FIXME: Don't other platforms also need to reinstantiate their copy.m_font for scaled size?
FontPlatformData FontPlatformData::cloneWithSize(const FontPlatformData& source, float size)
{
    FontPlatformData copy(source);
    copy.updateSize(size);
    return copy;
}

void FontPlatformData::updateSize(float size)
{
    m_size = size;
}
#endif

void FontPlatformData::updateSizeWithFontSizeAdjust(const std::optional<float>& fontSizeAdjust)
{
    if (!fontSizeAdjust.has_value())
        return;

    auto tmpFont = FontCache::forCurrentThread().fontForPlatformData(*this);
    auto adjustedFontSize = Style::adjustedFontSize(size(), fontSizeAdjust.value(), tmpFont->fontMetrics());

    if (adjustedFontSize == size())
        return;

    updateSize(std::min(adjustedFontSize, maximumAllowedFontSize));
}

#if !PLATFORM(COCOA) && !USE(FREETYPE)
Vector<FontPlatformData::FontVariationAxis> FontPlatformData::variationAxes(ShouldLocalizeAxisNames) const
{
    // FIXME: <webkit.org/b/219614> Not implemented yet.
    return { };
}
#endif

}
