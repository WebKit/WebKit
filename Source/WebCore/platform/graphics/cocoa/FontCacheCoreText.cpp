/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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
#include "FontCache.h"

#include "Color.h"
#include "Font.h"
#include "FontCascadeDescription.h"
#include "FontCreationContext.h"
#include "FontCustomPlatformData.h"
#include "FontDatabase.h"
#include "FontFamilySpecificationCoreText.h"
#include "FontInterrogation.h"
#include "FontMetricsNormalization.h"
#include "FontPaletteValues.h"
#include "Logging.h"
#include "StyleFontSizeFunctions.h"
#include "SystemFontDatabaseCoreText.h"
#include "UnrealizedCoreTextFont.h"
#include <CoreText/SFNTLayoutTypes.h>
#include <pal/spi/cf/CoreTextSPI.h>
#include <pal/spi/cocoa/AccessibilitySupportSPI.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace WebCore {

bool fontNameIsSystemFont(CFStringRef fontName)
{
    return CFStringGetLength(fontName) > 0 && CFStringGetCharacterAtIndex(fontName, 0) == '.';
}

static RetainPtr<CFArrayRef> variationAxesWithNonLocalizedAxesNames(CTFontDescriptorRef fontDescriptor)
{
    // Reading kCTFontVariationAxesAttribute returns non localized axes names
    return adoptCF(static_cast<CFArrayRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontVariationAxesAttribute)));
}

static RetainPtr<CFArrayRef> variationAxes(CTFontRef font, ShouldLocalizeAxisNames shouldLocalizeAxisNames)
{
    if (shouldLocalizeAxisNames == ShouldLocalizeAxisNames::Yes)
        return adoptCF(CTFontCopyVariationAxes(font));
    RetainPtr fontDescriptor = adoptCF(CTFontCopyFontDescriptor(font));
    return variationAxesWithNonLocalizedAxesNames(fontDescriptor.get());
}

VariationDefaultsMap defaultVariationValues(CTFontRef font, ShouldLocalizeAxisNames shouldLocalizeAxisNames)
{
    VariationDefaultsMap result;
    auto axes = variationAxes(font, shouldLocalizeAxisNames);
    if (!axes)
        return result;
    auto size = CFArrayGetCount(axes.get());
    for (CFIndex i = 0; i < size; ++i) {
        CFDictionaryRef axis = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(axes.get(), i));
        CFNumberRef axisIdentifier = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisIdentifierKey));
        String axisName = static_cast<CFStringRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisNameKey));
        CFNumberRef defaultValue = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisDefaultValueKey));
        CFNumberRef minimumValue = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisMinimumValueKey));
        CFNumberRef maximumValue = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisMaximumValueKey));
        uint32_t rawAxisIdentifier = 0;
        Boolean success = CFNumberGetValue(axisIdentifier, kCFNumberSInt32Type, &rawAxisIdentifier);
        ASSERT_UNUSED(success, success);
        float rawDefaultValue = 0;
        float rawMinimumValue = 0;
        float rawMaximumValue = 0;
        CFNumberGetValue(defaultValue, kCFNumberFloatType, &rawDefaultValue);
        CFNumberGetValue(minimumValue, kCFNumberFloatType, &rawMinimumValue);
        CFNumberGetValue(maximumValue, kCFNumberFloatType, &rawMaximumValue);

        if (rawMinimumValue > rawMaximumValue)
            std::swap(rawMinimumValue, rawMaximumValue);

        char b1 = rawAxisIdentifier >> 24;
        char b2 = (rawAxisIdentifier & 0xFF0000) >> 16;
        char b3 = (rawAxisIdentifier & 0xFF00) >> 8;
        char b4 = rawAxisIdentifier & 0xFF;
        FontTag resultKey = { { b1, b2, b3, b4 } };
        VariationDefaults resultValues = { axisName, rawDefaultValue, rawMinimumValue, rawMaximumValue };
        result.set(resultKey, resultValues);
    }
    return result;
}

static std::optional<bool>& overrideEnhanceTextLegibility()
{
    static NeverDestroyed<std::optional<bool>> overrideEnhanceTextLegibility;
    return overrideEnhanceTextLegibility.get();
}

void setOverrideEnhanceTextLegibility(bool override)
{
    overrideEnhanceTextLegibility() = override;
}

static bool& platformShouldEnhanceTextLegibility()
{
    static NeverDestroyed<bool> shouldEnhanceTextLegibility = _AXSEnhanceTextLegibilityEnabled();
    return shouldEnhanceTextLegibility.get();
}

static inline bool shouldEnhanceTextLegibility()
{
    return overrideEnhanceTextLegibility().value_or(platformShouldEnhanceTextLegibility());
}

RetainPtr<CTFontRef> preparePlatformFont(UnrealizedCoreTextFont&& originalFont, const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, FontTypeForPreparation fontTypeForPreparation, ApplyTraitsVariations applyTraitsVariations)
{
    originalFont.modifyFromContext(fontDescription, fontCreationContext, fontTypeForPreparation, applyTraitsVariations, shouldEnhanceTextLegibility());
    return originalFont.realize();
}

RefPtr<Font> FontCache::similarFont(const FontDescription& description, const String& family)
{
    // Attempt to find an appropriate font using a match based on the presence of keywords in
    // the requested names. For example, we'll match any name that contains "Arabic" to Geeza Pro.
    if (family.isEmpty())
        return nullptr;

#if PLATFORM(IOS_FAMILY)
    // Substitute the default monospace font for well-known monospace fonts.
    if (equalLettersIgnoringASCIICase(family, "monaco"_s) || equalLettersIgnoringASCIICase(family, "menlo"_s))
        return fontForFamily(description, "courier"_s);

    // Substitute Verdana for Lucida Grande.
    if (equalLettersIgnoringASCIICase(family, "lucida grande"_s))
        return fontForFamily(description, "verdana"_s);
#endif

    static constexpr ASCIILiteral matchWords[] = { "Arabic"_s, "Pashto"_s, "Urdu"_s };
    auto familyMatcher = StringView(family);
    for (auto matchWord : matchWords) {
        if (equalIgnoringASCIICase(familyMatcher, matchWord))
            return fontForFamily(description, isFontWeightBold(description.weight()) ? "GeezaPro-Bold"_s : "GeezaPro"_s);
    }
    return nullptr;
}

