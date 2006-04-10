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
#define SYNTHETIC_OBLIQUE_ANGLE 14

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

#define CONTEXT_DPI (72.0)
#define SCALE_EM_TO_UNITS(X, U_PER_EM) (X * ((1.0 * CONTEXT_DPI) / (CONTEXT_DPI * U_PER_EM)))

typedef float WebGlyphWidth;

struct WidthMap {
    ATSGlyphRef startRange;
    ATSGlyphRef endRange;
    WidthMap *next;
    WebGlyphWidth *widths;
};

typedef struct GlyphEntry {
    ATSGlyphRef glyph;
    WebTextRenderer *renderer;
} GlyphEntry;

struct GlyphMap {
    UChar32 startRange;
    UChar32 endRange;
    GlyphMap *next;
    GlyphEntry *glyphs;
};

typedef struct WidthIterator {
    WebTextRenderer *renderer;
    const WebCoreTextRun *run;
    const WebCoreTextStyle *style;
    unsigned currentCharacter;
    float runWidthSoFar;
    float widthToStart;
    float padding;
    float padPerSpace;
    float finalRoundingWidth;
} WidthIterator;

typedef struct ATSULayoutParameters
{
    const WebCoreTextRun *run;
    const WebCoreTextStyle *style;
    ATSUTextLayout layout;
    WebTextRenderer **renderers;
    UniChar *charBuffer;
    bool hasSyntheticBold;
    bool syntheticBoldPass;
    float padPerSpace;
} ATSULayoutParameters;

static WebTextRenderer *rendererForAlternateFont(WebTextRenderer *, WebCoreFont);

static WidthMap *extendWidthMap(WebTextRenderer *, ATSGlyphRef);
static ATSGlyphRef extendGlyphMap(WebTextRenderer *, UChar32);
static void updateGlyphMapEntry(WebTextRenderer *, UChar32, ATSGlyphRef, WebTextRenderer *substituteRenderer);

static void freeWidthMap(WidthMap *);
static void freeGlyphMap(GlyphMap *);
static inline ATSGlyphRef glyphForCharacter(WebTextRenderer **, UChar32);

// Measuring runs.
static float CG_floatWidthForRun(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *,
    float *widthBuffer, WebTextRenderer **rendererBuffer, CGGlyph *glyphBuffer, float *startPosition, int *numGlyphsResult);
static float ATSU_floatWidthForRun(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *);

// Drawing runs.
static void CG_draw(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *, const WebCoreTextGeometry *);
static void ATSU_draw(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *, const WebCoreTextGeometry *);

// Selection point detection in runs.
static int CG_pointToOffset(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *,
    int x, bool includePartialGlyphs);
static int ATSU_pointToOffset(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *,
    int x, bool includePartialGlyphs);

// Selection rect.
static NSRect CG_selectionRect(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *, const WebCoreTextGeometry *);
static NSRect ATSU_selectionRect(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *, const WebCoreTextGeometry *);

// Drawing highlight.
static void CG_drawHighlight(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *, const WebCoreTextGeometry *);
static void ATSU_drawHighlight(WebTextRenderer *, const WebCoreTextRun *, const WebCoreTextStyle *, const WebCoreTextGeometry *);

static bool setUpFont(WebTextRenderer *);

// Iterator functions
static void initializeWidthIterator(WidthIterator *iterator, WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style);
static unsigned advanceWidthIterator(WidthIterator *iterator, unsigned offset, float *widths, WebTextRenderer **renderersUsed, ATSGlyphRef *glyphsUsed);

static bool fillStyleWithAttributes(ATSUStyle style, NSFont *theFont);
static bool shouldUseATSU(const WebCoreTextRun *run);

#if !ERROR_DISABLED
static NSString *pathFromFont(NSFont *font);
#endif

static void createATSULayoutParameters(ATSULayoutParameters *params, WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style);
static void disposeATSULayoutParameters(ATSULayoutParameters *params);

// Globals
static bool alwaysUseATSU = NO;

// Character property functions.

static inline bool isSpace(UChar32 c)
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

static inline bool isRoundingHackCharacter(UChar32 c)
{
    return (((c & ~0xFF) == 0 && isRoundingHackCharacterTable[c]));
}

// Map utility functions

static inline WebGlyphWidth widthForGlyph(WebTextRenderer *renderer, ATSGlyphRef glyph)
{
    WidthMap *map;
    for (map = renderer->glyphToWidthMap; 1; map = map->next) {
        if (!map)
            map = extendWidthMap(renderer, glyph);
        if (glyph >= map->startRange && glyph <= map->endRange)
            break;
    }
    WebGlyphWidth width = map->widths[glyph - map->startRange];
    if (width >= 0)
        return width;
    NSFont *font = renderer->font.font;
    float pointSize = [font pointSize];
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    CGSize advance;
    if (!WKGetGlyphTransformedAdvances(font, &m, &glyph, &advance)) {
        ERROR("Unable to cache glyph widths for %@ %f", [font displayName], pointSize);
        advance.width = 0;
    }
    width = advance.width + renderer->syntheticBoldOffset;
    map->widths[glyph - map->startRange] = width;
    return width;
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
        
        Fixed lastNativePos = 0;
        float lastAdjustedPos = 0;
        const WebCoreTextRun *run = params->run;
        const UniChar *characters = run->characters + run->from;
        WebTextRenderer **renderers = params->renderers + run->from;
        WebTextRenderer *renderer;
        WebTextRenderer *lastRenderer = 0;
        UniChar ch, nextCh;
        ByteCount offset = layoutRecords[0].originalOffset;
        nextCh = *(UniChar *)(((char *)characters)+offset);
        bool shouldRound = false;
        bool syntheticBoldPass = params->syntheticBoldPass;
        Fixed syntheticBoldOffset = 0;
        ATSGlyphRef spaceGlyph = 0;
        bool hasExtraSpacing = style->letterSpacing | style->wordSpacing | style->padding;
        float padding = style->padding;
        // In the CoreGraphics code path, the rounding hack is applied in logical order.
        // Here it is applied in visual left-to-right order, which may be better.
        ItemCount lastRoundingChar = 0;
        ItemCount i;
        for (i = 1; i < count; i++) {
            bool isLastChar = i == count - 1;
            renderer = renderers[offset / 2];
            if (renderer != lastRenderer) {
                lastRenderer = renderer;
                // The CoreGraphics interpretation of NSFontAntialiasedIntegerAdvancementsRenderingMode seems
                // to be "round each glyph's width to the nearest integer". This is not the same as ATSUI
                // does in any of its device-metrics modes.
                shouldRound = [renderer->font.font renderingMode] == NSFontAntialiasedIntegerAdvancementsRenderingMode;
                if (syntheticBoldPass) {
                    syntheticBoldOffset = FloatToFixed(renderer->syntheticBoldOffset);
                    spaceGlyph = renderer->spaceGlyph;
                }
            }
            float width = FixedToFloat(layoutRecords[i].realPos - lastNativePos);
            lastNativePos = layoutRecords[i].realPos;
            if (shouldRound)
                width = roundf(width);
            width += renderer->syntheticBoldOffset;
            if (renderer->treatAsFixedPitch ? width == renderer->spaceWidth : (layoutRecords[i-1].flags & kATSGlyphInfoIsWhiteSpace))
                width = renderer->adjustedSpaceWidth;

            if (hasExtraSpacing) {
                if (width && style->letterSpacing)
                    width +=style->letterSpacing;
                if (isSpace(nextCh)) {
                    if (style->padding) {
                        if (padding < params->padPerSpace) {
                            width += padding;
                            padding = 0;
                        } else {
                            width += params->padPerSpace;
                            padding -= params->padPerSpace;
                        }
                    }
                    if (offset != 0 && !isSpace(*((UniChar *)(((char *)characters)+offset) - 1)) && style->wordSpacing)
                        width += style->wordSpacing;
                }
            }

            ch = nextCh;
            offset = layoutRecords[i].originalOffset;
            // Use space for nextCh at the end of the loop so that we get inside the rounding hack code.
            // We won't actually round unless the other conditions are satisfied.
            nextCh = isLastChar ? ' ' : *(UniChar *)(((char *)characters)+offset);

            if (isRoundingHackCharacter(ch))
                width = ceilf(width);
            lastAdjustedPos = lastAdjustedPos + width;
            if (isRoundingHackCharacter(nextCh)
                && (!isLastChar
                    || style->applyRunRounding
                    || (run->to < (int)run->length && isRoundingHackCharacter(characters[run->to - run->from])))) {
                if (!style->rtl)
                    lastAdjustedPos = ceilf(lastAdjustedPos);
                else {
                    float roundingWidth = ceilf(lastAdjustedPos) - lastAdjustedPos;
                    Fixed rw = FloatToFixed(roundingWidth);
                    ItemCount j;
                    for (j = lastRoundingChar; j < i; j++)
                        layoutRecords[j].realPos += rw;
                    lastRoundingChar = i;
                    lastAdjustedPos += roundingWidth;
                }
            }
            if (syntheticBoldPass) {
                if (syntheticBoldOffset)
                    layoutRecords[i-1].realPos += syntheticBoldOffset;
                else
                    layoutRecords[i-1].glyphID = spaceGlyph;
            }
            layoutRecords[i].realPos = FloatToFixed(lastAdjustedPos);
        }
        
        status = ATSUDirectReleaseLayoutDataArrayPtr(iLineRef, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, (void **)&layoutRecords);
    }
    *oCallbackStatus = kATSULayoutOperationCallbackStatusHandled;
    return noErr;
}

