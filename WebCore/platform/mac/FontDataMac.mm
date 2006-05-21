/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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
#import "Font.h"
#import "FontData.h"
#import "Color.h"

#import <wtf/Assertions.h>

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#import "FontCache.h"

#import "WebCoreSystemInterface.h"

#import "FloatRect.h"
#import "FontDescription.h"

#import <float.h>

#import <unicode/uchar.h>
#import <unicode/unorm.h>

// FIXME: Just temporary for the #defines of constants that we will eventually stop using.
#import "GlyphBuffer.h"

@interface NSFont (WebAppKitSecretAPI)
- (BOOL)_isFakeFixedPitch;
@end

namespace WebCore
{

#define SMALLCAPS_FONTSIZE_MULTIPLIER 0.7f
#define SPACE 0x0020
#define CONTEXT_DPI (72.0)
#define SCALE_EM_TO_UNITS(X, U_PER_EM) (X * ((1.0 * CONTEXT_DPI) / (CONTEXT_DPI * U_PER_EM)))

#if !ERROR_DISABLED
static NSString *pathFromFont(NSFont *font);
#endif

bool initFontData(FontData* fontData)
{
    ATSUStyle fontStyle;
    if (ATSUCreateStyle(&fontStyle) != noErr)
        return false;
    
    ATSUFontID fontId = wkGetNSFontATSUFontId(fontData->m_font.font);
    if (!fontId) {
        ATSUDisposeStyle(fontStyle);
        return false;
    }

    ATSUAttributeTag tag = kATSUFontTag;
    ByteCount size = sizeof(ATSUFontID);
    ATSUFontID *valueArray[1] = {&fontId};
    OSStatus status = ATSUSetAttributes(fontStyle, 1, &tag, &size, (void* const*)valueArray);
    if (status != noErr) {
        ATSUDisposeStyle(fontStyle);
        return false;
    }

    if (wkGetATSStyleGroup(fontStyle, &fontData->m_styleGroup) != noErr) {
        ATSUDisposeStyle(fontStyle);
        return false;
    }

    ATSUDisposeStyle(fontStyle);

    return true;
}

static NSString *webFallbackFontFamily(void)
{
    static NSString *webFallbackFontFamily = nil;
    if (!webFallbackFontFamily)
        webFallbackFontFamily = [[[NSFont systemFontOfSize:16.0] familyName] retain];
    return webFallbackFontFamily;
}

void FontData::platformInit()
{
    m_styleGroup = 0;
    m_ATSUStyleInitialized = false;
    m_ATSUMirrors = false;
    
    m_syntheticBoldOffset = m_font.syntheticBold ? ceilf([m_font.font pointSize] / 24.0f) : 0.f;
    
    bool failedSetup = false;
    if (!initFontData(this)) {
        // Ack! Something very bad happened, like a corrupt font.
        // Try looking for an alternate 'base' font for this renderer.

        // Special case hack to use "Times New Roman" in place of "Times".
        // "Times RO" is a common font whose family name is "Times".
        // It overrides the normal "Times" family font.
        // It also appears to have a corrupt regular variant.
        NSString *fallbackFontFamily;
        if ([[m_font.font familyName] isEqual:@"Times"])
            fallbackFontFamily = @"Times New Roman";
        else
            fallbackFontFamily = webFallbackFontFamily();
        
        // Try setting up the alternate font.
        // This is a last ditch effort to use a substitute font when something has gone wrong.
#if !ERROR_DISABLED
        NSFont *initialFont = m_font.font;
#endif
        m_font.font = [[NSFontManager sharedFontManager] convertFont:m_font.font toFamily:fallbackFontFamily];
#if !ERROR_DISABLED
        NSString *filePath = pathFromFont(initialFont);
        if (!filePath)
            filePath = @"not known";
#endif
        if (!initFontData(this)) {
            if ([fallbackFontFamily isEqual:@"Times New Roman"]) {
                // OK, couldn't setup Times New Roman as an alternate to Times, fallback
                // on the system font.  If this fails we have no alternative left.
                m_font.font = [[NSFontManager sharedFontManager] convertFont:m_font.font toFamily:webFallbackFontFamily()];
                if (!initFontData(this)) {
                    // We tried, Times, Times New Roman, and the system font. No joy. We have to give up.
                    LOG_ERROR("unable to initialize with font %@ at %@", initialFont, filePath);
                    failedSetup = true;
                }
            } else {
                // We tried the requested font and the system font. No joy. We have to give up.
                LOG_ERROR("unable to initialize with font %@ at %@", initialFont, filePath);
                failedSetup = true;
            }
        }

        // Report the problem.
        LOG_ERROR("Corrupt font detected, using %@ in place of %@ located at \"%@\".",
            [m_font.font familyName], [initialFont familyName], filePath);
    }

    // If all else fails, try to set up using the system font.
    // This is probably because Times and Times New Roman are both unavailable.
    if (failedSetup) {
        m_font.font = [NSFont systemFontOfSize:[m_font.font pointSize]];
        LOG_ERROR("failed to set up font, using system font %s", m_font.font);
        initFontData(this);
    }
    
    int iAscent;
    int iDescent;
    int iLineGap;
    unsigned unitsPerEm;
    wkGetFontMetrics(m_font.font, &iAscent, &iDescent, &iLineGap, &unitsPerEm); 
    float pointSize = [m_font.font pointSize];
    float fAscent = SCALE_EM_TO_UNITS(iAscent, unitsPerEm) * pointSize;
    float fDescent = -SCALE_EM_TO_UNITS(iDescent, unitsPerEm) * pointSize;
    float fLineGap = SCALE_EM_TO_UNITS(iLineGap, unitsPerEm) * pointSize;

    // We need to adjust Times, Helvetica, and Courier to closely match the
    // vertical metrics of their Microsoft counterparts that are the de facto
    // web standard. The AppKit adjustment of 20% is too big and is
    // incorrectly added to line spacing, so we use a 15% adjustment instead
    // and add it to the ascent.
    NSString *familyName = [m_font.font familyName];
    if ([familyName isEqualToString:@"Times"] || [familyName isEqualToString:@"Helvetica"] || [familyName isEqualToString:@"Courier"])
        fAscent += floorf(((fAscent + fDescent) * 0.15f) + 0.5f);

    m_ascent = lroundf(fAscent);
    m_descent = lroundf(fDescent);
    m_lineGap = lroundf(fLineGap);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;
    
    // Measure the actual character "x", because AppKit synthesizes X height rather than getting it from the font.
    // Unfortunately, NSFont will round this for us so we don't quite get the right value.
    NSGlyph xGlyph = m_characterToGlyphMap.glyphDataForCharacter('x', this).glyph;
    if (xGlyph) {
        NSRect xBox = [m_font.font boundingRectForGlyph:xGlyph];
        // Use the maximum of either width or height because "x" is nearly square
        // and web pages that foolishly use this metric for width will be laid out
        // poorly if we return an accurate height. Classic case is Times 13 point,
        // which has an "x" that is 7x6 pixels.
        m_xHeight = MAX(NSMaxX(xBox), NSMaxY(xBox));
    } else
        m_xHeight = [m_font.font xHeight];

    [m_font.font retain];
}

void FontData::platformDestroy()
{
    if (m_styleGroup)
        wkReleaseStyleGroup(m_styleGroup);

    if (m_ATSUStyleInitialized)
        ATSUDisposeStyle(m_ATSUStyle);
        
    [m_font.font release];
}

FontData* FontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        NS_DURING
            float size = [m_font.font pointSize] * SMALLCAPS_FONTSIZE_MULTIPLIER;
            FontPlatformData smallCapsFont([[NSFontManager sharedFontManager] convertFont:m_font.font toSize:size]);
            
