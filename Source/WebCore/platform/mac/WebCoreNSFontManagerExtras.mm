/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "WebCoreNSFontManagerExtras.h"

#if PLATFORM(MAC)

#import "ColorMac.h"
#import "FontAttributeChanges.h"
#import <AppKit/NSFontManager.h>

namespace WebCore {

static NSFont *firstFontConversionSpecimen(NSFontManager *fontManager)
{
    const double standardWeight = 5.;
    return [fontManager fontWithFamily:@"Helvetica" traits:0 weight:standardWeight size:10.];
}

static NSFont *secondFontConversionSpecimen(NSFontManager *fontManager)
{
    const double standardBoldWeight = 9.;
    return [fontManager fontWithFamily:@"Times" traits:NSFontItalicTrait weight:standardBoldWeight size:12.];
}

static FontChanges computedFontChanges(NSFontManager *fontManager, NSFont *originalFontA, NSFont *convertedFontA, NSFont *convertedFontB)
{
    if (!convertedFontA || !convertedFontB)
        return { };

    // Since there's still no way to directly ask NSFontManager what style change it's going
    // to do we instead pass two "specimen" fonts to it and let it change them. We then deduce
    // what style change it was doing by looking at what happened to each of the two fonts.
    // So if it was making the text bold, both fonts will be bold after the fact.
    // This logic was originally from WebHTMLView, and is now in WebCore so that it is shared
    // between WebKitLegacy and WebKit.
    const double minimumBoldWeight = 7.;

    NSString *convertedFamilyNameA = convertedFontA.familyName;
    NSString *convertedFamilyNameB = convertedFontB.familyName;

    auto convertedPointSizeA = convertedFontA.pointSize;
    auto convertedPointSizeB = convertedFontB.pointSize;

    auto convertedFontWeightA = [fontManager weightOfFont:convertedFontA];
    auto convertedFontWeightB = [fontManager weightOfFont:convertedFontB];

    bool convertedFontAIsItalic = !!([fontManager traitsOfFont:convertedFontA] & NSItalicFontMask);
    bool convertedFontBIsItalic = !!([fontManager traitsOfFont:convertedFontB] & NSItalicFontMask);

    bool convertedFontAIsBold = convertedFontWeightA > minimumBoldWeight;

    FontChanges changes;
    if ([convertedFamilyNameA isEqualToString:convertedFamilyNameB]) {
        changes.setFontName(convertedFontA.fontName);
        changes.setFontFamily(convertedFamilyNameA);
    }

    int originalPointSizeA = originalFontA.pointSize;
    if (convertedPointSizeA == convertedPointSizeB)
        changes.setFontSize(convertedPointSizeA);
    else if (convertedPointSizeA < originalPointSizeA)
        changes.setFontSizeDelta(-1);
    else if (convertedPointSizeA > originalPointSizeA)
        changes.setFontSizeDelta(1);

    if (convertedFontWeightA == convertedFontWeightB)
        changes.setBold(convertedFontAIsBold);

    if (convertedFontAIsItalic == convertedFontBIsItalic)
        changes.setItalic(convertedFontAIsItalic);

    return changes;
}

FontChanges computedFontChanges(NSFontManager *fontManager)
{
    NSFont *originalFontA = firstFontConversionSpecimen(fontManager);
    return computedFontChanges(fontManager, originalFontA, [fontManager convertFont:originalFontA], [fontManager convertFont:secondFontConversionSpecimen(fontManager)]);
}

FontAttributeChanges computedFontAttributeChanges(NSFontManager *fontManager, id attributeConverter)
{
    FontAttributeChanges changes;

    auto shadow = adoptNS([[NSShadow alloc] init]);
    [shadow setShadowOffset:NSMakeSize(1, 1)];

    NSFont *originalFontA = firstFontConversionSpecimen(fontManager);
    NSDictionary *originalAttributesA = @{ NSFontAttributeName : originalFontA };
    NSDictionary *originalAttributesB = @{
        NSBackgroundColorAttributeName : NSColor.blackColor,
        NSFontAttributeName : secondFontConversionSpecimen(fontManager),
        NSForegroundColorAttributeName : NSColor.whiteColor,
        NSShadowAttributeName : shadow.get(),
        NSStrikethroughStyleAttributeName : @(NSUnderlineStyleSingle),
        NSSuperscriptAttributeName : @1,
        NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle)
    };

    NSDictionary *convertedAttributesA = [attributeConverter convertAttributes:originalAttributesA];
    NSDictionary *convertedAttributesB = [attributeConverter convertAttributes:originalAttributesB];

    NSColor *convertedBackgroundColorA = [convertedAttributesA objectForKey:NSBackgroundColorAttributeName];
    if (convertedBackgroundColorA == [convertedAttributesB objectForKey:NSBackgroundColorAttributeName])
        changes.setBackgroundColor(colorFromNSColor(convertedBackgroundColorA ?: NSColor.clearColor));

    changes.setFontChanges(computedFontChanges(fontManager, originalFontA, [convertedAttributesA objectForKey:NSFontAttributeName], [convertedAttributesB objectForKey:NSFontAttributeName]));

    NSColor *convertedForegroundColorA = [convertedAttributesA objectForKey:NSForegroundColorAttributeName];
    if (convertedForegroundColorA == [convertedAttributesB objectForKey:NSForegroundColorAttributeName])
        changes.setForegroundColor(colorFromNSColor(convertedForegroundColorA ?: NSColor.blackColor));

    NSShadow *convertedShadow = [convertedAttributesA objectForKey:NSShadowAttributeName];
    if (convertedShadow) {
        FloatSize offset { static_cast<float>(convertedShadow.shadowOffset.width), static_cast<float>(convertedShadow.shadowOffset.height) };
        changes.setShadow({ colorFromNSColor(convertedShadow.shadowColor ?: NSColor.blackColor), offset, convertedShadow.shadowBlurRadius });
    } else if (![convertedAttributesB objectForKey:NSShadowAttributeName])
        changes.setShadow({ });

    int convertedSuperscriptA = [[convertedAttributesA objectForKey:NSSuperscriptAttributeName] intValue];
    if (convertedSuperscriptA == [[convertedAttributesB objectForKey:NSSuperscriptAttributeName] intValue]) {
        if (convertedSuperscriptA > 0)
            changes.setVerticalAlign(VerticalAlignChange::Superscript);
        else if (convertedSuperscriptA < 0)
            changes.setVerticalAlign(VerticalAlignChange::Subscript);
        else
            changes.setVerticalAlign(VerticalAlignChange::Baseline);
    }

    int convertedStrikeThroughA = [[convertedAttributesA objectForKey:NSStrikethroughStyleAttributeName] intValue];
    if (convertedStrikeThroughA == [[convertedAttributesB objectForKey:NSStrikethroughStyleAttributeName] intValue])
        changes.setStrikeThrough(convertedStrikeThroughA != NSUnderlineStyleNone);

    int convertedUnderlineA = [[convertedAttributesA objectForKey:NSUnderlineStyleAttributeName] intValue];
    if (convertedUnderlineA == [[convertedAttributesB objectForKey:NSUnderlineStyleAttributeName] intValue])
        changes.setUnderline(convertedUnderlineA != NSUnderlineStyleNone);

    return changes;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
