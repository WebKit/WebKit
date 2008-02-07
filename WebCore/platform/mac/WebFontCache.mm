/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#import "WebFontCache.h"

#import <math.h>

#define SYNTHESIZED_FONT_TRAITS (NSBoldFontMask | NSItalicFontMask)

#define IMPORTANT_FONT_TRAITS (0 \
    | NSBoldFontMask \
    | NSCompressedFontMask \
    | NSCondensedFontMask \
    | NSExpandedFontMask \
    | NSItalicFontMask \
    | NSNarrowFontMask \
    | NSPosterFontMask \
    | NSSmallCapsFontMask \
)

#define DESIRED_WEIGHT 5

static BOOL acceptableChoice(NSFontTraitMask desiredTraits, int desiredWeight,
    NSFontTraitMask candidateTraits, int candidateWeight)
{
    desiredTraits &= ~SYNTHESIZED_FONT_TRAITS;
    return (candidateTraits & desiredTraits) == desiredTraits;
}

static BOOL betterChoice(NSFontTraitMask desiredTraits, int desiredWeight,
    NSFontTraitMask chosenTraits, int chosenWeight,
    NSFontTraitMask candidateTraits, int candidateWeight)
{
    if (!acceptableChoice(desiredTraits, desiredWeight, candidateTraits, candidateWeight)) {
        return NO;
    }
    
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
        NSBoldFontMask,
        0 };
    int i = 0;
    NSFontTraitMask mask;
    while ((mask = masks[i++])) {
        BOOL desired = (desiredTraits & mask) != 0;
        BOOL chosenHasUnwantedTrait = desired != ((chosenTraits & mask) != 0);
        BOOL candidateHasUnwantedTrait = desired != ((candidateTraits & mask) != 0);
        if (!candidateHasUnwantedTrait && chosenHasUnwantedTrait)
            return YES;
        if (!chosenHasUnwantedTrait && candidateHasUnwantedTrait)
            return NO;
    }
    
    int chosenWeightDelta = chosenWeight - desiredWeight;
    int candidateWeightDelta = candidateWeight - desiredWeight;
    
    int chosenWeightDeltaMagnitude = abs(chosenWeightDelta);
    int candidateWeightDeltaMagnitude = abs(candidateWeightDelta);
    
    // Smaller magnitude wins.
    // If both have same magnitude, tie breaker is that the smaller weight wins.
    // Otherwise, first font in the array wins (should almost never happen).
    if (candidateWeightDeltaMagnitude < chosenWeightDeltaMagnitude) {
        return YES;
    }
    if (candidateWeightDeltaMagnitude == chosenWeightDeltaMagnitude && candidateWeight < chosenWeight) {
        return YES;
    }
    
    return NO;
}

@implementation WebFontCache

// Family name is somewhat of a misnomer here.  We first attempt to find an exact match
// comparing the desiredFamily to the PostScript name of the installed fonts.  If that fails
// we then do a search based on the family names of the installed fonts.
+ (NSFont *)internalFontWithFamily:(NSString *)desiredFamily traits:(NSFontTraitMask)desiredTraits size:(float)size
{
    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    // Look for an exact match first.
    NSEnumerator *availableFonts = [[fontManager availableFonts] objectEnumerator];
    NSString *availableFont;
    NSFont *nameMatchedFont = nil;
    while ((availableFont = [availableFonts nextObject])) {
        if ([desiredFamily caseInsensitiveCompare:availableFont] == NSOrderedSame) {
            nameMatchedFont = [NSFont fontWithName:availableFont size:size];

            // Special case Osaka-Mono.  According to <rdar://problem/3999467>, we need to 
            // treat Osaka-Mono as fixed pitch.
            if ([desiredFamily caseInsensitiveCompare:@"Osaka-Mono"] == NSOrderedSame && desiredTraits == 0)
                return nameMatchedFont;

            NSFontTraitMask traits = [fontManager traitsOfFont:nameMatchedFont];
            if ((traits & desiredTraits) == desiredTraits)
                return [fontManager convertFont:nameMatchedFont toHaveTrait:desiredTraits];
            break;
        }
    }

    // Do a simple case insensitive search for a matching font family.
    // NSFontManager requires exact name matches.
    // This addresses the problem of matching arial to Arial, etc., but perhaps not all the issues.
    NSEnumerator *e = [[fontManager availableFontFamilies] objectEnumerator];
    NSString *availableFamily;
    while ((availableFamily = [e nextObject])) {
        if ([desiredFamily caseInsensitiveCompare:availableFamily] == NSOrderedSame)
            break;
    }

    if (!availableFamily)
        availableFamily = [nameMatchedFont familyName];

    // Found a family, now figure out what weight and traits to use.
    BOOL choseFont = false;
    int chosenWeight = 0;
    NSFontTraitMask chosenTraits = 0;

    NSArray *fonts = [fontManager availableMembersOfFontFamily:availableFamily];    
    unsigned n = [fonts count];
    unsigned i;
    for (i = 0; i < n; i++) {
        NSArray *fontInfo = [fonts objectAtIndex:i];

        // Array indices must be hard coded because of lame AppKit API.
        int fontWeight = [[fontInfo objectAtIndex:2] intValue];
        NSFontTraitMask fontTraits = [[fontInfo objectAtIndex:3] unsignedIntValue];

        BOOL newWinner;
        if (!choseFont)
            newWinner = acceptableChoice(desiredTraits, DESIRED_WEIGHT, fontTraits, fontWeight);
        else
            newWinner = betterChoice(desiredTraits, DESIRED_WEIGHT, chosenTraits, chosenWeight, fontTraits, fontWeight);

        if (newWinner) {
            choseFont = YES;
            chosenWeight = fontWeight;
            chosenTraits = fontTraits;

            if (chosenWeight == DESIRED_WEIGHT && (chosenTraits & IMPORTANT_FONT_TRAITS) == (desiredTraits & IMPORTANT_FONT_TRAITS))
                break;
        }
    }

    if (!choseFont)
        return nil;

    NSFont *font = [fontManager fontWithFamily:availableFamily traits:chosenTraits weight:chosenWeight size:size];

    if (!font)
        return nil;

    NSFontTraitMask actualTraits = 0;
    if (desiredTraits & (NSItalicFontMask | NSBoldFontMask))
        actualTraits = [[NSFontManager sharedFontManager] traitsOfFont:font];

    bool syntheticBold = (desiredTraits & NSBoldFontMask) && !(actualTraits & NSBoldFontMask);
    bool syntheticOblique = (desiredTraits & NSItalicFontMask) && !(actualTraits & NSItalicFontMask);

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

+ (NSFont *)fontWithFamily:(NSString *)desiredFamily traits:(NSFontTraitMask)desiredTraits size:(float)size
{
#ifndef BUILDING_ON_TIGER
    NSFont *font = [self internalFontWithFamily:desiredFamily traits:desiredTraits size:size];
    if (font)
        return font;

    // Auto activate the font before looking for it a second time.
    // Ignore the result because we want to use our own algorithm to actually find the font.
    [NSFont fontWithName:desiredFamily size:size];
#endif

    return [self internalFontWithFamily:desiredFamily traits:desiredTraits size:size];
}

@end