            // AppKit resets the type information (screen/printer) when you convert a font to a different size.
            // We have to fix up the font that we're handed back.
            smallCapsFont.font = fontDescription.usePrinterFont() ? [smallCapsFont.font printerFont] : [smallCapsFont.font screenFont];

            if (smallCapsFont.font) {
                NSFontManager *fontManager = [NSFontManager sharedFontManager];
                NSFontTraitMask fontTraits = [fontManager traitsOfFont:m_font.font];

                if (m_font.syntheticBold)
                    fontTraits |= NSBoldFontMask;
                if (m_font.syntheticOblique)
                    fontTraits |= NSItalicFontMask;

                NSFontTraitMask smallCapsFontTraits = [fontManager traitsOfFont:smallCapsFont.font];
                smallCapsFont.syntheticBold = (fontTraits & NSBoldFontMask) && !(smallCapsFontTraits & NSBoldFontMask);
                smallCapsFont.syntheticOblique = (fontTraits & NSItalicFontMask) && !(smallCapsFontTraits & NSItalicFontMask);

                m_smallCapsFontData = FontCache::getCachedFontData(&smallCapsFont);
            }
        NS_HANDLER
            NSLog(@"uncaught exception selecting font for small caps: %@", localException);
        NS_ENDHANDLER
    }
    return m_smallCapsFontData;
}

