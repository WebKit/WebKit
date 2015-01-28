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
#import "Font.h"
#import "FontCascade.h"
#import "FontPlatformData.h"
#import "NSFontSPI.h"
#import "WebCoreNSStringExtras.h"
#import "WebCoreSystemInterface.h"
#import <AppKit/AppKit.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>
#import <wtf/text/AtomicStringHash.h>

namespace WebCore {

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

static inline FontTraitsMask toTraitsMask(NSFontTraitMask appKitTraits, NSInteger appKitWeight)
{
    return static_cast<FontTraitsMask>(((appKitTraits & NSFontItalicTrait) ? FontStyleItalicMask : FontStyleNormalMask)
        | FontVariantNormalMask
        | (appKitWeight == 1 ? FontWeight100Mask :
            appKitWeight == 2 ? FontWeight200Mask :
            appKitWeight <= 4 ? FontWeight300Mask :
            appKitWeight == 5 ? FontWeight400Mask :
            appKitWeight == 6 ? FontWeight500Mask :
            appKitWeight <= 8 ? FontWeight600Mask :
            appKitWeight == 9 ? FontWeight700Mask :
            appKitWeight <= 11 ? FontWeight800Mask :
                FontWeight900Mask));
}

// Keep a cache for mapping desired font families to font families actually
// available on the system for performance.
static NSMutableDictionary* desiredFamilyToAvailableFamilyDictionary()
{
    ASSERT(isMainThread());
    static NSMutableDictionary *dictionary = [[NSMutableDictionary alloc] init];
    return dictionary;
}

static inline void rememberDesiredFamilyToAvailableFamilyMapping(NSString* desiredFamily, NSString* availableFamily)
{
    static const NSUInteger maxCacheSize = 128;
    NSMutableDictionary *familyMapping = desiredFamilyToAvailableFamilyDictionary();
    ASSERT([familyMapping count] <= maxCacheSize);
    if ([familyMapping count] == maxCacheSize) {
        for (NSString *key in familyMapping) {
            [familyMapping removeObjectForKey:key];
            break;
        }
    }
    id value = availableFamily ? availableFamily : [NSNull null];
    [familyMapping setObject:value forKey:desiredFamily];
}

// Family name is somewhat of a misnomer here. We first attempt to find an exact match
// comparing the desiredFamily to the PostScript name of the installed fonts. If that fails
// we then do a search based on the family names of the installed fonts.
static NSFont *fontWithFamily(NSString *desiredFamily, NSFontTraitMask desiredTraits, int desiredWeight, float size)
{
    if (stringIsCaseInsensitiveEqualToString(desiredFamily, @"-webkit-system-font")
        || stringIsCaseInsensitiveEqualToString(desiredFamily, @"-apple-system-font")) {
        // We ignore italic for system font.
        return (desiredWeight >= 7) ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size];
    }

    id cachedAvailableFamily = [desiredFamilyToAvailableFamilyDictionary() objectForKey:desiredFamily];
    if (cachedAvailableFamily == [NSNull null]) {
        // We already know this font is not available.
        return nil;
    }

    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSString *availableFamily = cachedAvailableFamily;
    if (!availableFamily) {
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
            NSFontTraitMask desiredTraitsForNameMatch = desiredTraits | (desiredWeight >= 7 ? NSBoldFontMask : 0);
            for (NSString *availableFont in [fontManager availableFonts]) {
                if ([desiredFamily caseInsensitiveCompare:availableFont] == NSOrderedSame) {
                    nameMatchedFont = [NSFont fontWithName:availableFont size:size];

                    // Special case Osaka-Mono. According to <rdar://problem/3999467>, we need to
                    // treat Osaka-Mono as fixed pitch.
                    if ([desiredFamily caseInsensitiveCompare:@"Osaka-Mono"] == NSOrderedSame && !desiredTraitsForNameMatch)
                        return nameMatchedFont;

                    NSFontTraitMask traits = [fontManager traitsOfFont:nameMatchedFont];
                    if ((traits & desiredTraitsForNameMatch) == desiredTraitsForNameMatch)
                        return [fontManager convertFont:nameMatchedFont toHaveTrait:desiredTraitsForNameMatch];

                    availableFamily = [nameMatchedFont familyName];
                    break;
                }
            }
        }

        rememberDesiredFamilyToAvailableFamilyMapping(desiredFamily, availableFamily);
        if (!availableFamily)
            return nil;
    }

    // Found a family, now figure out what weight and traits to use.
    bool choseFont = false;
    int chosenWeight = 0;
    NSFontTraitMask chosenTraits = 0;
    NSString *chosenFullName = 0;

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
            newWinner = betterChoice(desiredTraits, desiredWeight, chosenTraits, chosenWeight, fontTraits, fontWeight);

        if (newWinner) {
            choseFont = YES;
            chosenWeight = fontWeight;
            chosenTraits = fontTraits;
            chosenFullName = fontFullName;

            if (chosenWeight == desiredWeight && (chosenTraits & IMPORTANT_FONT_TRAITS) == (desiredTraits & IMPORTANT_FONT_TRAITS))
                break;
        }
    }

    if (!choseFont)
        return nil;

    NSFont *font = [NSFont fontWithName:chosenFullName size:size];

    if (!font)
        return nil;

    NSFontTraitMask actualTraits = 0;
    if (desiredTraits & NSFontItalicTrait)
        actualTraits = [fontManager traitsOfFont:font];
    int actualWeight = [fontManager weightOfFont:font];

    bool syntheticBold = desiredWeight >= 7 && actualWeight < 7;
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

    return font;
}

