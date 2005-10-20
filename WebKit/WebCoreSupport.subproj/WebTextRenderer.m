/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import "WebTextRenderer.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#import <WebKit/WebGraphicsBridge.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKitSystemInterface.h>

#import <float.h>

#import <unicode/uchar.h>
#import <unicode/unorm.h>

// FIXME: FATAL_ALWAYS seems like a bad idea; lets stop using it.

#define SMALLCAPS_FONTSIZE_MULTIPLIER 0.7f

// Should be more than enough for normal usage.
#define NUM_SUBSTITUTE_FONT_MAPS 10

// According to http://www.unicode.org/Public/UNIDATA/UCD.html#Canonical_Combining_Class_Values
#define HIRAGANA_KATAKANA_VOICING_MARKS 8

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

#define UNINITIALIZED_GLYPH_WIDTH 65535

#define INVALID_WIDTH (-FLT_MAX)

#if !defined(ScaleEmToUnits)
#define CONTEXT_DPI	(72.0)

#define ScaleEmToUnits(X, U_PER_EM)	(X * ((1.0 * CONTEXT_DPI) / (CONTEXT_DPI * U_PER_EM)))
#endif

typedef float WebGlyphWidth;
typedef UInt32 UnicodeChar;

struct WidthMap {
    ATSGlyphRef startRange;
    ATSGlyphRef endRange;
    WidthMap *next;
    WebGlyphWidth *widths;
};

typedef struct GlyphEntry {
    ATSGlyphRef glyph;
    WebTextRenderer *substituteRenderer;
} GlyphEntry;

struct GlyphMap {
    UnicodeChar startRange;
    UnicodeChar endRange;
    GlyphMap *next;
    GlyphEntry *glyphs;
};

typedef struct CharacterWidthIterator {
    WebTextRenderer *renderer;
    const WebCoreTextRun *run;
    const WebCoreTextStyle *style;
    unsigned currentCharacter;
    float runWidthSoFar;
    float widthToStart;
    float padding;
    float padPerSpace;
} CharacterWidthIterator;

typedef struct ATSULayoutParameters
{
    WebTextRenderer *renderer;
    const WebCoreTextRun *run;
    const WebCoreTextStyle *style;
    ATSUTextLayout layout;
} ATSULayoutParameters;

@interface WebTextRenderer (WebInternal)

- (WebTextRenderer *)substituteRendererForCharacters:(const unichar *)characters length:(int)numCharacters families:(NSString **)families;
- (WebTextRenderer *)rendererForAlternateFont:(WebCoreFont)alternateNSFont;

- (WidthMap *)extendGlyphToWidthMapToInclude:(ATSGlyphRef)glyphID;
- (ATSGlyphRef)extendCharacterToGlyphMapToInclude:(UnicodeChar)c;
- (void)updateGlyphEntryForCharacter:(UnicodeChar)c glyphID:(ATSGlyphRef)glyphID substituteRenderer:(WebTextRenderer *)substituteRenderer;

// Measuring runs.
static inline float floatWidthForRun(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, float *widthBuffer, WebTextRenderer **rendererBuffer,
    CGGlyph *glyphBuffer, float *startPosition, int *numGlyphs);
static float CG_floatWidthForRun(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, float *widthBuffer, WebTextRenderer **rendererBuffer, CGGlyph *glyphBuffer, float *startPosition, int *numGlyphsResult);
static float ATSU_floatWidthForRun(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style);

// Drawing runs.
- (void)CG_drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;
- (void)ATSU_drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;

// Selection point detection in runs.
- (int)CG_pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs;
- (int)ATSU_pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs;

// Drawing highlight for runs.
- (void)CG_drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;
- (void)ATSU_drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;

- (BOOL)setUpFont;

- (ATSUTextLayout)createATSUTextLayoutForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style;

- (WebTextRenderer *)smallCapsRenderer;

@end

// Character property functions.

static inline BOOL isSpace(UniChar c)
{
    return c == SPACE || c == '\t' || c == '\n' || c == NO_BREAK_SPACE;
}

