/*	
        IFTextRenderer.m
	    
	    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFTextRenderer.h"

#import <Cocoa/Cocoa.h>

#import <ApplicationServices/ApplicationServices.h>
#import <CoreGraphics/CoreGraphicsPrivate.h>

#import <WebKit/IFTextRendererFactory.h>
#import <WebKit/WebKitDebug.h>

#import <QD/ATSUnicodePriv.h>

#define NON_BREAKING_SPACE 0xA0
#define SPACE 0x20

#define ROUND_TO_INT(x) (unsigned int)((x)+.5)

#define LOCAL_BUFFER_SIZE 1024

// Covers most of latin1.
#define INITIAL_BLOCK_SIZE 0x200

// Get additional blocks of glyphs and widths in bigger chunks.
// This will typically be for other character sets.
#define INCREMENTAL_BLOCK_SIZE 0x400

#define UNINITIALIZED_GLYPH_WIDTH 65535

// combining char, hangul jamo, or Apple corporate variant tag
#define JunseongStart 0x1160
#define JonseongEnd 0x11F9
#define IsHangulConjoiningJamo(X) (X >= JunseongStart && X <= JonseongEnd)
#define IsNonBaseChar(X) ((CFCharacterSetIsCharacterMember(nonBaseChars, X) || IsHangulConjoiningJamo(X) || (((X) & 0x1FFFF0) == 0xF870)))


typedef float IFGlyphWidth;

struct WidthMap {
    ATSGlyphRef startRange;
    ATSGlyphRef endRange;
    WidthMap *next;
    IFGlyphWidth *widths;
};

struct GlyphMap {
    UniChar startRange;
    UniChar endRange;
    GlyphMap *next;
    ATSGlyphRef *glyphs;
};


@interface NSLanguage : NSObject 
{
}
+ (NSLanguage *)defaultLanguage;
@end

@interface NSFont (IFPrivate)
- (ATSUFontID)_atsFontID;
- (CGFontRef)_backingCGSFont;
// Private method to find a font for a character.
+ (NSFont *) findFontLike:(NSFont *)aFont forCharacter:(UInt32)c inLanguage:(NSLanguage *) language;
+ (NSFont *) findFontLike:(NSFont *)aFont forString:(NSString *)string withRange:(NSRange)range inLanguage:(NSLanguage *) language;
- (NSGlyph)_defaultGlyphForChar:(unichar)uu;
@end

@class NSCGSFont;


static CFCharacterSetRef nonBaseChars = NULL;


@interface IFTextRenderer (IFPrivate)
- (WidthMap *)extendGlyphToWidthMapToInclude:(ATSGlyphRef)glyphID;
- (ATSGlyphRef)extendCharacterToGlyphMapToInclude:(UniChar) c;
@end


static void freeWidthMap (WidthMap *map)
{
    if (!map)
	return;
    freeWidthMap (map->next);
    free (map->widths);
    free (map);
}


static void freeGlyphMap (GlyphMap *map)
{
    if (!map)
	return;
    freeGlyphMap (map->next);
    free (map->glyphs);
    free (map);
}


static inline ATSGlyphRef glyphForCharacter (GlyphMap *map, UniChar c)
{
    if (map == 0)
        return nonGlyphID;
        
    if (c >= map->startRange && c <= map->endRange)
        return ((ATSGlyphRef *)map->glyphs)[c-map->startRange];
        
    return glyphForCharacter (map->next, c);
}
 
 
static void setGlyphForCharacter (GlyphMap *map, ATSGlyphRef glyph, UniChar c)
{
    if (map == 0)
        return;
        
    if (c >= map->startRange && c <= map->endRange)
        ((ATSGlyphRef *)map->glyphs)[c-map->startRange] = glyph;
    else    
        setGlyphForCharacter (map->next, glyph, c);
}


#ifdef _TIMING        
static double totalCGGetAdvancesTime = 0;
#endif

static inline IFGlyphWidth widthForGlyph (IFTextRenderer *renderer, WidthMap *map, ATSGlyphRef glyph)
{
    IFGlyphWidth width;
    bool errorResult;
    
    if (map == 0){
        map = [renderer extendGlyphToWidthMapToInclude: glyph];
        return widthForGlyph (renderer, map, glyph);
    }
        
    if (glyph >= map->startRange && glyph <= map->endRange){
        width = ((IFGlyphWidth *)map->widths)[glyph-map->startRange];
        if (width == UNINITIALIZED_GLYPH_WIDTH){

#ifdef _TIMING        
            double startTime = CFAbsoluteTimeGetCurrent();
#endif
            errorResult = CGFontGetGlyphScaledAdvances ([renderer->font _backingCGSFont], &glyph, 1, &map->widths[glyph-map->startRange], [renderer->font pointSize]);
            if (errorResult == 0)
                [NSException raise:NSInternalInconsistencyException format:@"Optimization assumption violation:  unable to cache glyph widths - for %@ %f",  [renderer->font displayName], [renderer->font pointSize]];
    
#ifdef _TIMING        
            double thisTime = CFAbsoluteTimeGetCurrent() - startTime;
            totalCGGetAdvancesTime += thisTime;
#endif
            return ((IFGlyphWidth *)map->widths)[glyph-map->startRange];
        }
        return width;
    }

    return widthForGlyph (renderer, map->next, glyph);
}


static inline  IFGlyphWidth widthForCharacter (IFTextRenderer *renderer, UniChar c)
{
    return widthForGlyph (renderer, renderer->glyphToWidthMap, glyphForCharacter(renderer->characterToGlyphMap, c));
}


static void FillStyleWithAttributes(ATSUStyle style, NSFont *theFont)
{
    if (theFont) {
        ATSUFontID fontId = (ATSUFontID)[theFont _atsFontID];
        ATSUAttributeTag tag = kATSUFontTag;
        ByteCount size = sizeof(ATSUFontID);
        ATSUFontID *valueArray[1] = {&fontId};

        if (fontId) {
            if (ATSUSetAttributes(style, 1, &tag, &size, (void **)valueArray) != noErr)
                [NSException raise:NSInternalInconsistencyException format:@"Failed to set font (%@) ATSUStyle 0x%X", theFont, style];
        }
    }
}


static unsigned int findLengthOfCharacterCluster(const UniChar *characters, unsigned int length)
{
    unsigned int clusterLength = 1, k;
    UniChar nextC;

    if (length == 0)
        return 0;
            
    if (length == 1)
        return 1;
    
    if (IsNonBaseChar(characters[0]))
        return 1;
                
    nextC = characters[1];
    if (IsNonBaseChar(nextC)){
        // Find all the non base characters after the current character.
        for (k = 2; k < length; k++){
            if (!IsNonBaseChar(characters[k]))
                break;
        }
        clusterLength = k;
    }
    else
        clusterLength = 1;

    return clusterLength;
}


@implementation IFTextRenderer

+ (void)initialize
{
    nonBaseChars = CFCharacterSetGetPredefined(kCFCharacterSetNonBase);
}


- (NSFont *)substituteFontForString: (NSString *)string
{
    NSFont *substituteFont;

    substituteFont = [NSFont findFontLike:font forString:string withRange:NSMakeRange (0,[string length]) inLanguage:[NSLanguage defaultLanguage]];

    if ([substituteFont isEqual: font])
        return nil;
        
    return substituteFont;
}


- (NSFont *)substituteFontForCharacters: (const unichar *)characters length: (int)numCharacters
{
    NSFont *substituteFont;
    NSString *string = [[NSString alloc] initWithCharactersNoCopy:(unichar *)characters length: numCharacters freeWhenDone: NO];

    substituteFont = [self substituteFontForString: string];

    [string release];
    
    return substituteFont;
}


/* Convert non-breaking spaces into spaces. */
- (void)convertCharacters: (const UniChar *)characters length: (unsigned)numCharacters toGlyphs: (ATSGlyphVector *)glyphs
{
    unsigned i;
    UniChar localBuffer[LOCAL_BUFFER_SIZE];
    UniChar *buffer = localBuffer;
    OSStatus status;
    
    for (i = 0; i < numCharacters; i++) {
        if (characters[i] == NON_BREAKING_SPACE) {
            break;
        }
    }
    
    if (i < numCharacters) {
        if (numCharacters > LOCAL_BUFFER_SIZE) {
            buffer = (UniChar *)malloc(sizeof(UniChar) * numCharacters);
        }
        
        for (i = 0; i < numCharacters; i++) {
            if (characters[i] == NON_BREAKING_SPACE) {
                buffer[i] = SPACE;
            } else {
                buffer[i] = characters[i];
            }
        }
        
        characters = buffer;
    }
    
    status = ATSUConvertCharToGlyphs(styleGroup, characters, 0, numCharacters, 0, glyphs);
    
    if (buffer != localBuffer) {
        free(buffer);
    }
}


