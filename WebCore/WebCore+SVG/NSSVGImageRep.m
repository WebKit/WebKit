/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "NSSVGImageRep.h"

#import <WebCore+SVG/DrawDocumentPrivate.h>
#import <WebCore+SVG/DrawView.h>

static NSString *SVGDataPasteboardType = @"SVGDataPasteboardType";

@implementation NSSVGImageRep

+ (void)load
{
    // Register this ImageRep subclass right when WebCore+SVG is loaded.
    [NSImageRep registerImageRepClass:self];
}

+ (BOOL)canInitWithData:(NSData *)data
{
    if (!data)
        return NO;
    
    // This tests to see if the first 1k of the file has the string "svg"
    // If it doesn't, we assume this is not an SVG file
    // Clearly this doesn't work with svgz files.
    int length = [data length];
    if (length > 1024)
        length = 1024;
    NSString *testString = [[NSString alloc] initWithBytes:[data bytes] length:length encoding:NSUTF8StringEncoding];
    NSRange range = [testString rangeOfString:@"svg" options:NSCaseInsensitiveSearch];
    [testString release];
    if ((range.location > 0) && (range.length == 3))
        return YES;
    
    return NO;
}

+ (BOOL)canInitWithPasteboard:(NSPasteboard *)pasteboard
{
    NSString *type = [pasteboard availableTypeFromArray:[self imageUnfilteredPasteboardTypes]];
    if ([type isEqualToString:NSFilenamesPboardType]) {
        NSArray *names = [pasteboard propertyListForType:NSFilenamesPboardType];
        NSEnumerator *filenameEnumerator = [names objectEnumerator];
        NSString *filename = nil;
        while ((filename = [filenameEnumerator nextObject])) {
            if ([[filename pathExtension] isEqualToString:@"svg"])
                return YES;
        }
    }
    return (type != nil);
}

+ (NSArray *)imageUnfilteredFileTypes
{
    static NSArray *types = nil;
    if (!types)
        types = [[NSArray alloc]  initWithObjects:@"svg", nil];
    return types;
}

+ (NSArray *)imageUnfilteredPasteboardTypes
{
    static NSArray *types = nil;
    if (!types)
        types = [[NSArray alloc] initWithObjects:SVGDataPasteboardType, NSFilenamesPboardType, nil];
    return types;
}

- (BOOL)draw
{
    NSLog(@"NSSVGImageRep draw");
    return [_cachedRepHack draw];;
}

- (BOOL)drawAtPoint:(NSPoint)point
{
    NSLog(@"NSSVGImageRep drawAtPoint: %@", NSStringFromPoint(point));
    return [_cachedRepHack drawAtPoint:point];
}

- (BOOL)drawInRect:(NSRect)rect
{
    NSLog(@"NSSVGImageRep drawInRect: %@", NSStringFromRect(rect));
    return [_cachedRepHack drawInRect:rect];
}

+ (id)imageRepWithData:(NSData *)svgData
{
    return [[[self alloc] initWithData:svgData] autorelease];
}

- (id)initWithData:(NSData *)svgData
{
    NSLog(@"NSSVGImageRep initWithData");
    if ((self = [super init])) {
        _drawDocument = [[DrawDocument alloc] initWithSVGData:svgData];
        if (!_drawDocument) {
            [self release];
            return nil;
        }
        
        [self setAlpha:YES];
        //[self setBitsPerSample:32];
        [self setColorSpaceName:NSCalibratedRGBColorSpace];
        [self setOpaque:NO];
        NSSize documentSize = [_drawDocument canvasSize];
        [self setSize:documentSize];
        [self setPixelsWide:(int)documentSize.width];
        [self setPixelsHigh:(int)documentSize.height];
        
        _view = [[DrawView alloc] initWithFrame:NSMakeRect(0,0,documentSize.width,documentSize.width)];
        [_view setDocument:_drawDocument];
        [_view sizeToFitViewBox];
        
        // Drawing at other than 0,0, or at some zooms was not working correctly
        // when I hacked this class together.  Hence the temporary
        // "Convert to a NSBitmapImageRep and let it do everything" hack.
        // This should be fixed once the rendering logic settles down a bit more.
        // Currently drawing NSSVGImageReps at scaled sizes is very ugly as a result.
        _cachedRepHack = [[_view bitmapImageRepForCachingDisplayInRect:[_view bounds]] retain];
        [_view cacheDisplayInRect:[_view bounds] toBitmapImageRep:_cachedRepHack];
    }
    return self;
}

- (NSData *)representationUsingType:(NSBitmapImageFileType)storageType properties:(NSDictionary *)properties
{
    return [_cachedRepHack representationUsingType:storageType properties:properties];
}

- (void)setSize:(NSSize)newSize
{
    NSLog(@"NSSVGImageRep setSize:");
    [super setSize:newSize];
}

@end