@implementation WebTextRenderer

static NSString *webFallbackFontFamily(void)
{
    static NSString *webFallbackFontFamily = nil;
    if (!webFallbackFontFamily)
	webFallbackFontFamily = [[[NSFont systemFontOfSize:16.0] familyName] retain];
    return webFallbackFontFamily;
}

- initWithFont:(WebCoreFont)f
{
    [super init];
    
    font = f;

    syntheticBoldOffset = f.syntheticBold ? ceilf([f.font pointSize] / 24.0f) : 0.f;
    
    bool failedSetup = false;
    if (!setUpFont(self)) {
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
            fallbackFontFamily = webFallbackFontFamily();
        
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
        if (!setUpFont(self)) {
	    if ([fallbackFontFamily isEqual:@"Times New Roman"]) {
		// OK, couldn't setup Times New Roman as an alternate to Times, fallback
		// on the system font.  If this fails we have no alternative left.
		font.font = [[NSFontManager sharedFontManager] convertFont:font.font toFamily:webFallbackFontFamily()];
		if (!setUpFont(self)) {
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
        setUpFont(self);
    }
    
    int iAscent;
    int iDescent;
    int iLineGap;
    unsigned unitsPerEm;
    WKGetFontMetrics(font.font, &iAscent, &iDescent, &iLineGap, &unitsPerEm); 
    float pointSize = [font.font pointSize];
    float fAscent = SCALE_EM_TO_UNITS(iAscent, unitsPerEm) * pointSize;
    float fDescent = -SCALE_EM_TO_UNITS(iDescent, unitsPerEm) * pointSize;
    float fLineGap = SCALE_EM_TO_UNITS(iLineGap, unitsPerEm) * pointSize;

    // We need to adjust Times, Helvetica, and Courier to closely match the
    // vertical metrics of their Microsoft counterparts that are the de facto
    // web standard. The AppKit adjustment of 20% is too big and is
    // incorrectly added to line spacing, so we use a 15% adjustment instead
    // and add it to the ascent.
    NSString *familyName = [font.font familyName];
    if ([familyName isEqualToString:@"Times"] || [familyName isEqualToString:@"Helvetica"] || [familyName isEqualToString:@"Courier"])
        fAscent += floorf(((fAscent + fDescent) * 0.15f) + 0.5f);

    ascent = lroundf(fAscent);
    descent = lroundf(fDescent);
    lineGap = lroundf(fLineGap);

    lineSpacing = ascent + descent + lineGap;

    [font.font retain];

    return self;
}

static void destroy(WebTextRenderer *renderer)
{
    if (renderer->styleGroup)
        WKReleaseStyleGroup(renderer->styleGroup);

    freeWidthMap(renderer->glyphToWidthMap);
    freeGlyphMap(renderer->characterToGlyphMap);

    if (renderer->ATSUStyleInitialized)
        ATSUDisposeStyle(renderer->_ATSUStyle);
}

- (void)dealloc
{
    destroy(self);
    
    [font.font release];
    [smallCapsRenderer release];

    [super dealloc];
}

- (void)finalize
{
    destroy(self);

    [super finalize];
}

- (int)ascent
{
    return ascent;
}

- (int)descent
{
    return descent;
}

- (int)lineSpacing
{
    return lineSpacing;
}

- (float)xHeight
{
    // Measure the actual character "x", because AppKit synthesizes X height rather than getting it from the font.
    // Unfortunately, NSFont will round this for us so we don't quite get the right value.
    WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:font];
    NSGlyph xGlyph = glyphForCharacter(&renderer, 'x');
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
        ATSU_draw(self, run, style, geometry);
    else
        CG_draw(self, run, style, geometry);
}

- (float)floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style
{
    if (shouldUseATSU(run))
        return ATSU_floatWidthForRun(self, run, style);
    return CG_floatWidthForRun(self, run, style, 0, 0, 0, 0, 0);
}

static void drawHorizontalLine(float x, float y, float width, NSColor *color, float thickness, bool shouldAntialias)
{
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    CGContextRef cgContext = (CGContextRef)[graphicsContext graphicsPort];

    CGContextSaveGState(cgContext);

    [color set];
    CGContextSetLineWidth(cgContext, thickness);
    CGContextSetShouldAntialias(cgContext, shouldAntialias);

    float halfThickness = thickness / 2;

    CGPoint linePoints[2];
    linePoints[0].x = x + halfThickness;
    linePoints[0].y = y + halfThickness;
    linePoints[1].x = x + width - halfThickness;
    linePoints[1].y = y + halfThickness;
    CGContextStrokeLineSegments(cgContext, linePoints, 2);

    CGContextRestoreGState(cgContext);
}

- (void)drawLineForCharacters:(NSPoint)point yOffset:(float)yOffset width:(int)width color:(NSColor *)color thickness:(float)thickness
{
    // Note: This function assumes that point.x and point.y are integers (and that's currently always the case).

    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    CGContextRef cgContext = (CGContextRef)[graphicsContext graphicsPort];

    bool printing = ![graphicsContext isDrawingToScreen];

    float x = point.x;
    float y = point.y + yOffset;

    // Leave 1.0 in user space between the baseline of the text and the top of the underline.
    // FIXME: Is this the right distance for space above the underline? Even for thick underlines on large sized text?
    y += 1;

    if (printing) {
        // When printing, use a minimum thickness of 0.5 in user space.
        // See bugzilla bug 4255 for details of why 0.5 is the right minimum thickness to use while printing.
        if (thickness < 0.5)
            thickness = 0.5;

        // When printing, use antialiasing instead of putting things on integral pixel boundaries.
    } else {
        // On screen, use a minimum thickness of 1.0 in user space (later rounded to an integral number in device space).
        if (thickness < 1)
            thickness = 1;

        // On screen, round all parameters to integer boundaries in device space.
        CGRect lineRect = CGContextConvertRectToDeviceSpace(cgContext, CGRectMake(x, y, width, thickness));
        lineRect.origin.x = roundf(lineRect.origin.x);
        lineRect.origin.y = roundf(lineRect.origin.y);
        lineRect.size.width = roundf(lineRect.size.width);
        lineRect.size.height = roundf(lineRect.size.height);
        if (lineRect.size.height == 0) // don't let thickness round down to 0 pixels
            lineRect.size.height = 1;
        lineRect = CGContextConvertRectToUserSpace(cgContext, lineRect);
        x = lineRect.origin.x;
        y = lineRect.origin.y;
        width = lineRect.size.width;
        thickness = lineRect.size.height;
    }

    // FIXME: How about using a rectangle fill instead of drawing a line?
    drawHorizontalLine(x, y, width, color, thickness, printing);
}

- (NSRect)selectionRectForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    if (shouldUseATSU(run))
        return ATSU_selectionRect(self, run, style, geometry);
    else
        return CG_selectionRect(self, run, style, geometry);
}

- (void)drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    if (shouldUseATSU(run))
        ATSU_drawHighlight(self, run, style, geometry);
    else
        CG_drawHighlight(self, run, style, geometry);
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

- (int)pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x includePartialGlyphs:(BOOL)includePartialGlyphs
{
    if (shouldUseATSU(run))
        return ATSU_pointToOffset(self, run, style, x, includePartialGlyphs);
    return CG_pointToOffset(self, run, style, x, includePartialGlyphs);
}

+ (void)setAlwaysUseATSU:(bool)f
{
    alwaysUseATSU = f;
}

@end

static WebTextRenderer *getSmallCapsRenderer(WebTextRenderer *renderer)
{
    if (!renderer->smallCapsRenderer) {
	NS_DURING
            float size = [renderer->font.font pointSize] * SMALLCAPS_FONTSIZE_MULTIPLIER;
            WebCoreFont smallCapsFont;
            WebCoreInitializeFont(&smallCapsFont);
            smallCapsFont.font = [[NSFontManager sharedFontManager] convertFont:renderer->font.font toSize:size];
	    renderer->smallCapsRenderer = [rendererForAlternateFont(renderer, smallCapsFont) retain];
	NS_HANDLER
            NSLog(@"uncaught exception selecting font for small caps: %@", localException);
	NS_ENDHANDLER
    }
    return renderer->smallCapsRenderer;
}

static inline bool fontContainsString(NSFont *font, NSString *string)
{
    NSCharacterSet *set = [[font coveredCharacterSet] invertedSet];
    return set && [string rangeOfCharacterFromSet:set].location == NSNotFound;
}

static NSFont *findSubstituteFont(WebTextRenderer *renderer, NSString *string, NSString **families)
{
    NSFont *substituteFont = nil;

    // First search the CSS family fallback list.
    // Start at 1 (2nd font) because we've already failed on the first lookup.
    NSString *family = nil;
    int i = 1;
    while (families && families[i]) {
        family = families[i++];
        NSFont *f = [[WebTextRendererFactory sharedFactory] cachedFontFromFamily:family
            traits:[[NSFontManager sharedFontManager] traitsOfFont:renderer->font.font]
            size:[renderer->font.font pointSize]];
        if (f && fontContainsString(f, string)) {
            substituteFont = f; 
            break;
        }
    }
    
    // Now do string based lookup.
    if (substituteFont == nil)
        substituteFont = WKGetFontInLanguageForRange(renderer->font.font, string, NSMakeRange(0, [string length]));

    // Now do character based lookup.
    if (substituteFont == nil && [string length] == 1)
        substituteFont = WKGetFontInLanguageForCharacter(renderer->font.font, [string characterAtIndex:0]);

    // Check to make sure this is a distinct font.
    if (substituteFont && [[substituteFont screenFont] isEqual:[renderer->font.font screenFont]])
        substituteFont = nil;

    // Now that we have a substitute font, attempt to match it to the best variation.
    // If we have a good match return that, otherwise return the font the AppKit has found.
    if (substituteFont) {
        NSFontManager *manager = [NSFontManager sharedFontManager];
        NSFont *bestVariation = [manager fontWithFamily:[substituteFont familyName]
            traits:[manager traitsOfFont:renderer->font.font]
            weight:[manager weightOfFont:renderer->font.font]
            size:[renderer->font.font pointSize]];
        if (bestVariation)
            substituteFont = bestVariation;
    }

    return substituteFont;
}

static WebTextRenderer *rendererForAlternateFont(WebTextRenderer *renderer, WebCoreFont alternateFont)
{
    if (!alternateFont.font)
        return nil;

    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSFontTraitMask fontTraits = [fontManager traitsOfFont:renderer->font.font];
    if (renderer->font.syntheticBold)
        fontTraits |= NSBoldFontMask;
    if (renderer->font.syntheticOblique)
        fontTraits |= NSItalicFontMask;
    NSFontTraitMask alternateFontTraits = [fontManager traitsOfFont:alternateFont.font];

    alternateFont.syntheticBold = (fontTraits & NSBoldFontMask) && !(alternateFontTraits & NSBoldFontMask);
    alternateFont.syntheticOblique = (fontTraits & NSItalicFontMask) && !(alternateFontTraits & NSItalicFontMask);
    alternateFont.forPrinter = renderer->font.forPrinter;

    return [[WebTextRendererFactory sharedFactory] rendererWithFont:alternateFont];
}

static WebTextRenderer *findSubstituteRenderer(WebTextRenderer *renderer, const unichar *characters, int numCharacters, NSString **families)
{
    WebCoreFont substituteFont;
    WebCoreInitializeFont(&substituteFont);
    NSString *string = [[NSString alloc] initWithCharactersNoCopy:(unichar *)characters length: numCharacters freeWhenDone: NO];
    substituteFont.font = findSubstituteFont(renderer, string, families);
    [string release];
    return rendererForAlternateFont(renderer, substituteFont);
}

// Nasty hack to determine if we should round or ceil space widths.
// If the font is monospace or fake monospace we ceil to ensure that 
// every character and the space are the same width.  Otherwise we round.
static bool computeWidthForSpace(WebTextRenderer *renderer)
{
    renderer->spaceGlyph = extendGlyphMap(renderer, SPACE);
    if (renderer->spaceGlyph == 0)
        return NO;

    float width = widthForGlyph(renderer, renderer->spaceGlyph);

    renderer->spaceWidth = width;

    renderer->treatAsFixedPitch = [[WebTextRendererFactory sharedFactory] isFontFixedPitch:renderer->font];
    renderer->adjustedSpaceWidth = renderer->treatAsFixedPitch ? ceilf(width) : roundf(width);
    
    return YES;
}

static bool setUpFont(WebTextRenderer *renderer)
{
    renderer->font.font = renderer->font.forPrinter ? [renderer->font.font printerFont] : [renderer->font.font screenFont];

    ATSUStyle fontStyle;
    if (ATSUCreateStyle(&fontStyle) != noErr)
        return NO;

    if (!fillStyleWithAttributes(fontStyle, renderer->font.font)) {
        ATSUDisposeStyle(fontStyle);
        return NO;
    }

    if (WKGetATSStyleGroup(fontStyle, &renderer->styleGroup) != noErr) {
        ATSUDisposeStyle(fontStyle);
        return NO;
    }

    ATSUDisposeStyle(fontStyle);

    if (!computeWidthForSpace(renderer)) {
        freeGlyphMap(renderer->characterToGlyphMap);
        renderer->characterToGlyphMap = 0;
        WKReleaseStyleGroup(renderer->styleGroup);
        renderer->styleGroup = 0;
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
    float syntheticBoldOffset, bool syntheticOblique)
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
        matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, -tanf(SYNTHETIC_OBLIQUE_ANGLE * acosf(0) / 90), 1, 0, 0)); 
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

