//
//  WebStringTruncator.m
//
//  Created by Darin Adler on Fri May 10 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//
//  Complete rewrite with API similar to slow truncator by Al Dul

#import <WebKit/WebStringTruncator.h>

#import <Cocoa/Cocoa.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebTextRenderer.h>

#define STRING_BUFFER_SIZE 2048
#define ELLIPSIS_CHARACTER 0x2026

static NSFont *currentFont;
static WebTextRenderer *currentRenderer;
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

static float stringWidth(WebTextRenderer *renderer, const unichar *characters, unsigned length)
{
    WebCoreTextRun run;
    WebCoreInitializeTextRun (&run, characters, length, 0, length);
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.applyRunRounding = NO;
    style.applyWordRounding = NO;
    return [renderer floatWidthForRun:&run style:&style widths:0];
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
        [WebTextRendererFactory createSharedFactory];
        currentRenderer = [[[WebTextRendererFactory sharedFactory] rendererWithFont:font usingPrinterFont:NO] retain];
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
        keepCount = maxWidth * ratio;
        
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

@implementation WebStringTruncator

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
    unichar *s = malloc(sizeof(unichar) * length);
    [string getCharacters:s];
    float width = stringWidth([[WebTextRendererFactory sharedFactory] rendererWithFont:font usingPrinterFont:NO], s, length);
    free(s);
    return width;
}

@end
