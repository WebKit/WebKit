/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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
#include "FontCacheCoreText.h"
#include "FontCascadeDescription.h"
#include "FontMetricsNormalization.h"

#include <pal/system/ios/UserInterfaceIdiom.h>
#include <ranges>
#include <wtf/cf/TypeCastsCF.h>

namespace WebCore {

SystemFontDatabaseCoreText& SystemFontDatabaseCoreText::forCurrentThread()
{
    return FontCache::forCurrentThread().systemFontDatabaseCoreText();
}

SystemFontDatabase& SystemFontDatabase::singleton()
{
    return SystemFontDatabaseCoreText::forCurrentThread();
}

SystemFontDatabaseCoreText::SystemFontDatabaseCoreText() = default;

RetainPtr<CTFontRef> SystemFontDatabaseCoreText::createSystemUIFont(const CascadeListParameters& parameters, CFStringRef locale)
{
    // Work around a quirk of the platform API.
    // If the passed string is empty instead of null,
    // CoreText doesn't use the system's locale instead.
    // We need to use the system locale in this case.
    if (locale && !CFStringGetLength(locale))
        locale = nullptr;
    auto result = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, parameters.size, locale));
    ASSERT(result);
    return createFontByApplyingWeightWidthItalicsAndFallbackBehavior(result.get(), parameters.weight, parameters.width, parameters.italic, parameters.size, parameters.allowUserInstalledFonts);
}

RetainPtr<CTFontRef> SystemFontDatabaseCoreText::createSystemDesignFont(SystemFontKind systemFontKind, const CascadeListParameters& parameters)
{
    CFStringRef design = kCTFontUIFontDesignDefault;
    switch (systemFontKind) {
    case SystemFontKind::UISerif:
        design = kCTFontUIFontDesignSerif;
        break;
    case SystemFontKind::UIMonospace:
        design = kCTFontUIFontDesignMonospaced;
        break;
    case SystemFontKind::UIRounded:
        design = kCTFontUIFontDesignRounded;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return createFontByApplyingWeightWidthItalicsAndFallbackBehavior(nullptr, parameters.weight, parameters.width, parameters.italic, parameters.size, parameters.allowUserInstalledFonts, design);
}

RetainPtr<CTFontRef> SystemFontDatabaseCoreText::createTextStyleFont(const CascadeListParameters& parameters)
{
    RetainPtr<CFStringRef> localeString = isEmpty(parameters.locale) ? nullptr : createCFString(parameters.locale);
    auto descriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(createCFString(parameters.fontName).get(), protect(contentSizeCategory()).get(), localeString.get()));
    // FIXME: Use createFontByApplyingWeightWidthItalicsAndFallbackBehavior().
    CTFontSymbolicTraits traits = (parameters.weight >= kCTFontWeightSemibold ? kCTFontTraitBold : 0)
        | (parameters.width >= kCTFontWidthSemiExpanded ? kCTFontTraitExpanded : 0)
        | (parameters.width <= kCTFontWidthSemiCondensed ? kCTFontTraitCondensed : 0)
        | (parameters.italic ? kCTFontTraitItalic : 0);
    if (traits)
        descriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(descriptor.get(), traits, traits));
    return createFontForInstalledFonts(descriptor.get(), parameters.size, parameters.allowUserInstalledFonts);
}

Vector<RetainPtr<CTFontDescriptorRef>> SystemFontDatabaseCoreText::cascadeList(const CascadeListParameters& parameters, SystemFontKind systemFontKind)
{
    ASSERT(parameters.fontName);
    return m_systemFontCache.ensure(parameters, [&] {
        auto localeString = createCFString(parameters.locale);
        RetainPtr<CTFontRef> systemFont;
        switch (systemFontKind) {
        case SystemFontKind::SystemUI:
            systemFont = createSystemUIFont(parameters, localeString.get());
            break;
        case SystemFontKind::UISerif:
        case SystemFontKind::UIMonospace:
        case SystemFontKind::UIRounded:
            systemFont = createSystemDesignFont(systemFontKind, parameters);
            break;
        case SystemFontKind::TextStyle:
            systemFont = createTextStyleFont(parameters);
            break;
        }
        ASSERT(systemFont);
        auto result = computeCascadeList(systemFont.get(), localeString.get());
        ASSERT(!result.isEmpty());
        return result;
    }).iterator->value;
}