- initWithFont:(NSFont *)f
{
    if ([f glyphPacking] != NSNativeShortGlyphPacking &&
        [f glyphPacking] != NSTwoByteGlyphPacking)
        [NSException raise:NSInternalInconsistencyException format:@"%@: Don't know how to pack glyphs for font %@ %f", self, [f displayName], [f pointSize]];
        
    [super init];
    
    font = [f retain];
    ascent = -1;
    descent = -1;
    lineSpacing = -1;
    
    OSStatus errCode;
    ATSUStyle style;
    
    if ((errCode = ATSUCreateStyle(&style)) != noErr)
        [NSException raise:NSInternalInconsistencyException format:@"%@: Failed to alloc ATSUStyle %d", self, errCode];

    FillStyleWithAttributes(style, font);

    if ((errCode = ATSUGetStyleGroup(style, &styleGroup)) != noErr) {
        [NSException raise:NSInternalInconsistencyException format:@"%@: Failed to create attribute group from ATSUStyle 0x%X %d", self, style, errCode];
    }
    
    ATSUDisposeStyle(style);

    spaceGlyph = nonGlyphID;
    
    return self;
}


- (void)dealloc
{
    [font release];

    if (styleGroup)
        ATSUDisposeStyleGroup(styleGroup);

    freeWidthMap (glyphToWidthMap);
    freeGlyphMap (characterToGlyphMap);
    
    [super dealloc];
}


