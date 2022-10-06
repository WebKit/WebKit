/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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
#include "FontDatabase.h"
#include "FontFamilySpecificationCoreText.h"
#include "FontPaletteValues.h"
#include "SystemFontDatabaseCoreText.h"
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

// FIXME: This seems like it should be in PlatformHave.h.
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#define HAS_CORE_TEXT_WIDTH_ATTRIBUTE (PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV))

namespace WebCore {

static inline void appendTrueTypeFeature(CFMutableArrayRef features, int type, int selector)
{
    auto typeNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &type));
    auto selectorNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &selector));
    CFTypeRef featureKeys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    CFTypeRef featureValues[] = { typeNumber.get(), selectorNumber.get() };
    auto feature = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, featureKeys, featureValues, WTF_ARRAY_LENGTH(featureKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFArrayAppendValue(features, feature.get());
}

static inline bool tagEquals(FontTag tag, const char comparison[4])
{
    return equalIgnoringASCIICase(tag.data(), comparison, 4);
}

static inline void appendOpenTypeFeature(CFMutableArrayRef features, const FontFeature& feature)
{
    auto featureKey = adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(feature.tag().data()), feature.tag().size() * sizeof(FontTag::value_type), kCFStringEncodingASCII, false));
    int rawFeatureValue = feature.value();
    auto featureValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawFeatureValue));
    CFTypeRef featureDictionaryKeys[] = { kCTFontOpenTypeFeatureTag, kCTFontOpenTypeFeatureValue };
    CFTypeRef featureDictionaryValues[] = { featureKey.get(), featureValue.get() };
    auto featureDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, featureDictionaryKeys, featureDictionaryValues, WTF_ARRAY_LENGTH(featureDictionaryValues), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFArrayAppendValue(features, featureDictionary.get());
}

typedef HashMap<FontTag, int, FourCharacterTagHash, FourCharacterTagHashTraits> FeaturesMap;
typedef HashMap<FontTag, float, FourCharacterTagHash, FourCharacterTagHashTraits> VariationsMap;

