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

#import "config.h"
#import "FontCache.h"

namespace WebCore {

static inline void appendTrueTypeFeature(CFMutableArrayRef, const FontFeature&)
{
    // FIXME: We should map OpenType feature strings to the TrueType feature type identifiers listed in <CoreText/SFNTLayoutTypes.h>.
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

RetainPtr<CTFontRef> applyFontFeatureSettings(CTFontRef originalFont, const FontFeatureSettings* features)
{
    if (!originalFont || !features || !features->size())
        return originalFont;

    RetainPtr<CFMutableArrayRef> featureArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, features->size(), &kCFTypeArrayCallBacks));
    for (size_t i = 0; i < features->size(); ++i) {
        appendTrueTypeFeature(featureArray.get(), features->at(i));
        appendOpenTypeFeature(featureArray.get(), features->at(i));
    }
    CFArrayRef featureArrayPtr = featureArray.get();
    RetainPtr<CFDictionaryRef> dictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, (const void**)&kCTFontFeatureSettingsAttribute, (const void**)&featureArrayPtr, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    RetainPtr<CTFontDescriptorRef> descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(dictionary.get()));
    return adoptCF(CTFontCreateCopyWithAttributes(originalFont, CTFontGetSize(originalFont), nullptr, descriptor.get()));
}

}
