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

#include "FontCache.h"
#include "FontCascadeDescription.h"

#if PLATFORM(IOS_FAMILY)
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

#if USE(PLATFORM_SYSTEM_FALLBACK_LIST)

RetainPtr<CTFontRef> SystemFontDatabaseCoreText::createSystemUIFont(const CascadeListParameters& parameters, CFStringRef locale)
{
    auto result = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, parameters.size, locale));
    ASSERT(result);
    return createFontByApplyingWeightItalicsAndFallbackBehavior(result.get(), parameters.weight, parameters.italic, parameters.size, parameters.allowUserInstalledFonts);
}

#if HAVE(DESIGN_SYSTEM_UI_FONTS)
RetainPtr<CTFontRef> SystemFontDatabaseCoreText::createDesignSystemUIFont(ClientUse clientUse, const CascadeListParameters& parameters)
{
    CFStringRef design = kCTFontUIFontDesignDefault;
    switch (clientUse) {
    case ClientUse::ForSystemUISerif:
        design = kCTFontUIFontDesignSerif;
        break;
    case ClientUse::ForSystemUIMonospaced:
        design = kCTFontUIFontDesignMonospaced;
        break;
    case ClientUse::ForSystemUIRounded:
        design = kCTFontUIFontDesignRounded;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return createFontByApplyingWeightItalicsAndFallbackBehavior(nullptr, parameters.weight, parameters.italic, parameters.size, parameters.allowUserInstalledFonts, design);
}
#endif

RetainPtr<CTFontRef> SystemFontDatabaseCoreText::createTextStyleFont(const CascadeListParameters& parameters)
{
#if PLATFORM(IOS_FAMILY)
    auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(parameters.fontName.string().createCFString().get(), RenderThemeIOS::contentSizeCategory(), nullptr));
    // FIXME: Use createFontByApplyingWeightItalicsAndFallbackBehavior() once <rdar://problem/33046041> is fixed.
    CTFontSymbolicTraits traits = (parameters.weight >= kCTFontWeightSemibold ? kCTFontTraitBold : 0) | (parameters.italic ? kCTFontTraitItalic : 0);
    if (traits)
        fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), traits, traits));
    return createFontForInstalledFonts(fontDescriptor.get(), parameters.size, parameters.allowUserInstalledFonts);
#else
    UNUSED_PARAM(parameters);
    ASSERT_NOT_REACHED();
    return nullptr;
#endif
}