void SystemFontDatabase::platformInvalidate()
{
    SystemFontDatabaseCoreText::forCurrentThread().clear();
}

void SystemFontDatabaseCoreText::clear()
{
    // Don't call this directly. Instead, you should be calling FontCache::invalidateAllFontCaches().
    m_systemFontCache.clear();
    m_serifFamilies.clear();
    m_sansSeriferifFamilies.clear();
    m_cursiveFamilies.clear();
    m_fantasyFamilies.clear();
    m_monospaceFamilies.clear();
}

RetainPtr<CTFontRef> SystemFontDatabaseCoreText::createFontByApplyingWeightWidthItalicsAndFallbackBehavior(CTFontRef font, CGFloat weight, CGFloat width, bool italic, float size, AllowUserInstalledFonts allowUserInstalledFonts, CFStringRef design)
{
    auto weightNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &weight));
    auto widthNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &width));
    const float systemFontItalicSlope = 0.07;
    float italicsRawNumber = italic ? systemFontItalicSlope : 0;
    auto italicsNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &italicsRawNumber));
    CFTypeRef traitsKeys[] = { kCTFontWeightTrait, kCTFontWidthTrait, kCTFontSlantTrait, kCTFontUIFontDesignTrait };
    CFTypeRef traitsValues[] = { weightNumber.get(), widthNumber.get(), italicsNumber.get(), design ? static_cast<CFTypeRef>(design) : static_cast<CFTypeRef>(kCTFontUIFontDesignDefault) };
    auto traitsDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, traitsKeys, traitsValues, std::size(traitsKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
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
    auto fallbackDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, fallbackDictionaryKeys, fallbackDictionaryValues, std::size(fallbackDictionaryKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto modifiedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor, fallbackDictionary.get()));
    return modifiedFontDescriptor;
}

Vector<RetainPtr<CTFontDescriptorRef>> SystemFontDatabaseCoreText::computeCascadeList(CTFontRef font, CFStringRef locale)
{
    CFTypeRef arrayValues[] = { locale };
    auto localeArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, arrayValues, std::size(arrayValues), &kCFTypeArrayCallBacks));
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

NODELETE static CGFloat mapWeight(FontSelectionValue weight)
{
    if (weight < FontSelectionValue(150))
        return kCTFontWeightUltraLight;
    if (weight < FontSelectionValue(250))
        return kCTFontWeightThin;
    if (weight < FontSelectionValue(350))
        return kCTFontWeightLight;
    if (weight < FontSelectionValue(450))
        return kCTFontWeightRegular;
    if (weight < FontSelectionValue(550))
        return kCTFontWeightMedium;
    if (weight < FontSelectionValue(650))
        return kCTFontWeightSemibold;
    if (weight < FontSelectionValue(750))
        return kCTFontWeightBold;
    if (weight < FontSelectionValue(850))
        return kCTFontWeightHeavy;
    return kCTFontWeightBlack;
}

NODELETE static CGFloat mapWidth(FontSelectionValue width)
{
    struct PiecewisePoint {
        FontSelectionValue input;
        CGFloat output;
    };
    static const std::array piecewisePoints {
        PiecewisePoint { FontSelectionValue(37.5f), kCTFontWidthUltraCompressed },
        PiecewisePoint { FontSelectionValue(50), kCTFontWidthExtraCompressed }, // ultra condensed
        PiecewisePoint { FontSelectionValue(62.5f), kCTFontWidthExtraCondensed },
        PiecewisePoint { FontSelectionValue(75), kCTFontWidthCondensed },
        PiecewisePoint { FontSelectionValue(87.5f), kCTFontWidthSemiCondensed },
        PiecewisePoint { FontSelectionValue(100), kCTFontWidthStandard },
        PiecewisePoint { FontSelectionValue(112.5f), kCTFontWidthSemiExpanded },
        PiecewisePoint { FontSelectionValue(125), kCTFontWidthExpanded },
        PiecewisePoint { FontSelectionValue(150), kCTFontWidthExtraExpanded },
    };
    for (size_t i = 0; i < std::size(piecewisePoints) - 1; ++i) {
        auto& previous = piecewisePoints[i];
        auto& next = piecewisePoints[i + 1];
        auto middleInput = (previous.input + next.input) / 2;
        if (width < middleInput)
            return previous.output;
    }
    return piecewisePoints[std::size(piecewisePoints) - 1].output;
}