static const uint8_t isRoundingHackCharacterTable[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*\t*/, 1 /*\n*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*-*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*?*/,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*no-break space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static inline BOOL isRoundingHackCharacter(UniChar c)
{
    return (((c & ~0xFF) == 0 && isRoundingHackCharacterTable[c]));
}

// Map utility functions
static void freeWidthMap(WidthMap *map);
static void freeGlyphMap(GlyphMap *map);

static WebGlyphWidth getUncachedWidth(WebTextRenderer *renderer, WidthMap *map, ATSGlyphRef glyph)
{
    NSFont *font = renderer->font.font;

    float pointSize = [font pointSize];
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    CGSize advance;
    if (!WKGetGlyphTransformedAdvances(font, &m, &glyph, &advance)) {
        ERROR("Unable to cache glyph widths for %@ %f", [font displayName], pointSize);
        return 0;
    }

    return advance.width;
}

static inline WebGlyphWidth widthFromMap(WebTextRenderer *renderer, WidthMap *map, ATSGlyphRef glyph)
{
    while (1) {
        if (map == 0)
            map = [renderer extendGlyphToWidthMapToInclude:glyph];

        if (glyph >= map->startRange && glyph <= map->endRange) {
            WebGlyphWidth width = map->widths[glyph - map->startRange];
            if (width == UNINITIALIZED_GLYPH_WIDTH) {
                width = getUncachedWidth(renderer, map, glyph);
                map->widths[glyph - map->startRange] = width;
            }
            return width;
        }

        map = map->next;
    }
}    

static inline WebGlyphWidth widthForGlyph(WebTextRenderer *renderer, ATSGlyphRef glyph)
{
    return widthFromMap(renderer, renderer->glyphToWidthMap, glyph) + renderer->syntheticBoldOffset;
}

static OSStatus overrideLayoutOperation(ATSULayoutOperationSelector iCurrentOperation, ATSULineRef iLineRef, UInt32 iRefCon, void *iOperationCallbackParameterPtr, ATSULayoutOperationCallbackStatus *oCallbackStatus)
{
    ATSULayoutParameters *params = (ATSULayoutParameters *)iRefCon;
    OSStatus status;
    ItemCount count;
    ATSLayoutRecord *layoutRecords;
    const WebCoreTextStyle *style = params->style;

    if (style->applyWordRounding) {
        status = ATSUDirectGetLayoutDataArrayPtrFromLineRef(iLineRef, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, true, (void **)&layoutRecords, &count);
        if (status != noErr) {
            *oCallbackStatus = kATSULayoutOperationCallbackStatusContinue;
            return status;
        }
        
        // The CoreGraphics interpretation of NSFontAntialiasedIntegerAdvancementsRenderingMode seems
        // to be "round each glyph's width to the nearest integer". This is not the same as ATSUI
        // does in any of its device-metrics modes.
        Boolean roundEachGlyph = [params->renderer->font.font renderingMode] == NSFontAntialiasedIntegerAdvancementsRenderingMode;
        Fixed lastNativePos = 0;
        float lastAdjustedPos = 0;
        const WebCoreTextRun *run = params->run;
        const UniChar *characters = run->characters;
        WebTextRenderer *renderer = params->renderer;
        UniChar ch, nextCh;
        nextCh = *(UniChar *)(((char *)characters)+layoutRecords[0].originalOffset);
        // In the CoreGraphics code path, the rounding hack is applied in logical order.
        // Here it is applied in visual left-to-right order, which may be better.
        ItemCount i;
        for (i = 1; i < count; i++) {
            BOOL isLastChar = i == count - 1;
            ch = nextCh;

            // Use space for nextCh at the end of the loop so that we get inside the rounding hack code.
            // We won't actually round unless the other conditions are satisfied.
            nextCh = isLastChar ? ' ' : *(UniChar *)(((char *)characters)+layoutRecords[i].originalOffset);

            float width = FixedToFloat(layoutRecords[i].realPos - lastNativePos);
            lastNativePos = layoutRecords[i].realPos;
            if (roundEachGlyph)
                width = roundf(width);
            if (renderer->treatAsFixedPitch ? width == renderer->spaceWidth : (layoutRecords[i-1].flags & kATSGlyphInfoIsWhiteSpace))
                width = renderer->adjustedSpaceWidth;
            if (isRoundingHackCharacter(ch))
                width = ceilf(width);
            lastAdjustedPos = lastAdjustedPos + width;
            if (isRoundingHackCharacter(nextCh))
                if (!isLastChar
                        || (style->applyRunRounding && (run->length == 1 || run->to - run->from > 1))
                        || (run->to < (int)run->length && isRoundingHackCharacter(characters[run->to])))
                    lastAdjustedPos = ceilf(lastAdjustedPos);
            layoutRecords[i].realPos = FloatToFixed(lastAdjustedPos);
        }
        
        status = ATSUDirectReleaseLayoutDataArrayPtr(iLineRef, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, (void **)&layoutRecords);
    }
    *oCallbackStatus = kATSULayoutOperationCallbackStatusHandled;
    return noErr;
}

// Iterator functions
static void initializeCharacterWidthIterator(CharacterWidthIterator *iterator, WebTextRenderer *renderer, const WebCoreTextRun *run , const WebCoreTextStyle *style);
static float widthForNextCharacter(CharacterWidthIterator *iterator, ATSGlyphRef *glyphUsed, WebTextRenderer **rendererUsed);

static BOOL fillStyleWithAttributes(ATSUStyle style, NSFont *theFont);
static BOOL shouldUseATSU(const WebCoreTextRun *run);

#if !ERROR_DISABLED
static NSString *pathFromFont(NSFont *font);
#endif

static void createATSULayoutParameters(ATSULayoutParameters *params, WebTextRenderer *renderer, const WebCoreTextRun *run , const WebCoreTextStyle *style);
static void disposeATSULayoutParameters(ATSULayoutParameters *params);

// Globals
static BOOL alwaysUseATSU = NO;

@implementation WebTextRenderer

+ (NSString *)webFallbackFontFamily
{
    static NSString *webFallbackFontFamily = nil;
    if (!webFallbackFontFamily)
	webFallbackFontFamily = [[[NSFont systemFontOfSize:16.0] familyName] retain];
    return webFallbackFontFamily;
}

- initWithFont:(WebCoreFont)f
{
    [super init];
    
    // Quartz can only handle fonts with these glyph packings.  Other packings have
    // been deprecated.
    if ([f.font glyphPacking] != NSNativeShortGlyphPacking && [f.font glyphPacking] != NSTwoByteGlyphPacking) {
        // Apparently there are many deprecated fonts out there with unsupported packing types.
        // Log and use fallback font.
        // This change fixes the many crashes reported in 3782533.
        // Most likely, the problem is encountered when people upgrade from OS 9, or have OS 9 fonts installed on OS X.
        NSLog(@"%s:%d  Unable to use deprecated font %@ %f, using system font instead", __FILE__, __LINE__, [f.font displayName], [f.font pointSize]);
        f.font = [NSFont systemFontOfSize:[f.font pointSize]];
    }

    font = f;

    syntheticBoldOffset = f.syntheticBold ? ceilf([f.font pointSize] / 24.0f) : 0.f;
    
    bool failedSetup = false;
    if (![self setUpFont]) {
        // Ack! Something very bad happened, like a corrupt font.
        // Try looking for an alternate 'base' font for this renderer.

        // Special case hack to use "Times New Roman" in place of "Times".
        // "Times RO" is a common font whose family name is "Times".
        // It overrides the normal "Times" family font.
        // It also appears to have a corrupt regular variant.
        NSString *fallbackFontFamily;
        if ([[font.font familyName] isEqual:@"Times"])
            fallbackFontFamily = @"Times New Roman";
        else
            fallbackFontFamily = [WebTextRenderer webFallbackFontFamily];
        
        // Try setting up the alternate font.
        // This is a last ditch effort to use a substitute font when something has gone wrong.
#if !ERROR_DISABLED
        NSFont *initialFont = font.font;
#endif
        font.font = [[NSFontManager sharedFontManager] convertFont:font.font toFamily:fallbackFontFamily];
#if !ERROR_DISABLED
        NSString *filePath = pathFromFont(initialFont);
        if (!filePath)
            filePath = @"not known";
#endif
        if (![self setUpFont]) {
	    if ([fallbackFontFamily isEqual:@"Times New Roman"]) {
		// OK, couldn't setup Times New Roman as an alternate to Times, fallback
		// on the system font.  If this fails we have no alternative left.
		font.font = [[NSFontManager sharedFontManager] convertFont:font.font toFamily:[WebTextRenderer webFallbackFontFamily]];
		if (![self setUpFont]) {
		    // We tried, Times, Times New Roman, and the system font. No joy. We have to give up.
		    ERROR("%@ unable to initialize with font %@ at %@", self, initialFont, filePath);
                    failedSetup = true;
		}
	    } else {
		// We tried the requested font and the system font. No joy. We have to give up.
		ERROR("%@ unable to initialize with font %@ at %@", self, initialFont, filePath);
                failedSetup = true;
	    }
        }

        // Report the problem.
        ERROR("Corrupt font detected, using %@ in place of %@ located at \"%@\".",
            [font.font familyName], [initialFont familyName], filePath);
    }

    // If all else fails, try to set up using the system font.
    // This is probably because Times and Times New Roman are both unavailable.
    if (failedSetup) {
        font.font = [NSFont systemFontOfSize:[font.font pointSize]];
        ERROR("%@ failed to set up font, using system font %s", self, font.font);
        [self setUpFont];
    }
    
    int iAscent;
    int iDescent;
    int iLineGap;
    unsigned unitsPerEm;
    WKGetFontMetrics(font.font, &iAscent, &iDescent, &iLineGap, &unitsPerEm); 
    float pointSize = [font.font pointSize];
    float asc = ScaleEmToUnits(iAscent, unitsPerEm) * pointSize;
    float dsc = -ScaleEmToUnits(iDescent, unitsPerEm) * pointSize;
    float _lineGap = ScaleEmToUnits(iLineGap, unitsPerEm) * pointSize;

    // We need to adjust Times, Helvetica, and Courier to closely match the
    // vertical metrics of their Microsoft counterparts that are the de facto
    // web standard.  The AppKit adjustment of 20% is too big and is
    // incorrectly added to line spacing, so we use a 15% adjustment instead
    // and add it to the ascent.
    NSString *familyName = [font.font familyName];
    if ([familyName isEqualToString:@"Times"] || [familyName isEqualToString:@"Helvetica"] || [familyName isEqualToString:@"Courier"])
        asc += floorf(((asc + dsc) * 0.15f) + 0.5f);

    ascent = ROUND_TO_INT(asc);
    descent = ROUND_TO_INT(dsc);

    _lineGap = _lineGap > 0.0 ? floorf(_lineGap + 0.5f) : 0.0f;
    lineGap = (int)_lineGap;
    lineSpacing = ascent + descent + lineGap;

    [font.font retain];
    
    return self;
}

- (void)free
{
    if (styleGroup)
        WKReleaseStyleGroup(styleGroup);

    freeWidthMap(glyphToWidthMap);
    freeGlyphMap(characterToGlyphMap);

    if (ATSUStyleInitialized)
        ATSUDisposeStyle(_ATSUStyle);
}

- (void)dealloc
{
    [self free];
    
    [font.font release];
    [smallCapsRenderer release];

    [super dealloc];
}

- (void)finalize
{
    [self free];
    [super finalize];
}

- (int)ascent
{
    // This simple return obviously can't throw an exception.
    return ascent;
}

- (int)descent
{
    // This simple return obviously can't throw an exception.
    return descent;
}

- (int)lineSpacing
{
    // This simple return obviously can't throw an exception.
    return lineSpacing;
}

- (float)xHeight
{
    // Measure the actual character "x", because AppKit synthesizes X height rather than getting it from the font.
    // Unfortunately, NSFont will round this for us so we don't quite get the right value.
    NSGlyph xGlyph = [font.font glyphWithName:@"x"];
    if (xGlyph) {
        NSRect xBox = [font.font boundingRectForGlyph:xGlyph];
        // Use the maximum of either width or height because "x" is nearly square
        // and web pages that foolishly use this metric for width will be laid out
        // poorly if we return an accurate height. Classic case is Times 13 point,
        // which has an "x" that is 7x6 pixels.
        return MAX(NSMaxX(xBox), NSMaxY(xBox));
    }

    return [font.font xHeight];
}

- (void)drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    if (shouldUseATSU(run))
        [self ATSU_drawRun:run style:style geometry:geometry];
    else
        [self CG_drawRun:run style:style geometry:geometry];
}

- (float)floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style widths:(float *)widthBuffer
{
    return floatWidthForRun(self, run, style, widthBuffer, 0, 0, 0, 0);
}

- (void)drawLineForCharacters:(NSPoint)point yOffset:(float)yOffset width: (int)width color:(NSColor *)color thickness:(float)thickness
{
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    CGContextRef cgContext;

    // This will draw the text from the top of the bounding box down.
    // Qt expects to draw from the baseline.
    // Remember that descender is negative.
    point.y -= [self lineSpacing] - [self descent];
    
    BOOL flag = [graphicsContext shouldAntialias];

    // We don't want antialiased lines on screen, but we do when printing (else they are too thick).
    if ([graphicsContext isDrawingToScreen]) {
        [graphicsContext setShouldAntialias:NO];
    }
    
    [color set];

    cgContext = (CGContextRef)[graphicsContext graphicsPort];

    // hack to make thickness 2 underlines for internation text input look right
    if (thickness > 1.5F && thickness < 2.5F) {
        yOffset += .5F;
    }

    if (thickness == 0.0F) {
        if ([graphicsContext isDrawingToScreen]) {
            CGSize size = CGSizeApplyAffineTransform(CGSizeMake(1.0F, 1.0F), CGAffineTransformInvert(CGContextGetCTM(cgContext)));
            CGContextSetLineWidth(cgContext, size.width);
        } else {
            // See bugzilla bug 4255 for details of why we do this when printing
            CGContextSetLineWidth(cgContext, 0.5F);
        }
    } else {
        CGContextSetLineWidth(cgContext, thickness);
    }
    
    // Use CGContextStrokeLineSegments.
    // With Q2DX turned on CGContextStrokeLineSegments sometimes fails to draw lines.  See 3952084.
    // Tiger shipped with Q2DX disabled, tho, so we can use CGContextStrokeLineSegments.
    CGPoint linePoints[2];
    linePoints[0].x = point.x;
    linePoints[0].y = point.y + [self lineSpacing] + 1.5F - [self descent] + yOffset;
    linePoints[1].x = point.x + width - 1.0F;
    linePoints[1].y = linePoints[0].y;
    CGContextStrokeLineSegments(cgContext, linePoints, 2);

    [graphicsContext setShouldAntialias: flag];
}


