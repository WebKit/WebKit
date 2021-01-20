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

#include "PlatformImage.h"

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGImage.h>
#include <ImageIO/CGImageDestination.h>
#include <algorithm>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

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

#ifdef _WIN32
#define FORMAT_SIZE_T "Iu"
#else
#define FORMAT_SIZE_T "zu"
#endif

#ifdef _WIN32
static const CFStringRef kUTTypePNG = CFSTR("public.png");
#endif

namespace ImageDiff {

std::unique_ptr<PlatformImage> PlatformImage::createFromStdin(size_t imageSize)
{
    unsigned char buffer[2048];
    CFMutableDataRef data = CFDataCreateMutable(0, imageSize);

    size_t bytesRemaining = imageSize;
    while (bytesRemaining > 0) {
        size_t bytesToRead = std::min<size_t>(bytesRemaining, 2048);
        size_t bytesRead = fread(buffer, 1, bytesToRead, stdin);
        CFDataAppendBytes(data, buffer, static_cast<CFIndex>(bytesRead));
        bytesRemaining -= static_cast<int>(bytesRead);
    }
    CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData(data);
    CFRelease(data);
    CGImageRef image = CGImageCreateWithPNGDataProvider(dataProvider, 0, false, kCGRenderingIntentDefault);
    CFRelease(dataProvider);

    return std::make_unique<PlatformImage>(image);
}

std::unique_ptr<PlatformImage> PlatformImage::createFromDiffData(void* data, size_t width, size_t height)
{
    static CGColorSpaceRef diffColorspace = CGColorSpaceCreateDeviceGray();
    CGDataProviderRef provider = CGDataProviderCreateWithData(0, data, width * height, [](void*, const void* data, size_t) {
        free(const_cast<void*>(data));
    });
    CGImageRef image = CGImageCreate(width, height, 8, 8, width, diffColorspace, 0, provider, 0, false, kCGRenderingIntentDefault);
    CFRelease(provider);

    return std::make_unique<PlatformImage>(image);
}

PlatformImage::PlatformImage(CGImageRef image)
    : m_image(image)
{
}

PlatformImage::~PlatformImage()
{
    CFRelease(m_image);
    free(m_buffer);
}

size_t PlatformImage::width() const
{
    return CGImageGetWidth(m_image);
}

size_t PlatformImage::height() const
{
    return CGImageGetHeight(m_image);
}

size_t PlatformImage::rowBytes() const
{
    return width() * 4;
}

bool PlatformImage::hasAlpha() const
{
    CGImageAlphaInfo info = CGImageGetAlphaInfo(m_image);
    return (info >= kCGImageAlphaPremultipliedLast) && (info <= kCGImageAlphaFirst);
}

unsigned char* PlatformImage::pixels() const
{
    size_t width = this->width();
    size_t height = this->height();
    size_t rowBytes = width * 4;

    m_buffer = calloc(height, rowBytes);
    CGContextRef context = CGBitmapContextCreate(m_buffer, width, height, 8, rowBytes, CGImageGetColorSpace(m_image), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), m_image);
    CFRelease(context);
    return reinterpret_cast<unsigned char*>(m_buffer);
}

void PlatformImage::writeAsPNGToStdout()
{
    CFMutableDataRef imageData = CFDataCreateMutable(0, 0);
    CGImageDestinationRef imageDest = CGImageDestinationCreateWithData(imageData, kUTTypePNG, 1, 0);
    CGImageDestinationAddImage(imageDest, m_image, 0);
    CGImageDestinationFinalize(imageDest);
    printf("Content-Length: %" FORMAT_SIZE_T "\n", static_cast<size_t>(CFDataGetLength(imageData)));
    fwrite(CFDataGetBytePtr(imageData), 1, CFDataGetLength(imageData), stdout);
    CFRelease(imageData);
    CFRelease(imageDest);
}

} // namespace ImageDiff