SystemFontDatabaseCoreText::CascadeListParameters SystemFontDatabaseCoreText::systemFontParameters(const FontDescription& description, AtomStringImpl* familyName, SystemFontKind systemFontKind, AllowUserInstalledFonts allowUserInstalledFonts)
{
    CascadeListParameters result;
    result.locale = description.usedLocale();
    result.size = description.usedSize();
    result.italic = isItalic(description.fontStyleSlope());
    result.allowUserInstalledFonts = allowUserInstalledFonts;

    result.weight = mapWeight(description.weight());
    result.width = mapWidth(description.width());

    switch (systemFontKind) {
    case SystemFontKind::SystemUI: {
        result.fontName = "system-ui"_atom;
        break;
    }
    case SystemFontKind::UISerif: {
        result.fontName = "ui-serif"_atom;
        break;
    }
    case SystemFontKind::UIMonospace: {
        result.fontName = "ui-monospace"_atom;
        break;
    }
    case SystemFontKind::UIRounded: {
        result.fontName = "ui-rounded"_atom;
        break;
    }
    case SystemFontKind::TextStyle:
        result.fontName = familyName;
        break;
    }

    return result;
}

std::optional<SystemFontKind> SystemFontDatabaseCoreText::matchSystemFontUse(AtomStringImpl* string)
{
    if (equalLettersIgnoringASCIICase(string, "-webkit-system-font"_s)
        || equalLettersIgnoringASCIICase(string, "-apple-system"_s)
        || equalLettersIgnoringASCIICase(string, "-apple-system-font"_s)
        || equalLettersIgnoringASCIICase(string, "system-ui"_s)
        || equalLettersIgnoringASCIICase(string, "ui-sans-serif"_s))
        return SystemFontKind::SystemUI;

    if (equalLettersIgnoringASCIICase(string, "ui-serif"_s))
        return SystemFontKind::UISerif;
    if (equalLettersIgnoringASCIICase(string, "ui-monospace"_s))
        return SystemFontKind::UIMonospace;
    if (equalLettersIgnoringASCIICase(string, "ui-rounded"_s))
        return SystemFontKind::UIRounded;

    auto compareAsPointer = [](AtomStringImpl* lhs, AtomStringImpl* rhs) {
        return lhs < rhs;
    };

    if (m_textStyles.isEmpty()) {
        m_textStyles = {
            AtomStringImpl::add(kCTUIFontTextStyleHeadline),
            AtomStringImpl::add(kCTUIFontTextStyleBody),
            AtomStringImpl::add(kCTUIFontTextStyleTitle1),
            AtomStringImpl::add(kCTUIFontTextStyleTitle2),
            AtomStringImpl::add(kCTUIFontTextStyleTitle3),
            AtomStringImpl::add(kCTUIFontTextStyleSubhead),
            AtomStringImpl::add(kCTUIFontTextStyleFootnote),
            AtomStringImpl::add(kCTUIFontTextStyleCaption1),
            AtomStringImpl::add(kCTUIFontTextStyleCaption2),
            AtomStringImpl::add(kCTUIFontTextStyleShortHeadline),
            AtomStringImpl::add(kCTUIFontTextStyleShortBody),
            AtomStringImpl::add(kCTUIFontTextStyleShortSubhead),
            AtomStringImpl::add(kCTUIFontTextStyleShortFootnote),
            AtomStringImpl::add(kCTUIFontTextStyleShortCaption1),
            AtomStringImpl::add(kCTUIFontTextStyleTallBody),
            AtomStringImpl::add(kCTUIFontTextStyleTitle0),
            AtomStringImpl::add(kCTUIFontTextStyleTitle4),
        };
        std::ranges::sort(m_textStyles, compareAsPointer);
    }

    if (std::ranges::binary_search(m_textStyles, string, compareAsPointer))
        return SystemFontKind::TextStyle;

    return std::nullopt;
}

