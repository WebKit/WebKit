//
//  WebNSImageExtras.m
//  WebKit
//
//  Created by Chris Blumenberg on Wed Sep 25 2002.
//  Copyright (c) 2002 Apple Computer Inc. All rights reserved.
//

#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSImageExtras.h>

@implementation NSImage (WebExtras)

- (void)_web_scaleToMaxSize:(NSSize)size
{
    float heightResizeDelta = 0.0, widthResizeDelta = 0.0, resizeDelta = 0.0;
    NSSize originalSize = [self size];

    if(originalSize.width > size.width){
        widthResizeDelta = size.width / originalSize.width;
        resizeDelta = widthResizeDelta;
    }

    if(originalSize.height > size.height){
        heightResizeDelta = size.height / originalSize.height;
        if((resizeDelta == 0.0) || (resizeDelta > heightResizeDelta)){
            resizeDelta = heightResizeDelta;
        }
    }
    
    if(resizeDelta > 0.0){
        NSSize newSize = NSMakeSize((originalSize.width * resizeDelta), (originalSize.height * resizeDelta));
        [self setScalesWhenResized:YES];
        [self setSize:newSize];
    }
}

- (void)_web_dissolveToFraction:(float)delta
{
    NSImage *dissolvedImage = [[[NSImage alloc] initWithSize:[self size]] autorelease];
    BOOL isFlipped = [self isFlipped];
    
    [self setFlipped:NO];
    
    [dissolvedImage lockFocus];
    [self dissolveToPoint:NSZeroPoint fraction: delta];
    [dissolvedImage unlockFocus];

    [self lockFocus];
    [dissolvedImage compositeToPoint:NSZeroPoint operation:NSCompositeCopy];
    [self unlockFocus];

    [self setFlipped:isFlipped];
}

@end
