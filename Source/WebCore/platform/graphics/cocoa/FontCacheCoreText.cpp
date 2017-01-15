/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "CoreTextSPI.h"
#include "Font.h"

#include <CoreText/SFNTLayoutTypes.h>

#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

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
#if ENABLE(VARIATION_FONTS)
typedef HashMap<FontTag, float, FourCharacterTagHash, FourCharacterTagHashTraits> VariationsMap;
#endif

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

#if ENABLE(VARIATION_FONTS)
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

        // FIXME: Remove when <rdar://problem/28893836> is fixed
#define WORKAROUND_CORETEXT_VARIATIONS_EXTENTS_BUG (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < 110000)
#if WORKAROUND_CORETEXT_VARIATIONS_EXTENTS_BUG
        float epsilon = 0.001;
        rawMinimumValue += epsilon;
        rawMaximumValue -= epsilon;
#endif
#undef WORKAROUND_CORETEXT_VARIATIONS_EXTENTS_BUG

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
#endif

RetainPtr<CTFontRef> preparePlatformFont(CTFontRef originalFont, TextRenderingMode textRenderingMode, const FontFeatureSettings* fontFaceFeatures, const FontVariantSettings* fontFaceVariantSettings, const FontFeatureSettings& features, const FontVariantSettings& variantSettings, const FontVariationSettings& variations)
{
    if (!originalFont || (!features.size() && variations.isEmpty() && (textRenderingMode == AutoTextRendering) && variantSettings.isAllNormal()
        && (!fontFaceFeatures || !fontFaceFeatures->size()) && (!fontFaceVariantSettings || fontFaceVariantSettings->isAllNormal())))
        return originalFont;

    // This algorithm is described at http://www.w3.org/TR/css3-fonts/#feature-precedence
    FeaturesMap featuresToBeApplied;

    // Step 1: CoreText handles default features (such as required ligatures).

    // Step 2: Consult with font-variant-* inside @font-face
    if (fontFaceVariantSettings)
        featuresToBeApplied = computeFeatureSettingsFromVariants(*fontFaceVariantSettings);

    // Step 3: Consult with font-feature-settings inside @font-face
    if (fontFaceFeatures) {
        for (auto& fontFaceFeature : *fontFaceFeatures)
            featuresToBeApplied.set(fontFaceFeature.tag(), fontFaceFeature.value());
    }

    // Step 4: Font-variant
    for (auto& newFeature : computeFeatureSettingsFromVariants(variantSettings))
        featuresToBeApplied.set(newFeature.key, newFeature.value);

    // Step 5: Other properties (text-rendering)
    if (textRenderingMode == OptimizeSpeed) {
        featuresToBeApplied.set(fontFeatureTag("liga"), 0);
        featuresToBeApplied.set(fontFeatureTag("clig"), 0);
        featuresToBeApplied.set(fontFeatureTag("dlig"), 0);
        featuresToBeApplied.set(fontFeatureTag("hlig"), 0);
        featuresToBeApplied.set(fontFeatureTag("calt"), 0);
    }

    // Step 6: Font-feature-settings
    for (auto& newFeature : features)
        featuresToBeApplied.set(newFeature.tag(), newFeature.value());

#if ENABLE(VARIATION_FONTS)
    auto defaultValues = defaultVariationValues(originalFont);

    VariationsMap variationsToBeApplied;
    for (auto& newVariation : variations) {
        auto iterator = defaultValues.find(newVariation.tag());
        if (iterator != defaultValues.end()) {
            float valueToApply = std::max(std::min(newVariation.value(), iterator->value.maximumValue), iterator->value.minimumValue);

            // FIXME: Remove when <rdar://problem/28707822> is fixed
#define WORKAROUND_CORETEXT_VARIATIONS_DEFAULT_VALUE_BUG (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < 110000)
#if WORKAROUND_CORETEXT_VARIATIONS_DEFAULT_VALUE_BUG
            if (valueToApply == iterator->value.defaultValue)
                valueToApply += 0.0001;
#endif
#undef WORKAROUND_CORETEXT_VARIATIONS_DEFAULT_VALUE_BUG

            variationsToBeApplied.set(newVariation.tag(), valueToApply);
        }
    }
#endif

    auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    if (!featuresToBeApplied.isEmpty()) {
        auto featureArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, features.size(), &kCFTypeArrayCallBacks));
        for (auto& p : featuresToBeApplied) {
            auto feature = FontFeature(p.key, p.value);
            appendTrueTypeFeature(featureArray.get(), feature);
            appendOpenTypeFeature(featureArray.get(), feature);
        }
        CFDictionaryAddValue(attributes.get(), kCTFontFeatureSettingsAttribute, featureArray.get());
    }