static FeaturesMap computeFeatureSettingsFromVariants(const FontVariantSettings& variantSettings)
{
    FeaturesMap result;

    switch (variantSettings.commonLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        result.add(fontFeatureTag("liga"), 1);
        result.add(fontFeatureTag("clig"), 1);
        break;
    case FontVariantLigatures::No:
        result.add(fontFeatureTag("liga"), 0);
        result.add(fontFeatureTag("clig"), 0);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.discretionaryLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        result.add(fontFeatureTag("dlig"), 1);
        break;
    case FontVariantLigatures::No:
        result.add(fontFeatureTag("dlig"), 0);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.historicalLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        result.add(fontFeatureTag("hlig"), 1);
        break;
    case FontVariantLigatures::No:
        result.add(fontFeatureTag("hlig"), 0);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.contextualAlternates) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        result.add(fontFeatureTag("calt"), 1);
        break;
    case FontVariantLigatures::No:
        result.add(fontFeatureTag("calt"), 0);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.position) {
    case FontVariantPosition::Normal:
        break;
    case FontVariantPosition::Subscript:
        result.add(fontFeatureTag("subs"), 1);
        break;
    case FontVariantPosition::Superscript:
        result.add(fontFeatureTag("sups"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.caps) {
    case FontVariantCaps::Normal:
        break;
    case FontVariantCaps::AllSmall:
        result.add(fontFeatureTag("c2sc"), 1);
        FALLTHROUGH;
    case FontVariantCaps::Small:
        result.add(fontFeatureTag("smcp"), 1);
        break;
    case FontVariantCaps::AllPetite:
        result.add(fontFeatureTag("c2pc"), 1);
        FALLTHROUGH;
    case FontVariantCaps::Petite:
        result.add(fontFeatureTag("pcap"), 1);
        break;
    case FontVariantCaps::Unicase:
        result.add(fontFeatureTag("unic"), 1);
        break;
    case FontVariantCaps::Titling:
        result.add(fontFeatureTag("titl"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.numericFigure) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        result.add(fontFeatureTag("lnum"), 1);
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        result.add(fontFeatureTag("onum"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.numericSpacing) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        result.add(fontFeatureTag("pnum"), 1);
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        result.add(fontFeatureTag("tnum"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.numericFraction) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        result.add(fontFeatureTag("frac"), 1);
        break;
    case FontVariantNumericFraction::StackedFractions:
        result.add(fontFeatureTag("afrc"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.numericOrdinal) {
    case FontVariantNumericOrdinal::Normal:
        break;
    case FontVariantNumericOrdinal::Yes:
        result.add(fontFeatureTag("ordn"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.numericSlashedZero) {
    case FontVariantNumericSlashedZero::Normal:
        break;
    case FontVariantNumericSlashedZero::Yes:
        result.add(fontFeatureTag("zero"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    // We don't do anything if "normal".
    if (!variantSettings.alternates.isNormal()) {
        auto values = variantSettings.alternates.values();
        if (values.historicalForms)
            result.add(fontFeatureTag("hist"), 1);
        
        // TODO: handle other tags.
    }

    switch (variantSettings.eastAsianVariant) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        result.add(fontFeatureTag("jp78"), 1);
        break;
    case FontVariantEastAsianVariant::Jis83:
        result.add(fontFeatureTag("jp83"), 1);
        break;
    case FontVariantEastAsianVariant::Jis90:
        result.add(fontFeatureTag("jp90"), 1);
        break;
    case FontVariantEastAsianVariant::Jis04:
        result.add(fontFeatureTag("jp04"), 1);
        break;
    case FontVariantEastAsianVariant::Simplified:
        result.add(fontFeatureTag("smpl"), 1);
        break;
    case FontVariantEastAsianVariant::Traditional:
        result.add(fontFeatureTag("trad"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.eastAsianWidth) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        result.add(fontFeatureTag("fwid"), 1);
        break;
    case FontVariantEastAsianWidth::Proportional:
        result.add(fontFeatureTag("pwid"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (variantSettings.eastAsianRuby) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        result.add(fontFeatureTag("ruby"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return result;
}

static inline bool fontNameIsSystemFont(CFStringRef fontName)
{
    return CFStringGetLength(fontName) > 0 && CFStringGetCharacterAtIndex(fontName, 0) == '.';
}

static RetainPtr<CFArrayRef> variationAxes(CTFontRef font, ShouldLocalizeAxisNames shouldLocalizeAxisNames)
{
#if defined(HAVE_CTFontCopyVariationAxesInternal) // This macro is defined inside CoreText, not WebKit.
    if (shouldLocalizeAxisNames == ShouldLocalizeAxisNames::Yes)
        return adoptCF(CTFontCopyVariationAxes(font));
    return adoptCF(CTFontCopyVariationAxesInternal(font));
#else
    UNUSED_PARAM(shouldLocalizeAxisNames);
    return adoptCF(CTFontCopyVariationAxes(font));
#endif
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

        auto b1 = rawAxisIdentifier >> 24;
        auto b2 = (rawAxisIdentifier & 0xFF0000) >> 16;
        auto b3 = (rawAxisIdentifier & 0xFF00) >> 8;
        auto b4 = rawAxisIdentifier & 0xFF;
        FontTag resultKey = {{ static_cast<char>(b1), static_cast<char>(b2), static_cast<char>(b3), static_cast<char>(b4) }};
        VariationDefaults resultValues = { axisName, rawDefaultValue, rawMinimumValue, rawMaximumValue };
        result.set(resultKey, resultValues);
    }
    return result;
}

static inline bool fontIsSystemFont(CTFontRef font)
{
    if (isSystemFont(font))
        return true;

    auto name = adoptCF(CTFontCopyPostScriptName(font));
    return fontNameIsSystemFont(name.get());
}

// These values were calculated by performing a linear regression on the CSS weights/widths/slopes and Core Text weights/widths/slopes of San Francisco.
// FIXME: <rdar://problem/31312602> Get the real values from Core Text.
static inline float normalizeGXWeight(float value)
{
    return 523.7 * value - 109.3;
}

// These values were experimentally gathered from the various named weights of San Francisco.
static struct {
    float ctWeight;
    float cssWeight;
} keyframes[] = {
    { -0.8, 30 },
    { -0.4, 274 },
    { 0, 400 },
    { 0.23, 510 },
    { 0.3, 590 },
    { 0.4, 700 },
    { 0.56, 860 },
    { 0.62, 1000 },
};
static_assert(WTF_ARRAY_LENGTH(keyframes) > 0);

float normalizeCTWeight(float value)
{
    if (value < keyframes[0].ctWeight)
        return keyframes[0].cssWeight;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyframes) - 1; ++i) {
        auto& before = keyframes[i];
        auto& after = keyframes[i + 1];
        if (value >= before.ctWeight && value <= after.ctWeight) {
            float ratio = (value - before.ctWeight) / (after.ctWeight - before.ctWeight);
            return ratio * (after.cssWeight - before.cssWeight) + before.cssWeight;
        }
    }
    return keyframes[WTF_ARRAY_LENGTH(keyframes) - 1].cssWeight;
}

static inline float normalizeSlope(float value)
{
    return value * 300;
}

static inline float denormalizeGXWeight(float value)
{
    return (value + 109.3) / 523.7;
}

float denormalizeCTWeight(float value)
{
    if (value < keyframes[0].cssWeight)
        return keyframes[0].ctWeight;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyframes) - 1; ++i) {
        auto& before = keyframes[i];
        auto& after = keyframes[i + 1];
        if (value >= before.cssWeight && value <= after.cssWeight) {
            float ratio = (value - before.cssWeight) / (after.cssWeight - before.cssWeight);
            return ratio * (after.ctWeight - before.ctWeight) + before.ctWeight;
        }
    }
    return keyframes[WTF_ARRAY_LENGTH(keyframes) - 1].ctWeight;
}

static inline float denormalizeSlope(float value)
{
    return value / 300;
}

static inline float denormalizeVariationWidth(float value)
{
    if (value <= 125)
        return value / 100;
    if (value <= 150)
        return (value + 125) / 200;
    return (value + 400) / 400;
}

static inline float normalizeVariationWidth(float value)
{
    if (value <= 1.25)
        return value * 100;
    if (value <= 1.375)
        return value * 200 - 125;
    return value * 400 - 400;
}

#if !HAS_CORE_TEXT_WIDTH_ATTRIBUTE
static inline float normalizeWidth(float value)
{
    return normalizeVariationWidth(value + 1);
}
#endif

struct FontType {
    FontType(CTFontRef font)
    {
        bool foundStat = false;
        bool foundTrak = false;
        auto tables = adoptCF(CTFontCopyAvailableTables(font, kCTFontTableOptionNoOptions));
        if (!tables)
            return;
        auto size = CFArrayGetCount(tables.get());
        for (CFIndex i = 0; i < size; ++i) {
            auto tableTag = static_cast<CTFontTableTag>(reinterpret_cast<uintptr_t>(CFArrayGetValueAtIndex(tables.get(), i)));
            switch (tableTag) {
            case kCTFontTableFvar:
                if (variationType == VariationType::NotVariable)
                    variationType = VariationType::TrueTypeGX;
                break;
            case kCTFontTableSTAT:
                foundStat = true;
                variationType = VariationType::OpenType18;
                break;
            case kCTFontTableMorx:
            case kCTFontTableMort:
                aatShaping = true;
                break;
            case kCTFontTableGPOS:
            case kCTFontTableGSUB:
                openTypeShaping = true;
                break;
            case kCTFontTableTrak:
                foundTrak = true;
                break;
            }
        }
        if (foundStat && foundTrak)
            trackingType = TrackingType::Automatic;
        else if (foundTrak)
            trackingType = TrackingType::Manual;
    }

    enum class VariationType : uint8_t { NotVariable, TrueTypeGX, OpenType18, };
    VariationType variationType { VariationType::NotVariable };
    enum class TrackingType : uint8_t { None, Automatic, Manual, };
    TrackingType trackingType { TrackingType::None };
    bool openTypeShaping { false };
    bool aatShaping { false };
};

static void addLightPalette(CFMutableDictionaryRef attributes)
{
    CFIndex light = kCTFontPaletteLight;
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &light));
    CFDictionaryAddValue(attributes, kCTFontPaletteAttribute, number.get());
}

static void addDarkPalette(CFMutableDictionaryRef attributes)
{
    CFIndex dark = kCTFontPaletteDark;
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &dark));
    CFDictionaryAddValue(attributes, kCTFontPaletteAttribute, number.get());
}

static void addAttributesForCustomFontPalettes(CFMutableDictionaryRef attributes, std::optional<FontPaletteIndex> basePalette, const Vector<FontPaletteValues::OverriddenColor>& overrideColors)
{
    if (basePalette) {
        switch (basePalette->type) {
        case FontPaletteIndex::Type::Light:
            addLightPalette(attributes);
            break;
        case FontPaletteIndex::Type::Dark:
            addDarkPalette(attributes);
            break;
        case FontPaletteIndex::Type::Integer: {
            int64_t rawIndex = basePalette->integer; // There is no kCFNumberUIntType.
            auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &rawIndex));
            CFDictionaryAddValue(attributes, kCTFontPaletteAttribute, number.get());
            break;
        }
        }
    }

    if (!overrideColors.isEmpty()) {
        auto overrideDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        for (const auto& pair : overrideColors) {
            const auto& color = pair.second;
            int64_t rawIndex = pair.first; // There is no kCFNumberUIntType.
            auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &rawIndex));
            auto colorObject = cachedCGColor(color);
            CFDictionaryAddValue(overrideDictionary.get(), number.get(), colorObject.get());
        }
        if (CFDictionaryGetCount(overrideDictionary.get()))
            CFDictionaryAddValue(attributes, kCTFontPaletteColorsAttribute, overrideDictionary.get());
    }
}

static void addAttributesForFontPalettes(CFMutableDictionaryRef attributes, const FontPalette& fontPalette, const FontPaletteValues* fontPaletteValues)
{
    switch (fontPalette.type) {
    case FontPalette::Type::Normal:
        break;
    case FontPalette::Type::Light:
        addLightPalette(attributes);
        break;
    case FontPalette::Type::Dark:
        addDarkPalette(attributes);
        break;
    case FontPalette::Type::Custom: {
        if (fontPaletteValues)
            addAttributesForCustomFontPalettes(attributes, fontPaletteValues->basePalette(), fontPaletteValues->overrideColors());
        break;
    }
    }
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

RetainPtr<CTFontRef> preparePlatformFont(CTFontRef originalFont, const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, bool applyWeightWidthSlopeVariations)
{
    if (!originalFont)
        return originalFont;

    FontType fontType { originalFont };

    auto fontOpticalSizing = fontDescription.opticalSizing();

    auto defaultValues = defaultVariationValues(originalFont, ShouldLocalizeAxisNames::No);

    auto fontSelectionRequest = fontDescription.fontSelectionRequest();
    auto fontStyleAxis = fontDescription.fontStyleAxis();

    bool forceOpticalSizingOn = fontOpticalSizing == FontOpticalSizing::Enabled && fontType.variationType == FontType::VariationType::TrueTypeGX && defaultValues.contains({{'o', 'p', 's', 'z'}});
    bool forceVariations = defaultValues.contains({{'w', 'g', 'h', 't'}}) || defaultValues.contains({{'w', 'd', 't', 'h'}}) || (fontStyleAxis == FontStyleAxis::ital && defaultValues.contains({{'i', 't', 'a', 'l'}})) || (fontStyleAxis == FontStyleAxis::slnt && defaultValues.contains({{'s', 'l', 'n', 't'}}));
    const auto& variations = fontDescription.variationSettings();

    const auto& features = fontDescription.featureSettings();
    const auto& variantSettings = fontDescription.variantSettings();
    auto textRenderingMode = fontDescription.textRenderingMode();
    auto shouldDisableLigaturesForSpacing = fontDescription.shouldDisableLigaturesForSpacing();
    bool dontNeedToApplyFontPalettes = fontDescription.fontPalette().type == FontPalette::Type::Normal;

    // We might want to check fontType.trackingType == FontType::TrackingType::Manual here, but in order to maintain compatibility with the rest of the system, we don't.
    bool noFontFeatureSettings = features.isEmpty();
    bool noFontVariationSettings = !forceVariations && variations.isEmpty();
    bool textRenderingModeIsAuto = textRenderingMode == TextRenderingMode::AutoTextRendering;
    bool variantSettingsIsNormal = variantSettings.isAllNormal();
    bool dontNeedToApplyOpticalSizing = fontOpticalSizing == FontOpticalSizing::Enabled && !forceOpticalSizingOn;
    bool fontFaceDoesntSpecifyFeatures = !fontCreationContext.fontFaceFeatures() || fontCreationContext.fontFaceFeatures()->isEmpty();
    if (noFontFeatureSettings && noFontVariationSettings && textRenderingModeIsAuto && variantSettingsIsNormal && dontNeedToApplyOpticalSizing && fontFaceDoesntSpecifyFeatures && !shouldDisableLigaturesForSpacing && dontNeedToApplyFontPalettes) {
#if HAVE(CTFONTCREATEFORCHARACTERSWITHLANGUAGEANDOPTION)
        return originalFont;
#else
        return createFontForInstalledFonts(originalFont, fontDescription.shouldAllowUserInstalledFonts());
#endif
    }

    // This algorithm is described at https://drafts.csswg.org/css-fonts-4/#feature-variation-precedence
    FeaturesMap featuresToBeApplied;
    VariationsMap variationsToBeApplied;

    bool needsConversion = fontType.variationType == FontType::VariationType::TrueTypeGX;

    auto applyVariation = [&](const FontTag& tag, float value) {
        auto iterator = defaultValues.find(tag);
        if (iterator == defaultValues.end())
            return;
        float valueToApply = clampTo(value, iterator->value.minimumValue, iterator->value.maximumValue);
        variationsToBeApplied.set(tag, valueToApply);
    };

    auto applyFeature = [&](const FontTag& tag, int value) {
        // AAT doesn't differentiate between liga and clig. We need to make sure they always agree.
        featuresToBeApplied.set(tag, value);
        if (fontType.aatShaping) {
            if (tag == fontFeatureTag("liga"))
                featuresToBeApplied.set(fontFeatureTag("clig"), value);
            else if (tag == fontFeatureTag("clig"))
                featuresToBeApplied.set(fontFeatureTag("liga"), value);
        }
    };

    // Step 1: CoreText handles default features (such as required ligatures).

    // Step 2: font-weight, font-stretch, and font-style
    // The system font is somewhat magical. Don't mess with its variations.
    if (applyWeightWidthSlopeVariations
#if USE(NON_VARIABLE_SYSTEM_FONT)
        && !fontIsSystemFont(originalFont)
#endif
    ) {
        float weight = fontSelectionRequest.weight;
        float width = fontSelectionRequest.width;
        float slope = fontSelectionRequest.slope.value_or(normalItalicValue());
        if (auto weightValue = fontCreationContext.fontFaceCapabilities().weight)
            weight = std::max(std::min(weight, static_cast<float>(weightValue->maximum)), static_cast<float>(weightValue->minimum));
        if (auto widthValue = fontCreationContext.fontFaceCapabilities().width)
            width = std::max(std::min(width, static_cast<float>(widthValue->maximum)), static_cast<float>(widthValue->minimum));
        if (auto slopeValue = fontCreationContext.fontFaceCapabilities().weight)
            slope = std::max(std::min(slope, static_cast<float>(slopeValue->maximum)), static_cast<float>(slopeValue->minimum));
        if (shouldEnhanceTextLegibility() && fontIsSystemFont(originalFont)) {
            auto ctWeight = denormalizeCTWeight(weight);
            ctWeight = CTFontGetAccessibilityBoldWeightOfWeight(ctWeight);
            weight = normalizeCTWeight(ctWeight);
        }
        if (needsConversion) {
            weight = denormalizeGXWeight(weight);
            width = denormalizeVariationWidth(width);
            slope = denormalizeSlope(slope);
        }
        applyVariation({{'w', 'g', 'h', 't'}}, weight);
        applyVariation({{'w', 'd', 't', 'h'}}, width);
        if (fontStyleAxis == FontStyleAxis::ital)
            applyVariation({{'i', 't', 'a', 'l'}}, 1);
        else
            applyVariation({{'s', 'l', 'n', 't'}}, slope);
    }

    // FIXME: Implement Step 5: font-named-instance

    // FIXME: Implement Step 6: the font-variation-settings descriptor inside @font-face

    // Step 7: Consult with font-feature-settings inside @font-face
    if (fontCreationContext.fontFaceFeatures() && !fontCreationContext.fontFaceFeatures()->isEmpty()) {
        for (auto& fontFaceFeature : *fontCreationContext.fontFaceFeatures())
            applyFeature(fontFaceFeature.tag(), fontFaceFeature.value());
    }

    // FIXME: Move font-optical-sizing handling here. It should be step 9.

    // Step 10: Font-variant
    for (auto& newFeature : computeFeatureSettingsFromVariants(variantSettings))
        applyFeature(newFeature.key, newFeature.value);

    // Step 11: Other properties
    if (textRenderingMode == TextRenderingMode::OptimizeSpeed) {
        applyFeature(fontFeatureTag("liga"), 0);
        applyFeature(fontFeatureTag("clig"), 0);
        applyFeature(fontFeatureTag("dlig"), 0);
        applyFeature(fontFeatureTag("hlig"), 0);
        applyFeature(fontFeatureTag("calt"), 0);
    }
    if (shouldDisableLigaturesForSpacing) {
        applyFeature(fontFeatureTag("liga"), 0);
        applyFeature(fontFeatureTag("clig"), 0);
        applyFeature(fontFeatureTag("dlig"), 0);
        applyFeature(fontFeatureTag("hlig"), 0);
        // Core Text doesn't disable calt when letter-spacing is applied, so we won't either.
    }

    // Step 13: Font-feature-settings
    for (auto& newFeature : features)
        applyFeature(newFeature.tag(), newFeature.value());

    // Step 12: font-variation-settings
    for (auto& newVariation : variations)
        applyVariation(newVariation.tag(), newVariation.value());

    auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    if (!featuresToBeApplied.isEmpty()) {
        auto featureArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, features.size(), &kCFTypeArrayCallBacks));
        for (auto& p : featuresToBeApplied) {
            auto feature = FontFeature(p.key, p.value);

            // CoreText does not map hlig and hist for TrueType fonts.
            if (fontType.aatShaping && (tagEquals(feature.tag(), "hlig") || tagEquals(feature.tag(), "hist"))) {
                if (feature.enabled())
                    appendTrueTypeFeature(featureArray.get(), kLigaturesType, kHistoricalLigaturesOnSelector);
                else if (tagEquals(feature.tag(), "hlig"))
                    appendTrueTypeFeature(featureArray.get(), kLigaturesType, kHistoricalLigaturesOffSelector);
                continue;
            }

            appendOpenTypeFeature(featureArray.get(), feature);
        }
        CFDictionaryAddValue(attributes.get(), kCTFontFeatureSettingsAttribute, featureArray.get());
    }
    if (!variationsToBeApplied.isEmpty()) {
        auto variationDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        for (auto& p : variationsToBeApplied) {
            long long bitwiseTag = p.key[0] << 24 | p.key[1] << 16 | p.key[2] << 8 | p.key[3];
            auto tagNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &bitwiseTag));
            auto valueNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &p.value));
            CFDictionarySetValue(variationDictionary.get(), tagNumber.get(), valueNumber.get());
        }
        CFDictionaryAddValue(attributes.get(), kCTFontVariationAttribute, variationDictionary.get());
    }

    // Step 9: font-optical-sizing
    // FIXME: Apply this before font-variation-settings
    if (forceOpticalSizingOn || textRenderingMode == TextRenderingMode::OptimizeLegibility) {
#if HAVE(CORETEXT_AUTO_OPTICAL_SIZING)
        CFDictionaryAddValue(attributes.get(), kCTFontOpticalSizeAttribute, CFSTR("auto"));
#else
        auto size = CTFontGetSize(originalFont);
        auto sizeNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &size));
        CFDictionaryAddValue(attributes.get(), kCTFontOpticalSizeAttribute, sizeNumber.get());
