//
//  WebNSImageExtras.m
//  WebKit
//
//  Created by Chris Blumenberg on Wed Sep 25 2002.
//  Copyright (c) 2002 Apple Computer Inc. All rights reserved.
//

#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSImageExtras.h>

// see +load for details
@interface NSBitmapImageRep (SPINeededForJagGreen)
+ (void)_setEnableFlippedImageFix:(BOOL)f;
@end
static BOOL AKBugIsFixed = NO;


@implementation NSImage (WebExtras)

+ (void)load
{
    // See 3094525, 3065097, 2767424.  We need to call this SPI to get it fixed in JagGreen.
    // Pre-green we're SOL.  In Panther the fix is always on.
    //
    // FIXME 3125264: We can nuke this code, the category above, and half the code in
    // _web_dissolveToFraction when we drop Jag support.
    //
    if ([[NSBitmapImageRep class] respondsToSelector:@selector(_setEnableFlippedImageFix:)]) {
        [NSBitmapImageRep _setEnableFlippedImageFix:YES];
        AKBugIsFixed = YES;
    }
}

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

    if (AKBugIsFixed) {
        NSPoint point = [self isFlipped] ? NSMakePoint(0, [self size].height) : NSZeroPoint;
        
        // In this case the dragging image is always correct.
        [dissolvedImage setFlipped:[self isFlipped]];

        [dissolvedImage lockFocus];
        [self dissolveToPoint:point fraction: delta];
        [dissolvedImage unlockFocus];

        [self lockFocus];
        [dissolvedImage compositeToPoint:point operation:NSCompositeCopy];
        [self unlockFocus];
    } else {
        // In this case Thousands mode will have an inverted drag image.  Millions is OK.
        // FIXME 3125264: this branch of code can go when we drop Jaguar support.
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
    [dissolvedImage release];
}

@end
