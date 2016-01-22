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

#if !PLATFORM(IOS)

#import "CoreGraphicsSPI.h"
#import "CoreTextSPI.h"
#import "Font.h"
#import "FontCascade.h"
#import "FontPlatformData.h"
#import "NSFontSPI.h"
#import "WebCoreNSStringExtras.h"
#import "WebCoreSystemInterface.h"
#import <AppKit/AppKit.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Optional.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>
#import <wtf/text/AtomicStringHash.h>

namespace WebCore {

#if !ENABLE(PLATFORM_FONT_LOOKUP)

#define SYNTHESIZED_FONT_TRAITS (NSBoldFontMask | NSItalicFontMask)

#define IMPORTANT_FONT_TRAITS (0 \
    | NSCompressedFontMask \
    | NSCondensedFontMask \
    | NSExpandedFontMask \
    | NSItalicFontMask \
    | NSNarrowFontMask \
    | NSPosterFontMask \
    | NSSmallCapsFontMask \
)

static bool acceptableChoice(NSFontTraitMask desiredTraits, NSFontTraitMask candidateTraits)
{
    desiredTraits &= ~SYNTHESIZED_FONT_TRAITS;
    return (candidateTraits & desiredTraits) == desiredTraits;
}

static bool betterChoice(NSFontTraitMask desiredTraits, int desiredWeight, NSFontTraitMask chosenTraits, int chosenWeight, NSFontTraitMask candidateTraits, int candidateWeight)
{
    if (!acceptableChoice(desiredTraits, candidateTraits))
        return false;

    // A list of the traits we care about.
    // The top item in the list is the worst trait to mismatch; if a font has this
    // and we didn't ask for it, we'd prefer any other font in the family.
    const NSFontTraitMask masks[] = {
        NSPosterFontMask,
        NSSmallCapsFontMask,
        NSItalicFontMask,
        NSCompressedFontMask,
        NSCondensedFontMask,
        NSExpandedFontMask,
        NSNarrowFontMask,
        0
    };

    int i = 0;
    NSFontTraitMask mask;
    while ((mask = masks[i++])) {
        bool desired = desiredTraits & mask;
        bool chosenHasUnwantedTrait = desired != (chosenTraits & mask);
        bool candidateHasUnwantedTrait = desired != (candidateTraits & mask);
        if (!candidateHasUnwantedTrait && chosenHasUnwantedTrait)
            return true;
        if (!chosenHasUnwantedTrait && candidateHasUnwantedTrait)
            return false;
    }

    int chosenWeightDeltaMagnitude = abs(chosenWeight - desiredWeight);
    int candidateWeightDeltaMagnitude = abs(candidateWeight - desiredWeight);

    // If both are the same distance from the desired weight, prefer the candidate if it is further from medium.
    if (chosenWeightDeltaMagnitude == candidateWeightDeltaMagnitude)
        return abs(candidateWeight - 6) > abs(chosenWeight - 6);

    // Otherwise, prefer the one closer to the desired weight.
    return candidateWeightDeltaMagnitude < chosenWeightDeltaMagnitude;
}

// Keep a cache for mapping desired font families to font families actually available on the system for performance.
using AvailableFamilyMap = HashMap<std::pair<AtomicString, NSFontTraitMask>, AtomicString>;
static AvailableFamilyMap& desiredFamilyToAvailableFamilyMap()
{
    ASSERT(isMainThread());
    static NeverDestroyed<AvailableFamilyMap> map;
    return map;
}

static bool hasDesiredFamilyToAvailableFamilyMapping(const AtomicString& desiredFamily, NSFontTraitMask desiredTraits, NSString*& availableFamily)
{
    AtomicString value = desiredFamilyToAvailableFamilyMap().get(std::make_pair(desiredFamily, desiredTraits));
    availableFamily = value.isEmpty() ? nil : static_cast<NSString*>(value);
    return !value.isNull();
}

static inline void rememberDesiredFamilyToAvailableFamilyMapping(const AtomicString& desiredFamily, NSFontTraitMask desiredTraits, NSString* availableFamily)
{
    static const unsigned maxCacheSize = 128;
    auto& familyMapping = desiredFamilyToAvailableFamilyMap();
    ASSERT(familyMapping.size() <= maxCacheSize);
    if (familyMapping.size() >= maxCacheSize)
        familyMapping.remove(familyMapping.begin());

    // Store nil as an emptyAtom to distinguish from missing values (nullAtom).
    AtomicString value = availableFamily ? AtomicString(availableFamily) : emptyAtom;
    familyMapping.add(std::make_pair(desiredFamily, desiredTraits), value);
}