static void fontCacheRegisteredFontsChangedNotificationCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void *, CFDictionaryRef)
{
    ASSERT_UNUSED(observer, isMainThread() && observer == FontCache::forCurrentThread().ptr());

    ensureOnMainThread([] {
        FontCache::invalidateAllFontCaches();
    });
}

void FontCache::platformInit()
{
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, &fontCacheRegisteredFontsChangedNotificationCallback, kCTFontManagerRegisteredFontsChangedNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);

#if PLATFORM(IOS_FAMILY)
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, &fontCacheRegisteredFontsChangedNotificationCallback, getUIContentSizeCategoryDidChangeNotificationName(), nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
#endif

    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, &fontCacheRegisteredFontsChangedNotificationCallback, kAXSEnhanceTextLegibilityChangedNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);

#if PLATFORM(MAC)
    CFNotificationCenterRef center = CFNotificationCenterGetLocalCenter();
    const CFStringRef notificationName = kCFLocaleCurrentLocaleDidChangeNotification;
#else
    CFNotificationCenterRef center = CFNotificationCenterGetDarwinNotifyCenter();
    const CFStringRef notificationName = CFSTR("com.apple.language.changed");
#endif
    CFNotificationCenterAddObserver(center, this, &fontCacheRegisteredFontsChangedNotificationCallback, notificationName, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
}

Vector<String> FontCache::systemFontFamilies()
{
    Vector<String> fontFamilies;

    auto availableFontFamilies = adoptCF(CTFontManagerCopyAvailableFontFamilyNames());
    CFIndex count = CFArrayGetCount(availableFontFamilies.get());
    for (CFIndex i = 0; i < count; ++i) {
        auto fontName = dynamic_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(availableFontFamilies.get(), i));
        if (!fontName) {
            ASSERT_NOT_REACHED();
            continue;
        }

        if (fontNameIsSystemFont(fontName))
            continue;

        fontFamilies.append(fontName);
    }

    return fontFamilies;
}

static inline bool isSystemFont(const String& family)
{
    // String's operator[] handles out-of-bounds by returning 0.
    return family[0] == '.';
}

bool FontCache::isSystemFontForbiddenForEditing(const String& fontFamily)
{
    return isSystemFont(fontFamily);
}

static CTFontSymbolicTraits computeTraits(const FontDescription& fontDescription)
{
    CTFontSymbolicTraits traits = 0;
    if (fontDescription.italic())
        traits |= kCTFontTraitItalic;
    if (isFontWeightBold(fontDescription.weight()))
        traits |= kCTFontTraitBold;
    return traits;
}

SynthesisPair computeNecessarySynthesis(CTFontRef font, const FontDescription& fontDescription, OptionSet<FontLookupOptions> synthesisOptions, ShouldComputePhysicalTraits shouldComputePhysicalTraits, bool isPlatformFont)
{
    if (CTFontIsAppleColorEmoji(font))
        return SynthesisPair(false, false);

    if (isPlatformFont)
        return SynthesisPair(false, false);

    bool needsSyntheticBold = fontDescription.hasAutoFontSynthesisWeight()
        && !synthesisOptions.contains(FontLookupOptions::DisallowBoldSynthesis);
    bool needsSyntheticOblique = fontDescription.hasAutoFontSynthesisStyle()
        && !synthesisOptions.contains(FontLookupOptions::DisallowObliqueSynthesis);

    if (!needsSyntheticBold && !needsSyntheticOblique)
        return SynthesisPair(false, false);

    CTFontSymbolicTraits desiredTraits = computeTraits(fontDescription);
    CTFontSymbolicTraits actualTraits = 0;
    if (isFontWeightBold(fontDescription.weight()) || isItalic(fontDescription.italic())) {
        if (shouldComputePhysicalTraits == ShouldComputePhysicalTraits::Yes)
            actualTraits = CTFontGetPhysicalSymbolicTraits(font);
        else
            actualTraits = CTFontGetSymbolicTraits(font);
    }

    needsSyntheticBold = needsSyntheticBold && (desiredTraits & kCTFontTraitBold) && !(actualTraits & kCTFontTraitBold);
    needsSyntheticOblique = needsSyntheticOblique && (desiredTraits & kCTFontTraitItalic) && !(actualTraits & kCTFontTraitItalic);

    return SynthesisPair(needsSyntheticBold, needsSyntheticOblique);
}

class FontCacheAllowlist {
public:
    static FontCacheAllowlist& singleton() WTF_REQUIRES_LOCK(lock)
    {
        static NeverDestroyed<FontCacheAllowlist> allowlist;
        return allowlist;
    }

    void set(const Vector<String>& inputAllowlist) WTF_REQUIRES_LOCK(lock)
    {
        m_families.clear();
        for (auto& item : inputAllowlist)
            m_families.add(item);
    }

    bool allows(const AtomString& family) const WTF_REQUIRES_LOCK(lock)
    {
        return m_families.isEmpty() || m_families.contains(family);
    }

    static Lock lock;

private:
    HashSet<String, ASCIICaseInsensitiveHash> m_families;
};

Lock FontCacheAllowlist::lock;

void FontCache::setFontAllowlist(const Vector<String>& inputAllowlist)
{
    Locker locker { FontCacheAllowlist::lock };
    FontCacheAllowlist::singleton().set(inputAllowlist);
}

// Because this struct holds intermediate values which may be in the compressed -1 - 1 GX range, we don't want to use the relatively large
// quantization of FontSelectionValue. Instead, do this logic with floats.
struct MinMax {
    float minimum;
    float maximum;
};

