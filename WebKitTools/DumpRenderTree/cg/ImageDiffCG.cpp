/*
 * Copyright (C) 2005, 2007 Apple Inc. All rights reserved.
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

#define min min

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGImage.h>
#include <ImageIO/CGImageDestination.h>
#include <stdio.h>
#include <wtf/Platform.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(WIN)
#include <fcntl.h>
#include <io.h>
#endif

#if PLATFORM(MAC)
#include <LaunchServices/UTCoreTypes.h>
#endif

#ifndef CGFLOAT_DEFINED
#ifdef __LP64__
typedef double CGFloat;
#else
typedef float CGFloat;
#endif
#define CGFLOAT_DEFINED 1
#endif

using namespace std;

#if PLATFORM(WIN)
static const CFStringRef kUTTypePNG = CFSTR("public.png");
#endif

static RetainPtr<CGImageRef> createImageFromStdin(int bytesRemaining)
{
    unsigned char buffer[2048];
    RetainPtr<CFMutableDataRef> data(AdoptCF, CFDataCreateMutable(0, bytesRemaining));

    while (bytesRemaining > 0) {
        size_t bytesToRead = min(bytesRemaining, 2048);
        size_t bytesRead = fread(buffer, 1, bytesToRead, stdin);
        CFDataAppendBytes(data.get(), buffer, static_cast<CFIndex>(bytesRead));
        bytesRemaining -= static_cast<int>(bytesRead);
    }
    RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateWithCFData(data.get()));
    return RetainPtr<CGImageRef>(AdoptCF, CGImageCreateWithPNGDataProvider(dataProvider.get(), 0, false, kCGRenderingIntentDefault));
}

static RetainPtr<CGContextRef> getDifferenceBitmap(CGImageRef testBitmap, CGImageRef referenceBitmap)
{
    // we must have both images to take diff
    if (!testBitmap || !referenceBitmap)
        return 0;

    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    static CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFDataSetLength(data, CGImageGetHeight(testBitmap) * CGImageGetBytesPerRow(testBitmap));
    RetainPtr<CGContextRef> context(AdoptCF, CGBitmapContextCreate(CFDataGetMutableBytePtr(data), CGImageGetWidth(testBitmap), CGImageGetHeight(testBitmap),
        CGImageGetBitsPerComponent(testBitmap), CGImageGetBytesPerRow(testBitmap), colorSpace.get(), kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst));

    CGContextSetBlendMode(context.get(), kCGBlendModeNormal);
    CGContextDrawImage(context.get(), CGRectMake(0, 0, static_cast<CGFloat>(CGImageGetWidth(testBitmap)), static_cast<CGFloat>(CGImageGetHeight(testBitmap))), testBitmap);
    CGContextSetBlendMode(context.get(), kCGBlendModeDifference);
    CGContextDrawImage(context.get(), CGRectMake(0, 0, static_cast<CGFloat>(CGImageGetWidth(referenceBitmap)), static_cast<CGFloat>(CGImageGetHeight(referenceBitmap))), referenceBitmap);

    return context;
}

/**
 * Counts the number of non-black pixels, and returns the percentage
 * of non-black pixels to total pixels in the image.
 */
static float computePercentageDifferent(CGContextRef diffBitmap, unsigned threshold)
{
    // if diffBiatmap is nil, then there was an error, and it didn't match.
    if (!diffBitmap)
        return 100.0f;

    size_t pixelsHigh = CGBitmapContextGetHeight(diffBitmap);
    size_t pixelsWide = CGBitmapContextGetWidth(diffBitmap);
    size_t bytesPerRow = CGBitmapContextGetBytesPerRow(diffBitmap);
    unsigned char* pixelRowData = static_cast<unsigned char*>(CGBitmapContextGetData(diffBitmap));
    unsigned differences = 0;

    // NOTE: This may not be safe when switching between ENDIAN types
    for (unsigned row = 0; row < pixelsHigh; row++) {
        for (unsigned col = 0; col < (pixelsWide * 4); col += 4) {
            unsigned char* red = pixelRowData + col;
            unsigned char* green = red + 1;
            unsigned char* blue = red + 2;
            unsigned distance = *red + *green + *blue;
            if (distance > threshold) {
                differences++;
                // shift the pixels towards white to make them more visible
                *red = static_cast<unsigned char>(min(UCHAR_MAX, *red + 100));
                *green = static_cast<unsigned char>(min(UCHAR_MAX, *green + 100));
                *blue = static_cast<unsigned char>(min(UCHAR_MAX, *blue + 100));
            }
        }
        pixelRowData += bytesPerRow;
    }

    float totalPixels = static_cast<float>(pixelsHigh * pixelsWide);
    return (differences * 100.f) / totalPixels;
}

static void compareImages(CGImageRef actualBitmap, CGImageRef baselineBitmap, unsigned threshold)
{
    // prepare the difference blend to check for pixel variations
    RetainPtr<CGContextRef> diffBitmap = getDifferenceBitmap(actualBitmap, baselineBitmap);

    float percentage = computePercentageDifferent(diffBitmap.get(), threshold);

    percentage = (float)((int)(percentage * 100.0f)) / 100.0f; // round to 2 decimal places

    // send message to let them know if an image was wrong
    if (percentage > 0.0f) {
        // since the diff might actually show something, send it to stdout
        RetainPtr<CGImageRef> image(AdoptCF, CGBitmapContextCreateImage(diffBitmap.get()));
        RetainPtr<CFMutableDataRef> imageData(AdoptCF, CFDataCreateMutable(0, 0));
        RetainPtr<CGImageDestinationRef> imageDest(AdoptCF, CGImageDestinationCreateWithData(imageData.get(), kUTTypePNG, 1, 0));
        CGImageDestinationAddImage(imageDest.get(), image.get(), 0);
        CGImageDestinationFinalize(imageDest.get());
        printf("Content-length: %lu\n", CFDataGetLength(imageData.get()));
        fwrite(CFDataGetBytePtr(imageData.get()), 1, CFDataGetLength(imageData.get()), stdout);
        fprintf(stdout, "diff: %01.2f%% failed\n", percentage);
    } else
        fprintf(stdout, "diff: %01.2f%% passed\n", percentage);
}

int main(int argc, const char* argv[])
{
#if PLATFORM(WIN)
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
#endif

    unsigned threshold = 0;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--threshold")) {
            if (i >= argc - 1)
                exit(1);
            threshold = strtol(argv[i + 1], 0, 0);
            ++i;
            continue;
        }
    }

    char buffer[2048];
    RetainPtr<CGImageRef> actualImage;
    RetainPtr<CGImageRef> baselineImage;

    while (fgets(buffer, sizeof(buffer), stdin)) {
        // remove the CR
        char* newLineCharacter = strchr(buffer, '\n');
        if (newLineCharacter)
            *newLineCharacter = '\0';

        if (!strncmp("Content-length: ", buffer, 16)) {
            strtok(buffer, " ");
            int imageSize = strtol(strtok(0, " "), 0, 10);

            if (imageSize > 0 && !actualImage)
                actualImage = createImageFromStdin(imageSize);
            else if (imageSize > 0 && !baselineImage)
                baselineImage = createImageFromStdin(imageSize);
            else
                fputs("error, image size must be specified.\n", stdout);
        }

        if (actualImage && baselineImage) {
            compareImages(actualImage.get(), baselineImage.get(), threshold);
            actualImage = 0;
            baselineImage = 0;
        }

        fflush(stdout);
    }

    return 0;
}
