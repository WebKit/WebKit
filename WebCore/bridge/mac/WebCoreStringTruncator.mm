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

#import "config.h"
#import "WebCoreStringTruncator.h"

#import <Cocoa/Cocoa.h>

#import <kxmlcore/Assertions.h>
#import "WebTextRendererFactory.h"
#import "WebTextRenderer.h"

#define STRING_BUFFER_SIZE 2048
#define ELLIPSIS_CHARACTER 0x2026

static NSFont *currentFont;
static WebTextRenderer* currentRenderer;
static float currentEllipsisWidth;

typedef unsigned TruncationFunction(NSString *string, unsigned length, unsigned keepCount, unichar *buffer);

static unsigned centerTruncateToBuffer(NSString *string, unsigned length, unsigned keepCount, unichar *buffer)
{
    ASSERT(keepCount < length);
    ASSERT(keepCount < STRING_BUFFER_SIZE);
    
    unsigned omitStart = (keepCount + 1) / 2;
    unsigned omitEnd = NSMaxRange([string rangeOfComposedCharacterSequenceAtIndex:omitStart + (length - keepCount) - 1]);
    omitStart = [string rangeOfComposedCharacterSequenceAtIndex:omitStart].location;
    
    NSRange beforeRange = NSMakeRange(0, omitStart);
    NSRange afterRange = NSMakeRange(omitEnd, length - omitEnd);
    
    unsigned truncatedLength = beforeRange.length + 1 + afterRange.length;
    ASSERT(truncatedLength <= length);

    [string getCharacters:buffer range:beforeRange];
    buffer[beforeRange.length] = ELLIPSIS_CHARACTER;
    [string getCharacters:&buffer[beforeRange.length + 1] range:afterRange];
    
    return truncatedLength;
}

static unsigned rightTruncateToBuffer(NSString *string, unsigned length, unsigned keepCount, unichar *buffer)
{
    ASSERT(keepCount < length);
    ASSERT(keepCount < STRING_BUFFER_SIZE);
    
    NSRange keepRange = NSMakeRange(0, [string rangeOfComposedCharacterSequenceAtIndex:keepCount].location);
    
    [string getCharacters:buffer range:keepRange];
    buffer[keepRange.length] = ELLIPSIS_CHARACTER;
    
    return keepRange.length + 1;
}

static float stringWidth(WebTextRenderer* renderer, const unichar *characters, unsigned length)
{
    WebCoreTextRun run;
    WebCoreInitializeTextRun (&run, characters, length, 0, length);
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.applyRunRounding = NO;
    style.applyWordRounding = NO;
    return [renderer floatWidthForRun:&run style:&style];
}

