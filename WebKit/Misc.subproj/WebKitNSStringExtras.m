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

#import "WebKitNSStringExtras.h"

#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebNSFileManagerExtras.h>

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
    
    if (canUseFastRenderer(buffer, length)) {
        WebCoreFont f;
        WebCoreInitializeFont(&f);
        f.font = font;
        WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:f];

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

    if (canUseFastRenderer(buffer, length)) {
        WebCoreFont f;
        WebCoreInitializeFont(&f);
        f.font = font;
        WebTextRenderer *renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:f];

        WebCoreTextRun run;
        WebCoreInitializeTextRun (&run, buffer, length, 0, length);
        WebCoreTextStyle style;
        WebCoreInitializeEmptyTextStyle(&style);
        style.applyRunRounding = NO;
        style.applyWordRounding = NO;
        width = [renderer floatWidthForRun:&run style:&style];
    } else {
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
    if (![[path pathExtension] _webkit_isCaseInsensitiveEqualToString:@"lproj"]) {
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

- (BOOL)_webkit_isCaseInsensitiveEqualToString:(NSString *)string
{
  return [self compare:string options:(NSCaseInsensitiveSearch|NSLiteralSearch)] == NSOrderedSame;
}

-(BOOL)_webkit_hasCaseInsensitivePrefix:(NSString *)prefix
{
    return [self rangeOfString:prefix options:(NSCaseInsensitiveSearch | NSAnchoredSearch)].location != NSNotFound;
}

-(BOOL)_webkit_hasCaseInsensitiveSuffix:(NSString *)suffix
{
    return [self rangeOfString:suffix options:(NSCaseInsensitiveSearch | NSBackwardsSearch | NSAnchoredSearch)].location != NSNotFound;
}

-(BOOL)_webkit_hasCaseInsensitiveSubstring:(NSString *)substring
{
    return [self rangeOfString:substring options:NSCaseInsensitiveSearch].location != NSNotFound;
}

-(NSString *)_webkit_filenameByFixingIllegalCharacters
{
    NSMutableString *filename = [[self mutableCopy] autorelease];

    // Strip null characters.
	unichar nullChar = 0;
    [filename replaceOccurrencesOfString:[NSString stringWithCharacters:&nullChar length:0] withString:@"" options:0 range:NSMakeRange(0, [filename length])];

    // Replace "/" with "-".
    [filename replaceOccurrencesOfString:@"/" withString:@"-" options:0 range:NSMakeRange(0, [filename length])];

    // Replace ":" with "-".
    [filename replaceOccurrencesOfString:@":" withString:@"-" options:0 range:NSMakeRange(0, [filename length])];
    
    // Strip leading dots.
    while ([filename hasPrefix:@"."]) {
        [filename deleteCharactersInRange:NSMakeRange(0,1)];
    }
    
    return filename;
}

-(NSString *)_webkit_stringByTrimmingWhitespace
{
    NSMutableString *trimmed = [[self mutableCopy] autorelease];
    CFStringTrimWhitespace((CFMutableStringRef)trimmed);
    return trimmed;
}

- (NSString *)_webkit_stringByCollapsingNonPrintingCharacters
{
    NSMutableString *result = [NSMutableString string];
    static NSCharacterSet *charactersToTurnIntoSpaces = nil;
    static NSCharacterSet *charactersToNotTurnIntoSpaces = nil;
    
    if (charactersToTurnIntoSpaces == nil) {
        NSMutableCharacterSet *set = [[NSMutableCharacterSet alloc] init];
        [set addCharactersInRange:NSMakeRange(0x00, 0x21)];
        [set addCharactersInRange:NSMakeRange(0x7F, 0x01)];
        charactersToTurnIntoSpaces = [set copy];
        [set release];
        charactersToNotTurnIntoSpaces = [[charactersToTurnIntoSpaces invertedSet] retain];
    }
    
    unsigned length = [self length];
    unsigned position = 0;
    while (position != length) {
        NSRange nonSpace = [self rangeOfCharacterFromSet:charactersToNotTurnIntoSpaces
            options:0 range:NSMakeRange(position, length - position)];
        if (nonSpace.location == NSNotFound) {
            break;
        }

        NSRange space = [self rangeOfCharacterFromSet:charactersToTurnIntoSpaces
            options:0 range:NSMakeRange(nonSpace.location, length - nonSpace.location)];
        if (space.location == NSNotFound) {
            space.location = length;
        }

        if (space.location > nonSpace.location) {
            if (position != 0) {
                [result appendString:@" "];
            }
            [result appendString:[self substringWithRange:
                NSMakeRange(nonSpace.location, space.location - nonSpace.location)]];
        }

        position = space.location;
    }
    
    return result;
}

-(NSString *)_webkit_fixedCarbonPOSIXPath
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:self]) {
        // Files exists, no need to fix.
        return self;
    }

    NSMutableArray *pathComponents = [[[self pathComponents] mutableCopy] autorelease];
    NSString *volumeName = [pathComponents objectAtIndex:1];
    if ([volumeName isEqualToString:@"Volumes"]) {
        // Path starts with "/Volumes", so the volume name is the next path component.
        volumeName = [pathComponents objectAtIndex:2];
        // Remove "Volumes" from the path because it may incorrectly be part of the path (3163647).
        // We'll add it back if we have to.
        [pathComponents removeObjectAtIndex:1];
    }

    if (!volumeName) {
        // Should only happen if self == "/", so this shouldn't happen because that always exists.
        return self;
    }

    if ([[fileManager _webkit_startupVolumeName] isEqualToString:volumeName]) {
        // Startup volume name is included in path, remove it.
        [pathComponents removeObjectAtIndex:1];
    } else if ([[fileManager directoryContentsAtPath:@"/Volumes"] containsObject:volumeName]) {
        // Path starts with other volume name, prepend "/Volumes".
        [pathComponents insertObject:@"Volumes" atIndex:1];
    } else {
        // It's valid.
        return self;
    }

    NSString *path = [NSString pathWithComponents:pathComponents];

    if (![fileManager fileExistsAtPath:path]) {
        // File at canonicalized path doesn't exist, return original.
        return self;
    }

    return path;
}

@end
