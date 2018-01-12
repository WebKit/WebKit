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
#include "FontDescription.h"

#include "FontCache.h"
#include "FontFamilySpecificationCoreText.h"
#include <pal/spi/cocoa/CoreTextSPI.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

#if PLATFORM(IOS)
#include "RenderThemeIOS.h"
#endif

#if USE_PLATFORM_SYSTEM_FALLBACK_LIST

namespace WebCore {

class SystemFontDatabase {
public:
    struct CoreTextCascadeListParameters {
        CoreTextCascadeListParameters()
        {
        }

        CoreTextCascadeListParameters(WTF::HashTableDeletedValueType)
            : fontName(WTF::HashTableDeletedValue)
        {
        }

        bool isHashTableDeletedValue() const
        {
            return fontName.isHashTableDeletedValue();
        }

        bool operator==(const CoreTextCascadeListParameters& other) const
        {
            return fontName == other.fontName
                && locale == other.locale
                && weight == other.weight
                && size == other.size
                && italic == other.italic;
        }

        unsigned hash() const
        {
            IntegerHasher hasher;
            ASSERT(!fontName.isNull());
            hasher.add(locale.existingHash());
            hasher.add(locale.isNull() ? 0 : locale.existingHash());
            hasher.add(weight);
            hasher.add(size);
            hasher.add(italic);
            return hasher.hash();
        }

        struct CoreTextCascadeListParametersHash : WTF::PairHash<AtomicString, float> {
            static unsigned hash(const CoreTextCascadeListParameters& parameters)
            {
                return parameters.hash();
            }
            static bool equal(const CoreTextCascadeListParameters& a, const CoreTextCascadeListParameters& b)
            {
                return a == b;
            }
            static const bool safeToCompareToEmptyOrDeleted = true;
        };

        AtomicString fontName;
        AtomicString locale;
        CGFloat weight { 0 };
        float size { 0 };
        bool italic { false };
    };

    static SystemFontDatabase& singleton()
    {
        static NeverDestroyed<SystemFontDatabase> database = SystemFontDatabase();
        return database.get();
    }

    enum class ClientUse { ForSystemUI, ForTextStyle };

    Vector<RetainPtr<CTFontDescriptorRef>> systemFontCascadeList(const CoreTextCascadeListParameters& parameters, ClientUse clientUse)
    {
        ASSERT(!parameters.fontName.isNull());
        return m_systemFontCache.ensure(parameters, [&] {
            auto localeString = parameters.locale.string().createCFString();
            RetainPtr<CTFontRef> systemFont;
            if (clientUse == ClientUse::ForSystemUI) {
                systemFont = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, parameters.size, localeString.get()));
                ASSERT(systemFont);
                // FIXME: Use applyWeightAndItalics() in both cases once <rdar://problem/33046041> is fixed.
                systemFont = applyWeightAndItalics(systemFont.get(), parameters.weight, parameters.italic, parameters.size);
            } else {
#if PLATFORM(IOS)
                ASSERT(clientUse == ClientUse::ForTextStyle);
                auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(parameters.fontName.string().createCFString().get(), RenderThemeIOS::contentSizeCategory(), nullptr));
                CTFontSymbolicTraits traits = (parameters.weight >= kCTFontWeightSemibold ? kCTFontTraitBold : 0) | (parameters.italic ? kCTFontTraitItalic : 0);
                if (traits)
                    fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), traits, traits));
                systemFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), parameters.size, nullptr));
#else
                ASSERT_NOT_REACHED();
#endif
            }
            ASSERT(systemFont);
            auto result = computeCascadeList(systemFont.get(), localeString.get());
            ASSERT(!result.isEmpty());
            return result;
        }).iterator->value;
    }

    void clear()
    {
        m_systemFontCache.clear();
    }

