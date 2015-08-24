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
struct FontDescriptionKey {
    FontDescriptionKey() = default;

    FontDescriptionKey(const FontDescription& description)
        : m_size(description.computedPixelSize())
        , m_weight(description.weight())
        , m_flags(makeFlagKey(description))
        , m_locale(description.locale())
        , m_featureSettings(description.featureSettings())
    { }

    explicit FontDescriptionKey(WTF::HashTableDeletedValueType)
        : m_size(cHashTableDeletedSize)
    { }

    bool operator==(const FontDescriptionKey& other) const
    {
        return m_size == other.m_size && m_weight == other.m_weight && m_flags == other.m_flags && m_locale == other.m_locale
            && ((m_featureSettings == other.m_featureSettings) || (m_featureSettings && other.m_featureSettings && m_featureSettings.get() == other.m_featureSettings.get()));
    }

    bool operator!=(const FontDescriptionKey& other) const
    {
        return !(*this == other);
    }

    bool isHashTableDeletedValue() const { return m_size == cHashTableDeletedSize; }

    inline unsigned computeHash() const
    {
        unsigned toHash[] = {m_size, m_weight, m_flags, m_locale.isNull() ? 0 : m_locale.impl()->existingHash(), m_featureSettings ? m_featureSettings->hash() : 0};
        return StringHasher::hashMemory(toHash, sizeof(toHash));
    }

private:
    static unsigned makeFlagKey(const FontDescription& description)
    {
        static_assert(USCRIPT_CODE_LIMIT < 0x1000, "Script code must fit in an unsigned along with the other flags");
        return static_cast<unsigned>(description.script()) << 11
            | static_cast<unsigned>(description.textRenderingMode()) << 9
            | static_cast<unsigned>(description.smallCaps()) << 8
            | static_cast<unsigned>(description.fontSynthesis()) << 6
            | static_cast<unsigned>(description.widthVariant()) << 4
            | static_cast<unsigned>(description.nonCJKGlyphOrientation()) << 3
            | static_cast<unsigned>(description.orientation()) << 2
            | static_cast<unsigned>(description.italic()) << 1
            | static_cast<unsigned>(description.renderingMode());
    }

    static const unsigned cHashTableDeletedSize = 0xFFFFFFFFU;

    unsigned m_size { 0 };
    unsigned m_weight { 0 };
    unsigned m_flags { 0 };
    AtomicString m_locale;
    RefPtr<FontFeatureSettings> m_featureSettings;
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

#if PLATFORM(IOS)
    static float weightOfCTFont(CTFontRef);
#endif
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
    Vector<FontTraitsMask> getTraitsInFamily(const AtomicString&);

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

    WEBCORE_EXPORT void purgeInactiveFontDataIfNeeded();

    // FIXME: This method should eventually be removed.
    FontPlatformData* getCachedFontPlatformData(const FontDescription&, const AtomicString& family, bool checkingAlternateName = false);

    // These methods are implemented by each platform.
#if PLATFORM(COCOA)
    FontPlatformData* getCustomFallbackFont(const UInt32, const FontDescription&);
#endif
    std::unique_ptr<FontPlatformData> createFontPlatformData(const FontDescription&, const AtomicString& family);

    Timer m_purgeTimer;

#if PLATFORM(COCOA)
    friend class ComplexTextController;
#endif
    friend class Font;
};

#if PLATFORM(COCOA)

struct SynthesisPair {
    SynthesisPair(bool needsSyntheticBold, bool needsSyntheticOblique)
        : needsSyntheticBold(needsSyntheticBold)
        , needsSyntheticOblique(needsSyntheticOblique)
    {
    }

    std::pair<bool, bool> boldObliquePair() const
    {
        return std::make_pair(needsSyntheticBold, needsSyntheticOblique);
    }

    bool needsSyntheticBold;
    bool needsSyntheticOblique;
};

RetainPtr<CTFontRef> preparePlatformFont(CTFontRef, TextRenderingMode, const FontFeatureSettings*);
FontWeight fontWeightFromCoreText(CGFloat weight);
uint16_t toCoreTextFontWeight(FontWeight);
bool isFontWeightBold(FontWeight);
void platformInvalidateFontCache();
SynthesisPair computeNecessarySynthesis(CTFontRef, const FontDescription&, bool isPlatformFont = false);
RetainPtr<CTFontRef> platformFontWithFamilySpecialCase(const AtomicString& family, FontWeight, CTFontSymbolicTraits, float size);
RetainPtr<CTFontRef> platformFontWithFamily(const AtomicString& family, CTFontSymbolicTraits, FontWeight, const FontFeatureSettings*, TextRenderingMode, float size);
RetainPtr<CTFontRef> platformLookupFallbackFont(CTFontRef, FontWeight, const AtomicString& locale, const UChar* characters, unsigned length);
bool requiresCustomFallbackFont(UChar32 character);

#else

inline void FontCache::platformPurgeInactiveFontData()
{
}

#endif

}

#endif
