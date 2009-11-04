/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
    Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>

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
#include "PlatformString.h"
#include "StringHash.h"
#include <utility>
#include <wtf/ListHashSet.h>
#include <wtf/StdLibExtras.h>

using namespace WTF;

namespace WebCore {

void FontCache::platformInit()
{
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font&, const UChar*, int)
{
    return 0;
}

FontPlatformData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return new FontPlatformData(font.fontDescription());
}

FontPlatformData* FontCache::getLastResortFallbackFont(const FontDescription&)
{
    return new FontPlatformData();
}

void FontCache::getTraitsInFamily(const AtomicString&, Vector<unsigned>&)
{
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString&)
{
    return new FontPlatformData(fontDescription);
}

} // namespace WebCore