- (int)widthForString:(NSString *)string
{
    UniChar localCharacterBuffer[LOCAL_BUFFER_SIZE];
    UniChar *characterBuffer = localCharacterBuffer;
    const UniChar *usedCharacterBuffer = CFStringGetCharactersPtr((CFStringRef)string);
    unsigned int length;
    int width;

    // Get the characters from the string into a buffer.
    length = [string length];
    if (!usedCharacterBuffer) {
        if (length > LOCAL_BUFFER_SIZE)
            characterBuffer = (UniChar *)malloc(length * sizeof(UniChar));
        [string getCharacters:characterBuffer];
        usedCharacterBuffer = characterBuffer;
    }

    width = [self widthForCharacters:usedCharacterBuffer length:length];
    
    if (characterBuffer != localCharacterBuffer)
        free(characterBuffer);

    return width;
}


- (int)ascent
{
    if (ascent < 0)  {
        ascent = ROUND_TO_INT([font ascender]);
    }
    return ascent;
}


- (int)descent
{
    if (descent < 0)  {
        descent = ROUND_TO_INT(-[font descender]);
    }
    return descent;
}


- (int)lineSpacing
{
    if (lineSpacing < 0) {
        lineSpacing = ROUND_TO_INT([font defaultLineHeightForFont]);
    }
    return lineSpacing;
}


- (void) slowPackGlyphsForCharacters:(const UniChar *)characters numCharacters: (unsigned int)numCharacters glyphBuffer:(CGGlyph **)glyphBuffer numGlyphs:(unsigned int *)numGlyphs
{
    ATSGlyphVector glyphVector;
    unsigned int j;
    CGGlyph *glyphBufPtr;
    ATSLayoutRecord *glyphRecord;

    ATSInitializeGlyphVector(numCharacters, 0, &glyphVector);
    [self convertCharacters: characters length: numCharacters toGlyphs: &glyphVector];

    *numGlyphs = glyphVector.numGlyphs;
    *glyphBuffer = glyphBufPtr = (CGGlyph *)malloc (*numGlyphs * sizeof(CGGlyph));
    glyphRecord = (ATSLayoutRecord *)glyphVector.firstRecord;
    for (j = 0; j < *numGlyphs; j++){
        *glyphBufPtr++ = glyphRecord->glyphID;
        glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + glyphVector.recordSize);
    }
    
    ATSClearGlyphVector(&glyphVector);
}


