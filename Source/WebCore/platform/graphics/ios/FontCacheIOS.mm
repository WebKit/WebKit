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

static RetainPtr<CTFontRef> getSystemFontFallbackForCharacters(CTFontRef font, const AtomicString& locale, const UChar* characters, unsigned length)
{
    // FIXME: Unify this with platformLookupFallbackFont()
    RetainPtr<CFStringRef> localeString;
    if (!locale.isNull())
        localeString = locale.string().createCFString();

    CFIndex coveredLength = 0;
    return adoptCF(CTFontCreatePhysicalFontForCharactersWithLanguage(font, (const UTF16Char*)characters, (CFIndex)length, localeString.get(), &coveredLength));
}

RetainPtr<CTFontRef> platformLookupFallbackFont(CTFontRef font, FontWeight fontWeight, const AtomicString& locale, const UChar* characters, unsigned length)
{
    // For system fonts we use CoreText fallback mechanism.
    if (length && CTFontDescriptorIsSystemUIFont(adoptCF(CTFontCopyFontDescriptor(font)).get()))
        return getSystemFontFallbackForCharacters(font, locale, characters, length);

    RetainPtr<CFStringRef> localeString;
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000
    if (!locale.isNull())
        localeString = locale.string().createCFString();
#endif
    RetainPtr<CTFontDescriptorRef> fallbackFontDescriptor = adoptCF(CTFontCreatePhysicalFontDescriptorForCharactersWithLanguage(font, characters, length, localeString.get(), nullptr));
    UChar32 c = *characters;
    if (length > 1 && U16_IS_LEAD(c) && U16_IS_TRAIL(characters[1]))
        c = U16_GET_SUPPLEMENTARY(c, characters[1]);
    // Arabic
    if (c >= 0x0600 && c <= 0x06ff) {
        auto familyName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(fallbackFontDescriptor.get(), kCTFontFamilyNameAttribute)));
        if (fontFamilyShouldNotBeUsedForArabic(familyName.get())) {
            CFStringRef newFamilyName = isFontWeightBold(fontWeight) ? CFSTR("GeezaPro-Bold") : CFSTR("GeezaPro");
            CFTypeRef keys[] = { kCTFontFamilyNameAttribute };
            CFTypeRef values[] = { newFamilyName };
            RetainPtr<CFDictionaryRef> attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            fallbackFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fallbackFontDescriptor.get(), attributes.get()));
        }
    }
    return adoptCF(CTFontCreateWithFontDescriptor(fallbackFontDescriptor.get(), CTFontGetSize(font), nullptr));
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    return *fontForFamily(fontDescription, AtomicString(".PhoneFallback", AtomicString::ConstructFromLiteral), false);
}

float FontCache::weightOfCTFont(CTFontRef font)
{
    RetainPtr<CFDictionaryRef> traits = adoptCF(CTFontCopyTraits(font));

    CFNumberRef resultRef = (CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontWeightTrait);
    float result = 0;
    CFNumberGetValue(resultRef, kCFNumberFloatType, &result);

    return result;
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

    if (equalLettersIgnoringASCIICase(family, "-webkit-system-font") || equalLettersIgnoringASCIICase(family, "-apple-system") || equalLettersIgnoringASCIICase(family, "-apple-system-font")) {
        CTFontUIFontType fontType = kCTFontUIFontSystem;
        if (weight > FontWeight300) {
            // The code below has been copied from CoreText/UIFoundation. However, in WebKit we synthesize the oblique,
            // so we should investigate the result <rdar://problem/14449340>:
            if (traits & kCTFontTraitBold)
                fontType = kCTFontUIFontEmphasizedSystem;
        } else if (weight > FontWeight200)
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemLight);
        else if (weight > FontWeight100)
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemThin);
        else
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemUltraLight);
        RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(fontType, size, nullptr));
        if (traits & kCTFontTraitItalic)
            fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), kCTFontItalicTrait, kCTFontItalicTrait));
        return adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr));
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-system-monospaced-numbers")) {
        RetainPtr<CTFontDescriptorRef> systemFontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, size, nullptr));
        RetainPtr<CTFontDescriptorRef> monospaceFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithFeature(systemFontDescriptor.get(), (CFNumberRef)@(kNumberSpacingType), (CFNumberRef)@(kMonospacedNumbersSelector)));
        return adoptCF(CTFontCreateWithFontDescriptor(monospaceFontDescriptor.get(), size, nullptr));
    }

    return nullptr;
}

} // namespace WebCore