#if ENABLE(VARIATION_FONTS)
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
#endif

    if (textRenderingMode == OptimizeLegibility) {
        CGFloat size = CTFontGetSize(originalFont);
        auto sizeNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &size));
        CFDictionaryAddValue(attributes.get(), kCTFontOpticalSizeAttribute, sizeNumber.get());
    }
    auto descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    auto result = adoptCF(CTFontCreateCopyWithAttributes(originalFont, CTFontGetSize(originalFont), nullptr, descriptor.get()));
    return result;
}

FontWeight fontWeightFromCoreText(CGFloat weight)
{
    if (weight < -0.6)
        return FontWeight100;
    if (weight < -0.365)
        return FontWeight200;
    if (weight < -0.115)
        return FontWeight300;
    if (weight <  0.130)
        return FontWeight400;
    if (weight <  0.235)
        return FontWeight500;
    if (weight <  0.350)
        return FontWeight600;
    if (weight <  0.500)
        return FontWeight700;
    if (weight <  0.700)
        return FontWeight800;
    return FontWeight900;
}

static inline FontTraitsMask toTraitsMask(CTFontSymbolicTraits ctFontTraits, CGFloat weight)
{
    FontTraitsMask weightMask;
    switch (fontWeightFromCoreText(weight)) {
    case FontWeight100:
        weightMask = FontWeight100Mask;
        break;
    case FontWeight200:
        weightMask = FontWeight200Mask;
        break;
    case FontWeight300:
        weightMask = FontWeight300Mask;
        break;
    case FontWeight400:
        weightMask = FontWeight400Mask;
        break;
    case FontWeight500:
        weightMask = FontWeight500Mask;
        break;
    case FontWeight600:
        weightMask = FontWeight600Mask;
        break;
    case FontWeight700:
        weightMask = FontWeight700Mask;
        break;
    case FontWeight800:
        weightMask = FontWeight800Mask;
        break;
    case FontWeight900:
        weightMask = FontWeight900Mask;
        break;
    }
    return static_cast<FontTraitsMask>(((ctFontTraits & kCTFontTraitItalic) ? FontStyleItalicMask : FontStyleNormalMask) | weightMask);
}

bool isFontWeightBold(FontWeight fontWeight)
{
    return fontWeight >= FontWeight600;
}

uint16_t toCoreTextFontWeight(FontWeight fontWeight)
{
    static const int coreTextFontWeights[] = {
        100, // FontWeight100
        200, // FontWeight200
        300, // FontWeight300
        400, // FontWeight400
        500, // FontWeight500
        600, // FontWeight600
        700, // FontWeight700
        800, // FontWeight800
        900, // FontWeight900
    };
    return coreTextFontWeights[fontWeight];
}

