//
//  IFCachedTextRenderer.m
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFCachedTextRenderer.h"

#import <Cocoa/Cocoa.h>

#import <ApplicationServices/ApplicationServices.h>
#import <CoreGraphics/CGFontPrivate.h>
#import <WebKit/WebKitDebug.h>

#define NON_BREAKING_SPACE 0xA0
#define SPACE 0x20

//#define ROUND_TO_INT(x) (int)(((x) > (floor(x) + .5)) ? ceil(x) : floor(x))
#define ROUND_TO_INT(x) (unsigned int)((x)+.5)

#ifdef FOOOOFOOO
static inline int ROUND_TO_INT (float x)
{
    int floored = (int)(x);
    
    if (((float)x) > ((float)(floored) + .5))
        return (int)ceil(x);
    return floored;
}
#endif

#define LOCAL_GLYPH_BUFFER_SIZE 1024

#define INITIAL_GLYPH_CACHE_MAX 512
#define INCREMENTAL_GLYPH_CACHE_BLOCK 1024

#define UNINITIALIZED_GLYPH_WIDTH 65535

// These definitions are used to bound the character-to-glyph mapping cache.  The
// range is limited to LATIN1.  When accessing the cache a check must be made to
// determine that a character range does not include a composable charcter.

// The first displayable character in latin1. (SPACE)
#define FIRST_CACHE_CHARACTER (0x20)

// The last character in latin1 extended A. (LATIN SMALL LETTER LONG S)
#define LAST_CACHE_CHARACTER (0x17F)

@interface NSFont (IFPrivate)
- (ATSUFontID)_atsFontID;
- (CGFontRef)_backingCGSFont;
@end

static void InitATSGlyphVector(ATSGlyphVector *glyphVector, UInt32 numGlyphs)
{
    if (glyphVector->numAllocatedGlyphs == 0) {
        ATSInitializeGlyphVector(numGlyphs, 0, glyphVector);

//#warning Aki: 6/28/00 Need to reconsider these when we do bidi
        ATSFree(glyphVector->levelIndexes);
        glyphVector->levelIndexes = NULL;
    } else if (glyphVector->numAllocatedGlyphs < numGlyphs) {
        ATSGrowGlyphVector(numGlyphs - glyphVector->numAllocatedGlyphs, glyphVector);
    }
}

static void ResetATSGlyphVector(ATSGlyphVector *glyphVector)
{
    ATSGlyphVector tmpVector = *glyphVector;

    // Prevents glyph array & style settings from deallocated
    glyphVector->firstRecord = NULL;
    glyphVector->styleSettings = NULL;
    glyphVector->levelIndexes = NULL;
    ATSClearGlyphVector(glyphVector);

    glyphVector->numAllocatedGlyphs = tmpVector.numAllocatedGlyphs;
    glyphVector->recordSize = tmpVector.recordSize;
    glyphVector->firstRecord = tmpVector.firstRecord;
    glyphVector->styleSettings = tmpVector.styleSettings;
    glyphVector->levelIndexes = tmpVector.levelIndexes;
}

@class NSCGSFont;

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

/* Convert non-breaking spaces into spaces. */
static void ConvertCharactersToGlyphs(ATSStyleGroupPtr styleGroup, const UniChar *characters, int numCharacters, ATSGlyphVector *glyphs)
{
    int i;
    UniChar localBuffer[LOCAL_GLYPH_BUFFER_SIZE];
    UniChar *buffer = localBuffer;
    
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
    
    ATSUConvertCharToGlyphs(styleGroup, characters, 0, numCharacters, 0, glyphs);
    
    if (buffer != localBuffer) {
        free(buffer);
    }
}

@implementation IFCachedTextRenderer