static int toAppKitFontWeight(FontWeight fontWeight)
{
    static const int appKitFontWeights[] = {
        2, // FontWeight100
        3, // FontWeight200
        4, // FontWeight300
        5, // FontWeight400
        6, // FontWeight500
        8, // FontWeight600
        9, // FontWeight700
        10, // FontWeight800
        12, // FontWeight900
    };
    return appKitFontWeights[fontWeight];
}

static inline FontWeight appkitWeightToFontWeight(NSInteger appKitWeight)
{
    if (appKitWeight == 1)
        return FontWeight100;
    if (appKitWeight == 2)
        return FontWeight200;
    if (appKitWeight <= 4)
        return FontWeight300;
    if (appKitWeight == 5)
        return FontWeight400;
    if (appKitWeight == 6)
        return FontWeight500;
    if (appKitWeight <= 8)
        return FontWeight600;
    if (appKitWeight == 9)
        return FontWeight700;
    if (appKitWeight <= 11)
        return FontWeight800;
    return FontWeight900;
}

static NSFontTraitMask toNSFontTraits(CTFontSymbolicTraits traits)
{
    NSFontTraitMask result = 0;
    if (traits & kCTFontBoldTrait)
        result |= NSBoldFontMask;
    if (traits & kCTFontItalicTrait)
        result |= NSItalicFontMask;
    return result;
}

#endif // PLATFORM_FONT_LOOKUP

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

