/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "SimpleFontData.h"

#import "BlockExceptions.h"
#import "Color.h"
#import "FloatRect.h"
#import "Font.h"
#import "FontCache.h"
#import "FontDescription.h"
#import "SharedBuffer.h"
#import "WebCoreSystemInterface.h"
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <float.h>
#import <unicode/uchar.h>
#import <wtf/Assertions.h>
#import <wtf/StdLibExtras.h>
#import <wtf/RetainPtr.h>

@interface NSFont (WebAppKitSecretAPI)
- (BOOL)_isFakeFixedPitch;
@end

namespace WebCore {
  
const float smallCapsFontSizeMultiplier = 0.7f;
const float contextDPI = 72.0f;
static inline float scaleEmToUnits(float x, unsigned unitsPerEm) { return x * (contextDPI / (contextDPI * unitsPerEm)); }

static bool initFontData(SimpleFontData* fontData)
{
    if (!fontData->m_font.cgFont())
        return false;

#ifdef BUILDING_ON_TIGER
    ATSUStyle fontStyle;
    if (ATSUCreateStyle(&fontStyle) != noErr)
        return false;

    ATSUFontID fontId = fontData->m_font.m_atsuFontID;
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
#endif

    return true;
}

static NSString *webFallbackFontFamily(void)
{
    DEFINE_STATIC_LOCAL(RetainPtr<NSString>, webFallbackFontFamily, ([[NSFont systemFontOfSize:16.0f] familyName]));
    return webFallbackFontFamily.get();
}

#if !ERROR_DISABLED
#ifdef __LP64__
static NSString* pathFromFont(NSFont*)
{
    // FMGetATSFontRefFromFont is not available in 64-bit. As pathFromFont is only used for debugging
    // purposes, returning nil is acceptable.
    return nil;
}
#else
static NSString* pathFromFont(NSFont *font)
{
#ifndef BUILDING_ON_TIGER
    ATSFontRef atsFont = FMGetATSFontRefFromFont(CTFontGetPlatformFont(toCTFontRef(font), 0));
#else
    ATSFontRef atsFont = FMGetATSFontRefFromFont(wkGetNSFontATSUFontId(font));
#endif
    FSRef fileRef;

#ifndef BUILDING_ON_TIGER
    OSStatus status = ATSFontGetFileReference(atsFont, &fileRef);
    if (status != noErr)
        return nil;
#else
    FSSpec oFile;
    OSStatus status = ATSFontGetFileSpecification(atsFont, &oFile);
    if (status != noErr)
        return nil;

    status = FSpMakeFSRef(&oFile, &fileRef);
    if (status != noErr)
        return nil;
#endif

    UInt8 filePathBuffer[PATH_MAX];
    status = FSRefMakePath(&fileRef, filePathBuffer, PATH_MAX);
    if (status == noErr)
        return [NSString stringWithUTF8String:(const char*)filePathBuffer];

    return nil;
}
#endif // __LP64__
#endif // !ERROR_DISABLED

void SimpleFontData::platformInit()
{
#ifdef BUILDING_ON_TIGER
    m_styleGroup = 0;
#endif
#if USE(ATSUI)
    m_ATSUStyleInitialized = false;
    m_ATSUMirrors = false;
    m_checkedShapesArabic = false;
    m_shapesArabic = false;
#endif

    m_syntheticBoldOffset = m_font.m_syntheticBold ? 1.0f : 0.f;

    bool failedSetup = false;
    if (!initFontData(this)) {
        // Ack! Something very bad happened, like a corrupt font.
        // Try looking for an alternate 'base' font for this renderer.

        // Special case hack to use "Times New Roman" in place of "Times".
        // "Times RO" is a common font whose family name is "Times".
        // It overrides the normal "Times" family font.
        // It also appears to have a corrupt regular variant.
        NSString *fallbackFontFamily;
        if ([[m_font.font() familyName] isEqual:@"Times"])
            fallbackFontFamily = @"Times New Roman";
        else
            fallbackFontFamily = webFallbackFontFamily();
        
        // Try setting up the alternate font.
        // This is a last ditch effort to use a substitute font when something has gone wrong.
#if !ERROR_DISABLED
        RetainPtr<NSFont> initialFont = m_font.font();
#endif
        if (m_font.font())
            m_font.setFont([[NSFontManager sharedFontManager] convertFont:m_font.font() toFamily:fallbackFontFamily]);
        else
            m_font.setFont([NSFont fontWithName:fallbackFontFamily size:m_font.size()]);
#if !ERROR_DISABLED
        NSString *filePath = pathFromFont(initialFont.get());
        if (!filePath)
            filePath = @"not known";
#endif
        if (!initFontData(this)) {
            if ([fallbackFontFamily isEqual:@"Times New Roman"]) {
                // OK, couldn't setup Times New Roman as an alternate to Times, fallback
                // on the system font.  If this fails we have no alternative left.
                m_font.setFont([[NSFontManager sharedFontManager] convertFont:m_font.font() toFamily:webFallbackFontFamily()]);
                if (!initFontData(this)) {
                    // We tried, Times, Times New Roman, and the system font. No joy. We have to give up.
                    LOG_ERROR("unable to initialize with font %@ at %@", initialFont.get(), filePath);
                    failedSetup = true;
                }
            } else {
                // We tried the requested font and the system font. No joy. We have to give up.
                LOG_ERROR("unable to initialize with font %@ at %@", initialFont.get(), filePath);
                failedSetup = true;
            }
        }

        // Report the problem.
        LOG_ERROR("Corrupt font detected, using %@ in place of %@ located at \"%@\".",
            [m_font.font() familyName], [initialFont.get() familyName], filePath);
    }

    // If all else fails, try to set up using the system font.
    // This is probably because Times and Times New Roman are both unavailable.
    if (failedSetup) {
        m_font.setFont([NSFont systemFontOfSize:[m_font.font() pointSize]]);
        LOG_ERROR("failed to set up font, using system font %s", m_font.font());
        initFontData(this);
    }
    
    int iAscent;
    int iDescent;
    int iLineGap;
#ifdef BUILDING_ON_TIGER
    wkGetFontMetrics(m_font.cgFont(), &iAscent, &iDescent, &iLineGap, &m_unitsPerEm);
#else
    iAscent = CGFontGetAscent(m_font.cgFont());
    iDescent = CGFontGetDescent(m_font.cgFont());
    iLineGap = CGFontGetLeading(m_font.cgFont());
    m_unitsPerEm = CGFontGetUnitsPerEm(m_font.cgFont());
#endif

    float pointSize = m_font.m_size;
    float fAscent = scaleEmToUnits(iAscent, m_unitsPerEm) * pointSize;
    float fDescent = -scaleEmToUnits(iDescent, m_unitsPerEm) * pointSize;
    float fLineGap = scaleEmToUnits(iLineGap, m_unitsPerEm) * pointSize;

    // We need to adjust Times, Helvetica, and Courier to closely match the
    // vertical metrics of their Microsoft counterparts that are the de facto
    // web standard. The AppKit adjustment of 20% is too big and is
    // incorrectly added to line spacing, so we use a 15% adjustment instead
    // and add it to the ascent.
    NSString *familyName = [m_font.font() familyName];
    if ([familyName isEqualToString:@"Times"] || [familyName isEqualToString:@"Helvetica"] || [familyName isEqualToString:@"Courier"])
        fAscent += floorf(((fAscent + fDescent) * 0.15f) + 0.5f);
    else if ([familyName isEqualToString:@"Geeza Pro"]) {
        // Geeza Pro has glyphs that draw slightly above the ascent or far below the descent. Adjust
        // those vertical metrics to better match reality, so that diacritics at the bottom of one line
        // do not overlap diacritics at the top of the next line.
        fAscent *= 1.08f;
        fDescent *= 2.f;
    }

    m_ascent = lroundf(fAscent);
    m_descent = lroundf(fDescent);
    m_lineGap = lroundf(fLineGap);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;
    
    // Hack Hiragino line metrics to allow room for marked text underlines.
    // <rdar://problem/5386183>
    if (m_descent < 3 && m_lineGap >= 3 && [familyName hasPrefix:@"Hiragino"]) {
        m_lineGap -= 3 - m_descent;
        m_descent = 3;
    }
    
    // Measure the actual character "x", because AppKit synthesizes X height rather than getting it from the font.
    // Unfortunately, NSFont will round this for us so we don't quite get the right value.
    GlyphPage* glyphPageZero = GlyphPageTreeNode::getRootChild(this, 0)->page();
    NSGlyph xGlyph = glyphPageZero ? glyphPageZero->glyphDataForCharacter('x').glyph : 0;
    if (xGlyph) {
        NSRect xBox = [m_font.font() boundingRectForGlyph:xGlyph];
        // Use the maximum of either width or height because "x" is nearly square
        // and web pages that foolishly use this metric for width will be laid out
        // poorly if we return an accurate height. Classic case is Times 13 point,
        // which has an "x" that is 7x6 pixels.
        m_xHeight = MAX(NSMaxX(xBox), NSMaxY(xBox));
    } else
        m_xHeight = [m_font.font() xHeight];
}

void SimpleFontData::platformDestroy()
{
#ifdef BUILDING_ON_TIGER
    if (m_styleGroup)
        wkReleaseStyleGroup(m_styleGroup);
#endif
#if USE(ATSUI)
    if (m_ATSUStyleInitialized)
        ATSUDisposeStyle(m_ATSUStyle);
#endif
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        if (isCustomFont()) {
            FontPlatformData smallCapsFontData(m_font);
            smallCapsFontData.m_size = smallCapsFontData.m_size * smallCapsFontSizeMultiplier;
            m_smallCapsFontData = new SimpleFontData(smallCapsFontData, true, false);
        } else {
            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            float size = [m_font.font() pointSize] * smallCapsFontSizeMultiplier;
            FontPlatformData smallCapsFont([[NSFontManager sharedFontManager] convertFont:m_font.font() toSize:size]);
            
            // AppKit resets the type information (screen/printer) when you convert a font to a different size.
            // We have to fix up the font that we're handed back.
            smallCapsFont.setFont(fontDescription.usePrinterFont() ? [smallCapsFont.font() printerFont] : [smallCapsFont.font() screenFont]);

            if (smallCapsFont.font()) {
                NSFontManager *fontManager = [NSFontManager sharedFontManager];
                NSFontTraitMask fontTraits = [fontManager traitsOfFont:m_font.font()];

                if (m_font.m_syntheticBold)
                    fontTraits |= NSBoldFontMask;
                if (m_font.m_syntheticOblique)
                    fontTraits |= NSItalicFontMask;

                NSFontTraitMask smallCapsFontTraits = [fontManager traitsOfFont:smallCapsFont.font()];
                smallCapsFont.m_syntheticBold = (fontTraits & NSBoldFontMask) && !(smallCapsFontTraits & NSBoldFontMask);
                smallCapsFont.m_syntheticOblique = (fontTraits & NSItalicFontMask) && !(smallCapsFontTraits & NSItalicFontMask);

                m_smallCapsFontData = fontCache()->getCachedFontData(&smallCapsFont);
            }
            END_BLOCK_OBJC_EXCEPTIONS;
        }
    }
    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    NSString *string = [[NSString alloc] initWithCharactersNoCopy:const_cast<unichar*>(characters) length:length freeWhenDone:NO];
    NSCharacterSet *set = [[m_font.font() coveredCharacterSet] invertedSet];
    bool result = set && [string rangeOfCharacterFromSet:set].location == NSNotFound;
    [string release];
    return result;
}