// The "void*" parameter makes the function match the prototype for callbacks from callOnMainThread.
static void invalidateFontCache(void*)
{
    if (!isMainThread()) {
        callOnMainThread(&invalidateFontCache, 0);
        return;
    }
    fontCache().invalidate();
    [desiredFamilyToAvailableFamilyDictionary() removeAllObjects];
}

static void fontCacheRegisteredFontsChangedNotificationCallback(CFNotificationCenterRef, void* observer, CFStringRef name, const void *, CFDictionaryRef)
{
    ASSERT_UNUSED(observer, observer == &fontCache());
    ASSERT_UNUSED(name, CFEqual(name, kCTFontManagerRegisteredFontsChangedNotification));
    invalidateFontCache(0);
}

void FontCache::platformInit()
{
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, fontCacheRegisteredFontsChangedNotificationCallback, kCTFontManagerRegisteredFontsChangedNotification, 0, CFNotificationSuspensionBehaviorDeliverImmediately);
}

static int toAppKitFontWeight(FontWeight fontWeight)
{
    static int appKitFontWeights[] = {
        2,  // FontWeight100
        3,  // FontWeight200
        4,  // FontWeight300
        5,  // FontWeight400
        6,  // FontWeight500
        8,  // FontWeight600
        9,  // FontWeight700
        10, // FontWeight800
        12, // FontWeight900
    };
    return appKitFontWeights[fontWeight];
}

static inline bool isAppKitFontWeightBold(NSInteger appKitFontWeight)
{
    return appKitFontWeight >= 7;
}

static bool shouldAutoActivateFontIfNeeded(const AtomicString& family)
{
#ifndef NDEBUG
    // This cache is not thread safe so the following assertion is there to
    // make sure this function is always called from the same thread.
    static ThreadIdentifier initThreadId = currentThread();
    ASSERT(currentThread() == initThreadId);
#endif

    static NeverDestroyed<HashSet<AtomicString>> knownFamilies;
    static const int maxCacheSize = 128;
    ASSERT(knownFamilies.get().size() <= maxCacheSize);
    if (knownFamilies.get().size() == maxCacheSize)
        knownFamilies.get().remove(knownFamilies.get().begin());

    // Only attempt to auto-activate fonts once for performance reasons.
    return knownFamilies.get().add(family).isNewEntry;
}

RefPtr<Font> FontCache::systemFallbackForCharacters(const FontDescription& description, const Font* originalFontData, bool isPlatformFont, const UChar* characters, int length)
{
    UChar32 character;
    U16_GET(characters, 0, 0, length, character);
    const FontPlatformData& platformData = originalFontData->platformData();
    NSFont *nsFont = platformData.nsFont();

    NSString *string = [[NSString alloc] initWithCharactersNoCopy:const_cast<UChar*>(characters) length:length freeWhenDone:NO];
    NSFont *substituteFont = [NSFont findFontLike:nsFont forString:string withRange:NSMakeRange(0, [string length]) inLanguage:nil];
    [string release];

    if (!substituteFont && length == 1)
        substituteFont = [NSFont findFontLike:nsFont forCharacter:characters[0] inLanguage:nil];
    if (!substituteFont)
        return 0;

    // Use the family name from the AppKit-supplied substitute font, requesting the
    // traits, weight, and size we want. One way this does better than the original
    // AppKit request is that it takes synthetic bold and oblique into account.
    // But it does create the possibility that we could end up with a font that
    // doesn't actually cover the characters we need.

    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    NSFontTraitMask traits = 0;
    NSInteger weight;
    CGFloat size;

    if (nsFont) {
        if (description.italic())
            traits = [fontManager traitsOfFont:nsFont];
        if (platformData.m_syntheticBold)
            traits |= NSBoldFontMask;
        if (platformData.m_syntheticOblique)
            traits |= NSFontItalicTrait;
        weight = [fontManager weightOfFont:nsFont];
        size = [nsFont pointSize];
    } else {
        // For custom fonts nsFont is nil.
        traits = description.italic() ? NSFontItalicTrait : 0;
        weight = toAppKitFontWeight(description.weight());
        size = description.computedPixelSize();
    }

    NSFontTraitMask substituteFontTraits = [fontManager traitsOfFont:substituteFont];
    NSInteger substituteFontWeight = [fontManager weightOfFont:substituteFont];

    if (traits != substituteFontTraits || weight != substituteFontWeight || !nsFont) {
        if (NSFont *bestVariation = [fontManager fontWithFamily:[substituteFont familyName] traits:traits weight:weight size:size]) {
            if (!nsFont || (([fontManager traitsOfFont:bestVariation] != substituteFontTraits || [fontManager weightOfFont:bestVariation] != substituteFontWeight)
                && [[bestVariation coveredCharacterSet] longCharacterIsMember:character]))
                substituteFont = bestVariation;
        }
    }

    substituteFont = description.usePrinterFont() ? [substituteFont printerFont] : [substituteFont screenFont];

    substituteFontTraits = [fontManager traitsOfFont:substituteFont];
    substituteFontWeight = [fontManager weightOfFont:substituteFont];

    FontPlatformData alternateFont(substituteFont, platformData.size(), platformData.isPrinterFont(),
        !isPlatformFont && isAppKitFontWeightBold(weight) && !isAppKitFontWeightBold(substituteFontWeight),
        !isPlatformFont && (traits & NSFontItalicTrait) && !(substituteFontTraits & NSFontItalicTrait),
        platformData.m_orientation);

    return fontForPlatformData(alternateFont);
}

