/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#include <math.h>

#include <kwqdebug.h>
#include <qfontmetrics.h>

#import <Cocoa/Cocoa.h>

#define DIRECT_TO_CG

#import <KWQMetrics.h>
#import <KWQTextStorage.h>
#import <KWQTextContainer.h>

#define FLOOR_TO_INT(x) (int)(floor(x))
//#define ROUND_TO_INT(x) (int)(((x) > (floor(x) + .5)) ? ceil(x) : floor(x))
#define ROUND_TO_INT(x) (int)(x+.5)
#ifdef FOOOOFOOO
static inline int ROUND_TO_INT (float x)
{
    int floored = (int)(x);
    
    if (((float)x) > ((float)(floored) + .5))
        return (int)ceil(x);
    return floored;
}
#endif

@implementation KWQSmallLayoutFragment
- (NSRange)glyphRange
{
    NSRange glyphRange;
    
    glyphRange.location = 0;
    glyphRange.length = glyphRangeLength;
    
    return glyphRange;
}

- (void)setGlyphRange: (NSRange)r
{
    glyphRangeLength = r.length;
}

- (void)setBoundingRect: (NSRect)r
{
    width = (unsigned short)r.size.width;
    height = (unsigned short)r.size.height;
}

- (NSRect)boundingRect
{
    NSRect boundingRect;
    
    boundingRect.origin.x = 0;
    boundingRect.origin.y = 0;
    boundingRect.size.width = (float)width;
    boundingRect.size.height = (float)height;
    
#ifdef _DEBUG_LAYOUT_FRAGMENT
    accessCount++;
#endif

    return boundingRect;
}

#ifdef _DEBUG_LAYOUT_FRAGMENT
- (int)accessCount { return accessCount; }
#endif

#ifdef _DEBUG_LAYOUT_FRAGMENT
- (NSComparisonResult)compare: (id)val
{
    if ([val accessCount] > accessCount)
        return NSOrderedDescending;
    else if ([val accessCount] < accessCount)
        return NSOrderedAscending;
    return NSOrderedSame;
}

#endif

@end

@implementation KWQLargeLayoutFragment
- (NSRange)glyphRange
{
    return glyphRange;
}

- (void)setGlyphRange: (NSRange)r
{
    glyphRange = r;
}

- (void)setBoundingRect: (NSRect)r
{
    boundingRect = r;
}

- (NSRect)boundingRect
{
#ifdef _DEBUG_LAYOUT_FRAGMENT
    accessCount++;
#endif

    return boundingRect;
}

#ifdef _DEBUG_LAYOUT_FRAGMENT
- (int)accessCount { return accessCount; }
#endif

#ifdef _DEBUG_LAYOUT_FRAGMENT
- (NSComparisonResult)compare: (id)val
{
    if ([val accessCount] > accessCount)
        return NSOrderedDescending;
    else if ([val accessCount] < accessCount)
        return NSOrderedAscending;
    return NSOrderedSame;
}

#endif

@end

static NSMutableDictionary *metricsCache = nil;

/*
    
*/
@implementation KWQLayoutInfo

#ifdef DIRECT_TO_CG

static void __IFInitATSGlyphVector(ATSGlyphVector *glyphVector, UInt32 numGlyphs) {
    if (glyphVector->numAllocatedGlyphs == 0) {
        ATSInitializeGlyphVector(numGlyphs, 0, glyphVector);

//#warning Aki: 6/28/00 Need to reconsider these when we do bidi
        ATSFree(glyphVector->levelIndexes);
        glyphVector->levelIndexes = NULL;
    } else if (glyphVector->numAllocatedGlyphs < numGlyphs) {
        ATSGrowGlyphVector(numGlyphs - glyphVector->numAllocatedGlyphs, glyphVector);
    }
}

