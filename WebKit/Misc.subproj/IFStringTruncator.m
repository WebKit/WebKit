//
//  IFStringTruncator.m
//
//  Created by Darin Adler on Fri May 10 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//
//  Complete rewrite with API similar to slow truncator by Al Dul

#import <WebKit/IFStringTruncator.h>
#import <Cocoa/Cocoa.h>

#import <WebKit/WebKitDebug.h>
#import <WebKit/IFTextRendererFactory.h>
#import <WebKit/IFTextRenderer.h>

#define STRING_BUFFER_SIZE 2048
#define ELLIPSIS_CHARACTER 0x2026

static NSFont *currentFont;
static IFTextRenderer *currentRenderer;
static float currentEllipsisWidth;

@implementation IFStringTruncator

+ (unsigned)centerTruncateString:(NSString *)string length:(unsigned)length keepCount:(unsigned)keepCount toBuffer:(unichar *)buffer
{
    unsigned omitStart;
    NSRange omitEndRange, beforeRange, afterRange;
    unsigned truncatedLength;
    
    WEBKIT_ASSERT(keepCount < STRING_BUFFER_SIZE);
    
    omitStart = (keepCount + 1) / 2;
    
    omitEndRange = [string rangeOfComposedCharacterSequenceAtIndex:omitStart + (length - keepCount) - 1];
    
    beforeRange.location = 0;
    beforeRange.length = [string rangeOfComposedCharacterSequenceAtIndex:omitStart].location;
    
    afterRange.location = omitEndRange.location + omitEndRange.length;
    afterRange.length = length - afterRange.location;
    
    truncatedLength = beforeRange.length + 1 + afterRange.length;
    WEBKIT_ASSERT(truncatedLength <= length);

    [string getCharacters:buffer range:beforeRange];
    buffer[beforeRange.length] = ELLIPSIS_CHARACTER;
    [string getCharacters:&buffer[beforeRange.length + 1] range:afterRange];
    
    return truncatedLength;
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth
{
    return [self centerTruncateString:string toWidth:maxWidth withFont:[NSFont menuFontOfSize:0]];
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
    
    WEBKIT_ASSERT_VALID_ARG(font, font);
    WEBKIT_ASSERT_VALID_ARG(maxWidth, maxWidth >= 0);
    
    if (![currentFont isEqual:font]) {
        [currentFont release];
        currentFont = [font retain];
        [currentRenderer release];
        [IFTextRendererFactory createSharedFactory];
        currentRenderer = [[[IFTextRendererFactory sharedFactory] rendererWithFont:font] retain];
        ellipsis = ELLIPSIS_CHARACTER;
        currentEllipsisWidth = [currentRenderer floatWidthForCharacters:&ellipsis length:1 applyRounding: NO];
    }
    
    WEBKIT_ASSERT(currentRenderer);

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
                                              length:truncatedLength applyRounding: NO];
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
        WEBKIT_ASSERT(widthForLargestKnownToFit <= maxWidth);
        WEBKIT_ASSERT(widthForSmallestKnownToNotFit > maxWidth);

        ratio = (keepCountForSmallestKnownToNotFit - keepCountForLargestKnownToFit)
            / (widthForSmallestKnownToNotFit - widthForLargestKnownToFit);
        keepCount = maxWidth * ratio;
        
        if (keepCount <= keepCountForLargestKnownToFit) {
            keepCount = keepCountForLargestKnownToFit + 1;
        } else if (keepCount >= keepCountForSmallestKnownToNotFit) {
            keepCount = keepCountForSmallestKnownToNotFit - 1;
        }
        
        WEBKIT_ASSERT(keepCount < length);
        WEBKIT_ASSERT(keepCount > 0);
        WEBKIT_ASSERT(keepCount < keepCountForSmallestKnownToNotFit);
        WEBKIT_ASSERT(keepCount > keepCountForLargestKnownToFit);
        
	truncatedLength = [self centerTruncateString:string
                                              length:length
                                           keepCount:keepCount
                                            toBuffer:stringBuffer];
        
        width = [currentRenderer floatWidthForCharacters:stringBuffer length:truncatedLength applyRounding: NO];
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
