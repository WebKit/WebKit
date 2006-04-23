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

#import <JavaScriptCore/Assertions.h>
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

// Have to leave these in because Safari calls them from its Debug menu.
+ (BOOL)shouldUseThreadedDecoding
{
    return NO;
}

+ (void)setShouldUseThreadedDecoding:(BOOL)threadedDecode
{
    // Do nothing. We don't support this now.
}

- (void)setPatternPhaseForContext:(CGContextRef)context inUserSpace:(CGPoint)point
{
    WKSetPatternPhaseInUserSpace(context, point);
}

- (id <WebCoreImageRenderer>)imageRendererWithMIMEType:(NSString *)MIMEType
{
    return [[[WebImageRenderer alloc] initWithMIMEType:MIMEType] autorelease];
}

- (id <WebCoreImageRenderer>)imageRenderer
{
    return [self imageRendererWithMIMEType:nil];
}

- (id <WebCoreImageRenderer>)imageRendererWithData:(NSData*)data MIMEType:(NSString *)MIMEType
{
    return [[[WebImageRenderer alloc] initWithData:data MIMEType:MIMEType] autorelease];
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
    return [[[WebImageRenderer alloc] initWithSize:s] autorelease];
}

- (id <WebCoreImageRenderer>)imageRendererWithName:(NSString *)name
{
    return [[[WebImageRenderer alloc] initWithContentsOfFile:name] autorelease];
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

- (NSData *)imageDataForName:(NSString *)filename
{
    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    NSString *imagePath = [bundle pathForResource:filename ofType:@"tiff"];
    NSData *data = [NSData dataWithContentsOfFile:imagePath];
    return data;
}


@end
