/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "FontCache.h"

#import "CoreGraphicsSPI.h"
#import "CoreTextSPI.h"
#import "Font.h"
#import "FontCascade.h"
#import "FontPlatformData.h"

#if PLATFORM(MAC)
#import "NSFontSPI.h"
#import "WebCoreNSStringExtras.h"
#import "WebCoreSystemInterface.h"
#import <AppKit/AppKit.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>
#import <wtf/text/AtomicStringHash.h>
#endif

namespace WebCore {

#if PLATFORM(MAC)

static CGFloat toNSFontWeight(FontWeight fontWeight)
{
    static const CGFloat nsFontWeights[] = {
        NSFontWeightUltraLight,
        NSFontWeightThin,
        NSFontWeightLight,
        NSFontWeightRegular,
        NSFontWeightMedium,
        NSFontWeightSemibold,
        NSFontWeightBold,
        NSFontWeightHeavy,
        NSFontWeightBlack
    };
    ASSERT(fontWeight >= 0 && fontWeight <= 8);
    return nsFontWeights[fontWeight];
}

RetainPtr<CTFontRef> platformFontWithFamilySpecialCase(const AtomicString& family, FontWeight weight, CTFontSymbolicTraits desiredTraits, float size)
{
    if (equalLettersIgnoringASCIICase(family, "-webkit-system-font") || equalLettersIgnoringASCIICase(family, "-apple-system") || equalLettersIgnoringASCIICase(family, "-apple-system-font")) {
        RetainPtr<CTFontRef> result = toCTFont([NSFont systemFontOfSize:size weight:toNSFontWeight(weight)]);
        if (desiredTraits & kCTFontItalicTrait) {
            if (auto italicizedFont = adoptCF(CTFontCreateCopyWithSymbolicTraits(result.get(), size, nullptr, desiredTraits, desiredTraits)))
                result = italicizedFont;
        }
        return result;
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-system-monospaced-numbers")) {
        int numberSpacingType = kNumberSpacingType;
        int monospacedNumbersSelector = kMonospacedNumbersSelector;
        RetainPtr<CFNumberRef> numberSpacingNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &numberSpacingType));
        RetainPtr<CFNumberRef> monospacedNumbersNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &monospacedNumbersSelector));
        CFTypeRef featureKeys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
        CFTypeRef featureValues[] = { numberSpacingNumber.get(), monospacedNumbersNumber.get() };
        RetainPtr<CFDictionaryRef> featureIdentifier = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, featureKeys, featureValues, WTF_ARRAY_LENGTH(featureKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFTypeRef featureIdentifiers[] = { featureIdentifier.get() };
        RetainPtr<CFArrayRef> featureArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, featureIdentifiers, 1, &kCFTypeArrayCallBacks));
        CFTypeRef attributesKeys[] = { kCTFontFeatureSettingsAttribute };
        CFTypeRef attributesValues[] = { featureArray.get() };
        RetainPtr<CFDictionaryRef> attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, attributesKeys, attributesValues, WTF_ARRAY_LENGTH(attributesKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

        RetainPtr<CTFontRef> result = toCTFont([NSFont systemFontOfSize:size]);
        return adoptCF(CTFontCreateCopyWithAttributes(result.get(), size, nullptr, adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get())).get()));
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-menu"))
        return toCTFont([NSFont menuFontOfSize:size]);

    if (equalLettersIgnoringASCIICase(family, "-apple-status-bar"))
        return toCTFont([NSFont labelFontOfSize:size]);

    return nullptr;
}

void platformInvalidateFontCache()
{
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.
    if (RefPtr<Font> font = fontForFamily(fontDescription, AtomicString("Times", AtomicString::ConstructFromLiteral)))
        return *font;

    // The Times fallback will almost always work, but in the highly unusual case where
    // the user doesn't have it, we fall back on Lucida Grande because that's
    // guaranteed to be there, according to Nathan Taylor. This is good enough
    // to avoid a crash at least.
    return *fontForFamily(fontDescription, AtomicString("Lucida Grande", AtomicString::ConstructFromLiteral), nullptr, nullptr, false);
}

#endif // PLATFORM(MAC)

Ref<Font> FontCache::lastResortFallbackFontForEveryCharacter(const FontDescription& fontDescription)
{
    auto result = fontForFamily(fontDescription, AtomicString("LastResort", AtomicString::ConstructFromLiteral));
    ASSERT(result);
    return *result;
}

} // namespace WebCore
