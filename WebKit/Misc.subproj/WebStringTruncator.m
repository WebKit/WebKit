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

@implementation WebStringTruncator

+ (unsigned)centerTruncateString:(NSString *)string length:(unsigned)length keepCount:(unsigned)keepCount toBuffer:(unichar *)buffer
{
    unsigned omitStart;
    NSRange omitEndRange, beforeRange, afterRange;
    unsigned truncatedLength;
    
    ASSERT(keepCount < STRING_BUFFER_SIZE);
    
    omitStart = (keepCount + 1) / 2;
    
    omitEndRange = [string rangeOfComposedCharacterSequenceAtIndex:omitStart + (length - keepCount) - 1];
    
    beforeRange.location = 0;
    beforeRange.length = [string rangeOfComposedCharacterSequenceAtIndex:omitStart].location;
    
    afterRange.location = omitEndRange.location + omitEndRange.length;
    afterRange.length = length - afterRange.location;
    
    truncatedLength = beforeRange.length + 1 + afterRange.length;
    ASSERT(truncatedLength <= length);

    [string getCharacters:buffer range:beforeRange];
    buffer[beforeRange.length] = ELLIPSIS_CHARACTER;
    [string getCharacters:&buffer[beforeRange.length + 1] range:afterRange];
    
    return truncatedLength;
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth
{
    return [self centerTruncateString:string toWidth:maxWidth withFont:[NSFont menuFontOfSize:0]];
}

+ (NSString *)rightTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font
{
    unichar stringBuffer[STRING_BUFFER_SIZE];
    WebTextRenderer *renderer;
    unichar ellipsis;
    float ellipsisWidth;
    float width;
    unsigned truncatedLength = [string length];

    ASSERT (truncatedLength+1 < STRING_BUFFER_SIZE);
    // FIXME:  Allocate buffer is string doesn't fit in local buffer.
    
    [string getCharacters:stringBuffer];
    renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:font];
    width = [renderer floatWidthForCharacters:stringBuffer
                                              stringLength:truncatedLength fromCharacterPosition: 0 numberOfCharacters: truncatedLength withPadding: 0 applyRounding: NO attemptFontSubstitution: YES widths: 0];
    if (width <= maxWidth)
        return string;

    ellipsis = ELLIPSIS_CHARACTER;
    ellipsisWidth = [renderer floatWidthForCharacters:&ellipsis stringLength:1 fromCharacterPosition: 0 numberOfCharacters: 1 withPadding: 0 applyRounding: NO attemptFontSubstitution: YES widths: 0];

    maxWidth -= ellipsisWidth;
    while (width > maxWidth && truncatedLength){	
        truncatedLength--;
        width = [renderer floatWidthForCharacters:stringBuffer
                                              stringLength:truncatedLength fromCharacterPosition: 0 numberOfCharacters: truncatedLength withPadding: 0 applyRounding: NO attemptFontSubstitution: YES widths: 0];
    }

    stringBuffer[truncatedLength++] = ELLIPSIS_CHARACTER; 
    
    return [NSString stringWithCharacters:stringBuffer length:truncatedLength];
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font
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
        currentRenderer = [[[WebTextRendererFactory sharedFactory] rendererWithFont:font] retain];
        ellipsis = ELLIPSIS_CHARACTER;
        currentEllipsisWidth = [currentRenderer floatWidthForCharacters:&ellipsis stringLength:1 fromCharacterPosition: 0 numberOfCharacters: 1 withPadding: 0 applyRounding: NO attemptFontSubstitution: YES widths: 0];
    }
    
    ASSERT(currentRenderer);

    length = [string length];
    if (length > STRING_BUFFER_SIZE) {
        keepCount = STRING_BUFFER_SIZE - 1; // need 1 character for the ellipsis
        truncatedLength = [self centerTruncateString:string
                                             length:length
                                          keepCount:keepCount
                                           toBuffer:stringBuffer];
    } else {
        keepCount = length;
        [string getCharacters:stringBuffer];
        truncatedLength = length;
    }

    width = [currentRenderer floatWidthForCharacters:stringBuffer
                                              stringLength:truncatedLength fromCharacterPosition: 0 numberOfCharacters: truncatedLength withPadding: 0 applyRounding: NO attemptFontSubstitution: YES widths: 0];
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
        
	truncatedLength = [self centerTruncateString:string
                                              length:length
                                           keepCount:keepCount
                                            toBuffer:stringBuffer];
        
        width = [currentRenderer floatWidthForCharacters:stringBuffer stringLength:truncatedLength fromCharacterPosition: 0 numberOfCharacters: truncatedLength withPadding: 0 applyRounding: NO attemptFontSubstitution: YES widths: 0];
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
	truncatedLength = [self centerTruncateString:string
                                              length:length
                                           keepCount:keepCount
                                            toBuffer:stringBuffer];
    }
        
    return [NSString stringWithCharacters:stringBuffer length:truncatedLength];
}

@end
