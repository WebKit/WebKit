/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#import "FontCascade.h"
#import "RenderThemeIOS.h"
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>

namespace WebCore {

void platformInvalidateFontCache()
{
}

bool requiresCustomFallbackFont(UChar32 character)
{
    return character == AppleLogo || character == blackCircle || character == narrowNonBreakingSpace;
}

FontPlatformData* FontCache::getCustomFallbackFont(const UInt32 c, const FontDescription& description)
{
    ASSERT(requiresCustomFallbackFont(c));

    static NeverDestroyed<AtomicString> helveticaFamily("Helvetica Neue", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> lockClockFamily("LockClock-Light", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> timesNewRomanPSMTFamily("TimesNewRomanPSMT", AtomicString::ConstructFromLiteral);

    AtomicString* family = nullptr;
    switch (c) {
    case AppleLogo:
        family = &helveticaFamily.get();
        break;
    case blackCircle:
        family = &lockClockFamily.get();
        break;
    case narrowNonBreakingSpace:
        family = &timesNewRomanPSMTFamily.get();
        break;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ASSERT(family);
    if (!family)
        return nullptr;
    return getCachedFontPlatformData(description, *family);
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    return *fontForFamily(fontDescription, AtomicString(".PhoneFallback", AtomicString::ConstructFromLiteral));
}

float FontCache::weightOfCTFont(CTFontRef font)
{
    RetainPtr<CFDictionaryRef> traits = adoptCF(CTFontCopyTraits(font));

    CFNumberRef resultRef = (CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontWeightTrait);
    float result = 0;
    CFNumberGetValue(resultRef, kCFNumberFloatType, &result);

    return result;
}

static RetainPtr<CTFontDescriptorRef> baseSystemFontDescriptor(FontWeight weight, bool bold, float size)
{
    CTFontUIFontType fontType = kCTFontUIFontSystem;
    if (weight > FontWeight300) {
        if (bold)
            fontType = kCTFontUIFontEmphasizedSystem;
    } else if (weight > FontWeight200)
        fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemLight);
    else if (weight > FontWeight100)
        fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemThin);
    else
        fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemUltraLight);
    return adoptCF(CTFontDescriptorCreateForUIType(fontType, size, nullptr));
}

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000
static RetainPtr<NSDictionary> systemFontModificationAttributes(FontWeight weight, bool italic)
{
    RetainPtr<NSMutableDictionary> traitsDictionary = adoptNS([[NSMutableDictionary alloc] init]);

    ASSERT(weight >= FontWeight100 && weight <= FontWeight900);
    float ctWeights[] = {
        static_cast<float>(kCTFontWeightUltraLight),
        static_cast<float>(kCTFontWeightThin),
        static_cast<float>(kCTFontWeightLight),
        static_cast<float>(kCTFontWeightRegular),
        static_cast<float>(kCTFontWeightMedium),
        static_cast<float>(kCTFontWeightSemibold),
        static_cast<float>(kCTFontWeightBold),
        static_cast<float>(kCTFontWeightHeavy),
        static_cast<float>(kCTFontWeightBlack)
    };
    [traitsDictionary setObject:[NSNumber numberWithFloat:ctWeights[weight]] forKey:static_cast<NSString *>(kCTFontWeightTrait)];

    [traitsDictionary setObject:@YES forKey:static_cast<NSString *>(kCTFontUIFontDesignTrait)];

    if (italic)
        [traitsDictionary setObject:[NSNumber numberWithInt:kCTFontItalicTrait] forKey:static_cast<NSString *>(kCTFontSymbolicTrait)];

    return @{ static_cast<NSString *>(kCTFontTraitsAttribute) : traitsDictionary.get() };
}
#endif

static RetainPtr<CTFontDescriptorRef> systemFontDescriptor(FontWeight weight, bool bold, bool italic, float size)
{
    RetainPtr<CTFontDescriptorRef> fontDescriptor = baseSystemFontDescriptor(weight, bold, size);
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000
    RetainPtr<NSDictionary> attributes = systemFontModificationAttributes(weight, italic);
    return adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), static_cast<CFDictionaryRef>(attributes.get())));
#else
    if (italic)
        return adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), kCTFontItalicTrait, kCTFontItalicTrait));
    return fontDescriptor;
#endif
}

RetainPtr<CTFontRef> platformFontWithFamilySpecialCase(const AtomicString& family, FontWeight weight, CTFontSymbolicTraits traits, float size)
{
    if (family.startsWith("UICTFontTextStyle")) {
        traits &= (kCTFontBoldTrait | kCTFontItalicTrait);
        RetainPtr<CFStringRef> familyNameStr = family.string().createCFString();
        RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(familyNameStr.get(), RenderThemeIOS::contentSizeCategory(), nullptr));
        if (traits)
            fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), traits, traits));

        return adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr));
    }

    if (equalLettersIgnoringASCIICase(family, "-webkit-system-font") || equalLettersIgnoringASCIICase(family, "-apple-system") || equalLettersIgnoringASCIICase(family, "-apple-system-font") || equalLettersIgnoringASCIICase(family, "system-ui")) {
        return adoptCF(CTFontCreateWithFontDescriptor(systemFontDescriptor(weight, traits & kCTFontTraitBold, traits & kCTFontTraitItalic, size).get(), size, nullptr));
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-system-monospaced-numbers")) {
        RetainPtr<CTFontDescriptorRef> systemFontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, size, nullptr));
        RetainPtr<CTFontDescriptorRef> monospaceFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithFeature(systemFontDescriptor.get(), (CFNumberRef)@(kNumberSpacingType), (CFNumberRef)@(kMonospacedNumbersSelector)));
        return adoptCF(CTFontCreateWithFontDescriptor(monospaceFontDescriptor.get(), size, nullptr));
    }

    return nullptr;
}

} // namespace WebCore