Vector<RetainPtr<CTFontDescriptorRef>> SystemFontDatabaseCoreText::cascadeList(const FontDescription& description, AtomStringImpl* cssFamily, SystemFontKind systemFontKind, AllowUserInstalledFonts allowUserInstalledFonts)
{
    return cascadeList(systemFontParameters(description, cssFamily, systemFontKind, allowUserInstalledFonts), systemFontKind);
}

static RefPtr<StringImpl> genericFamily(StringImpl* locale, MemoryCompactRobinHoodHashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>& map, CFStringRef ctKey)
{
    return map.ensure(locale, [&] {
        auto descriptor = adoptCF(CTFontDescriptorCreateForCSSFamily(ctKey, createCFString(locale).get()));
        auto value = adoptCF(checked_cf_cast<CFStringRef>(CTFontDescriptorCopyAttribute(descriptor.get(), kCTFontFamilyNameAttribute)));
        return StringImpl::create(value.get());
    }).iterator->value;
}

RefPtr<StringImpl> SystemFontDatabaseCoreText::serifFamily(StringImpl* locale)
{
    return genericFamily(locale, m_serifFamilies, kCTFontCSSFamilySerif);
}

RefPtr<StringImpl> SystemFontDatabaseCoreText::sansSerifFamily(StringImpl* locale)
{
    return genericFamily(locale, m_sansSeriferifFamilies, kCTFontCSSFamilySansSerif);
}

RefPtr<StringImpl> SystemFontDatabaseCoreText::cursiveFamily(StringImpl* locale)
{
    return genericFamily(locale, m_cursiveFamilies, kCTFontCSSFamilyCursive);
}

RefPtr<StringImpl> SystemFontDatabaseCoreText::fantasyFamily(StringImpl* locale)
{
    return genericFamily(locale, m_fantasyFamilies, kCTFontCSSFamilyFantasy);
}

RefPtr<StringImpl> SystemFontDatabaseCoreText::monospaceFamily(StringImpl* locale)
{
    auto result = genericFamily(locale, m_monospaceFamilies, kCTFontCSSFamilyMonospace);
#if PLATFORM(MAC) && ENABLE(MONOSPACE_FONT_EXCEPTION)
    // In general, CoreText uses Monaco for monospaced (see: Terminal.app and Xcode.app).
    // For now, we want to use Courier for web compatibility, until we have more time to do compatibility testing.
    if (equalLettersIgnoringASCIICase(result, "monaco"_s))
        return "Courier"_str;
#elif PLATFORM(IOS_FAMILY) && ENABLE(MONOSPACE_FONT_EXCEPTION)
    if (equalLettersIgnoringASCIICase(result, "courier new"_s))
        return "Courier"_str;
#endif
    return result;
}

static inline FontSelectionValue cssWeightOfSystemFontDescriptor(CTFontDescriptorRef fontDescriptor)
{
    auto resultRef = adoptCF(static_cast<CFNumberRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontCSSWeightAttribute)));
    float result = 0;
    if (resultRef && CFNumberGetValue(resultRef.get(), kCFNumberFloatType, &result))
        return FontSelectionValue(result);

    auto traitsRef = adoptCF(static_cast<CFDictionaryRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontTraitsAttribute)));
    resultRef = static_cast<CFNumberRef>(CFDictionaryGetValue(traitsRef.get(), kCTFontWeightTrait));
    CFNumberGetValue(resultRef.get(), kCFNumberFloatType, &result);
    return FontSelectionValue(normalizeCTWeight(result));
}

NODELETE static CTFontTextStylePlatform fontPlatform()
{
#if PLATFORM(VISION)
    if (PAL::currentUserInterfaceIdiomIsVision())
        return kCTFontTextStylePlatformVision;
    return kCTFontTextStylePlatformPhone;
#else
    return kCTFontTextStylePlatformDefault;
#endif
}