- (void)drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    if (shouldUseATSU(run))
        [self ATSU_drawHighlightForRun:run style:style geometry:geometry];
    else
        [self CG_drawHighlightForRun:run style:style geometry:geometry];
}

- (int)misspellingLineThickness
{
    return 3;
}

- (int)misspellingLinePatternWidth
{
    return 4;
}

// the number of transparent pixels after the dot
- (int)misspellingLinePatternGapWidth
{
    return 1;
}

- (void)drawLineForMisspelling:(NSPoint)point withWidth:(int)width
{
    // Constants for pattern color
    static NSColor *spellingPatternColor = nil;
    static bool usingDot = false;
    int patternHeight = [self misspellingLineThickness];
    int patternWidth = [self misspellingLinePatternWidth];
 
    // Initialize pattern color if needed
    if (!spellingPatternColor) {
        NSImage *image = [NSImage imageNamed:@"SpellingDot"];
        ASSERT(image); // if image is not available, we want to know
        NSColor *color = (image ? [NSColor colorWithPatternImage:image] : nil);
        if (color)
            usingDot = true;
        else
            color = [NSColor redColor];
        spellingPatternColor = [color retain];
    }

    // Make sure to draw only complete dots.
    // NOTE: Code here used to shift the underline to the left and increase the width
    // to make sure everything gets underlined, but that results in drawing out of
    // bounds (e.g. when at the edge of a view) and could make it appear that the
    // space between adjacent misspelled words was underlined.
    if (usingDot) {
        // allow slightly more considering that the pattern ends with a transparent pixel
        int widthMod = width % patternWidth;
        if (patternWidth - widthMod > [self misspellingLinePatternGapWidth])
            width -= widthMod;
    }
    
    // Draw underline
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    CGContextRef context = (CGContextRef)[currentContext graphicsPort];
    CGContextSaveGState(context);

    [spellingPatternColor set];

    CGPoint transformedOrigin = CGPointApplyAffineTransform(CGPointMake(point.x, point.y), CGContextGetCTM(context));
    CGContextSetPatternPhase(context, CGSizeMake(transformedOrigin.x, transformedOrigin.y));

    NSRectFillUsingOperation(NSMakeRect(point.x, point.y, width, patternHeight), NSCompositeSourceOver);
    
    CGContextRestoreGState(context);
}

- (int)pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs
{
    if (shouldUseATSU(run))
        return [self ATSU_pointToOffset:run style:style position:x reversed:reversed includePartialGlyphs:includePartialGlyphs];
    return [self CG_pointToOffset:run style:style position:x reversed:reversed includePartialGlyphs:includePartialGlyphs];
}

@end


// ------------------- Private API -------------------


@implementation WebTextRenderer (WebInternal)

+ (void)setAlwaysUseATSU:(BOOL)f
{
    alwaysUseATSU = f;
}

- (WebTextRenderer *)smallCapsRenderer
{
    if (!smallCapsRenderer) {
	NS_DURING
            float size = [font.font pointSize] * SMALLCAPS_FONTSIZE_MULTIPLIER;
            WebCoreFont smallCapsFont;
            WebCoreInitializeFont(&smallCapsFont);
            smallCapsFont.font = [[NSFontManager sharedFontManager] convertFont:font.font toSize:size];
	    smallCapsRenderer = [[self rendererForAlternateFont:smallCapsFont] retain];
	NS_HANDLER
            NSLog(@"uncaught exception selecting font for small caps: %@", localException);
	NS_ENDHANDLER
    }
    return smallCapsRenderer;
}

static inline BOOL fontContainsString(NSFont *font, NSString *string)
{
    NSCharacterSet *set = [[font coveredCharacterSet] invertedSet];
    return set && [string rangeOfCharacterFromSet:set].location == NSNotFound;
}

- (NSFont *)substituteFontForString:(NSString *)string families:(NSString **)families
{
    NSFont *substituteFont = nil;

    // First search the CSS family fallback list.
    // Start at 1 (2nd font) because we've already failed on the first lookup.
    NSString *family = nil;
    int i = 1;
    while (families && families[i]) {
        family = families[i++];
        NSFont *f = [[WebTextRendererFactory sharedFactory] cachedFontFromFamily:family
            traits:[[NSFontManager sharedFontManager] traitsOfFont:font.font] size:[font.font pointSize]];
        if (f && fontContainsString(f, string)) {
            substituteFont = f; 
            break;
        }
    }
    
    // Now do string based lookup.
    if (substituteFont == nil)
        substituteFont = WKGetFontInLanguageForRange(font.font, string, NSMakeRange(0, [string length]));

    // Now do character based lookup.
    if (substituteFont == nil && [string length] == 1)
        substituteFont = WKGetFontInLanguageForCharacter(font.font, [string characterAtIndex:0]);

    // Check to make sure this is a distinct font.
    if (substituteFont && [[substituteFont screenFont] isEqual:[font.font screenFont]])
        substituteFont = nil;

    // Now that we have a substitute font, attempt to match it to the best variation.
    // If we have a good match return that, otherwise return the font the AppKit has found.
    if (substituteFont) {
        NSFontManager *manager = [NSFontManager sharedFontManager];
        NSFont *bestVariation = [manager fontWithFamily:[substituteFont familyName] traits:[manager traitsOfFont:font.font]
            weight:[manager weightOfFont:font.font] size:[font.font pointSize]];
        if (bestVariation)
            substituteFont = bestVariation;
    }

    return substituteFont;
}

- (WebTextRenderer *)rendererForAlternateFont:(WebCoreFont)alternateFont
{
    if (!alternateFont.font)
        return nil;

    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSFontTraitMask fontTraits = [fontManager traitsOfFont:font.font];
    if (font.syntheticBold)
        fontTraits |= NSBoldFontMask;
    if (font.syntheticOblique)
        fontTraits |= NSItalicFontMask;
    NSFontTraitMask alternateFontTraits = [fontManager traitsOfFont:alternateFont.font];

    alternateFont.syntheticBold = (fontTraits & NSBoldFontMask) && !(alternateFontTraits & NSBoldFontMask);
    alternateFont.syntheticOblique = (fontTraits & NSItalicFontMask) && !(alternateFontTraits & NSItalicFontMask);
    alternateFont.forPrinter = font.forPrinter;

    return [[WebTextRendererFactory sharedFactory] rendererWithFont:alternateFont];
}

- (WebTextRenderer *)substituteRendererForCharacters:(const unichar *)characters length:(int)numCharacters families:(NSString **)families
{
    WebCoreFont substituteFont;
    WebCoreInitializeFont(&substituteFont);
    NSString *string = [[NSString alloc] initWithCharactersNoCopy:(unichar *)characters length: numCharacters freeWhenDone: NO];
    substituteFont.font = [self substituteFontForString:string families:families];
    [string release];
    return [self rendererForAlternateFont:substituteFont];
}

- (void)convertCharacters:(const UniChar *)characters length:(unsigned)numCharacters toGlyphs:(WKGlyphVectorRef)glyphs
{
    OSStatus status = WKConvertCharToGlyphs(styleGroup, characters, numCharacters, glyphs);
    if (status != noErr)
        FATAL_ALWAYS("unable to get glyphs for %@ %f error = (%d)", self, [font.font displayName], [font.font pointSize], status);
}

// Nasty hack to determine if we should round or ceil space widths.
// If the font is monospace or fake monospace we ceil to ensure that 
// every character and the space are the same width.  Otherwise we round.
- (BOOL)computeWidthForSpace
{
    spaceGlyph = [self extendCharacterToGlyphMapToInclude:SPACE];
    if (spaceGlyph == 0) {
        return NO;
    }

    float width = widthForGlyph(self, spaceGlyph);

    spaceWidth = width;

    treatAsFixedPitch = [[WebTextRendererFactory sharedFactory] isFontFixedPitch:font];
    adjustedSpaceWidth = treatAsFixedPitch ? ceilf(width) : (int)ROUND_TO_INT(width);
    
    return YES;
}