Vector<RetainPtr<CTFontDescriptorRef>> SystemFontDatabaseCoreText::cascadeList(const CascadeListParameters& parameters, ClientUse clientUse)
{
    ASSERT(!parameters.fontName.isNull());
    return m_systemFontCache.ensure(parameters, [&] {
        auto localeString = parameters.locale.string().createCFString();
        RetainPtr<CTFontRef> systemFont;
        switch (clientUse) {
        case ClientUse::ForSystemUI:
            systemFont = createSystemUIFont(parameters, localeString.get());
            break;
        case ClientUse::ForSystemUISerif:
        case ClientUse::ForSystemUIMonospaced:
        case ClientUse::ForSystemUIRounded:
#if HAVE(DESIGN_SYSTEM_UI_FONTS)
            systemFont = createDesignSystemUIFont(clientUse, parameters);
#endif
            break;
        case ClientUse::ForTextStyle:
            systemFont = createTextStyleFont(parameters);
            break;
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
    m_serifFamilies.clear();
    m_sansSeriferifFamilies.clear();
    m_cursiveFamilies.clear();
    m_fantasyFamilies.clear();
    m_monospaceFamilies.clear();
}

RetainPtr<CTFontRef> SystemFontDatabaseCoreText::createFontByApplyingWeightItalicsAndFallbackBehavior(CTFontRef font, CGFloat weight, bool italic, float size, AllowUserInstalledFonts allowUserInstalledFonts, CFStringRef design)
{
    auto weightNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &weight));
    const float systemFontItalicSlope = 0.07;
    float italicsRawNumber = italic ? systemFontItalicSlope : 0;
    auto italicsNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &italicsRawNumber));
    CFTypeRef traitsKeys[] = { kCTFontWeightTrait, kCTFontSlantTrait, kCTFontUIFontDesignTrait };
    CFTypeRef traitsValues[] = { weightNumber.get(), italicsNumber.get(), design ? static_cast<CFTypeRef>(design) : static_cast<CFTypeRef>(kCTFontUIFontDesignDefault) };
    auto traitsDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, traitsKeys, traitsValues, WTF_ARRAY_LENGTH(traitsKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionaryAddValue(attributes.get(), kCTFontTraitsAttribute, traitsDictionary.get());
    addAttributesForInstalledFonts(attributes.get(), allowUserInstalledFonts);
    auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    if (font)
        return adoptCF(CTFontCreateCopyWithAttributes(font, size, nullptr, modification.get()));
    return adoptCF(CTFontCreateWithFontDescriptor(modification.get(), size, nullptr));
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

SystemFontDatabaseCoreText::CascadeListParameters SystemFontDatabaseCoreText::systemFontParameters(const FontDescription& description, const AtomString& familyName, ClientUse clientUse, AllowUserInstalledFonts allowUserInstalledFonts)
{
    CascadeListParameters result;
    result.locale = description.locale();
    result.size = description.computedSize();
    result.italic = isItalic(description.italic());
    result.allowUserInstalledFonts = allowUserInstalledFonts;

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

    switch (clientUse) {
    case ClientUse::ForSystemUI: {
        static NeverDestroyed<AtomString> systemUI = AtomString("system-ui", AtomString::ConstructFromLiteral);
        result.fontName = systemUI.get();
        break;
    }
    case ClientUse::ForSystemUISerif: {
        static NeverDestroyed<AtomString> systemUISerif = AtomString("system-ui-serif", AtomString::ConstructFromLiteral);
        result.fontName = systemUISerif.get();
        break;
    }
    case ClientUse::ForSystemUIMonospaced: {
        static NeverDestroyed<AtomString> systemUIMonospaced = AtomString("system-ui-monospaced", AtomString::ConstructFromLiteral);
        result.fontName = systemUIMonospaced.get();
        break;
    }
    case ClientUse::ForSystemUIRounded: {
        static NeverDestroyed<AtomString> systemUIRounded = AtomString("system-ui-rounded", AtomString::ConstructFromLiteral);
        result.fontName = systemUIRounded.get();
        break;
    }
    case ClientUse::ForTextStyle:
        result.fontName = familyName;
        break;
    }

    return result;
}

Vector<RetainPtr<CTFontDescriptorRef>> SystemFontDatabaseCoreText::cascadeList(const FontDescription& description, const AtomString& cssFamily, ClientUse clientUse, AllowUserInstalledFonts allowUserInstalledFonts)
{
    return cascadeList(systemFontParameters(description, cssFamily, clientUse, allowUserInstalledFonts), clientUse);
}

#endif // USE(PLATFORM_SYSTEM_FALLBACK_LIST)

static String genericFamily(const String& locale, HashMap<String, String>& map, CFStringRef ctKey)
{
    return map.ensure(locale, [&] {
        auto descriptor = adoptCF(CTFontDescriptorCreateForCSSFamily(ctKey, locale.createCFString().get()));
        return adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(descriptor.get(), kCTFontFamilyNameAttribute))).get();
    }).iterator->value;
}

String SystemFontDatabaseCoreText::serifFamily(const String& locale)
{
    return genericFamily(locale, m_serifFamilies, kCTFontCSSFamilySerif);
}

String SystemFontDatabaseCoreText::sansSerifFamily(const String& locale)
{
    return genericFamily(locale, m_sansSeriferifFamilies, kCTFontCSSFamilySansSerif);
}

String SystemFontDatabaseCoreText::cursiveFamily(const String& locale)
{
    return genericFamily(locale, m_cursiveFamilies, kCTFontCSSFamilyCursive);
}

String SystemFontDatabaseCoreText::fantasyFamily(const String& locale)
{
    return genericFamily(locale, m_fantasyFamilies, kCTFontCSSFamilyFantasy);
}

String SystemFontDatabaseCoreText::monospaceFamily(const String& locale)
{
    auto result = genericFamily(locale, m_monospaceFamilies, kCTFontCSSFamilyMonospace);
#if PLATFORM(MAC) && ENABLE(MONOSPACE_FONT_EXCEPTION)
    // In general, CoreText uses Monaco for monospaced (see: Terminal.app and Xcode.app).
    // For now, we want to use Courier for web compatibility, until we have more time to do compatibility testing.
    if (equalLettersIgnoringASCIICase(result, "monaco"))
        return "Courier"_str;
#elif PLATFORM(IOS_FAMILY) && ENABLE(MONOSPACE_FONT_EXCEPTION)
    if (equalLettersIgnoringASCIICase(result, "courier new"))
        return "Courier"_str;
#endif
    return result;
}

}
