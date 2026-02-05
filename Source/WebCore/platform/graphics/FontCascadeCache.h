/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

#include <WebCore/FontCascadeFonts.h>
#include <WebCore/FontDescription.h>
#include <WebCore/FontTaggedSettings.h>
#include <array>
#include <wtf/HashMap.h>
#include <wtf/PointerComparison.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

struct FontDescriptionKeyRareData : public RefCounted<FontDescriptionKeyRareData> {
    WTF_MAKE_TZONE_ALLOCATED(FontDescriptionKeyRareData);
public:
    static Ref<FontDescriptionKeyRareData> create(FontFeatureSettings&& featureSettings, FontVariationSettings&& variationSettings, FontVariantAlternates&& variantAlternates, FontPalette&& fontPalette, FontSizeAdjust&& fontSizeAdjust)
    {
        return adoptRef(*new FontDescriptionKeyRareData(WTF::move(featureSettings), WTF::move(variationSettings), WTF::move(variantAlternates), WTF::move(fontPalette), WTF::move(fontSizeAdjust)));
    }

    const FontFeatureSettings& featureSettings() const
    {
        return m_featureSettings;
    }

    const FontVariationSettings& variationSettings() const
    {
        return m_variationSettings;
    }

    const FontPalette& fontPalette() const
    {
        return m_fontPalette;
    }

    const FontVariantAlternates& variantAlternates() const
    {
        return m_variantAlternates;
    }

    const FontSizeAdjust& fontSizeAdjust() const
    {
        return m_fontSizeAdjust;
    }

    bool operator==(const FontDescriptionKeyRareData& other) const
    {
        return m_featureSettings == other.m_featureSettings
            && m_variationSettings == other.m_variationSettings
            && m_variantAlternates == other.m_variantAlternates
            && m_fontPalette == other.m_fontPalette
            && m_fontSizeAdjust == other.m_fontSizeAdjust;
    }

private:
    FontDescriptionKeyRareData(FontFeatureSettings&& featureSettings, FontVariationSettings&& variationSettings, FontVariantAlternates&& variantAlternates, FontPalette&& fontPalette, FontSizeAdjust&& fontSizeAdjust)
        : m_featureSettings(WTF::move(featureSettings))
        , m_variationSettings(WTF::move(variationSettings))
        , m_variantAlternates(WTF::move(variantAlternates))
        , m_fontPalette(WTF::move(fontPalette))
        , m_fontSizeAdjust(WTF::move(fontSizeAdjust))
    {
    }

    FontFeatureSettings m_featureSettings;
    FontVariationSettings m_variationSettings;
    FontVariantAlternates m_variantAlternates;
    FontPalette m_fontPalette;
    FontSizeAdjust m_fontSizeAdjust;
};

inline void add(Hasher& hasher, const FontDescriptionKeyRareData& key)
{
    add(hasher, key.featureSettings(), key.variationSettings(), key.variantAlternates(), key.fontPalette(), key.fontSizeAdjust());
}


// This key contains the FontDescription fields other than family that matter when fetching FontDatas (platform fonts).
struct FontDescriptionKey {
    FontDescriptionKey() = default;

    FontDescriptionKey(const FontDescription& description)
        : m_size(description.computedSize())
        , m_fontSelectionRequest(description.fontSelectionRequest())
        , m_flags(makeFlagsKey(description))
        , m_locale(description.computedLocale())
    {
        auto featureSettings = description.featureSettings();
        auto variationSettings = description.variationSettings();
        auto variantAlternates = description.variantAlternates();
        auto fontPalette = description.fontPalette();
        auto fontSizeAdjust = description.fontSizeAdjust();
        if (!featureSettings.isEmpty() || !variationSettings.isEmpty() || !variantAlternates.isNormal() || fontPalette.type != FontPalette::Type::Normal || !fontSizeAdjust.isNone())
            lazyInitialize(m_rareData, FontDescriptionKeyRareData::create(WTF::move(featureSettings), WTF::move(variationSettings), WTF::move(variantAlternates), WTF::move(fontPalette), WTF::move(fontSizeAdjust)));
    }

    explicit FontDescriptionKey(WTF::HashTableDeletedValueType)
        : m_isDeletedValue(true)
    { }

    bool operator==(const FontDescriptionKey& other) const
    {
        return m_isDeletedValue == other.m_isDeletedValue
            && m_size == other.m_size
            && m_fontSelectionRequest == other.m_fontSelectionRequest
            && m_flags == other.m_flags
            && m_locale == other.m_locale
            && arePointingToEqualData(m_rareData, other.m_rareData);
    }

    bool isHashTableDeletedValue() const { return m_isDeletedValue; }
    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

