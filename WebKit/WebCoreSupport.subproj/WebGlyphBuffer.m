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

