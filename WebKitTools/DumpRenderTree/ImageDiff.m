/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <Foundation/Foundation.h>
#import <QuartzCore/CIFilter.h>
#import <QuartzCore/CIImage.h>
#import <QuartzCore/CIContext.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSCIImageRep.h>

/* prototypes */
int main(int argc, const char *argv[]);
NSBitmapImageRep *getImageFromStdin(int imageSize);
void compareImages(NSBitmapImageRep *actualBitmap, NSBitmapImageRep *baselineImage);
NSBitmapImageRep *getDifferenceBitmap(NSBitmapImageRep *testBitmap, NSBitmapImageRep *referenceBitmap);
float computePercentageDifferent(NSBitmapImageRep *diffBitmap);


int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    char buffer[2048];
    NSBitmapImageRep *actualImage = nil;
    NSBitmapImageRep *baselineImage = nil;

    NSAutoreleasePool *innerPool = [[NSAutoreleasePool alloc] init];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        // remove the CR
        char *newLineCharacter = strchr(buffer, '\n');
        if (newLineCharacter) {
            *newLineCharacter = '\0';
        }
        
        if (strncmp("Content-length: ", buffer, 16) == 0) {
            strtok(buffer, " ");
            int imageSize = strtol(strtok(NULL, " "), NULL, 10);

            if(imageSize > 0 && actualImage == nil) 
                actualImage = getImageFromStdin(imageSize);
            else if (imageSize > 0 && baselineImage == nil)
                baselineImage = getImageFromStdin(imageSize);
            else
                fputs("error, image size must be specified.\n", stdout);
        }
        
        if (actualImage != nil && baselineImage != nil) {
            compareImages(actualImage, baselineImage);
            actualImage = nil;
            baselineImage = nil;
            [innerPool release];
            innerPool = [[NSAutoreleasePool alloc] init];
        }
        
        fflush(stdout);
    }
    [innerPool release];
    
    [pool release];
    return 0;
}

NSBitmapImageRep *getImageFromStdin(int bytesRemaining)
{
    unsigned char buffer[2048];
    NSMutableData *data = [[NSMutableData alloc] initWithCapacity:bytesRemaining];
    
    int bytesRead = 0;
    while (bytesRemaining > 0) {
        bytesRead = (bytesRemaining > 2048 ? 2048 : bytesRemaining);
        fread(buffer, bytesRead, 1, stdin);
        [data appendBytes:buffer length:bytesRead];
        bytesRemaining -= bytesRead;
    }
    
    NSBitmapImageRep *image = [NSBitmapImageRep imageRepWithData:data];
    [data release];
    
    return image; 
}

void compareImages(NSBitmapImageRep *actualBitmap, NSBitmapImageRep *baselineBitmap)
{
    // prepare the difference blend to check for pixel variations
    NSBitmapImageRep *diffBitmap = getDifferenceBitmap(actualBitmap, baselineBitmap);
            
    float percentage = computePercentageDifferent(diffBitmap);
    
    // send message to let them know if an image was wrong
    if (percentage > 0.0) {
        // since the diff might actually show something, send it to stdout
        NSData *diffPNGData = [diffBitmap representationUsingType:NSPNGFileType properties:nil];
        fprintf(stdout, "Content-length: %d\n", [diffPNGData length]);
        fwrite([diffPNGData bytes], [diffPNGData length], 1, stdout);
        fprintf(stdout, "diff: %01.2f%% failed\n", percentage);
    } else
        fprintf(stdout, "diff: %01.2f%% passed\n", percentage);
}

NSBitmapImageRep *getDifferenceBitmap(NSBitmapImageRep *testBitmap, NSBitmapImageRep *referenceBitmap)
{
    // we must have both images to take diff
    if (testBitmap == nil || referenceBitmap == nil)
        return nil;

    // create a new graphics context to draw our CIImage on
    NSBitmapImageRep *diffBitmap = [[testBitmap copy] autorelease]; // FIXME: likely faster ways than copying.
    NSGraphicsContext *nsContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:diffBitmap];
    CIImage *referenceImage = [[CIImage alloc] initWithBitmapImageRep:referenceBitmap];
    CIImage *testImage = [[CIImage alloc] initWithBitmapImageRep:testBitmap];
    CIFilter *diffBlend = [CIFilter filterWithName:@"CIDifferenceBlendMode"];

    // generate the diff image
    [diffBlend setValue:referenceImage forKey:@"inputImage"];
    [diffBlend setValue:testImage forKey:@"inputBackgroundImage"];
    CIImage *diffImage = [diffBlend valueForKey:@"outputImage"];
    
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
    // if diffBiatmap is nil, then there was an error, and it didn't match.
    if (diffBitmap == nil)
        return 100.0;
        
    int totalPixels = [diffBitmap pixelsHigh] * [diffBitmap pixelsWide];
    int totalBytes = [diffBitmap bytesPerRow] * [diffBitmap pixelsHigh];
    unsigned char *bitmapData = [diffBitmap bitmapData];
    int differences = 0;
    
    // NOTE: This may not be safe when switching between ENDIAN types
    for (int i = 0; i < totalBytes; i += 4) {
        if (*(bitmapData + i) != 0 || *(bitmapData + i + 1) != 0 || *(bitmapData + i + 2) != 0)
            differences++;
    }
    
    return (differences * 100.0)/totalPixels;
}
