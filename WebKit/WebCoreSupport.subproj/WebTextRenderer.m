//
//  IFTextRenderer.m
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFTextRenderer.h"

#import <Cocoa/Cocoa.h>

#import <ApplicationServices/ApplicationServices.h>
#import <CoreGraphics/CoreGraphicsPrivate.h>

#import <WebKit/IFTextRendererFactory.h>
#import <WebKit/WebKitDebug.h>

#define NON_BREAKING_SPACE 0xA0
#define SPACE 0x20

#define ROUND_TO_INT(x) (unsigned int)((x)+.5)

#define LOCAL_GLYPH_BUFFER_SIZE 1024

#define INITIAL_GLYPH_CACHE_MAX 512
#define INCREMENTAL_GLYPH_CACHE_BLOCK 1024

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
@end

@class NSCGSFont;


static CFCharacterSetRef nonBaseChars = NULL;


static void freeWidthMap (WidthMap *map)
{
    if (map->next)
        freeWidthMap (map->next);
    free (map->widths);
    free (map);
}


static void freeGlyphMap (GlyphMap *map)
{
    if (map->next)
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

#if 1
//#warning Aki 7/20/2000 This code should be disabled once the brain dead bug 2499383 is fixed
            {
                ATSUFontFeatureType types[8] = {kDiacriticsType, kTypographicExtrasType, kFractionsType, kSmartSwashType, kSmartSwashType, kSmartSwashType, kSmartSwashType, kSmartSwashType};
                ATSUFontFeatureSelector selectors[8] = {kDecomposeDiacriticsSelector, kSmartQuotesOffSelector, kNoFractionsSelector, kWordInitialSwashesOffSelector, kWordFinalSwashesOffSelector, kLineInitialSwashesOffSelector, kLineFinalSwashesOffSelector, kNonFinalSwashesOffSelector};
                ATSUSetFontFeatures(style, 8, types, selectors);
            }
#endif
        }
    }
}