- (BOOL)setUpFont
{
    font.font = font.forPrinter ? [font.font printerFont] : [font.font screenFont];

    ATSUStyle fontStyle;
    if (ATSUCreateStyle(&fontStyle) != noErr)
        return NO;

    if (!fillStyleWithAttributes(fontStyle, font.font)) {
        ATSUDisposeStyle(fontStyle);
        return NO;
    }

    if (WKGetATSStyleGroup(fontStyle, &styleGroup) != noErr) {
        ATSUDisposeStyle(fontStyle);
        return NO;
    }

    ATSUDisposeStyle(fontStyle);

    if (![self computeWidthForSpace]) {
        freeGlyphMap(characterToGlyphMap);
        characterToGlyphMap = 0;
        WKReleaseStyleGroup(styleGroup);
        styleGroup = 0;
        return NO;
    }
    
    return YES;
}

#if !ERROR_DISABLED

static NSString *pathFromFont(NSFont *font)
{
    FSSpec oFile;
    OSStatus status = ATSFontGetFileSpecification(FMGetATSFontRefFromFont((FMFont)WKGetNSFontATSUFontId(font)), &oFile);
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

// Useful page for testing http://home.att.net/~jameskass
static void drawGlyphs(NSFont *font, NSColor *color, CGGlyph *glyphs, CGSize *advances, float x, float y, int numGlyphs,
    float syntheticBoldOffset, BOOL syntheticOblique)
{
    NSGraphicsContext *gContext = [NSGraphicsContext currentContext];
    CGContextRef cgContext = (CGContextRef)[gContext graphicsPort];

    bool originalShouldUseFontSmoothing = WKCGContextGetShouldSmoothFonts(cgContext);
    CGContextSetShouldSmoothFonts(cgContext, [WebView _shouldUseFontSmoothing]);
    
    NSFont *drawFont;
    if ([gContext isDrawingToScreen]) {
        drawFont = [font screenFont];
        if (drawFont != font)
            // We are getting this in too many places (3406411); use ERROR so it only prints on debug versions for now. (We should debug this also, eventually).
            ERROR("Attempting to set non-screen font (%@) when drawing to screen.  Using screen font anyway, may result in incorrect metrics.",
                [[[font fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    } else {
        drawFont = [font printerFont];
        if (drawFont != font)
            NSLog(@"Attempting to set non-printer font (%@) when printing.  Using printer font anyway, may result in incorrect metrics.",
                [[[font fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    }
    
    CGContextSetFont(cgContext, WKGetCGFontFromNSFont(drawFont));

    CGAffineTransform matrix;
    memcpy(&matrix, [drawFont matrix], sizeof(matrix));
    if ([gContext isFlipped]) {
        matrix.b = -matrix.b;
        matrix.d = -matrix.d;
    }
    if (syntheticOblique)
        matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, -tanf(14 * acosf(0) / 90), 1, 0, 0)); 
    CGContextSetTextMatrix(cgContext, matrix);

    WKSetCGFontRenderingMode(cgContext, drawFont);
    CGContextSetFontSize(cgContext, 1.0f);

    [color set];

    CGContextSetTextPosition(cgContext, x, y);
    CGContextShowGlyphsWithAdvances(cgContext, glyphs, advances, numGlyphs);
    if (syntheticBoldOffset) {
        CGContextSetTextPosition(cgContext, x + syntheticBoldOffset, y);
        CGContextShowGlyphsWithAdvances(cgContext, glyphs, advances, numGlyphs);
    }

    CGContextSetShouldSmoothFonts(cgContext, originalShouldUseFontSmoothing);
}

- (void)CG_drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    if (run->length == 0)
        return;

    CharacterWidthIterator widthIterator;
    WebCoreTextRun completeRun = *run;
    completeRun.from = 0;
    completeRun.to = run->length;
    initializeCharacterWidthIterator(&widthIterator, self, &completeRun, style);
    
    float startPosition = 0;

    // The starting point needs to be adjusted to account for the width of
    // the glyphs at the start of the run.
    while (widthIterator.currentCharacter < (unsigned)run->from) {
        startPosition += widthForNextCharacter(&widthIterator, 0, 0);
    }
    float startX = startPosition + geometry->point.x;
    
    float backgroundWidth = 0.0F;
    while (widthIterator.currentCharacter < (unsigned)run->to) {
        backgroundWidth += widthForNextCharacter(&widthIterator, 0, 0);
    }

    if (style->backgroundColor != nil) {
        // Calculate the width of the selection background by adding
        // up the advances of all the glyphs in the selection.
        
        [style->backgroundColor set];

        float yPos = geometry->useFontMetricsForSelectionYAndHeight ? geometry->point.y - [self ascent] - (lineGap/2) : geometry->selectionY;
        float height = geometry->useFontMetricsForSelectionYAndHeight ? [self lineSpacing] : geometry->selectionHeight;
        if (style->rtl) {
            float completeRunWidth = startPosition + backgroundWidth;
            while (widthIterator.currentCharacter < run->length) {
                completeRunWidth += widthForNextCharacter(&widthIterator, 0, 0);
            }

            [NSBezierPath fillRect:NSMakeRect(geometry->point.x + completeRunWidth - startPosition - backgroundWidth, yPos, backgroundWidth, height)];
        } else {
            [NSBezierPath fillRect:NSMakeRect(startX, yPos, backgroundWidth, height)];
        }
    }
}


- (void)CG_drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    float *widthBuffer, localWidthBuffer[LOCAL_BUFFER_SIZE];
    CGGlyph *glyphBuffer, localGlyphBuffer[LOCAL_BUFFER_SIZE];
    WebTextRenderer **rendererBuffer, *localRendererBuffer[LOCAL_BUFFER_SIZE];
    CGSize *advances, localAdvanceBuffer[LOCAL_BUFFER_SIZE];
    int numGlyphs = 0, i;
    float startX;
    unsigned length = run->length;
    
    if (run->length == 0)
        return;

    if (length*MAX_GLYPH_EXPANSION > LOCAL_BUFFER_SIZE) {
        advances = (CGSize *)calloc(length*MAX_GLYPH_EXPANSION, sizeof(CGSize));
        widthBuffer = (float *)calloc(length*MAX_GLYPH_EXPANSION, sizeof(float));
        glyphBuffer = (CGGlyph *)calloc(length*MAX_GLYPH_EXPANSION, sizeof(ATSGlyphRef));
        rendererBuffer = (WebTextRenderer **)calloc(length*MAX_GLYPH_EXPANSION, sizeof(WebTextRenderer *));
    } else {
        advances = localAdvanceBuffer;
        widthBuffer = localWidthBuffer;
        glyphBuffer = localGlyphBuffer;
        rendererBuffer = localRendererBuffer;
    }

    floatWidthForRun(self, run, style, widthBuffer, rendererBuffer, glyphBuffer, &startX, &numGlyphs);
        
    // Eek.  We couldn't generate ANY glyphs for the run.
    if (numGlyphs <= 0)
        return;
        
    // Fill the advances array.
    for (i = 0; i < numGlyphs; i++) {
        advances[i].width = widthBuffer[i];
        advances[i].height = 0;
    }

    // Calculate the starting point of the glyphs to be displayed by adding
    // all the advances up to the first glyph.
    startX += geometry->point.x;

    if (style->backgroundColor != nil)
        [self CG_drawHighlightForRun:run style:style geometry:geometry];
    
    // Swap the order of the glyphs if right-to-left.
    if (style->rtl) {
        int i;
        int mid = numGlyphs / 2;
        int end;
        for (i = 0, end = numGlyphs - 1; i < mid; ++i, --end) {
            CGGlyph gswap1 = glyphBuffer[i];
            CGGlyph gswap2 = glyphBuffer[end];
            glyphBuffer[i] = gswap2;
            glyphBuffer[end] = gswap1;

            CGSize aswap1 = advances[i];
            CGSize aswap2 = advances[end];
            advances[i] = aswap2;
            advances[end] = aswap1;

            WebTextRenderer *rswap1 = rendererBuffer[i];
            WebTextRenderer *rswap2 = rendererBuffer[end];
            rendererBuffer[i] = rswap2;
            rendererBuffer[end] = rswap1;
        }
    }

    // Finally, draw the glyphs.
    int lastFrom = 0;
    int pos = 0;

    // Draw each contiguous run of glyphs that use the same renderer.
    WebTextRenderer *currentRenderer = rendererBuffer[pos];
    float nextX = startX;
    int nextGlyph = pos;
    while (nextGlyph < numGlyphs) {
        WebTextRenderer *nextRenderer = rendererBuffer[nextGlyph];
        if (nextRenderer && nextRenderer != currentRenderer) {
            drawGlyphs(currentRenderer->font.font, style->textColor, &glyphBuffer[lastFrom], &advances[lastFrom],
                startX, geometry->point.y, nextGlyph - lastFrom,
                currentRenderer->syntheticBoldOffset, currentRenderer->font.syntheticOblique);
            lastFrom = nextGlyph;
            currentRenderer = nextRenderer;
            startX = nextX;
        }
        nextX += advances[nextGlyph].width;
        nextGlyph++;
    }
    drawGlyphs(currentRenderer->font.font, style->textColor, &glyphBuffer[lastFrom], &advances[lastFrom],
        startX, geometry->point.y, nextGlyph - lastFrom,
        currentRenderer->syntheticBoldOffset, currentRenderer->font.syntheticOblique);

    if (advances != localAdvanceBuffer) {
        free(advances);
        free(widthBuffer);
        free(glyphBuffer);
        free(rendererBuffer);
    }
}

static inline float floatWidthForRun(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, float *widthBuffer, WebTextRenderer **rendererBuffer,
    CGGlyph *glyphBuffer, float *startPosition, int *numGlyphs)
{
    if (shouldUseATSU(run))
        return ATSU_floatWidthForRun(renderer, run, style);
    
    return CG_floatWidthForRun(renderer, run, style, widthBuffer, rendererBuffer, glyphBuffer, startPosition, numGlyphs);
}

static float CG_floatWidthForRun(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, float *widthBuffer, WebTextRenderer **rendererBuffer, CGGlyph *glyphBuffer, float *startPosition, int *numGlyphsResult)
{
    float _nextWidth;
    CharacterWidthIterator widthIterator;
    WebTextRenderer *rendererUsed = nil;
    ATSGlyphRef glyphUsed;
    int numGlyphs = 0;
    
    initializeCharacterWidthIterator(&widthIterator, renderer, run, style);
    if (startPosition)
        *startPosition = widthIterator.widthToStart;
    while (widthIterator.currentCharacter < (unsigned)widthIterator.run->to) {
        _nextWidth = widthForNextCharacter(&widthIterator, &glyphUsed, &rendererUsed);
        if (_nextWidth == INVALID_WIDTH)
            break;
        if (rendererBuffer)
            rendererBuffer[numGlyphs] = rendererUsed;
        if (glyphBuffer)
            glyphBuffer[numGlyphs] = glyphUsed;
        if (widthBuffer)
            widthBuffer[numGlyphs] = _nextWidth;
        numGlyphs++;
    }
        
    if (numGlyphsResult)
        *numGlyphsResult = numGlyphs;

    return widthIterator.runWidthSoFar;
}

- (void)updateGlyphEntryForCharacter:(UnicodeChar)c glyphID:(ATSGlyphRef)glyphID substituteRenderer:(WebTextRenderer *)substituteRenderer
{
    GlyphMap *map;
    for (map = characterToGlyphMap; map; map = map->next) {
        UnicodeChar start = map->startRange;
        if (c >= start && c <= map->endRange) {
            int i = c - start;
            map->glyphs[i].glyph = glyphID;
            // This renderer will leak.
            // No problem though; we want it to stick around forever.
            // Max theoretical retain counts applied here will be num_fonts_on_system * num_glyphs_in_font.
            map->glyphs[i].substituteRenderer = [substituteRenderer retain];
            break;
        }
    }
}

- (ATSGlyphRef)extendCharacterToGlyphMapToInclude:(UnicodeChar)c
{
    GlyphMap *map = (GlyphMap *)calloc(1, sizeof(GlyphMap));
    ATSLayoutRecord *glyphRecord;
    char glyphVector[WKGlyphVectorSize];
    UnicodeChar end, start;
    unsigned blockSize;
    
    if (characterToGlyphMap == 0)
        blockSize = INITIAL_BLOCK_SIZE;
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (c / blockSize) * blockSize;
    end = start + (blockSize - 1);
        
    LOG(FontCache, "%@ (0x%04x) adding glyphs for 0x%04x to 0x%04x", font, c, start, end);

    map->startRange = start;
    map->endRange = end;
    
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
            buffer['\n'] = ' ';
            buffer['\t'] = ' ';
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

    OSStatus status = WKInitializeGlyphVector(count, &glyphVector);
    if (status != noErr) {
        // This should never happen, perhaps indicates a bad font!  If it does the
        // font substitution code will find an alternate font.
        free(map);
        return 0;
    }

    [self convertCharacters:&buffer[0] length:bufferLength toGlyphs:&glyphVector];
    unsigned numGlyphs = WKGetGlyphVectorNumGlyphs(&glyphVector);
    if (numGlyphs != count) {
        // This should never happen, perhaps indicates a bad font?
        // If it does happen, the font substitution code will find an alternate font.
        free(map);
        return 0;
    }

    map->glyphs = (GlyphEntry *)calloc(count, sizeof(GlyphEntry));
    glyphRecord = (ATSLayoutRecord *)WKGetGlyphVectorFirstRecord(glyphVector);
    for (i = 0; i < count; i++) {
        map->glyphs[i].glyph = glyphRecord->glyphID;
        glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + WKGetGlyphVectorRecordSize(glyphVector));
    }
    WKClearGlyphVector(&glyphVector);
    
    if (characterToGlyphMap == 0)
        characterToGlyphMap = map;
    else {
        GlyphMap *lastMap = characterToGlyphMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    ATSGlyphRef glyphID = map->glyphs[c - start].glyph;

    // Special case for characters 007F-00A0.
    if (glyphID == 0 && c >= 0x7F && c <= 0xA0) {
        glyphID = WKGetDefaultGlyphForChar(font.font, c);
        map->glyphs[c - start].glyph = glyphID;
    }

    return glyphID;
}

- (WidthMap *)extendGlyphToWidthMapToInclude:(ATSGlyphRef)glyphID
{
    WidthMap *map = (WidthMap *)calloc(1, sizeof(WidthMap));
    unsigned end;
    ATSGlyphRef start;
    unsigned blockSize;
    unsigned i, count;
    
    NSFont *f = font.font;
    if (glyphToWidthMap == 0) {
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
        start = (glyphID / blockSize) * blockSize;
    }
    end = ((unsigned)start) + blockSize; 

    LOG(FontCache, "%@ (0x%04x) adding widths for range 0x%04x to 0x%04x", font, glyphID, start, end);

    map->startRange = start;
    map->endRange = end;
    count = end - start + 1;

    map->widths = (WebGlyphWidth *)malloc(count * sizeof(WebGlyphWidth));
    for (i = 0; i < count; i++)
        map->widths[i] = UNINITIALIZED_GLYPH_WIDTH;

    if (glyphToWidthMap == 0)
        glyphToWidthMap = map;
    else {
        WidthMap *lastMap = glyphToWidthMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    return map;
}

- (void)initializeATSUStyle
{
    // The two NSFont calls in this method (pointSize and _atsFontID)
    // are both exception-safe.

    if (!ATSUStyleInitialized) {
        OSStatus status;
        ByteCount propTableSize;
        
        status = ATSUCreateStyle(&_ATSUStyle);
        if (status != noErr)
            FATAL_ALWAYS("ATSUCreateStyle failed (%d)", status);
    
        ATSUFontID fontID = WKGetNSFontATSUFontId(font.font);
        if (fontID == 0) {
            ATSUDisposeStyle(_ATSUStyle);
            ERROR("unable to get ATSUFontID for %@", font.font);
            return;
        }
        
        CGAffineTransform transform = CGAffineTransformMakeScale(1, -1);
        Fixed fontSize = FloatToFixed([font.font pointSize]);
        ATSUAttributeTag styleTags[] = { kATSUSizeTag, kATSUFontTag, kATSUFontMatrixTag};
        ByteCount styleSizes[] = {  sizeof(Fixed), sizeof(ATSUFontID), sizeof(CGAffineTransform) };
        ATSUAttributeValuePtr styleValues[] = { &fontSize, &fontID, &transform  };
        status = ATSUSetAttributes(_ATSUStyle, 3, styleTags, styleSizes, styleValues);
        if (status != noErr)
            FATAL_ALWAYS("ATSUSetAttributes failed (%d)", status);
        status = ATSFontGetTable(fontID, 'prop', 0, 0, 0, &propTableSize);
        if (status == noErr)    // naively assume that if a 'prop' table exists then it contains mirroring info
            ATSUMirrors = YES;
        else if (status == kATSInvalidFontTableAccess)
            ATSUMirrors = NO;
        else
            FATAL_ALWAYS("ATSFontGetTable failed (%d)", status);

        ATSUStyleInitialized = YES;
    }
}

- (ATSUTextLayout)createATSUTextLayoutForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style
{
    // The only Cocoa calls here are to NSGraphicsContext and the self
    // call to _initializeATSUStyle, which are all exception-safe.

    ATSUTextLayout layout;
    UniCharCount runLength;
    ATSUFontID ATSUSubstituteFont;
    UniCharArrayOffset substituteOffset;
    UniCharCount substituteLength;
    OSStatus status;
    ATSULayoutOperationOverrideSpecifier overrideSpecifier;
    
    [self initializeATSUStyle];
    
    // FIXME: This is currently missing the following required features that the CoreGraphics code path has:
    // - \n, \t, and nonbreaking space render as a space.
    // - Other control characters do not render (other code path uses zero-width spaces).
    // - Small caps.
    // - Synthesized bold.
    // - Synthesized oblique.

    runLength = run->to - run->from;
    status = ATSUCreateTextLayoutWithTextPtr(
            run->characters,
            run->from,      // offset
            runLength,      // length
            run->length,    // total length
            1,              // styleRunCount
            &runLength,     // length of style run
            &_ATSUStyle, 
            &layout);
    if (status != noErr)
        FATAL_ALWAYS("ATSUCreateTextLayoutWithTextPtr failed(%d)", status);

    CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    ATSLineLayoutOptions lineLayoutOptions = kATSLineKeepSpacesOutOfMargin | kATSLineHasNoHangers;
    Boolean rtl = style->rtl;
    overrideSpecifier.operationSelector = kATSULayoutOperationPostLayoutAdjustment;
    overrideSpecifier.overrideUPP = overrideLayoutOperation;
    ATSUAttributeTag tags[] = { kATSUCGContextTag, kATSULineLayoutOptionsTag, kATSULineDirectionTag, kATSULayoutOperationOverrideTag };
    ByteCount sizes[] = { sizeof(CGContextRef), sizeof(ATSLineLayoutOptions), sizeof(Boolean), sizeof(ATSULayoutOperationOverrideSpecifier) };
    ATSUAttributeValuePtr values[] = { &cgContext, &lineLayoutOptions, &rtl, &overrideSpecifier };
    
    status = ATSUSetLayoutControls(layout, (style->applyWordRounding ? 4 : 3), tags, sizes, values);
    if (status != noErr)
        FATAL_ALWAYS("ATSUSetLayoutControls failed(%d)", status);

    status = ATSUSetTransientFontMatching(layout, YES);
    if (status != noErr)
        FATAL_ALWAYS("ATSUSetTransientFontMatching failed(%d)", status);

    substituteOffset = run->from;
    while ((status = ATSUMatchFontsToText(layout, substituteOffset, kATSUToTextEnd, &ATSUSubstituteFont, &substituteOffset, &substituteLength)) == kATSUFontsMatched || status == kATSUFontsNotMatched) {
        WebTextRenderer *substituteRenderer = [self substituteRendererForCharacters:run->characters+substituteOffset length:substituteLength families:style->families];
        if (substituteRenderer) {
            [substituteRenderer initializeATSUStyle];
            if (substituteRenderer && substituteRenderer->_ATSUStyle)
                ATSUSetRunStyle(layout, substituteRenderer->_ATSUStyle, substituteOffset, substituteLength);
            // ignoring errors
        }
        substituteOffset += substituteLength;
    }
    // ignoring errors in font substitution
        
    return layout;
}


- (ATSTrapezoid)trapezoidForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style atPoint:(NSPoint )p
{
    // The only Cocoa call here is the self call to
    // _createATSUTextLayoutForRun:, which is exception-safe.

    OSStatus status;
    
    if (run->to - run->from <= 0) {
        ATSTrapezoid nilTrapezoid = { {0,0} , {0,0}, {0,0}, {0,0} };
        return nilTrapezoid;
    }
        
    ATSULayoutParameters params;
    createATSULayoutParameters(&params, self, run, style);

    ATSTrapezoid firstGlyphBounds;
    ItemCount actualNumBounds;
    status = ATSUGetGlyphBounds(params.layout, FloatToFixed(p.x), FloatToFixed(p.y), run->from, run->to - run->from, kATSUseFractionalOrigins, 1, &firstGlyphBounds, &actualNumBounds);    
    if (status != noErr)
        FATAL_ALWAYS("ATSUGetGlyphBounds() failed(%d)", status);
    if (actualNumBounds != 1)
        FATAL_ALWAYS("unexpected result from ATSUGetGlyphBounds(): actualNumBounds(%d) != 1", actualNumBounds);

    disposeATSULayoutParameters(&params);
            
    return firstGlyphBounds;
}


static float ATSU_floatWidthForRun(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style)
{
    ATSTrapezoid oGlyphBounds;
    
    oGlyphBounds = [renderer trapezoidForRun:run style:style atPoint:NSMakePoint(0,0)];
    
    float width = 
        MAX(FixedToFloat(oGlyphBounds.upperRight.x), FixedToFloat(oGlyphBounds.lowerRight.x)) - 
        MIN(FixedToFloat(oGlyphBounds.upperLeft.x), FixedToFloat(oGlyphBounds.lowerLeft.x));
    
    return width;
}

// Be sure to free the run.characters allocated by this function.
static WebCoreTextRun addDirectionalOverride(const WebCoreTextRun *run, BOOL rtl)
{
    WebCoreTextRun swappedRun;
    
    UniChar *swappedCharacters = (UniChar *)malloc(sizeof(UniChar)*(run->length+2));
    memcpy(swappedCharacters+1, run->characters, sizeof(UniChar)*run->length);
    swappedRun.from = (run->from == -1 ? 0 : run->from) + 1;
    swappedRun.to = (run->to == -1 ? (int)run->length - 1 : run->to + 1);
    swappedRun.length = run->length+2;
    swappedCharacters[swappedRun.from - 1] = rtl ? RIGHT_TO_LEFT_OVERRIDE : LEFT_TO_RIGHT_OVERRIDE;
    swappedCharacters[swappedRun.to] = POP_DIRECTIONAL_FORMATTING;
    swappedRun.characters = swappedCharacters;

    return swappedRun;
}

// Be sure to free the run.characters allocated by this function.
static WebCoreTextRun applyMirroringToRun(const WebCoreTextRun *run)
{
    WebCoreTextRun swappedRun;
    unsigned int i;
    
    UniChar *swappedCharacters = (UniChar *)malloc(sizeof(UniChar)*(run->length));
    for (i = 0; i < run->length; i++) {
        // will choke on surrogate pairs?
        swappedCharacters[i] = u_charMirror(run->characters[i]);
    }
    swappedRun.from = run->from;
    swappedRun.to = run->to;
    swappedRun.length = run->length;
    swappedRun.characters = swappedCharacters;

    return swappedRun;
}

- (void)ATSU_drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    // The only Cocoa calls made here are to NSColor and NSBezierPath,
    // plus the self calls to _createATSUTextLayoutForRun: and
    // _trapezoidForRun:. These are all exception-safe.

    int from, to;
    float selectedLeftX;
    const WebCoreTextRun *aRun = run;
    WebCoreTextRun swappedRun;

    if (style->backgroundColor == nil)
        return;
    
    if (style->directionalOverride) {
        swappedRun = addDirectionalOverride(run, style->rtl);
        aRun = &swappedRun;
    } else if (style->rtl && !ATSUMirrors) {
        swappedRun = applyMirroringToRun(run);
        aRun = &swappedRun;
    }

    from = aRun->from;
    to = aRun->to;
    if (from == -1)
        from = 0;
    if (to == -1)
        to = run->length;
   
    int runLength = to - from;
    if (runLength <= 0)
        return;

    WebCoreTextRun completeRun = *aRun;
    completeRun.from = 0;
    completeRun.to = aRun->length;
    ATSULayoutParameters params;
    createATSULayoutParameters(&params, self, &completeRun, style);
    ItemCount count;
    ATSLayoutRecord *layoutRecords;
    OSStatus status = ATSUDirectGetLayoutDataArrayPtrFromTextLayout(params.layout, 0, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, (void **)&layoutRecords, &count);
    if (status != noErr)
        FATAL_ALWAYS("ATSUDirectGetLayoutDataArrayPtrFromTextLayout failed(%d)", status);
    ItemCount i = 0;
    if (!style->rtl)
        while (i < count && layoutRecords[i].originalOffset < sizeof(UniChar) * aRun->from)
            i++;
    else while (i < count && layoutRecords[i].originalOffset >= sizeof(UniChar) * aRun->to)
            i++;
    ItemCount j = i;
    if (!style->rtl)
        while (j < count - 1 && layoutRecords[j].originalOffset < sizeof(UniChar) * aRun->to)
            j++;
    else while (j < count - 1 && layoutRecords[j].originalOffset >= sizeof(UniChar) * aRun->from)
            j++;
    
    float backgroundWidth = FixedToFloat(layoutRecords[j].realPos - layoutRecords[i].realPos);

    selectedLeftX = geometry->point.x + FixedToFloat(layoutRecords[i].realPos);
    
    [style->backgroundColor set];

    float yPos = geometry->useFontMetricsForSelectionYAndHeight ? geometry->point.y - [self ascent] : geometry->selectionY;
    float height = geometry->useFontMetricsForSelectionYAndHeight ? [self lineSpacing] : geometry->selectionHeight;
    [NSBezierPath fillRect:NSMakeRect(selectedLeftX, yPos, backgroundWidth, height)];

    disposeATSULayoutParameters(&params);

    if (style->directionalOverride || (style->rtl && !ATSUMirrors))
        free((void *)swappedRun.characters);
}


- (void)ATSU_drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    // The only Cocoa calls made here are to NSColor and NSGraphicsContext, plus the self
    // calls to _createATSUTextLayoutForRun: and
    // ATSU_drawHighlightForRun:. These are all exception-safe.

    OSStatus status;
    int from, to;
    const WebCoreTextRun *aRun = run;
    WebCoreTextRun swappedRun;
    
    if (style->directionalOverride) {
        swappedRun = addDirectionalOverride(run, style->rtl);
        aRun = &swappedRun;
    } else if (style->rtl && !ATSUMirrors) {
        swappedRun = applyMirroringToRun(run);
        aRun = &swappedRun;
    }

    from = aRun->from;
    to = aRun->to;
    if (from == -1)
        from = 0;
    if (to == -1)
        to = run->length;

    int runLength = to - from;
    if (runLength <= 0)
        return;

    WebCoreTextRun completeRun = *aRun;
    completeRun.from = 0;
    completeRun.to = aRun->length;
    ATSULayoutParameters params;
    createATSULayoutParameters(&params, self, &completeRun, style);

    if (style->backgroundColor != nil)
        [self ATSU_drawHighlightForRun:run style:style geometry:geometry];

    [style->textColor set];

    // ATSUI can't draw beyond -32768 to +32767 so we translate the CTM and tell ATSUI to draw at (0, 0).
    NSGraphicsContext *gContext = [NSGraphicsContext currentContext];
    CGContextRef context = (CGContextRef)[gContext graphicsPort];
    CGContextTranslateCTM(context, geometry->point.x, geometry->point.y);
    BOOL flipped = [gContext isFlipped];
    if (!flipped)
        CGContextScaleCTM(context, 1.0, -1.0);
    status = ATSUDrawText(params.layout, aRun->from, runLength, 0, 0);
    if (!flipped)
        CGContextScaleCTM(context, 1.0, -1.0);
    CGContextTranslateCTM(context, -geometry->point.x, -geometry->point.y);

    if (status != noErr) {
        // Nothing to do but report the error (dev build only).
        ERROR("ATSUDrawText() failed(%d)", status);
    }

    disposeATSULayoutParameters(&params);
    
    if (style->directionalOverride || (style->rtl && !ATSUMirrors))
        free((void *)swappedRun.characters);
}

- (int)ATSU_pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs
{
    // The only Cocoa calls made here is to the self call to
    // _createATSUTextLayoutForRun:. This is exception-safe.

    const WebCoreTextRun *aRun = run;
    WebCoreTextRun swappedRun;
    
    if (style->rtl && !style->directionalOverride) {
        // Work around ATSUPositionToOffset problem with e.g. Lucida Grande 14
        swappedRun = *run;
        swappedRun.characters += run->from;
        swappedRun.from = 0;
        swappedRun.to -= run->from;
        swappedRun.length -= run->from;
        aRun = &swappedRun;
    }
    // Enclose in LRO/RLO - PDF to force ATSU to render visually.
    if (style->directionalOverride) {
        swappedRun = addDirectionalOverride(aRun, style->rtl);
        aRun = &swappedRun;
    } else if (style->rtl && !ATSUMirrors) {
        swappedRun = applyMirroringToRun(aRun);
        aRun = &swappedRun;
    }

    ATSULayoutParameters params;
    createATSULayoutParameters(&params, self, aRun, style);

    UniCharArrayOffset primaryOffset = aRun->from;
    
    // FIXME: No idea how to avoid including partial glyphs.   Not even sure if that's the behavior
    // this yields now.
    Boolean isLeading;
    UniCharArrayOffset secondaryOffset = 0;
    OSStatus status = ATSUPositionToOffset(params.layout, FloatToFixed(x), FloatToFixed(-1), &primaryOffset, &isLeading, &secondaryOffset);
    unsigned offset;
    if (status == noErr) {
        offset = (unsigned)primaryOffset;
    } else {
        // Failed to find offset!  Return 0 offset.
        offset = 0;
    }

    disposeATSULayoutParameters(&params);
    
    if (style->directionalOverride || (style->rtl && !ATSUMirrors))
        free((void *)swappedRun.characters);

    return offset - aRun->from;
}

- (int)CG_pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs
{
    float delta = (float)x;
    unsigned offset = run->from;
    CharacterWidthIterator widthIterator;
    
    initializeCharacterWidthIterator(&widthIterator, self, run, style);

    if (reversed) {
        float width = [self floatWidthForRun:run style:style widths:nil];
        delta -= width;
        while (offset < run->length) {
            float w = widthForNextCharacter(&widthIterator, 0, 0);
            if (w == INVALID_WIDTH) {
                // Something very bad happened, like we only have half of a surrogate pair.
                break;
            }
            else {
                if (w) {
                    if (includePartialGlyphs)
                       w -= w/2;
                    delta += w;
                    if (delta >= 0)
                        break;
                    if (includePartialGlyphs)
                        delta += w;
                }
                offset = widthIterator.currentCharacter;
            }
        }
    } else {
        while (offset < run->length) {
            float w = widthForNextCharacter(&widthIterator, 0, 0);
            if (w == INVALID_WIDTH) {
                // Something very bad happened, like we only have half of a surrogate pair.
                break;
            }
            else {
                if (w) {
                    if (includePartialGlyphs)
                        w -= w/2;
                    delta -= w;
                    if (delta <= 0) 
                        break;
                    if (includePartialGlyphs)
                        delta -= w;
                }
                offset = widthIterator.currentCharacter;
            }
        }
    }
    
    return offset - run->from;
}

@end

// ------------------- Private functions -------------------

static void freeWidthMap(WidthMap *map)
{
    while (map) {
        WidthMap *next = map->next;
        free(map->widths);
        free(map);
        map = next;
    }
}

static void freeGlyphMap(GlyphMap *map)
{
    while (map) {
        GlyphMap *next = map->next;
        free(map->glyphs);
        free(map);
        map = next;
    }
}

static inline ATSGlyphRef glyphForCharacter(GlyphMap *map, UnicodeChar c, WebTextRenderer **renderer)
{
    if (map == 0)
        return nonGlyphID;
    
    // this loop is hot, so it is written to avoid LSU stalls
    while (map) {
        UnicodeChar start = map->startRange;
        GlyphMap *nextMap = map->next;
        if (c >= start && c <= map->endRange) {
            GlyphEntry *ge = &map->glyphs[c - start];
            WebTextRenderer *substituteRenderer = ge->substituteRenderer;
            if (substituteRenderer)
                *renderer = substituteRenderer;
            return ge->glyph;
        }
        map = nextMap;
    }

    return nonGlyphID;
}

static void initializeCharacterWidthIterator(CharacterWidthIterator *iterator, WebTextRenderer *renderer, const WebCoreTextRun *run , const WebCoreTextStyle *style) 
{
    iterator->renderer = renderer;
    iterator->run = run;
    iterator->style = style;
    iterator->currentCharacter = run->from;
    iterator->runWidthSoFar = 0;

    // If the padding is non-zero, count the number of spaces in the run
    // and divide that by the padding for per space addition.
    iterator->padding = style->padding;
    if (iterator->padding > 0){
        uint numSpaces = 0;
        int from = run->from;
        int len = run->to - from;
        int k;
        for (k = from; k < from + len; k++) {
            if (isSpace(run->characters[k])) {
                numSpaces++;
            }
        }
        iterator->padPerSpace = ceilf((((float)style->padding) / ((float)numSpaces)));
    }
    else {
        iterator->padPerSpace = 0;
    }
    
    // Calculate width up to starting position of the run.  This is
    // necessary to ensure that our rounding hacks are always consistently
    // applied.
    if (run->from != 0){
        WebCoreTextRun startPositionRun = *run;
        startPositionRun.from = 0;
        startPositionRun.to = run->from;
        CharacterWidthIterator startPositionIterator;
        initializeCharacterWidthIterator(&startPositionIterator, renderer, &startPositionRun, style);
        
        while (startPositionIterator.currentCharacter < (unsigned)startPositionRun.to)
            widthForNextCharacter(&startPositionIterator, 0, 0);
        iterator->widthToStart = startPositionIterator.runWidthSoFar;
    }
    else
        iterator->widthToStart = 0;
}

static void createATSULayoutParameters(ATSULayoutParameters *params, WebTextRenderer *renderer, const WebCoreTextRun *run , const WebCoreTextStyle *style) 
{
    params->renderer = renderer;
    params->run = run;
    params->style = style;
    params->layout = [renderer createATSUTextLayoutForRun:run style:style];
    ATSUSetTextLayoutRefCon(params->layout, (UInt32)params);
}

static void disposeATSULayoutParameters(ATSULayoutParameters *params)
{
    ATSUDisposeTextLayout(params->layout);
}

static inline float ceilCurrentWidth(CharacterWidthIterator *iterator)
{
    float totalWidth = iterator->widthToStart + iterator->runWidthSoFar;
    float delta = ceilf(totalWidth) - totalWidth;
    iterator->runWidthSoFar += delta;
    return delta;
}

// Return INVALID_WIDTH if an error is encountered or we're at the end of the range in the run.
static float widthForNextCharacter(CharacterWidthIterator *iterator, ATSGlyphRef *glyphUsed, WebTextRenderer **rendererUsed)
{
    WebTextRenderer *renderer = iterator->renderer;
    const WebCoreTextRun *run = iterator->run;
    const WebCoreTextStyle *style = iterator->style;
    unsigned currentCharacter = iterator->currentCharacter;

    ATSGlyphRef glyph = 0;
    
    // Check for end of run.
    if (currentCharacter >= (unsigned)run->to)
        return INVALID_WIDTH;

    const UniChar *cp = &run->characters[currentCharacter];
    
    UnicodeChar c = *cp;

    if (U16_IS_TRAIL(c))
        return INVALID_WIDTH;

    // Do we have a surrogate pair?  If so, determine the full Unicode (32 bit)
    // code point before glyph lookup.
    unsigned clusterLength = 1;
    if (U16_IS_LEAD(c)) {
        // Make sure we have another character and it's a low surrogate.
        if (currentCharacter + 1 >= run->length)
            return INVALID_WIDTH;
        UniChar low = cp[1];
        if (!U16_IS_TRAIL(low))
            return INVALID_WIDTH;
        c = U16_GET_SUPPLEMENTARY(c, low);
        clusterLength = 2;
    }

    // Deal with Hiragana and Katakana voiced and semi-voiced syllables.
    // Normalize into composed form, and then look for glyph with base + combined mark.
    if (c >= 0x3041 && c <= 0x30FE) { // Early out to minimize performance impact. Do we have a Hiragana/Katakana character?
        if (currentCharacter < (unsigned)run->to) {
            UnicodeChar nextCharacter = run->characters[currentCharacter+1];
            if (u_getCombiningClass(nextCharacter) == HIRAGANA_KATAKANA_VOICING_MARKS) {
                // Normalize into composed form using 3.2 rules.
                UChar normalizedCharacters[2] = { 0, 0 };
                UErrorCode uStatus = 0;                
                int32_t resultLength = unorm_normalize(&run->characters[currentCharacter], 2,
                    UNORM_NFC, UNORM_UNICODE_3_2, &normalizedCharacters[0], 2, &uStatus);
                if (resultLength == 1 && uStatus == 0) {
                    c = normalizedCharacters[0];
                    clusterLength = 2;
                }
            }
        }
    }

    if (style->rtl)
        c = u_charMirror(c);
    
    // If small-caps, convert lowercase to upper.
    if (style->smallCaps && !u_isUUppercase(c)) {
        UnicodeChar newC = u_toupper(c);
        if (newC != c) {
            c = newC;
            renderer = [renderer smallCapsRenderer];
        }
    }
    
    glyph = glyphForCharacter(renderer->characterToGlyphMap, c, &renderer);
    if (glyph == nonGlyphID)
        glyph = [renderer extendCharacterToGlyphMapToInclude:c];

    // Now that we have glyph and font, get its width.
    WebGlyphWidth width;
    if (style->tabWidth != 0.0F && c == '\t') {
        width = style->tabWidth - fmodf(style->xpos+iterator->runWidthSoFar, style->tabWidth);
    } else {
        width = widthForGlyph(renderer, glyph);
        // We special case spaces in two ways when applying word rounding.
        // First, we round spaces to an adjusted width in all fonts.
        // Second, in fixed-pitch fonts we ensure that all characters that
        // match the width of the space character have the same width as the space character.
        if (style->applyWordRounding && (renderer->treatAsFixedPitch ? width == renderer->spaceWidth : glyph == renderer->spaceGlyph))
            width = renderer->adjustedSpaceWidth;
    }
    
    // Try to find a substitute font if this font didn't have a glyph for a character in the
    // string.  If one isn't found we end up drawing and measuring the 0 glyph, usually a box.
    if (glyph == 0 && style->attemptFontSubstitution) {
        UniChar characterArray[2];
        unsigned characterArrayLength;
        
        if (c <= 0xFFFF) {
            characterArray[0] = c;
            characterArrayLength = 1;
        } else {
            characterArray[0] = U16_LEAD(c);
            characterArray[1] = U16_TRAIL(c);
            characterArrayLength = 2;
        }
        
        WebTextRenderer *substituteRenderer = [renderer substituteRendererForCharacters:characterArray
            length:characterArrayLength families:style->families];
        if (substituteRenderer) {
            WebCoreTextRun clusterRun;
            WebCoreInitializeTextRun(&clusterRun, characterArray, characterArrayLength, 0, characterArrayLength);
            WebCoreTextStyle clusterStyle = *iterator->style;
            clusterStyle.padding = 0;
            clusterStyle.applyRunRounding = false;
            clusterStyle.attemptFontSubstitution = false;
            
            int cNumGlyphs;
            ATSGlyphRef localGlyphBuffer[MAX_GLYPH_EXPANSION];            
            width = floatWidthForRun(substituteRenderer, &clusterRun, &clusterStyle, 0, 0, localGlyphBuffer, 0, &cNumGlyphs);
            if (cNumGlyphs == 1) {
                glyph = localGlyphBuffer[0];
                [renderer updateGlyphEntryForCharacter:c glyphID:glyph substituteRenderer:substituteRenderer];
                renderer = substituteRenderer;
            }
        }
    }

    // Force characters that are used to determine word boundaries for the rounding hack
    // to be integer width, so following words will start on an integer boundary.
    if (style->applyWordRounding && isRoundingHackCharacter(c))
        width = ceilf(width);
    
    // Account for letter-spacing
    if (style->letterSpacing && width > 0)
        width += style->letterSpacing;

    // Account for padding. WebCore uses space padding to justify text.
    // We distribute the specified padding over the available spaces in the run.
    if (isSpace(c)) {
        if (iterator->padding > 0) {
            // Only use left over padding if note evenly divisible by 
            // number of spaces.
            if (iterator->padding < iterator->padPerSpace){
                width += iterator->padding;
                iterator->padding = 0;
            } else {
                width += iterator->padPerSpace;
                iterator->padding -= iterator->padPerSpace;
            }
        }
        
        // Account for word-spacing.  We apply additional space between "words" by
        // adding width to the space character.
        if (currentCharacter > 0 && !isSpace(cp[-1]))
            width += style->wordSpacing;
    }

    iterator->runWidthSoFar += width;

    // Advance past the character we just dealt with.
    currentCharacter += clusterLength;
    iterator->currentCharacter = currentCharacter;

    // Account for float/integer impedance mismatch between CG and khtml.  "Words" (characters 
    // followed by a character defined by isSpace()) are always an integer width.  We adjust the 
    // width of the last character of a "word" to ensure an integer width.  When we move khtml to
    // floats we can remove this (and related) hacks.
    //
    // Check to see if the next character is a "RoundingHackCharacter", if so, adjust.
    if (style->applyWordRounding && currentCharacter < run->length && isRoundingHackCharacter(cp[clusterLength])) {
        float delta = ceilCurrentWidth(iterator);
        if (delta)
            width += delta;
    } else {
        if (style->applyRunRounding && currentCharacter >= (unsigned)run->to && (run->length == 1 || run->to - run->from > 1)) {
            float delta = ceilCurrentWidth(iterator);
            if (delta)
                width += delta;
        }
    }

    if (rendererUsed)
        *rendererUsed = renderer;
    
    if (glyphUsed)
        *glyphUsed = glyph;

    return width;
}

static BOOL fillStyleWithAttributes(ATSUStyle style, NSFont *theFont)
{
    if (!theFont)
        return NO;
    ATSUFontID fontId = WKGetNSFontATSUFontId(theFont);
    LOG(FontCache, "fillStyleWithAttributes: font = %p,%@, _atsFontID = %x\n", theFont, theFont, (unsigned)fontId);
    if (!fontId)
        return NO;
    ATSUAttributeTag tag = kATSUFontTag;
    ByteCount size = sizeof(ATSUFontID);
    ATSUFontID *valueArray[1] = {&fontId};
    OSStatus status = ATSUSetAttributes(style, 1, &tag, &size, (void *)valueArray);
    if (status != noErr) {
        LOG(FontCache, "fillStyleWithAttributes failed(%d): font = %p,%@, _atsFontID = %x\n", (int)status, theFont, theFont, (unsigned)fontId);
        return NO;
    }
    return YES;
}

static BOOL shouldUseATSU(const WebCoreTextRun *run)
{
    if (alwaysUseATSU)
        return YES;
        
    const UniChar *characters = run->characters;
    int to = run->to;
    int i;
    for (i = run->from; i < to; i++) {
        UniChar c = characters[i];
        if (c < 0x300)      // U+0300 through U+036F Combining diacritical marks
            continue;
        if (c <= 0x36F)
            return YES;

        if (c < 0x0591)     // U+0591 through U+1059 Arabic, Hebrew, Syriac, Thaana, Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
            continue;
        if (c <= 0x1059)
            return YES;

        if (c < 0x1100)     // U+1100 through U+11FF Hangul Jamo (only Ancient Korean should be left here if you precompose; Modern Korean will be precomposed as a result of step A)
            continue;
        if (c <= 0x11FF)
            return YES;

        if (c < 0x1780)     // U+1780 through U+18AF Khmer, Mongolian
            continue;
        if (c <= 0x18AF)
            return YES;

        if (c < 0x1900)     // U+1900 through U+194F Limbu (Unicode 4.0)
            continue;
        if (c <= 0x194F)
            return YES;

        if (c < 0x20D0)     // U+20D0 through U+20FF Combining marks for symbols
            continue;
        if (c <= 0x20FF)
            return YES;

        if (c < 0xFE20)     // U+FE20 through U+FE2F Combining half marks
            continue;
        if (c <= 0xFE2F)
            return YES;
    }

    return NO;
}