private:
    SystemFontDatabase()
    {
    }

    static RetainPtr<CTFontRef> applyWeightAndItalics(CTFontRef font, CGFloat weight, bool italic, float size)
    {
        auto weightNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &weight));
        const float systemFontItalicSlope = 0.07;
        float italicsRawNumber = italic ? systemFontItalicSlope : 0;
        auto italicsNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &italicsRawNumber));
        CFTypeRef traitsKeys[] = { kCTFontWeightTrait, kCTFontSlantTrait, kCTFontUIFontDesignTrait };
        CFTypeRef traitsValues[] = { weightNumber.get(), italicsNumber.get(), kCFBooleanTrue };
        auto traitsDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, traitsKeys, traitsValues, WTF_ARRAY_LENGTH(traitsKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFTypeRef modificationKeys[] = { kCTFontTraitsAttribute };
        CFTypeRef modificationValues[] = { traitsDictionary.get() };
        auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, modificationKeys, modificationValues, WTF_ARRAY_LENGTH(modificationKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
        return adoptCF(CTFontCreateCopyWithAttributes(font, size, nullptr, modification.get()));
    }

    static RetainPtr<CTFontDescriptorRef> removeCascadeList(CTFontDescriptorRef fontDescriptor)
    {
        auto emptyArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, &kCFTypeArrayCallBacks));
        CFTypeRef fallbackDictionaryKeys[] = { kCTFontCascadeListAttribute };
        CFTypeRef fallbackDictionaryValues[] = { emptyArray.get() };
        auto fallbackDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, fallbackDictionaryKeys, fallbackDictionaryValues, WTF_ARRAY_LENGTH(fallbackDictionaryKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        auto modifiedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor, fallbackDictionary.get()));
        return modifiedFontDescriptor;
    }

    static Vector<RetainPtr<CTFontDescriptorRef>> computeCascadeList(CTFontRef font, CFStringRef locale)
    {
        CFTypeRef arrayValues[] = { locale };
        auto localeArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, arrayValues, WTF_ARRAY_LENGTH(arrayValues), &kCFTypeArrayCallBacks));
        auto cascadeList = adoptCF(CTFontCopyDefaultCascadeListForLanguages(font, localeArray.get()));
        Vector<RetainPtr<CTFontDescriptorRef>> result;
        // WebKit handles the cascade list, and WebKit 2's IPC code doesn't know how to serialize Core Text's cascade list.
        result.append(removeCascadeList(adoptCF(CTFontCopyFontDescriptor(font)).get()));
        if (cascadeList) {
            CFIndex arrayLength = CFArrayGetCount(cascadeList.get());
            for (CFIndex i = 0; i < arrayLength; ++i)
                result.append(static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(cascadeList.get(), i)));
        }
        return result;
    }

    HashMap<CoreTextCascadeListParameters, Vector<RetainPtr<CTFontDescriptorRef>>, CoreTextCascadeListParameters::CoreTextCascadeListParametersHash, SimpleClassHashTraits<CoreTextCascadeListParameters>> m_systemFontCache;
};

static inline bool isSystemFontString(const AtomicString& string)
{
    return equalLettersIgnoringASCIICase(string, "-webkit-system-font")
        || equalLettersIgnoringASCIICase(string, "-apple-system")
        || equalLettersIgnoringASCIICase(string, "-apple-system-font")
        || equalLettersIgnoringASCIICase(string, "system-ui");
}

#if PLATFORM(IOS)

template<typename T, typename U, std::size_t size, std::size_t... indices> std::array<T, size> convertArray(U (&array)[size], std::index_sequence<indices...>)
{
    return { { array[indices]... } };
}

template<typename T, typename U, std::size_t size> inline std::array<T, size> convertArray(U (&array)[size])
{
    return convertArray<T>(array, std::make_index_sequence<size> { });
}

static inline bool isUIFontTextStyle(const AtomicString& string)
{
    static const CFStringRef styles[] = {
        kCTUIFontTextStyleHeadline,
        kCTUIFontTextStyleBody,
        kCTUIFontTextStyleTitle1,
        kCTUIFontTextStyleTitle2,
        kCTUIFontTextStyleTitle3,
        kCTUIFontTextStyleSubhead,
        kCTUIFontTextStyleFootnote,
        kCTUIFontTextStyleCaption1,
        kCTUIFontTextStyleCaption2,
        kCTUIFontTextStyleShortHeadline,
        kCTUIFontTextStyleShortBody,
        kCTUIFontTextStyleShortSubhead,
        kCTUIFontTextStyleShortFootnote,
        kCTUIFontTextStyleShortCaption1,
        kCTUIFontTextStyleTallBody,
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
        kCTUIFontTextStyleTitle0,
        kCTUIFontTextStyleTitle4,
#endif
    };
    
    static auto strings { makeNeverDestroyed(convertArray<AtomicString>(styles)) };
    return std::find(strings.get().begin(), strings.get().end(), string) != strings.get().end();
}
#endif