RefPtr<Font> FontCache::similarFont(const FontDescription& description)
{
    // Attempt to find an appropriate font using a match based on 
    // the presence of keywords in the the requested names.  For example, we'll
    // match any name that contains "Arabic" to Geeza Pro.
    RefPtr<Font> font;
    for (unsigned i = 0; i < description.familyCount(); ++i) {
        const AtomicString& family = description.familyAt(i);
        if (family.isEmpty())
            continue;
        static String* matchWords[3] = { new String("Arabic"), new String("Pashto"), new String("Urdu") };
        DEPRECATED_DEFINE_STATIC_LOCAL(AtomicString, geezaStr, ("Geeza Pro", AtomicString::ConstructFromLiteral));
        for (int j = 0; j < 3 && !font; ++j) {
            if (family.contains(*matchWords[j], false))
                font = fontForFamily(description, geezaStr);
        }
    }
    return font.release();
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(AtomicString, timesStr, ("Times", AtomicString::ConstructFromLiteral));

    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.
    RefPtr<Font> font = fontForFamily(fontDescription, timesStr, false);
    if (font)
        return *font;

    // The Times fallback will almost always work, but in the highly unusual case where
    // the user doesn't have it, we fall back on Lucida Grande because that's
    // guaranteed to be there, according to Nathan Taylor. This is good enough
    // to avoid a crash at least.
    DEPRECATED_DEFINE_STATIC_LOCAL(AtomicString, lucidaGrandeStr, ("Lucida Grande", AtomicString::ConstructFromLiteral));
    return *fontForFamily(fontDescription, lucidaGrandeStr, false);
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    NSString *availableFamily;
    for (availableFamily in [fontManager availableFontFamilies]) {
        if ([familyName caseInsensitiveCompare:availableFamily] == NSOrderedSame)
            break;
    }

    if (!availableFamily) {
        // Match by PostScript name.
        for (NSString *availableFont in [fontManager availableFonts]) {
            if ([familyName caseInsensitiveCompare:availableFont] == NSOrderedSame) {
                NSFont *font = [NSFont fontWithName:availableFont size:10];
                NSInteger weight = [fontManager weightOfFont:font];
                traitsMasks.append(toTraitsMask([fontManager traitsOfFont:font], weight));
                break;
            }
        }
        return;
    }

    NSArray *fonts = [fontManager availableMembersOfFontFamily:availableFamily];
    traitsMasks.reserveCapacity([fonts count]);
    for (NSArray *fontInfo in fonts) {
        // Array indices must be hard coded because of lame AppKit API.
        NSInteger fontWeight = [[fontInfo objectAtIndex:2] intValue];
        NSFontTraitMask fontTraits = [[fontInfo objectAtIndex:3] unsignedIntValue];
        traitsMasks.uncheckedAppend(toTraitsMask(fontTraits, fontWeight));
    }
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    NSFontTraitMask traits = fontDescription.italic() ? NSFontItalicTrait : 0;
    NSInteger weight = toAppKitFontWeight(fontDescription.weight());
    float size = fontDescription.computedPixelSize();

    NSFont *nsFont = fontWithFamily(family, traits, weight, size);
    if (!nsFont) {
        if (!shouldAutoActivateFontIfNeeded(family))
            return nullptr;

        // Auto activate the font before looking for it a second time.
        // Ignore the result because we want to use our own algorithm to actually find the font.
        [NSFont fontWithName:family size:size];

        nsFont = fontWithFamily(family, traits, weight, size);
        if (!nsFont)
            return nullptr;
    }

    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSFontTraitMask actualTraits = 0;
    if (fontDescription.italic())
        actualTraits = [fontManager traitsOfFont:nsFont];
    NSInteger actualWeight = [fontManager weightOfFont:nsFont];

    NSFont *platformFont = fontDescription.usePrinterFont() ? [nsFont printerFont] : [nsFont screenFont];
    bool syntheticBold = isAppKitFontWeightBold(weight) && !isAppKitFontWeightBold(actualWeight);
    bool syntheticOblique = (traits & NSFontItalicTrait) && !(actualTraits & NSFontItalicTrait);

    return std::make_unique<FontPlatformData>(platformFont, size, fontDescription.usePrinterFont(), syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant());
}

} // namespace WebCore

#endif // !PLATFORM(IOS)