struct VariationCapabilities {
    std::optional<MinMax> weight;
    std::optional<MinMax> width;
    std::optional<MinMax> slope;
};

static std::optional<MinMax> extractVariationBounds(CFDictionaryRef axis)
{
    CFNumberRef minimumValue = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisMinimumValueKey));
    CFNumberRef maximumValue = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisMaximumValueKey));
    float rawMinimumValue = 0;
    float rawMaximumValue = 0;
    CFNumberGetValue(minimumValue, kCFNumberFloatType, &rawMinimumValue);
    CFNumberGetValue(maximumValue, kCFNumberFloatType, &rawMaximumValue);
    if (rawMinimumValue < rawMaximumValue)
        return {{ rawMinimumValue, rawMaximumValue }};
    return std::nullopt;
}

static VariationCapabilities variationCapabilitiesForFontDescriptor(CTFontDescriptorRef fontDescriptor)
{
    VariationCapabilities result;

    if (!adoptCF(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontVariationAttribute)))
        return result;

    auto variations = variationAxesWithNonLocalizedAxesNames(fontDescriptor);
    if (!variations)
        return result;

    auto axisCount = CFArrayGetCount(variations.get());
    if (!axisCount)
        return result;

    for (CFIndex i = 0; i < axisCount; ++i) {
        CFDictionaryRef axis = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(variations.get(), i));
        CFNumberRef axisIdentifier = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisIdentifierKey));
        uint32_t rawAxisIdentifier = 0;
        Boolean success = CFNumberGetValue(axisIdentifier, kCFNumberSInt32Type, &rawAxisIdentifier);
        ASSERT_UNUSED(success, success);
        if (rawAxisIdentifier == 0x77676874) // 'wght'
            result.weight = extractVariationBounds(axis);
        else if (rawAxisIdentifier == 0x77647468) // 'wdth'
            result.width = extractVariationBounds(axis);
        else if (rawAxisIdentifier == 0x736C6E74) // 'slnt'
            result.slope = extractVariationBounds(axis);
    }

    bool optOutFromGXNormalization = CTFontDescriptorIsSystemUIFont(fontDescriptor);

    auto variationType = [&] {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=247987 Stop creating a whole CTFont here. Ideally we'd be able to do all the inspection we need to do without one.
        auto font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor, 0, nullptr));
        return FontInterrogation(font.get()).variationType;
    }();
    if (variationType == FontInterrogation::VariationType::TrueTypeGX && !optOutFromGXNormalization) {
        if (result.weight)
            result.weight = { { normalizeGXWeight(result.weight.value().minimum), normalizeGXWeight(result.weight.value().maximum) } };
        if (result.width)
            result.width = { { normalizeVariationWidth(result.width.value().minimum), normalizeVariationWidth(result.width.value().maximum) } };
        if (result.slope)
            result.slope = { { normalizeSlope(result.slope.value().minimum), normalizeSlope(result.slope.value().maximum) } };
    }

    auto minimum = static_cast<float>(FontSelectionValue::minimumValue());
    auto maximum = static_cast<float>(FontSelectionValue::maximumValue());
    if (result.weight && (result.weight.value().minimum < minimum || result.weight.value().maximum > maximum))
        result.weight = { };
    if (result.width && (result.width.value().minimum < minimum || result.width.value().maximum > maximum))
        result.width = { };
    if (result.slope && (result.slope.value().minimum < minimum || result.slope.value().maximum > maximum))
        result.slope = { };

    return result;
}

static float getCSSAttribute(CTFontDescriptorRef fontDescriptor, const CFStringRef attribute, float fallback)
{
    auto number = adoptCF(static_cast<CFNumberRef>(CTFontDescriptorCopyAttribute(fontDescriptor, attribute)));
    if (!number)
        return fallback;
    float cssValue;
    auto success = CFNumberGetValue(number.get(), kCFNumberFloatType, &cssValue);
    ASSERT_UNUSED(success, success);
    return cssValue;
}

FontSelectionCapabilities capabilitiesForFontDescriptor(CTFontDescriptorRef fontDescriptor)
{
    if (!fontDescriptor)
        return { };

    VariationCapabilities variationCapabilities = variationCapabilitiesForFontDescriptor(fontDescriptor);

    if (!variationCapabilities.slope) {
        auto traits = adoptCF(static_cast<CFDictionaryRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontTraitsAttribute)));
        if (traits) {
            if (!variationCapabilities.slope) {
                auto symbolicTraitsNumber = static_cast<CFNumberRef>(CFDictionaryGetValue(traits.get(), kCTFontSymbolicTrait));
                if (symbolicTraitsNumber) {
                    int32_t symbolicTraits;
                    auto success = CFNumberGetValue(symbolicTraitsNumber, kCFNumberSInt32Type, &symbolicTraits);
                    ASSERT_UNUSED(success, success);
                    auto slopeValue = static_cast<float>(symbolicTraits & kCTFontTraitItalic ? italicValue() : normalItalicValue());
                    variationCapabilities.slope = {{ slopeValue, slopeValue }};
                } else
                    variationCapabilities.slope = {{ static_cast<float>(normalItalicValue()), static_cast<float>(normalItalicValue()) }};
            }
        }
    }

    if (!variationCapabilities.weight) {
        auto value = getCSSAttribute(fontDescriptor, kCTFontCSSWeightAttribute, static_cast<float>(normalWeightValue()));
        variationCapabilities.weight = {{ value, value }};
    }

    if (!variationCapabilities.width) {
        auto value = getCSSAttribute(fontDescriptor, kCTFontCSSWidthAttribute, static_cast<float>(normalWidthValue()));
        variationCapabilities.width = {{ value, value }};
    }

    FontSelectionCapabilities result = {{ FontSelectionValue(variationCapabilities.weight.value().minimum), FontSelectionValue(variationCapabilities.weight.value().maximum) },
        { FontSelectionValue(variationCapabilities.width.value().minimum), FontSelectionValue(variationCapabilities.width.value().maximum) },
        { FontSelectionValue(variationCapabilities.slope.value().minimum), FontSelectionValue(variationCapabilities.slope.value().maximum) }};
    ASSERT(result.weight.isValid());
    ASSERT(result.width.isValid());
    ASSERT(result.slope.isValid());
    return result;
}

