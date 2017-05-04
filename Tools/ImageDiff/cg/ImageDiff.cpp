/*
 * Copyright (C) 2005, 2007, 2015 Apple Inc. All rights reserved.
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

// FIXME: We need to be able to include these defines from a config.h somewhere.
#define JS_EXPORT_PRIVATE

#include <algorithm>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <wtf/MathExtras.h>
#endif

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGImage.h>
#include <ImageIO/CGImageDestination.h>

#if __APPLE__
#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#include <MobileCoreServices/UTCoreTypes.h>
#else
#include <CoreServices/CoreServices.h>
#endif

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

#ifdef _WIN32
static const CFStringRef kUTTypePNG = CFSTR("public.png");
#endif

static CGImageRef createImageFromStdin(long bytesRemaining)
{
    unsigned char buffer[2048];
    CFMutableDataRef data = CFDataCreateMutable(0, bytesRemaining);

    while (bytesRemaining > 0) {
        size_t bytesToRead = std::min(bytesRemaining, 2048L);
        size_t bytesRead = fread(buffer, 1, bytesToRead, stdin);
        CFDataAppendBytes(data, buffer, static_cast<CFIndex>(bytesRead));
        bytesRemaining -= static_cast<int>(bytesRead);
    }
    CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData(data);
    CFRelease(data);
    CGImageRef result = CGImageCreateWithPNGDataProvider(dataProvider, 0, false, kCGRenderingIntentDefault);
    CFRelease(dataProvider);
    
    return result;
}

static void releaseMallocBuffer(void* info, const void* data, size_t size)
{
    free((void*)data);
}

static CGImageRef createDifferenceImage(CGImageRef baseImage, CGImageRef testImage, float& difference)
{
    size_t width = CGImageGetWidth(baseImage);
    size_t height = CGImageGetHeight(baseImage);
    size_t rowBytes = width * 4;

    // Draw base image in bitmap context
    void* baseBuffer = calloc(height, rowBytes);
    CGContextRef baseContext = CGBitmapContextCreate(baseBuffer, width, height, 8, rowBytes, CGImageGetColorSpace(baseImage), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGContextDrawImage(baseContext, CGRectMake(0, 0, width, height), baseImage);

    // Draw test image in bitmap context
    void* buffer = calloc(height, rowBytes);
    CGContextRef context = CGBitmapContextCreate(buffer, width, height, 8, rowBytes, CGImageGetColorSpace(testImage), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), testImage);

    // Compare the content of the 2 bitmaps
    void* diffBuffer = malloc(width * height);
    float count = 0.0f;
    float sum = 0.0f;
    float maxDistance = 0.0f;
    unsigned char* basePixel = reinterpret_cast<unsigned char*>(baseBuffer);
    unsigned char* pixel = reinterpret_cast<unsigned char*>(buffer);
    unsigned char* diff = reinterpret_cast<unsigned char*>(diffBuffer);
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            float red = (pixel[0] - basePixel[0]) / max<float>(255 - basePixel[0], basePixel[0]);
            float green = (pixel[1] - basePixel[1]) / max<float>(255 - basePixel[1], basePixel[1]);
            float blue = (pixel[2] - basePixel[2]) / max<float>(255 - basePixel[2], basePixel[2]);
            float alpha = (pixel[3] - basePixel[3]) / max<float>(255 - basePixel[3], basePixel[3]);
            float distance = sqrtf(red * red + green * green + blue * blue + alpha * alpha) / 2.0f;
            
            *diff++ = (unsigned char)(distance * 255.0f);
            
            if (distance >= 1.0f / 255.0f) {
                count += 1.0f;
                sum += distance;
                if (distance > maxDistance)
                    maxDistance = distance;
            }
            
            basePixel += 4;
            pixel += 4;
        }
    }
    
    // Compute the difference as a percentage combining both the number of different pixels and their difference amount i.e. the average distance over the entire image
    if (count > 0.0f)
        difference = 100.0f * sum / (height * width);
    else
        difference = 0.0f;

    CGImageRef diffImage = nullptr;
    // Generate a normalized diff image if there is any difference
    if (difference > 0.0f) {
        if (maxDistance < 1.0f) {
            diff = reinterpret_cast<unsigned char*>(diffBuffer);
            for (size_t p = 0; p < height * width; ++p)
                diff[p] = diff[p] / maxDistance;
        }
        
        static CGColorSpaceRef diffColorspace = CGColorSpaceCreateDeviceGray();
        CGDataProviderRef provider = CGDataProviderCreateWithData(0, diffBuffer, width * height, releaseMallocBuffer);
        diffImage = CGImageCreate(width, height, 8, 8, width, diffColorspace, 0, provider, 0, false, kCGRenderingIntentDefault);
        CFRelease(provider);
    } else
        free(diffBuffer);
    
    CFRelease(baseContext);
    CFRelease(context);
    
    // Destroy drawing buffers
    if (buffer)
        free(buffer);
    if (baseBuffer)
        free(baseBuffer);
    
    return diffImage;
}

static inline bool imageHasAlpha(CGImageRef image)
{
    CGImageAlphaInfo info = CGImageGetAlphaInfo(image);
    
    return (info >= kCGImageAlphaPremultipliedLast) && (info <= kCGImageAlphaFirst);
}

int main(int argc, const char* argv[])
{
#ifdef _WIN32
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
#endif

    float tolerance = 0.0f;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--tolerance")) {
            if (i >= argc - 1)
                exit(1);
            tolerance = strtof(argv[i + 1], 0);
            ++i;
            continue;
        }
    }

    char buffer[2048];
    CGImageRef actualImage = nullptr;
    CGImageRef baselineImage = nullptr;

    while (fgets(buffer, sizeof(buffer), stdin)) {
        // remove the CR
        char* newLineCharacter = strchr(buffer, '\n');
        if (newLineCharacter)
            *newLineCharacter = '\0';

        if (!strncmp("Content-Length: ", buffer, 16)) {
            strtok(buffer, " ");
            long imageSize = strtol(strtok(0, " "), 0, 10);

            if (imageSize > 0 && !actualImage)
                actualImage = createImageFromStdin(imageSize);
            else if (imageSize > 0 && !baselineImage)
                baselineImage = createImageFromStdin(imageSize);
            else
                fputs("Error: image size must be specified.\n", stderr);
        }

        if (actualImage && baselineImage) {
            CGImageRef diffImage = nullptr;
            float difference = 100.0f;

            if ((CGImageGetWidth(actualImage) == CGImageGetWidth(baselineImage)) && (CGImageGetHeight(actualImage) == CGImageGetHeight(baselineImage)) && (imageHasAlpha(actualImage) == imageHasAlpha(baselineImage))) {
                diffImage = createDifferenceImage(actualImage, baselineImage, difference); // difference is passed by reference
                if (difference <= tolerance)
                    difference = 0.0f;
                else {
                    difference = roundf(difference * 100.0f) / 100.0f;
                    difference = max(difference, 0.01f); // round to 2 decimal places
                }
            } else {
                if (CGImageGetWidth(actualImage) != CGImageGetWidth(baselineImage) || CGImageGetHeight(actualImage) != CGImageGetHeight(baselineImage)) {
#ifdef _WIN32
                    fprintf(stderr, "Error: test and reference images have different sizes. Test image is %Iux%Iu, reference image is %Iux%Iu\n",
#else
                    fprintf(stderr, "Error: test and reference images have different sizes. Test image is %zux%zu, reference image is %zux%zu\n",
#endif
                        CGImageGetWidth(actualImage), CGImageGetHeight(actualImage),
                        CGImageGetWidth(baselineImage), CGImageGetHeight(baselineImage));
                } else if (imageHasAlpha(actualImage) != imageHasAlpha(baselineImage))
                    fprintf(stderr, "Error: test and reference images differ in alpha. Test image %s alpha, reference image %s alpha.\n",
                    imageHasAlpha(actualImage) ? "has" : "does not have",
                        imageHasAlpha(baselineImage) ? "has" : "does not have");
            }
            
            if (difference > 0.0f) {
                if (diffImage) {
                    CFMutableDataRef imageData = CFDataCreateMutable(0, 0);
                    CGImageDestinationRef imageDest = CGImageDestinationCreateWithData(imageData, kUTTypePNG, 1, 0);
                    CGImageDestinationAddImage(imageDest, diffImage, 0);
                    CGImageDestinationFinalize(imageDest);
#ifdef _WIN32
                    printf("Content-Length: %Iu\n", static_cast<size_t>(CFDataGetLength(imageData)));
#else
                    printf("Content-Length: %zu\n", static_cast<size_t>(CFDataGetLength(imageData)));
#endif
                    fwrite(CFDataGetBytePtr(imageData), 1, CFDataGetLength(imageData), stdout);
                    
                    CFRelease(imageData);
                    CFRelease(imageDest);
                }

                fprintf(stdout, "diff: %01.2f%% failed\n", difference);
            } else
                fprintf(stdout, "diff: %01.2f%% passed\n", difference);
            
            if (diffImage)
                CFRelease(diffImage);
            if (actualImage)
                CFRelease(actualImage);
            if (baselineImage)
                CFRelease(baselineImage);
            actualImage = nullptr;
            baselineImage = nullptr;
        }
        

        fflush(stdout);
    }

    return 0;
}