static NSString *truncateString(NSString *string, float maxWidth, NSFont *font, TruncationFunction truncateToBuffer)
{
    unsigned length = [string length];
    if (length == 0) {
        return string;
    }

    unichar stringBuffer[STRING_BUFFER_SIZE];
    unsigned keepCount;
    unsigned truncatedLength;
    float width;
    unichar ellipsis;
    unsigned keepCountForLargestKnownToFit, keepCountForSmallestKnownToNotFit;
    float widthForLargestKnownToFit, widthForSmallestKnownToNotFit;
    float ratio;
    
    ASSERT_ARG(font, font);
    ASSERT_ARG(maxWidth, maxWidth >= 0);
    
    if (![currentFont isEqual:font]) {
        [currentFont release];
        currentFont = [font retain];
        [currentRenderer release];
        WebCoreFont f;
        WebCoreInitializeFont(&f);
        f.font = font;
        currentRenderer = [[[WebTextRendererFactory sharedFactory] rendererWithFont:f] retain];
        ellipsis = ELLIPSIS_CHARACTER;
        currentEllipsisWidth = stringWidth(currentRenderer, &ellipsis, 1);
    }
    
    ASSERT(currentRenderer);

    if (length > STRING_BUFFER_SIZE) {
        keepCount = STRING_BUFFER_SIZE - 1; // need 1 character for the ellipsis
        truncatedLength = centerTruncateToBuffer(string, length, keepCount, stringBuffer);
    } else {
        keepCount = length;
        [string getCharacters:stringBuffer];
        truncatedLength = length;
    }

    width = stringWidth(currentRenderer, stringBuffer, truncatedLength);
    if (width <= maxWidth) {
        return string;
    }

    keepCountForLargestKnownToFit = 0;
    widthForLargestKnownToFit = currentEllipsisWidth;
    
    keepCountForSmallestKnownToNotFit = keepCount;
    widthForSmallestKnownToNotFit = width;
    
    if (currentEllipsisWidth >= maxWidth) {
        keepCountForLargestKnownToFit = 1;
        keepCountForSmallestKnownToNotFit = 2;
    }
    
    while (keepCountForLargestKnownToFit + 1 < keepCountForSmallestKnownToNotFit) {
        ASSERT(widthForLargestKnownToFit <= maxWidth);
        ASSERT(widthForSmallestKnownToNotFit > maxWidth);

        ratio = (keepCountForSmallestKnownToNotFit - keepCountForLargestKnownToFit)
            / (widthForSmallestKnownToNotFit - widthForLargestKnownToFit);
        keepCount = static_cast<unsigned>(maxWidth * ratio);
        
        if (keepCount <= keepCountForLargestKnownToFit) {
            keepCount = keepCountForLargestKnownToFit + 1;
        } else if (keepCount >= keepCountForSmallestKnownToNotFit) {
            keepCount = keepCountForSmallestKnownToNotFit - 1;
        }
        
        ASSERT(keepCount < length);
        ASSERT(keepCount > 0);
        ASSERT(keepCount < keepCountForSmallestKnownToNotFit);
        ASSERT(keepCount > keepCountForLargestKnownToFit);
        
        truncatedLength = truncateToBuffer(string, length, keepCount, stringBuffer);

        width = stringWidth(currentRenderer, stringBuffer, truncatedLength);
        if (width <= maxWidth) {
            keepCountForLargestKnownToFit = keepCount;
            widthForLargestKnownToFit = width;
        } else {
            keepCountForSmallestKnownToNotFit = keepCount;
            widthForSmallestKnownToNotFit = width;
        }
    }
    
    if (keepCountForLargestKnownToFit == 0) {
        keepCountForLargestKnownToFit = 1;
    }
    
    if (keepCount != keepCountForLargestKnownToFit) {
        keepCount = keepCountForLargestKnownToFit;
        truncatedLength = truncateToBuffer(string, length, keepCount, stringBuffer);
    }
    
    return [NSString stringWithCharacters:stringBuffer length:truncatedLength];
}

@implementation WebCoreStringTruncator

static NSFont *defaultMenuFont(void)
{
    static NSFont *defaultMenuFont = nil;
    if (defaultMenuFont == nil) {
        defaultMenuFont = [[NSFont menuFontOfSize:0] retain];
    }
    return defaultMenuFont;
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth
{
    return truncateString(string, maxWidth, defaultMenuFont(), centerTruncateToBuffer);
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font
{
    return truncateString(string, maxWidth, font, centerTruncateToBuffer);
}

+ (NSString *)rightTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font
{
    return truncateString(string, maxWidth, font, rightTruncateToBuffer);
}

+ (float)widthOfString:(NSString *)string font:(NSFont *)font
{
    unsigned length = [string length];
    unichar *s = static_cast<unichar*>(malloc(sizeof(unichar) * length));
    [string getCharacters:s];
    WebCoreFont f;
    WebCoreInitializeFont(&f);
    f.font = font;
    float width = stringWidth([[WebTextRendererFactory sharedFactory] rendererWithFont:f], s, length);
    free(s);
    return width;
}

@end
