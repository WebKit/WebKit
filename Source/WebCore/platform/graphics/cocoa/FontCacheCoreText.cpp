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
#include <CoreText/SFNTLayoutTypes.h>

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

static inline void appendTrueTypeFeature(CFMutableArrayRef features, const FontFeature& feature)
{
    if (equalIgnoringASCIICase(feature.tag(), "liga") || equalIgnoringASCIICase(feature.tag(), "clig")) {
        if (feature.enabled()) {
            appendRawTrueTypeFeature(features, kLigaturesType, kCommonLigaturesOnSelector);
            appendRawTrueTypeFeature(features, kLigaturesType, kContextualLigaturesOnSelector);
        } else {
            appendRawTrueTypeFeature(features, kLigaturesType, kCommonLigaturesOffSelector);
            appendRawTrueTypeFeature(features, kLigaturesType, kContextualLigaturesOffSelector);
        }
    } else if (equalIgnoringASCIICase(feature.tag(), "dlig")) {
        if (feature.enabled())
            appendRawTrueTypeFeature(features, kLigaturesType, kRareLigaturesOnSelector);
        else
            appendRawTrueTypeFeature(features, kLigaturesType, kRareLigaturesOffSelector);
    } else if (equalIgnoringASCIICase(feature.tag(), "hlig")) {
        if (feature.enabled())
            appendRawTrueTypeFeature(features, kLigaturesType, kHistoricalLigaturesOnSelector);
        else
            appendRawTrueTypeFeature(features, kLigaturesType, kHistoricalLigaturesOffSelector);
    } else if (equalIgnoringASCIICase(feature.tag(), "calt")) {
        if (feature.enabled())
            appendRawTrueTypeFeature(features, kContextualAlternatesType, kContextualAlternatesOnSelector);
        else
            appendRawTrueTypeFeature(features, kContextualAlternatesType, kContextualAlternatesOffSelector);
    } else if (equalIgnoringASCIICase(feature.tag(), "subs") && feature.enabled())
        appendRawTrueTypeFeature(features, kVerticalPositionType, kInferiorsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "sups") && feature.enabled())
        appendRawTrueTypeFeature(features, kVerticalPositionType, kSuperiorsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "smcp") && feature.enabled())
        appendRawTrueTypeFeature(features, kLowerCaseType, kLowerCaseSmallCapsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "c2sc") && feature.enabled())
        appendRawTrueTypeFeature(features, kUpperCaseType, kUpperCaseSmallCapsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "pcap") && feature.enabled())
        appendRawTrueTypeFeature(features, kLowerCaseType, kLowerCasePetiteCapsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "c2pc") && feature.enabled())
        appendRawTrueTypeFeature(features, kUpperCaseType, kUpperCasePetiteCapsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "unic") && feature.enabled())
        appendRawTrueTypeFeature(features, kLetterCaseType, 14);
    else if (equalIgnoringASCIICase(feature.tag(), "titl") && feature.enabled())
        appendRawTrueTypeFeature(features, kStyleOptionsType, kTitlingCapsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "lnum") && feature.enabled())
        appendRawTrueTypeFeature(features, kNumberCaseType, kUpperCaseNumbersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "onum") && feature.enabled())
        appendRawTrueTypeFeature(features, kNumberCaseType, kLowerCaseNumbersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "pnum") && feature.enabled())
        appendRawTrueTypeFeature(features, kNumberSpacingType, kProportionalNumbersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "tnum") && feature.enabled())
        appendRawTrueTypeFeature(features, kNumberSpacingType, kMonospacedNumbersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "frac") && feature.enabled())
        appendRawTrueTypeFeature(features, kFractionsType, kDiagonalFractionsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "afrc") && feature.enabled())
        appendRawTrueTypeFeature(features, kFractionsType, kVerticalFractionsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "ordn") && feature.enabled())
        appendRawTrueTypeFeature(features, kVerticalPositionType, kOrdinalsSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "zero") && feature.enabled())
        appendRawTrueTypeFeature(features, kTypographicExtrasType, kSlashedZeroOnSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "hist") && feature.enabled())
        appendRawTrueTypeFeature(features, kLigaturesType, kHistoricalLigaturesOnSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "jp78") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kJIS1978CharactersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "jp83") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kJIS1983CharactersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "jp90") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kJIS1990CharactersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "jp04") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kJIS2004CharactersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "smpl") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kSimplifiedCharactersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "trad") && feature.enabled())
        appendRawTrueTypeFeature(features, kCharacterShapeType, kTraditionalCharactersSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "fwid") && feature.enabled())
        appendRawTrueTypeFeature(features, kTextSpacingType, kMonospacedTextSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "pwid") && feature.enabled())
        appendRawTrueTypeFeature(features, kTextSpacingType, kProportionalTextSelector);
    else if (equalIgnoringASCIICase(feature.tag(), "ruby") && feature.enabled())
        appendRawTrueTypeFeature(features, kRubyKanaType, kRubyKanaOnSelector);
}

static inline void appendOpenTypeFeature(CFMutableArrayRef features, const FontFeature& feature)
{
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 80000)
    RetainPtr<CFStringRef> featureKey = feature.tag().string().createCFString();
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

RetainPtr<CTFontRef> preparePlatformFont(CTFontRef originalFont, TextRenderingMode textRenderingMode, const FontFeatureSettings* features)
{
    if (!originalFont || ((!features || !features->size()) && (textRenderingMode != OptimizeLegibility)))
        return originalFont;

    RetainPtr<CFMutableDictionaryRef> attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    if (features && features->size()) {
        RetainPtr<CFMutableArrayRef> featureArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, features->size(), &kCFTypeArrayCallBacks));
        for (size_t i = 0; i < features->size(); ++i) {
            appendTrueTypeFeature(featureArray.get(), features->at(i));
            appendOpenTypeFeature(featureArray.get(), features->at(i));
        }
        CFDictionaryAddValue(attributes.get(), kCTFontFeatureSettingsAttribute, featureArray.get());
    }
    if (textRenderingMode == OptimizeLegibility) {
        CGFloat size = CTFontGetSize(originalFont);
        RetainPtr<CFNumberRef> sizeNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &size));
        CFDictionaryAddValue(attributes.get(), kCTFontOpticalSizeAttribute, sizeNumber.get());
    }
    RetainPtr<CTFontDescriptorRef> descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    return adoptCF(CTFontCreateCopyWithAttributes(originalFont, CTFontGetSize(originalFont), nullptr, descriptor.get()));
}

}
