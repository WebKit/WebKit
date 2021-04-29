/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontFamilySpecificationCoreText.h"

#include "FontCache.h"
#include "FontSelector.h"
#include <pal/spi/cf/CoreTextSPI.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>

#include <CoreText/CoreText.h>

namespace WebCore {

struct FontFamilySpecificationKey {
    RetainPtr<CTFontDescriptorRef> fontDescriptor;
    FontDescriptionKey fontDescriptionKey;

    FontFamilySpecificationKey() = default;

    FontFamilySpecificationKey(CTFontDescriptorRef fontDescriptor, const FontDescription& fontDescription)
        : fontDescriptor(fontDescriptor)
        , fontDescriptionKey(fontDescription)
    { }

    explicit FontFamilySpecificationKey(WTF::HashTableDeletedValueType deletedValue)
        : fontDescriptionKey(deletedValue)
    { }

    bool operator==(const FontFamilySpecificationKey& other) const
    {
        return safeCFEqual(fontDescriptor.get(), other.fontDescriptor.get()) && fontDescriptionKey == other.fontDescriptionKey;
    }

    bool operator!=(const FontFamilySpecificationKey& other) const
    {
        return !(*this == other);
    }

    bool isHashTableDeletedValue() const { return fontDescriptionKey.isHashTableDeletedValue(); }
};

struct FontFamilySpecificationKeyHash {
    static unsigned hash(const FontFamilySpecificationKey& key) { return computeHash(safeCFHash(key.fontDescriptor.get()), key.fontDescriptionKey); }
    static bool equal(const FontFamilySpecificationKey& a, const FontFamilySpecificationKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

using FontMap = HashMap<FontFamilySpecificationKey, std::unique_ptr<FontPlatformData>, FontFamilySpecificationKeyHash, SimpleClassHashTraits<FontFamilySpecificationKey>>;

static FontMap& fontMap()
{
    static NeverDestroyed<FontMap> fontMap;
    return fontMap;
}

void clearFontFamilySpecificationCoreTextCache()
{
    fontMap().clear();
}

FontFamilySpecificationCoreText::FontFamilySpecificationCoreText(CTFontDescriptorRef fontDescriptor)
    : m_fontDescriptor(fontDescriptor)
{
}

FontFamilySpecificationCoreText::~FontFamilySpecificationCoreText() = default;

FontRanges FontFamilySpecificationCoreText::fontRanges(const FontDescription& fontDescription) const
{
    auto& fontPlatformData = fontMap().ensure(FontFamilySpecificationKey(m_fontDescriptor.get(), fontDescription), [&] () {
        auto size = fontDescription.computedSize();

        auto font = adoptCF(CTFontCreateWithFontDescriptor(m_fontDescriptor.get(), size, nullptr));

        font = preparePlatformFont(font.get(), fontDescription, nullptr, { });

        auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(font.get(), fontDescription, ShouldComputePhysicalTraits::Yes).boldObliquePair();

        return makeUnique<FontPlatformData>(font.get(), size, false, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());
    }).iterator->value;

    ASSERT(fontPlatformData);

    return FontRanges(FontCache::singleton().fontForPlatformData(*fontPlatformData));
}

}