#endif
    } else if (fontOpticalSizing == FontOpticalSizing::Disabled) {
#if HAVE(CORETEXT_AUTO_OPTICAL_SIZING)
        CFDictionaryAddValue(attributes.get(), kCTFontOpticalSizeAttribute, CFSTR("none"));
#endif
    }

    addAttributesForFontPalettes(attributes.get(), fontDescription.fontPalette(), fontCreationContext.fontPaletteValues());

    addAttributesForInstalledFonts(attributes.get(), fontDescription.shouldAllowUserInstalledFonts());

    auto descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    return adoptCF(CTFontCreateCopyWithAttributes(originalFont, CTFontGetSize(originalFont), nullptr, descriptor.get()));
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

#if !HAS_CORE_TEXT_WIDTH_ATTRIBUTE
static float stretchFromCoreTextTraits(CFDictionaryRef traits)
{
    auto widthNumber = static_cast<CFNumberRef>(CFDictionaryGetValue(traits, kCTFontWidthTrait));
    if (!widthNumber)
        return normalStretchValue();

    float ctWidth;
    auto success = CFNumberGetValue(widthNumber, kCFNumberFloatType, &ctWidth);
    ASSERT_UNUSED(success, success);
    return normalizeWidth(ctWidth);
}
#endif

static void fontCacheRegisteredFontsChangedNotificationCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void *, CFDictionaryRef)
{
    ASSERT_UNUSED(observer, isMainThread() && observer == &FontCache::forCurrentThread());

    ensureOnMainThread([] {
        FontCache::invalidateAllFontCaches();
    });
}