- (void)drawString:(NSString *)string atPoint:(NSPoint)point withColor:(NSColor *)color
{
    UniChar localCharacterBuffer[LOCAL_BUFFER_SIZE];
    UniChar *characterBuffer = localCharacterBuffer;
    const UniChar *usedCharacterBuffer = CFStringGetCharactersPtr((CFStringRef)string);
    unsigned int length;

    // Get the characters from the string into a buffer.
    length = [string length];
    if (!usedCharacterBuffer) {
        if (length > LOCAL_BUFFER_SIZE)
            characterBuffer = (UniChar *)malloc(length * sizeof(UniChar));
        [string getCharacters:characterBuffer];
        usedCharacterBuffer = characterBuffer;
    }

    [self drawCharacters: usedCharacterBuffer length: length atPoint: point withColor: color];
    
    if (characterBuffer != localCharacterBuffer)
        free(characterBuffer);
}


- (NSPoint)drawGlyphs: (CGGlyph *)glyphs numGlyphs: (unsigned int)numGlyphs atPoint: (NSPoint)point withColor: (NSColor *)color
{
    unsigned int i;
    CGSize *advances, localAdvanceBuffer[LOCAL_BUFFER_SIZE];
    CGContextRef cgContext;
    NSPoint advancePoint = point;

    if (numGlyphs == 0)
        return point;
        
    // Determine if we can use the local stack buffer, otherwise allocate.
    if (numGlyphs > LOCAL_BUFFER_SIZE) {
        advances = (CGSize *)malloc(numGlyphs * sizeof(CGSize));
    } else {
        advances = localAdvanceBuffer;
    }

    for (i = 0; i < numGlyphs; i++) {
        advances[i].width = widthForGlyph(self, glyphToWidthMap, glyphs[i]);
        if (glyphs[i] == spaceGlyph)
            advances[i].width = ROUND_TO_INT(advances[i].width);
        advances[i].height = 0;
        advancePoint.x += advances[i].width;
    }

    // Setup the color and font.
    [color set];
    [font set];
    
    // Finally, draw the glyphs.
    cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

    CGContextSetTextPosition (cgContext, point.x, point.y);
    CGContextShowGlyphsWithAdvances (cgContext, glyphs, advances, numGlyphs);
    //CGContextShowGlyphsWithDeviceAdvances (cgContext, glyphs, advances, numGlyphs);

    if (advances != localAdvanceBuffer) {
        free(advances);
    }
    return advancePoint;
}

// Useful page for testing http://home.att.net/~jameskass

- (NSPoint)slowDrawCharacters:(const UniChar *)characters length: (unsigned int)length atPoint:(NSPoint)point withColor:(NSColor *)color
{
    unsigned int charPos = 0, lastDrawnGlyph = 0;
    unsigned int clusterLength, i, numGlyphs, fragmentLength;
    NSFont *substituteFont;
    CGGlyph *glyphs;
    ATSGlyphRef glyphID;
    
    [self slowPackGlyphsForCharacters: characters numCharacters: length glyphBuffer: &glyphs numGlyphs: &numGlyphs];

    // FIXME:  This assumes that we'll always get one glyph per character cluster.
    for (i = 0; i < numGlyphs; i++){
        glyphID = glyphs[i];

        clusterLength = findLengthOfCharacterCluster (&characters[charPos], length - charPos);
        
        if (glyphID == 0){
            // Draw everthing up to this point.
            fragmentLength = i - lastDrawnGlyph;
            if (fragmentLength > 0)
                point = [self drawGlyphs: &glyphs[lastDrawnGlyph] numGlyphs: fragmentLength atPoint: point withColor: color];
            
            // Draw the character in the alternate font.
            substituteFont = [self substituteFontForCharacters: &characters[charPos] length: clusterLength];
            if (substituteFont)
                point = [[[IFTextRendererFactory sharedFactory] rendererWithFont: substituteFont] slowDrawCharacters: &characters[charPos] length: clusterLength atPoint: point withColor: color];
            else
                point = [self drawGlyphs: &glyphs[i] numGlyphs: 1 atPoint: point withColor: color];
                
            lastDrawnGlyph = i+1;
        }

        charPos += clusterLength;
    }

    fragmentLength = numGlyphs - lastDrawnGlyph;
    if (fragmentLength > 0)
        point = [self drawGlyphs: &glyphs[lastDrawnGlyph] numGlyphs: fragmentLength atPoint: point withColor: color];
        
    if (glyphs)
        free(glyphs);
    
    return point;
}