bool FontData::containsCharacters(const UChar* characters, int length) const
{
    NSString *string = [[NSString alloc] initWithCharactersNoCopy:(UniChar*)characters length:length freeWhenDone:NO];
    NSCharacterSet *set = [[m_font.font coveredCharacterSet] invertedSet];
    bool result = set && [string rangeOfCharacterFromSet:set].location == NSNotFound;
    [string release];
    return result;
}

#if !ERROR_DISABLED

static NSString *pathFromFont(NSFont *font)
{
    FSSpec oFile;
    OSStatus status = ATSFontGetFileSpecification(FMGetATSFontRefFromFont((FMFont)wkGetNSFontATSUFontId(font)), &oFile);
    if (status == noErr) {
        OSErr err;
        FSRef fileRef;
        err = FSpMakeFSRef(&oFile, &fileRef);
        if (err == noErr) {
            UInt8 filePathBuffer[PATH_MAX];
            status = FSRefMakePath(&fileRef, filePathBuffer, PATH_MAX);
            if (status == noErr)
                return [NSString stringWithUTF8String:(const char *)filePathBuffer];
        }
    }
    return nil;
}

#endif

void FontData::determinePitch()
{
    NSFont* f = m_font.font;
    // Special case Osaka-Mono.
    // According to <rdar://problem/3999467>, we should treat Osaka-Mono as fixed pitch.
    // Note that the AppKit does not report Osaka-Mono as fixed pitch.

    // Special case MS-PGothic.
    // According to <rdar://problem/4032938, we should not treat MS-PGothic as fixed pitch.
    // Note that AppKit does report MS-PGothic as fixed pitch.

    NSString *name = [f fontName];
    m_treatAsFixedPitch = ([f isFixedPitch] || [f _isFakeFixedPitch] || 
           [name caseInsensitiveCompare:@"Osaka-Mono"] == NSOrderedSame) && 
          ![name caseInsensitiveCompare:@"MS-PGothic"] == NSOrderedSame;
}

float FontData::platformWidthForGlyph(Glyph glyph, UChar) const
{
    NSFont *font = m_font.font;
    float pointSize = [font pointSize];
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    CGSize advance;
    if (!wkGetGlyphTransformedAdvances(font, &m, &glyph, &advance)) {
        LOG_ERROR("Unable to cache glyph widths for %@ %f", [font displayName], pointSize);
        advance.width = 0;
    }
    return advance.width + m_syntheticBoldOffset;
}

}