void FontCache::platformInit()
{
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, &fontCacheRegisteredFontsChangedNotificationCallback, kCTFontManagerRegisteredFontsChangedNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);

#if PLATFORM(IOS_FAMILY)
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, &fontCacheRegisteredFontsChangedNotificationCallback, getUIContentSizeCategoryDidChangeNotificationName(), 0, CFNotificationSuspensionBehaviorDeliverImmediately);
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
    // AtomString's operator[] handles out-of-bounds by returning 0.
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

SynthesisPair computeNecessarySynthesis(CTFontRef font, const FontDescription& fontDescription, ShouldComputePhysicalTraits shouldComputePhysicalTraits, bool isPlatformFont)
{
    if (CTFontIsAppleColorEmoji(font))
        return SynthesisPair(false, false);

    if (isPlatformFont)
        return SynthesisPair(false, false);

    CTFontSymbolicTraits desiredTraits = computeTraits(fontDescription);

    CTFontSymbolicTraits actualTraits = 0;
    if (isFontWeightBold(fontDescription.weight()) || isItalic(fontDescription.italic())) {
        if (shouldComputePhysicalTraits == ShouldComputePhysicalTraits::Yes)
            actualTraits = CTFontGetPhysicalSymbolicTraits(font);
        else
            actualTraits = CTFontGetSymbolicTraits(font);
    }

    bool needsSyntheticBold = fontDescription.hasAutoFontSynthesisWeight() && (desiredTraits & kCTFontTraitBold) && !(actualTraits & kCTFontTraitBold);
    bool needsSyntheticOblique = fontDescription.hasAutoFontSynthesisStyle() && (desiredTraits & kCTFontTraitItalic) && !(actualTraits & kCTFontTraitItalic);

    return SynthesisPair(needsSyntheticBold, needsSyntheticOblique);
}