#if !ENABLE(PLATFORM_FONT_LOOKUP)
RetainPtr<CTFontRef> platformFontWithFamily(const AtomicString& family, CTFontSymbolicTraits requestedTraits, FontWeight weight, TextRenderingMode, float size)
{
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSString *availableFamily;
    int chosenWeight;
    NSFont *font;

    NSFontTraitMask desiredTraits = toNSFontTraits(requestedTraits);
    NSFontTraitMask desiredTraitsForNameMatch = desiredTraits | (weight >= FontWeight600 ? NSBoldFontMask : 0);
    if (hasDesiredFamilyToAvailableFamilyMapping(family, desiredTraitsForNameMatch, availableFamily)) {
        if (!availableFamily) {
            // We already know the desired font family does not map to any available font family.
            return nil;
        }
    }

    if (!availableFamily) {
        NSString *desiredFamily = family;

        // Do a simple case insensitive search for a matching font family.
        // NSFontManager requires exact name matches.
        // This addresses the problem of matching arial to Arial, etc., but perhaps not all the issues.
        for (availableFamily in [fontManager availableFontFamilies]) {
            if ([desiredFamily caseInsensitiveCompare:availableFamily] == NSOrderedSame)
                break;
        }

        if (!availableFamily) {
            // Match by PostScript name.
            NSFont *nameMatchedFont = nil;
            for (NSString *availableFont in [fontManager availableFonts]) {
                if ([desiredFamily caseInsensitiveCompare:availableFont] == NSOrderedSame) {
                    nameMatchedFont = [NSFont fontWithName:availableFont size:size];

                    // Special case Osaka-Mono. According to <rdar://problem/3999467>, we need to
                    // treat Osaka-Mono as fixed pitch.
                    if ([desiredFamily caseInsensitiveCompare:@"Osaka-Mono"] == NSOrderedSame && !desiredTraitsForNameMatch)
                        return toCTFont(nameMatchedFont);

                    NSFontTraitMask traits = [fontManager traitsOfFont:nameMatchedFont];
                    if ((traits & desiredTraitsForNameMatch) == desiredTraitsForNameMatch)
                        return toCTFont([fontManager convertFont:nameMatchedFont toHaveTrait:desiredTraitsForNameMatch]);

                    availableFamily = [nameMatchedFont familyName];
                    break;
                }
            }
        }

        rememberDesiredFamilyToAvailableFamilyMapping(family, desiredTraitsForNameMatch, availableFamily);
        if (!availableFamily)
            return nil;
    }

    // Found a family, now figure out what weight and traits to use.
    bool choseFont = false;
    chosenWeight = 0;
    NSFontTraitMask chosenTraits = 0;
    NSString *chosenFullName = 0;

    int appKitDesiredWeight = toAppKitFontWeight(weight);
    NSArray *fonts = [fontManager availableMembersOfFontFamily:availableFamily];
    for (NSArray *fontInfo in fonts) {
        // Array indices must be hard coded because of lame AppKit API.
        NSString *fontFullName = [fontInfo objectAtIndex:0];
        NSInteger fontWeight = [[fontInfo objectAtIndex:2] intValue];
        NSFontTraitMask fontTraits = [[fontInfo objectAtIndex:3] unsignedIntValue];

        BOOL newWinner;
        if (!choseFont)
            newWinner = acceptableChoice(desiredTraits, fontTraits);
        else
            newWinner = betterChoice(desiredTraits, appKitDesiredWeight, chosenTraits, chosenWeight, fontTraits, fontWeight);

        if (newWinner) {
            choseFont = YES;
            chosenWeight = fontWeight;
            chosenTraits = fontTraits;
            chosenFullName = fontFullName;

            if (chosenWeight == appKitDesiredWeight && (chosenTraits & IMPORTANT_FONT_TRAITS) == (desiredTraits & IMPORTANT_FONT_TRAITS))
                break;
        }
    }

    if (!choseFont)
        return nil;

    font = [NSFont fontWithName:chosenFullName size:size];

    if (!font)
        return nil;

    NSFontTraitMask actualTraits = 0;
    if (desiredTraits & NSFontItalicTrait)
        actualTraits = [fontManager traitsOfFont:font];
    FontWeight actualWeight = appkitWeightToFontWeight([fontManager weightOfFont:font]);

    bool syntheticBold = isFontWeightBold(weight) && isFontWeightBold(actualWeight);
    bool syntheticOblique = (desiredTraits & NSFontItalicTrait) && !(actualTraits & NSFontItalicTrait);

    // There are some malformed fonts that will be correctly returned by -fontWithFamily:traits:weight:size: as a match for a particular trait,
    // though -[NSFontManager traitsOfFont:] incorrectly claims the font does not have the specified trait. This could result in applying 
    // synthetic bold on top of an already-bold font, as reported in <http://bugs.webkit.org/show_bug.cgi?id=6146>. To work around this
    // problem, if we got an apparent exact match, but the requested traits aren't present in the matched font, we'll try to get a font from 
    // the same family without those traits (to apply the synthetic traits to later).
    NSFontTraitMask nonSyntheticTraits = desiredTraits;

    if (syntheticBold)
        nonSyntheticTraits &= ~NSBoldFontMask;

    if (syntheticOblique)
        nonSyntheticTraits &= ~NSItalicFontMask;

    if (nonSyntheticTraits != desiredTraits) {
        NSFont *fontWithoutSyntheticTraits = [fontManager fontWithFamily:availableFamily traits:nonSyntheticTraits weight:chosenWeight size:size];
        if (fontWithoutSyntheticTraits)
            font = fontWithoutSyntheticTraits;
    }

    return toCTFont(font);
}
#endif

void platformInvalidateFontCache()
{
#if !ENABLE(PLATFORM_FONT_LOOKUP)
    desiredFamilyToAvailableFamilyMap().clear();
#endif
}

RetainPtr<CTFontRef> platformLookupFallbackFont(CTFontRef font, FontWeight, const AtomicString& locale, const UChar* characters, unsigned length)
{
    RetainPtr<CFStringRef> localeString;
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101100
    if (!locale.isNull())
        localeString = locale.string().createCFString();
#else
    UNUSED_PARAM(locale);
#endif

    CFIndex coveredLength = 0;
    return adoptCF(CTFontCreateForCharactersWithLanguage(font, characters, length, localeString.get(), &coveredLength));
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.
    if (RefPtr<Font> font = fontForFamily(fontDescription, AtomicString("Times", AtomicString::ConstructFromLiteral), false))
        return *font;

    // The Times fallback will almost always work, but in the highly unusual case where
    // the user doesn't have it, we fall back on Lucida Grande because that's
    // guaranteed to be there, according to Nathan Taylor. This is good enough
    // to avoid a crash at least.
    return *fontForFamily(fontDescription, AtomicString("Lucida Grande", AtomicString::ConstructFromLiteral), false);
}

} // namespace WebCore

#endif // !PLATFORM(IOS)
