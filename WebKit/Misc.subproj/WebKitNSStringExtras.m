/*
    WebKitNSStringExtras.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebKitNSStringExtras.h"

#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>

@implementation NSString (WebKitExtras)

- (void)_web_drawAtPoint:(NSPoint)point font:(NSFont *)font textColor:(NSColor *)textColor;
{
    WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:font usingPrinterFont:NO];
    unsigned length = [self length];
    UniChar *buffer = (UniChar *)malloc(sizeof(UniChar) * length);

    [self getCharacters:buffer];
    [renderer drawCharacters:buffer
                stringLength:length
       fromCharacterPosition:0
         toCharacterPosition:length
                     atPoint:point
                 withPadding:0
               withTextColor:textColor
             backgroundColor:nil
                 rightToLeft:NO
               letterSpacing:0
                 wordSpacing:0
                   smallCaps:false
                 fontFamilies:0];
    free(buffer);
}

- (float)_web_widthWithFont:(NSFont *)font
{
    unsigned length = [self length];
    float width;
    UniChar *buffer = (UniChar *)malloc(sizeof(UniChar) * length);

    [self getCharacters:buffer];
    WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:font usingPrinterFont:NO];
    width = [renderer _floatWidthForCharacters:buffer
                stringLength:length
                fromCharacterPosition: 0
                numberOfCharacters: length
                withPadding:0
                applyRounding: NO
                attemptFontSubstitution: YES
                widths: 0
                fonts: 0
                glyphs: 0
                numGlyphs: 0
                letterSpacing: 0
                wordSpacing: 0
                smallCaps: false
                fontFamilies: 0];
    free(buffer);
    
    return width;
}

@end
