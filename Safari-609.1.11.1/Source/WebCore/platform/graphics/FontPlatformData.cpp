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

#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if OS(DARWIN) && USE(CG)
#include "SharedBuffer.h"
#include <CoreGraphics/CGFont.h>
#endif

namespace WebCore {

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : m_isHashTableDeletedValue(true)
{
}

FontPlatformData::FontPlatformData()
{
}

FontPlatformData::FontPlatformData(float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode)
    : m_size(size)
    , m_orientation(orientation)
    , m_widthVariant(widthVariant)
    , m_textRenderingMode(textRenderingMode)
    , m_syntheticBold(syntheticBold)
    , m_syntheticOblique(syntheticOblique)
{
}

#if USE(CG) && PLATFORM(WIN)
FontPlatformData::FontPlatformData(CGFontRef cgFont, float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode)
    : FontPlatformData(size, syntheticBold, syntheticOblique, orientation, widthVariant, textRenderingMode)
{
    m_cgFont = cgFont;
    ASSERT(m_cgFont);
}
#endif

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

FontPlatformData FontPlatformData::cloneWithSize(const FontPlatformData& source, float size)
{
    FontPlatformData copy(source);
    copy.m_size = size;
    return copy;
}
#endif

}
