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
#import <Cocoa/Cocoa.h>

#import <WebKit/IFImageRendererFactory.h>
#import <WebKit/IFImageRenderer.h>
#import <WebKit/WebKitDebug.h>

@implementation IFImageRendererFactory

+ (void)createSharedFactory
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    WEBKIT_ASSERT([[self sharedFactory] isMemberOfClass:self]);
}

+ (IFImageRendererFactory *)sharedFactory
{
    return (IFImageRendererFactory *)[super sharedFactory];
}

- (id <WebCoreImageRenderer>)imageRenderer
{
    NSImage *imageRenderer = [[IFImageRenderer alloc] init];

    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initForIncrementalLoad];
    [imageRenderer addRepresentation: rep];
    [rep release];
    [imageRenderer setFlipped: YES];

    [imageRenderer setScalesWhenResized: NO];
    return [imageRenderer autorelease];
}


- (id <WebCoreImageRenderer>)imageRendererWithBytes: (const void *)bytes length:(unsigned)length
{
    // FIXME:  Why must we copy the data here?
    //NSData *data = [[NSData alloc] initWithBytesNoCopy: (void *)bytes length: length freeWhenDone: NO];
    NSData *data = [[NSData alloc] initWithBytes: (void *)bytes length: length];
    IFImageRenderer *imageRenderer = [[IFImageRenderer alloc] initWithData: data];
    [imageRenderer setScalesWhenResized: NO];
    NSArray *reps = [imageRenderer representations];
    NSImageRep *rep = [reps objectAtIndex: 0];
    // Force the image to use the pixel size and ignore the dpi.
    [rep setSize:NSMakeSize([rep pixelsWide], [rep pixelsHigh])];
    [data release];
    [imageRenderer setFlipped: YES];
    return [imageRenderer autorelease];
}

- (id <WebCoreImageRenderer>)imageRendererWithSize: (NSSize)s
{
    IFImageRenderer *imageRenderer = [[[IFImageRenderer alloc] initWithSize: s] autorelease];
    [imageRenderer setScalesWhenResized: NO];
    return imageRenderer;
}


@end
