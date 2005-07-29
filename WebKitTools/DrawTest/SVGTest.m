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

#import "SVGTest.h"

#import <WebCore+SVG/DrawView.h>
#import <WebCore+SVG/DrawDocument.h>

@implementation SVGTest

+ (id)testWithSVGPath:(NSString *)svgPath imagePath:(NSString *)imagePath
{
    SVGTest *test = [[self alloc] initWithSVGPath:svgPath imagePath:imagePath];
    return [test autorelease];
}

static DrawView *__sharedDrawView = nil;
+ (DrawView *)sharedDrawView
{
    if (!__sharedDrawView) {
        __sharedDrawView = [[DrawView alloc] initWithFrame:NSMakeRect(0,0,0,0)];
    }
    return __sharedDrawView;
}

- (id)initWithSVGPath:(NSString *)svgPath imagePath:(NSString *)imagePath
{
    if (self = [super init]) {
        _svgPath = [svgPath copy];
        _imagePath = [imagePath copy];
    }
    return self;
}

- (NSString *)imagePath
{
    return _imagePath;
}

- (NSString *)svgPath
{
    return _svgPath;
}

- (NSImage *)image
{
    if (!_image && _imagePath) {
        _image = [[NSImage alloc] initByReferencingFile:_imagePath];
    }
    return _image;
}

- (DrawDocument *)svgDocument
{
    if (!_svgDocument && _svgPath) {
        _svgDocument = [[DrawDocument alloc] initWithContentsOfFile:_svgPath];
    }
    return _svgDocument;
}

- (NSString *)name
{
    NSMutableString *name = [[[[_svgPath lastPathComponent] stringByDeletingPathExtension] mutableCopy] autorelease];
    [name replaceOccurrencesOfString:@"_" withString:@" " options:0 range:NSMakeRange(0, [name length])];
    return [name capitalizedString];
}

- (void)generateCompositeIfNecessary
{
    if (!_compositeImage) {
        DrawView *view = [SVGTest sharedDrawView];
        [view setDocument:[self svgDocument]];
        [view sizeToFitViewBox];
        NSSize svgSize = [view bounds].size;
        
        NSImage *image = [self image];
        NSSize imageSize = [image size];
        
        NSBitmapImageRep *svgImage = [view bitmapImageRepForCachingDisplayInRect:[view bounds]];
        [view cacheDisplayInRect:[view bounds] toBitmapImageRep:svgImage];
        
        NSSize unionSize = NSMakeSize(MAX(svgSize.width, imageSize.width), MAX(svgSize.height, imageSize.height));
        _compositeImage = [[NSImage alloc] initWithSize:unionSize];
        
        [_compositeImage lockFocus];
        [svgImage drawInRect:NSMakeRect(0,0,svgSize.width,svgSize.height)];
        [image drawInRect:NSMakeRect(0,0,imageSize.width,imageSize.height)
                fromRect:NSMakeRect(0,0,imageSize.width,imageSize.height)
                 operation:NSCompositeXOR fraction:1.0];
        [_compositeImage unlockFocus];
    }
}

- (NSImage *)compositeImage
{
    [self generateCompositeIfNecessary];
    return _compositeImage;
}


@end