RefPtr<Font> FontCache::similarFont(const FontDescription& description, const AtomicString& family)
{
    // Attempt to find an appropriate font using a match based on the presence of keywords in
    // the requested names. For example, we'll match any name that contains "Arabic" to Geeza Pro.
    if (family.isEmpty())
        return nullptr;

#if PLATFORM(IOS)
    // Substitute the default monospace font for well-known monospace fonts.
    if (equalLettersIgnoringASCIICase(family, "monaco") || equalLettersIgnoringASCIICase(family, "menlo")) {
        static NeverDestroyed<AtomicString> courier("courier", AtomicString::ConstructFromLiteral);
        return fontForFamily(description, courier);
    }

    // Substitute Verdana for Lucida Grande.
    if (equalLettersIgnoringASCIICase(family, "lucida grande")) {
        static NeverDestroyed<AtomicString> verdana("verdana", AtomicString::ConstructFromLiteral);
        return fontForFamily(description, verdana);
    }
#endif

    static NeverDestroyed<String> arabic(ASCIILiteral("Arabic"));
    static NeverDestroyed<String> pashto(ASCIILiteral("Pashto"));
    static NeverDestroyed<String> urdu(ASCIILiteral("Urdu"));
    static String* matchWords[3] = { &arabic.get(), &pashto.get(), &urdu.get() };
    static NeverDestroyed<AtomicString> geezaPlain("GeezaPro", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> geezaBold("GeezaPro-Bold", AtomicString::ConstructFromLiteral);
    for (String* matchWord : matchWords) {
        if (family.contains(*matchWord, false))
            return fontForFamily(description, isFontWeightBold(description.weight()) ? geezaBold : geezaPlain);
    }
    return nullptr;
}

Vector<FontTraitsMask> FontCache::getTraitsInFamily(const AtomicString& familyName)
{
    auto familyNameStr = familyName.string().createCFString();
    CFTypeRef keys[] = { kCTFontFamilyNameAttribute };
    CFTypeRef values[] = { familyNameStr.get() };
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    auto matchedDescriptors = adoptCF(CTFontDescriptorCreateMatchingFontDescriptors(fontDescriptor.get(), nullptr));
    if (!matchedDescriptors)
        return { };

    CFIndex numMatches = CFArrayGetCount(matchedDescriptors.get());
    if (!numMatches)
        return { };

    Vector<FontTraitsMask> traitsMasks;
    traitsMasks.reserveInitialCapacity(numMatches);
    for (CFIndex i = 0; i < numMatches; ++i) {
        auto traits = adoptCF((CFDictionaryRef)CTFontDescriptorCopyAttribute((CTFontDescriptorRef)CFArrayGetValueAtIndex(matchedDescriptors.get(), i), kCTFontTraitsAttribute));
        CFNumberRef resultRef = (CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontSymbolicTrait);
        CFNumberRef weightRef = (CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontWeightTrait);
        if (resultRef && weightRef) {
            CTFontSymbolicTraits symbolicTraits;
            CFNumberGetValue(resultRef, kCFNumberIntType, &symbolicTraits);
            CGFloat weight = 0;
            CFNumberGetValue(weightRef, kCFNumberCGFloatType, &weight);
            traitsMasks.uncheckedAppend(toTraitsMask(symbolicTraits, weight));
        }
    }
    return traitsMasks;
}

static void invalidateFontCache()
{
    if (!isMainThread()) {
        callOnMainThread([] {
            invalidateFontCache();
        });
        return;
    }

    FontCache::singleton().invalidate();

    platformInvalidateFontCache();
}

static void fontCacheRegisteredFontsChangedNotificationCallback(CFNotificationCenterRef, void* observer, CFStringRef name, const void *, CFDictionaryRef)
{
    ASSERT_UNUSED(observer, observer == &FontCache::singleton());
    ASSERT_UNUSED(name, CFEqual(name, kCTFontManagerRegisteredFontsChangedNotification));

    invalidateFontCache();
}

void FontCache::platformInit()
{
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, fontCacheRegisteredFontsChangedNotificationCallback, kCTFontManagerRegisteredFontsChangedNotification, 0, CFNotificationSuspensionBehaviorDeliverImmediately);
}

Vector<String> FontCache::systemFontFamilies()
{
    // FIXME: <rdar://problem/21890188>
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto emptyFontDescriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    auto matchedDescriptors = adoptCF(CTFontDescriptorCreateMatchingFontDescriptors(emptyFontDescriptor.get(), nullptr));
    if (!matchedDescriptors)
        return { };

    CFIndex numMatches = CFArrayGetCount(matchedDescriptors.get());
    if (!numMatches)
        return { };

    HashSet<String> visited;
    for (CFIndex i = 0; i < numMatches; ++i) {
        auto fontDescriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(matchedDescriptors.get(), i));
        if (auto familyName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontFamilyNameAttribute))))
            visited.add(familyName.get());
    }

    Vector<String> result;
    copyToVector(visited, result);
    return result;
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

SynthesisPair computeNecessarySynthesis(CTFontRef font, const FontDescription& fontDescription, bool isPlatformFont)
{
#if PLATFORM(IOS)
    if (CTFontIsAppleColorEmoji(font))
        return SynthesisPair(false, false);
#endif

    if (isPlatformFont)
        return SynthesisPair(false, false);

    CTFontSymbolicTraits desiredTraits = computeTraits(fontDescription);

    CTFontSymbolicTraits actualTraits = 0;
    if (isFontWeightBold(fontDescription.weight()) || fontDescription.italic())
        actualTraits = CTFontGetSymbolicTraits(font);

    bool needsSyntheticBold = (fontDescription.fontSynthesis() & FontSynthesisWeight) && (desiredTraits & kCTFontTraitBold) && !(actualTraits & kCTFontTraitBold);
    bool needsSyntheticOblique = (fontDescription.fontSynthesis() & FontSynthesisStyle) && (desiredTraits & kCTFontTraitItalic) && !(actualTraits & kCTFontTraitItalic);

    return SynthesisPair(needsSyntheticBold, needsSyntheticOblique);
}