static const FontDatabase::InstalledFont* findClosestFont(const FontDatabase::InstalledFontFamily& familyFonts, FontSelectionRequest fontSelectionRequest)
{
    auto capabilities = familyFonts.installedFonts.map([](auto& font) {
        return font.capabilities;
    });
    FontSelectionAlgorithm fontSelectionAlgorithm(fontSelectionRequest, WTFMove(capabilities), familyFonts.capabilities);
    auto index = fontSelectionAlgorithm.indexOfBestCapabilities();
    if (index == notFound)
        return nullptr;

    return &familyFonts.installedFonts[index];
}

FontDatabase& FontCache::database(AllowUserInstalledFonts allowUserInstalledFonts)
{
    return allowUserInstalledFonts == AllowUserInstalledFonts::Yes ? m_databaseAllowingUserInstalledFonts : m_databaseDisallowingUserInstalledFonts;
}

Vector<FontSelectionCapabilities> FontCache::getFontSelectionCapabilitiesInFamily(const AtomString& familyName, AllowUserInstalledFonts allowUserInstalledFonts)
{
    auto& fontDatabase = database(allowUserInstalledFonts);
    const auto& fonts = fontDatabase.collectionForFamily(familyName.string());
    return fonts.installedFonts.map([](auto& font) {
        return font.capabilities;
    });
}

struct FontLookup {
    RetainPtr<CTFontDescriptorRef> result;
    bool createdFromPostScriptName { false };
};

static bool isDotPrefixedForbiddenFont(const AtomString& family)
{
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ForbidsDotPrefixedFonts))
        return family.startsWith('.');
    return equalLettersIgnoringASCIICase(family, ".applesystemuifontserif"_s)
        || equalLettersIgnoringASCIICase(family, ".sf ns mono"_s)
        || equalLettersIgnoringASCIICase(family, ".sf ui mono"_s)
        || equalLettersIgnoringASCIICase(family, ".sf arabic"_s)
        || equalLettersIgnoringASCIICase(family, ".applesystemuifontrounded"_s);
}

static bool isAllowlistedFamily(const AtomString& family)
{
    if (isSystemFont(family.string()))
        return true;

    Locker locker { FontCacheAllowlist::lock };
    return FontCacheAllowlist::singleton().allows(family);
}

static FontLookup platformFontLookupWithFamily(FontDatabase& fontDatabase, const AtomString& family, FontSelectionRequest request, OptionSet<FontLookupOptions> options)
{
    if (!isAllowlistedFamily(family))
        return { nullptr };

    if (isDotPrefixedForbiddenFont(family)) {
        // If you want to use these fonts, use system-ui, ui-serif, ui-monospace, or ui-rounded.
        return { nullptr };
    }

    const auto& familyFonts = fontDatabase.collectionForFamily(family.string());
    if (familyFonts.isEmpty()) {
        // The CSS spec states that font-family only accepts a name of an actual font family. However, in WebKit, we claim to also
        // support supplying a PostScript name instead. However, this creates problems when the other properties (font-weight,
        // font-style) disagree with the traits of the PostScript-named font. The solution we have come up with is, when the default
        // values for font-weight and font-style are supplied, honor the PostScript name, but if font-weight specifies bold or
        // font-style specifies italic, then we run the regular matching algorithm on the family of the PostScript font. This way,
        // if content simply states "font-family: PostScriptName;" without specifying the other font properties, it will be honored,
        // but if a <b> appears as a descendent element, it will be honored too.
        const auto& postScriptFont = fontDatabase.fontForPostScriptName(family);
        if (!postScriptFont.fontDescriptor)
            return { nullptr };
        if (!options.contains(FontLookupOptions::ExactFamilyNameMatch)
            && ((isItalic(request.slope) && !isItalic(postScriptFont.capabilities.slope.maximum))
            || (isFontWeightBold(request.weight) && !isFontWeightBold(postScriptFont.capabilities.weight.maximum)))) {
            auto postScriptFamilyName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(postScriptFont.fontDescriptor.get(), kCTFontFamilyNameAttribute)));
            if (!postScriptFamilyName)
                return { nullptr };
            const auto& familyFonts = fontDatabase.collectionForFamily(String(postScriptFamilyName.get()));
            if (familyFonts.isEmpty())
                return { nullptr };
            if (const auto* installedFont = findClosestFont(familyFonts, request)) {
                if (!installedFont->fontDescriptor)
                    return { nullptr };
                return { installedFont->fontDescriptor.get(), true };
            }
            return { nullptr };
        }
        return { postScriptFont.fontDescriptor.get(), true };
    }

    if (const auto* installedFont = findClosestFont(familyFonts, request))
        return { installedFont->fontDescriptor.get(), false };

    return { nullptr };
}

void FontCache::platformInvalidate()
{
    // FIXME: Workers need to access SystemFontDatabaseCoreText.
    m_fontFamilySpecificationCoreTextCache.clear();
    m_systemFontDatabaseCoreText.clear();

    platformShouldEnhanceTextLegibility() = _AXSEnhanceTextLegibilityEnabled();
}

struct SpecialCaseFontLookupResult {
    UnrealizedCoreTextFont unrealizedCoreTextFont;
    FontTypeForPreparation fontTypeForPreparation;
};