static inline SystemFontDatabase::CoreTextCascadeListParameters systemFontParameters(const FontCascadeDescription& description, const AtomicString& familyName, SystemFontDatabase::ClientUse clientUse)
{
    SystemFontDatabase::CoreTextCascadeListParameters result;
    result.locale = description.locale();
    result.size = description.computedSize();
    result.italic = isItalic(description.italic());

    auto weight = description.weight();
    if (FontCache::singleton().shouldMockBoldSystemFontForAccessibility())
        weight = weight + FontSelectionValue(200);

    if (weight < FontSelectionValue(150))
        result.weight = kCTFontWeightUltraLight;
    else if (weight < FontSelectionValue(250))
        result.weight = kCTFontWeightThin;
    else if (weight < FontSelectionValue(350))
        result.weight = kCTFontWeightLight;
    else if (weight < FontSelectionValue(450))
        result.weight = kCTFontWeightRegular;
    else if (weight < FontSelectionValue(550))
        result.weight = kCTFontWeightMedium;
    else if (weight < FontSelectionValue(650))
        result.weight = kCTFontWeightSemibold;
    else if (weight < FontSelectionValue(750))
        result.weight = kCTFontWeightBold;
    else if (weight < FontSelectionValue(850))
        result.weight = kCTFontWeightHeavy;
    else
        result.weight = kCTFontWeightBlack;

    if (clientUse == SystemFontDatabase::ClientUse::ForSystemUI) {
        static NeverDestroyed<AtomicString> systemUI = AtomicString("system-ui", AtomicString::ConstructFromLiteral);
        result.fontName = systemUI.get();
    } else {
        ASSERT(clientUse == SystemFontDatabase::ClientUse::ForTextStyle);
        result.fontName = familyName;
    }

    return result;
}

void FontDescription::invalidateCaches()
{
    SystemFontDatabase::singleton().clear();
}

static inline Vector<RetainPtr<CTFontDescriptorRef>> systemFontCascadeList(const FontCascadeDescription& description, const AtomicString& cssFamily, SystemFontDatabase::ClientUse clientUse)
{
    return SystemFontDatabase::singleton().systemFontCascadeList(systemFontParameters(description, cssFamily, clientUse), clientUse);
}

unsigned FontCascadeDescription::effectiveFamilyCount() const
{
    // FIXME: Move all the other system font keywords from platformFontWithFamilySpecialCase() to here.
    unsigned result = 0;
    for (unsigned i = 0; i < familyCount(); ++i) {
        const auto& cssFamily = familyAt(i);
        if (isSystemFontString(cssFamily))
            result += systemFontCascadeList(*this, cssFamily, SystemFontDatabase::ClientUse::ForSystemUI).size();
#if PLATFORM(IOS)
        else if (isUIFontTextStyle(cssFamily))
            result += systemFontCascadeList(*this, cssFamily, SystemFontDatabase::ClientUse::ForTextStyle).size();
#endif
        else
            ++result;
    }
    return result;
}

FontFamilySpecification FontCascadeDescription::effectiveFamilyAt(unsigned index) const
{
    // The special cases in this function need to match the behavior in FontCacheIOS.mm and FontCacheMac.mm. On systems
    // where USE_PLATFORM_SYSTEM_FALLBACK_LIST is set to true, this code is used for regular (element style) lookups,
    // and the code in FontDescriptionCocoa.cpp is used when src:local(special-cased-name) is specified inside an
    // @font-face block.
    // FIXME: Currently, an @font-face block corresponds to a single item in the font-family: fallback list, which
    // means that "src:local(system-ui)" can't follow the Core Text cascade list (the way it does for regular lookups).
    // These two behaviors should be unified, which would hopefully allow us to delete this duplicate code.
    for (unsigned i = 0; i < familyCount(); ++i) {
        const auto& cssFamily = familyAt(i);
        if (isSystemFontString(cssFamily)) {
            auto cascadeList = systemFontCascadeList(*this, cssFamily, SystemFontDatabase::ClientUse::ForSystemUI);
            if (index < cascadeList.size())
                return FontFamilySpecification(cascadeList[index].get());
            index -= cascadeList.size();
        }
#if PLATFORM(IOS)
        else if (isUIFontTextStyle(cssFamily)) {
            auto cascadeList = systemFontCascadeList(*this, cssFamily, SystemFontDatabase::ClientUse::ForTextStyle);
            if (index < cascadeList.size())
                return FontFamilySpecification(cascadeList[index].get());
            index -= cascadeList.size();
        }
#endif
        else if (!index)
            return cssFamily;
        else
            --index;
    }
    ASSERT_NOT_REACHED();
    return nullAtom();
}

}

#endif