class Allowlist {
public:
    static Allowlist& singleton() WTF_REQUIRES_LOCK(lock)
    {
        static NeverDestroyed<Allowlist> allowlist;
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

Lock Allowlist::lock;

void FontCache::setFontAllowlist(const Vector<String>& inputAllowlist)
{
    Locker locker { Allowlist::lock };
    Allowlist::singleton().set(inputAllowlist);
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

    auto font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor, 0, nullptr));
    auto variations = variationAxes(font.get(), ShouldLocalizeAxisNames::No);
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

    bool optOutFromGXNormalization = false;
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    optOutFromGXNormalization = CTFontDescriptorIsSystemUIFont(fontDescriptor);
#endif

    if (FontType(font.get()).variationType == FontType::VariationType::TrueTypeGX && !optOutFromGXNormalization) {
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

#if !HAS_CORE_TEXT_WIDTH_ATTRIBUTE
    bool weightOrWidthComeFromTraits = !variationCapabilities.weight || !variationCapabilities.width;
#else
    bool weightOrWidthComeFromTraits = false;
#endif

    if (!variationCapabilities.slope || weightOrWidthComeFromTraits) {
        auto traits = adoptCF(static_cast<CFDictionaryRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontTraitsAttribute)));
        if (traits) {
#if !HAS_CORE_TEXT_WIDTH_ATTRIBUTE
            if (!variationCapabilities.width) {
                auto widthValue = stretchFromCoreTextTraits(traits.get());
                variationCapabilities.width = {{ widthValue, widthValue }};
            }
#endif

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

#if HAS_CORE_TEXT_WIDTH_ATTRIBUTE
    if (!variationCapabilities.width) {
        auto value = getCSSAttribute(fontDescriptor, kCTFontCSSWidthAttribute, static_cast<float>(normalStretchValue()));
        variationCapabilities.width = {{ value, value }};
    }
#endif

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
    RetainPtr<CTFontRef> result;
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

    Locker locker { Allowlist::lock };
    return Allowlist::singleton().allows(family);
}

static FontLookup platformFontLookupWithFamily(FontDatabase& fontDatabase, const AtomString& family, FontSelectionRequest request, float size)
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
        if ((isItalic(request.slope) && !isItalic(postScriptFont.capabilities.slope.maximum))
            || (isFontWeightBold(request.weight) && !isFontWeightBold(postScriptFont.capabilities.weight.maximum))) {
            auto postScriptFamilyName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(postScriptFont.fontDescriptor.get(), kCTFontFamilyNameAttribute)));
            if (!postScriptFamilyName)
                return { nullptr };
            const auto& familyFonts = fontDatabase.collectionForFamily(String(postScriptFamilyName.get()));
            if (familyFonts.isEmpty())
                return { nullptr };
            if (const auto* installedFont = findClosestFont(familyFonts, request)) {
                if (!installedFont->fontDescriptor)
                    return { nullptr };
                return { adoptCF(CTFontCreateWithFontDescriptor(installedFont->fontDescriptor.get(), size, nullptr)), true };
            }
            return { nullptr };
        }
        return { adoptCF(CTFontCreateWithFontDescriptor(postScriptFont.fontDescriptor.get(), size, nullptr)), true };
    }

    if (const auto* installedFont = findClosestFont(familyFonts, request))
        return { adoptCF(CTFontCreateWithFontDescriptor(installedFont->fontDescriptor.get(), size, nullptr)), false };

    return { nullptr };
}

