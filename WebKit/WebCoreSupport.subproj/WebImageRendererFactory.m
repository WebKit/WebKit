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

#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageRenderer.h>

#import <WebKit/WebAssertions.h>
#import <WebKitSystemInterface.h>

@implementation WebImageRendererFactory

+ (void)createSharedFactory
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

+ (WebImageRendererFactory *)sharedFactory
{
    return (WebImageRendererFactory *)[super sharedFactory];
}

- (id <WebCoreImageRenderer>)imageRendererWithMIMEType:(NSString *)MIMEType
{
    WebImageRenderer *imageRenderer = [[WebImageRenderer alloc] initWithMIMEType:MIMEType];

#ifndef USE_CGIMAGEREF
    NSImage *image = [imageRenderer image];

    if (![MIMEType isEqual:@"application/pdf"]) {
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initForIncrementalLoad];
        [image addRepresentation:rep];
        [rep autorelease];
    }

    [image setFlipped:YES];
    
    // Turn the default caching mode back on when the image has completed load.
    // Caching intermediate progressive representations causes problems for
    // progressive loads.  We also rely on the NSBitmapImageRep surviving during
    // incremental loads.  See 3165631 and 3262592.
    [image setCacheMode: NSImageCacheNever];

    [image setScalesWhenResized:NO];
#endif
        
    return [imageRenderer autorelease];
}

- (id <WebCoreImageRenderer>)imageRenderer
{
    return [self imageRendererWithMIMEType:nil];
}

- (id <WebCoreImageRenderer>)imageRendererWithData:(NSData*)data MIMEType:(NSString *)MIMEType
{
    WebImageRenderer *imageRenderer = [[WebImageRenderer alloc] initWithData:data MIMEType:MIMEType];

#ifndef USE_CGIMAGEREF
    NSImage *image = [imageRenderer image];

    NSArray *reps = [image representations];
    if ([reps count] == 0){
        [imageRenderer release];
        return nil;
    }
    
    // Force the image to use the pixel size and ignore the dpi.
    [image setScalesWhenResized:NO];
    if ([reps count] > 0){
        NSImageRep *rep = [reps objectAtIndex:0];
        [rep setSize:NSMakeSize([rep pixelsWide], [rep pixelsHigh])];
        if ([imageRenderer frameCount] > 1)
            [imageRenderer setOriginalData:data];
    }
    
    [image setFlipped:YES];
#endif

    return [imageRenderer autorelease];
}

- (id <WebCoreImageRenderer>)imageRendererWithBytes:(const void *)bytes length:(unsigned)length MIMEType:(NSString *)MIMEType
{
    NSData *data = [[NSData alloc] initWithBytes:(void *)bytes length:length];
    WebImageRenderer *imageRenderer = (WebImageRenderer *)[self imageRendererWithData:data MIMEType:MIMEType];
    [data autorelease];
    return imageRenderer;
}

- (id <WebCoreImageRenderer>)imageRendererWithBytes:(const void *)bytes length:(unsigned)length
{
    return [self imageRendererWithBytes:bytes length:length MIMEType:nil];
}

- (id <WebCoreImageRenderer>)imageRendererWithSize:(NSSize)s
{
    WebImageRenderer *imageRenderer = [[[WebImageRenderer alloc] initWithSize:s] autorelease];
#ifndef USE_CGIMAGEREF
    [[imageRenderer image] setScalesWhenResized:NO];
#endif
    return imageRenderer;
}

- (id <WebCoreImageRenderer>)imageRendererWithName:(NSString *)name
{
    WebImageRenderer *imageRenderer = [[[WebImageRenderer alloc] initWithContentsOfFile:name] autorelease];
#ifndef USE_CGIMAGEREF
    [[imageRenderer image] setScalesWhenResized:NO];
    [[imageRenderer image] setFlipped:YES];
#endif
    return imageRenderer;
}

struct CompositeOperator
{
    NSString *name;
    NSCompositingOperation value;
};

#define NUM_COMPOSITE_OPERATORS 14
struct CompositeOperator NSCompositingOperations[NUM_COMPOSITE_OPERATORS] = {
    { @"clear", NSCompositeClear },
    { @"copy", NSCompositeCopy },
    { @"source-over", NSCompositeSourceOver },
    { @"source-in", NSCompositeSourceIn },
    { @"source-out", NSCompositeSourceOut },
    { @"source-atop", NSCompositeSourceAtop },
    { @"destination-over", NSCompositeDestinationOver },
    { @"destination-in", NSCompositeDestinationIn },
    { @"destination-out", NSCompositeDestinationOut },
    { @"destination-atop", NSCompositeDestinationAtop },
    { @"xor", NSCompositeXOR },
    { @"darker", NSCompositePlusDarker },
    { @"highlight", NSCompositeHighlight },
    { @"lighter", NSCompositeHighlight }    // Per AppKit
};

- (int)CGCompositeOperationInContext:(CGContextRef)context
{
	return [[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO] compositingOperation];
}

- (void)setCGCompositeOperation:(int)op inContext:(CGContextRef)context
{
	[[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO] setCompositingOperation:op];
}

- (void)setCGCompositeOperationFromString:(NSString *)operatorString inContext:(CGContextRef)context
{
    NSCompositingOperation op = NSCompositeSourceOver;
    
    if (operatorString) {
        int i;
        
        for (i = 0; i < NUM_COMPOSITE_OPERATORS; i++) {
            if ([operatorString caseInsensitiveCompare:NSCompositingOperations[i].name] == NSOrderedSame) {
                op = NSCompositingOperations[i].value;
                break;
            }
        }
    }
    
	[[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO] setCompositingOperation:op];
}


- (NSArray *)supportedMIMETypes
{
    static NSArray *imageMIMETypes = nil;

    if (imageMIMETypes == nil) {
        NSEnumerator *enumerator = [[NSImage imageFileTypes] objectEnumerator];
        NSMutableSet *mimes = [NSMutableSet set];
        NSString *type;

        while ((type = [enumerator nextObject]) != nil) {
            NSString *mime = WKGetMIMETypeForExtension(type);
            if (mime != nil && ![mime isEqualToString:@"application/octet-stream"]) {
                [mimes addObject:mime];
            }
        }

        // image/pjpeg is the MIME type for progressive jpeg. These files have the jpg file extension.
        // This is workaround of NSURLFileTypeMappings's limitation of only providing 1 MIME type for an extension.
        [mimes addObject:@"image/pjpeg"];
        
        imageMIMETypes = [[mimes allObjects] retain];
    }

    return imageMIMETypes;
}


@end
