//
//  WebStringTruncator.m
//
//  Created by Darin Adler on Fri May 10 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//
//  Complete rewrite with API similar to slow truncator by Al Dul

#import <WebKit/WebStringTruncator.h>
#import <Cocoa/Cocoa.h>

#import <WebFoundation/WebAssertions.h>
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
    return [renderer floatWidthForCharacters:characters
                                stringLength:length
                       fromCharacterPosition:0
                          numberOfCharacters:length
                                 withPadding:0
                               applyRounding:NO
                     attemptFontSubstitution:YES
                                      widths:0
                               letterSpacing:0
                                 wordSpacing:0
                                fontFamilies:0];
}

static NSString *truncateString(NSString *string, float maxWidth, NSFont *font, TruncationFunction truncateToBuffer)
{
    unichar stringBuffer[STRING_BUFFER_SIZE];
    unsigned length;
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

    length = [string length];
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

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth
{
    return truncateString(string, maxWidth, [NSFont menuFontOfSize:0], centerTruncateToBuffer);
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font
{
    return truncateString(string, maxWidth, font, centerTruncateToBuffer);
}

+ (NSString *)rightTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font
{
    return truncateString(string, maxWidth, font, rightTruncateToBuffer);
}

@end