void SimpleFontData::determinePitch()
{
    NSFont* f = m_font.font();
    // Special case Osaka-Mono.
    // According to <rdar://problem/3999467>, we should treat Osaka-Mono as fixed pitch.
    // Note that the AppKit does not report Osaka-Mono as fixed pitch.

    // Special case MS-PGothic.
    // According to <rdar://problem/4032938>, we should not treat MS-PGothic as fixed pitch.
    // Note that AppKit does report MS-PGothic as fixed pitch.

    // Special case MonotypeCorsiva
    // According to <rdar://problem/5454704>, we should not treat MonotypeCorsiva as fixed pitch.
    // Note that AppKit does report MonotypeCorsiva as fixed pitch.

    NSString *name = [f fontName];
    m_treatAsFixedPitch = ([f isFixedPitch] || [f _isFakeFixedPitch] ||
           [name caseInsensitiveCompare:@"Osaka-Mono"] == NSOrderedSame) &&
           [name caseInsensitiveCompare:@"MS-PGothic"] != NSOrderedSame &&
           [name caseInsensitiveCompare:@"MonotypeCorsiva"] != NSOrderedSame;
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    NSFont* font = m_font.font();
    float pointSize = m_font.m_size;
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    CGSize advance;
    if (!wkGetGlyphTransformedAdvances(m_font.cgFont(), font, &m, &glyph, &advance)) {
        LOG_ERROR("Unable to cache glyph widths for %@ %f", [font displayName], pointSize);
        advance.width = 0;
    }
    return advance.width + m_syntheticBoldOffset;
}

