/*
    WebKitNSStringExtras.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebKitNSStringExtras.h"

#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>

@implementation NSString (WebKitExtras)

- (void)_web_drawString:(NSString *)string atPoint:(NSPoint)point font: (NSFont *)font textColor:(NSColor *)textColor;
{
    if (string == nil) {
        return;
    }
    
    WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:font];
    unsigned length = [string length];
    UniChar *buffer = (UniChar *)malloc(sizeof(UniChar) * length);

    [string getCharacters:buffer];
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
                 wordSpacing:0];
    free(buffer);
}

- (float)_web_widthForString:(NSString *)string font: (NSFont *)font
{
    if (string == nil)
        return 0;
        
    unsigned length = [string length];
    float width;
    UniChar *buffer = (UniChar *)malloc(sizeof(UniChar) * length);

    [string getCharacters:buffer];
    WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:font];
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
                wordSpacing: 0];
    free(buffer);
    
    return width;
}

@end
