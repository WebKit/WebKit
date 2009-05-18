/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther

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
#include "FontPlatformData.h"
#include "Font.h"
#include "StringHash.h"
#include <wtf/StdLibExtras.h>

#include <QHash>

namespace WebCore {

FontCache* fontCache()
{
    DEFINE_STATIC_LOCAL(FontCache, globalFontCache, ());
    return &globalFontCache;
}

FontCache::FontCache()
{
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
}

typedef QHash<FontDescription, FontPlatformData*> FontPlatformDataCache;

// using Q_GLOBAL_STATIC leads to crash. TODO investigate the way to fix this.
static FontPlatformDataCache* gFontPlatformDataCache;

uint qHash(const FontDescription& key)
{
    uint value = CaseFoldingHash::hash(key.family().family());
    value ^= key.computedPixelSize();
    value ^= static_cast<int>(key.weight());
    return value;
}

FontPlatformData* FontCache::getCachedFontPlatformData(const FontDescription& description, const AtomicString& family, bool checkingAlternateName)
{
    if (!gFontPlatformDataCache)
        gFontPlatformDataCache = new FontPlatformDataCache;

    FontPlatformData* fontData = gFontPlatformDataCache->value(description, 0);
    if (!fontData) {
        fontData =  new FontPlatformData(description);
        gFontPlatformDataCache->insert(description, fontData);
    }

    return fontData;
}

SimpleFontData* FontCache::getCachedFontData(const FontPlatformData*)
{
    return 0;
}

FontPlatformData* FontCache::getLastResortFallbackFont(const FontDescription&)
{
    return 0;
}

void FontCache::releaseFontData(const WebCore::SimpleFontData*)
{
}

void FontCache::addClient(FontSelector*)
{
}

void FontCache::removeClient(FontSelector*)
{
}

} // namespace WebCore
