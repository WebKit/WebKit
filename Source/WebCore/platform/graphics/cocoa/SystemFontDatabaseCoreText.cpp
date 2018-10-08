/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SystemFontDatabaseCoreText.h"

#if USE_PLATFORM_SYSTEM_FALLBACK_LIST

#include "FontCache.h"
#include "FontCascadeDescription.h"

#if PLATFORM(IOS)
#include "RenderThemeIOS.h"
#endif

namespace WebCore {

SystemFontDatabaseCoreText& SystemFontDatabaseCoreText::singleton()
{
    static NeverDestroyed<SystemFontDatabaseCoreText> database = SystemFontDatabaseCoreText();
    return database.get();
}

SystemFontDatabaseCoreText::SystemFontDatabaseCoreText()
{
}

Vector<RetainPtr<CTFontDescriptorRef>> SystemFontDatabaseCoreText::cascadeList(const CascadeListParameters& parameters, ClientUse clientUse)
{
    ASSERT(!parameters.fontName.isNull());
    return m_systemFontCache.ensure(parameters, [&] {
        auto localeString = parameters.locale.string().createCFString();
        RetainPtr<CTFontRef> systemFont;
        if (clientUse == ClientUse::ForSystemUI) {
            systemFont = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, parameters.size, localeString.get()));
            ASSERT(systemFont);
            // FIXME: Use applyWeightItalicsAndFallbackBehavior() in both cases once <rdar://problem/33046041> is fixed.
            systemFont = applyWeightItalicsAndFallbackBehavior(systemFont.get(), parameters.weight, parameters.italic, parameters.size, parameters.allowUserInstalledFonts);
        } else {
#if PLATFORM(IOS)
            ASSERT(clientUse == ClientUse::ForTextStyle);
            auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(parameters.fontName.string().createCFString().get(), RenderThemeIOS::contentSizeCategory(), nullptr));
            CTFontSymbolicTraits traits = (parameters.weight >= kCTFontWeightSemibold ? kCTFontTraitBold : 0) | (parameters.italic ? kCTFontTraitItalic : 0);
            if (traits)
                fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), traits, traits));
            systemFont = createFontForInstalledFonts(fontDescriptor.get(), parameters.size, parameters.allowUserInstalledFonts);
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

void SystemFontDatabaseCoreText::clear()
{
    m_systemFontCache.clear();
}

RetainPtr<CTFontRef> SystemFontDatabaseCoreText::applyWeightItalicsAndFallbackBehavior(CTFontRef font, CGFloat weight, bool italic, float size, AllowUserInstalledFonts allowUserInstalledFonts)
{
    auto weightNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &weight));
    const float systemFontItalicSlope = 0.07;
    float italicsRawNumber = italic ? systemFontItalicSlope : 0;
    auto italicsNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &italicsRawNumber));
    CFTypeRef traitsKeys[] = { kCTFontWeightTrait, kCTFontSlantTrait, kCTFontUIFontDesignTrait };
    CFTypeRef traitsValues[] = { weightNumber.get(), italicsNumber.get(), kCFBooleanTrue };
    auto traitsDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, traitsKeys, traitsValues, WTF_ARRAY_LENGTH(traitsKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionaryAddValue(attributes.get(), kCTFontTraitsAttribute, traitsDictionary.get());
    addAttributesForInstalledFonts(attributes.get(), allowUserInstalledFonts);
    auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    return adoptCF(CTFontCreateCopyWithAttributes(font, size, nullptr, modification.get()));
}

RetainPtr<CTFontDescriptorRef> SystemFontDatabaseCoreText::removeCascadeList(CTFontDescriptorRef fontDescriptor)
{
    auto emptyArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, &kCFTypeArrayCallBacks));
    CFTypeRef fallbackDictionaryKeys[] = { kCTFontCascadeListAttribute };
    CFTypeRef fallbackDictionaryValues[] = { emptyArray.get() };
    auto fallbackDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, fallbackDictionaryKeys, fallbackDictionaryValues, WTF_ARRAY_LENGTH(fallbackDictionaryKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto modifiedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor, fallbackDictionary.get()));
    return modifiedFontDescriptor;
}

Vector<RetainPtr<CTFontDescriptorRef>> SystemFontDatabaseCoreText::computeCascadeList(CTFontRef font, CFStringRef locale)
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

SystemFontDatabaseCoreText::CascadeListParameters SystemFontDatabaseCoreText::systemFontParameters(const FontCascadeDescription& description, const AtomicString& familyName, ClientUse clientUse, AllowUserInstalledFonts allowUserInstalledFonts)
{
    CascadeListParameters result;
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

    if (clientUse == ClientUse::ForSystemUI) {
        static NeverDestroyed<AtomicString> systemUI = AtomicString("system-ui", AtomicString::ConstructFromLiteral);
        result.fontName = systemUI.get();
    } else {
        ASSERT(clientUse == ClientUse::ForTextStyle);
        result.fontName = familyName;
    }

    result.allowUserInstalledFonts = allowUserInstalledFonts;

    return result;
}

Vector<RetainPtr<CTFontDescriptorRef>> SystemFontDatabaseCoreText::cascadeList(const FontCascadeDescription& description, const AtomicString& cssFamily, ClientUse clientUse, AllowUserInstalledFonts allowUserInstalledFonts)
{
    return cascadeList(systemFontParameters(description, cssFamily, clientUse, allowUserInstalledFonts), clientUse);
}

}

#endif
