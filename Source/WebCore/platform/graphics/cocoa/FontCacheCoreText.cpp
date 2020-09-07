/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "Font.h"
#include "FontCascadeDescription.h"
#include "FontFamilySpecificationCoreText.h"
#include "RenderThemeCocoa.h"
#include "SystemFontDatabaseCoreText.h"
#include <pal/spi/cocoa/CoreTextSPI.h>

#include <CoreText/SFNTLayoutTypes.h>

#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>

// FIXME: This seems like it should be in PlatformHave.h.
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#define HAS_CORE_TEXT_WIDTH_ATTRIBUTE (PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV))

namespace WebCore {

static inline void appendRawTrueTypeFeature(CFMutableArrayRef features, int type, int selector)
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

static inline void appendTrueTypeFeature(CFMutableArrayRef features, const FontFeature& feature)
{
    if (tagEquals(feature.tag(), "liga") || tagEquals(feature.tag(), "clig")) {
        if (feature.enabled()) {
            appendRawTrueTypeFeature(features, kLigaturesType, kCommonLigaturesOnSelector);
            appendRawTrueTypeFeature(features, kLigaturesType, kContextualLigaturesOnSelector);
        } else {
            appendRawTrueTypeFeature(features, kLigaturesType, kCommonLigaturesOffSelector);
            appendRawTrueTypeFeature(features, kLigaturesType, kContextualLigaturesOffSelector);
        }
    } else if (tagEquals(feature.tag(), "dlig")) {
        if (feature.enabled())
            appendRawTrueTypeFeature(features, kLigaturesType, kRareLigaturesOnSelector);
        else
            appendRawTrueTypeFeature(features, kLigaturesType, kRareLigaturesOffSelector);
    } else if (tagEquals(feature.tag(), "hlig")) {
        if (feature.enabled())
            appendRawTrueTypeFeature(features, kLigaturesType, kHistoricalLigaturesOnSelector);
        else
            appendRawTrueTypeFeature(features, kLigaturesType, kHistoricalLigaturesOffSelector);
    } else if (tagEquals(feature.tag(), "calt")) {
        if (feature.enabled())
            appendRawTrueTypeFeature(features, kContextualAlternatesType, kContextualAlternatesOnSelector);
        else
            appendRawTrueTypeFeature(features, kContextualAlternatesType, kContextualAlternatesOffSelector);
    } else if (tagEquals(feature.tag(), "subs") && feature.enabled())
        appendRawTrueTypeFeature(features, kVerticalPositionType, kInferiorsSelector);
    else if (tagEquals(feature.tag(), "sups") && feature.enabled())
        appendRawTrueTypeFeature(features, kVerticalPositionType, kSuperiorsSelector);
    else if (tagEquals(feature.tag(), "smcp") && feature.enabled())
        appendRawTrueTypeFeature(features, kLowerCaseType, kLowerCaseSmallCapsSelector);
    else if (tagEquals(feature.tag(), "c2sc") && feature.enabled())
        appendRawTrueTypeFeature(features, kUpperCaseType, kUpperCaseSmallCapsSelector);
    else if (tagEquals(feature.tag(), "pcap") && feature.enabled())
        appendRawTrueTypeFeature(features, kLowerCaseType, kLowerCasePetiteCapsSelector);
    else if (tagEquals(feature.tag(), "c2pc") && feature.enabled())
        appendRawTrueTypeFeature(features, kUpperCaseType, kUpperCasePetiteCapsSelector);
    else if (tagEquals(feature.tag(), "unic") && feature.enabled())
        appendRawTrueTypeFeature(features, kLetterCaseType, 14);
    else if (tagEquals(feature.tag(), "titl") && feature.enabled())
        appendRawTrueTypeFeature(features, kStyleOptionsType, kTitlingCapsSelector);
    else if (tagEquals(feature.tag(), "lnum") && feature.enabled())
        appendRawTrueTypeFeature(features, kNumberCaseType, kUpperCaseNumbersSelector);
    else if (tagEquals(feature.tag(), "onum") && feature.enabled())
        appendRawTrueTypeFeature(features, kNumberCaseType, kLowerCaseNumbersSelector);
    else if (tagEquals(feature.tag(), "pnum") && feature.enabled())
        appendRawTrueTypeFeature(features, kNumberSpacingType, kProportionalNumbersSelector);
    else if (tagEquals(feature.tag(), "tnum") && feature.enabled())
        appendRawTrueTypeFeature(features, kNumberSpacingType, kMonospacedNumbersSelector);
    else if (tagEquals(feature.tag(), "frac") && feature.enabled())
        appendRawTrueTypeFeature(features, kFractionsType, kDiagonalFractionsSelector);
    else if (tagEquals(feature.tag(), "afrc") && feature.enabled())
        appendRawTrueTypeFeature(features, kFractionsType, kVerticalFractionsSelector);
    else if (tagEquals(feature.tag(), "ordn") && feature.enabled())
        appendRawTrueTypeFeature(features, kVerticalPositionType, kOrdinalsSelector);
    else if (tagEquals(feature.tag(), "zero") && feature.enabled())
        appendRawTrueTypeFeature(features, kTypographicExtrasType, kSlashedZeroOnSelector);
    else if (tagEquals(feature.tag(), "hist") && feature.enabled())
        appendRawTrueTypeFeature(features, kLigaturesType, kHistoricalLigaturesOnSelector);
    else if (tagEquals(feature.tag(), "jp78") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kJIS1978CharactersSelector);
    else if (tagEquals(feature.tag(), "jp83") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kJIS1983CharactersSelector);
    else if (tagEquals(feature.tag(), "jp90") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kJIS1990CharactersSelector);
    else if (tagEquals(feature.tag(), "jp04") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kJIS2004CharactersSelector);
    else if (tagEquals(feature.tag(), "smpl") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kSimplifiedCharactersSelector);
    else if (tagEquals(feature.tag(), "trad") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kTraditionalCharactersSelector);
    else if (tagEquals(feature.tag(), "fwid") && feature.enabled())
        appendRawTrueTypeFeature(features, kTextSpacingType, kMonospacedTextSelector);
    else if (tagEquals(feature.tag(), "pwid") && feature.enabled())
        appendRawTrueTypeFeature(features, kTextSpacingType, kProportionalTextSelector);
    else if (tagEquals(feature.tag(), "ruby") && feature.enabled())
        appendRawTrueTypeFeature(features, kRubyKanaType, kRubyKanaOnSelector);
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

    switch (variantSettings.alternates) {
    case FontVariantAlternates::Normal:
        break;
    case FontVariantAlternates::HistoricalForms:
        result.add(fontFeatureTag("hist"), 1);
        break;
    default:
        ASSERT_NOT_REACHED();
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

struct VariationDefaults {
    float defaultValue;
    float minimumValue;
    float maximumValue;
};

typedef HashMap<FontTag, VariationDefaults, FourCharacterTagHash, FourCharacterTagHashTraits> VariationDefaultsMap;

static VariationDefaultsMap defaultVariationValues(CTFontRef font)
{
    VariationDefaultsMap result;
    auto axes = adoptCF(CTFontCopyVariationAxes(font));
    if (!axes)
        return result;
    auto size = CFArrayGetCount(axes.get());
    for (CFIndex i = 0; i < size; ++i) {
        CFDictionaryRef axis = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(axes.get(), i));
        CFNumberRef axisIdentifier = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisIdentifierKey));
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
        VariationDefaults resultValues = { rawDefaultValue, rawMinimumValue, rawMaximumValue };
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
static inline float normalizeWeight(float value)
{
    return 523.7 * value - 109.3;
}

static inline float normalizeSlope(float value)
{
    return value * 300;
}

static inline float denormalizeWeight(float value)
{
    return (value + 109.3) / 523.7;
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
        auto tables = adoptCF(CTFontCopyAvailableTables(font, kCTFontTableOptionNoOptions));
        if (!tables)
            return;
        bool foundStat = false;
        bool foundTrak = false;
        auto size = CFArrayGetCount(tables.get());
        for (CFIndex i = 0; i < size; ++i) {
            // This is so yucky.
            // https://developer.apple.com/reference/coretext/1510774-ctfontcopyavailabletables
            // "The returned set will contain unboxed values, which can be extracted like so:"
            // "CTFontTableTag tag = (CTFontTableTag)(uintptr_t)CFArrayGetValueAtIndex(tags, index);"
            CTFontTableTag tableTag = static_cast<CTFontTableTag>(reinterpret_cast<uintptr_t>(CFArrayGetValueAtIndex(tables.get(), i)));
            switch (tableTag) {
            case 'fvar':
                if (variationType == VariationType::NotVariable)
                    variationType = VariationType::TrueTypeGX;
                break;
            case 'STAT':
                foundStat = true;
                variationType = VariationType::OpenType18;
                break;
            case 'morx':
            case 'mort':
                aatShaping = true;
                break;
            case 'GPOS':
            case 'GSUB':
                openTypeShaping = true;
                break;
            case 'trak':
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

RetainPtr<CTFontRef> preparePlatformFont(CTFontRef originalFont, const FontDescription& fontDescription, const FontFeatureSettings* fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities, bool applyWeightWidthSlopeVariations)
{
    if (!originalFont)
        return originalFont;

    FontType fontType { originalFont };

    auto fontOpticalSizing = fontDescription.opticalSizing();

    auto defaultValues = defaultVariationValues(originalFont);

    auto fontSelectionRequest = fontDescription.fontSelectionRequest();
    auto fontStyleAxis = fontDescription.fontStyleAxis();

    bool forceOpticalSizingOn = fontOpticalSizing == FontOpticalSizing::Enabled && fontType.variationType == FontType::VariationType::TrueTypeGX && defaultValues.contains({{'o', 'p', 's', 'z'}});
    bool forceVariations = defaultValues.contains({{'w', 'g', 'h', 't'}}) || defaultValues.contains({{'w', 'd', 't', 'h'}}) || (fontStyleAxis == FontStyleAxis::ital && defaultValues.contains({{'i', 't', 'a', 'l'}})) || (fontStyleAxis == FontStyleAxis::slnt && defaultValues.contains({{'s', 'l', 'n', 't'}}));
    const auto& variations = fontDescription.variationSettings();

    const auto& features = fontDescription.featureSettings();
    const auto& variantSettings = fontDescription.variantSettings();
    auto textRenderingMode = fontDescription.textRenderingMode();
    auto shouldDisableLigaturesForSpacing = fontDescription.shouldDisableLigaturesForSpacing();

    // We might want to check fontType.trackingType == FontType::TrackingType::Manual here, but in order to maintain compatibility with the rest of the system, we don't.
    bool noFontFeatureSettings = features.isEmpty();
    bool noFontVariationSettings = !forceVariations && variations.isEmpty();
    bool textRenderingModeIsAuto = textRenderingMode == TextRenderingMode::AutoTextRendering;
    bool variantSettingsIsNormal = variantSettings.isAllNormal();
    bool dontNeedToApplyOpticalSizing = fontOpticalSizing == FontOpticalSizing::Enabled && !forceOpticalSizingOn;
    bool fontFaceDoesntSpecifyFeatures = !fontFaceFeatures || fontFaceFeatures->isEmpty();
    if (noFontFeatureSettings && noFontVariationSettings && textRenderingModeIsAuto && variantSettingsIsNormal && dontNeedToApplyOpticalSizing && fontFaceDoesntSpecifyFeatures && !shouldDisableLigaturesForSpacing) {
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
    if (applyWeightWidthSlopeVariations && !fontIsSystemFont(originalFont)) {
        float weight = fontSelectionRequest.weight;
        float width = fontSelectionRequest.width;
        float slope = fontSelectionRequest.slope.valueOr(normalItalicValue());
        if (auto weightValue = fontFaceCapabilities.weight)
            weight = std::max(std::min(weight, static_cast<float>(weightValue->maximum)), static_cast<float>(weightValue->minimum));
        if (auto widthValue = fontFaceCapabilities.width)
            width = std::max(std::min(width, static_cast<float>(widthValue->maximum)), static_cast<float>(widthValue->minimum));
        if (auto slopeValue = fontFaceCapabilities.weight)
            slope = std::max(std::min(slope, static_cast<float>(slopeValue->maximum)), static_cast<float>(slopeValue->minimum));
        if (needsConversion) {
            weight = denormalizeWeight(weight);
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
    if (fontFaceFeatures) {
        for (auto& fontFaceFeature : *fontFaceFeatures)
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

    auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    if (!featuresToBeApplied.isEmpty()) {
        auto featureArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, features.size(), &kCFTypeArrayCallBacks));
        for (auto& p : featuresToBeApplied) {
            auto feature = FontFeature(p.key, p.value);
            if (fontType.aatShaping)
                appendTrueTypeFeature(featureArray.get(), feature);
            if (fontType.openTypeShaping)
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

    addAttributesForInstalledFonts(attributes.get(), fontDescription.shouldAllowUserInstalledFonts());

    auto descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    return adoptCF(CTFontCreateCopyWithAttributes(originalFont, CTFontGetSize(originalFont), nullptr, descriptor.get()));
}

RefPtr<Font> FontCache::similarFont(const FontDescription& description, const AtomString& family)
{
    // Attempt to find an appropriate font using a match based on the presence of keywords in
    // the requested names. For example, we'll match any name that contains "Arabic" to Geeza Pro.
    if (family.isEmpty())
        return nullptr;

#if PLATFORM(IOS_FAMILY)
    // Substitute the default monospace font for well-known monospace fonts.
    if (equalLettersIgnoringASCIICase(family, "monaco") || equalLettersIgnoringASCIICase(family, "menlo")) {
        static MainThreadNeverDestroyed<const AtomString> courier("courier", AtomString::ConstructFromLiteral);
        return fontForFamily(description, courier);
    }

    // Substitute Verdana for Lucida Grande.
    if (equalLettersIgnoringASCIICase(family, "lucida grande")) {
        static MainThreadNeverDestroyed<const AtomString> verdana("verdana", AtomString::ConstructFromLiteral);
        return fontForFamily(description, verdana);
    }
#endif

    static NeverDestroyed<String> arabic(MAKE_STATIC_STRING_IMPL("Arabic"));
    static NeverDestroyed<String> pashto(MAKE_STATIC_STRING_IMPL("Pashto"));
    static NeverDestroyed<String> urdu(MAKE_STATIC_STRING_IMPL("Urdu"));
    static String* matchWords[3] = { &arabic.get(), &pashto.get(), &urdu.get() };
    static MainThreadNeverDestroyed<const AtomString> geezaPlain("GeezaPro", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> geezaBold("GeezaPro-Bold", AtomString::ConstructFromLiteral);
    for (String* matchWord : matchWords) {
        if (family.containsIgnoringASCIICase(*matchWord))
            return fontForFamily(description, isFontWeightBold(description.weight()) ? geezaBold : geezaPlain);
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

static void invalidateFontCache();

static void fontCacheRegisteredFontsChangedNotificationCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void *, CFDictionaryRef)
{
    ASSERT_UNUSED(observer, observer == &FontCache::singleton());

    invalidateFontCache();
}

void FontCache::platformInit()
{
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, &fontCacheRegisteredFontsChangedNotificationCallback, kCTFontManagerRegisteredFontsChangedNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);

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
        CFStringRef fontName = static_cast<CFStringRef>(CFArrayGetValueAtIndex(availableFontFamilies.get(), i));
        if (CFGetTypeID(fontName) != CFStringGetTypeID()) {
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
        if (shouldComputePhysicalTraits == ShouldComputePhysicalTraits::Yes) {
#if HAVE(CTFONTGETPHYSICALSYMBOLICTRAITS)
            actualTraits = CTFontGetPhysicalSymbolicTraits(font);
#else
            auto fontForSynthesisComputation = retainPtr(font);
            if (auto physicalFont = adoptCF(CTFontCopyPhysicalFont(font)))
                fontForSynthesisComputation = WTFMove(physicalFont);
            actualTraits = CTFontGetSymbolicTraits(fontForSynthesisComputation.get());
#endif
        } else
            actualTraits = CTFontGetSymbolicTraits(font);
    }

    bool needsSyntheticBold = (fontDescription.fontSynthesis() & FontSynthesisWeight) && (desiredTraits & kCTFontTraitBold) && !(actualTraits & kCTFontTraitBold);
    bool needsSyntheticOblique = (fontDescription.fontSynthesis() & FontSynthesisStyle) && (desiredTraits & kCTFontTraitItalic) && !(actualTraits & kCTFontTraitItalic);

    return SynthesisPair(needsSyntheticBold, needsSyntheticOblique);
}

typedef HashSet<String, ASCIICaseInsensitiveHash> Allowlist;
static Allowlist& fontAllowlist()
{
    static NeverDestroyed<Allowlist> allowlist;
    return allowlist;
}

void FontCache::setFontAllowlist(const Vector<String>& inputAllowlist)
{
    Allowlist& allowlist = fontAllowlist();
    allowlist.clear();
    for (auto& item : inputAllowlist)
        allowlist.add(item);
}

class FontDatabase {
public:
    static FontDatabase& singletonAllowingUserInstalledFonts()
    {
        static NeverDestroyed<FontDatabase> database(AllowUserInstalledFonts::Yes);
        return database;
    }

    static FontDatabase& singletonDisallowingUserInstalledFonts()
    {
        static NeverDestroyed<FontDatabase> database(AllowUserInstalledFonts::No);
        return database;
    }

    FontDatabase(const FontDatabase&) = delete;
    FontDatabase& operator=(const FontDatabase&) = delete;

    struct InstalledFont {
        InstalledFont() = default;

        InstalledFont(CTFontDescriptorRef fontDescriptor, AllowUserInstalledFonts allowUserInstalledFonts)
            : fontDescriptor(fontDescriptor)
            , capabilities(capabilitiesForFontDescriptor(fontDescriptor))
        {
#if HAVE(CTFONTCREATEFORCHARACTERSWITHLANGUAGEANDOPTION)
            UNUSED_PARAM(allowUserInstalledFonts);
#else
            if (allowUserInstalledFonts != AllowUserInstalledFonts::No)
                return;
            auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            addAttributesForInstalledFonts(attributes.get(), allowUserInstalledFonts);
            this->fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor, attributes.get()));
#endif
        }

        RetainPtr<CTFontDescriptorRef> fontDescriptor;
        FontSelectionCapabilities capabilities;
    };

    struct InstalledFontFamily {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        InstalledFontFamily() = default;

        explicit InstalledFontFamily(Vector<InstalledFont>&& installedFonts)
            : installedFonts(WTFMove(installedFonts))
        {
            for (auto& font : this->installedFonts)
                expand(font);
        }

        void expand(const InstalledFont& installedFont)
        {
            capabilities.expand(installedFont.capabilities);
        }

        bool isEmpty() const
        {
            return installedFonts.isEmpty();
        }

        size_t size() const
        {
            return installedFonts.size();
        }

        Vector<InstalledFont> installedFonts;
        FontSelectionCapabilities capabilities;
    };

    const InstalledFontFamily& collectionForFamily(const String& familyName)
    {
        auto folded = FontCascadeDescription::foldedFamilyName(familyName);
        {
            auto locker = holdLock(m_familyNameToFontDescriptorsLock);
            auto it = m_familyNameToFontDescriptors.find(folded);
            if (it != m_familyNameToFontDescriptors.end())
                return *it->value;
        }

        auto installedFontFamily = [&] {
            auto familyNameString = folded.createCFString();
            auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            CFDictionaryAddValue(attributes.get(), kCTFontFamilyNameAttribute, familyNameString.get());
            addAttributesForInstalledFonts(attributes.get(), m_allowUserInstalledFonts);
            auto fontDescriptorToMatch = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
            auto mandatoryAttributes = installedFontMandatoryAttributes(m_allowUserInstalledFonts);
            if (auto matches = adoptCF(CTFontDescriptorCreateMatchingFontDescriptors(fontDescriptorToMatch.get(), mandatoryAttributes.get()))) {
                auto count = CFArrayGetCount(matches.get());
                Vector<InstalledFont> result;
                result.reserveInitialCapacity(count);
                for (CFIndex i = 0; i < count; ++i) {
                    InstalledFont installedFont(static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(matches.get(), i)), m_allowUserInstalledFonts);
                    result.uncheckedAppend(WTFMove(installedFont));
                }
                return makeUnique<InstalledFontFamily>(WTFMove(result));
            }
            return makeUnique<InstalledFontFamily>();
        }();

        auto locker = holdLock(m_familyNameToFontDescriptorsLock);
        return *m_familyNameToFontDescriptors.add(folded.isolatedCopy(), WTFMove(installedFontFamily)).iterator->value;
    }

    const InstalledFont& fontForPostScriptName(const AtomString& postScriptName)
    {
        const auto& folded = FontCascadeDescription::foldedFamilyName(postScriptName);
        return m_postScriptNameToFontDescriptors.ensure(folded, [&] {
            auto postScriptNameString = folded.createCFString();
            CFStringRef nameAttribute = kCTFontPostScriptNameAttribute;
            auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            CFDictionaryAddValue(attributes.get(), kCTFontEnabledAttribute, kCFBooleanTrue);
            CFDictionaryAddValue(attributes.get(), nameAttribute, postScriptNameString.get());
            addAttributesForInstalledFonts(attributes.get(), m_allowUserInstalledFonts);
            auto fontDescriptorToMatch = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
            auto mandatoryAttributes = installedFontMandatoryAttributes(m_allowUserInstalledFonts);
            auto match = adoptCF(CTFontDescriptorCreateMatchingFontDescriptor(fontDescriptorToMatch.get(), mandatoryAttributes.get()));
            return InstalledFont(match.get(), m_allowUserInstalledFonts);
        }).iterator->value;
    }

    void clear()
    {
        {
            auto locker = holdLock(m_familyNameToFontDescriptorsLock);
            m_familyNameToFontDescriptors.clear();
        }
        m_postScriptNameToFontDescriptors.clear();
    }

private:
    friend class NeverDestroyed<FontDatabase>;

    FontDatabase(AllowUserInstalledFonts allowUserInstalledFonts)
        : m_allowUserInstalledFonts(allowUserInstalledFonts)
    {
    }

    Lock m_familyNameToFontDescriptorsLock;
    HashMap<String, std::unique_ptr<InstalledFontFamily>> m_familyNameToFontDescriptors;
    HashMap<String, InstalledFont> m_postScriptNameToFontDescriptors;
    AllowUserInstalledFonts m_allowUserInstalledFonts;
};

// Because this struct holds intermediate values which may be in the compressed -1 - 1 GX range, we don't want to use the relatively large
// quantization of FontSelectionValue. Instead, do this logic with floats.
struct MinMax {
    float minimum;
    float maximum;
};

struct VariationCapabilities {
    Optional<MinMax> weight;
    Optional<MinMax> width;
    Optional<MinMax> slope;
};

static Optional<MinMax> extractVariationBounds(CFDictionaryRef axis)
{
    CFNumberRef minimumValue = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisMinimumValueKey));
    CFNumberRef maximumValue = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisMaximumValueKey));
    float rawMinimumValue = 0;
    float rawMaximumValue = 0;
    CFNumberGetValue(minimumValue, kCFNumberFloatType, &rawMinimumValue);
    CFNumberGetValue(maximumValue, kCFNumberFloatType, &rawMaximumValue);
    if (rawMinimumValue < rawMaximumValue)
        return {{ rawMinimumValue, rawMaximumValue }};
    return WTF::nullopt;
}

static VariationCapabilities variationCapabilitiesForFontDescriptor(CTFontDescriptorRef fontDescriptor)
{
    VariationCapabilities result;

    if (!adoptCF(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontVariationAttribute)))
        return result;

    auto font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor, 0, nullptr));
    auto variations = adoptCF(CTFontCopyVariationAxes(font.get()));
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
            result.weight = {{ normalizeWeight(result.weight.value().minimum), normalizeWeight(result.weight.value().maximum) }};
        if (result.width)
            result.width = {{ normalizeVariationWidth(result.width.value().minimum), normalizeVariationWidth(result.width.value().maximum) }};
        if (result.slope)
            result.slope = {{ normalizeSlope(result.slope.value().minimum), normalizeSlope(result.slope.value().maximum) }};
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
    Vector<FontSelectionCapabilities> capabilities;
    capabilities.reserveInitialCapacity(familyFonts.size());
    for (auto& font : familyFonts.installedFonts)
        capabilities.uncheckedAppend(font.capabilities);
    FontSelectionAlgorithm fontSelectionAlgorithm(fontSelectionRequest, capabilities, familyFonts.capabilities);
    auto index = fontSelectionAlgorithm.indexOfBestCapabilities();
    if (index == notFound)
        return nullptr;