static std::optional<SpecialCaseFontLookupResult> fontDescriptorWithFamilySpecialCase(const AtomString& family, const FontDescription& fontDescription, float size, AllowUserInstalledFonts allowUserInstalledFonts)
{
    // FIXME: See comment in FontCascadeDescription::effectiveFamilyAt() in FontDescriptionCocoa.cpp
    std::optional<SystemFontKind> systemDesign;

    if (equalLettersIgnoringASCIICase(family, "ui-serif"_s))
        systemDesign = SystemFontKind::UISerif;
    else if (equalLettersIgnoringASCIICase(family, "ui-monospace"_s))
        systemDesign = SystemFontKind::UIMonospace;
    else if (equalLettersIgnoringASCIICase(family, "ui-rounded"_s))
        systemDesign = SystemFontKind::UIRounded;

    if (equalLettersIgnoringASCIICase(family, "-webkit-system-font"_s) || equalLettersIgnoringASCIICase(family, "-apple-system"_s) || equalLettersIgnoringASCIICase(family, "-apple-system-font"_s) || equalLettersIgnoringASCIICase(family, "system-ui"_s) || equalLettersIgnoringASCIICase(family, "ui-sans-serif"_s)) {
        ASSERT(!systemDesign);
        systemDesign = SystemFontKind::SystemUI;
    }

    if (systemDesign) {
        auto cascadeList = SystemFontDatabaseCoreText::forCurrentThread().cascadeList(fontDescription, family, *systemDesign, allowUserInstalledFonts);
        if (cascadeList.isEmpty())
            return std::nullopt;
        return { { RetainPtr { cascadeList[0] }, FontTypeForPreparation::SystemFont } };
    }

    if (family.startsWith("UICTFontTextStyle"_s)) {
        const auto& request = fontDescription.fontSelectionRequest();
        CTFontSymbolicTraits traits = (isFontWeightBold(request.weight) ? kCTFontTraitBold : 0) | (isItalic(request.slope) ? kCTFontTraitItalic : 0);
        auto descriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(family.string().createCFString().get(), contentSizeCategory(), fontDescription.computedLocale().string().createCFString().get()));
        if (traits) {
            // FIXME: rdar://105369379 As far as I can tell, there's no modification to the attributes dictionary that has the same effect as CTFontDescriptorCreateCopyWithSymbolicTraits(),
            // because there doesn't seem to be a place to specify the bitmask. That's the reason we're creating the derived CTFontDescriptor here, rather than in UnrealizedCoreTextFont::realize().
            return { { adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(descriptor.get(), traits, traits)), FontTypeForPreparation::SystemFont } };
        }
        return { { WTFMove(descriptor), FontTypeForPreparation::SystemFont } };
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-menu"_s))
        return { { adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontMenuItem, size, fontDescription.computedLocale().string().createCFString().get())), FontTypeForPreparation::SystemFont } };

    if (equalLettersIgnoringASCIICase(family, "-apple-status-bar"_s))
        return { { adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, size, fontDescription.computedLocale().string().createCFString().get())), FontTypeForPreparation::SystemFont } };

    if (equalLettersIgnoringASCIICase(family, "lastresort"_s))
        return { { adoptCF(CTFontDescriptorCreateLastResort()), FontTypeForPreparation::NonSystemFont } };

    if (equalLettersIgnoringASCIICase(family, "-apple-system-monospaced-numbers"_s)) {
        auto systemFontDescriptor = UnrealizedCoreTextFont { adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, size, nullptr)) };
        systemFontDescriptor.modify([](CFMutableDictionaryRef attributes) {
            int numberSpacingType = kNumberSpacingType;
            int monospacedNumbersSelector = kMonospacedNumbersSelector;
            auto numberSpacingNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &numberSpacingType));
            auto monospacedNumbersNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &monospacedNumbersSelector));
            CFTypeRef keys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
            CFTypeRef values[] = { numberSpacingNumber.get(), monospacedNumbersNumber.get() };
            ASSERT(std::size(keys) == std::size(values));
            auto settingsDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            CFTypeRef entries[] = { settingsDictionary.get() };
            auto settingsArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, entries, std::size(entries), &kCFTypeArrayCallBacks));
            CFDictionaryAddValue(attributes, kCTFontFeatureSettingsAttribute, settingsArray.get());
        });
        return { { systemFontDescriptor, FontTypeForPreparation::SystemFont } };
    }

    return std::nullopt;
}

static RetainPtr<CTFontRef> fontWithFamily(FontDatabase& fontDatabase, const AtomString& family, const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, float size, OptionSet<FontLookupOptions> options)
{
    ASSERT(fontDatabase.allowUserInstalledFonts() == fontDescription.shouldAllowUserInstalledFonts());

    if (family.isEmpty())
        return nullptr;

    if (auto lookupResult = fontDescriptorWithFamilySpecialCase(family, fontDescription, size, fontDescription.shouldAllowUserInstalledFonts())) {
        lookupResult->unrealizedCoreTextFont.setSize(size);
        lookupResult->unrealizedCoreTextFont.modify([&](CFMutableDictionaryRef attributes) {
            addAttributesForInstalledFonts(attributes, fontDescription.shouldAllowUserInstalledFonts());
        });
        return preparePlatformFont(WTFMove(lookupResult->unrealizedCoreTextFont), fontDescription, fontCreationContext, lookupResult->fontTypeForPreparation);
    }
    auto fontLookup = platformFontLookupWithFamily(fontDatabase, family, fontDescription.fontSelectionRequest(), options);
    UnrealizedCoreTextFont unrealizedFont = { WTFMove(fontLookup.result) };
    unrealizedFont.setSize(size);
    ApplyTraitsVariations applyTraitsVariations = fontLookup.createdFromPostScriptName ? ApplyTraitsVariations::No : ApplyTraitsVariations::Yes;
    return preparePlatformFont(WTFMove(unrealizedFont), fontDescription, fontCreationContext, FontTypeForPreparation::NonSystemFont, applyTraitsVariations);
}

#if PLATFORM(MAC)
bool FontCache::shouldAutoActivateFontIfNeeded(const AtomString& family)
{
    if (family.isEmpty())
        return false;

    static const unsigned maxCacheSize = 128;
    ASSERT(m_knownFamilies.size() <= maxCacheSize);
    if (m_knownFamilies.size() == maxCacheSize)
        m_knownFamilies.remove(m_knownFamilies.random());

    // Only attempt to auto-activate fonts once for performance reasons.
    return m_knownFamilies.add(family).isNewEntry;
}

