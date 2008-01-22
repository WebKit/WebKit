/*
    Copyright (C) 2007 Trolltech ASA

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "config.h"
#include "SimpleFontData.h"

namespace WebCore {

SimpleFontData::SimpleFontData(const FontPlatformData& font, bool customFont, bool loading, SVGFontData*)
    : m_font(font), m_isCustomFont(customFont), m_isLoading(loading)
{
}

SimpleFontData::~SimpleFontData()
{
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    return true;
}

const SimpleFontData* SimpleFontData::fontDataForCharacter(UChar32) const
{
    return this;
}

bool SimpleFontData::isSegmented() const
{
    return false;
}

}