void FontCache::platformInvalidate()
{
    // FIXME: Workers need to access SystemFontDatabaseCoreText.
    m_fontFamilySpecificationCoreTextCache.clear();
    m_systemFontDatabaseCoreText.clear();

    platformShouldEnhanceTextLegibility() = _AXSEnhanceTextLegibilityEnabled();
}

static RetainPtr<CTFontRef> fontWithFamilySpecialCase(const AtomString& family, const FontDescription& fontDescription, float size, AllowUserInstalledFonts allowUserInstalledFonts)
{
    // FIXME: See comment in FontCascadeDescription::effectiveFamilyAt() in FontDescriptionCocoa.cpp
    std::optional<SystemFontKind> systemDesign;

#if HAVE(DESIGN_SYSTEM_UI_FONTS)
    if (equalLettersIgnoringASCIICase(family, "ui-serif"_s))
        systemDesign = SystemFontKind::UISerif;
    else if (equalLettersIgnoringASCIICase(family, "ui-monospace"_s))
        systemDesign = SystemFontKind::UIMonospace;
    else if (equalLettersIgnoringASCIICase(family, "ui-rounded"_s))
        systemDesign = SystemFontKind::UIRounded;
#endif

    if (equalLettersIgnoringASCIICase(family, "-webkit-system-font"_s) || equalLettersIgnoringASCIICase(family, "-apple-system"_s) || equalLettersIgnoringASCIICase(family, "-apple-system-font"_s) || equalLettersIgnoringASCIICase(family, "system-ui"_s) || equalLettersIgnoringASCIICase(family, "ui-sans-serif"_s)) {
        ASSERT(!systemDesign);
        systemDesign = SystemFontKind::SystemUI;
    }

    if (systemDesign) {
        auto cascadeList = SystemFontDatabaseCoreText::forCurrentThread().cascadeList(fontDescription, family, *systemDesign, allowUserInstalledFonts);
        if (cascadeList.isEmpty())
            return nullptr;
        return createFontForInstalledFonts(cascadeList[0].get(), size, allowUserInstalledFonts);
    }

    if (family.startsWith("UICTFontTextStyle"_s)) {
        const auto& request = fontDescription.fontSelectionRequest();
        CTFontSymbolicTraits traits = (isFontWeightBold(request.weight) ? kCTFontTraitBold : 0) | (isItalic(request.slope) ? kCTFontTraitItalic : 0);
        auto descriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(family.string().createCFString().get(), contentSizeCategory(), fontDescription.computedLocale().string().createCFString().get()));
        if (traits)
            descriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(descriptor.get(), traits, traits));
        return createFontForInstalledFonts(descriptor.get(), size, allowUserInstalledFonts);
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-menu"_s)) {
        auto result = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontMenuItem, size, fontDescription.computedLocale().string().createCFString().get()));
        return createFontForInstalledFonts(result.get(), allowUserInstalledFonts);
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-status-bar"_s)) {
        auto result = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, fontDescription.computedLocale().string().createCFString().get()));
        return createFontForInstalledFonts(result.get(), allowUserInstalledFonts);
    }

    if (equalLettersIgnoringASCIICase(family, "lastresort"_s)) {
        auto lastResort = adoptCF(CTFontDescriptorCreateLastResort());
        return adoptCF(CTFontCreateWithFontDescriptor(lastResort.get(), size, nullptr));
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-system-monospaced-numbers"_s)) {
        int numberSpacingType = kNumberSpacingType;
        int monospacedNumbersSelector = kMonospacedNumbersSelector;
        auto numberSpacingNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &numberSpacingType));
        auto monospacedNumbersNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &monospacedNumbersSelector));
        auto systemFontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, size, nullptr));
        auto monospaceFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithFeature(systemFontDescriptor.get(), numberSpacingNumber.get(), monospacedNumbersNumber.get()));
        return createFontForInstalledFonts(monospaceFontDescriptor.get(), size, allowUserInstalledFonts);
    }

    return nullptr;
}