static void CG_drawHighlight(WebTextRenderer *renderer, const WebCoreTextRun * run, const WebCoreTextStyle *style, const WebCoreTextGeometry *geometry)
{
    if (run->length == 0)
        return;

    if (style->backgroundColor == nil)
        return;

    [style->backgroundColor set];
    [NSBezierPath fillRect:CG_selectionRect(renderer, run, style, geometry)];
}

static NSRect CG_selectionRect(WebTextRenderer *renderer, const WebCoreTextRun * run, const WebCoreTextStyle *style, const WebCoreTextGeometry *geometry)
{
    float yPos = geometry->useFontMetricsForSelectionYAndHeight
        ? geometry->point.y - renderer->ascent - (renderer->lineGap / 2) : geometry->selectionY;
    float height = geometry->useFontMetricsForSelectionYAndHeight
        ? renderer->lineSpacing : geometry->selectionHeight;

    WebCoreTextRun completeRun = *run;
    completeRun.from = 0;
    completeRun.to = run->length;

    WidthIterator it;
    initializeWidthIterator(&it, renderer, &completeRun, style);
    
    advanceWidthIterator(&it, run->from, 0, 0, 0);
    float beforeWidth = it.runWidthSoFar;
    advanceWidthIterator(&it, run->to, 0, 0, 0);
    float afterWidth = it.runWidthSoFar;
    // Using roundf() rather than ceilf() for the right edge as a compromise to ensure correct caret positioning
    if (style->rtl) {
        advanceWidthIterator(&it, run->length, 0, 0, 0);
        float totalWidth = it.runWidthSoFar;
        return NSMakeRect(geometry->point.x + floorf(totalWidth - afterWidth), yPos, roundf(totalWidth - beforeWidth) - floorf(totalWidth - afterWidth), height);
    } else {
        return NSMakeRect(geometry->point.x + floorf(beforeWidth), yPos, roundf(afterWidth) - floorf(beforeWidth), height);
    }
}