    return &familyFonts.installedFonts[index];
}

Vector<FontSelectionCapabilities> FontCache::getFontSelectionCapabilitiesInFamily(const AtomString& familyName, AllowUserInstalledFonts allowUserInstalledFonts)
{
    auto& fontDatabase = allowUserInstalledFonts == AllowUserInstalledFonts::Yes ? FontDatabase::singletonAllowingUserInstalledFonts() : FontDatabase::singletonDisallowingUserInstalledFonts();
    const auto& fonts = fontDatabase.collectionForFamily(familyName.string());
    if (fonts.isEmpty())
        return { };

    Vector<FontSelectionCapabilities> result;
    result.reserveInitialCapacity(fonts.size());
    for (const auto& font : fonts.installedFonts)
        result.uncheckedAppend(font.capabilities);
    return result;
}

struct FontLookup {
    RetainPtr<CTFontRef> result;
    bool createdFromPostScriptName { false };
};

static FontLookup platformFontLookupWithFamily(const AtomString& family, FontSelectionRequest request, float size, AllowUserInstalledFonts allowUserInstalledFonts)
{
    const auto& allowlist = fontAllowlist();
    if (!isSystemFont(family.string()) && allowlist.size() && !allowlist.contains(family))
        return { nullptr };

    if (equalLettersIgnoringASCIICase(family, ".applesystemuifontserif")
        || equalLettersIgnoringASCIICase(family, ".sf ns mono")
        || equalLettersIgnoringASCIICase(family, ".sf ui mono")
        || equalLettersIgnoringASCIICase(family, ".applesystemuifontrounded")) {
        // If you want to use these fonts, use ui-serif, ui-monospace, and ui-rounded.
        return { nullptr };
    }

    auto& fontDatabase = allowUserInstalledFonts == AllowUserInstalledFonts::Yes ? FontDatabase::singletonAllowingUserInstalledFonts() : FontDatabase::singletonDisallowingUserInstalledFonts();
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

static void invalidateFontCache()
{
    if (!isMainThread()) {
        callOnMainThread([] {
            invalidateFontCache();
        });
        return;
    }

    SystemFontDatabaseCoreText::singleton().clear();
    clearFontFamilySpecificationCoreTextCache();

    FontDatabase::singletonAllowingUserInstalledFonts().clear();
    FontDatabase::singletonDisallowingUserInstalledFonts().clear();

    FontCache::singleton().invalidate();
}

static RetainPtr<CTFontRef> fontWithFamilySpecialCase(const AtomString& family, const FontDescription& fontDescription, float size, AllowUserInstalledFonts allowUserInstalledFonts)
{
    // FIXME: See comment in FontCascadeDescription::effectiveFamilyAt() in FontDescriptionCocoa.cpp
    Optional<SystemFontKind> systemDesign;

#if HAVE(DESIGN_SYSTEM_UI_FONTS)
    if (equalLettersIgnoringASCIICase(family, "ui-serif"))
        systemDesign = SystemFontKind::UISerif;
    else if (equalLettersIgnoringASCIICase(family, "ui-monospace"))
        systemDesign = SystemFontKind::UIMonospace;
    else if (equalLettersIgnoringASCIICase(family, "ui-rounded"))
        systemDesign = SystemFontKind::UIRounded;
#endif

    if (equalLettersIgnoringASCIICase(family, "-webkit-system-font") || equalLettersIgnoringASCIICase(family, "-apple-system") || equalLettersIgnoringASCIICase(family, "-apple-system-font") || equalLettersIgnoringASCIICase(family, "system-ui") || equalLettersIgnoringASCIICase(family, "ui-sans-serif")) {
        ASSERT(!systemDesign);
        systemDesign = SystemFontKind::SystemUI;
    }

    if (systemDesign) {
        auto cascadeList = SystemFontDatabaseCoreText::singleton().cascadeList(fontDescription, family, *systemDesign, allowUserInstalledFonts);
        if (cascadeList.isEmpty())
            return nullptr;
        return createFontForInstalledFonts(cascadeList[0].get(), size, allowUserInstalledFonts);
    }

    if (family.startsWith("UICTFontTextStyle")) {
        const auto& request = fontDescription.fontSelectionRequest();
        CTFontSymbolicTraits traits = (isFontWeightBold(request.weight) || FontCache::singleton().shouldMockBoldSystemFontForAccessibility() ? kCTFontTraitBold : 0) | (isItalic(request.slope) ? kCTFontTraitItalic : 0);
        auto descriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(family.string().createCFString().get(), RenderThemeCocoa::singleton().contentSizeCategory(), fontDescription.computedLocale().string().createCFString().get()));
        if (traits)
            descriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(descriptor.get(), traits, traits));
        return createFontForInstalledFonts(descriptor.get(), size, allowUserInstalledFonts);
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-menu")) {
        auto result = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontMenuItem, size, fontDescription.computedLocale().string().createCFString().get()));
        return createFontForInstalledFonts(result.get(), allowUserInstalledFonts);
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-status-bar")) {
        auto result = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, fontDescription.computedLocale().string().createCFString().get()));
        return createFontForInstalledFonts(result.get(), allowUserInstalledFonts);
    }

    if (equalLettersIgnoringASCIICase(family, "lastresort")) {
        static const CTFontDescriptorRef lastResort = CTFontDescriptorCreateLastResort();
        return adoptCF(CTFontCreateWithFontDescriptor(lastResort, size, nullptr));
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-system-monospaced-numbers")) {
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

static RetainPtr<CTFontRef> fontWithFamily(const AtomString& family, const FontDescription& fontDescription, const FontFeatureSettings* fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities, float size)
{
    if (family.isEmpty())
        return nullptr;

    FontLookup fontLookup;
    fontLookup.result = fontWithFamilySpecialCase(family, fontDescription, size, fontDescription.shouldAllowUserInstalledFonts());
    if (!fontLookup.result)
        fontLookup = platformFontLookupWithFamily(family, fontDescription.fontSelectionRequest(), size, fontDescription.shouldAllowUserInstalledFonts());
    return preparePlatformFont(fontLookup.result.get(), fontDescription, fontFaceFeatures, fontFaceCapabilities, !fontLookup.createdFromPostScriptName);
}

#if PLATFORM(MAC)
static bool shouldAutoActivateFontIfNeeded(const AtomString& family)
{
#ifndef NDEBUG
    // This cache is not thread safe so the following assertion is there to
    // make sure this function is always called from the same thread.
    static Thread* initThread = &Thread::current();
    ASSERT(initThread == &Thread::current());
#endif

    static NeverDestroyed<HashSet<AtomString>> knownFamilies;
    static const unsigned maxCacheSize = 128;
    ASSERT(knownFamilies.get().size() <= maxCacheSize);
    if (knownFamilies.get().size() == maxCacheSize)
        knownFamilies.get().remove(knownFamilies.get().random());

    // Only attempt to auto-activate fonts once for performance reasons.
    return knownFamilies.get().add(family).isNewEntry;
}

static void autoActivateFont(const String& name, CGFloat size)
{
    auto fontName = name.createCFString();
    CFTypeRef keys[] = { kCTFontNameAttribute };
    CFTypeRef values[] = { fontName.get() };
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    if (auto newFont = CTFontCreateWithFontDescriptor(descriptor.get(), size, nullptr))
        CFRelease(newFont);
}
#endif

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomString& family, const FontFeatureSettings* fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities)
{
    float size = fontDescription.computedPixelSize();

    auto font = fontWithFamily(family, fontDescription, fontFaceFeatures, fontFaceCapabilities, size);

#if PLATFORM(MAC)
    if (!font) {
        if (!shouldAutoActivateFontIfNeeded(family))
            return nullptr;

        // Auto activate the font before looking for it a second time.
        // Ignore the result because we want to use our own algorithm to actually find the font.
        autoActivateFont(family.string(), size);

        font = fontWithFamily(family, fontDescription, fontFaceFeatures, fontFaceCapabilities, size);
    }
#endif

    if (!font)
        return nullptr;

    if (fontDescription.shouldAllowUserInstalledFonts() == AllowUserInstalledFonts::No)
        m_seenFamiliesForPrewarming.add(FontCascadeDescription::foldedFamilyName(family));

    auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(font.get(), fontDescription).boldObliquePair();

    return makeUnique<FontPlatformData>(font.get(), size, syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());
}

