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

// FIXME: FATAL seems like a bad idea; lets stop using it.

#define SMALLCAPS_FONTSIZE_MULTIPLIER 0.7f
#define SYNTHETIC_OBLIQUE_ANGLE 14

// Should be more than enough for normal usage.
#define NUM_SUBSTITUTE_FONT_MAPS 10

#define SPACE 0x0020
#define NO_BREAK_SPACE 0x00A0
#define ZERO_WIDTH_SPACE 0x200B
#define POP_DIRECTIONAL_FORMATTING 0x202C
#define LEFT_TO_RIGHT_OVERRIDE 0x202D
#define RIGHT_TO_LEFT_OVERRIDE 0x202E

// MAX_GLYPH_EXPANSION is the maximum numbers of glyphs that may be
// use to represent a single Unicode code point.
#define MAX_GLYPH_EXPANSION 4
#define LOCAL_BUFFER_SIZE 2048

// Covers Latin-1.
#define INITIAL_BLOCK_SIZE 0x200

// Get additional blocks of glyphs and widths in bigger chunks.
// This will typically be for other character sets.
#define INCREMENTAL_BLOCK_SIZE 0x400

#define CONTEXT_DPI (72.0)
#define SCALE_EM_TO_UNITS(X, U_PER_EM) (X * ((1.0 * CONTEXT_DPI) / (CONTEXT_DPI * U_PER_EM)))

#define WKGlyphVectorSize (50 * 32)

typedef float WebGlyphWidth;

struct WidthMap {
    WidthMap() :next(0), widths(0) {}

    ATSGlyphRef startRange;
    ATSGlyphRef endRange;
    WidthMap *next;
    WebGlyphWidth *widths;
};

typedef struct GlyphEntry {
    Glyph glyph;
    const FontData *renderer;
} GlyphEntry;

struct GlyphMap {
    UChar startRange;
    UChar endRange;
    GlyphMap *next;
    GlyphEntry *glyphs;
};

static WidthMap *extendWidthMap(const FontData *, ATSGlyphRef);
static ATSGlyphRef extendGlyphMap(const FontData *, UChar32);

static void freeWidthMap(WidthMap *);
static void freeGlyphMap(GlyphMap *);

static bool setUpFont(FontData *);

static bool fillStyleWithAttributes(ATSUStyle style, NSFont *theFont);

#if !ERROR_DISABLED
static NSString *pathFromFont(NSFont *font);
#endif

// Map utility functions

float FontData::widthForGlyph(Glyph glyph) const
{
    WidthMap *map;
    for (map = m_glyphToWidthMap; 1; map = map->next) {
        if (!map)
            map = extendWidthMap(this, glyph);
        if (glyph >= map->startRange && glyph <= map->endRange)
            break;
    }
    float width = map->widths[glyph - map->startRange];
    if (width >= 0)
        return width;
    NSFont *font = m_font.font;
    float pointSize = [font pointSize];
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    CGSize advance;
    if (!wkGetGlyphTransformedAdvances(font, &m, &glyph, &advance)) {
        LOG_ERROR("Unable to cache glyph widths for %@ %f", [font displayName], pointSize);
        advance.width = 0;
    }
    width = advance.width + m_syntheticBoldOffset;
    map->widths[glyph - map->startRange] = width;
    return width;
}

static NSString *webFallbackFontFamily(void)
{
    static NSString *webFallbackFontFamily = nil;
    if (!webFallbackFontFamily)
	webFallbackFontFamily = [[[NSFont systemFontOfSize:16.0] familyName] retain];
    return webFallbackFontFamily;
}

