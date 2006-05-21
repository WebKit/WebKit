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
#include "FontPlatformData.h"
#include "Font.h"
#include "FontFallbackList.h"
#include "StringHash.h"
#include <wtf/HashMap.h>

namespace WebCore
{

struct FontPlatformDataCacheKey
{
    FontPlatformDataCacheKey(const AtomicString& family = AtomicString(), unsigned size = 0, bool bold = false, bool italic = false)
    :m_family(family), m_size(size), m_bold(bold), m_italic(italic)
    {}

    bool operator==(const FontPlatformDataCacheKey& other) const
    {
        return equalIgnoringCase(m_family, other.m_family) && m_size == other.m_size && m_bold == other.m_bold && m_italic == other.m_italic;
    }
    
    AtomicString m_family;
    unsigned m_size;
    bool m_bold;
    bool m_italic;
};

inline unsigned computeHash(const FontPlatformDataCacheKey& fontKey)
{
    unsigned hashCodes[3] = {
        CaseInsensitiveHash::hash(fontKey.m_family.impl()),
        fontKey.m_size,
        static_cast<unsigned>(fontKey.m_bold) << 1 | static_cast<unsigned>(fontKey.m_italic)
    };
    return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), 3 * sizeof(unsigned) / sizeof(UChar));
}

struct FontPlatformDataCacheKeyHash {
    static unsigned hash(const FontPlatformDataCacheKey& font)
    {
        return computeHash(font);
    }
         
    static bool equal(const FontPlatformDataCacheKey& a, const FontPlatformDataCacheKey& b)
    {
        return a == b;
    }
};

struct FontPlatformDataCacheKeyTraits : WTF::GenericHashTraits<FontPlatformDataCacheKey> {
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;
    static const FontPlatformDataCacheKey& deletedValue()
    {
        static FontPlatformDataCacheKey key(nullAtom, 0xFFFFFFFFU, false, false);
        return key;
    }
    static const FontPlatformDataCacheKey& emptyValue()
    {
        static FontPlatformDataCacheKey key(nullAtom, 0, false, false);
        return key;
    }
};

typedef HashMap<FontPlatformDataCacheKey, FontPlatformData*, FontPlatformDataCacheKeyHash, FontPlatformDataCacheKeyTraits> FontPlatformDataCache;

static FontPlatformDataCache* gFontPlatformDataCache = 0;

FontPlatformData* FontCache::getCachedFontPlatformData(const Font& font, const AtomicString& familyName)
{
    if (!gFontPlatformDataCache) {
        gFontPlatformDataCache = new FontPlatformDataCache;
        platformInit();
    }

    const FontDescription& desc = font.fontDescription();
    FontPlatformDataCacheKey key(familyName, desc.computedPixelSize(), desc.bold(), desc.italic());
    FontPlatformData* result = 0;
    FontPlatformDataCache::iterator it = gFontPlatformDataCache->find(key);
    if (it == gFontPlatformDataCache->end()) {
        result = createFontPlatformData(font, familyName);
        gFontPlatformDataCache->set(key, result);
    } else
        result = it->second;
    return result;
}

struct FontDataCacheKeyHash {
    static unsigned hash(const FontPlatformData& platformData)
    {
        return platformData.hash();
    }
         
    static bool equal(const FontPlatformData& a, const FontPlatformData& b)
    {
        return a == b;
    }
};

struct FontDataCacheKeyTraits : WTF::GenericHashTraits<FontPlatformData> {
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;
    static const FontPlatformData& deletedValue()
    {
        static FontPlatformData key = FontPlatformData::Deleted();
        return key;
    }
    static const FontPlatformData& emptyValue()
    {
        static FontPlatformData key;
        return key;
    }
};

typedef HashMap<FontPlatformData, FontData*, FontDataCacheKeyHash, FontDataCacheKeyTraits> FontDataCache;

static FontDataCache* gFontDataCache = 0;

FontData* FontCache::getCachedFontData(const FontPlatformData* platformData)
{
    if (!platformData)
        return 0;

    if (!gFontDataCache)
        gFontDataCache = new FontDataCache;
    
    FontData* result = gFontDataCache->get(*platformData);
    if (!result) {
        result = new FontData(*platformData);
        gFontDataCache->set(*platformData, result);
    }
        
    return result;
}

const FontData* FontCache::getFontData(const Font& font, int& familyIndex)
{
    FontPlatformData* result = 0;

    int startIndex = familyIndex;
    const FontFamily* startFamily = &font.fontDescription().family();
    for (int i = 0; startFamily && i < startIndex; i++)
        startFamily = startFamily->next();
    const FontFamily* currFamily = startFamily;
    while (currFamily && !result) {
        familyIndex++;
        if (currFamily->family().length())
            result = getCachedFontPlatformData(font, currFamily->family());
        currFamily = currFamily->next();
    }

    if (!currFamily)
        familyIndex = cAllFamiliesScanned;

    if (!result)
        // We didn't find a font. Try to find a similar font using our own specific knowledge about our platform.
        // For example on OS X, we know to map any families containing the words Arabic, Pashto, or Urdu to the
        // Geeza Pro font.
        result = getSimilarFontPlatformData(font);

    if (!result && startIndex == 0)
        // We still don't have a result.  Hand back our last resort fallback font.  We only do the last resort fallback
        // when trying to find the primary font.  Otherwise our fallback will rely on the actual characters used.
        result = getLastResortFallbackFont(font);

    // Now that we have a result, we need to go from FontPlatformData -> FontData.
    return getCachedFontData(result);
}

}