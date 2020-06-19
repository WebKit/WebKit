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

#import "Font.h"
#import "FontCascade.h"
#import "FontPlatformData.h"
#import "SystemFontDatabaseCoreText.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/CoreTextSPI.h>

#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#import <pal/spi/mac/NSFontSPI.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>
#import <wtf/text/AtomStringHash.h>
#endif

#import <wtf/SoftLinking.h>

namespace WebCore {

#if PLATFORM(MAC)

static CGFloat toNSFontWeight(FontSelectionValue fontWeight)
{
    if (fontWeight < FontSelectionValue(150))
        return NSFontWeightUltraLight;
    if (fontWeight < FontSelectionValue(250))
        return NSFontWeightThin;
    if (fontWeight < FontSelectionValue(350))
        return NSFontWeightLight;
    if (fontWeight < FontSelectionValue(450))
        return NSFontWeightRegular;
    if (fontWeight < FontSelectionValue(550))
        return NSFontWeightMedium;
    if (fontWeight < FontSelectionValue(650))
        return NSFontWeightSemibold;
    if (fontWeight < FontSelectionValue(750))
        return NSFontWeightBold;
    if (fontWeight < FontSelectionValue(850))
        return NSFontWeightHeavy;
    return NSFontWeightBlack;
}

RetainPtr<CTFontRef> platformFontWithFamilySpecialCase(const AtomString& family, const FontDescription& fontDescription, float size, AllowUserInstalledFonts allowUserInstalledFonts)
{
    // FIXME: See comment in FontCascadeDescription::effectiveFamilyAt() in FontDescriptionCocoa.cpp
    const auto& request = fontDescription.fontSelectionRequest();

    // FIXME: Migrate this to use SystemFontDatabaseCoreText like the design system-ui block below.
    if (equalLettersIgnoringASCIICase(family, "-webkit-system-font") || equalLettersIgnoringASCIICase(family, "-apple-system") || equalLettersIgnoringASCIICase(family, "-apple-system-font") || equalLettersIgnoringASCIICase(family, "system-ui")) {
        RetainPtr<CTFontRef> result = toCTFont([NSFont systemFontOfSize:size weight:toNSFontWeight(request.weight)]);
        if (isItalic(request.slope)) {
            CTFontSymbolicTraits desiredTraits = kCTFontItalicTrait;
            if (isFontWeightBold(request.weight))
                desiredTraits |= kCTFontBoldTrait;
            if (auto italicizedFont = adoptCF(CTFontCreateCopyWithSymbolicTraits(result.get(), size, nullptr, desiredTraits, desiredTraits)))
                result = italicizedFont;
        }
        return createFontForInstalledFonts(result.get(), allowUserInstalledFonts);
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
        auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(attributes.get(), kCTFontFeatureSettingsAttribute, featureArray.get());
        addAttributesForInstalledFonts(attributes.get(), allowUserInstalledFonts);
        RetainPtr<CTFontRef> result = toCTFont([NSFont systemFontOfSize:size]);
        auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
        return adoptCF(CTFontCreateCopyWithAttributes(result.get(), size, nullptr, modification.get()));
    }

    if (equalLettersIgnoringASCIICase(family, "lastresort")) {
        static const CTFontDescriptorRef lastResort = CTFontDescriptorCreateLastResort();
        return adoptCF(CTFontCreateWithFontDescriptor(lastResort, size, nullptr));
    }

    return nullptr;
}

#endif // PLATFORM(MAC)

} // namespace WebCore