typedef HashSet<RetainPtr<CTFontRef>, WTF::RetainPtrObjectHash<CTFontRef>, WTF::RetainPtrObjectHashTraits<CTFontRef>> FallbackDedupSet;
static FallbackDedupSet& fallbackDedupSet()
{
    static NeverDestroyed<FallbackDedupSet> dedupSet;
    return dedupSet.get();
}

void FontCache::platformPurgeInactiveFontData()
{
    Vector<CTFontRef> toRemove;
    for (auto& font : fallbackDedupSet()) {
        if (CFGetRetainCount(font.get()) == 1)
            toRemove.append(font.get());
    }
    for (auto& font : toRemove)
        fallbackDedupSet().remove(font);

    FontDatabase::singletonAllowingUserInstalledFonts().clear();
    FontDatabase::singletonDisallowingUserInstalledFonts().clear();
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
    auto attribute = adoptCF(static_cast<CFBooleanRef>(CTFontCopyAttribute(font, kCTFontUserInstalledAttribute)));
    return attribute && CFBooleanGetValue(attribute.get());
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

RefPtr<Font> FontCache::systemFallbackForCharacters(const FontDescription& description, const Font* originalFontData, IsForPlatformFont isForPlatformFont, PreferColoredFont, const UChar* characters, unsigned length)
{
    const FontPlatformData& platformData = originalFontData->platformData();

    auto fullName = String(adoptCF(CTFontCopyFullName(platformData.font())).get());
    if (!fullName.isEmpty())
        m_fontNamesRequiringSystemFallbackForPrewarming.add(fullName);

    auto result = lookupFallbackFont(platformData.font(), description.weight(), description.computedLocale(), description.shouldAllowUserInstalledFonts(), characters, length);
    result = preparePlatformFont(result.get(), description, nullptr, { });

    if (!result)
        return lastResortFallbackFont(description);

    // FontCascade::drawGlyphBuffer() requires that there are no duplicate Font objects which refer to the same thing. This is enforced in
    // FontCache::fontForPlatformData(), where our equality check is based on hashing the FontPlatformData, whose hash includes the raw CoreText
    // font pointer.
    CTFontRef substituteFont = fallbackDedupSet().add(result).iterator->get();

    auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(substituteFont, description, ShouldComputePhysicalTraits::No, isForPlatformFont == IsForPlatformFont::Yes).boldObliquePair();

    FontPlatformData alternateFont(substituteFont, platformData.size(), syntheticBold, syntheticOblique, platformData.orientation(), platformData.widthVariant(), platformData.textRenderingMode());

    return fontForPlatformData(alternateFont);
}

const AtomString& FontCache::platformAlternateFamilyName(const AtomString& familyName)
{
    static const UChar heitiString[] = { 0x9ed1, 0x4f53 };
    static const UChar songtiString[] = { 0x5b8b, 0x4f53 };
    static const UChar weiruanXinXiMingTi[] = { 0x5fae, 0x8edf, 0x65b0, 0x7d30, 0x660e, 0x9ad4 };
    static const UChar weiruanYaHeiString[] = { 0x5fae, 0x8f6f, 0x96c5, 0x9ed1 };
    static const UChar weiruanZhengHeitiString[] = { 0x5fae, 0x8edf, 0x6b63, 0x9ed1, 0x9ad4 };

    static MainThreadNeverDestroyed<const AtomString> songtiSC("Songti SC", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> songtiTC("Songti TC", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> heitiSCReplacement("PingFang SC", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> heitiTCReplacement("PingFang TC", AtomString::ConstructFromLiteral);

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
        if (equalLettersIgnoringASCIICase(familyName, "simsun"))
            return songtiSC;
        if (equal(familyName, weiruanXinXiMingTi))
            return songtiTC;
        break;
    case 10:
        if (equalLettersIgnoringASCIICase(familyName, "ms mingliu"))
            return songtiTC;
        if (equalIgnoringASCIICase(familyName, "\\5b8b\\4f53"))
            return songtiSC;
        break;
    case 18:
        if (equalLettersIgnoringASCIICase(familyName, "microsoft jhenghei"))
            return heitiTCReplacement;
        break;
    }

    return nullAtom();
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
    if (auto result = fontForFamily(fontDescription, AtomString("Times", AtomString::ConstructFromLiteral)))
        return *result;

    // LastResort is guaranteed to be non-null.
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    auto fontDescriptor = adoptCF(CTFontDescriptorCreateLastResort());
    auto font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), fontDescription.computedPixelSize(), nullptr));
#else
    // Even if Helvetica doesn't exist, CTFontCreateWithName will return
    // a thin wrapper around a GraphicsFont which represents LastResort.
    auto font = adoptCF(CTFontCreateWithName(CFSTR("Helvetica"), fontDescription.computedPixelSize(), nullptr));
#endif
    auto [syntheticBold, syntheticOblique] = computeNecessarySynthesis(font.get(), fontDescription).boldObliquePair();
    FontPlatformData platformData(font.get(), fontDescription.computedPixelSize(), syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());
    return fontForPlatformData(platformData);
}

FontCache::PrewarmInformation FontCache::collectPrewarmInformation() const
{
    return { copyToVector(m_seenFamiliesForPrewarming), copyToVector(m_fontNamesRequiringSystemFallbackForPrewarming) };
}

void FontCache::prewarm(const PrewarmInformation& prewarmInformation)
{
    if (prewarmInformation.isEmpty())
        return;

    if (!m_prewarmQueue)
        m_prewarmQueue = WorkQueue::create("WebKit font prewarm queue");

    auto& database = FontDatabase::singletonDisallowingUserInstalledFonts();

    m_prewarmQueue->dispatch([&database, prewarmInformation = prewarmInformation.isolatedCopy()] {
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
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure())
        return;

    Vector<String> families = std::initializer_list<String> {
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
    FontCache::singleton().prewarm(prewarmInfo);
}

}
