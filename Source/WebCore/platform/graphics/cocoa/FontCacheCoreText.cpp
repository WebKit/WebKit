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

#include <CoreText/SFNTLayoutTypes.h>

#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static inline void appendRawTrueTypeFeature(CFMutableArrayRef features, int type, int selector)
{
    RetainPtr<CFNumberRef> typeNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &type));
    RetainPtr<CFNumberRef> selectorNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &selector));
    CFTypeRef featureKeys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    CFTypeRef featureValues[] = { typeNumber.get(), selectorNumber.get() };
    RetainPtr<CFDictionaryRef> feature = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, featureKeys, featureValues, WTF_ARRAY_LENGTH(featureKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFArrayAppendValue(features, feature.get());
}

static inline bool tagEquals(FontFeatureTag tag, const char comparison[4]) 
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
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 80000)
    RetainPtr<CFStringRef> featureKey = adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(feature.tag().data()), feature.tag().size() * sizeof(FontFeatureTag::value_type), kCFStringEncodingASCII, false));
    int rawFeatureValue = feature.value();
    RetainPtr<CFNumberRef> featureValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawFeatureValue));
    CFStringRef featureDictionaryKeys[] = { kCTFontOpenTypeFeatureTag, kCTFontOpenTypeFeatureValue };
    CFTypeRef featureDictionaryValues[] = { featureKey.get(), featureValue.get() };
    RetainPtr<CFDictionaryRef> featureDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, (const void**)featureDictionaryKeys, featureDictionaryValues, WTF_ARRAY_LENGTH(featureDictionaryValues), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFArrayAppendValue(features, featureDictionary.get());
#else
    UNUSED_PARAM(features);
    UNUSED_PARAM(feature);
#endif
}

typedef HashMap<FontFeatureTag, int, FontFeatureTagHash, FontFeatureTagHashTraits> FeaturesMap;

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

RetainPtr<CTFontRef> applyFontFeatureSettings(CTFontRef originalFont, const FontFeatureSettings* fontFaceFeatures, const FontVariantSettings* fontFaceVariantSettings, const FontFeatureSettings& features, const FontVariantSettings& variantSettings)
{
    if (!originalFont || (!features.size() && variantSettings.isAllNormal()
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

    // Step 5: Other properties

    // Step 6: Font-feature-settings
    for (auto& newFeature : features)
        featuresToBeApplied.set(newFeature.tag(), newFeature.value());

    if (!featuresToBeApplied.size())
        return originalFont;

    RetainPtr<CFMutableArrayRef> featureArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, features.size(), &kCFTypeArrayCallBacks));
    for (auto& p : featuresToBeApplied) {
        auto feature = FontFeature(p.key, p.value);
        appendTrueTypeFeature(featureArray.get(), feature);
        appendOpenTypeFeature(featureArray.get(), feature);
    }
    CFArrayRef featureArrayPtr = featureArray.get();
    RetainPtr<CFDictionaryRef> dictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, (const void**)&kCTFontFeatureSettingsAttribute, (const void**)&featureArrayPtr, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    RetainPtr<CTFontDescriptorRef> descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(dictionary.get()));
    return adoptCF(CTFontCreateCopyWithAttributes(originalFont, CTFontGetSize(originalFont), nullptr, descriptor.get()));
}

}