static void autoActivateFont(const String& name, CGFloat size)
{
    auto fontName = name.createCFString();
    CFTypeRef keys[] = { kCTFontNameAttribute, kCTFontEnabledAttribute };
    CFTypeRef values[] = { fontName.get(), kCFBooleanTrue };
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    auto newFont = adoptCF(CTFontCreateWithFontDescriptor(descriptor.get(), size, nullptr));
}
#endif

static void registerFontIfNeeded(const String& family) WTF_REQUIRES_LOCK(userInstalledFontMapLock())
{
    if (auto fontURL = userInstalledFontMap().getOptional(family)) {
        RELEASE_LOG_FORWARDABLE(Fonts, FONTCACHECORETEXT_REGISTER_FONT, family.utf8().data(), fontURL->string().utf8().data());
        RetainPtr cfURL = fontURL->createCFURL();

        CFErrorRef error = nullptr;
        if (!CTFontManagerRegisterFontsForURL(cfURL.get(), kCTFontManagerScopeProcess, &error)) {
            RetainPtr descriptionCF = adoptCF(CFErrorCopyDescription(error));
            String error(descriptionCF.get());
            RELEASE_LOG_FORWARDABLE(Fonts, FONTCACHECORETEXT_REGISTER_ERROR, family.utf8().data(), error.utf8().data());
        }

        userInstalledFontMap().removeIf([&](auto& keyAndValue) {
            return keyAndValue.value == fontURL;
        });
    }
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomString& family, const FontCreationContext& fontCreationContext, OptionSet<FontLookupOptions> options)
{
    {
        Locker locker(userInstalledFontMapLock());
        if (!userInstalledFontMap().isEmpty()) {
            auto fontFamily = family.string().convertToASCIILowercase();
            registerFontIfNeeded(fontFamily);
            auto fontNames = userInstalledFontFamilyMap().find(fontFamily);
            if (fontNames != userInstalledFontFamilyMap().end()) {
                for (auto& fontName : fontNames->value)
                    registerFontIfNeeded(fontName);
                userInstalledFontFamilyMap().remove(fontNames);
            }
        }
    }

    auto size = fontDescription.adjustedSizeForFontFace(fontCreationContext.sizeAdjust());
    auto& fontDatabase = database(fontDescription.shouldAllowUserInstalledFonts());
    auto font = fontWithFamily(fontDatabase, family, fontDescription, fontCreationContext, size, options);

#if PLATFORM(MAC)
    if (!font) {
        if (!shouldAutoActivateFontIfNeeded(family))
            return nullptr;

        // Auto activate the font before looking for it a second time.
        // Ignore the result because we want to use our own algorithm to actually find the font.
        autoActivateFont(family.string(), size);

        font = fontWithFamily(fontDatabase, family, fontDescription, fontCreationContext, size, options);
    }
#endif

    if (!font)
        return nullptr;

    if (fontDescription.shouldAllowUserInstalledFonts() == AllowUserInstalledFonts::No)
        m_seenFamiliesForPrewarming.add(FontCascadeDescription::foldedFamilyName(family));

    auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(font.get(), fontDescription, options).boldObliquePair();

    FontPlatformData platformData(font.get(), size, syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());

    platformData.updateSizeWithFontSizeAdjust(fontDescription.fontSizeAdjust(), fontDescription.computedSize());
    return makeUnique<FontPlatformData>(platformData);
}

void FontCache::platformPurgeInactiveFontData()
{
    Vector<CTFontRef> toRemove;
    for (auto& font : m_fallbackFonts) {
        if (CFGetRetainCount(font.get()) == 1)
            toRemove.append(font.get());
    }
    for (auto& font : toRemove)
        m_fallbackFonts.remove(font);

    m_databaseAllowingUserInstalledFonts.clear();
    m_databaseDisallowingUserInstalledFonts.clear();
}

#if PLATFORM(IOS_FAMILY)
static inline bool isArabicCharacter(char16_t character)
{
    return character >= 0x0600 && character <= 0x06FF;
}
#endif

#if ASSERT_ENABLED
static bool isUserInstalledFont(CTFontRef font)
{
    return adoptCF(CTFontCopyAttribute(font, kCTFontUserInstalledAttribute)) == kCFBooleanTrue;
}
#endif

static RetainPtr<CTFontRef> lookupFallbackFont(CTFontRef font, FontSelectionValue fontWeight, const AtomString& locale, AllowUserInstalledFonts allowUserInstalledFonts, StringView characterCluster)
{
    ASSERT(characterCluster.length() > 0);

    RetainPtr<CFStringRef> localeString;
    if (!locale.isNull())
        localeString = locale.string().createCFString();

    CFIndex coveredLength = 0;
    auto upconvertedCharacters = characterCluster.upconvertedCharacters();
    auto fallbackOption = allowUserInstalledFonts == AllowUserInstalledFonts::No ? kCTFontFallbackOptionSystem : kCTFontFallbackOptionDefault;
    auto result = adoptCF(CTFontCreateForCharactersWithLanguageAndOption(font, reinterpret_cast<const UTF16Char*>(upconvertedCharacters.get()), characterCluster.length(), localeString.get(), fallbackOption, &coveredLength));
    ASSERT(!isUserInstalledFont(result.get()) || allowUserInstalledFonts == AllowUserInstalledFonts::Yes);

#if PLATFORM(IOS_FAMILY)
    // FIXME: This is so unfortunate. The reason this is here is that certain fonts which are early in the system font cascade list
    // (used to?) perform poorly. In order to speed up the browser, we block those fonts, and use other faster fonts instead.
    // However, this performance analysis was done, like, 10 years ago, and the probability that these fonts are still too slow
    // seems quite low. We should re-analyze performance to see if we can delete this code.
    char16_t firstCharacter = characterCluster[0];
    if (isArabicCharacter(firstCharacter)) {
        auto familyName = adoptCF(static_cast<CFStringRef>(CTFontCopyAttribute(result.get(), kCTFontFamilyNameAttribute)));
        if (fontFamilyShouldNotBeUsedForArabic(familyName.get())) {
            CFStringRef newFamilyName = isFontWeightBold(fontWeight) ? CFSTR("GeezaPro-Bold") : CFSTR("GeezaPro");
            CFTypeRef keys[] = { kCTFontNameAttribute };
            CFTypeRef values[] = { newFamilyName };
            auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
            result = adoptCF(CTFontCreateCopyWithAttributes(result.get(), CTFontGetSize(result.get()), nullptr, modification.get()));
        }
    }
#else
    UNUSED_PARAM(fontWeight);
#endif

    return result;
}

