/*	
    WebTextRenderer.m	    
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebTextRenderer.h"

#import <Cocoa/Cocoa.h>

#import <ApplicationServices/ApplicationServices.h>
#import <CoreGraphics/CoreGraphicsPrivate.h>

#import <WebCore/WebCoreUnicode.h>

#import <WebKit/WebGlyphBuffer.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebUnicode.h>

#import <QD/ATSUnicodePriv.h>

#import <float.h>

#define NON_BREAKING_SPACE 0x00A0
#define SPACE 0x0020

#define IS_CONTROL_CHARACTER(c) ((c) < 0x0020 || (c) == 0x007F)

#define ROUND_TO_INT(x) (unsigned int)((x)+.5)

// Lose precision beyond 1000ths place. This is to work around an apparent
// bug in CoreGraphics where there seem to be small errors to some metrics.
#define CEIL_TO_INT(x) ((int)(x + 0.999)) /* ((int)(x + 1.0 - FLT_EPSILON)) */

#define LOCAL_BUFFER_SIZE 1024

// Covers Latin1.
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


typedef float WebGlyphWidth;

struct WidthMap {
    ATSGlyphRef startRange;
    ATSGlyphRef endRange;
    WidthMap *next;
    WebGlyphWidth *widths;
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

@interface NSFont (WebPrivate)
- (ATSUFontID)_atsFontID;
- (CGFontRef)_backingCGSFont;
// Private method to find a font for a character.
+ (NSFont *) findFontLike:(NSFont *)aFont forCharacter:(UInt32)c inLanguage:(NSLanguage *) language;
+ (NSFont *) findFontLike:(NSFont *)aFont forString:(NSString *)string withRange:(NSRange)range inLanguage:(NSLanguage *) language;
- (NSGlyph)_defaultGlyphForChar:(unichar)uu;
@end

@class NSCGSFont;


static CFCharacterSetRef nonBaseChars = NULL;


@interface WebTextRenderer (WebPrivate)
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

static inline WebGlyphWidth widthForGlyph (WebTextRenderer *renderer, WidthMap *map, ATSGlyphRef glyph)
{
    WebGlyphWidth width;
    BOOL errorResult;
    
    if (map == 0){
        map = [renderer extendGlyphToWidthMapToInclude: glyph];
        return widthForGlyph (renderer, map, glyph);
    }
        
    if (glyph >= map->startRange && glyph <= map->endRange){
        width = ((WebGlyphWidth *)map->widths)[glyph-map->startRange];
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
            return ((WebGlyphWidth *)map->widths)[glyph-map->startRange];
        }
        return width;
    }

    return widthForGlyph (renderer, map->next, glyph);
}


static inline  WebGlyphWidth widthForCharacter (WebTextRenderer *renderer, UniChar c)
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
    unsigned int k;

    if (length <= 1)
        return length;
    
    if (IsNonBaseChar(characters[0]))
        return 1;
                
    // Find all the non base characters after the current character.
    for (k = 1; k < length; k++)
        if (!IsNonBaseChar(characters[k]))
            break;
    return k;
}


@implementation WebTextRenderer

static BOOL bufferTextDrawing = NO;

+ (BOOL)shouldBufferTextDrawing
{
    return bufferTextDrawing;
}

