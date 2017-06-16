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

#include "CoreTextSPI.h"
#include "FontFamilySpecificationCoreText.h"
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

#if USE_PLATFORM_SYSTEM_FALLBACK_LIST

namespace WebCore {

class SystemFontDatabase {
public:
    struct CoreTextCascadeListParameters {
        CoreTextCascadeListParameters()
        {
        }
        
        CoreTextCascadeListParameters(WTF::HashTableDeletedValueType)
            : locale(WTF::HashTableDeletedValue)
        {
        }
        
        bool isHashTableDeletedValue() const
        {
            return locale.isHashTableDeletedValue();
        }
        
        bool operator==(const CoreTextCascadeListParameters& other) const
        {
            return locale == other.locale
                && weight == other.weight
                && size == other.size
                && italic == other.italic;
        }
        
        unsigned hash() const
        {
            IntegerHasher hasher;
            hasher.add(locale.isNull() ? 0 : locale.existingHash());
            hasher.add(weight);
            hasher.add(size);
            hasher.add(italic);
            return hasher.hash();
        }
    
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
    
    Vector<RetainPtr<CTFontDescriptorRef>> systemFontCascadeList(const CoreTextCascadeListParameters& parameters)
    {
        auto createSystemFont = [&] {
            auto localeString = parameters.locale.string().createCFString();
            return std::make_pair(localeString, adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, parameters.size, localeString.get())));
        };
        
        // Avoid adding an empty value to the hash map
        if (!parameters.size) {
            auto systemFont = createSystemFont().second;
            auto descriptor = adoptCF(CTFontCopyFontDescriptor(systemFont.get()));
            return { removeCascadeList(descriptor.get()) };
        }
        
        return m_systemFontCache.ensure(parameters, [&] {
            RetainPtr<CFStringRef> localeString;
            RetainPtr<CTFontRef> systemFont;
            std::tie(localeString, systemFont) = createSystemFont();
            systemFont = applyWeightAndItalics(systemFont.get(), parameters.weight, parameters.italic, parameters.size);
            return computeCascadeList(systemFont.get(), localeString.get());
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
        CFIndex arrayLength = CFArrayGetCount(cascadeList.get());
        for (CFIndex i = 0; i < arrayLength; ++i)
            result.append(static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(cascadeList.get(), i)));
        return result;
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
    
    HashMap<CoreTextCascadeListParameters, Vector<RetainPtr<CTFontDescriptorRef>>, CoreTextCascadeListParametersHash, SimpleClassHashTraits<CoreTextCascadeListParameters>> m_systemFontCache;
};

static inline bool isSystemFontString(const AtomicString& string)
{
    return equalLettersIgnoringASCIICase(string, "-webkit-system-font")
        || equalLettersIgnoringASCIICase(string, "-apple-system")
        || equalLettersIgnoringASCIICase(string, "-apple-system-font")
        || equalLettersIgnoringASCIICase(string, "system-ui");
}

static inline SystemFontDatabase::CoreTextCascadeListParameters systemFontParameters(const FontCascadeDescription& description)
{
    SystemFontDatabase::CoreTextCascadeListParameters result;
    result.locale = description.locale();
    result.size = description.computedSize();
    result.italic = isItalic(description.italic());
    
    auto weight = description.weight();
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
        
    return result;
}

void FontDescription::invalidateCaches()
{
    SystemFontDatabase::singleton().clear();
}

unsigned FontCascadeDescription::effectiveFamilyCount() const
{
    // FIXME: Move all the other system font keywords from platformFontWithFamilySpecialCase() to here.
    unsigned result = 0;
    for (unsigned i = 0; i < familyCount(); ++i) {
        const auto& cssFamily = familyAt(i);
        if (isSystemFontString(cssFamily))
            result += SystemFontDatabase::singleton().systemFontCascadeList(systemFontParameters(*this)).size();
        else
            ++result;
    }
    return result;
}

FontFamilySpecification FontCascadeDescription::effectiveFamilyAt(unsigned index) const
{
    for (unsigned i = 0; i < familyCount(); ++i) {
        const auto& cssFamily = familyAt(i);
        if (isSystemFontString(cssFamily)) {
            auto cascadeList = SystemFontDatabase::singleton().systemFontCascadeList(systemFontParameters(*this));
            if (index < cascadeList.size())
                return FontFamilySpecification(cascadeList[index].get());
            index -= cascadeList.size();
        } else if (!index)
            return cssFamily;
        else
            --index;
    }
    ASSERT_NOT_REACHED();
    return nullAtom;
}

}

#endif
