/*
 * Copyright (C) 2002 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageRenderer.h>

#import <WebKit/WebAssertions.h>
#import <Foundation/NSURLFileTypeMappings.h>

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
    NSImage *imageRenderer = [[WebImageRenderer alloc] initWithMIMEType:MIMEType];

    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initForIncrementalLoad];
    [imageRenderer addRepresentation:rep];
    [rep autorelease];
    [imageRenderer setFlipped:YES];
    
    // Turn the default caching mode back on when the image has completed load.
    // Caching intermediate progressive representations causes problems for
    // progressive loads.  We also rely on the NSBitmapImageRep surviving during
    // incremental loads.  See 3165631 and 3262592.
    [imageRenderer setCacheMode: NSImageCacheNever];

    [imageRenderer setScalesWhenResized:NO];
        
    return [imageRenderer autorelease];
}

- (id <WebCoreImageRenderer>)imageRenderer
{
    return [self imageRendererWithMIMEType:nil];
}

- (id <WebCoreImageRenderer>)imageRendererWithData:(NSData*)data MIMEType:(NSString *)MIMEType
{
    WebImageRenderer *imageRenderer = [[WebImageRenderer alloc] initWithData:data MIMEType:MIMEType];

    NSArray *reps = [imageRenderer representations];
    if ([reps count] == 0){
        [imageRenderer release];
        return nil;
    }
    
    // Force the image to use the pixel size and ignore the dpi.
    [imageRenderer setScalesWhenResized:NO];
    if ([reps count] > 0){
        NSImageRep *rep = [reps objectAtIndex:0];
        [rep setSize:NSMakeSize([rep pixelsWide], [rep pixelsHigh])];
        if ([imageRenderer frameCount] > 1)
            imageRenderer->originalData = [data retain];
    }
    
    [imageRenderer setFlipped:YES];

    return [imageRenderer autorelease];
}

- (id <WebCoreImageRenderer>)imageRendererWithBytes:(const void *)bytes length:(unsigned)length MIMEType:(NSString *)MIMEType
{
    NSData *data = [[NSData alloc] initWithBytes:(void *)bytes length:length];
    WebImageRenderer *imageRenderer = [self imageRendererWithData:data MIMEType:MIMEType];
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
    [imageRenderer setScalesWhenResized:NO];
    return imageRenderer;
}

- (id <WebCoreImageRenderer>)imageRendererWithName:(NSString *)name
{
    WebImageRenderer *imageRenderer = [[[WebImageRenderer alloc] initWithContentsOfFile:name] autorelease];
    [imageRenderer setScalesWhenResized:NO];
    [imageRenderer setFlipped:YES];
    return imageRenderer;
}


- (NSArray *)supportedMIMETypes
{
    static NSArray *imageMIMETypes = nil;

    if(!imageMIMETypes){
        NSArray *unsupportedTypes = [NSArray arrayWithObjects:
            @"application/pdf",
            @"application/postscript",
            nil];
        
        NSEnumerator *enumerator = [[NSImage imageFileTypes] objectEnumerator];
        NSURLFileTypeMappings *mappings = [NSURLFileTypeMappings sharedMappings];
        NSMutableSet *mimes = [NSMutableSet set];
        NSString *type;

        while ((type = [enumerator nextObject]) != nil) {
            NSString *mime = [mappings MIMETypeForExtension:type];
            if(mime && ![mime isEqualToString:@"application/octet-stream"] &&
               ![unsupportedTypes containsObject:mime]){
                [mimes addObject:mime];
            }
        }

        imageMIMETypes = [[mimes allObjects] retain];
    }

    return imageMIMETypes;
}


@end
