/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#import "FontCache.h"
#import "FontPlatformData.h"
#import "Font.h"
#import "WebTextRendererFactory.h"
#import "WebCoreSystemInterface.h"

namespace WebCore
{

const FontData* FontCache::getFontData(const Font& font, int& familyIndex)
{
    FontPlatformData platformData = [[WebTextRendererFactory sharedFactory] fontWithDescription:&font.fontDescription() familyIndex:&familyIndex];
    if (!platformData.font)
        return 0;
    return [[WebTextRendererFactory sharedFactory] rendererWithFont: platformData];
}

const FontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    NSFont* nsFont = font.getNSFont();
    NSString *string = [[NSString alloc] initWithCharactersNoCopy:(UniChar*)characters length:length freeWhenDone:NO];
    NSFont* substituteFont = wkGetFontInLanguageForRange(nsFont, string, NSMakeRange(0, [string length]));
    if (!substituteFont && [string length] == 1)
        substituteFont = wkGetFontInLanguageForCharacter(nsFont, [string characterAtIndex:0]);
    [string release];
    
    // Now that we have a substitute font, attempt to match it to the best variation.
    // If we have a good match return that, otherwise return the font the AppKit has found.
    if (substituteFont) {
        NSFontTraitMask traits = 0;
        if (font.fontDescription().italic())
            traits |= NSItalicFontMask;
        if (font.fontDescription().weight() >= WebCore::cBoldWeight)
            traits |= NSBoldFontMask;
        float size = font.fontDescription().computedPixelSize();
    
        NSFontManager *manager = [NSFontManager sharedFontManager];
        NSFont *bestVariation = [manager fontWithFamily:[substituteFont familyName]
            traits:traits
            weight:[manager weightOfFont:nsFont]
            size:size];
        if (bestVariation)
            substituteFont = bestVariation;
        
        substituteFont = font.fontDescription().usePrinterFont() ? [substituteFont printerFont] : [substituteFont screenFont];

        NSFontTraitMask actualTraits = [manager traitsOfFont:substituteFont];
        FontPlatformData alternateFont(substituteFont, 
                                       (traits & NSBoldFontMask) && !(actualTraits & NSBoldFontMask),
                                       (traits & NSItalicFontMask) && !(actualTraits & NSItalicFontMask));
        return [[WebTextRendererFactory sharedFactory] rendererWithFont: alternateFont];
    }

    return 0;
}

}