typedef enum {
	_IFNonBaseCharacter,
	_IFMissingGlyph,
	_IFDrawSucceeded,
} _IFFailedDrawReason;

- (_IFFailedDrawReason)_drawCharacters:(const UniChar *)characters length: (unsigned int)length atPoint:(NSPoint)point withColor:(NSColor *)color
{
    uint i;
    CGGlyph *glyphs, localGlyphBuffer[LOCAL_BUFFER_SIZE];
    ATSGlyphRef glyphID;
    _IFFailedDrawReason result = _IFDrawSucceeded;
    
    // FIXME: Deal with other styles of glyph packing.
    if ([font glyphPacking] != NSNativeShortGlyphPacking &&
        [font glyphPacking] != NSTwoByteGlyphPacking)
        [NSException raise:NSInternalInconsistencyException format:@"%@: Don't know how to deal with font %@", self, [font displayName]];
    
    // Determine if we can use the local stack buffer, otherwise allocate.
    if (length > LOCAL_BUFFER_SIZE) {
        glyphs = (CGGlyph *)malloc(length * sizeof(CGGlyph));
    } else {
        glyphs = localGlyphBuffer;
    }
    
    // Pack the glyph buffer and ensure that we have glyphs for all the
    // characters.  If we're missing a glyph or have a non-base character
    // stop drawing and return a failure code.
    for (i = 0; i < length; i++) {
        UniChar c = characters[i];

        // Icky.  Deal w/ non breaking spaces.
        if (c == NON_BREAKING_SPACE)
            c = SPACE;

        // Is the a non-base character?
        if (IsNonBaseChar(c)) {
            result = _IFNonBaseCharacter;
            goto cleanup;
        }
        
        glyphID = glyphForCharacter (characterToGlyphMap, c);
        if (glyphID == nonGlyphID) {
            glyphID = [self extendCharacterToGlyphMapToInclude: c];
        }

        // Does this font not contain a glyph for the character?
        if (glyphID == 0){
            result = _IFMissingGlyph;
            goto cleanup;
        }
            
        glyphs[i] = glyphID;
    }

    [self drawGlyphs: glyphs numGlyphs: length atPoint: point withColor: color];

cleanup:
    if (glyphs != localGlyphBuffer) {
        free(glyphs);
    }
    return result;
}


- (void)drawCharacters:(const UniChar *)characters length: (unsigned int)length atPoint:(NSPoint)point withColor:(NSColor *)color
{
    NSFont *substituteFont;
    _IFFailedDrawReason reason = [self _drawCharacters: characters length: length atPoint: point withColor: color];
    
    // ASSUMPTION:  We normally fail because we're trying to render characters
    // that doesn't have glyphs in the specified fonts.  Instead of immediately using the slow path
    // we look for a font corresponding to the first character cluster in the string
    // and try rendering the entire string with that font.  If that fails we fall back 
    // to the safe slow drawing.
    if (reason == _IFMissingGlyph) {
        unsigned int clusterLength;
        
        clusterLength = findLengthOfCharacterCluster(characters, length);
        substituteFont = [self substituteFontForCharacters: characters length: clusterLength];
        if (substituteFont)
            reason = [[[IFTextRendererFactory sharedFactory] rendererWithFont: substituteFont] _drawCharacters: characters length: length atPoint: point withColor: color];
         
         if (!substituteFont || reason != _IFDrawSucceeded)
            [self slowDrawCharacters: characters length: length atPoint: point withColor: color];
    }
    else if (reason == _IFNonBaseCharacter) {
        [self slowDrawCharacters: characters length: length atPoint: point withColor: color];
    }
}


- (void)drawUnderlineForString:(NSString *)string atPoint:(NSPoint)point withColor:(NSColor *)color
{
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    int width = [self widthForString: string];
    CGContextRef cgContext;
    float lineWidth;
    
    // This will draw the text from the top of the bounding box down.
    // Qt expects to draw from the baseline.
    // Remember that descender is negative.
    point.y -= [self lineSpacing] - [self descent];
    
    BOOL flag = [graphicsContext shouldAntialias];

    [graphicsContext setShouldAntialias: NO];

    [color set];

    cgContext = (CGContextRef)[graphicsContext graphicsPort];
    lineWidth = 0.0;
    if ([graphicsContext isDrawingToScreen] && lineWidth == 0.0) {
        CGSize size = CGSizeApplyAffineTransform(CGSizeMake(1.0, 1.0), CGAffineTransformInvert(CGContextGetCTM(cgContext)));
        lineWidth = size.width;
    }
    CGContextSetLineWidth(cgContext, lineWidth);
    CGContextMoveToPoint(cgContext, point.x, point.y + [font defaultLineHeightForFont] + 1.5 - [self descent]);
    CGContextAddLineToPoint(cgContext, point.x + width, point.y + [font defaultLineHeightForFont] + 1.5 - [self descent]);
    CGContextStrokePath(cgContext);

    [graphicsContext setShouldAntialias: flag];
}