+ (void)initialize
{
    WebKitInitializeUnicode();
    nonBaseChars = CFCharacterSetGetPredefined(kCFCharacterSetNonBase);
    bufferTextDrawing = [[[NSUserDefaults standardUserDefaults] stringForKey:@"BufferTextDrawing"] isEqual: @"YES"];
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


/* Convert non-breaking spaces into spaces, and skip control characters. */
- (void)convertCharacters: (const UniChar *)characters length: (unsigned)numCharacters toGlyphs: (ATSGlyphVector *)glyphs skipControlCharacters:(BOOL)skipControlCharacters
{
    unsigned i, numCharactersInBuffer;
    UniChar localBuffer[LOCAL_BUFFER_SIZE];
    UniChar *buffer = localBuffer;
    OSStatus status;
    
    for (i = 0; i < numCharacters; i++) {
        UniChar c = characters[i];
        if ((skipControlCharacters && IS_CONTROL_CHARACTER(c)) || c == NON_BREAKING_SPACE) {
            break;
        }
    }
    
    if (i < numCharacters) {
        if (numCharacters > LOCAL_BUFFER_SIZE) {
            buffer = (UniChar *)malloc(sizeof(UniChar) * numCharacters);
        }
        
        numCharactersInBuffer = 0;
        for (i = 0; i < numCharacters; i++) {
            UniChar c = characters[i];
            if (c == NON_BREAKING_SPACE) {
                buffer[numCharactersInBuffer++] = SPACE;
            } else if (!(skipControlCharacters && IS_CONTROL_CHARACTER(c))) {
                buffer[numCharactersInBuffer++] = characters[i];
            }
        }
        
        characters = buffer;
        numCharacters = numCharactersInBuffer;
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


- (int)widthForCharacters:(const UniChar *)characters length:(unsigned)stringLength
{
    return ROUND_TO_INT([self floatWidthForCharacters:characters stringLength:stringLength fromCharacterPosition:0 numberOfCharacters:stringLength withPadding: 0 applyRounding:YES attemptFontSubstitution: YES widths: 0]);
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


- (float)xHeight
{
    return [font xHeight];
}


// Useful page for testing http://home.att.net/~jameskass
static void _drawGlyphs(NSFont *font, NSColor *color, CGGlyph *glyphs, CGSize *advances, float x, float y, int numGlyphs)
{
    CGContextRef cgContext;

    if ([WebTextRenderer shouldBufferTextDrawing] && [[WebTextRendererFactory sharedFactory] coalesceTextDrawing]){
        // Add buffered glyphs and advances
        // FIXME:  If we ever use this again, need to add RTL.
        WebGlyphBuffer *gBuffer = [[WebTextRendererFactory sharedFactory] glyphBufferForFont: font andColor: color];
        [gBuffer addGlyphs: glyphs advances: advances count: numGlyphs at: x : y];
    }
    else {
        cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
        // Setup the color and font.
        [color set];
        [font set];

        CGContextSetTextPosition (cgContext, x, y);
        CGContextShowGlyphsWithAdvances (cgContext, glyphs, advances, numGlyphs);
    }
}

- (void)drawCharacters:(const UniChar *)characters stringLength: (unsigned int)length fromCharacterPosition: (int)from toCharacterPosition: (int)to atPoint:(NSPoint)point withPadding: (int)padding withTextColor:(NSColor *)textColor backgroundColor: (NSColor *)backgroundColor rightToLeft: (BOOL)rtl
{
    float *widthBuffer, localWidthBuffer[LOCAL_BUFFER_SIZE];
    CGGlyph *glyphBuffer, localGlyphBuffer[LOCAL_BUFFER_SIZE];
    NSFont **fontBuffer, *localFontBuffer[LOCAL_BUFFER_SIZE];
    CGSize *advances, localAdvanceBuffer[LOCAL_BUFFER_SIZE];
    int numGlyphs, i;
    float startX, nextX, backgroundWidth = 0.0;
    NSFont *currentFont;
    
    if (length == 0)
        return;
                
    // WARNING:  the character to glyph translation must result in less than
    // length glyphs.  As of now this is always true.
    if (length > LOCAL_BUFFER_SIZE) {
        advances = (CGSize *)calloc(length, sizeof(CGSize));
        widthBuffer = (float *)calloc(length, sizeof(float));
        glyphBuffer = (CGGlyph *)calloc(length, sizeof(ATSGlyphRef));
        fontBuffer = (NSFont **)calloc(length, sizeof(NSFont *));
    } else {
        advances = localAdvanceBuffer;
        widthBuffer = localWidthBuffer;
        glyphBuffer = localGlyphBuffer;
        fontBuffer = localFontBuffer;
    }

    [self _floatWidthForCharacters:characters 
        stringLength:length 
        fromCharacterPosition: 0 
        numberOfCharacters: length
        withPadding: padding
        applyRounding: YES
        attemptFontSubstitution: YES 
        widths: widthBuffer 
        fonts: fontBuffer
        glyphs: glyphBuffer
        numGlyphs: &numGlyphs];

    if (from == -1)
        from = 0;
    if (to == -1)
        to = numGlyphs;

    for (i = 0; (int)i < MIN(to,(int)numGlyphs); i++){
        advances[i].width = widthBuffer[i];
        advances[i].height = 0;
    }

    startX = point.x;
    for (i = 0; (int)i < MIN(from,(int)numGlyphs); i++)
        startX += advances[i].width;

    for (i = from; (int)i < MIN(to,(int)numGlyphs); i++)
        backgroundWidth += advances[i].width;

    if (backgroundColor != nil){
        [backgroundColor set];
        [NSBezierPath fillRect:NSMakeRect(startX, point.y - [self ascent], backgroundWidth, [self lineSpacing])];
    }
    
    // Finally, draw the glyphs.
    if (from < (int)numGlyphs){
        int lastFrom = from;
        int pos = from;

        if (rtl && numGlyphs > 1){
            int i;
            int end = numGlyphs;
            CGGlyph gswap1, gswap2;
            CGSize aswap1, aswap2;
            NSFont *fswap1, *fswap2;
            
            for (i = pos, end = numGlyphs; i < (numGlyphs - pos)/2; i++){
                gswap1 = glyphBuffer[i];
                gswap2 = glyphBuffer[--end];
                glyphBuffer[i] = gswap2;
                glyphBuffer[end] = gswap1;
            }
            for (i = pos, end = numGlyphs; i < (numGlyphs - pos)/2; i++){
                aswap1 = advances[i];
                aswap2 = advances[--end];
                advances[i] = aswap2;
                advances[end] = aswap1;
            }
            for (i = pos, end = numGlyphs; i < (numGlyphs - pos)/2; i++){
                fswap1 = fontBuffer[i];
                fswap2 = fontBuffer[--end];
                fontBuffer[i] = fswap2;
                fontBuffer[end] = fswap1;
            }
        }

        currentFont = fontBuffer[pos];
        nextX = startX;
        int nextGlyph = pos;
        // FIXME:  Don't run over the end of the glyph buffer when the
        // number of glyphs is less than the number of characters.  This
        // happens when a ligature is included in the glyph run.  The code
        // below will just stop drawing glyphs at the end of the glyph array
        // as a temporary hack.  The appropriate fix need to map character
        // ranges to glyph ranges.
        while (nextGlyph < to && nextGlyph < numGlyphs){
            if ((fontBuffer[nextGlyph] != 0 && fontBuffer[nextGlyph] != currentFont)){
                _drawGlyphs(currentFont, textColor, &glyphBuffer[lastFrom], &advances[lastFrom], startX, point.y, nextGlyph - lastFrom);
                lastFrom = nextGlyph;
                currentFont = fontBuffer[nextGlyph];
                startX = nextX;
            }
            nextX += advances[nextGlyph].width;
            nextGlyph++;
        }
        _drawGlyphs(currentFont, textColor, &glyphBuffer[lastFrom], &advances[lastFrom], startX, point.y, nextGlyph - lastFrom);
    }

    if (advances != localAdvanceBuffer) {
        free(advances);
        free(widthBuffer);
        free(glyphBuffer);
        free(fontBuffer);
    }
}


- (void)drawUnderlineForCharacters:(const UniChar *)characters stringLength:(unsigned)length atPoint:(NSPoint)point withColor:(NSColor *)color
{
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    int width = [self widthForCharacters:characters length:length];
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


- (float)floatWidthForCharacters:(const UniChar *)characters stringLength:(unsigned)stringLength characterPosition: (int)pos
{
    // Return the width of the first complete character at the specified position.  Even though
    // the first 'character' may contain more than one unicode characters this method will
    // work correctly.
    return [self floatWidthForCharacters:characters stringLength:stringLength fromCharacterPosition:pos numberOfCharacters:1 withPadding: 0 applyRounding: YES attemptFontSubstitution: YES widths: nil];
}


- (float)floatWidthForCharacters:(const UniChar *)characters stringLength:(unsigned)stringLength fromCharacterPosition: (int)pos numberOfCharacters: (int)len
{
    return [self floatWidthForCharacters:characters stringLength:stringLength fromCharacterPosition:pos numberOfCharacters:len withPadding: 0 applyRounding: YES attemptFontSubstitution: YES widths: nil];
}


- (float)floatWidthForCharacters:(const UniChar *)characters stringLength:(unsigned)stringLength fromCharacterPosition: (int)pos numberOfCharacters: (int)len withPadding: (int)padding applyRounding: (BOOL)applyRounding attemptFontSubstitution: (BOOL)attemptSubstitution widths: (float *)widthBuffer
{
    return [self _floatWidthForCharacters:characters stringLength:stringLength fromCharacterPosition:pos numberOfCharacters:len withPadding: 0 applyRounding: YES attemptFontSubstitution: YES widths: widthBuffer fonts: nil  glyphs: nil numGlyphs: nil];
}

#ifdef DEBUG_COMBINING
static const char *directionNames[] = {
        "DirectionL", 	// Left Letter 
        "DirectionR",	// Right Letter
        "DirectionEN",	// European Number
        "DirectionES",	// European Separator
        "DirectionET",	// European Terminator (post/prefix e.g. $ and %)
        "DirectionAN",	// Arabic Number
        "DirectionCS",	// Common Separator 
        "DirectionB", 	// Paragraph Separator (aka as PS)
        "DirectionS", 	// Segment Separator (TAB)
        "DirectionWS", 	// White space
        "DirectionON",	// Other Neutral

	// types for explicit controls
        "DirectionLRE", 
        "DirectionLRO", 

        "DirectionAL", 	// Arabic Letter (Right-to-left)

        "DirectionRLE", 
        "DirectionRLO", 
        "DirectionPDF", 

        "DirectionNSM", 	// Non-spacing Mark
        "DirectionBN"	// Boundary neutral (type of RLE etc after explicit levels)
};

static const char *joiningNames[] = {
        "JoiningOther",
        "JoiningDual",
        "JoiningRight",
        "JoiningCausing"
};
#endif

- (float)_floatWidthForCharacters:(const UniChar *)characters stringLength:(unsigned)stringLength fromCharacterPosition: (int)pos numberOfCharacters: (int)len withPadding: (int)padding applyRounding: (BOOL)applyRounding attemptFontSubstitution: (BOOL)attemptSubstitution widths: (float *)widthBuffer fonts: (NSFont **)fontBuffer glyphs: (CGGlyph *)glyphBuffer numGlyphs: (int *)_numGlyphs
{
    float totalWidth = 0;
    unsigned int i, clusterLength;
    NSFont *substituteFont = nil;
    ATSGlyphRef glyphID;
    float lastWidth = 0;
    uint numSpaces = 0;
    int padPerSpace = 0;
    int numGlyphs = 0;

    if (len <= 0){
        if (_numGlyphs)
            *_numGlyphs = 0;
        return 0;
    }

#define SHAPE_STRINGS
#ifdef SHAPE_STRINGS
    UniChar munged[stringLength];
    {
        UniChar *shaped;
        int lengthOut;
        shaped = shapedString ((UniChar *)characters, stringLength,
                               pos,
                               len,
                               0, &lengthOut);
        if (shaped){
            //printf ("%d input, %d output\n", len, lengthOut);
            //for (i = 0; i < (int)len; i++){
            //    printf ("0x%04x shaped to 0x%04x\n", characters[i], shaped[i]);
            //}
            // FIXME:  Hack-o-rific, copy shaped buffer into munged
            // character buffer.
            for (i = 0; i < (unsigned int)pos; i++){
                munged[i] = characters[i];
            }
            for (i = pos; i < (unsigned int)pos+lengthOut; i++){
                munged[i] = shaped[i-pos];
            }
            characters = munged;
            len = lengthOut;
        }
    }
#endif
    
    // If the padding is non-zero, count the number of spaces in the string
    // and divide that by the padding for per space addition.
    if (padding > 0){
        for (i = pos; i < (uint)pos+len; i++){
            if (characters[i] == NON_BREAKING_SPACE || characters[i] == SPACE)
                numSpaces++;
        }
        padPerSpace = CEIL_TO_INT ((((float)padding) / ((float)numSpaces)));
    }

    //printf("width: font %s, size %.1f, text \"%s\"\n", [[font fontName] cString], [font pointSize], [[NSString stringWithCharacters:characters length:length] UTF8String]);
    for (i = 0; i < stringLength; i++) {
        UniChar c = characters[i];
        
        // Skip control characters.
        if (IS_CONTROL_CHARACTER(c)) {
            continue;
        }
        
        if (c == NON_BREAKING_SPACE) {
            c = SPACE;
        }
        
        // Drop out early if we've measured to the end of the requested
        // fragment.
        if ((int)i - pos >= len) {
            // Check if next character is a space. If so, we have to apply rounding.
            if (c == SPACE && applyRounding) {
                float delta = CEIL_TO_INT(totalWidth) - totalWidth;
                totalWidth += delta;
                if (widthBuffer)
                    widthBuffer[numGlyphs - 1] += delta;
            }
            break;
        }

        glyphID = glyphForCharacter(characterToGlyphMap, c);
        if (glyphID == nonGlyphID) {
            glyphID = [self extendCharacterToGlyphMapToInclude: c];
        }

#ifdef DEBUG_DIACRITICAL
        if (IsNonBaseChar(c)){
            printf ("NonBaseCharacter 0x%04x, joining attribute %d(%s), combining class %d, direction %d, glyph %d, width %f\n", c, WebCoreUnicodeJoiningFunction(c), joiningNames(WebCoreUnicodeJoiningFunction(c)), WebCoreUnicodeCombiningClassFunction(c), WebCoreUnicodeDirectionFunction(c), glyphID, widthForGlyph(self, glyphToWidthMap, glyphID));
        }
#endif
        
        // Try to find a substitute font if this font didn't have a glyph for a character in the
        // string.  If one isn't found we end up drawing and measuring the 0 glyph, usually a box.
        if (glyphID == 0 && attemptSubstitution) {
            clusterLength = findLengthOfCharacterCluster (&characters[i], stringLength - i);
            substituteFont = [self substituteFontForCharacters: &characters[i] length: clusterLength];
            if (substituteFont) {
                int cNumGlyphs;
                lastWidth = [[[WebTextRendererFactory sharedFactory] rendererWithFont: substituteFont] 
                                _floatWidthForCharacters: &characters[i] 
                                stringLength: clusterLength 
                                fromCharacterPosition: 0 numberOfCharacters: clusterLength 
                                withPadding: 0 applyRounding: NO attemptFontSubstitution: NO 
                                widths: ((widthBuffer != 0 ) ? (&widthBuffer[numGlyphs]) : nil)
                                fonts: nil
                                glyphs: ((glyphBuffer != 0 ) ? (&glyphBuffer[numGlyphs]) : nil)
                                numGlyphs: &cNumGlyphs];
                if (fontBuffer){
                    int j;
                    for (j = 0; j < cNumGlyphs; j++)
                        fontBuffer[numGlyphs+j] = substituteFont;
                }
                numGlyphs += cNumGlyphs;
            }
        }
        
        // If we have a valid glyph OR if we couldn't find a substitute font
        // measure the glyph.
        if (glyphID > 0 || ((glyphID == 0) && substituteFont == nil)) {
            if (glyphID == spaceGlyph && applyRounding) {
                if (lastWidth > 0){
                    float delta = CEIL_TO_INT(totalWidth) - totalWidth;
                    totalWidth += delta;
                    if (widthBuffer)
                        widthBuffer[numGlyphs - 1] += delta;
                }   
                lastWidth = CEIL_TO_INT(widthForGlyph(self, glyphToWidthMap, glyphID));
                if (padding > 0){
                    // Only use left over padding if note evenly divisible by 
                    // number of spaces.
                    if (padding < padPerSpace){
                        lastWidth += padding;
                        padding = 0;
                    }
                    else {
                        lastWidth += padPerSpace;
                        padding -= padPerSpace;
                    }
                }
            }
            else
                lastWidth = widthForGlyph(self, glyphToWidthMap, glyphID);
            
            if (fontBuffer)
                fontBuffer[numGlyphs] = font;
            if (glyphBuffer)
                glyphBuffer[numGlyphs] = glyphID;
            if (widthBuffer){
                widthBuffer[numGlyphs] = lastWidth;
            }
            numGlyphs++;
        }
#ifdef DEBUG_COMBINING        
        printf ("Character 0x%04x, joining attribute %d(%s), combining class %d, direction %d(%s)\n", c, WebCoreUnicodeJoiningFunction(c), joiningNames[WebCoreUnicodeJoiningFunction(c)], WebCoreUnicodeCombiningClassFunction(c), WebCoreUnicodeDirectionFunction(c), directionNames[WebCoreUnicodeDirectionFunction(c)]);
#endif
        
        totalWidth += lastWidth;       
    }

    // Don't ever apply rounding for single character.  Single character measurement
    // intra word needs to be non-ceiled.
    if ((len > 1 || stringLength == 1) && applyRounding){
        float delta = CEIL_TO_INT(totalWidth) - totalWidth;
        totalWidth += delta;
        if (widthBuffer)
            widthBuffer[numGlyphs-1] += delta;
    }

    if (_numGlyphs)
        *_numGlyphs = numGlyphs;
    
    return totalWidth;
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
        
    LOG(FontCache, "%@ (0x%04x) adding glyphs for 0x%04x to 0x%04x", font, c, start, end);

    map->startRange = start;
    map->endRange = end;
    
    unsigned int i, count = end - start + 1;
    short unsigned int buffer[INCREMENTAL_BLOCK_SIZE+2];
    
    for (i = 0; i < count; i++){
        //if (IsNonBaseChar(i+start))
        //    buffer[i] = 0;
        //else
            buffer[i] = i+start;
    }

    ATSInitializeGlyphVector(count, 0, &glyphVector);
    [self convertCharacters: &buffer[0] length: count toGlyphs: &glyphVector skipControlCharacters: NO];
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

    LOG(FontCache, "%@ (0x%04x) adding widths for range 0x%04x to 0x%04x", font, glyphID, start, end);

    map->startRange = start;
    map->endRange = end;
    count = end - start + 1;

    map->widths = (WebGlyphWidth *)malloc (count * sizeof(WebGlyphWidth));

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
    LOG(FontCache, "%@ total time to advances lookup %f seconds", font, totalCGGetAdvancesTime);
#endif
    return map;
}

@end
