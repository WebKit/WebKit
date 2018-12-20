/*
 * Copyright (C) 2006, 2008 Apple Inc.  All rights reserved.
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

#include "FontDescription.h"
#include "FontPlatformData.h"
#include "FontTaggedSettings.h"
#include "Timer.h"
#include <array>
#include <limits.h>
#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "FontCacheCoreText.h"
#endif

#if OS(WINDOWS)
#include <windows.h>
#include <objidl.h>
#include <mlang.h>
#endif

namespace WebCore {

class FontCascade;
class FontPlatformData;
class FontSelector;
class OpenTypeVerticalData;
class Font;

#if PLATFORM(WIN)
#if USE(IMLANG_FONT_LINK2)
typedef IMLangFontLink2 IMLangFontLinkType;
#else
typedef IMLangFontLink IMLangFontLinkType;
#endif
#endif

// This key contains the FontDescription fields other than family that matter when fetching FontDatas (platform fonts).
struct FontDescriptionKey {
    FontDescriptionKey() = default;

    FontDescriptionKey(const FontDescription& description)
        : m_size(description.computedPixelSize())
        , m_fontSelectionRequest(description.fontSelectionRequest())
        , m_flags(makeFlagsKey(description))
        , m_locale(description.locale())
        , m_featureSettings(description.featureSettings())
#if ENABLE(VARIATION_FONTS)
        , m_variationSettings(description.variationSettings())
#endif
    { }

    explicit FontDescriptionKey(WTF::HashTableDeletedValueType)
        : m_size(cHashTableDeletedSize)
    { }

    bool operator==(const FontDescriptionKey& other) const
    {
        return m_size == other.m_size
            && m_fontSelectionRequest == other.m_fontSelectionRequest
            && m_flags == other.m_flags
            && m_locale == other.m_locale
#if ENABLE(VARIATION_FONTS)
            && m_variationSettings == other.m_variationSettings
#endif
            && m_featureSettings == other.m_featureSettings;
    }

    bool operator!=(const FontDescriptionKey& other) const
    {
        return !(*this == other);
    }

    bool isHashTableDeletedValue() const { return m_size == cHashTableDeletedSize; }

    inline unsigned computeHash() const
    {
        IntegerHasher hasher;
        hasher.add(m_size);
        hasher.add(m_fontSelectionRequest.weight);
        hasher.add(m_fontSelectionRequest.width);
        hasher.add(m_fontSelectionRequest.slope.value_or(normalItalicValue()));
        hasher.add(m_locale.existingHash());
        for (unsigned flagItem : m_flags)
            hasher.add(flagItem);
        hasher.add(m_featureSettings.hash());
#if ENABLE(VARIATION_FONTS)
        hasher.add(m_variationSettings.hash());
#endif
        return hasher.hash();
    }

private:
    static std::array<unsigned, 2> makeFlagsKey(const FontDescription& description)
    {
        unsigned first = static_cast<unsigned>(description.script()) << 14
            | static_cast<unsigned>(description.shouldAllowUserInstalledFonts()) << 13
            | static_cast<unsigned>(description.fontStyleAxis() == FontStyleAxis::slnt) << 12
            | static_cast<unsigned>(description.opticalSizing()) << 11
            | static_cast<unsigned>(description.textRenderingMode()) << 9
            | static_cast<unsigned>(description.fontSynthesis()) << 6
            | static_cast<unsigned>(description.widthVariant()) << 4
            | static_cast<unsigned>(description.nonCJKGlyphOrientation()) << 3
            | static_cast<unsigned>(description.orientation()) << 2
            | static_cast<unsigned>(description.renderingMode());
        unsigned second = static_cast<unsigned>(description.variantEastAsianRuby()) << 27
            | static_cast<unsigned>(description.variantEastAsianWidth()) << 25
            | static_cast<unsigned>(description.variantEastAsianVariant()) << 22
            | static_cast<unsigned>(description.variantAlternates()) << 21
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
        return {{ first, second }};
    }

    static const unsigned cHashTableDeletedSize = 0xFFFFFFFFU;

    // FontCascade::locale() is explicitly not included in this struct.
    unsigned m_size { 0 };
    FontSelectionRequest m_fontSelectionRequest;
    std::array<unsigned, 2> m_flags {{ 0, 0 }};
    AtomicString m_locale;
    FontFeatureSettings m_featureSettings;
#if ENABLE(VARIATION_FONTS)
    FontVariationSettings m_variationSettings;
#endif
};

struct FontDescriptionKeyHash {
    static unsigned hash(const FontDescriptionKey& key)
    {
        return key.computeHash();
    }

    static bool equal(const FontDescriptionKey& a, const FontDescriptionKey& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

class FontCache {
    friend class WTF::NeverDestroyed<FontCache>;

    WTF_MAKE_NONCOPYABLE(FontCache); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static FontCache& singleton();

    // These methods are implemented by the platform.
    RefPtr<Font> systemFallbackForCharacters(const FontDescription&, const Font* originalFontData, bool isPlatformFont, const UChar* characters, unsigned length);
    Vector<String> systemFontFamilies();
    void platformInit();

#if PLATFORM(COCOA)
    WEBCORE_EXPORT static void setFontWhitelist(const Vector<String>&);
#endif
#if PLATFORM(WIN)
    IMLangFontLinkType* getFontLinkInterface();
    static void comInitialize();
    static void comUninitialize();
    static IMultiLanguage* getMultiLanguageInterface();
#endif

    // This function exists so CSSFontSelector can have a unified notion of preinstalled fonts and @font-face.
    // It comes into play when you create an @font-face which shares a family name as a preinstalled font.
    Vector<FontSelectionCapabilities> getFontSelectionCapabilitiesInFamily(const AtomicString&, AllowUserInstalledFonts);

    WEBCORE_EXPORT RefPtr<Font> fontForFamily(const FontDescription&, const AtomicString&, const FontFeatureSettings* fontFaceFeatures = nullptr, const FontVariantSettings* fontFaceVariantSettings = nullptr, FontSelectionSpecifiedCapabilities fontFaceCapabilities = { }, bool checkingAlternateName = false);
    WEBCORE_EXPORT Ref<Font> lastResortFallbackFont(const FontDescription&);
    WEBCORE_EXPORT Ref<Font> fontForPlatformData(const FontPlatformData&);
    RefPtr<Font> similarFont(const FontDescription&, const AtomicString& family);

    void addClient(FontSelector&);
    void removeClient(FontSelector&);

    unsigned short generation();
    WEBCORE_EXPORT void invalidate();

    WEBCORE_EXPORT size_t fontCount();
    WEBCORE_EXPORT size_t inactiveFontCount();
    WEBCORE_EXPORT void purgeInactiveFontData(unsigned count = UINT_MAX);
    void platformPurgeInactiveFontData();

#if PLATFORM(WIN)
    RefPtr<Font> fontFromDescriptionAndLogFont(const FontDescription&, const LOGFONT&, AtomicString& outFontFamilyName);
#endif

#if ENABLE(OPENTYPE_VERTICAL)
    RefPtr<OpenTypeVerticalData> verticalData(const FontPlatformData&);
#endif

    std::unique_ptr<FontPlatformData> createFontPlatformDataForTesting(const FontDescription&, const AtomicString& family);
    
    bool shouldMockBoldSystemFontForAccessibility() const { return m_shouldMockBoldSystemFontForAccessibility; }
    void setShouldMockBoldSystemFontForAccessibility(bool shouldMockBoldSystemFontForAccessibility) { m_shouldMockBoldSystemFontForAccessibility = shouldMockBoldSystemFontForAccessibility; }

    struct PrewarmInformation {
        Vector<String> seenFamilies;
        Vector<String> fontNamesRequiringSystemFallback;

        bool isEmpty() const;
        PrewarmInformation isolatedCopy() const;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<PrewarmInformation> decode(Decoder&);
    };
    PrewarmInformation collectPrewarmInformation() const;
    void prewarm(const PrewarmInformation&);

private:
    FontCache();
    ~FontCache() = delete;

    WEBCORE_EXPORT void purgeInactiveFontDataIfNeeded();

    // FIXME: This method should eventually be removed.
    FontPlatformData* getCachedFontPlatformData(const FontDescription&, const AtomicString& family, const FontFeatureSettings* fontFaceFeatures = nullptr, const FontVariantSettings* fontFaceVariantSettings = nullptr, FontSelectionSpecifiedCapabilities fontFaceCapabilities = { }, bool checkingAlternateName = false);

    // These methods are implemented by each platform.
#if PLATFORM(COCOA)
    FontPlatformData* getCustomFallbackFont(const UInt32, const FontDescription&);
#endif
    WEBCORE_EXPORT std::unique_ptr<FontPlatformData> createFontPlatformData(const FontDescription&, const AtomicString& family, const FontFeatureSettings* fontFaceFeatures, const FontVariantSettings* fontFaceVariantSettings, FontSelectionSpecifiedCapabilities fontFaceCapabilities);
    
    static const AtomicString& alternateFamilyName(const AtomicString&);
    static const AtomicString& platformAlternateFamilyName(const AtomicString&);

    Timer m_purgeTimer;
    
    bool m_shouldMockBoldSystemFontForAccessibility { false };

#if PLATFORM(COCOA)
    ListHashSet<String> m_seenFamiliesForPrewarming;
    ListHashSet<String> m_fontNamesRequiringSystemFallbackForPrewarming;
    RefPtr<WorkQueue> m_prewarmQueue;

    friend class ComplexTextController;
#endif
    friend class Font;
};

inline std::unique_ptr<FontPlatformData> FontCache::createFontPlatformDataForTesting(const FontDescription& fontDescription, const AtomicString& family)
{
    return createFontPlatformData(fontDescription, family, nullptr, nullptr, { });
}

#if !PLATFORM(COCOA)

inline void FontCache::platformPurgeInactiveFontData()
{
}

#endif


inline bool FontCache::PrewarmInformation::isEmpty() const
{
    return seenFamilies.isEmpty() && fontNamesRequiringSystemFallback.isEmpty();
}

inline FontCache::PrewarmInformation FontCache::PrewarmInformation::isolatedCopy() const
{
    return { seenFamilies.isolatedCopy(), fontNamesRequiringSystemFallback.isolatedCopy() };
}

template<class Encoder>
void FontCache::PrewarmInformation::encode(Encoder& encoder) const
{
    encoder << seenFamilies;
    encoder << fontNamesRequiringSystemFallback;
}

template<class Decoder>
Optional<FontCache::PrewarmInformation> FontCache::PrewarmInformation::decode(Decoder& decoder)
{
    PrewarmInformation prewarmInformation;
    if (!decoder.decode(prewarmInformation.seenFamilies))
        return { };
    if (!decoder.decode(prewarmInformation.fontNamesRequiringSystemFallback))
        return { };

    return prewarmInformation;
}

}