typedef HashSet<String, ASCIICaseInsensitiveHash> Whitelist;
static Whitelist& fontWhitelist()
{
    static NeverDestroyed<Whitelist> whitelist;
    return whitelist;
}

void FontCache::setFontWhitelist(const Vector<String>& inputWhitelist)
{
    Whitelist& whitelist = fontWhitelist();
    whitelist.clear();
    for (auto& item : inputWhitelist)
        whitelist.add(item);
}

#if ENABLE(PLATFORM_FONT_LOOKUP)
static bool isSystemFont(const AtomicString& family)
{
    return family.length() >= 1 && family[0] == '.';
}

static RetainPtr<CTFontRef> platformFontLookupWithFamily(const AtomicString& family, CTFontSymbolicTraits requestedTraits, FontWeight weight, float size)
{
    const auto& whitelist = fontWhitelist();
    if (!isSystemFont(family) && whitelist.size() && !whitelist.contains(family))
        return nullptr;

    return adoptCF(CTFontCreateForCSS(family.string().createCFString().get(), toCoreTextFontWeight(weight), requestedTraits, size));
}
#endif

static RetainPtr<CTFontRef> fontWithFamily(const AtomicString& family, CTFontSymbolicTraits desiredTraits, FontWeight weight, const FontFeatureSettings& featureSettings, const FontVariantSettings& variantSettings, const FontVariationSettings& variationSettings, const FontFeatureSettings* fontFaceFeatures, const FontVariantSettings* fontFaceVariantSettings, const TextRenderingMode& textRenderingMode, float size)
{
    if (family.isEmpty())
        return nullptr;

    auto foundFont = platformFontWithFamilySpecialCase(family, weight, desiredTraits, size);
    if (!foundFont) {
#if ENABLE(PLATFORM_FONT_LOOKUP)
        foundFont = platformFontLookupWithFamily(family, desiredTraits, weight, size);
#else
        foundFont = platformFontWithFamily(family, desiredTraits, weight, textRenderingMode, size);
#endif
    }
    return preparePlatformFont(foundFont.get(), textRenderingMode, fontFaceFeatures, fontFaceVariantSettings, featureSettings, variantSettings, variationSettings);
}

#if PLATFORM(MAC)
static bool shouldAutoActivateFontIfNeeded(const AtomicString& family)
{
#ifndef NDEBUG
    // This cache is not thread safe so the following assertion is there to
    // make sure this function is always called from the same thread.
    static ThreadIdentifier initThreadId = currentThread();
    ASSERT(currentThread() == initThreadId);
#endif

    static NeverDestroyed<HashSet<AtomicString>> knownFamilies;
    static const unsigned maxCacheSize = 128;
    ASSERT(knownFamilies.get().size() <= maxCacheSize);
    if (knownFamilies.get().size() == maxCacheSize)
        knownFamilies.get().remove(knownFamilies.get().begin());

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

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family, const FontFeatureSettings* fontFaceFeatures, const FontVariantSettings* fontFaceVariantSettings)
{
    CTFontSymbolicTraits traits = computeTraits(fontDescription);
    float size = fontDescription.computedPixelSize();

    auto font = fontWithFamily(family, traits, fontDescription.weight(), fontDescription.featureSettings(), fontDescription.variantSettings(), fontDescription.variationSettings(), fontFaceFeatures, fontFaceVariantSettings, fontDescription.textRenderingMode(), size);

#if PLATFORM(MAC)
    if (!font) {
        if (!shouldAutoActivateFontIfNeeded(family))
            return nullptr;

        // Auto activate the font before looking for it a second time.
        // Ignore the result because we want to use our own algorithm to actually find the font.
        autoActivateFont(family.string(), size);

        font = fontWithFamily(family, traits, fontDescription.weight(), fontDescription.featureSettings(), fontDescription.variantSettings(), fontDescription.variationSettings(), fontFaceFeatures, fontFaceVariantSettings, fontDescription.textRenderingMode(), size);
    }
#endif

    if (!font)
        return nullptr;

    bool syntheticBold, syntheticOblique;
    std::tie(syntheticBold, syntheticOblique) = computeNecessarySynthesis(font.get(), fontDescription).boldObliquePair();

    return std::make_unique<FontPlatformData>(font.get(), size, syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());
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
}

