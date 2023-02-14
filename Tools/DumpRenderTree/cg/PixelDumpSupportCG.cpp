/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2007 Eric Seidel <eric@webkit.org>
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "config.h"
#include "PixelDumpSupportCG.h"

#include "DumpRenderTree.h"
#include "PixelDumpSupport.h"
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <ctype.h>
#include <wtf/Assertions.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/SHA1.h>

#if PLATFORM(IOS_FAMILY)
#include <MobileCoreServices/UTCoreTypes.h>
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>
#elif PLATFORM(MAC)
#include <CoreServices/CoreServices.h>
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>
#endif

using namespace std;

static void printPNG(CGImageRef image, const char* checksum, double scaleFactor)
{
    auto imageData = adoptCF(CFDataCreateMutable(0, 0));
    auto imageDest = adoptCF(CGImageDestinationCreateWithData(imageData.get(), kUTTypePNG, 1, 0));

    auto propertiesDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    double resolutionWidth = 72.0 * scaleFactor;
    double resolutionHeight = 72.0 * scaleFactor;
    auto resolutionWidthNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &resolutionWidth));
    auto resolutionHeightNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &resolutionHeight));
    CFDictionarySetValue(propertiesDictionary.get(), kCGImagePropertyDPIWidth, resolutionWidthNumber.get());
    CFDictionarySetValue(propertiesDictionary.get(), kCGImagePropertyDPIHeight, resolutionHeightNumber.get());

    CGImageDestinationAddImage(imageDest.get(), image, propertiesDictionary.get());
    CGImageDestinationFinalize(imageDest.get());

    const UInt8* data = CFDataGetBytePtr(imageData.get());
    CFIndex dataLength = CFDataGetLength(imageData.get());

    printPNG(static_cast<const unsigned char*>(data), static_cast<size_t>(dataLength), checksum);
}

void computeSHA1HashStringForBitmapContext(BitmapContext* context, char hashString[33])
{
    CGContextRef bitmapContext = context->cgContext();

    ASSERT(CGBitmapContextGetBitsPerPixel(bitmapContext) == 32); // ImageDiff assumes 32 bit RGBA, we must as well.
    size_t pixelsHigh = CGBitmapContextGetHeight(bitmapContext);
    size_t pixelsWide = CGBitmapContextGetWidth(bitmapContext);
    size_t bytesPerRow = CGBitmapContextGetBytesPerRow(bitmapContext);

    // We need to swap the bytes to ensure consistent hashes independently of endianness
    SHA1 sha1;
    unsigned char* bitmapData = static_cast<unsigned char*>(CGBitmapContextGetData(bitmapContext));
    if ((CGBitmapContextGetBitmapInfo(bitmapContext) & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Big) {
        for (unsigned row = 0; row < pixelsHigh; row++) {
            uint32_t buffer[pixelsWide];
            for (unsigned column = 0; column < pixelsWide; column++)
                buffer[column] = OSReadLittleInt32(bitmapData, 4 * column);
            sha1.addBytes(reinterpret_cast<const uint8_t*>(buffer), 4 * pixelsWide);
            bitmapData += bytesPerRow;
        }
    } else {
        for (unsigned row = 0; row < pixelsHigh; row++) {
            sha1.addBytes(bitmapData, 4 * pixelsWide);
            bitmapData += bytesPerRow;
        }
    }
    SHA1::Digest hash;
    sha1.computeHash(hash);

    hashString[0] = '\0';
    for (int i = 0; i < 16; i++)
        snprintf(hashString, 33, "%s%02x", hashString, hash[i]);
}

void dumpBitmap(BitmapContext* context, const char* checksum)
{
    RetainPtr<CGImageRef> image = adoptCF(CGBitmapContextCreateImage(context->cgContext()));
    printPNG(image.get(), checksum, context->scaleFactor());
}

RefPtr<BitmapContext> createBitmapContext(size_t pixelsWide, size_t pixelsHigh, size_t& rowBytes)
{
    rowBytes = (4 * pixelsWide + 63) & ~63; // Use a multiple of 64 bytes to improve CG performance

    UniqueBitmapBuffer buffer { calloc(pixelsHigh, rowBytes), std::free };
    if (!buffer) {
        WTFLogAlways("DumpRenderTree: calloc(%zu, %zu) failed\n", pixelsHigh, rowBytes);
        return nullptr;
    }
    
    // Creating this bitmap in the device color space prevents any color conversion when the image of the web view is drawn into it.
    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    auto context = adoptCF(CGBitmapContextCreate(buffer.get(), pixelsWide, pixelsHigh, 8, rowBytes, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) | static_cast<uint32_t>(kCGBitmapByteOrder32Host)));
    if (!context) {
        WTFLogAlways("DumpRenderTree: CGBitmapContextCreate(%p, %zu, %zu, 8, %zu, %p, 0x%x) failed\n", buffer.get(), pixelsHigh, pixelsWide, rowBytes, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) | static_cast<uint32_t>(kCGBitmapByteOrder32Host));
        return nullptr;
    }

    return BitmapContext::createByAdoptingBitmapAndContext(WTFMove(buffer), WTFMove(context));
}
