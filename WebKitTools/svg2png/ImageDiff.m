/*
 * Copyright (C) 2005 Ben La Monica <ben.lamonica@gmail.com>.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#import <QuartzCore/CIFilter.h>
#import <QuartzCore/CIImage.h>
#import <QuartzCore/CIContext.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSCIImageRep.h>

#import "ImageDiff.h"

/**
 * Takes a bitmap and performs a difference blend with a reference 
 * image. If the images are exactly the same, the diffBitmap will 
 * be solid black.
 *
 * It will write this difference blended bitmap to a file.
 * 
 * Returns the difference bitmap
 */
NSBitmapImageRep *getDifferenceBitmap(NSBitmapImageRep *testBitmap, NSBitmapImageRep *referenceBitmap)
{
    // create a new graphics context to draw our CIImage on
    NSBitmapImageRep *diffBitmap = [testBitmap copy];
    NSGraphicsContext *nsContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:diffBitmap];
    CIImage *referenceImage = [[CIImage alloc] initWithBitmapImageRep:referenceBitmap];
    CIImage *testImage = [[CIImage alloc] initWithBitmapImageRep:testBitmap];
    CIImage *diffImage = nil;
    CIFilter *diffBlend = [CIFilter filterWithName:@"CIDifferenceBlendMode"];

    // generate the diff image
    [diffBlend setValue:referenceImage forKey:@"inputImage"];
    [diffBlend setValue:testImage forKey:@"inputBackgroundImage"];
    diffImage = [diffBlend valueForKey:@"outputImage"];
    
    // prepare to draw the image, save current state
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext: nsContext];
    
    // draw the difference image
    [[nsContext CIContext] drawImage:diffImage atPoint:CGPointZero fromRect:[diffImage extent]];
    
    // restore the previous context and state
    [NSGraphicsContext restoreGraphicsState];
        
    [referenceImage release];
    [testImage release];
    
    return diffBitmap;
}

/**
 * Counts the number of non-black pixels, and returns the percentage
 * of non-black pixels to total pixels in the image.
 */
float computePercentageDifferent(NSBitmapImageRep *diffBitmap)
{
    int totalPixels = [diffBitmap pixelsHigh] * [diffBitmap pixelsWide];
    int totalBytes = [diffBitmap bytesPerPlane];
    unsigned char *bitmapData = [diffBitmap bitmapData];
    int differences = 0;
    
    // NOTE: This may not be safe when switching between ENDIAN types
    for (int i = 0; i < totalBytes; i += 4) {
        if (*(bitmapData + i) != 0 || *(bitmapData + i + 1) != 0 || *(bitmapData + i + 2) != 0)
            differences++;
    }
    
    return (differences * 100.0)/totalPixels;
}

/**
 * Takes two images and places them in an animated GIF that repeats with a 2 second delay
 */
void saveAnimatedGIFToFile(NSBitmapImageRep *firstFrame, NSBitmapImageRep *secondFrame, NSString *animatedGIFPath)
{
    // options to set the delay on each frame of 2 seconds
    NSDictionary *frameProperties = [NSDictionary 
        dictionaryWithObject:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:2] forKey:(NSString *) kCGImagePropertyGIFDelayTime]
        forKey:(NSString *) kCGImagePropertyGIFDictionary];
        
    // options to turn on looping for the GIF
    NSDictionary *gifProperties = [NSDictionary 
        dictionaryWithObject:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:0] forKey:(NSString *) kCGImagePropertyGIFLoopCount]
        forKey:(NSString *) kCGImagePropertyGIFDictionary];
    
    // set the place to save the GIF to
    CGImageDestinationRef animatedGIF = CGImageDestinationCreateWithURL(
        (CFURLRef) [NSURL fileURLWithPath:animatedGIFPath], 
        kUTTypeGIF, 
        2, // two images in this GIF
        NULL
    );
    
    // get the data for the first frame
    CGDataProviderRef firstFrameProvider = CGDataProviderCreateWithData(
        NULL,
        [firstFrame bitmapData],
        [firstFrame bytesPerRow] * [firstFrame pixelsHigh],
        NULL
    );
    
    // create a CGImage for the first frame
    CGImageRef cgFirstFrame = CGImageCreate (
        [firstFrame pixelsWide],
        [firstFrame pixelsHigh],
        [secondFrame bitsPerPixel] / [secondFrame samplesPerPixel],
        [firstFrame bitsPerPixel],
        [firstFrame bytesPerRow],
        CGColorSpaceCreateDeviceRGB(),
        kCGImageAlphaNone,
        firstFrameProvider,
        NULL,
        0,
        kCGRenderingIntentDefault
    );
    
    // get the data for the second frame
    CGDataProviderRef secondFrameProvider = CGDataProviderCreateWithData(
        NULL,
        [secondFrame bitmapData],
        [secondFrame bytesPerRow] * [secondFrame pixelsHigh],
        NULL
    );
    
    // create a CGImage for the second frame
    CGImageRef cgSecondFrame = CGImageCreate (
        [secondFrame pixelsWide],
        [secondFrame pixelsHigh],
        [secondFrame bitsPerPixel] / [secondFrame samplesPerPixel],
        [secondFrame bitsPerPixel],
        [secondFrame bytesPerRow],
        CGColorSpaceCreateDeviceRGB(),
        kCGImageAlphaNone,
        secondFrameProvider,
        NULL,
        0,
        kCGRenderingIntentDefault
    );
    
    // place the two images into the final animated gif and set related options
    CGImageDestinationAddImage(animatedGIF, cgFirstFrame, (CFDictionaryRef) frameProperties);
    CGImageDestinationAddImage(animatedGIF, cgSecondFrame, (CFDictionaryRef) frameProperties);
    CGImageDestinationSetProperties(animatedGIF, (CFDictionaryRef) gifProperties);
    
    // save the gif to a file
    CGImageDestinationFinalize(animatedGIF);
    
    // release the memory used
    CGDataProviderRelease(firstFrameProvider);
    CGDataProviderRelease(secondFrameProvider);
    CGImageRelease(cgFirstFrame);
    CGImageRelease(cgSecondFrame);
    CFRelease(animatedGIF);
} 
