/*
    WebKitNSStringExtras.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebKitNSStringExtras.h"

#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>

#import <WebCore/WebCoreUnicode.h>

@implementation NSString (WebKitExtras)

- (void)_web_drawAtPoint:(NSPoint)point font:(NSFont *)font textColor:(NSColor *)textColor;
{
    unsigned i, length = [self length];
    UniChar *buffer = (UniChar *)malloc(sizeof(UniChar) * length);
    BOOL fastRender = YES;

    [self getCharacters:buffer];
    
    // Check if this string only contains normal, and left-to-right characters.
    // If not hand the string over to the appkit for slower but correct rendering.
    for (i = 0; i < length; i++){
        WebCoreUnicodeDirection direction = WebCoreUnicodeDirectionFunction (buffer[i]);
        if (direction == DirectionR || direction > DirectionON){
            fastRender = NO;
            break;
        }
    }

    if (fastRender){
        WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:font usingPrinterFont:NO];
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
    }
    else {
        // WebTextRenderer assumes drawing from baseline.
        point.y -= [font ascender];
        [self drawAtPoint:point withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, textColor, NSForegroundColorAttributeName, nil]];
    }

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
