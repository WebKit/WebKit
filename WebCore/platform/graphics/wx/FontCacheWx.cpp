/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontCache.h"
#include "Font.h"
#include "FontPlatformData.h"
#include "NotImplemented.h"
#include "SimpleFontData.h"

namespace WebCore {

void FontCache::platformInit()
{
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    SimpleFontData* fontData = 0;
    fontData = getCachedFontData(font.fontDescription(), font.family().family());
    if (!fontData->containsCharacters(characters, length))
        fontData = getSimilarFontPlatformData(font);
    if (!fontData->containsCharacters(characters, length))
        fontData = getLastResortFallbackFont(font.fontDescription());
    
    ASSERT(fontData->containsCharacters(characters, length));
    return fontData;
}

SimpleFontData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return getCachedFontData(font.fontDescription(), font.family().family());
}

SimpleFontData* FontCache::getLastResortFallbackFont(const FontDescription& fontDescription)
{
    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.
    SimpleFontData* fallback = 0;
#if OS(WINDOWS) || (OS(DARWIN) && !defined(BUILDING_ON_TIGER))
    static AtomicString fallbackName("Arial Unicode MS");
#else
    static AtomicString fallbackName("Times New Roman");
#endif
    fallback = getCachedFontData(fontDescription, fallbackName);
    
    return fallback;
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    return new FontPlatformData(fontDescription,family);
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
    notImplemented();
}

}
