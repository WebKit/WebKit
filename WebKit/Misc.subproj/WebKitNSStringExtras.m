/*
    WebKitNSStringExtras.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebKitNSStringExtras.h"

#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>

#import <WebCore/WebCoreUnicode.h>

@implementation NSString (WebKitExtras)

static BOOL canUseFastRenderer (const UniChar *buffer, unsigned length)
{
    unsigned i;
    for (i = 0; i < length; i++){
        WebCoreUnicodeDirection direction = WebCoreUnicodeDirectionFunction (buffer[i]);
        if (direction == DirectionR || direction > DirectionON){
            return NO;
        }
    }
    return YES;
}

- (void)_web_drawAtPoint:(NSPoint)point font:(NSFont *)font textColor:(NSColor *)textColor;
{
    unsigned length = [self length];
    UniChar *buffer = (UniChar *)malloc(sizeof(UniChar) * length);

    [self getCharacters:buffer];
    
    if (canUseFastRenderer(buffer, length)){
        WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:font usingPrinterFont:NO];

        WebCoreTextRun run = WebCoreMakeTextRun (buffer, length, 0, length);
        WebCoreTextStyle style = WebCoreMakeEmptyTextStyle();
        style.textColor = textColor;
        [renderer drawRun:run style:style atPoint:point];
    }
    else {
        // WebTextRenderer assumes drawing from baseline.
        point.y -= [font ascender];
        [self drawAtPoint:point withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, textColor, NSForegroundColorAttributeName, nil]];
    }

    free(buffer);
}

- (void)_web_drawDoubledAtPoint:(NSPoint)textPoint
             withTopColor:(NSColor *)topColor
              bottomColor:(NSColor *)bottomColor
                     font:(NSFont *)font
{
    // turn off font smoothing so translucent text draws correctly (Radar 3118455)
    [NSGraphicsContext saveGraphicsState];
    CGContextSetShouldSmoothFonts([[NSGraphicsContext currentContext] graphicsPort], false);
    [self _web_drawAtPoint:textPoint
                      font:font
                 textColor:bottomColor];

    textPoint.y += 1;
    [self _web_drawAtPoint:textPoint
                      font:font
                 textColor:topColor];
    [NSGraphicsContext restoreGraphicsState];
}

- (float)_web_widthWithFont:(NSFont *)font
{
    unsigned length = [self length];
    float width;
    UniChar *buffer = (UniChar *)malloc(sizeof(UniChar) * length);

    [self getCharacters:buffer];

    if (canUseFastRenderer(buffer, length)){
        WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:font usingPrinterFont:NO];

        WebCoreTextRun run = WebCoreMakeTextRun (buffer, length, 0, length);
        WebCoreTextStyle style = WebCoreMakeEmptyTextStyle();
        width = [renderer floatWidthForRun:run style:style
                    applyRounding: NO
                    attemptFontSubstitution: YES
                    widths: 0];
    }
    else {
        width = [self sizeWithAttributes:[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, nil]].width;
    }
    
    free(buffer);
    
    return width;
}

- (NSString *)_web_stringByAbbreviatingWithTildeInPath
{
    NSString *resolvedHomeDirectory = [NSHomeDirectory() stringByResolvingSymlinksInPath];
    NSString *path;
    
    if ([self hasPrefix:resolvedHomeDirectory]) {
        NSString *relativePath = [self substringFromIndex:[resolvedHomeDirectory length]];
        path = [NSHomeDirectory() stringByAppendingPathComponent:relativePath];
    } else {
        path = self;
    }
        
    return [path stringByAbbreviatingWithTildeInPath];
}

@end
