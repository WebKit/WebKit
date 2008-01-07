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
#include "FontFallbackList.h"
#include "FontPlatformData.h"
#include "FontSelector.h"
#include "StringHash.h"
#include <wtf/HashMap.h>

namespace WebCore {

struct FontPlatformDataCacheKey {
    FontPlatformDataCacheKey(const AtomicString& family = AtomicString(), unsigned size = 0, bool bold = false, bool italic = false,
                             bool isPrinterFont = false, FontRenderingMode renderingMode = NormalRenderingMode)
        : m_family(family)
        , m_size(size)
        , m_bold(bold)
        , m_italic(italic)
        , m_printerFont(isPrinterFont)
        , m_renderingMode(renderingMode)
    {
    }

    bool operator==(const FontPlatformDataCacheKey& other) const
    {
        return equalIgnoringCase(m_family, other.m_family) && m_size == other.m_size && 
               m_bold == other.m_bold && m_italic == other.m_italic && m_printerFont == other.m_printerFont &&
               m_renderingMode == other.m_renderingMode;
    }
    
    AtomicString m_family;
    unsigned m_size;
    bool m_bold;
    bool m_italic;
    bool m_printerFont;
    FontRenderingMode m_renderingMode;
};

inline unsigned computeHash(const FontPlatformDataCacheKey& fontKey)
{
    unsigned hashCodes[4] = {
        CaseFoldingHash::hash(fontKey.m_family),
        fontKey.m_size,
        static_cast<unsigned>(fontKey.m_bold) << 3 | static_cast<unsigned>(fontKey.m_italic) << 2 | static_cast<unsigned>(fontKey.m_printerFont) << 1 |
        static_cast<unsigned>(fontKey.m_renderingMode)
    };
    return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), 4 * sizeof(unsigned) / sizeof(UChar));
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

    static const bool safeToCompareToEmptyOrDeleted = true;
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

static const AtomicString& alternateFamilyName(const AtomicString& familyName)
{
    // Alias Courier <-> Courier New
    static AtomicString courier("Courier"), courierNew("Courier New");
    if (equalIgnoringCase(familyName, courier))
        return courierNew;
    if (equalIgnoringCase(familyName, courierNew))
        return courier;

    // Alias Times and Times New Roman.
    static AtomicString times("Times"), timesNewRoman("Times New Roman");
    if (equalIgnoringCase(familyName, times))
        return timesNewRoman;
    if (equalIgnoringCase(familyName, timesNewRoman))
        return times;
    
    // Alias Arial and Helvetica
    static AtomicString arial("Arial"), helvetica("Helvetica");
    if (equalIgnoringCase(familyName, arial))
        return helvetica;
    if (equalIgnoringCase(familyName, helvetica))
        return arial;

    return emptyAtom;
}

FontPlatformData* FontCache::getCachedFontPlatformData(const FontDescription& fontDescription, 
                                                       const AtomicString& familyName,
                                                       bool checkingAlternateName)
{
    if (!gFontPlatformDataCache) {
        gFontPlatformDataCache = new FontPlatformDataCache;
        platformInit();
    }

    FontPlatformDataCacheKey key(familyName, fontDescription.computedPixelSize(), fontDescription.bold(), fontDescription.italic(),
                                 fontDescription.usePrinterFont(), fontDescription.renderingMode());
    FontPlatformData* result = 0;
    bool foundResult;
    FontPlatformDataCache::iterator it = gFontPlatformDataCache->find(key);
    if (it == gFontPlatformDataCache->end()) {
        result = createFontPlatformData(fontDescription, familyName);
        gFontPlatformDataCache->set(key, result);
        foundResult = result;
    } else {
        result = it->second;
        foundResult = true;
    }

    if (!foundResult && !checkingAlternateName) {
        // We were unable to find a font.  We have a small set of fonts that we alias to other names, 
        // e.g., Arial/Helvetica, Courier/Courier New, etc.  Try looking up the font under the aliased name.
        const AtomicString& alternateName = alternateFamilyName(familyName);
        if (!alternateName.isEmpty())
            result = getCachedFontPlatformData(fontDescription, alternateName, true);
        if (result)
            gFontPlatformDataCache->set(key, new FontPlatformData(*result)); // Cache the result under the old name.
    }

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

    static const bool safeToCompareToEmptyOrDeleted = true;
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

typedef HashMap<FontPlatformData, SimpleFontData*, FontDataCacheKeyHash, FontDataCacheKeyTraits> FontDataCache;

static FontDataCache* gFontDataCache = 0;

SimpleFontData* FontCache::getCachedFontData(const FontPlatformData* platformData)
{
    if (!platformData)
        return 0;

    if (!gFontDataCache)
        gFontDataCache = new FontDataCache;
    
    SimpleFontData* result = gFontDataCache->get(*platformData);
    if (!result) {
        result = new SimpleFontData(*platformData);
        gFontDataCache->set(*platformData, result);
    }
        
    return result;
}

const FontData* FontCache::getFontData(const Font& font, int& familyIndex, FontSelector* fontSelector)
{
    FontPlatformData* result = 0;

    int startIndex = familyIndex;
    const FontFamily* startFamily = &font.fontDescription().family();
    for (int i = 0; startFamily && i < startIndex; i++)
        startFamily = startFamily->next();
    const FontFamily* currFamily = startFamily;
    while (currFamily && !result) {
        familyIndex++;
        if (currFamily->family().length()) {
            if (fontSelector) {
                FontData* data = fontSelector->getFontData(font.fontDescription(), currFamily->family());
                if (data)
                    return data;
            }
            result = getCachedFontPlatformData(font.fontDescription(), currFamily->family());
        }
        currFamily = currFamily->next();
    }

    if (!currFamily)
        familyIndex = cAllFamiliesScanned;

    if (!result)
        // We didn't find a font. Try to find a similar font using our own specific knowledge about our platform.
        // For example on OS X, we know to map any families containing the words Arabic, Pashto, or Urdu to the
        // Geeza Pro font.
        result = getSimilarFontPlatformData(font);

    if (!result && startIndex == 0) {
        // If it's the primary font that we couldn't find, we try the following. In all other cases, we will
        // just use per-character system fallback.

        if (fontSelector) {
            // Try the user's preferred standard font.
            if (FontData* data = fontSelector->getFontData(font.fontDescription(), "-webkit-standard"))
                return data;
        }

        // Still no result.  Hand back our last resort fallback font.
        result = getLastResortFallbackFont(font.fontDescription());
    }

    // Now that we have a result, we need to go from FontPlatformData -> FontData.
    return getCachedFontData(result);
}

} // namespace WebCore
