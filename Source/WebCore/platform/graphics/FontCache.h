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

#ifndef FontCache_h
#define FontCache_h

#include "FontDescription.h"
#include "Timer.h"
#include <array>
#include <limits.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <CoreText/CTFont.h>
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
struct FontDescriptionFontDataCacheKey {
#if !defined(_MSC_VER) || _MSC_VER > 1800
    FontDescriptionFontDataCacheKey() = default;
#else
    FontDescriptionFontDataCacheKey()
    {
        m_flags[0] = 0;
        m_flags[1] = 0;
    }
#endif

    FontDescriptionFontDataCacheKey(const FontDescription& description)
        : m_size(description.computedPixelSize())
        , m_weight(description.weight())
        , m_flags(makeFlagsKey(description))
        , m_featureSettings(description.featureSettings())
    { }

    explicit FontDescriptionFontDataCacheKey(WTF::HashTableDeletedValueType)
        : m_size(cHashTableDeletedSize)
    { }

    bool operator==(const FontDescriptionFontDataCacheKey& other) const
    {
        return m_size == other.m_size && m_weight == other.m_weight && m_flags == other.m_flags
            && m_featureSettings == other.m_featureSettings;
    }

    bool operator!=(const FontDescriptionFontDataCacheKey& other) const
    {
        return !(*this == other);
    }

    bool isHashTableDeletedValue() const { return m_size == cHashTableDeletedSize; }

    inline unsigned computeHash() const
    {
        unsigned hashValues[] = { m_size, m_weight, m_flags[0], m_flags[1], m_featureSettings.hash() };
        return StringHasher::hashMemory(hashValues, sizeof(hashValues));
    }

private:
    static std::array<unsigned, 2> makeFlagsKey(const FontDescription& description)
    {
        static_assert(USCRIPT_CODE_LIMIT < 0x1000, "Script code must fit in an unsigned along with the other flags");
        unsigned first = static_cast<unsigned>(description.script()) << 8
            | static_cast<unsigned>(description.fontSynthesis()) << 6
            | static_cast<unsigned>(description.widthVariant()) << 4
            | static_cast<unsigned>(description.nonCJKGlyphOrientation()) << 3
            | static_cast<unsigned>(description.orientation()) << 2
            | static_cast<unsigned>(description.italic()) << 1
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

    unsigned m_size { 0 };
    unsigned m_weight { 0 };
#if !defined(_MSC_VER) || _MSC_VER > 1800
    std::array<unsigned, 2> m_flags{ { 0, 0 } };
#else
    std::array<unsigned, 2> m_flags;
#endif
    FontFeatureSettings m_featureSettings;
};

struct FontDescriptionKeyHash {
    static unsigned hash(const FontDescriptionFontDataCacheKey& key)
    {
        return key.computeHash();
    }

    static bool equal(const FontDescriptionFontDataCacheKey& a, const FontDescriptionFontDataCacheKey& b)
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

#if PLATFORM(IOS)
    static float weightOfCTFont(CTFontRef);
#endif
#if PLATFORM(MAC)
    WEBCORE_EXPORT static void setFontWhitelist(const Vector<String>&);
#endif
#if PLATFORM(WIN)
    IMLangFontLinkType* getFontLinkInterface();
    static void comInitialize();
    static void comUninitialize();
    static IMultiLanguage* getMultiLanguageInterface();
#endif

    void getTraitsInFamily(const AtomicString&, Vector<unsigned>&);

    WEBCORE_EXPORT RefPtr<Font> fontForFamily(const FontDescription&, const AtomicString&, bool checkingAlternateName = false);
    WEBCORE_EXPORT Ref<Font> lastResortFallbackFont(const FontDescription&);
    WEBCORE_EXPORT Ref<Font> fontForPlatformData(const FontPlatformData&);
    RefPtr<Font> similarFont(const FontDescription&);

    void addClient(FontSelector*);
    void removeClient(FontSelector*);

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
    typedef AtomicString FontFileKey;
    PassRefPtr<OpenTypeVerticalData> getVerticalData(const FontFileKey&, const FontPlatformData&);
#endif

private:
    FontCache();
    ~FontCache() = delete;

    void purgeTimerFired();

    WEBCORE_EXPORT void purgeInactiveFontDataIfNeeded();

    // FIXME: This method should eventually be removed.
    FontPlatformData* getCachedFontPlatformData(const FontDescription&, const AtomicString& family, bool checkingAlternateName = false);

    // These methods are implemented by each platform.
#if PLATFORM(IOS)
    FontPlatformData* getCustomFallbackFont(const UInt32, const FontDescription&);
    PassRefPtr<Font> getSystemFontFallbackForCharacters(const FontDescription&, const Font*, const UChar* characters, unsigned length);
#endif
    std::unique_ptr<FontPlatformData> createFontPlatformData(const FontDescription&, const AtomicString& family);

    Timer m_purgeTimer;

#if PLATFORM(COCOA)
    friend class ComplexTextController;
#endif
    friend class Font;
};

#if PLATFORM(COCOA)
RetainPtr<CTFontRef> applyFontFeatureSettings(CTFontRef, const FontFeatureSettings* fontFaceFeatures, const FontVariantSettings* fontFaceVariantSettings, const FontFeatureSettings& features, const FontVariantSettings&);
#endif

#if !PLATFORM(MAC)
inline void FontCache::platformPurgeInactiveFontData()
{
}
#endif

}

#endif
