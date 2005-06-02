/*	
    WebGlyphBuffer.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebGlyphBuffer.h"

#import "WebNSObjectExtras.h"

#import <CoreGraphics/CGContext.h>

@implementation WebGlyphBuffer

- initWithFont: (NSFont *)f color: (NSColor *)c
{
    [super init];
    font = [f retain];
    color = [c retain];
    return self;
}

- (NSFont *)font
{
    return font;
}

- (NSColor *)color
{
    return color;
}

- (void)reset
{
    if (bufferedAdvances){
        free (bufferedAdvances);
        bufferedAdvances = 0;
    }
    if (bufferedGlyphs){
        free (bufferedGlyphs);
        bufferedGlyphs = 0;
    }
    bufferSize = 0;
    bufferedCount = 0;
}

- (void)dealloc
{
    [font release];
    [color release];
    [self reset];
    [super dealloc];
}

- (void)finalize
{
    [self reset];
    [super finalize];
}

- (void)drawInView: (NSView *)targetView
{
    CGContextRef cgContext;
    
    [targetView lockFocus];
    cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    [color set];
    [font set];
    CGContextSetTextPosition (cgContext, bufferedStartX, bufferedStartY);
    CGContextShowGlyphsWithAdvances (cgContext, bufferedGlyphs, bufferedAdvances, bufferedCount);
    [targetView unlockFocus];
}


- (void)addGlyphs: (CGGlyph *)newGlyphs advances: (CGSize *)newAdvances count: (int)count at: (float)startX : (float)startY
{
    int sizeNeeded = bufferedCount + count;
    BOOL firstBuffer = NO;

    // Check to see if this is the first buffering pass.
    if (!bufferedAdvances){
        lastPositionX = bufferedStartX = startX;
        lastPositionY = bufferedStartY = startY;
        firstBuffer = YES;
    }
    
    if (sizeNeeded > bufferSize){
        if (bufferSize == 0)
            bufferSize = 512;	// 512*2=1024 is the min size, see below.
        int sizeToAllocate = MAX (bufferSize*2, sizeNeeded);
        if (bufferedAdvances)
            bufferedAdvances = (CGSize *)realloc(bufferedAdvances, sizeToAllocate * sizeof(CGSize));
        else
            bufferedAdvances = (CGSize *)malloc (sizeToAllocate * sizeof(CGSize));
        if (bufferedGlyphs)
            bufferedGlyphs = (CGGlyph *)realloc(bufferedGlyphs, sizeToAllocate * sizeof(CGGlyph));
        else
            bufferedGlyphs = (CGGlyph *)malloc (sizeToAllocate * sizeof(CGGlyph));
        bufferSize = sizeToAllocate;
    }
    
    if (firstBuffer == NO){
        bufferedAdvances[bufferedCount-1].width = startX - lastPositionX;
        bufferedAdvances[bufferedCount-1].height = startY - lastPositionY;
    }
    
    int i;
    lastPositionX = startX;
    lastPositionY = startY;
    for (i = 0; i < count-1; i++){
        lastPositionX += newAdvances[i].width;
        lastPositionY += newAdvances[i].height;
    }

    memcpy (&bufferedAdvances[bufferedCount], newAdvances, count * sizeof(CGSize));
    memcpy (&bufferedGlyphs[bufferedCount], newGlyphs, count * sizeof(CGGlyph));
    bufferedCount = bufferedCount + count;
}
@end