- (float)slowFloatWidthForCharacters: (const UniChar *)characters length: (unsigned)length applyRounding: (BOOL)applyRounding
{
    float totalWidth = 0;
    unsigned int i, numGlyphs;
    ATSGlyphVector glyphVector;
    IFGlyphWidth glyphWidth;
    ATSLayoutRecord *glyphRecord;
    ATSGlyphRef glyphID;
    
    
    ATSInitializeGlyphVector(length, 0, &glyphVector);
    [self convertCharacters: characters length: length toGlyphs: &glyphVector];
    numGlyphs = glyphVector.numGlyphs;
    glyphRecord = (ATSLayoutRecord *)glyphVector.firstRecord;
    for (i = 0; i < numGlyphs; i++){
        glyphID = glyphRecord->glyphID;
        glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + glyphVector.recordSize);
        glyphWidth = widthForGlyph(self, glyphToWidthMap, glyphID);
        if (glyphID == spaceGlyph && applyRounding)
            glyphWidth = ROUND_TO_INT(glyphWidth);
        totalWidth += glyphWidth;
    }
    ATSClearGlyphVector(&glyphVector);
    
    return totalWidth;
}


- (float)floatWidthForCharacters:(const UniChar *)characters length:(unsigned)length applyRounding: (BOOL)applyRounding
{
    float totalWidth = 0;
    unsigned int i, clusterLength;
    NSFont *substituteFont;
    ATSGlyphRef glyphID;
    

    for (i = 0; i < length; i++) {
        UniChar c = characters[i];
        
        if (c == NON_BREAKING_SPACE) {
            c = SPACE;
        }
        else if (IsNonBaseChar(c)){
            return [self slowFloatWidthForCharacters: characters length: length applyRounding: applyRounding];
        }
        
        glyphID = glyphForCharacter(characterToGlyphMap, c);
        if (glyphID == nonGlyphID) {
            glyphID = [self extendCharacterToGlyphMapToInclude: c];
        }
        
        // Try to find a substitute font if this font didn't have a glyph for a character in the
        // string.  If one isn't found we end up drawing and measuring a box.
        if (glyphID == 0) {
            clusterLength = findLengthOfCharacterCluster (&characters[i], length - i);
            substituteFont = [self substituteFontForCharacters: &characters[i] length: clusterLength];
            if (substituteFont) {
                //WEBKITDEBUGLEVEL (WEBKIT_LOG_FONTCACHE, "substituting %s for %s, missing 0x%04x\n", DEBUG_OBJECT(substituteFont), DEBUG_OBJECT([font displayName]), c);
                totalWidth += [[[IFTextRendererFactory sharedFactory] rendererWithFont: substituteFont] floatWidthForCharacters: &characters[i] length: clusterLength applyRounding: YES];
            }
        }
        
        if (glyphID > 0 || ((glyphID == 0) && substituteFont == nil)) {
            if (glyphID == spaceGlyph && applyRounding)
                totalWidth += ROUND_TO_INT(widthForGlyph(self, glyphToWidthMap, glyphID));
            else
                totalWidth += widthForGlyph(self, glyphToWidthMap, glyphID);
        }                    
    }

    return totalWidth;
}


- (int)widthForCharacters:(const UniChar *)characters length:(unsigned)length
{
    return ROUND_TO_INT([self floatWidthForCharacters:characters length:length applyRounding:YES]);
}


// This method is rarely (never?) called.  It's slow.
- (void)drawString:(NSString *)string inRect:(NSRect)rect withColor:(NSColor *)color paragraphStyle:(NSParagraphStyle *)style
{
    [string drawInRect:rect withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:
        font, NSFontAttributeName,
        color, NSForegroundColorAttributeName,
        style, NSParagraphStyleAttributeName,
        nil]];
}