#if PLATFORM(IOS)
static inline bool isArabicCharacter(UChar character)
{
    return character >= 0x0600 && character <= 0x06FF;
}
#endif

static RetainPtr<CTFontRef> lookupFallbackFont(CTFontRef font, FontWeight fontWeight, const AtomicString& locale, const UChar* characters, unsigned length)
{
    ASSERT(length > 0);

    RetainPtr<CFStringRef> localeString;
#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200)
    if (!locale.isNull())
        localeString = locale.string().createCFString();
#else
    UNUSED_PARAM(locale);
#endif

    CFIndex coveredLength = 0;
    auto result = adoptCF(CTFontCreateForCharactersWithLanguage(font, characters, length, localeString.get(), &coveredLength));

#if PLATFORM(IOS)
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

RefPtr<Font> FontCache::systemFallbackForCharacters(const FontDescription& description, const Font* originalFontData, bool isPlatformFont, const UChar* characters, unsigned length)
{
#if PLATFORM(IOS)
    if (length && requiresCustomFallbackFont(*characters)) {
        auto* fallback = getCustomFallbackFont(*characters, description);
        if (!fallback)
            return nullptr;
        return fontForPlatformData(*fallback);
    }
#endif

    const FontPlatformData& platformData = originalFontData->platformData();
    auto result = lookupFallbackFont(platformData.font(), description.weight(), description.locale(), characters, length);
    result = preparePlatformFont(result.get(), description.textRenderingMode(), nullptr, nullptr, description.featureSettings(), description.variantSettings(), description.variationSettings());
    if (!result)
        return lastResortFallbackFont(description);

    // FontCascade::drawGlyphBuffer() requires that there are no duplicate Font objects which refer to the same thing. This is enforced in
    // FontCache::fontForPlatformData(), where our equality check is based on hashing the FontPlatformData, whose hash includes the raw CoreText
    // font pointer.
    CTFontRef substituteFont = fallbackDedupSet().add(result).iterator->get();

    bool syntheticBold, syntheticOblique;
    std::tie(syntheticBold, syntheticOblique) = computeNecessarySynthesis(substituteFont, description, isPlatformFont).boldObliquePair();

    FontPlatformData alternateFont(substituteFont, platformData.size(), syntheticBold, syntheticOblique, platformData.orientation(), platformData.widthVariant(), platformData.textRenderingMode());

    return fontForPlatformData(alternateFont);
}

const AtomicString& FontCache::platformAlternateFamilyName(const AtomicString& familyName)
{
    static const UChar heitiString[] = { 0x9ed1, 0x4f53 };
    static const UChar songtiString[] = { 0x5b8b, 0x4f53 };
    static const UChar weiruanXinXiMingTi[] = { 0x5fae, 0x8edf, 0x65b0, 0x7d30, 0x660e, 0x9ad4 };
    static const UChar weiruanYaHeiString[] = { 0x5fae, 0x8f6f, 0x96c5, 0x9ed1 };
    static const UChar weiruanZhengHeitiString[] = { 0x5fae, 0x8edf, 0x6b63, 0x9ed1, 0x9ad4 };

    static NeverDestroyed<AtomicString> songtiSC("Songti SC", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> songtiTC("Songti TC", AtomicString::ConstructFromLiteral);
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101100
    static NeverDestroyed<AtomicString> heitiSCReplacement("Heiti SC", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> heitiTCReplacement("Heiti TC", AtomicString::ConstructFromLiteral);
#else
    static NeverDestroyed<AtomicString> heitiSCReplacement("PingFang SC", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> heitiTCReplacement("PingFang TC", AtomicString::ConstructFromLiteral);
#endif

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
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101100
    case 15:
        if (equalLettersIgnoringASCIICase(familyName, "microsoft yahei"))
            return heitiSCReplacement;
        break;
#endif
    case 18:
        if (equalLettersIgnoringASCIICase(familyName, "microsoft jhenghei"))
            return heitiTCReplacement;
        break;
    }

    return nullAtom;
}

}