FontData::FontData(const FontPlatformData& f)
:m_styleGroup(0), m_font(f), m_characterToGlyphMap(0), m_glyphToWidthMap(0), m_treatAsFixedPitch(false),
 m_smallCapsFontData(0), m_ATSUStyleInitialized(false), m_ATSUMirrors(false)
{    
    m_font = f;

    m_syntheticBoldOffset = f.syntheticBold ? ceilf([f.font pointSize] / 24.0f) : 0.f;
    
    bool failedSetup = false;
    if (!setUpFont(this)) {
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
        if (!setUpFont(this)) {
	    if ([fallbackFontFamily isEqual:@"Times New Roman"]) {
		// OK, couldn't setup Times New Roman as an alternate to Times, fallback
		// on the system font.  If this fails we have no alternative left.
		m_font.font = [[NSFontManager sharedFontManager] convertFont:m_font.font toFamily:webFallbackFontFamily()];
		if (!setUpFont(this)) {
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
        setUpFont(this);
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

    [m_font.font retain];
}

FontData::~FontData()
{
    if (m_styleGroup)
        wkReleaseStyleGroup(m_styleGroup);

    freeWidthMap(m_glyphToWidthMap);
    freeGlyphMap(m_characterToGlyphMap);

    if (m_ATSUStyleInitialized)
        ATSUDisposeStyle(m_ATSUStyle);
        
    [m_font.font release];
    
    // We only get deleted when the cache gets cleared.  Since the smallCapsRenderer is also in that cache,
    // it will be deleted then, so we don't need to do anything here.
}

float FontData::xHeight() const
{
    // Measure the actual character "x", because AppKit synthesizes X height rather than getting it from the font.
    // Unfortunately, NSFont will round this for us so we don't quite get the right value.
    const FontData *renderer = this;
    NSGlyph xGlyph = glyphForCharacter(&renderer, 'x');
    if (xGlyph) {
        NSRect xBox = [m_font.font boundingRectForGlyph:xGlyph];
        // Use the maximum of either width or height because "x" is nearly square
        // and web pages that foolishly use this metric for width will be laid out
        // poorly if we return an accurate height. Classic case is Times 13 point,
        // which has an "x" that is 7x6 pixels.
        return MAX(NSMaxX(xBox), NSMaxY(xBox));
    }

    return [m_font.font xHeight];
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

// Nasty hack to determine if we should round or ceil space widths.
// If the font is monospace or fake monospace we ceil to ensure that 
// every character and the space are the same width.  Otherwise we round.
static bool computeWidthForSpace(FontData *renderer)
{
    renderer->m_spaceGlyph = extendGlyphMap(renderer, SPACE);
    if (renderer->m_spaceGlyph == 0)
        return false;

    float width = renderer->widthForGlyph(renderer->m_spaceGlyph);

    renderer->m_spaceWidth = width;

    renderer->determinePitch();
    renderer->m_adjustedSpaceWidth = renderer->m_treatAsFixedPitch ? ceilf(width) : roundf(width);
    
    return true;
}

static bool setUpFont(FontData *renderer)
{
    ATSUStyle fontStyle;
    if (ATSUCreateStyle(&fontStyle) != noErr)
        return false;

    if (!fillStyleWithAttributes(fontStyle, renderer->m_font.font)) {
        ATSUDisposeStyle(fontStyle);
        return false;
    }

    if (wkGetATSStyleGroup(fontStyle, &renderer->m_styleGroup) != noErr) {
        ATSUDisposeStyle(fontStyle);
        return false;
    }

    ATSUDisposeStyle(fontStyle);

    if (!computeWidthForSpace(renderer)) {
        freeGlyphMap(renderer->m_characterToGlyphMap);
        renderer->m_characterToGlyphMap = 0;
        wkReleaseStyleGroup(renderer->m_styleGroup);
        renderer->m_styleGroup = 0;
        return false;
    }
    
    return true;
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

void FontData::updateGlyphMapEntry(UChar c, ATSGlyphRef glyph, const FontData *substituteRenderer) const
{
    GlyphMap *map;
    for (map = m_characterToGlyphMap; map; map = map->next) {
        UChar32 start = map->startRange;
        if (c >= start && c <= map->endRange) {
            int i = c - start;
            map->glyphs[i].glyph = glyph;
            // This renderer will leak.
            // No problem though; we want it to stick around forever.
            // Max theoretical retain counts applied here will be num_fonts_on_system * num_glyphs_in_font.
            map->glyphs[i].renderer = substituteRenderer;
            break;
        }
    }
}

static ATSGlyphRef extendGlyphMap(const FontData *renderer, UChar32 c)
{
    GlyphMap *map = new GlyphMap;
    ATSLayoutRecord *glyphRecord;
    char glyphVector[WKGlyphVectorSize];
    UChar32 end, start;
    unsigned blockSize;
    
    if (renderer->m_characterToGlyphMap == 0)
        blockSize = INITIAL_BLOCK_SIZE;
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (c / blockSize) * blockSize;
    end = start + (blockSize - 1);

    map->startRange = start;
    map->endRange = end;
    map->next = 0;
    
    unsigned i;
    unsigned count = end - start + 1;
    unsigned short buffer[INCREMENTAL_BLOCK_SIZE * 2 + 2];
    unsigned bufferLength;

    if (start < 0x10000) {
        bufferLength = count;
        for (i = 0; i < count; i++)
            buffer[i] = i + start;

        if (start == 0) {
            // Control characters must not render at all.
            for (i = 0; i < 0x20; ++i)
                buffer[i] = ZERO_WIDTH_SPACE;
            buffer[0x7F] = ZERO_WIDTH_SPACE;

            // But \n, \t, and nonbreaking space must render as a space.
            buffer[(int)'\n'] = ' ';
            buffer[(int)'\t'] = ' ';
            buffer[NO_BREAK_SPACE] = ' ';
        }
    } else {
        bufferLength = count * 2;
        for (i = 0; i < count; i++) {
            int c = i + start;
            buffer[i * 2] = U16_LEAD(c);
            buffer[i * 2 + 1] = U16_TRAIL(c);
        }
    }

    OSStatus status = wkInitializeGlyphVector(count, &glyphVector);
    if (status != noErr) {
        // This should never happen, perhaps indicates a bad font!  If it does the
        // font substitution code will find an alternate font.
        delete map;
        return 0;
    }

    wkConvertCharToGlyphs(renderer->m_styleGroup, &buffer[0], bufferLength, &glyphVector);
    unsigned numGlyphs = wkGetGlyphVectorNumGlyphs(&glyphVector);
    if (numGlyphs != count) {
        // This should never happen, perhaps indicates a bad font?
        // If it does happen, the font substitution code will find an alternate font.
        wkClearGlyphVector(&glyphVector);
        delete map;
        return 0;
    }

    map->glyphs = new GlyphEntry[count];
    glyphRecord = (ATSLayoutRecord *)wkGetGlyphVectorFirstRecord(glyphVector);
    for (i = 0; i < count; i++) {
        map->glyphs[i].glyph = glyphRecord->glyphID;
        map->glyphs[i].renderer = renderer;
        glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + wkGetGlyphVectorRecordSize(glyphVector));
    }
    wkClearGlyphVector(&glyphVector);
    
    if (renderer->m_characterToGlyphMap == 0)
        renderer->m_characterToGlyphMap = map;
    else {
        GlyphMap *lastMap = renderer->m_characterToGlyphMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    ATSGlyphRef glyph = map->glyphs[c - start].glyph;

    // Special case for characters 007F-00A0.
    if (glyph == 0 && c >= 0x7F && c <= 0xA0) {
        glyph = wkGetDefaultGlyphForChar(renderer->m_font.font, c);
        map->glyphs[c - start].glyph = glyph;
    }

    return glyph;
}

static WidthMap *extendWidthMap(const FontData *renderer, ATSGlyphRef glyph)
{
    WidthMap *map = new WidthMap;
    unsigned end;
    ATSGlyphRef start;
    unsigned blockSize;
    unsigned i, count;
    
    NSFont *f = renderer->m_font.font;
    if (renderer->m_glyphToWidthMap == 0) {
        if ([f numberOfGlyphs] < INITIAL_BLOCK_SIZE)
            blockSize = [f numberOfGlyphs];
         else
            blockSize = INITIAL_BLOCK_SIZE;
    } else {
        blockSize = INCREMENTAL_BLOCK_SIZE;
    }
    if (blockSize == 0) {
        start = 0;
    } else {
        start = (glyph / blockSize) * blockSize;
    }
    end = ((unsigned)start) + blockSize; 

    map->startRange = start;
    map->endRange = end;
    count = end - start + 1;

    map->widths = new WebGlyphWidth[count];
    for (i = 0; i < count; i++)
        map->widths[i] = NAN;

    if (renderer->m_glyphToWidthMap == 0)
        renderer->m_glyphToWidthMap = map;
    else {
        WidthMap *lastMap = renderer->m_glyphToWidthMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    return map;
}

static void freeWidthMap(WidthMap *map)
{
    while (map) {
        WidthMap *next = map->next;
        delete []map->widths;
        delete map;
        map = next;
    }
}

static void freeGlyphMap(GlyphMap *map)
{
    while (map) {
        GlyphMap *next = map->next;
        delete []map->glyphs;
        delete map;
        map = next;
    }
}

Glyph FontData::glyphForCharacter(const FontData **renderer, unsigned c) const
{
    // this loop is hot, so it is written to avoid LSU stalls
    GlyphMap *map;
    GlyphMap *nextMap;
    for (map = (*renderer)->m_characterToGlyphMap; map; map = nextMap) {
        UChar start = map->startRange;
        nextMap = map->next;
        if (c >= start && c <= map->endRange) {
            GlyphEntry *ge = &map->glyphs[c - start];
            *renderer = ge->renderer;
            return ge->glyph;
        }
    }

    return extendGlyphMap(*renderer, c);
}

static bool fillStyleWithAttributes(ATSUStyle style, NSFont *theFont)
{
    if (!theFont)
        return false;
    ATSUFontID fontId = wkGetNSFontATSUFontId(theFont);
    if (!fontId)
        return false;
    ATSUAttributeTag tag = kATSUFontTag;
    ByteCount size = sizeof(ATSUFontID);
    ATSUFontID *valueArray[1] = {&fontId};
    OSStatus status = ATSUSetAttributes(style, 1, &tag, &size, (void* const*)valueArray);
    if (status != noErr)
        return false;
    return true;
}

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

}