- (ATSGlyphRef)extendCharacterToGlyphMapToInclude:(UniChar) c
{
    GlyphMap *map = (GlyphMap *)calloc (1, sizeof(GlyphMap));
    ATSLayoutRecord *glyphRecord;
    ATSGlyphVector glyphVector;
    UniChar end, start;
    unsigned int blockSize;
    ATSGlyphRef glyphID;
    
    if (characterToGlyphMap == 0)
        blockSize = INITIAL_BLOCK_SIZE;
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (c / blockSize) * blockSize;
    end = start + (blockSize - 1);
        
    WEBKITDEBUGLEVEL (WEBKIT_LOG_FONTCACHE, "%s (0x%04x) adding glyphs for 0x%04x to 0x%04x\n", DEBUG_OBJECT(font), c, start, end);

    map->startRange = start;
    map->endRange = end;
    
    unsigned int i, count = end - start + 1;
    short unsigned int buffer[INCREMENTAL_BLOCK_SIZE+2];
    
    for (i = 0; i < count; i++){
        if (IsNonBaseChar(i+start))
            buffer[i] = 0;
        else
            buffer[i] = i+start;
    }

    ATSInitializeGlyphVector(count, 0, &glyphVector);
    [self convertCharacters: &buffer[0] length: count toGlyphs: &glyphVector];
    if (glyphVector.numGlyphs != count)
        [NSException raise:NSInternalInconsistencyException format:@"Optimization assumption violation:  count and glyphID count not equal - for %@ %f", self, [font displayName], [font pointSize]];
            
    map->glyphs = (ATSGlyphRef *)malloc (count * sizeof(ATSGlyphRef));
    glyphRecord = (ATSLayoutRecord *)glyphVector.firstRecord;
    for (i = 0; i < count; i++) {
        map->glyphs[i] = glyphRecord->glyphID;
        glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + glyphVector.recordSize);
    }
    ATSClearGlyphVector(&glyphVector);
    
    if (characterToGlyphMap == 0)
        characterToGlyphMap = map;
    else {
        GlyphMap *lastMap = characterToGlyphMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    if (spaceGlyph == nonGlyphID)
        spaceGlyph = glyphForCharacter (characterToGlyphMap, SPACE);

    glyphID = map->glyphs[c - start];
    
    // Special case for characters 007F-00A0.
    if (glyphID == 0 && c >= 0x7F && c <= 0xA0){
        glyphID = [font _defaultGlyphForChar: c];
        map->glyphs[c - start] = glyphID;
    }

    return glyphID;
}


- (WidthMap *)extendGlyphToWidthMapToInclude:(ATSGlyphRef)glyphID
{
    WidthMap *map = (WidthMap *)calloc (1, sizeof(WidthMap));
    unsigned int end;
    ATSGlyphRef start;
    unsigned int blockSize;
    unsigned int i, count;
    
    if (glyphToWidthMap == 0){
        if ([font numberOfGlyphs] < INITIAL_BLOCK_SIZE)
            blockSize = [font numberOfGlyphs];
         else
            blockSize = INITIAL_BLOCK_SIZE;
    }
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (glyphID / blockSize) * blockSize;
    end = ((unsigned int)start) + blockSize; 
    if (end > 0xffff)
        end = 0xffff;

    WEBKITDEBUGLEVEL (WEBKIT_LOG_FONTCACHE, "%s (0x%04x) adding widths for range 0x%04x to 0x%04x\n", DEBUG_OBJECT(font), glyphID, start, end);

    map->startRange = start;
    map->endRange = end;
    count = end - start + 1;

    map->widths = (IFGlyphWidth *)malloc (count * sizeof(IFGlyphWidth));

    for (i = 0; i < count; i++){
        map->widths[i] = UNINITIALIZED_GLYPH_WIDTH;
    }

    if (glyphToWidthMap == 0)
        glyphToWidthMap = map;
    else {
        WidthMap *lastMap = glyphToWidthMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

#ifdef _TIMING
    WEBKITDEBUGLEVEL (WEBKIT_LOG_FONTCACHE, "%s total time to advances lookup %f seconds\n", DEBUG_OBJECT(font), totalCGGetAdvancesTime);
#endif
    return map;
}

@end