static void __IFResetATSGlyphVector(ATSGlyphVector *glyphVector) {
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

static void __IFFillStyleWithAttributes(ATSUStyle style, NSFont *theFont) {
    if (theFont) {
        ATSUFontID fontId = (ATSUFontID)[theFont _atsFontID];
        ATSUAttributeTag tag = kATSUFontTag;
        ByteCount size = sizeof(ATSUFontID);
        ATSUFontID *valueArray[1] = {&fontId};

        if (fontId) {
            if (ATSUSetAttributes(style, 1, &tag, &size, (const ATSUAttributeValuePtr)valueArray) != noErr)
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
#endif

- (void)drawString: (NSString *)string atPoint: (NSPoint)p withFont: (NSFont *)aFont color: (NSColor *)color
{
#ifdef DIRECT_TO_CG
    UniChar localBuffer[LOCAL_GLYPH_BUFFER_SIZE];
    const UniChar *_internalBuffer = CFStringGetCharactersPtr ((CFStringRef)string);
    const UniChar *internalBuffer;

    if (!_internalBuffer){
        CFStringGetCharacters((CFStringRef)string, CFRangeMake(0, CFStringGetLength((CFStringRef)string)), &localBuffer[0]);
        internalBuffer = &localBuffer[0];
    }
    else
        internalBuffer = _internalBuffer;

    CGContextRef cgContext;

    __IFInitATSGlyphVector(&_glyphVector, [string length]);

    (void)ATSUConvertCharToGlyphs(_styleGroup, internalBuffer, 0, [string length], 0, (ATSGlyphVector *)&_glyphVector);

    [color set];
    [aFont set];
    
    if ([aFont glyphPacking] != NSNativeShortGlyphPacking &&
        [aFont glyphPacking] != NSTwoByteGlyphPacking)
        [NSException raise:NSInternalInconsistencyException format:@"%@: Don't know how to deal with font %@", self, [aFont displayName]];
        
    {
        int i, numGlyphs = _glyphVector.numGlyphs;
        char localGlyphBuf[LOCAL_GLYPH_BUFFER_SIZE];
        char *usedGlyphBuf, *glyphBufPtr, *glyphBuf = 0;
        ATSLayoutRecord *glyphRecords = _glyphVector.firstRecord;
        
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
        //CGContextShowGlyphsAtPoint (cgContext, p.x, p.y + [frag boundingRect].size.height + (int)[font descender] - 1, (const short unsigned int *)usedGlyphBuf, numGlyphs);
        CGContextShowGlyphsAtPoint (cgContext, p.x, p.y + lineHeight - 1, (const short unsigned int *)usedGlyphBuf, numGlyphs);
        
        if (glyphBuf)
            free (glyphBuf);
    }

    __IFResetATSGlyphVector(&_glyphVector);
#else
    id <KWQLayoutFragment> frag = [storage getFragmentForString: (NSString *)string];
    NSLog (@"WARNING:  Unable to use CG for drawString:atPoint:withFont:color:\n");

    [self setColor: color];
    [textStorage setAttributes: [layoutInfo attributes]];
    [textStorage setString: string];
    [layoutManager drawGlyphsForGlyphRange:[frag glyphRange] atPoint:p];
#endif
}


+ (void)drawString: (NSString *)string atPoint: (NSPoint)p withFont: (NSFont *)aFont color: (NSColor *)color
{
    KWQLayoutInfo *layoutInfo = [KWQLayoutInfo getMetricsForFont: aFont];
    [layoutInfo drawString: string atPoint: p withFont: aFont color: color];
}


- (void)drawUnderlineForString: (NSString *)string atPoint: (NSPoint)p withFont: (NSFont *)aFont color: (NSColor *)color
{
#ifdef DIRECT_TO_CG
    NSRect rect = [self rectForString: string];
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
    CGContextMoveToPoint(cgContext, p.x, p.y + lineHeight + 0.5);
    CGContextAddLineToPoint(cgContext, p.x + rect.size.width, p.y + lineHeight + 0.5);
    CGContextStrokePath(cgContext);

    [graphicsContext setShouldAntialias: flag];
    
#else
    id <KWQLayoutFragment>frag = [storage getFragmentForString: (NSString *)string];

    [self setColor: color];
    [textStorage setAttributes: [self attributes]];
    [textStorage setString: string];
    NSRect lineRect = [self lineFragmentRectForGlyphAtIndex: 0 effectiveRange: 0];
    [self underlineGlyphRange:[frag glyphRange] underlineType:NSSingleUnderlineStyle lineFragmentRect:lineRect lineFragmentGlyphRange:[frag glyphRange] containerOrigin:p];
#endif
}

+ (void)drawUnderlineForString: (NSString *)string atPoint: (NSPoint)p withFont: (NSFont *)aFont color: (NSColor *)color
{
    KWQLayoutInfo *layoutInfo = [KWQLayoutInfo getMetricsForFont: aFont];
    
    [layoutInfo drawUnderlineForString: string atPoint: p withFont: aFont color: color];
}


#ifdef _DEBUG_LAYOUT_FRAGMENT
+ (void)_dumpLayoutCache: (NSDictionary *)fragCache
{
    int i, count;
    NSArray *stringKeys;
    NSString *string;
    id <KWQLayoutFragment>fragment;

    if (fragCache == nil){
        fprintf (stdout, "Fragment cache empty\n");
        return;
    }
    fprintf (stdout, "  Hits   String\n");
    
    stringKeys = [fragCache keysSortedByValueUsingSelector:@selector(compare:)];
    count = [stringKeys count];
    for (i = 0; i < count; i++){
        string = [stringKeys objectAtIndex: i];
        fragment = [fragCache objectForKey: [stringKeys objectAtIndex: i]];
        fprintf (stdout, "  %06d \"%s\"\n", [fragment accessCount], [string cString]);
    }
}

+ (void)_dumpAllLayoutCaches
{
    int i, count;
    NSFont *font;
    NSArray *fontKeys;
    KWQLayoutInfo *layoutInfo;
    int totalObjects = 0;
    
    fontKeys = [metricsCache allKeys];
    count = [fontKeys count];
    for (i = 0; i < count; i++){
        font = [fontKeys objectAtIndex: i];
        layoutInfo = [metricsCache objectForKey: [fontKeys objectAtIndex: i]];
        fprintf (stdout, "Cache information for font %s %f (%d objects)\n", [[font displayName] cString],[font pointSize], [[[layoutInfo textStorage] fragmentCache] count]);
        [KWQLayoutInfo _dumpLayoutCache: [[layoutInfo textStorage] fragmentCache]];
        totalObjects += [[[layoutInfo textStorage] fragmentCache] count];
    }
    fprintf (stdout, "Total cached objects %d\n", totalObjects);
}

#endif


+ (KWQLayoutInfo *)getMetricsForFont: (NSFont *)aFont
{
    KWQLayoutInfo *info = (KWQLayoutInfo *)[metricsCache objectForKey: aFont];
    if (info == nil){
        info = [[KWQLayoutInfo alloc] initWithFont: aFont];
        [KWQLayoutInfo setMetric: info forFont: aFont];
        [info release];
    }
    return info;
}


+ (void)setMetric: (KWQLayoutInfo *)info forFont: (NSFont *)aFont
{
    if (metricsCache == nil)
        metricsCache = [[NSMutableDictionary alloc] init];
    [metricsCache setObject: info forKey: aFont];
}


- initWithFont: (NSFont *)aFont
{
    OSStatus errCode;

    [super init];
    attributes = [[NSMutableDictionary dictionaryWithObjectsAndKeys:aFont, NSFontAttributeName, nil] retain];

    font = [aFont retain];
    lineHeight = ROUND_TO_INT([font ascender]) - ROUND_TO_INT([font descender]) + 1;

#ifdef DIRECT_TO_CG
    if ((errCode = ATSUCreateStyle(&_style)) != noErr)
        [NSException raise:NSInternalInconsistencyException format:@"%@: Failed to alloc ATSUStyle %d", self, errCode];

    __IFFillStyleWithAttributes(_style, aFont);

    if ((errCode = ATSUGetStyleGroup(_style, &_styleGroup)) != noErr) {
        [NSException raise:NSInternalInconsistencyException format:@"%@: Failed to create attribute group from ATSUStyle 0x%X %d", self, _style, errCode];
    }            

    if ((errCode = ATSUCreateStyle(&_asciiStyle)) != noErr)
        [NSException raise:NSInternalInconsistencyException format:@"%@: Failed to alloc ATSUStyle %d", self, errCode];

    __IFFillStyleWithAttributes(_asciiStyle, aFont);

    if ((errCode = ATSUGetStyleGroup(_asciiStyle, &_asciiStyleGroup)) != noErr) {
        [NSException raise:NSInternalInconsistencyException format:@"%@: Failed to create attribute group from ATSUStyle 0x%X %d", self, _style, errCode];
    }            
#endif

    textStorage = [[KWQTextStorage alloc] initWithFontAttribute: attributes];
    layoutManager = [[NSLayoutManager alloc] init];

    [layoutManager addTextContainer: [KWQTextContainer sharedInstance]];
    [textStorage addLayoutManager: layoutManager];    

    return self;
}


- (NSLayoutManager *)layoutManager
{
    return layoutManager;
}


- (KWQTextStorage *)textStorage
{
    return textStorage;
}

#ifdef DIRECT_TO_CG
- (void)_initializeCaches
{
    int i, errorResult;
    size_t numGlyphsInFont = CGFontGetNumberOfGlyphs([font _backingCGSFont]);
    short unsigned int sequentialGlyphs[GLYPH_CACHE_MAX];
            
    if (numGlyphsInFont > GLYPH_CACHE_MAX)
        widthCacheSize = GLYPH_CACHE_MAX;
    else
        widthCacheSize = (int)numGlyphsInFont;

    for (i = 0; i < widthCacheSize; i++)
        sequentialGlyphs[i] = i;
        
    fprintf (stdout, "number of glyphs in font %s %f = %ld\n", [[font displayName] cString], [font pointSize], CGFontGetNumberOfGlyphs([font _backingCGSFont])); 
    widthCache = (float *)calloc (1, widthCacheSize * sizeof(float));
    errorResult = CGFontGetGlyphScaledAdvances ([font _backingCGSFont], &sequentialGlyphs[0], widthCacheSize, widthCache, [font pointSize]);
    if (errorResult == 0)
        [NSException raise:NSInternalInconsistencyException format:@"Optimization assumption violation:  unable to cache glyph advances - for %@ %f", self, [font displayName], [font pointSize]];


    unsigned int asciiCount = LAST_ASCII_CHAR - FIRST_ASCII_CHAR + 1;
    short unsigned int asciiBuffer[LAST_ASCII_CHAR+1];
    for (i = FIRST_ASCII_CHAR; i <= LAST_ASCII_CHAR; i++){
        asciiBuffer[i] = i;
    }
    
    __IFInitATSGlyphVector(&_asciiCacheGlyphVector, asciiCount);
    (void)ATSUConvertCharToGlyphs(_asciiStyleGroup, &asciiBuffer[FIRST_ASCII_CHAR], 0, asciiCount, 0, (ATSGlyphVector *)&_asciiCacheGlyphVector);
    if (_asciiCacheGlyphVector.numGlyphs != asciiCount)
        [NSException raise:NSInternalInconsistencyException format:@"Optimization assumption violation:  ascii and glyphID count not equal - for %@ %f", self, [font displayName], [font pointSize]];
}
#endif

#ifdef DIRECT_TO_CG
static NSRect _rectForString (KWQLayoutInfo *self, const UniChar *internalBuffer, unsigned int stringLength)
{
    CGContextRef cgContext;
    int totalWidth = 0;
    unsigned int i, numGlyphs, index;
    int glyphID;
    ATSLayoutRecord *glyphRecords, *glyphRecordsPtr;
    NSFont *font = [self font];
    BOOL needGlyphAdvanceLookup = NO;
    BOOL needCharToGlyphLookup = NO;


    if ([font glyphPacking] != NSNativeShortGlyphPacking &&
        [font glyphPacking] != NSTwoByteGlyphPacking)
	[NSException raise:NSInternalInconsistencyException format:@"%@: Don't know how to pack glyphs for font %@ %f", self, [font displayName], [font pointSize]];
        
    cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

    if (self->widthCache == 0)
        [self _initializeCaches];

    // Check if we can use the cached character-to-glyph map.  We only use the character-to-glyph map
    // if all the characters in the string fall in the safe ASCII range.
    for (i = 0; i < stringLength; i++){
        if (internalBuffer[i] < FIRST_ASCII_CHAR || internalBuffer[i] > LAST_ASCII_CHAR){
            //fprintf (stdout, "char to glyph cache miss because of character 0x%04x\n", internalBuffer[i]);
            needCharToGlyphLookup = YES;
            break;
        }
    }

    // If we can't use the cached map, the calculate a map for this string.
    if (needCharToGlyphLookup){
        __IFInitATSGlyphVector(&self->_glyphVector, stringLength);
        (void)ATSUConvertCharToGlyphs(self->_styleGroup, internalBuffer, 0, stringLength, 0, (ATSGlyphVector *)&self->_glyphVector);
        glyphRecords = self->_glyphVector.firstRecord;
        numGlyphs = self->_glyphVector.numGlyphs;
    }
    else {
        glyphRecords = self->_asciiCacheGlyphVector.firstRecord;
        numGlyphs = stringLength;
    }
    
    // Now we have the glyphs, determine if we can use the cached glyph measurements.
    for (i = 0; i < numGlyphs; i++){
        if (needCharToGlyphLookup)
            index = i;
        else
            index = internalBuffer[i]-FIRST_ASCII_CHAR;
        glyphID = glyphRecords[index].glyphID;
        if (glyphID > self->widthCacheSize){
            needGlyphAdvanceLookup = YES;
            break;
        }
    }

    // If we can't use the cached glyph measurement ask CG for the measurements.
    if (needGlyphAdvanceLookup){
        char localGlyphBuf[LOCAL_GLYPH_BUFFER_SIZE];
        char *usedGlyphBuf, *glyphBufPtr, *allocatedGlyphBuf = 0;

        fprintf (stdout, "glyph measurement cache miss\n");
        
        // Now construct the glyph buffer.
        if (numGlyphs > LOCAL_GLYPH_BUFFER_SIZE/2)
            usedGlyphBuf = glyphBufPtr = allocatedGlyphBuf = (char *)malloc (numGlyphs * 2);
        else
            usedGlyphBuf = glyphBufPtr = &localGlyphBuf[0];
                
        if (needCharToGlyphLookup){
            int glyphID;

            glyphRecordsPtr = glyphRecords;
            for (i = 0; i < numGlyphs; i++){
                glyphID = glyphRecordsPtr->glyphID;
                *glyphBufPtr++ = (char)((glyphID >> 8) & 0x00FF);
                *glyphBufPtr++ = (char)(glyphID & 0x00FF);
                glyphRecordsPtr++;
            }
        }
        else {
            int glyphID;
            
            for (i = 0; i < numGlyphs; i++){
                index = internalBuffer[i]-FIRST_ASCII_CHAR;
                glyphID = glyphRecords[index].glyphID;
                *glyphBufPtr++ = (char)((glyphID >> 8) & 0x00FF);
                *glyphBufPtr++ = (char)(glyphID & 0x00FF);
            }
        }
        
        float localAdvanceBuf[LOCAL_GLYPH_BUFFER_SIZE];
        float *usedAdvanceBuf, *allocatedAdvanceBuf = 0;

        if (numGlyphs > LOCAL_GLYPH_BUFFER_SIZE)
            usedAdvanceBuf = allocatedAdvanceBuf = (float *)malloc (numGlyphs * sizeof(float));
        else
            usedAdvanceBuf = &localAdvanceBuf[0];
        
        if (numGlyphs < LOCAL_GLYPH_BUFFER_SIZE){
            CGFontGetGlyphScaledAdvances ([font _backingCGSFont], (const short unsigned int *)usedGlyphBuf, numGlyphs, usedAdvanceBuf, [font pointSize]);
            for (i = 0; i < numGlyphs; i++){
                //totalWidth += ScaleEmToUnits (advance[i], info->unitsPerEm);
                totalWidth += ROUND_TO_INT(usedAdvanceBuf[i]);
            }
        }
        
        if (allocatedAdvanceBuf)
            free (allocatedAdvanceBuf);

        if (allocatedGlyphBuf)
            free (allocatedGlyphBuf);
    } 
    else {
        float *widthCache = self->widthCache;
        for (i = 0; i < numGlyphs; i++){
            if (needCharToGlyphLookup)
                index = i;
            else
                index = internalBuffer[i]-FIRST_ASCII_CHAR;
            totalWidth += ROUND_TO_INT(widthCache[glyphRecords[index].glyphID]);
        }
    }
    
    if (needCharToGlyphLookup)
        __IFResetATSGlyphVector(&self->_glyphVector);
            
    return NSMakeRect (0,0,(float)totalWidth, (float)self->lineHeight);
}
#endif


- (NSRect)rectForString:(NSString *)string
 {
#ifdef DIRECT_TO_CG
    UniChar localBuffer[LOCAL_GLYPH_BUFFER_SIZE];
    const UniChar *_internalBuffer = CFStringGetCharactersPtr ((CFStringRef)string);
    const UniChar *internalBuffer;

    if (!_internalBuffer){
        CFStringGetCharacters((CFStringRef)string, CFRangeMake(0, CFStringGetLength((CFStringRef)string)), &localBuffer[0]);
        internalBuffer = &localBuffer[0];
    }
    else
        internalBuffer = _internalBuffer;

    return _rectForString (self, internalBuffer, [string length]);
#else
    id <KWQLayoutFragment> cachedFragment;

    cachedFragment = [textStorage getFragmentForString: string];
    
    return [cachedFragment boundingRect];
#endif
}


- (void)setColor: (NSColor *)color
{
    [attributes setObject: color forKey: NSForegroundColorAttributeName];
}

- (NSDictionary *)attributes
{
    return attributes;
}

- (int)lineHeight
{
    return lineHeight;
}

- (NSFont *)font
{
    return font;
}

- (void)dealloc
{
    [font release];
    [attributes release];
    [super dealloc];
}

@end


struct QFontMetricsPrivate {
    QFontMetricsPrivate(NSFont *aFont)
    {
        refCount = 0;
        font = [aFont retain];
        info = nil;
        spaceWidth = -1;
        xWidth = -1;
        ascent = -1;
        descent = -1;
    }
    ~QFontMetricsPrivate()
    {
        [info release];
        [font release];
    }
    KWQLayoutInfo *getInfo();
    int refCount;
    NSFont *font;
    int spaceWidth;
    int xWidth;
    int ascent;
    int descent;
private:
    KWQLayoutInfo *info;
};

KWQLayoutInfo *QFontMetricsPrivate::getInfo()
{
    if (!info)
        info = [[KWQLayoutInfo getMetricsForFont:font] retain];
    return info;
}

QFontMetrics::QFontMetrics()
{
}

QFontMetrics::QFontMetrics(const QFont &withFont)
: data(new QFontMetricsPrivate(const_cast<QFont&>(withFont).getFont()))
{
}

QFontMetrics::QFontMetrics(const QFontMetrics &withFont)
: data(withFont.data)
{
}

QFontMetrics::~QFontMetrics()
{
}

QFontMetrics &QFontMetrics::operator=(const QFontMetrics &withFont)
{
    data = withFont.data;
    return *this;
}

int QFontMetrics::baselineOffset() const
{
    return ascent();
}

int QFontMetrics::ascent() const
{
    if (data->ascent < 0)
        data->ascent = ROUND_TO_INT([data->font ascender]);
    return data->ascent;
}

int QFontMetrics::descent() const
{
    if (data->descent < 0)
        data->descent = ROUND_TO_INT(-[data->font descender]);
    return data->descent;
}

int QFontMetrics::height() const
{
    // According to Qt documentation: 
    // "This is always equal to ascent()+descent()+1 (the 1 is for the base line)."
    return ascent() + descent() + 1;
}

int QFontMetrics::width(QChar qc) const
{
    unichar c = qc.unicode();
    switch (c) {
        // cheesy, we use the char version of width to do the work here,
        // and since it doesn't have the optimization, we don't get an
        // infinite loop
        case ' ':
            if (data->spaceWidth < 0)
                data->spaceWidth = width(' ');
            return data->spaceWidth;
        case 'x':
            if (data->xWidth < 0)
                data->xWidth = width('x');
            return data->xWidth;
    }
    return ROUND_TO_INT(_rectForString(data->getInfo(), &c, 1).size.width);
}

int QFontMetrics::width(char c) const
{
    unichar ch = (uchar) c;
    return ROUND_TO_INT(_rectForString(data->getInfo(), &ch, 1).size.width);
}

int QFontMetrics::width(const QString &qstring, int len) const
{
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = _FAST_QSTRING_TO_NSSTRING (qstring);
    return ROUND_TO_INT([data->getInfo() rectForString: string].size.width);
}

int QFontMetrics::_width(const UniChar *uchars, int len) const
{
    return ROUND_TO_INT(_rectForString(data->getInfo(), uchars, len).size.width);
}


int QFontMetrics::_width(CFStringRef string) const
{
    return ROUND_TO_INT([data->getInfo() rectForString: (NSString *)string].size.width);
}

QRect QFontMetrics::boundingRect(const QString &qstring, int len) const
{
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = _FAST_QSTRING_TO_NSSTRING (qstring);
    NSRect rect = [data->getInfo() rectForString: string];

    return QRect(ROUND_TO_INT(rect.origin.x),
            ROUND_TO_INT(rect.origin.y),
            ROUND_TO_INT(rect.size.width),
            ROUND_TO_INT(rect.size.height));
}

QRect QFontMetrics::boundingRect(QChar qc) const
{
    unichar c = qc.unicode();
    NSRect rect = _rectForString(data->getInfo(), &c, 1);

    return QRect(ROUND_TO_INT(rect.origin.x),
            ROUND_TO_INT(rect.origin.y),
            ROUND_TO_INT(rect.size.width),
            ROUND_TO_INT(rect.size.height));
}

QSize QFontMetrics::size(int, const QString &qstring, int len, int tabstops, 
    int *tabarray, char **intern ) const
{
    if (tabstops != 0){
        KWQDEBUGLEVEL(KWQ_LOG_ERROR, "ERROR:  QFontMetrics::size() tabs not supported.\n");
    }
    
    KWQDEBUG1("string = %s\n", DEBUG_OBJECT(QSTRING_TO_NSSTRING(qstring)));
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = _FAST_QSTRING_TO_NSSTRING (qstring);
    NSRect rect = [data->getInfo() rectForString: string];

    return QSize (ROUND_TO_INT(rect.size.width),ROUND_TO_INT(rect.size.height));
}

int QFontMetrics::rightBearing(QChar) const
{
    _logNotYetImplemented();
    return 0;
}

int QFontMetrics::leftBearing(QChar) const
{
    _logNotYetImplemented();
    return 0;
}
