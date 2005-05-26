/*
    WebKitNSStringExtras.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebKitNSStringExtras.h"

#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>

#import <Foundation/NSString_NSURLExtras.h>
#import <unicode/uchar.h>

@implementation NSString (WebKitExtras)

static BOOL canUseFastRenderer(const UniChar *buffer, unsigned length)
{
    unsigned i;
    for (i = 0; i < length; i++) {
        UCharDirection direction = u_charDirection(buffer[i]);
        if (direction == U_RIGHT_TO_LEFT || direction > U_WHITE_SPACE_NEUTRAL) {
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

        WebCoreTextRun run;
        WebCoreInitializeTextRun (&run, buffer, length, 0, length);
        WebCoreTextStyle style;
        WebCoreInitializeEmptyTextStyle(&style);
        style.applyRunRounding = NO;
        style.applyWordRounding = NO;
        style.textColor = textColor;
        WebCoreTextGeometry geometry;
        WebCoreInitializeEmptyTextGeometry(&geometry);
        geometry.point = point;
        [renderer drawRun:&run style:&style geometry:&geometry];
    }
    else {
        // WebTextRenderer assumes drawing from baseline.
        if ([[NSView focusView] isFlipped])
            point.y -= [font ascender];
        else {
            point.y += [font descender];
        }
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

        WebCoreTextRun run;
        WebCoreInitializeTextRun (&run, buffer, length, 0, length);
        WebCoreTextStyle style;
        WebCoreInitializeEmptyTextStyle(&style);
        style.applyRunRounding = NO;
        style.applyWordRounding = NO;
        width = [renderer floatWidthForRun:&run style:&style widths: 0];
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

- (NSString *)_web_stringByStrippingReturnCharacters
{
    NSMutableString *newString = [[self mutableCopy] autorelease];
    [newString replaceOccurrencesOfString:@"\r" withString:@"" options:NSLiteralSearch range:NSMakeRange(0, [newString length])];
    [newString replaceOccurrencesOfString:@"\n" withString:@"" options:NSLiteralSearch range:NSMakeRange(0, [newString length])];
    return newString;
}

+ (NSStringEncoding)_web_encodingForResource:(Handle)resource
{
    short resRef = HomeResFile(resource);
    if (ResError() != noErr) {
        return NSMacOSRomanStringEncoding;
    }
    
    // Get the FSRef for the current resource file
    FSRef fref;
    OSStatus error = FSGetForkCBInfo(resRef, 0, NULL, NULL, NULL, &fref, NULL);
    if (error != noErr) {
        return NSMacOSRomanStringEncoding;
    }
    
    CFURLRef URL = CFURLCreateFromFSRef(NULL, &fref);
    if (URL == NULL) {
        return NSMacOSRomanStringEncoding;
    }
    
    NSString *path = [(NSURL *)URL path];
    CFRelease(URL);
    
    // Get the lproj directory name
    path = [path stringByDeletingLastPathComponent];
    if (![[path pathExtension] _web_isCaseInsensitiveEqualToString:@"lproj"]) {
        return NSMacOSRomanStringEncoding;
    }
    
    NSString *directoryName = [[path stringByDeletingPathExtension] lastPathComponent];
    CFStringRef locale = CFLocaleCreateCanonicalLocaleIdentifierFromString(NULL, (CFStringRef)directoryName);
    if (locale == NULL) {
        return NSMacOSRomanStringEncoding;
    }
            
    LangCode lang;
    RegionCode region;
    error = LocaleStringToLangAndRegionCodes([(NSString *)locale UTF8String], &lang, &region);
    CFRelease(locale);
    if (error != noErr) {
        return NSMacOSRomanStringEncoding;
    }
    
    TextEncoding encoding;
    error = UpgradeScriptInfoToTextEncoding(kTextScriptDontCare, lang, region, NULL, &encoding);
    if (error != noErr) {
        return NSMacOSRomanStringEncoding;
    }
    
    return CFStringConvertEncodingToNSStringEncoding(encoding);
}

@end