    friend void add(Hasher&, const FontDescriptionKey&);

private:
    static std::array<unsigned, 2> makeFlagsKey(const FontDescription& description)
    {
        unsigned first = static_cast<unsigned>(description.script()) << 15
            | static_cast<unsigned>(description.shouldDisableLigaturesForSpacing()) << 14
            | static_cast<unsigned>(description.shouldAllowUserInstalledFonts()) << 13
            | static_cast<unsigned>(description.fontStyleAxis() == FontStyleAxis::slnt) << 12
            | static_cast<unsigned>(description.opticalSizing()) << 11
            | static_cast<unsigned>(description.textRenderingMode()) << 9
            | static_cast<unsigned>(description.fontSynthesisSmallCaps()) << 8
            | static_cast<unsigned>(description.fontSynthesisStyle()) << 7
            | static_cast<unsigned>(description.fontSynthesisWeight()) << 6
            | static_cast<unsigned>(description.widthVariant()) << 4
            | static_cast<unsigned>(description.nonCJKGlyphOrientation()) << 3
            | static_cast<unsigned>(description.orientation()) << 2;
        unsigned second = static_cast<unsigned>(description.variantEmoji()) << 27
            | static_cast<unsigned>(description.variantEastAsianRuby()) << 26
            | static_cast<unsigned>(description.variantEastAsianWidth()) << 24
            | static_cast<unsigned>(description.variantEastAsianVariant()) << 21
            // variantAlternates is in the Rare object, it can't be a bitfield.
            | static_cast<unsigned>(description.variantNumericSlashedZero()) << 20
            | static_cast<unsigned>(description.variantNumericOrdinal()) << 19
            | static_cast<unsigned>(description.variantNumericFraction()) << 17
            | static_cast<unsigned>(description.variantNumericSpacing()) << 15
            | static_cast<unsigned>(description.variantNumericFigure()) << 13
            | static_cast<unsigned>(description.variantCaps()) << 10
            | static_cast<unsigned>(description.variantPosition()) << 8
            | static_cast<unsigned>(description.variantContextualAlternates()) << 6
            | static_cast<unsigned>(description.variantHistoricalLigatures()) << 4
            | static_cast<unsigned>(description.variantDiscretionaryLigatures()) << 2
            | static_cast<unsigned>(description.variantCommonLigatures());
        return { { first, second } };
    }

    bool m_isDeletedValue { false };
    float m_size { 0 };
    FontSelectionRequest m_fontSelectionRequest;
    std::array<unsigned, 2> m_flags { { 0, 0 } };
    AtomString m_locale;
    RefPtr<FontDescriptionKeyRareData> m_rareData;
};

inline void add(Hasher& hasher, const FontDescriptionKey& key)
{
    add(hasher, key.m_size, key.m_fontSelectionRequest, key.m_flags, key.m_locale);
    if (RefPtr rareData = key.m_rareData)
        add(hasher, *rareData);
}

} // namespace WebCore

namespace WTF {
template<> struct HashTraits<WebCore::FontDescriptionKey> : SimpleClassHashTraits<WebCore::FontDescriptionKey> {
};

} // namespace WTF

namespace WebCore {

// This class holds the name of a font family, and defines hashing and == of this name to
// use the rules for font family names instead of using straight string comparison.
class FontFamilyName {
public:
    FontFamilyName();
    FontFamilyName(const AtomString&);
    const AtomString& string() const;
    friend void add(Hasher&, const FontFamilyName&);

private:
    AtomString m_name;
};

bool operator==(const FontFamilyName&, const FontFamilyName&);

struct FontCascadeCacheKey {
    FontDescriptionKey fontDescriptionKey; // Shared with the lower level FontCache (caching Font objects)
    Vector<FontFamilyName, 3> families;
    unsigned fontSelectorId;
    unsigned fontSelectorVersion;
    bool hasComplexFontSelector { true };


    friend bool operator==(const FontCascadeCacheKey&, const FontCascadeCacheKey&) = default;
};

inline void add(Hasher& hasher, const FontCascadeCacheKey& key)
{
    add(hasher, key.fontDescriptionKey, key.families, key.fontSelectorId, key.fontSelectorVersion);
}

struct FontCascadeCacheEntry {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(FontCascadeCacheEntry);

    FontCascadeCacheKey key;
    Ref<FontCascadeFonts> fonts;
};

struct FontCascadeCacheKeyHashTraits : HashTraits<FontCascadeCacheKey> {
    static FontCascadeCacheKey emptyValue() { return { }; }
    static void constructDeletedValue(FontCascadeCacheKey& slot) { new (NotNull, &slot.fontDescriptionKey) FontDescriptionKey(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const FontCascadeCacheKey& key) { return key.fontDescriptionKey.isHashTableDeletedValue(); }
};

class FontCascadeCache {
    WTF_MAKE_TZONE_ALLOCATED(FontCascadeCache);
    WTF_MAKE_NONCOPYABLE(FontCascadeCache);
public:
    FontCascadeCache() = default;

    static FontCascadeCache& forCurrentThread();

    void invalidate();
    void clearMeasurementCaches();
    void pruneUnreferencedEntries();
    void pruneSystemFallbackFonts();
    Ref<FontCascadeFonts> retrieveOrAddCachedFonts(const FontCascadeDescription&, FontSelector*);

private:
    HashMap<FontCascadeCacheKey, std::unique_ptr<FontCascadeCacheEntry>, DefaultHash<FontCascadeCacheKey>, FontCascadeCacheKeyHashTraits> m_entries;
};

} // namespace WebCore