static bool hasMissingGlyphs(ATSGlyphVector *glyphs)
{
    unsigned int i, numGlyphs = glyphs->numGlyphs;
    ATSLayoutRecord *glyphRecords;

    glyphRecords = (ATSLayoutRecord *)glyphs->firstRecord;
    for (i = 0; i < numGlyphs; i++){
        if (glyphRecords[i].glyphID == 0){
            return YES;
        }
    }
    return NO;
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
- (void)convertCharacters: (const unichar *)characters length: (int)numCharacters glyphs: (ATSGlyphVector *)glyphs
{
    int i;
    UniChar localBuffer[LOCAL_GLYPH_BUFFER_SIZE];
    UniChar *buffer = localBuffer;
    OSStatus status;
    
    for (i = 0; i < numCharacters; i++) {
        if (characters[i] == NON_BREAKING_SPACE) {
            break;
        }
    }
    
    if (i < numCharacters) {
        if (numCharacters > LOCAL_GLYPH_BUFFER_SIZE) {
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
    
    if (buffer != localBuffer) {
        free(buffer);
    }
    
    status = ATSUConvertCharToGlyphs(styleGroup, characters, 0, numCharacters, 0, glyphs);
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
    UniChar localBuffer[LOCAL_GLYPH_BUFFER_SIZE];
    const UniChar *_internalBuffer = CFStringGetCharactersPtr ((CFStringRef)string);
    const UniChar *internalBuffer;

    if (!_internalBuffer) {
        // FIXME: Handle case where string is larger than LOCAL_GLYPH_BUFFER_SIZE?
        CFStringGetCharacters((CFStringRef)string, CFRangeMake(0, CFStringGetLength((CFStringRef)string)), &localBuffer[0]);
        internalBuffer = &localBuffer[0];
    }
    else
        internalBuffer = _internalBuffer;

    return [self widthForCharacters:internalBuffer length:[string length]];
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


- (bool) slowPackGlyphsForCharacters:(const UniChar *)characters numCharacters: (unsigned int)numCharacters glyphBuffer:(CGGlyph **)glyphBuffer numGlyphs:(unsigned int *)numGlyphs
{
    ATSGlyphVector _glyphVector;
    unsigned int j;
    CGGlyph *glyphBufPtr;
    ATSLayoutRecord *glyphRecords;

    ATSInitializeGlyphVector(numCharacters, 0, &_glyphVector);
    [self convertCharacters: characters length: numCharacters glyphs: &_glyphVector];
    if (hasMissingGlyphs (&_glyphVector))
        return NO;

    *numGlyphs = _glyphVector.numGlyphs;
    *glyphBuffer = glyphBufPtr = (CGGlyph *)malloc (*numGlyphs * sizeof(CGGlyph));
    glyphRecords = (ATSLayoutRecord *)_glyphVector.firstRecord;
    for (j = 0; j < *numGlyphs; j++){
        *glyphBufPtr++ = glyphRecords->glyphID;
        glyphRecords++;
    }
    
    ATSClearGlyphVector(&_glyphVector);

    return YES;
}


- (void)drawString:(NSString *)string atPoint:(NSPoint)point withColor:(NSColor *)color
{
    UniChar localCharacterBuffer[LOCAL_GLYPH_BUFFER_SIZE];
    UniChar *characterBuffer = 0, *_usedCharacterBuffer;
    const UniChar *usedCharacterBuffer = CFStringGetCharactersPtr ((CFStringRef)string);
    unsigned int length;

    // Get the unichar buffer from the string.
    length = [string length];
    if (!usedCharacterBuffer){
        if (length > LOCAL_GLYPH_BUFFER_SIZE)
            _usedCharacterBuffer = characterBuffer = (UniChar *)malloc(length * sizeof(UniChar));
        else
            _usedCharacterBuffer = &localCharacterBuffer[0];
        CFStringGetCharacters((CFStringRef)string, CFRangeMake(0, length), _usedCharacterBuffer);
        usedCharacterBuffer = _usedCharacterBuffer;
    }

    [self drawCharacters: usedCharacterBuffer length: length atPoint: point withColor: color];
    
    if (characterBuffer)
        free (characterBuffer);
}


- (void)drawCharacters:(const UniChar *)characters length: (unsigned int)length atPoint:(NSPoint)point withColor:(NSColor *)color
{
    unsigned int i;
    CGGlyph localGlyphBuf[LOCAL_GLYPH_BUFFER_SIZE];
#ifndef DRAW_WITHOUT_ADVANCES
    CGSize *advancePtr, advances[LOCAL_GLYPH_BUFFER_SIZE];
#endif
    CGGlyph *usedGlyphBuffer, *glyphBufPtr, *glyphBuffer = 0, *slowGlyphBuffer = 0;
    ATSGlyphRef glyphID;
    CGContextRef cgContext;
    int numGlyphs;
    NSFont *substituteFont;
    
    // FIXME:  Deal with other font encodings.
    if ([font glyphPacking] != NSNativeShortGlyphPacking &&
        [font glyphPacking] != NSTwoByteGlyphPacking)
        [NSException raise:NSInternalInconsistencyException format:@"%@: Don't know how to deal with font %@", self, [font displayName]];
    
    // Determine if we can use the local stack buffer, otherwise allocate.
    if (length > LOCAL_GLYPH_BUFFER_SIZE)
        usedGlyphBuffer = glyphBufPtr = glyphBuffer = (CGGlyph *)malloc (length * sizeof(CGGlyph));
    else
        usedGlyphBuffer = glyphBufPtr = &localGlyphBuf[0];

#ifndef DRAW_WITHOUT_ADVANCES
    advancePtr = &advances[0];
#endif
    
    // Set the number of glyphs to the character length.  This may during the character
    // scan if we find any non base characters.
    numGlyphs = length;
    
    // Pack the glyph buffer and ensure that we have glyphs for all the
    // characters.  If we're missing a glyph look for an alternate font.
    for (i = 0; i < length; i++){
        UniChar c = characters[i];

        // Icky.  Deal w/ non breaking spaces.
        if (c == NON_BREAKING_SPACE)
            c = SPACE;

        // Is the a combining character?  If so we have to do expensive
        // glyph lookup.
        if (IsNonBaseChar (c)){
            bool hasGlyphs;
            hasGlyphs = [self slowPackGlyphsForCharacters: characters numCharacters: length glyphBuffer: &slowGlyphBuffer numGlyphs: &numGlyphs];
            if (!hasGlyphs){
                substituteFont = [self substituteFontForCharacters: characters length: length];
                if (substituteFont){
                    [(IFTextRenderer *)[(IFTextRendererFactory *)[IFTextRendererFactory sharedFactory] rendererWithFont: substituteFont] drawCharacters: characters length: length atPoint: point withColor: color];
                    goto cleanup;
                }
            }
            usedGlyphBuffer = slowGlyphBuffer;
            break;
        }
    
        glyphID = glyphForCharacter (characterToGlyphMap, characters[i]);
        
        // glyphID == 0 means that the font doesn't contain a glyph for the character.
        if (glyphID == 0){
            substituteFont = [self substituteFontForCharacters: characters length: length];
            if (substituteFont){
                [(IFTextRenderer *)[(IFTextRendererFactory *)[IFTextRendererFactory sharedFactory] rendererWithFont: substituteFont] drawCharacters: characters length: length atPoint: point withColor: color];
                goto cleanup;
            }
        }

        *glyphBufPtr++ = glyphID;
        
#ifdef DRAW_WITHOUT_ADVANCES
#else
        advancePtr->width = widthForGlyph(self, glyphToWidthMap, glyphID);
        advancePtr->height = 0;
        advancePtr++;
#endif
    }

    // This will draw the text from the top of the bounding box down.
    // Qt expects to draw from the baseline.
    // Remember that descender is negative.
#ifdef DRAW_WITHOUT_ADVANCES
    point.y -= [self lineSpacing] - [self descent];
#else
    point.y += [self descent] - 1;
#endif

    // Setup the color and font.
    [color set];
    [font set];
    
    // Finally, draw the characters.
    cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

#ifdef DRAW_WITHOUT_ADVANCES
    CGContextSetCharacterSpacing(cgContext, 0.0);
    CGContextShowGlyphsAtPoint (cgContext, point.x, point.y + [font defaultLineHeightForFont] - 1, (CGGlyph *)usedGlyphBuffer, numGlyphs);
#else      
    CGContextSetTextPosition (cgContext, point.x, point.y);
    //CGContextShowGlyphsWithAdvances (cgContext, (CGGlyph *)usedGlyphBuffer, advances, numGlyphs);
    CGContextShowGlyphsWithDeviceAdvances (cgContext, (CGGlyph *)usedGlyphBuffer, advances, numGlyphs);
#endif
    
cleanup:
    if (glyphBuffer)
        free (glyphBuffer);
    if (slowGlyphBuffer)
        free (slowGlyphBuffer);
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
    CGContextMoveToPoint(cgContext, point.x, point.y + [font defaultLineHeightForFont] + 0.5);
    CGContextAddLineToPoint(cgContext, point.x + width, point.y + [font defaultLineHeightForFont] + 0.5);
    CGContextStrokePath(cgContext);

    [graphicsContext setShouldAntialias: flag];
}


- (int)slowWidthForCharacters: (const UniChar *)characters length: (unsigned)length
{
    float totalWidth = 0;
    unsigned int i, numGlyphs;
    ATSGlyphVector _glyphVector;
    IFGlyphWidth glyphWidth;
    ATSLayoutRecord *glyphRecords;
    ATSGlyphRef glyphID;
    
    
    ATSInitializeGlyphVector(length, 0, &_glyphVector);
    [self convertCharacters: (const unichar *)characters length: (int)length glyphs: (ATSGlyphVector *)&_glyphVector];
    numGlyphs = _glyphVector.numGlyphs;
    glyphRecords = (ATSLayoutRecord *)_glyphVector.firstRecord;
    for (i = 0; i < numGlyphs; i++){
        glyphID = glyphRecords[i].glyphID;
        glyphWidth = widthForGlyph(self, glyphToWidthMap, glyphID);
        totalWidth += glyphWidth;
    }
    ATSClearGlyphVector(&_glyphVector);
    
    return totalWidth;
}


- (int)widthForCharacters:(const UniChar *)characters length:(unsigned)length
{
    float totalWidth = 0;
    unsigned int i;
    NSFont *substituteFont;
    ATSGlyphRef glyphID;
    

    for (i = 0; i < length; i++){
        UniChar c = characters[i];
        
        if (c == NON_BREAKING_SPACE) {
        	c = SPACE;
        }
        else if (IsNonBaseChar(c)){
            return [self slowWidthForCharacters: characters length: length];
        }
        
        glyphID = glyphForCharacter(characterToGlyphMap, c);
        if (glyphID == nonGlyphID){
            glyphID = [self extendCharacterToGlyphMapToInclude: c];
        }
        
        // Try to find a substitute font if this font didn't have a glyph for a character in the
        // string.  If one isn't found we end up drawing and measuring a box.
        if (glyphID == 0){
            substituteFont = [self substituteFontForCharacters: characters length: length];
            if (substituteFont){
                WEBKITDEBUGLEVEL (WEBKIT_LOG_FONTCACHE, "substituting %s for %s, missing 0x%04x\n", DEBUG_OBJECT([substituteFont displayName]), DEBUG_OBJECT([font displayName]), c);
                return [[(IFTextRendererFactory *)[IFTextRendererFactory sharedFactory] rendererWithFont: substituteFont] widthForCharacters: characters length: length];
            }
        }

        totalWidth += widthForGlyph(self, glyphToWidthMap, glyphID);
    }

    return ROUND_TO_INT(totalWidth);
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
    ATSLayoutRecord *glyphRecords;
    ATSGlyphVector _glyphVector;
    UniChar end, start;
    unsigned int _end;
    unsigned int blockSize;
    
    if (characterToGlyphMap == 0)
        blockSize = INITIAL_BLOCK_SIZE;
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (c / blockSize) * blockSize;
    _end = ((unsigned int)start) + blockSize; 
    if (_end > 0xffff)
        end = 0xffff;
    else
        end = _end;
        
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

    ATSInitializeGlyphVector(count, 0, &_glyphVector);
    [self convertCharacters: &buffer[0] length: count glyphs: &_glyphVector];
    if (_glyphVector.numGlyphs != count)
        [NSException raise:NSInternalInconsistencyException format:@"Optimization assumption violation:  count and glyphID count not equal - for %@ %f", self, [font displayName], [font pointSize]];
            
    map->glyphs = (ATSGlyphRef *)malloc (count * sizeof(ATSGlyphRef));
    glyphRecords = (ATSLayoutRecord *)_glyphVector.firstRecord;
    for (i = 0; i < count; i++){
        ATSGlyphRef glyphID = glyphRecords[i].glyphID;
        map->glyphs[i] = glyphID;
    }
    ATSClearGlyphVector(&_glyphVector);
    
    if (characterToGlyphMap == 0)
        characterToGlyphMap = map;
    else {
        GlyphMap *lastMap = characterToGlyphMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    return map->glyphs[c - start];
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