static void CG_draw(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, const WebCoreTextGeometry *geometry)
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

    if (length * MAX_GLYPH_EXPANSION > LOCAL_BUFFER_SIZE) {
        advances = malloc(length * MAX_GLYPH_EXPANSION * sizeof(CGSize));
        widthBuffer = malloc(length * MAX_GLYPH_EXPANSION * sizeof(float));
        glyphBuffer = malloc(length * MAX_GLYPH_EXPANSION * sizeof(ATSGlyphRef));
        rendererBuffer = malloc(length * MAX_GLYPH_EXPANSION * sizeof(WebTextRenderer *));
    } else {
        advances = localAdvanceBuffer;
        widthBuffer = localWidthBuffer;
        glyphBuffer = localGlyphBuffer;
        rendererBuffer = localRendererBuffer;
    }

    CG_floatWidthForRun(renderer, run, style, widthBuffer, rendererBuffer, glyphBuffer, &startX, &numGlyphs);
        
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
        CG_drawHighlight(renderer, run, style, geometry);
    
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

    // Draw each contiguous run of glyphs that use the same renderer.
    WebTextRenderer *currentRenderer = rendererBuffer[0];
    float nextX = startX;
    int lastFrom = 0;
    int nextGlyph = 0;
    while (nextGlyph < numGlyphs) {
        WebTextRenderer *nextRenderer = rendererBuffer[nextGlyph];
        if (nextRenderer != currentRenderer) {
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

static float CG_floatWidthForRun(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, float *widthBuffer, WebTextRenderer **rendererBuffer, CGGlyph *glyphBuffer, float *startPosition, int *numGlyphsResult)
{
    WidthIterator it;
    WebCoreTextRun completeRun;
    const WebCoreTextRun *aRun;
    if (!style->rtl)
        aRun = run;
    else {
        completeRun = *run;
        completeRun.to = run->length;
        aRun = &completeRun;
    }
    initializeWidthIterator(&it, renderer, aRun, style);
    int numGlyphs = advanceWidthIterator(&it, run->to, widthBuffer, rendererBuffer, glyphBuffer);
    float runWidth = it.runWidthSoFar;
    if (startPosition) {
        if (!style->rtl)
            *startPosition = it.widthToStart;
        else {
            float finalRoundingWidth = it.finalRoundingWidth;
            advanceWidthIterator(&it, run->length, 0, 0, 0);
            *startPosition = it.runWidthSoFar - runWidth + finalRoundingWidth;
        }
    }
    if (numGlyphsResult)
        *numGlyphsResult = numGlyphs;
    return runWidth;
}

static void updateGlyphMapEntry(WebTextRenderer *renderer, UChar32 c, ATSGlyphRef glyph, WebTextRenderer *substituteRenderer)
{
    GlyphMap *map;
    for (map = renderer->characterToGlyphMap; map; map = map->next) {
        UChar32 start = map->startRange;
        if (c >= start && c <= map->endRange) {
            int i = c - start;
            map->glyphs[i].glyph = glyph;
            // This renderer will leak.
            // No problem though; we want it to stick around forever.
            // Max theoretical retain counts applied here will be num_fonts_on_system * num_glyphs_in_font.
            map->glyphs[i].renderer = [substituteRenderer retain];
            break;
        }
    }
}

static ATSGlyphRef extendGlyphMap(WebTextRenderer *renderer, UChar32 c)
{
    GlyphMap *map = malloc(sizeof(GlyphMap));
    ATSLayoutRecord *glyphRecord;
    char glyphVector[WKGlyphVectorSize];
    UChar32 end, start;
    unsigned blockSize;
    
    if (renderer->characterToGlyphMap == 0)
        blockSize = INITIAL_BLOCK_SIZE;
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (c / blockSize) * blockSize;
    end = start + (blockSize - 1);
        
    LOG(FontCache, "%@ (0x%04x) adding glyphs for 0x%04x to 0x%04x", renderer->font, c, start, end);

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

    WKConvertCharToGlyphs(renderer->styleGroup, &buffer[0], bufferLength, &glyphVector);
    unsigned numGlyphs = WKGetGlyphVectorNumGlyphs(&glyphVector);
    if (numGlyphs != count) {
        // This should never happen, perhaps indicates a bad font?
        // If it does happen, the font substitution code will find an alternate font.
        WKClearGlyphVector(&glyphVector);
        free(map);
        return 0;
    }

    map->glyphs = malloc(count * sizeof(GlyphEntry));
    glyphRecord = (ATSLayoutRecord *)WKGetGlyphVectorFirstRecord(glyphVector);
    for (i = 0; i < count; i++) {
        map->glyphs[i].glyph = glyphRecord->glyphID;
        map->glyphs[i].renderer = renderer;
        glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + WKGetGlyphVectorRecordSize(glyphVector));
    }
    WKClearGlyphVector(&glyphVector);
    
    if (renderer->characterToGlyphMap == 0)
        renderer->characterToGlyphMap = map;
    else {
        GlyphMap *lastMap = renderer->characterToGlyphMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    ATSGlyphRef glyph = map->glyphs[c - start].glyph;

    // Special case for characters 007F-00A0.
    if (glyph == 0 && c >= 0x7F && c <= 0xA0) {
        glyph = WKGetDefaultGlyphForChar(renderer->font.font, c);
        map->glyphs[c - start].glyph = glyph;
    }

    return glyph;
}

static WidthMap *extendWidthMap(WebTextRenderer *renderer, ATSGlyphRef glyph)
{
    WidthMap *map = (WidthMap *)calloc(1, sizeof(WidthMap));
    unsigned end;
    ATSGlyphRef start;
    unsigned blockSize;
    unsigned i, count;
    
    NSFont *f = renderer->font.font;
    if (renderer->glyphToWidthMap == 0) {
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

    LOG(FontCache, "%@ (0x%04x) adding widths for range 0x%04x to 0x%04x", renderer->font, glyph, start, end);

    map->startRange = start;
    map->endRange = end;
    count = end - start + 1;

    map->widths = malloc(count * sizeof(WebGlyphWidth));
    for (i = 0; i < count; i++)
        map->widths[i] = NAN;

    if (renderer->glyphToWidthMap == 0)
        renderer->glyphToWidthMap = map;
    else {
        WidthMap *lastMap = renderer->glyphToWidthMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    return map;
}

static void initializeATSUStyle(WebTextRenderer *renderer)
{
    // The two NSFont calls in this method (pointSize and _atsFontID) do not raise exceptions.

    if (!renderer->ATSUStyleInitialized) {
        OSStatus status;
        ByteCount propTableSize;
        
        status = ATSUCreateStyle(&renderer->_ATSUStyle);
        if (status != noErr)
            FATAL_ALWAYS("ATSUCreateStyle failed (%d)", status);
    
        ATSUFontID fontID = WKGetNSFontATSUFontId(renderer->font.font);
        if (fontID == 0) {
            ATSUDisposeStyle(renderer->_ATSUStyle);
            ERROR("unable to get ATSUFontID for %@", renderer->font.font);
            return;
        }
        
        CGAffineTransform transform = CGAffineTransformMakeScale(1, -1);
        if (renderer->font.syntheticOblique)
            transform = CGAffineTransformConcat(transform, CGAffineTransformMake(1, 0, -tanf(SYNTHETIC_OBLIQUE_ANGLE * acosf(0) / 90), 1, 0, 0)); 
        Fixed fontSize = FloatToFixed([renderer->font.font pointSize]);
        // Turn off automatic kerning until it is supported in the CG code path (6136 in bugzilla)
        Fract kerningInhibitFactor = FloatToFract(1.0);
        ATSUAttributeTag styleTags[4] = { kATSUSizeTag, kATSUFontTag, kATSUFontMatrixTag, kATSUKerningInhibitFactorTag };
        ByteCount styleSizes[4] = { sizeof(Fixed), sizeof(ATSUFontID), sizeof(CGAffineTransform), sizeof(Fract) };
        ATSUAttributeValuePtr styleValues[4] = { &fontSize, &fontID, &transform, &kerningInhibitFactor };
        status = ATSUSetAttributes(renderer->_ATSUStyle, 4, styleTags, styleSizes, styleValues);
        if (status != noErr)
            FATAL_ALWAYS("ATSUSetAttributes failed (%d)", status);
        status = ATSFontGetTable(fontID, 'prop', 0, 0, 0, &propTableSize);
        if (status == noErr)    // naively assume that if a 'prop' table exists then it contains mirroring info
            renderer->ATSUMirrors = YES;
        else if (status == kATSInvalidFontTableAccess)
            renderer->ATSUMirrors = NO;
        else
            FATAL_ALWAYS("ATSFontGetTable failed (%d)", status);

        // Turn off ligatures such as 'fi' to match the CG code path's behavior, until bugzilla 6135 is fixed.
        // Don't be too aggressive: if the font doesn't contain 'a', then assume that any ligatures it contains are
        // in characters that always go through ATSUI, and therefore allow them. Geeza Pro is an example.
        // See bugzilla 5166.
        if ([[renderer->font.font coveredCharacterSet] characterIsMember:'a']) {
            ATSUFontFeatureType featureTypes[] = { kLigaturesType };
            ATSUFontFeatureSelector featureSelectors[] = { kCommonLigaturesOffSelector };
            status = ATSUSetFontFeatures(renderer->_ATSUStyle, 1, featureTypes, featureSelectors);
        }

        renderer->ATSUStyleInitialized = YES;
    }
}

static void createATSULayoutParameters(ATSULayoutParameters *params, WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style)
{
    params->run = run;
    params->style = style;
    // FIXME: It is probably best to always allocate a buffer for RTL, since even if for this
    // renderer ATSUMirrors is true, for a substitute renderer it might be false.
    WebTextRenderer **renderers = malloc(run->length * sizeof(WebTextRenderer *));
    params->renderers = renderers;
    UniChar *charBuffer = (style->smallCaps || (style->rtl && !renderer->ATSUMirrors)) ? malloc(run->length * sizeof(UniChar)) : 0;
    params->charBuffer = charBuffer;
    params->syntheticBoldPass = false;

    // The only Cocoa calls here are to NSGraphicsContext, which does not raise exceptions.

    ATSUTextLayout layout;
    OSStatus status;
    ATSULayoutOperationOverrideSpecifier overrideSpecifier;
    
    initializeATSUStyle(renderer);
    
    // FIXME: This is currently missing the following required features that the CoreGraphics code path has:
    // - \n, \t, and nonbreaking space render as a space.
    // - Other control characters do not render (other code path uses zero-width spaces).

    UniCharCount totalLength = run->length;
    UniCharArrayOffset runTo = (run->to == -1 ? totalLength : (unsigned int)run->to);
    UniCharArrayOffset runFrom = run->from;
    
    if (charBuffer)
        memcpy(charBuffer, run->characters, totalLength * sizeof(UniChar));

    UniCharCount runLength = runTo - runFrom;
    
    status = ATSUCreateTextLayoutWithTextPtr(
            (charBuffer ? charBuffer : run->characters),
            runFrom,        // offset
            runLength,      // length
            totalLength,    // total length
            1,              // styleRunCount
            &runLength,     // length of style run
            &renderer->_ATSUStyle, 
            &layout);
    if (status != noErr)
        FATAL_ALWAYS("ATSUCreateTextLayoutWithTextPtr failed(%d)", status);
    params->layout = layout;
    ATSUSetTextLayoutRefCon(layout, (UInt32)params);

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

    params->hasSyntheticBold = false;
    ATSUFontID ATSUSubstituteFont;
    UniCharArrayOffset substituteOffset = runFrom;
    UniCharCount substituteLength;
    UniCharArrayOffset lastOffset;
    WebTextRenderer *substituteRenderer = 0;

    while (substituteOffset < runTo) {
        lastOffset = substituteOffset;
        status = ATSUMatchFontsToText(layout, substituteOffset, kATSUToTextEnd, &ATSUSubstituteFont, &substituteOffset, &substituteLength);
        if (status == kATSUFontsMatched || status == kATSUFontsNotMatched) {
            substituteRenderer = findSubstituteRenderer(renderer, run->characters+substituteOffset, substituteLength, style->families);
            if (substituteRenderer) {
                initializeATSUStyle(substituteRenderer);
                if (substituteRenderer->_ATSUStyle)
                    ATSUSetRunStyle(layout, substituteRenderer->_ATSUStyle, substituteOffset, substituteLength);
            } else
                substituteRenderer = renderer;
        } else {
            substituteOffset = runTo;
            substituteLength = 0;
        }

        bool isSmallCap = false;
        UniCharArrayOffset firstSmallCap = 0;
        WebTextRenderer *r = renderer;
        UniCharArrayOffset i;
        for (i = lastOffset;  ; i++) {
            if (i == substituteOffset || i == substituteOffset + substituteLength) {
                if (isSmallCap) {
                    isSmallCap = false;
                    initializeATSUStyle(getSmallCapsRenderer(r));
                    ATSUSetRunStyle(layout, getSmallCapsRenderer(r)->_ATSUStyle, firstSmallCap, i - firstSmallCap);
                }
                if (i == substituteOffset && substituteLength > 0)
                    r = substituteRenderer;
                else
                    break;
            }
            if (rtl && charBuffer && !r->ATSUMirrors)
                charBuffer[i] = u_charMirror(charBuffer[i]);
            if (style->smallCaps) {
                UniChar c = charBuffer[i];
                UniChar newC;
                if (U_GET_GC_MASK(c) & U_GC_M_MASK)
                    renderers[i] = isSmallCap ? getSmallCapsRenderer(r) : r;
                else if (!u_isUUppercase(c) && (newC = u_toupper(c)) != c) {
                    charBuffer[i] = newC;
                    if (!isSmallCap) {
                        isSmallCap = true;
                        firstSmallCap = i;
                    }
                    renderers[i] = getSmallCapsRenderer(r);
                } else {
                    if (isSmallCap) {
                        isSmallCap = false;
                        initializeATSUStyle(getSmallCapsRenderer(r));
                        ATSUSetRunStyle(layout, getSmallCapsRenderer(r)->_ATSUStyle, firstSmallCap, i - firstSmallCap);
                    }
                    renderers[i] = r;
                }
            } else
                renderers[i] = r;
            if (renderers[i]->syntheticBoldOffset)
                params->hasSyntheticBold = true;
        }
        substituteOffset += substituteLength;
    }
    if (style->padding) {
        float numSpaces = 0;
        unsigned k;
        for (k = 0; k < totalLength; k++)
            if (isSpace(run->characters[k]))
                numSpaces++;

        params->padPerSpace = ceilf(style->padding / numSpaces);
    } else
        params->padPerSpace = 0;
}

static void disposeATSULayoutParameters(ATSULayoutParameters *params)
{
    ATSUDisposeTextLayout(params->layout);
    free(params->charBuffer);
    free(params->renderers);
}

static ATSTrapezoid getTextBounds(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, NSPoint p)
{
    OSStatus status;
    
    if (run->to - run->from <= 0) {
        ATSTrapezoid nilTrapezoid = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
        return nilTrapezoid;
    }

    ATSULayoutParameters params;
    createATSULayoutParameters(&params, renderer, run, style);

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
    ATSTrapezoid oGlyphBounds = getTextBounds(renderer, run, style, NSZeroPoint);
    return MAX(FixedToFloat(oGlyphBounds.upperRight.x), FixedToFloat(oGlyphBounds.lowerRight.x)) -
        MIN(FixedToFloat(oGlyphBounds.upperLeft.x), FixedToFloat(oGlyphBounds.lowerLeft.x));
}

// Be sure to free the run.characters allocated by this function.
static WebCoreTextRun addDirectionalOverride(const WebCoreTextRun *run, bool rtl)
{
    int from = run->from;
    int to = run->to;
    if (from == -1)
        from = 0;
    if (to == -1)
        to = run->length;

    UniChar *charactersWithOverride = malloc(sizeof(UniChar) * (run->length + 2));

    charactersWithOverride[0] = rtl ? RIGHT_TO_LEFT_OVERRIDE : LEFT_TO_RIGHT_OVERRIDE;
    memcpy(&charactersWithOverride[1], &run->characters[0], sizeof(UniChar) * run->length);
    charactersWithOverride[run->length + 1] = POP_DIRECTIONAL_FORMATTING;

    WebCoreTextRun runWithOverride;

    runWithOverride.from = from + 1;
    runWithOverride.to = to + 1;
    runWithOverride.length = run->length + 2;
    runWithOverride.characters = charactersWithOverride;

    return runWithOverride;
}

static void ATSU_drawHighlight(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, const WebCoreTextGeometry *geometry)
{
    // The only Cocoa calls made here are to NSColor and NSBezierPath, and they do not raise exceptions.

    if (style->backgroundColor == nil)
        return;
    if (run->to <= run->from)
        return;
    
    [style->backgroundColor set];
    [NSBezierPath fillRect:ATSU_selectionRect(renderer, run, style, geometry)];
}

static NSRect ATSU_selectionRect(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, const WebCoreTextGeometry *geometry)
{
    int from = run->from;
    int to = run->to;
    if (from == -1)
        from = 0;
    if (to == -1)
        to = run->length;
        
    WebCoreTextRun completeRun = *run;
    completeRun.from = 0;
    completeRun.to = run->length;
    
    WebCoreTextRun *aRun = &completeRun;
    WebCoreTextRun swappedRun;
    
    if (style->directionalOverride) {
        swappedRun = addDirectionalOverride(aRun, style->rtl);
        aRun = &swappedRun;
        from++;
        to++;
    }
   
    ATSULayoutParameters params;
    createATSULayoutParameters(&params, renderer, aRun, style);
    
    ATSTrapezoid firstGlyphBounds;
    ItemCount actualNumBounds;
    
    OSStatus status = ATSUGetGlyphBounds(params.layout, 0, 0, from, to - from, kATSUseFractionalOrigins, 1, &firstGlyphBounds, &actualNumBounds);
    if (status != noErr || actualNumBounds != 1) {
        static ATSTrapezoid zeroTrapezoid = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
        firstGlyphBounds = zeroTrapezoid;
    }
    disposeATSULayoutParameters(&params);    
    
    float beforeWidth = MIN(FixedToFloat(firstGlyphBounds.lowerLeft.x), FixedToFloat(firstGlyphBounds.upperLeft.x));
    float afterWidth = MAX(FixedToFloat(firstGlyphBounds.lowerRight.x), FixedToFloat(firstGlyphBounds.upperRight.x));
    float yPos = geometry->useFontMetricsForSelectionYAndHeight
        ? geometry->point.y - renderer->ascent : geometry->selectionY;
    float height = geometry->useFontMetricsForSelectionYAndHeight
        ? renderer->lineSpacing : geometry->selectionHeight;

    NSRect rect = NSMakeRect(geometry->point.x + floorf(beforeWidth), yPos, roundf(afterWidth) - floorf(beforeWidth), height);

    if (style->directionalOverride)
        free((void *)swappedRun.characters);

    return rect;
}


static void ATSU_draw(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, const WebCoreTextGeometry *geometry)
{
    // The only Cocoa calls made here are to NSColor and NSGraphicsContext, and they do not raise exceptions.

    OSStatus status;
    int from, to;
    const WebCoreTextRun *aRun = run;
    WebCoreTextRun swappedRun;
    
    if (style->directionalOverride) {
        swappedRun = addDirectionalOverride(run, style->rtl);
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
    createATSULayoutParameters(&params, renderer, &completeRun, style);

    if (style->backgroundColor != nil)
        ATSU_drawHighlight(renderer, run, style, geometry);

    [style->textColor set];

    // ATSUI can't draw beyond -32768 to +32767 so we translate the CTM and tell ATSUI to draw at (0, 0).
    NSGraphicsContext *gContext = [NSGraphicsContext currentContext];
    CGContextRef context = (CGContextRef)[gContext graphicsPort];
    CGContextTranslateCTM(context, geometry->point.x, geometry->point.y);
    bool flipped = [gContext isFlipped];
    if (!flipped)
        CGContextScaleCTM(context, 1.0, -1.0);
    status = ATSUDrawText(params.layout, aRun->from, runLength, 0, 0);
    if (status == noErr && params.hasSyntheticBold) {
        // Force relayout for the bold pass
        ATSUClearLayoutCache(params.layout, 0);
        params.syntheticBoldPass = true;
        status = ATSUDrawText(params.layout, aRun->from, runLength, 0, 0);
    }
    if (!flipped)
        CGContextScaleCTM(context, 1.0, -1.0);
    CGContextTranslateCTM(context, -geometry->point.x, -geometry->point.y);

    if (status != noErr) {
        // Nothing to do but report the error (dev build only).
        ERROR("ATSUDrawText() failed(%d)", status);
    }

    disposeATSULayoutParameters(&params);
    
    if (style->directionalOverride)
        free((void *)swappedRun.characters);
}

static int ATSU_pointToOffset(WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style,
    int x, bool includePartialGlyphs)
{
    const WebCoreTextRun *aRun = run;
    WebCoreTextRun swappedRun;
    
    // Enclose in LRO/RLO - PDF to force ATSU to render visually.
    if (style->directionalOverride) {
        swappedRun = addDirectionalOverride(aRun, style->rtl);
        aRun = &swappedRun;
    }

    ATSULayoutParameters params;
    createATSULayoutParameters(&params, renderer, aRun, style);

    UniCharArrayOffset primaryOffset = aRun->from;
    
    // FIXME: No idea how to avoid including partial glyphs.
    // Not even sure if that's the behavior this yields now.
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
    
    if (style->directionalOverride)
        free((void *)swappedRun.characters);

    return offset - aRun->from;
}

static bool advanceWidthIteratorOneCharacter(WidthIterator *iterator, float *totalWidth)
{
    float widths[MAX_GLYPH_EXPANSION];
    WebTextRenderer *renderers[MAX_GLYPH_EXPANSION];
    ATSGlyphRef glyphs[MAX_GLYPH_EXPANSION];            
    unsigned numGlyphs = advanceWidthIterator(iterator, iterator->currentCharacter + 1, widths, renderers, glyphs);
    unsigned i;
    float w = 0;
    for (i = 0; i < numGlyphs; ++i)
        w += widths[i];
    *totalWidth = w;
    return numGlyphs != 0;
}

static int CG_pointToOffset(WebTextRenderer *renderer, const WebCoreTextRun * run, const WebCoreTextStyle *style,
    int x, bool includePartialGlyphs)
{
    float delta = (float)x;

    WidthIterator it;    
    initializeWidthIterator(&it, renderer, run, style);

    unsigned offset;

    if (style->rtl) {
        delta -= CG_floatWidthForRun(renderer, run, style, 0, 0, 0, 0, 0);
        while (1) {
            offset = it.currentCharacter;
            float w;
            if (!advanceWidthIteratorOneCharacter(&it, &w))
                break;
            delta += w;
            if (includePartialGlyphs) {
                if (delta - w / 2 >= 0)
                    break;
            } else {
                if (delta >= 0)
                    break;
            }
        }
    } else {
        while (1) {
            offset = it.currentCharacter;
            float w;
            if (!advanceWidthIteratorOneCharacter(&it, &w))
                break;
            delta -= w;
            if (includePartialGlyphs) {
                if (delta + w / 2 <= 0)
                    break;
            } else {
                if (delta <= 0)
                    break;
            }
        }
    }

    return offset - run->from;
}

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

static inline ATSGlyphRef glyphForCharacter(WebTextRenderer **renderer, UChar32 c)
{
    // this loop is hot, so it is written to avoid LSU stalls
    GlyphMap *map;
    GlyphMap *nextMap;
    for (map = (*renderer)->characterToGlyphMap; map; map = nextMap) {
        UChar32 start = map->startRange;
        nextMap = map->next;
        if (c >= start && c <= map->endRange) {
            GlyphEntry *ge = &map->glyphs[c - start];
            *renderer = ge->renderer;
            return ge->glyph;
        }
    }

    return extendGlyphMap(*renderer, c);
}

static void initializeWidthIterator(WidthIterator *iterator, WebTextRenderer *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style) 
{
    iterator->renderer = renderer;
    iterator->run = run;
    iterator->style = style;
    iterator->currentCharacter = run->from;
    iterator->runWidthSoFar = 0;
    iterator->finalRoundingWidth = 0;

    // If the padding is non-zero, count the number of spaces in the run
    // and divide that by the padding for per space addition.
    if (!style->padding) {
        iterator->padding = 0;
        iterator->padPerSpace = 0;
    } else {
        float numSpaces = 0;
        int k;
        for (k = run->from; k < run->to; k++)
            if (isSpace(run->characters[k]))
                numSpaces++;

        iterator->padding = style->padding;
        iterator->padPerSpace = ceilf(iterator->padding / numSpaces);
    }
    
    // Calculate width up to starting position of the run.  This is
    // necessary to ensure that our rounding hacks are always consistently
    // applied.
    if (run->from == 0) {
        iterator->widthToStart = 0;
    } else {
        WebCoreTextRun startPositionRun = *run;
        startPositionRun.from = 0;
        startPositionRun.to = run->length;
        WidthIterator startPositionIterator;
        initializeWidthIterator(&startPositionIterator, renderer, &startPositionRun, style);
        advanceWidthIterator(&startPositionIterator, run->from, 0, 0, 0);
        iterator->widthToStart = startPositionIterator.runWidthSoFar;
    }
}

static UChar32 normalizeVoicingMarks(WidthIterator *iterator)
{
    unsigned currentCharacter = iterator->currentCharacter;
    const WebCoreTextRun *run = iterator->run;
    if (currentCharacter + 1 < (unsigned)run->to) {
        if (u_getCombiningClass(run->characters[currentCharacter + 1]) == HIRAGANA_KATAKANA_VOICING_MARKS) {
            // Normalize into composed form using 3.2 rules.
            UChar normalizedCharacters[2] = { 0, 0 };
            UErrorCode uStatus = 0;                
            int32_t resultLength = unorm_normalize(&run->characters[currentCharacter], 2,
                UNORM_NFC, UNORM_UNICODE_3_2, &normalizedCharacters[0], 2, &uStatus);
            if (resultLength == 1 && uStatus == 0)
                return normalizedCharacters[0];
        }
    }
    return 0;
}

static unsigned advanceWidthIterator(WidthIterator *iterator, unsigned offset, float *widths, WebTextRenderer **renderersUsed, ATSGlyphRef *glyphsUsed)
{
    const WebCoreTextRun *run = iterator->run;
    if (offset > (unsigned)run->to)
        offset = run->to;

    unsigned numGlyphs = 0;

    unsigned currentCharacter = iterator->currentCharacter;
    const UniChar *cp = &run->characters[currentCharacter];

    const WebCoreTextStyle *style = iterator->style;
    bool rtl = style->rtl;
    bool needCharTransform = rtl | style->smallCaps;
    bool hasExtraSpacing = style->letterSpacing | style->wordSpacing | style->padding;

    float runWidthSoFar = iterator->runWidthSoFar;
    float lastRoundingWidth = iterator->finalRoundingWidth;

    while (currentCharacter < offset) {
        UChar32 c = *cp;

        unsigned clusterLength = 1;
        if (c >= 0x3041) {
            if (c <= 0x30FE) {
                // Deal with Hiragana and Katakana voiced and semi-voiced syllables.
                // Normalize into composed form, and then look for glyph with base + combined mark.
                // Check above for character range to minimize performance impact.
                UChar32 normalized = normalizeVoicingMarks(iterator);
                if (normalized) {
                    c = normalized;
                    clusterLength = 2;
                }
            } else if (U16_IS_SURROGATE(c)) {
                if (!U16_IS_SURROGATE_LEAD(c))
                    break;

                // Do we have a surrogate pair?  If so, determine the full Unicode (32 bit)
                // code point before glyph lookup.
                // Make sure we have another character and it's a low surrogate.
                if (currentCharacter + 1 >= run->length)
                    break;
                UniChar low = cp[1];
                if (!U16_IS_TRAIL(low))
                    break;
                c = U16_GET_SUPPLEMENTARY(c, low);
                clusterLength = 2;
            }
        }

        WebTextRenderer *renderer = iterator->renderer;

        if (needCharTransform) {
            if (rtl)
                c = u_charMirror(c);

            // If small-caps, convert lowercase to upper.
            if (style->smallCaps && !u_isUUppercase(c)) {
                UChar32 upperC = u_toupper(c);
                if (upperC != c) {
                    c = upperC;
                    renderer = getSmallCapsRenderer(renderer);
                }
            }
        }

        ATSGlyphRef glyph = glyphForCharacter(&renderer, c);

        // Now that we have glyph and font, get its width.
        WebGlyphWidth width;
        if (c == '\t' && style->tabWidth) {
            width = style->tabWidth - fmodf(style->xpos + runWidthSoFar, style->tabWidth);
        } else {
            width = widthForGlyph(renderer, glyph);
            // We special case spaces in two ways when applying word rounding.
            // First, we round spaces to an adjusted width in all fonts.
            // Second, in fixed-pitch fonts we ensure that all characters that
            // match the width of the space character have the same width as the space character.
            if (width == renderer->spaceWidth && (renderer->treatAsFixedPitch || glyph == renderer->spaceGlyph) && style->applyWordRounding)
                width = renderer->adjustedSpaceWidth;
        }

        // Try to find a substitute font if this font didn't have a glyph for a character in the
        // string. If one isn't found we end up drawing and measuring the 0 glyph, usually a box.
        if (glyph == 0 && style->attemptFontSubstitution) {
            WebTextRenderer *substituteRenderer = findSubstituteRenderer(renderer, cp, clusterLength, style->families);
            if (substituteRenderer) {
                WebCoreTextRun clusterRun = { cp, clusterLength, 0, clusterLength };
                WebCoreTextStyle clusterStyle = *style;
                clusterStyle.padding = 0;
                clusterStyle.applyRunRounding = NO;
                clusterStyle.attemptFontSubstitution = NO;
                
                int cNumGlyphs;
                float localWidthBuffer[MAX_GLYPH_EXPANSION];
                WebTextRenderer *localRendererBuffer[MAX_GLYPH_EXPANSION];
                ATSGlyphRef localGlyphBuffer[MAX_GLYPH_EXPANSION];            
                CG_floatWidthForRun(substituteRenderer, &clusterRun, &clusterStyle, localWidthBuffer, localRendererBuffer, localGlyphBuffer, 0, &cNumGlyphs);
                if (cNumGlyphs == 1) {
                    ASSERT(substituteRenderer == localRendererBuffer[0]);
                    width = localWidthBuffer[0];
                    glyph = localGlyphBuffer[0];
                    updateGlyphMapEntry(renderer, c, glyph, substituteRenderer);
                    renderer = substituteRenderer;
                }
            }
        }

        if (hasExtraSpacing) {
            // Account for letter-spacing.
            if (width && style->letterSpacing)
                width += style->letterSpacing;

            if (isSpace(c)) {
                // Account for padding. WebCore uses space padding to justify text.
                // We distribute the specified padding over the available spaces in the run.
                if (style->padding) {
                    // Use left over padding if not evenly divisible by number of spaces.
                    if (iterator->padding < iterator->padPerSpace) {
                        width += iterator->padding;
                        iterator->padding = 0;
                    } else {
                        width += iterator->padPerSpace;
                        iterator->padding -= iterator->padPerSpace;
                    }
                }

                // Account for word spacing.
                // We apply additional space between "words" by adding width to the space character.
                if (currentCharacter != 0 && !isSpace(cp[-1]) && style->wordSpacing)
                    width += style->wordSpacing;
            }
        }

        // Advance past the character we just dealt with.
        cp += clusterLength;
        currentCharacter += clusterLength;

        // Account for float/integer impedance mismatch between CG and KHTML. "Words" (characters 
        // followed by a character defined by isRoundingHackCharacter()) are always an integer width.
        // We adjust the width of the last character of a "word" to ensure an integer width.
        // If we move KHTML to floats we can remove this (and related) hacks.

        float oldWidth = width;

        // Force characters that are used to determine word boundaries for the rounding hack
        // to be integer width, so following words will start on an integer boundary.
        if (style->applyWordRounding && isRoundingHackCharacter(c))
            width = ceilf(width);

        // Check to see if the next character is a "rounding hack character", if so, adjust
        // width so that the total run width will be on an integer boundary.
        if ((style->applyWordRounding && currentCharacter < run->length && isRoundingHackCharacter(*cp))
                || (style->applyRunRounding && currentCharacter >= (unsigned)run->to)) {
            float totalWidth = iterator->widthToStart + runWidthSoFar + width;
            width += ceilf(totalWidth) - totalWidth;
        }

        runWidthSoFar += width;

        if (!widths) {
            ASSERT(!renderersUsed);
            ASSERT(!glyphsUsed);
        } else {
            ASSERT(renderersUsed);
            ASSERT(glyphsUsed);
            *widths++ = (rtl ? oldWidth + lastRoundingWidth : width);
            *renderersUsed++ = renderer;
            *glyphsUsed++ = glyph;
        }

        lastRoundingWidth = width - oldWidth;
        ++numGlyphs;
    }

    iterator->currentCharacter = currentCharacter;
    iterator->runWidthSoFar = runWidthSoFar;
    iterator->finalRoundingWidth = lastRoundingWidth;

    return numGlyphs;
}

static bool fillStyleWithAttributes(ATSUStyle style, NSFont *theFont)
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

static bool shouldUseATSU(const WebCoreTextRun *run)
{
    if (alwaysUseATSU)
        return YES;
        
    const UniChar *characters = run->characters;
    int to = run->to;
    int i;
    // Start from 0 since drawing and highlighting also measure the characters before run->from
    for (i = 0; i < to; i++) {
        UniChar c = characters[i];
        if (c < 0x300)      // U+0300 through U+036F Combining diacritical marks
            continue;
        if (c <= 0x36F)
            return YES;

        if (c < 0x0591 || c == 0x05BE)     // U+0591 through U+05CF excluding U+05BE Hebrew combining marks, Hebrew punctuation Paseq, Sof Pasuq and Nun Hafukha
            continue;
        if (c <= 0x05CF)
            return YES;

        if (c < 0x0600)     // U+0600 through U+1059 Arabic, Syriac, Thaana, Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
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
