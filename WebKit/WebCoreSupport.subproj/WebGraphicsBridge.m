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

#import "WebGraphicsBridge.h"

#import <HIServices/CoreDrag.h>
#import <HIServices/CoreDragPriv.h>

#import "WebAssertions.h"

#ifndef USE_APPEARANCE
#define USE_APPEARANCE 1
#endif
#import <AppKit/NSInterfaceStyle_Private.h>
#import <AppKit/NSWindow_Private.h>
#import <AppKit/NSView_Private.h>
#import <CoreGraphics/CGContextGState.h>
#import <CoreGraphics/CGStyle.h>

#import "WebImageRenderer.h"

@interface NSView (AppKitSecretsWebGraphicsBridgeKnowsAbout)
- (NSView *)_clipViewAncestor;
@end

@implementation WebGraphicsBridge

+ (void)createSharedBridge
{
    if (![self sharedBridge]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedBridge] isKindOfClass:self]);
}

+ (WebGraphicsBridge *)sharedBridge;
{
    return (WebGraphicsBridge *)[super sharedBridge];
}

- (void)setFocusRingStyle:(NSFocusRingPlacement)placement radius:(int)radius color:(NSColor *)color
{
    // get clip bounds
    NSView *clipView = [[NSView focusView] _clipViewAncestor];
    NSRect clipRect = [clipView convertRect:[clipView _focusRingVisibleRect] toView:nil];
    
#if SCALE
    if (__NSHasDisplayScaleFactor(NULL)) {
        float scaleFactor = [[view window] _scaleFactor];
        clipRect.size.width *= scaleFactor;
        clipRect.size.height *= scaleFactor;
    }
#endif
    
    // put together the style
    CGFocusRingStyle focusRingStyle;
    CGStyleRef focusRingStyleRef;
    focusRingStyle.version    = 0;
    focusRingStyle.ordering   = (CGFocusRingOrdering)placement;
    focusRingStyle.alpha      = .5;
    focusRingStyle.radius     = radius ? radius : kCGFocusRingRadiusDefault;
    focusRingStyle.threshold  = kCGFocusRingThresholdDefault;
    focusRingStyle.bounds     = *(CGRect*)&clipRect;
    focusRingStyle.accumulate = 0;
    focusRingStyle.tint = _NSDefaultControlTint() == NSBlueControlTint ? kCGFocusRingTintBlue : kCGFocusRingTintGraphite;
    
    NSColor *ringColor = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
    if (ringColor) {
        float c[4];
        [ringColor getRed:&c[0] green:&c[1] blue:&c[2] alpha:&c[3]];
        CGColorSpaceRef colorSpace = WebCGColorSpaceCreateRGB();
        CGColorRef colorRef = CGColorCreate(colorSpace, c);
        CGColorSpaceRelease(colorSpace);
        focusRingStyleRef = CGStyleCreateFocusRingWithColor(&focusRingStyle, colorRef);
        CGColorRelease(colorRef);
    }
    else {
        focusRingStyleRef = CGStyleCreateFocusRing(&focusRingStyle);
    }
    CGContextSetStyle([[NSGraphicsContext currentContext] graphicsPort], focusRingStyleRef);
    CGStyleRelease(focusRingStyleRef);
}

static void FlipImageSpec(CoreDragImageSpec* imageSpec) {    
    // run through and swap each row. we'll need a temporary row to hold the swapped row
    //
    unsigned char* tempRow = malloc(imageSpec->bytesPerRow);
    int            planes  = imageSpec->isPlanar ? imageSpec->samplesPerPixel : 1;
    
    int p;
    for (p = 0; p < planes; p++) {
	unsigned char* topRow = (unsigned char*)imageSpec->data[p];
	unsigned char* botRow = topRow + (imageSpec->pixelsHigh - 1) * imageSpec->bytesPerRow;
	int i;
	for (i = 0; i < imageSpec->pixelsHigh / 2; i++, topRow += imageSpec->bytesPerRow, botRow -= imageSpec->bytesPerRow) {
	    bcopy(topRow,  tempRow, imageSpec->bytesPerRow);
	    bcopy(botRow,  topRow,  imageSpec->bytesPerRow);
	    bcopy(tempRow, botRow,  imageSpec->bytesPerRow);
	}
    }
    free(tempRow);
}