- (void)initializeCaches
{
    unsigned int i, glyphsToCache;
    int errorResult;
    size_t numGlyphsInFont = CGFontGetNumberOfGlyphs([font _backingCGSFont]);
    short unsigned int sequentialGlyphs[INITIAL_GLYPH_CACHE_MAX];
    ATSLayoutRecord *glyphRecords;

    WEBKITDEBUGLEVEL (WEBKIT_LOG_FONTCACHE, "Caching %s %.0f (%ld glyphs) ascent = %f, descent = %f, defaultLineHeightForFont = %f\n", [[font displayName] cString], [font pointSize], numGlyphsInFont, [font ascender], [font descender], [font defaultLineHeightForFont]); 

    // Initially just cache the max of number of glyphs in font or
    // INITIAL_GLYPH_CACHE_MAX.  Holes in the cache will be filled on demand
    // in INCREMENTAL_GLYPH_CACHE_BLOCK chunks. 
    if (numGlyphsInFont > INITIAL_GLYPH_CACHE_MAX)
        glyphsToCache = INITIAL_GLYPH_CACHE_MAX;
    else
        glyphsToCache = numGlyphsInFont;
    widthCacheSize = (int)numGlyphsInFont;
    for (i = 0; i < glyphsToCache; i++)
        sequentialGlyphs[i] = i;
        
    widthCache = (IFGlyphWidth *)calloc (1, widthCacheSize * sizeof(IFGlyphWidth));
    
    // Some glyphs can have zero width, so we have to use a non-zero value
    // in empty slots to indicate they are uninitialized.
    for (i = glyphsToCache; i < widthCacheSize; i++){
        widthCache[i] = UNINITIALIZED_GLYPH_WIDTH;
    }
    
    CGContextRef cgContext;

    cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    errorResult = CGFontGetGlyphScaledAdvances ([font _backingCGSFont], &sequentialGlyphs[0], glyphsToCache, widthCache, [font pointSize]);
    if (errorResult == 0)
        [NSException raise:NSInternalInconsistencyException format:@"Optimization assumption violation:  unable to cache glyph advances - for %@ %f", self, [font displayName], [font pointSize]];

    unsigned int latinCount = LAST_CACHE_CHARACTER - FIRST_CACHE_CHARACTER + 1;
    short unsigned int latinBuffer[LAST_CACHE_CHARACTER+1];
    
    for (i = FIRST_CACHE_CHARACTER; i <= LAST_CACHE_CHARACTER; i++){
        latinBuffer[i] = i;
    }

    ATSGlyphVector latinGlyphVector;
    ATSInitializeGlyphVector(latinCount, 0, &latinGlyphVector);
    ConvertCharactersToGlyphs(styleGroup, &latinBuffer[FIRST_CACHE_CHARACTER], latinCount, &latinGlyphVector);
    if (latinGlyphVector.numGlyphs != latinCount)
        [NSException raise:NSInternalInconsistencyException format:@"Optimization assumption violation:  ascii and glyphID count not equal - for %@ %f", self, [font displayName], [font pointSize]];
        
    unsigned int numGlyphs = latinGlyphVector.numGlyphs;
    characterToGlyph = (ATSGlyphRef *)calloc (1, latinGlyphVector.numGlyphs * sizeof(ATSGlyphRef));
    glyphRecords = (ATSLayoutRecord *)latinGlyphVector.firstRecord;
    for (i = 0; i < numGlyphs; i++){
        characterToGlyph[i] = glyphRecords[i].glyphID;
    }
    ATSClearGlyphVector(&latinGlyphVector);

#define DEBUG_CACHE_SIZE
#ifdef DEBUG_CACHE_SIZE
    static int totalCacheSize = 0;
    
    totalCacheSize += widthCacheSize * sizeof(IFGlyphWidth) + numGlyphs * sizeof(ATSGlyphRef) + sizeof(*self);
    WEBKITDEBUGLEVEL (WEBKIT_LOG_MEMUSAGE, "memory usage in bytes:  widths = %ld, latin1 ext. character-to-glyph = %ld, total this cache = %ld, total all caches %d\n", widthCacheSize * sizeof(IFGlyphWidth), numGlyphs * sizeof(ATSGlyphRef), widthCacheSize * sizeof(IFGlyphWidth) + numGlyphs * sizeof(ATSGlyphRef) + sizeof(*self), totalCacheSize); 
#endif
}

