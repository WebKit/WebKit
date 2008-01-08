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
#include "FontCache.h"
#include "FontDescription.h"
#include "Font.h"

namespace WebCore {

bool FontCache::fontExists(const FontDescription &desc, const AtomicString& family)
{
    // try to construct a QFont inside WebCore::Font to see if we know about this font
    FontDescription fnt(desc);
    FontFamily fam;
    fam.setFamily(family);
    fnt.setFamily(fam);
    return Font(fnt, /*letterSpacing*/0, /*wordSpacing*/0).font().exactMatch();
}

FontPlatformData* FontCache::getCachedFontPlatformData(const FontDescription&, const AtomicString& family, bool checkingAlternateName)
{
    return 0;
}

SimpleFontData* FontCache::getCachedFontData(const FontPlatformData*)
{
    return 0;
}

FontPlatformData* FontCache::getLastResortFallbackFont(const FontDescription&)
{
    return 0;
}

}