#if USE(ATSUI)
void SimpleFontData::checkShapesArabic() const
{
    ASSERT(!m_checkedShapesArabic);

    m_checkedShapesArabic = true;
    
    ATSUFontID fontID = m_font.m_atsuFontID;
    if (!fontID) {
        LOG_ERROR("unable to get ATSUFontID for %@", m_font.font());
        return;
    }

    // This function is called only on fonts that contain Arabic glyphs. Our
    // heuristic is that if such a font has a glyph metamorphosis table, then
    // it includes shaping information for Arabic.
    FourCharCode tables[] = { 'morx', 'mort' };
    for (unsigned i = 0; i < sizeof(tables) / sizeof(tables[0]); ++i) {
        ByteCount tableSize;
        OSStatus status = ATSFontGetTable(fontID, tables[i], 0, 0, 0, &tableSize);
        if (status == noErr) {
            m_shapesArabic = true;
            return;
        }

        if (status != kATSInvalidFontTableAccess)
            LOG_ERROR("ATSFontGetTable failed (%d)", status);
    }
}
#endif

#if USE(CORE_TEXT)
CTFontRef SimpleFontData::getCTFont() const
{
    if (getNSFont())
        return toCTFontRef(getNSFont());
    if (!m_CTFont)
        m_CTFont.adoptCF(CTFontCreateWithGraphicsFont(m_font.cgFont(), m_font.size(), NULL, NULL));
    return m_CTFont.get();
}

CFDictionaryRef SimpleFontData::getCFStringAttributes() const
{
    if (m_CFStringAttributes)
        return m_CFStringAttributes.get();

    static const float kerningAdjustmentValue = 0;
    static CFNumberRef kerningAdjustment = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &kerningAdjustmentValue);

    static const int ligaturesNotAllowedValue = 0;
    static CFNumberRef ligaturesNotAllowed = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &ligaturesNotAllowedValue);

    static const int ligaturesAllowedValue = 1;
    static CFNumberRef ligaturesAllowed = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &ligaturesAllowedValue);

    static const void* attributeKeys[] = { kCTFontAttributeName, kCTKernAttributeName, kCTLigatureAttributeName };
    const void* attributeValues[] = { getCTFont(), kerningAdjustment, platformData().allowsLigatures() ? ligaturesAllowed : ligaturesNotAllowed };

    m_CFStringAttributes.adoptCF(CFDictionaryCreate(NULL, attributeKeys, attributeValues, sizeof(attributeKeys) / sizeof(*attributeKeys), &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    return m_CFStringAttributes.get();
}

#endif

} // namespace WebCore