RefPtr<Font> FontCache::systemFallbackForCharacterCluster(const FontDescription& description, const Font& originalFontData, IsForPlatformFont isForPlatformFont, PreferColoredFont, StringView characterCluster)
{
    const FontPlatformData& platformData = originalFontData.platformData();

    auto fullName = String(adoptCF(CTFontCopyFullName(platformData.ctFont())).get());
    if (!fullName.isEmpty())
        m_fontNamesRequiringSystemFallbackForPrewarming.add(fullName);

    auto result = lookupFallbackFont(platformData.ctFont(), description.weight(), description.computedLocale(), description.shouldAllowUserInstalledFonts(), characterCluster);
    result = preparePlatformFont(UnrealizedCoreTextFont { WTFMove(result) }, description, { });

    if (!result)
        return lastResortFallbackFont(description);

    // FontCascade::drawGlyphBuffer() requires that there are no duplicate Font objects which refer to the same thing. This is enforced in
    // FontCache::fontForPlatformData(), where our equality check is based on hashing the FontPlatformData, whose hash includes the raw CoreText
    // font pointer.
    CTFontRef substituteFont = m_fallbackFonts.add(result).iterator->get();

    auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(substituteFont, description, { }, ShouldComputePhysicalTraits::No, isForPlatformFont == IsForPlatformFont::Yes).boldObliquePair();

    RefPtr<const FontCustomPlatformData> customPlatformData = nullptr;
    if (safeCFEqual(platformData.ctFont(), substituteFont))
        customPlatformData = platformData.customPlatformData();
    FontPlatformData alternateFont(substituteFont, platformData.size(), syntheticBold, syntheticOblique, platformData.orientation(), platformData.widthVariant(), platformData.textRenderingMode(), customPlatformData.get());

    return fontForPlatformData(alternateFont);
}

ASCIILiteral FontCache::platformAlternateFamilyName(const String& familyName)
{
    static const char16_t heitiString[] = { 0x9ed1, 0x4f53 };
    static const char16_t songtiString[] = { 0x5b8b, 0x4f53 };
    static const char16_t weiruanXinXiMingTi[] = { 0x5fae, 0x8edf, 0x65b0, 0x7d30, 0x660e, 0x9ad4 };
    static const char16_t weiruanYaHeiString[] = { 0x5fae, 0x8f6f, 0x96c5, 0x9ed1 };
    static const char16_t weiruanZhengHeitiString[] = { 0x5fae, 0x8edf, 0x6b63, 0x9ed1, 0x9ad4 };

    static constexpr ASCIILiteral songtiSC = "Songti SC"_s;
    static constexpr ASCIILiteral songtiTC = "Songti TC"_s;
    static constexpr ASCIILiteral heitiSCReplacement = "PingFang SC"_s;
    static constexpr ASCIILiteral heitiTCReplacement = "PingFang TC"_s;

    switch (familyName.length()) {
    case 2:
        if (equal(familyName, songtiString))
            return songtiSC;
        if (equal(familyName, heitiString))
            return heitiSCReplacement;
        break;
    case 4:
        if (equal(familyName, weiruanYaHeiString))
            return heitiSCReplacement;
        break;
    case 5:
        if (equal(familyName, weiruanZhengHeitiString))
            return heitiTCReplacement;
        break;
    case 6:
        if (equalLettersIgnoringASCIICase(familyName, "simsun"_s))
            return songtiSC;
        if (equal(familyName, weiruanXinXiMingTi))
            return songtiTC;
        break;
    case 10:
        if (equalLettersIgnoringASCIICase(familyName, "ms mingliu"_s))
            return songtiTC;
        if (equalIgnoringASCIICase(familyName, "\\5b8b\\4f53"_s))
            return songtiSC;
        break;
    case 18:
        if (equalLettersIgnoringASCIICase(familyName, "microsoft jhenghei"_s))
            return heitiTCReplacement;
        break;
    }

    return { };
}

void addAttributesForInstalledFonts(CFMutableDictionaryRef attributes, AllowUserInstalledFonts allowUserInstalledFonts)
{
    if (allowUserInstalledFonts == AllowUserInstalledFonts::No) {
        CFDictionaryAddValue(attributes, kCTFontUserInstalledAttribute, kCFBooleanFalse);
        CTFontFallbackOption fallbackOption = kCTFontFallbackOptionSystem;
        auto fallbackOptionNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &fallbackOption));
        CFDictionaryAddValue(attributes, kCTFontFallbackOptionAttribute, fallbackOptionNumber.get());
    }
}

RetainPtr<CTFontRef> createFontForInstalledFonts(CTFontDescriptorRef fontDescriptor, CGFloat size, AllowUserInstalledFonts allowUserInstalledFonts)
{
    auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    addAttributesForInstalledFonts(attributes.get(), allowUserInstalledFonts);
    if (CFDictionaryGetCount(attributes.get())) {
        auto resultFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor, attributes.get()));
        return adoptCF(CTFontCreateWithFontDescriptor(resultFontDescriptor.get(), size, nullptr));
    }
    return adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor, size, nullptr));
}