auto SystemFontDatabase::platformSystemFontShorthandInfo(FontShorthand fontShorthand) -> SystemFontShorthandInfo
{
    auto interrogateFontDescriptorShorthandItem = [] (CTFontDescriptorRef fontDescriptor, StringImpl* family) {
        auto sizeNumber = adoptCF(static_cast<CFNumberRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontSizeAttribute)));
        float size = 0;
        CFNumberGetValue(sizeNumber.get(), kCFNumberFloatType, &size);
        auto weight = cssWeightOfSystemFontDescriptor(fontDescriptor);
        return SystemFontShorthandInfo { AtomStringImpl::add(family), size, FontSelectionValue(weight) };
    };

    auto interrogateTextStyleShorthandItem = [] (CFStringRef textStyle) {
        CGFloat weight = 0;
        float size = CTFontDescriptorGetTextStyleSize(textStyle, protect(contentSizeCategory()).get(), fontPlatform(), &weight, nullptr);
        auto cssWeight = normalizeCTWeight(weight);
        return SystemFontShorthandInfo { AtomStringImpl::add(textStyle), size, FontSelectionValue(cssWeight) };
    };

    switch (fontShorthand) {
    case FontShorthand::Caption:
    case FontShorthand::Icon:
    case FontShorthand::MessageBox:
        return interrogateFontDescriptorShorthandItem(adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, 0, nullptr)).get(), "system-ui"_si);
    case FontShorthand::Menu:
        return interrogateFontDescriptorShorthandItem(SystemFontDatabaseCoreText::menuFontDescriptor().get(), "-apple-menu"_si);
    case FontShorthand::SmallCaption:
        return interrogateFontDescriptorShorthandItem(SystemFontDatabaseCoreText::smallCaptionFontDescriptor().get(), "system-ui"_si);
    case FontShorthand::WebkitMiniControl:
        return interrogateFontDescriptorShorthandItem(SystemFontDatabaseCoreText::miniControlFontDescriptor().get(), "system-ui"_si);
    case FontShorthand::WebkitSmallControl:
        return interrogateFontDescriptorShorthandItem(SystemFontDatabaseCoreText::smallControlFontDescriptor().get(), "system-ui"_si);
    case FontShorthand::WebkitControl:
        return interrogateFontDescriptorShorthandItem(SystemFontDatabaseCoreText::controlFontDescriptor().get(), "system-ui"_si);
    case FontShorthand::AppleSystemHeadline:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleHeadline);
    case FontShorthand::AppleSystemBody:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleBody);
    case FontShorthand::AppleSystemSubheadline:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleSubhead);
    case FontShorthand::AppleSystemFootnote:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleFootnote);
    case FontShorthand::AppleSystemCaption1:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleCaption1);
    case FontShorthand::AppleSystemCaption2:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleCaption2);
    case FontShorthand::AppleSystemShortHeadline:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleShortHeadline);
    case FontShorthand::AppleSystemShortBody:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleShortBody);
    case FontShorthand::AppleSystemShortSubheadline:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleShortSubhead);
    case FontShorthand::AppleSystemShortFootnote:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleShortFootnote);
    case FontShorthand::AppleSystemShortCaption1:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleShortCaption1);
    case FontShorthand::AppleSystemTallBody:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleTallBody);
    case FontShorthand::AppleSystemTitle0:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleTitle0);
    case FontShorthand::AppleSystemTitle1:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleTitle1);
    case FontShorthand::AppleSystemTitle2:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleTitle2);
    case FontShorthand::AppleSystemTitle3:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleTitle3);
    case FontShorthand::AppleSystemTitle4:
        return interrogateTextStyleShorthandItem(kCTUIFontTextStyleTitle4);
    case FontShorthand::StatusBar:
        return interrogateFontDescriptorShorthandItem(SystemFontDatabaseCoreText::statusBarFontDescriptor().get(), "-apple-status-bar"_si);
    }
}

}
