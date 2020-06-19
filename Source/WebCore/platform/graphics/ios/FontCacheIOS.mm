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

#if PLATFORM(IOS_FAMILY)

#import "FontCascade.h"
#import "RenderThemeIOS.h"
#import "SystemFontDatabaseCoreText.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/CoreTextSPI.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/CString.h>

namespace WebCore {

bool requiresCustomFallbackFont(UChar32 character)
{
    return character == AppleLogo || character == narrowNonBreakingSpace;
}

FontPlatformData* FontCache::getCustomFallbackFont(const UInt32 c, const FontDescription& description)
{
    ASSERT(requiresCustomFallbackFont(c));

    static MainThreadNeverDestroyed<const AtomString> helveticaFamily("Helvetica Neue", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> timesNewRomanPSMTFamily("TimesNewRomanPSMT", AtomString::ConstructFromLiteral);

    const AtomString* family = nullptr;
    switch (c) {
    case AppleLogo:
        family = &helveticaFamily.get();
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

static RetainPtr<CTFontDescriptorRef> baseSystemFontDescriptor(FontSelectionValue weight, bool bold, float size)
{
    CTFontUIFontType fontType = kCTFontUIFontSystem;
    if (weight >= FontSelectionValue(350)) {
        if (bold)
            fontType = kCTFontUIFontEmphasizedSystem;
    } else if (weight >= FontSelectionValue(250))
        fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemLight);
    else if (weight >= FontSelectionValue(150))
        fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemThin);
    else
        fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemUltraLight);
    return adoptCF(CTFontDescriptorCreateForUIType(fontType, size, nullptr));
}

static RetainPtr<NSDictionary> systemFontModificationAttributes(FontSelectionValue weight, bool italic)
{
    RetainPtr<NSMutableDictionary> traitsDictionary = adoptNS([[NSMutableDictionary alloc] init]);

    float ctWeight = kCTFontWeightRegular;
    if (weight < FontSelectionValue(150))
        ctWeight = kCTFontWeightUltraLight;
    else if (weight < FontSelectionValue(250))
        ctWeight = kCTFontWeightThin;
    else if (weight < FontSelectionValue(350))
        ctWeight = kCTFontWeightLight;
    else if (weight < FontSelectionValue(450))
        ctWeight = kCTFontWeightRegular;
    else if (weight < FontSelectionValue(550))
        ctWeight = kCTFontWeightMedium;
    else if (weight < FontSelectionValue(650))
        ctWeight = kCTFontWeightSemibold;
    else if (weight < FontSelectionValue(750))
        ctWeight = kCTFontWeightBold;
    else if (weight < FontSelectionValue(850))
        ctWeight = kCTFontWeightHeavy;
    else
        ctWeight = kCTFontWeightBlack;
    [traitsDictionary setObject:[NSNumber numberWithFloat:ctWeight] forKey:static_cast<NSString *>(kCTFontWeightTrait)];

    [traitsDictionary setObject:@YES forKey:static_cast<NSString *>(kCTFontUIFontDesignTrait)];

    if (italic)
        [traitsDictionary setObject:@(kCTFontItalicTrait) forKey:static_cast<NSString *>(kCTFontSymbolicTrait)];

    return @{ static_cast<NSString *>(kCTFontTraitsAttribute) : traitsDictionary.get() };
}

static RetainPtr<CTFontDescriptorRef> systemFontDescriptor(FontSelectionValue weight, bool bold, bool italic, float size)
{
    RetainPtr<CTFontDescriptorRef> fontDescriptor = baseSystemFontDescriptor(weight, bold, size);
    RetainPtr<NSDictionary> attributes = systemFontModificationAttributes(weight, italic);
    return adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), static_cast<CFDictionaryRef>(attributes.get())));
}

RetainPtr<CTFontRef> platformFontWithFamilySpecialCase(const AtomString& family, const FontDescription& fontDescription, float size, AllowUserInstalledFonts allowUserInstalledFonts)
{
    // FIXME: See comment in FontCascadeDescription::effectiveFamilyAt() in FontDescriptionCocoa.cpp
    const auto& request = fontDescription.fontSelectionRequest();

    // FIXME: Migrate this to use SystemFontDatabaseCoreText like the design system-ui block below.
    if (equalLettersIgnoringASCIICase(family, "-webkit-system-font") || equalLettersIgnoringASCIICase(family, "-apple-system") || equalLettersIgnoringASCIICase(family, "-apple-system-font") || equalLettersIgnoringASCIICase(family, "system-ui")) {
        auto fontDescriptor = systemFontDescriptor(request.weight, isFontWeightBold(request.weight), isItalic(request.slope), size);
        return createFontForInstalledFonts(fontDescriptor.get(), size, allowUserInstalledFonts);
    }

    if (equalLettersIgnoringASCIICase(family, "-apple-system-monospaced-numbers")) {
        RetainPtr<CTFontDescriptorRef> systemFontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, size, nullptr));
        RetainPtr<CTFontDescriptorRef> monospaceFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithFeature(systemFontDescriptor.get(), (CFNumberRef)@(kNumberSpacingType), (CFNumberRef)@(kMonospacedNumbersSelector)));
        return createFontForInstalledFonts(monospaceFontDescriptor.get(), size, allowUserInstalledFonts);
    }

    if (equalLettersIgnoringASCIICase(family, "lastresort")) {
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
        static const CTFontDescriptorRef lastResort = CTFontDescriptorCreateLastResort();
        return adoptCF(CTFontCreateWithFontDescriptor(lastResort, size, nullptr));
#else
        // LastResort is special, so it's important to look this exact string up, and not some case-folded version.
        // We handle this here so any caching and case folding we do in our general text codepath is bypassed.
        return adoptCF(CTFontCreateWithName(CFSTR("LastResort"), size, nullptr));
#endif
    }

    return nullptr;
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