static inline bool isFontMatchingUserInstalledFontFallback(CTFontRef font, AllowUserInstalledFonts allowUserInstalledFonts)
{
    bool willFallbackToSystemOnly = false;
    if (auto fontFallbackOptionAttributeRef = adoptCF(static_cast<CFNumberRef>(CTFontCopyAttribute(font, kCTFontFallbackOptionAttribute)))) {
        int64_t fontFallbackOptionAttribute;
        CFNumberGetValue(fontFallbackOptionAttributeRef.get(), kCFNumberSInt64Type, &fontFallbackOptionAttribute);
        willFallbackToSystemOnly = fontFallbackOptionAttribute == kCTFontFallbackOptionSystem;
    }

    bool shouldFallbackToSystemOnly = allowUserInstalledFonts == AllowUserInstalledFonts::No;
    return willFallbackToSystemOnly == shouldFallbackToSystemOnly;
}

RetainPtr<CTFontRef> createFontForInstalledFonts(CTFontRef font, AllowUserInstalledFonts allowUserInstalledFonts)
{
    if (isFontMatchingUserInstalledFontFallback(font, allowUserInstalledFonts))
        return font;

    auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    addAttributesForInstalledFonts(attributes.get(), allowUserInstalledFonts);
    if (CFDictionaryGetCount(attributes.get())) {
        auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
        return adoptCF(CTFontCreateCopyWithAttributes(font, CTFontGetSize(font), nullptr, modification.get()));
    }
    return font;
}

void addAttributesForWebFonts(CFMutableDictionaryRef attributes, AllowUserInstalledFonts allowUserInstalledFonts)
{
    if (allowUserInstalledFonts == AllowUserInstalledFonts::No) {
        CTFontFallbackOption fallbackOption = kCTFontFallbackOptionSystem;
        auto fallbackOptionNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &fallbackOption));
        CFDictionaryAddValue(attributes, kCTFontFallbackOptionAttribute, fallbackOptionNumber.get());
    }
}

RetainPtr<CFSetRef> installedFontMandatoryAttributes(AllowUserInstalledFonts allowUserInstalledFonts)
{
    if (allowUserInstalledFonts == AllowUserInstalledFonts::No) {
        CFTypeRef mandatoryAttributesValues[] = { kCTFontFamilyNameAttribute, kCTFontPostScriptNameAttribute, kCTFontEnabledAttribute, kCTFontUserInstalledAttribute, kCTFontFallbackOptionAttribute };
        return adoptCF(CFSetCreate(kCFAllocatorDefault, mandatoryAttributesValues, std::size(mandatoryAttributesValues), &kCFTypeSetCallBacks));
    }
    return nullptr;
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.
    if (auto result = fontForFamily(fontDescription, AtomString("Times"_s)))
        return *result;

    // LastResort is guaranteed to be non-null.
    auto fontDescriptor = adoptCF(CTFontDescriptorCreateLastResort());
    auto font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), fontDescription.computedSize(), nullptr));
    auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(font.get(), fontDescription).boldObliquePair();
    FontPlatformData platformData(font.get(), fontDescription.computedSize(), syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());
    return fontForPlatformData(platformData);
}

FontCache::PrewarmInformation FontCache::collectPrewarmInformation() const
{
    return { copyToVector(m_seenFamiliesForPrewarming), copyToVector(m_fontNamesRequiringSystemFallbackForPrewarming) };
}

void FontCache::prewarm(PrewarmInformation&& prewarmInformation)
{
    if (prewarmInformation.isEmpty())
        return;

    if (!m_prewarmQueue)
        lazyInitialize(m_prewarmQueue, WorkQueue::create("WebKit font prewarm queue"_s));

    m_prewarmQueue->dispatch([&database = m_databaseDisallowingUserInstalledFonts, prewarmInformation = WTFMove(prewarmInformation).isolatedCopy()] {
        for (auto& family : prewarmInformation.seenFamilies)
            database.collectionForFamily(family);

        for (auto& fontName : prewarmInformation.fontNamesRequiringSystemFallback) {
            auto cfFontName = fontName.createCFString();
            if (auto warmingFont = adoptCF(CTFontCreateWithName(cfFontName.get(), 0, nullptr))) {
                // This is sufficient to warm CoreText caches for language and character specific fallbacks.
                CFIndex coveredLength = 0;
                UniChar character = ' ';

                auto fallbackWarmingFont = adoptCF(CTFontCreateForCharactersWithLanguageAndOption(warmingFont.get(), &character, 1, nullptr, kCTFontFallbackOptionSystem, &coveredLength));
            }
        }
    });
}

void FontCache::prewarmGlobally()
{
#if !HAVE(STATIC_FONT_REGISTRY)
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure())
        return;

    Vector<String> families {
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        ".SF NS Text"_s,
        ".SF NS Display"_s,
#endif
        "Arial"_s,
        "Helvetica"_s,
        "Helvetica Neue"_s,
        "Lucida Grande"_s,
        "Times"_s,
        "Times New Roman"_s,
    };

    FontCache::PrewarmInformation prewarmInfo;
    prewarmInfo.seenFamilies = WTFMove(families);
    FontCache::forCurrentThread()->prewarm(WTFMove(prewarmInfo));
#endif
}

void FontCache::platformReleaseNoncriticalMemory()
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=251560): We should be calling invalidate() on all platforms, but this causes a memory regression on iOS.
#if PLATFORM(MAC)
    invalidate();
#else
    m_systemFontDatabaseCoreText.clear();
    m_fontFamilySpecificationCoreTextCache.clear();
#endif
}

HashMap<String, URL>& userInstalledFontMap()
{
    static NeverDestroyed<HashMap<String, URL>> fontMap;
    return fontMap.get();
}

HashMap<String, Vector<String>>& userInstalledFontFamilyMap()
{
    static NeverDestroyed<HashMap<String, Vector<String>>> fontFamilyMap;
    return fontFamilyMap.get();
}

Lock& userInstalledFontMapLock()
{
    static Lock lock;
    return lock;
}

}
