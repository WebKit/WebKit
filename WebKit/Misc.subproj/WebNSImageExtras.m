//
//  WebNSImageExtras.m
//  WebKit
//
//  Created by Chris Blumenberg on Wed Sep 25 2002.
//  Copyright (c) 2002 Apple Computer Inc. All rights reserved.
//

#import <WebKit/WebNSImageExtras.h>

#import <WebKit/WebKitLogging.h>

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
    NSImage *dissolvedImage = [[NSImage alloc] initWithSize:[self size]];

    NSPoint point = [self isFlipped] ? NSMakePoint(0, [self size].height) : NSZeroPoint;
    
    // In this case the dragging image is always correct.
    [dissolvedImage setFlipped:[self isFlipped]];

    [dissolvedImage lockFocus];
    [self dissolveToPoint:point fraction: delta];
    [dissolvedImage unlockFocus];

    [self lockFocus];
    [dissolvedImage compositeToPoint:point operation:NSCompositeCopy];
    [self unlockFocus];

    [dissolvedImage release];
}

- (void)_web_saveAndOpen
{
    char path[] = "/tmp/XXXXXX.tiff";
    int fd = mkstemps(path, 5);
    if (fd != -1) {
        NSData *data = [self TIFFRepresentation];
        write(fd, [data bytes], [data length]);
        close(fd);
        [[NSWorkspace sharedWorkspace] openFile:[NSString stringWithCString:path]];
    }
}

@end