// Dashboard wants to set the drag image during dragging, but Cocoa does not allow this.  Instead we drop
// down to the CG API.  Converting an NSImage to a CGImageSpec is copied from NSDragManager.
- (void)setDraggingImage:(NSImage *)image at:(NSPoint)offset
{
    NSSize 		imageSize = [image size];
    CGPoint             imageOffset = {-offset.x, -(imageSize.height - offset.y)};
    CGRect		imageRect= CGRectMake(0, 0, imageSize.width, imageSize.height);
    NSBitmapImageRep 	*bitmapImage;
    CoreDragImageSpec	imageSpec;
    CGSRegionObj 	imageShape;
    BOOL                flipImage;
    OSStatus		error;
    
    // if the image contains an NSBitmapImageRep, we are done
    bitmapImage = (NSBitmapImageRep *)[image bestRepresentationForDevice:nil];    
    if (bitmapImage == nil || ![bitmapImage isKindOfClass:[NSBitmapImageRep class]] || !NSEqualSizes([bitmapImage size], imageSize)) {
        // otherwise we need to render the image and get the bitmap data from it
        [image lockFocus];
        bitmapImage = [[NSBitmapImageRep alloc] initWithFocusedViewRect:*(NSRect *)&imageRect];
        [image unlockFocus];
        
	// we may have to flip the bits we just read if the iamge was flipped since it means the cache was also
	// and CoreDragSetImage can't take a transform for rendering.
	flipImage = [image isFlipped];
        
    } else {
        flipImage = NO;
        [bitmapImage retain];
    }
    ASSERT_WITH_MESSAGE(bitmapImage, "dragging image does not contain bitmap");
    
    imageSpec.version = kCoreDragImageSpecVersionOne;
    imageSpec.pixelsWide = [bitmapImage pixelsWide];
    imageSpec.pixelsHigh = [bitmapImage pixelsHigh];
    imageSpec.bitsPerSample = [bitmapImage bitsPerSample];
    imageSpec.samplesPerPixel = [bitmapImage samplesPerPixel];
    imageSpec.bitsPerPixel = [bitmapImage bitsPerPixel];
    imageSpec.bytesPerRow = [bitmapImage bytesPerRow];
    imageSpec.isPlanar = [bitmapImage isPlanar];
    imageSpec.hasAlpha = [bitmapImage hasAlpha];
    [bitmapImage getBitmapDataPlanes:(unsigned char **)imageSpec.data];
    
    // if image was flipped, we have an upside down bitmap since the cache is rendered flipped
    //
    if (flipImage) {
	FlipImageSpec(&imageSpec);
    }
    
    error = CGSNewRegionWithRect(&imageRect, &imageShape);
    ASSERT_WITH_MESSAGE(error == kCGErrorSuccess, "Error getting shape for image: %d", error);
    if (error != kCGErrorSuccess) {
        [bitmapImage release];
        return;
    }
    
    // make sure image has integer offset
    //
    imageOffset.x = floor(imageOffset.x + 0.5);
    imageOffset.y = floor(imageOffset.y + 0.5);
    
    // TODO: what is overallAlpha for window?
    error = CoreDragSetImage(CoreDragGetCurrentDrag(), imageOffset, &imageSpec, imageShape, 1.0);
    CGSReleaseRegion(imageShape);
    ASSERT_WITH_MESSAGE(error == kCGErrorSuccess, "Error setting image for drag: %d", error);
    
    [bitmapImage release];

    // Hack:  We must post an event to wake up the NSDragManager, which is sitting in a nextEvent call
    // up the stack from us because the CF drag manager is too lame to use the RunLoop by itself.  This
    // is the most innocuous event, per Kristen.
    
    NSEvent *ev = [NSEvent mouseEventWithType:NSMouseMoved location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:0 pressure:0];
    [NSApp postEvent:ev atStart:YES];
}

- (void)setAdditionalPatternPhase:(NSPoint)phase
{
    _phase = phase;
}

- (NSPoint)additionalPatternPhase {
    return _phase;
}


- (CGColorSpaceRef)createRGBColorSpace
{
    return WebCGColorSpaceCreateRGB();
}

- (CGColorSpaceRef)createGrayColorSpace
{
    return WebCGColorSpaceCreateGray();
}

- (CGColorSpaceRef)createCMYKColorSpace
{
    return WebCGColorSpaceCreateCMYK();
}

@end
