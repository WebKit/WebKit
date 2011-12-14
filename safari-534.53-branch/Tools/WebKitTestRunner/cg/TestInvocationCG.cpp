/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestInvocation.h"

#include "PlatformWebView.h"
#include "TestController.h"
#include <ImageIO/CGImageDestination.h>
#include <WebKit2/WKImageCG.h>
#include <wtf/MD5.h>
#include <wtf/RetainPtr.h>
#include <wtf/StringExtras.h>

#if PLATFORM(MAC)
#include <LaunchServices/UTCoreTypes.h>
#endif

#if PLATFORM(WIN)
static const CFStringRef kUTTypePNG = CFSTR("public.png");
#endif

namespace WTR {

static CGContextRef createCGContextFromImage(WKImageRef wkImage)
{
    RetainPtr<CGImageRef> image(AdoptCF, WKImageCreateCGImage(wkImage));

    size_t pixelsWide = CGImageGetWidth(image.get());
    size_t pixelsHigh = CGImageGetHeight(image.get());
    size_t rowBytes = (4 * pixelsWide + 63) & ~63;
    void* buffer = calloc(pixelsHigh, rowBytes);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(buffer, pixelsWide, pixelsHigh, 8, rowBytes, colorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(colorSpace);
    
    CGContextDrawImage(context, CGRectMake(0, 0, pixelsWide, pixelsHigh), image.get());

    return context;
}

void computeMD5HashStringForContext(CGContextRef bitmapContext, char hashString[33])
{
    ASSERT(CGBitmapContextGetBitsPerPixel(bitmapContext) == 32); // ImageDiff assumes 32 bit RGBA, we must as well.
    size_t pixelsHigh = CGBitmapContextGetHeight(bitmapContext);
    size_t pixelsWide = CGBitmapContextGetWidth(bitmapContext);
    size_t bytesPerRow = CGBitmapContextGetBytesPerRow(bitmapContext);
    
    // We need to swap the bytes to ensure consistent hashes independently of endianness
    MD5 md5;
    unsigned char* bitmapData = static_cast<unsigned char*>(CGBitmapContextGetData(bitmapContext));
#if PLATFORM(MAC)
    if ((CGBitmapContextGetBitmapInfo(bitmapContext) & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Big) {
        for (unsigned row = 0; row < pixelsHigh; row++) {
            Vector<uint8_t> buffer(4 * pixelsWide);
            for (unsigned column = 0; column < pixelsWide; column++)
                buffer[column] = OSReadLittleInt32(bitmapData, 4 * column);
            md5.addBytes(buffer);
            bitmapData += bytesPerRow;
        }
    } else {
#endif
        for (unsigned row = 0; row < pixelsHigh; row++) {
            md5.addBytes(bitmapData, 4 * pixelsWide);
            bitmapData += bytesPerRow;
        }
#if PLATFORM(MAC)
    }
#endif

    Vector<uint8_t, 16> hash;
    md5.checksum(hash);
    
    hashString[0] = '\0';
    for (int i = 0; i < 16; i++)
        snprintf(hashString, 33, "%s%02x", hashString, hash[i]);
}

static void dumpBitmap(CGContextRef bitmapContext)
{
    RetainPtr<CGImageRef> image(AdoptCF, CGBitmapContextCreateImage(bitmapContext));
    RetainPtr<CFMutableDataRef> imageData(AdoptCF, CFDataCreateMutable(0, 0));
    RetainPtr<CGImageDestinationRef> imageDest(AdoptCF, CGImageDestinationCreateWithData(imageData.get(), kUTTypePNG, 1, 0));
    CGImageDestinationAddImage(imageDest.get(), image.get(), 0);
    CGImageDestinationFinalize(imageDest.get());

    const unsigned char* data = CFDataGetBytePtr(imageData.get());
    const size_t dataLength = CFDataGetLength(imageData.get());


    fprintf(stdout, "Content-Type: %s\n", "image/png");
    fprintf(stdout, "Content-Length: %lu\n", static_cast<unsigned long>(dataLength));
    
    const size_t bytesToWriteInOneChunk = 1 << 15;
    size_t dataRemainingToWrite = dataLength;
    while (dataRemainingToWrite) {
        size_t bytesToWriteInThisChunk = std::min(dataRemainingToWrite, bytesToWriteInOneChunk);
        size_t bytesWritten = fwrite(data, 1, bytesToWriteInThisChunk, stdout);
        if (bytesWritten != bytesToWriteInThisChunk)
            break;
        dataRemainingToWrite -= bytesWritten;
        data += bytesWritten;
    }
}

void TestInvocation::dumpPixelsAndCompareWithExpected(WKImageRef image)
{
    CGContextRef context = createCGContextFromImage(image);

    // Compute the hash of the bitmap context pixels
    char actualHash[33];
    computeMD5HashStringForContext(context, actualHash);
    fprintf(stdout, "\nActualHash: %s\n", actualHash);

    // Check the computed hash against the expected one and dump image on mismatch
    bool hashesMatch = false;
    if (m_expectedPixelHash.length() > 0) {
        ASSERT(m_expectedPixelHash.length() == 32);

        fprintf(stdout, "\nExpectedHash: %s\n", m_expectedPixelHash.c_str());

        // FIXME: Do case insensitive compare.
        if (m_expectedPixelHash == actualHash)
            hashesMatch = true;
    }

    if (!hashesMatch)
        dumpBitmap(context);
}

} // namespace WTR