static RetainPtr<CTFontRef> fontWithFamily(FontDatabase& fontDatabase, const AtomString& family, const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, float size)
{
    ASSERT(fontDatabase.allowUserInstalledFonts() == fontDescription.shouldAllowUserInstalledFonts());

    if (family.isEmpty())
        return nullptr;

    FontLookup fontLookup;
    fontLookup.result = fontWithFamilySpecialCase(family, fontDescription, size, fontDescription.shouldAllowUserInstalledFonts());
    if (!fontLookup.result)
        fontLookup = platformFontLookupWithFamily(fontDatabase, family, fontDescription.fontSelectionRequest(), size);
    return preparePlatformFont(fontLookup.result.get(), fontDescription, fontCreationContext, !fontLookup.createdFromPostScriptName);
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
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    auto newFont = adoptCF(CTFontCreateWithFontDescriptor(descriptor.get(), size, nullptr));
}
#endif

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomString& family, const FontCreationContext& fontCreationContext)
{
    float size = fontDescription.computedPixelSize();

    auto& fontDatabase = database(fontDescription.shouldAllowUserInstalledFonts());
    auto font = fontWithFamily(fontDatabase, family, fontDescription, fontCreationContext, size);

#if PLATFORM(MAC)
    if (!font) {
        if (!shouldAutoActivateFontIfNeeded(family))
            return nullptr;

        // Auto activate the font before looking for it a second time.
        // Ignore the result because we want to use our own algorithm to actually find the font.
        autoActivateFont(family.string(), size);

        font = fontWithFamily(fontDatabase, family, fontDescription, fontCreationContext, size);
    }
#endif

    if (!font)
        return nullptr;

    if (fontDescription.shouldAllowUserInstalledFonts() == AllowUserInstalledFonts::No)
        m_seenFamiliesForPrewarming.add(FontCascadeDescription::foldedFamilyName(family));

    auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(font.get(), fontDescription).boldObliquePair();

    return makeUnique<FontPlatformData>(font.get(), size, syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());
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
static inline bool isArabicCharacter(UChar character)
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

#if HAVE(CTFONTCREATEFORCHARACTERSWITHLANGUAGEANDOPTION)
static RetainPtr<CTFontRef> createFontForCharacters(CTFontRef font, CFStringRef localeString, AllowUserInstalledFonts allowUserInstalledFonts, const UChar* characters, unsigned length)
{
    CFIndex coveredLength = 0;
    auto fallbackOption = allowUserInstalledFonts == AllowUserInstalledFonts::No ? kCTFontFallbackOptionSystem : kCTFontFallbackOptionDefault;
    return adoptCF(CTFontCreateForCharactersWithLanguageAndOption(font, reinterpret_cast<const UniChar*>(characters), length, localeString, fallbackOption, &coveredLength));
}
#else
static RetainPtr<CTFontRef> createFontForCharacters(CTFontRef font, CFStringRef localeString, AllowUserInstalledFonts, const UChar* characters, unsigned length)
{
    CFIndex coveredLength = 0;
    return adoptCF(CTFontCreateForCharactersWithLanguage(font, reinterpret_cast<const UniChar*>(characters), length, localeString, &coveredLength));
}
#endif

static RetainPtr<CTFontRef> lookupFallbackFont(CTFontRef font, FontSelectionValue fontWeight, const AtomString& locale, AllowUserInstalledFonts allowUserInstalledFonts, const UChar* characters, unsigned length)
{
    ASSERT(length > 0);

    RetainPtr<CFStringRef> localeString;
// FIXME: Why not do this on watchOS and tvOS?
#if PLATFORM(IOS) || PLATFORM(MAC)
    if (!locale.isNull())
        localeString = locale.string().createCFString();
#else
    UNUSED_PARAM(locale);
#endif

    auto result = createFontForCharacters(font, localeString.get(), allowUserInstalledFonts, characters, length);
    ASSERT(!isUserInstalledFont(result.get()) || allowUserInstalledFonts == AllowUserInstalledFonts::Yes);

#if PLATFORM(IOS_FAMILY)
    // Callers of this function won't include multiple code points. "Length" is to know how many code units
    // are in the code point.
    UChar firstCharacter = characters[0];
    if (isArabicCharacter(firstCharacter)) {
        auto familyName = adoptCF(static_cast<CFStringRef>(CTFontCopyAttribute(result.get(), kCTFontFamilyNameAttribute)));
        if (fontFamilyShouldNotBeUsedForArabic(familyName.get())) {
            CFStringRef newFamilyName = isFontWeightBold(fontWeight) ? CFSTR("GeezaPro-Bold") : CFSTR("GeezaPro");
            CFTypeRef keys[] = { kCTFontNameAttribute };
            CFTypeRef values[] = { newFamilyName };
            auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
            result = adoptCF(CTFontCreateCopyWithAttributes(result.get(), CTFontGetSize(result.get()), nullptr, modification.get()));
        }
    }
#else
    UNUSED_PARAM(fontWeight);
#endif

    return result;
}

RefPtr<Font> FontCache::systemFallbackForCharacters(const FontDescription& description, const Font& originalFontData, IsForPlatformFont isForPlatformFont, PreferColoredFont, const UChar* characters, unsigned length)
{
    const FontPlatformData& platformData = originalFontData.platformData();

    auto fullName = String(adoptCF(CTFontCopyFullName(platformData.font())).get());
    if (!fullName.isEmpty())
        m_fontNamesRequiringSystemFallbackForPrewarming.add(fullName);

    auto result = lookupFallbackFont(platformData.font(), description.weight(), description.computedLocale(), description.shouldAllowUserInstalledFonts(), characters, length);
    result = preparePlatformFont(result.get(), description, { });

    if (!result)
        return lastResortFallbackFont(description);

    // FontCascade::drawGlyphBuffer() requires that there are no duplicate Font objects which refer to the same thing. This is enforced in
    // FontCache::fontForPlatformData(), where our equality check is based on hashing the FontPlatformData, whose hash includes the raw CoreText
    // font pointer.
    CTFontRef substituteFont = m_fallbackFonts.add(result).iterator->get();

    auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(substituteFont, description, ShouldComputePhysicalTraits::No, isForPlatformFont == IsForPlatformFont::Yes).boldObliquePair();

    const FontPlatformData::CreationData* creationData = nullptr;
    if (safeCFEqual(platformData.font(), substituteFont))
        creationData = platformData.creationData();
    FontPlatformData alternateFont(substituteFont, platformData.size(), syntheticBold, syntheticOblique, platformData.orientation(), platformData.widthVariant(), platformData.textRenderingMode(), creationData);

    return fontForPlatformData(alternateFont);
}

std::optional<ASCIILiteral> FontCache::platformAlternateFamilyName(const String& familyName)
{
    static const UChar heitiString[] = { 0x9ed1, 0x4f53 };
    static const UChar songtiString[] = { 0x5b8b, 0x4f53 };
    static const UChar weiruanXinXiMingTi[] = { 0x5fae, 0x8edf, 0x65b0, 0x7d30, 0x660e, 0x9ad4 };
    static const UChar weiruanYaHeiString[] = { 0x5fae, 0x8f6f, 0x96c5, 0x9ed1 };
    static const UChar weiruanZhengHeitiString[] = { 0x5fae, 0x8edf, 0x6b63, 0x9ed1, 0x9ad4 };

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

    return std::nullopt;
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
        return adoptCF(CFSetCreate(kCFAllocatorDefault, mandatoryAttributesValues, WTF_ARRAY_LENGTH(mandatoryAttributesValues), &kCFTypeSetCallBacks));
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
    auto font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), fontDescription.computedPixelSize(), nullptr));
    auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(font.get(), fontDescription).boldObliquePair();
    FontPlatformData platformData(font.get(), fontDescription.computedPixelSize(), syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());
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
        m_prewarmQueue = WorkQueue::create("WebKit font prewarm queue");

    m_prewarmQueue->dispatch([&database = m_databaseDisallowingUserInstalledFonts, prewarmInformation = WTFMove(prewarmInformation).isolatedCopy()] {
        for (auto& family : prewarmInformation.seenFamilies)
            database.collectionForFamily(family);

        for (auto& fontName : prewarmInformation.fontNamesRequiringSystemFallback) {
            auto cfFontName = fontName.createCFString();
            if (auto warmingFont = adoptCF(CTFontCreateWithName(cfFontName.get(), 0, nullptr))) {
                // This is sufficient to warm CoreText caches for language and character specific fallbacks.
                CFIndex coveredLength = 0;
                UniChar character = ' ';

#if HAVE(CTFONTCREATEFORCHARACTERSWITHLANGUAGEANDOPTION)
                auto fallbackWarmingFont = adoptCF(CTFontCreateForCharactersWithLanguageAndOption(warmingFont.get(), &character, 1, nullptr, kCTFontFallbackOptionSystem, &coveredLength));
#else
                auto fallbackWarmingFont = adoptCF(CTFontCreateForCharactersWithLanguage(warmingFont.get(), &character, 1, nullptr, &coveredLength));
#endif
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
    FontCache::forCurrentThread().prewarm(WTFMove(prewarmInfo));
#endif
}

void FontCache::platformReleaseNoncriticalMemory()
{
    m_systemFontDatabaseCoreText.clear();
    m_fontFamilySpecificationCoreTextCache.clear();
}

}