- initWithFont:(NSFont *)f
{
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
    if (glyphVector.numAllocatedGlyphs > 0)
        ATSClearGlyphVector(&glyphVector);
    free(widthCache);
    free(characterToGlyph);
    
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

- (void)drawString:(NSString *)string atPoint:(NSPoint)point withColor:(NSColor *)color
{
    // This will draw the text from the top of the bounding box down.
    // Qt expects to draw from the baseline.
    // Remember that descender is negative.
    point.y -= [self lineSpacing] - [self descent];
    
    UniChar localBuffer[LOCAL_GLYPH_BUFFER_SIZE];
    const UniChar *_internalBuffer = CFStringGetCharactersPtr ((CFStringRef)string);
    const UniChar *internalBuffer;

    if (!_internalBuffer){
        // FIXME: Handle case where length > LOCAL_GLYPH_BUFFER_SIZE
        CFStringGetCharacters((CFStringRef)string, CFRangeMake(0, CFStringGetLength((CFStringRef)string)), &localBuffer[0]);
        internalBuffer = &localBuffer[0];
    }
    else
        internalBuffer = _internalBuffer;

    CGContextRef cgContext;

    InitATSGlyphVector(&glyphVector, [string length]);

    ConvertCharactersToGlyphs(styleGroup, internalBuffer, [string length], &glyphVector);

    [color set];
    [font set];
    
    if ([font glyphPacking] != NSNativeShortGlyphPacking &&
        [font glyphPacking] != NSTwoByteGlyphPacking)
        [NSException raise:NSInternalInconsistencyException format:@"%@: Don't know how to deal with font %@", self, [font displayName]];
        
    {
        int i, numGlyphs = glyphVector.numGlyphs;
        char localGlyphBuf[LOCAL_GLYPH_BUFFER_SIZE];
        char *usedGlyphBuf, *glyphBufPtr, *glyphBuf = 0;
        ATSLayoutRecord *glyphRecords = (ATSLayoutRecord *)glyphVector.firstRecord;
        
        if (numGlyphs > LOCAL_GLYPH_BUFFER_SIZE/2)
            usedGlyphBuf = glyphBufPtr = glyphBuf = (char *)malloc (numGlyphs * 2);
        else
            usedGlyphBuf = glyphBufPtr = &localGlyphBuf[0];
            
        for (i = 0; i < numGlyphs; i++){
            *glyphBufPtr++ = (char)((glyphRecords->glyphID >> 8) & 0x00FF);
            *glyphBufPtr++ = (char)(glyphRecords->glyphID & 0x00FF);
            glyphRecords++;
        }
        cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
        CGContextSetCharacterSpacing(cgContext, 0.0);
        //CGContextShowGlyphsAtPoint (cgContext, p.x, p.y + lineHeight - 1 - ROUND_TO_INT(-[font descender]), (const short unsigned int *)usedGlyphBuf, numGlyphs);
        CGContextShowGlyphsAtPoint (cgContext, point.x, point.y + [font defaultLineHeightForFont] - 1, (const short unsigned int *)usedGlyphBuf, numGlyphs);
        
        if (glyphBuf)
            free (glyphBuf);
    }

    ResetATSGlyphVector(&glyphVector);
}

- (void)drawUnderlineForString:(NSString *)string atPoint:(NSPoint)point withColor:(NSColor *)color
{
    // This will draw the text from the top of the bounding box down.
    // Qt expects to draw from the baseline.
    // Remember that descender is negative.
    point.y -= [self lineSpacing] - [self descent];
    
    int width = [self widthForString: string];
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    CGContextRef cgContext;
    float lineWidth;
    
    [color set];

    BOOL flag = [graphicsContext shouldAntialias];

    [graphicsContext setShouldAntialias: NO];

    cgContext = (CGContextRef)[graphicsContext graphicsPort];
    lineWidth = 0.0;
    if ([graphicsContext isDrawingToScreen] && lineWidth == 0.0) {
        CGSize size = CGSizeApplyAffineTransform(CGSizeMake(1.0, 1.0), CGAffineTransformInvert(CGContextGetCTM(cgContext)));
        lineWidth = size.width;
    }
    CGContextSetLineWidth(cgContext, lineWidth);
    ///CGContextMoveToPoint(cgContext, p.x, p.y + lineHeight + 0.5 - ROUND_TO_INT(-[font descender]));
    //CGContextAddLineToPoint(cgContext, p.x + rect.size.width, p.y + lineHeight + 0.5 - ROUND_TO_INT(-[font descender]));
    CGContextMoveToPoint(cgContext, point.x, point.y + [font defaultLineHeightForFont] + 0.5);
    CGContextAddLineToPoint(cgContext, point.x + width, point.y + [font defaultLineHeightForFont] + 0.5);
    CGContextStrokePath(cgContext);

    [graphicsContext setShouldAntialias: flag];
}

- (int)widthForCharacters:(const UniChar *)characters length:(unsigned)length
{
    float totalWidth = 0;
    unsigned int i, index;
    int glyphID;
    ATSLayoutRecord *glyphRecords;
    unsigned int numGlyphs;
    
    ATSGlyphRef localCharacterToGlyph[LOCAL_GLYPH_BUFFER_SIZE];
    ATSGlyphRef *usedCharacterToGlyph, *allocateCharacterToGlyph = 0;
    
    BOOL needCharToGlyphLookup = NO;


    // Best case, and common case for latin1, performance will require two iterations over
    // the internalBuffer.
    // Pass 1 (on characters):  
    //      Determine if we can use character-to-glyph map by comparing characters to cache
    //      range.
    // Pass 2 (on characters):  
    //      Sum the widths using the character-to-glyph map and width cache.
    
    // FIXME:  For non-latin1 character sets we don't optimize.
    // Worst case performance we must lookup the character-to-glyph map and lookup the glyph
    // widths.
    
    if ([font glyphPacking] != NSNativeShortGlyphPacking &&
        [font glyphPacking] != NSTwoByteGlyphPacking)
	[NSException raise:NSInternalInconsistencyException format:@"%@: Don't know how to pack glyphs for font %@ %f", self, [font displayName], [font pointSize]];
    
    if (widthCache == 0)
        [self initializeCaches];
    
    // Pass 1:
    // Check if we can use the cached character-to-glyph map.  We only use the character-to-glyph map
    // if ALL the characters in the string fall in the safe cache range.  This must be done
    // to ensure that we don't match composable characters incorrectly.  This check could
    // be smarter.  Also the character-to-glyph could be extended to support other ranges
    // of unicode.  For now, we only optimize for latin1.
    for (i = 0; i < length; i++){
        if (characters[i] < FIRST_CACHE_CHARACTER || characters[i] > LAST_CACHE_CHARACTER){
            needCharToGlyphLookup = YES;
            break;
        }
    }

    // If we can't use the cached map, then calculate a map for this string.   Expensive.
    if (needCharToGlyphLookup){
        
        WEBKITDEBUGLEVEL(WEBKIT_LOG_FONTCACHECHARMISS, "character-to-glyph cache miss for character 0x%04x in %s, %.0f\n", characters[i], [[font displayName] lossyCString], [font pointSize]);
        InitATSGlyphVector(&glyphVector, length);
        ConvertCharactersToGlyphs(styleGroup, characters, length, &glyphVector);
        glyphRecords = (ATSLayoutRecord *)glyphVector.firstRecord;
        numGlyphs = glyphVector.numGlyphs;

        if (numGlyphs > LOCAL_GLYPH_BUFFER_SIZE)
            usedCharacterToGlyph = allocateCharacterToGlyph = (ATSGlyphRef *)calloc (1, numGlyphs * sizeof(ATSGlyphRef));
        else
            usedCharacterToGlyph = &localCharacterToGlyph[0];
            
        for (i = 0; i < numGlyphs; i++){
            glyphID = glyphRecords[i].glyphID;
            usedCharacterToGlyph[i] = glyphID;
            
            // Fill the block of glyphs for the glyph needed. If we're going to incur the overhead
            // of calling into CG, we may as well get a block of scaled glyph advances.
            if (widthCache[glyphID] == UNINITIALIZED_GLYPH_WIDTH) {
                short unsigned int sequentialGlyphs[INCREMENTAL_GLYPH_CACHE_BLOCK];
                unsigned int blockStart, blockEnd, blockID;
                int errorResult;
                
                blockStart = (glyphID / INCREMENTAL_GLYPH_CACHE_BLOCK) * INCREMENTAL_GLYPH_CACHE_BLOCK;
                blockEnd = blockStart + INCREMENTAL_GLYPH_CACHE_BLOCK;
                if (blockEnd > widthCacheSize)
                    blockEnd = widthCacheSize;
                WEBKITDEBUGLEVEL (WEBKIT_LOG_FONTCACHE, "width cache miss for glyph 0x%04x in %s, %.0f, filling block 0x%04x to 0x%04x\n", glyphID, [[font displayName] cString], [font pointSize], blockStart, blockEnd);
                for (blockID = blockStart; blockID < blockEnd; blockID++)
                    sequentialGlyphs[blockID-blockStart] = blockID;

                errorResult = CGFontGetGlyphScaledAdvances ([font _backingCGSFont], &sequentialGlyphs[0], blockEnd-blockStart, &widthCache[blockStart], [font pointSize]);
                if (errorResult == 0)
                    [NSException raise:NSInternalInconsistencyException format:@"Optimization assumption violation:  unable to cache glyph widths - for %@ %f", self, [font displayName], [font pointSize]];
            }
        }

        ResetATSGlyphVector(&glyphVector);
    }
    else {
        numGlyphs = length;
        usedCharacterToGlyph = characterToGlyph;
    }
    
    // Pass 2:
    // Sum the widths for all the glyphs.
    if (needCharToGlyphLookup){
        for (i = 0; i < numGlyphs; i++){
            totalWidth += widthCache[usedCharacterToGlyph[i]];
        }
        
        if (allocateCharacterToGlyph)
            free (allocateCharacterToGlyph);
    }
    else {
        for (i = 0; i < numGlyphs; i++){
            index = characters[i]-FIRST_CACHE_CHARACTER;
            totalWidth += widthCache[usedCharacterToGlyph[index]];
        }
    }
    
    return totalWidth;
}

- (void)drawString:(NSString *)string inRect:(NSRect)rect withColor:(NSColor *)color paragraphStyle:(NSParagraphStyle *)style
{
    [string drawInRect:rect withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:
        font, NSFontAttributeName,
        color, NSForegroundColorAttributeName,
        style, NSParagraphStyleAttributeName,
        nil]];
}

@end
