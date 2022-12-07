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

#include "PixelDumpSupport.h"
#include "PlatformWebView.h"
#include "TestController.h"
#include <ImageIO/ImageIO.h>
#include <WebKit/WKImageCG.h>
#include <wtf/ASCIICType.h>
#include <wtf/RetainPtr.h>
#include <wtf/SHA1.h>

#if PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
#include <CoreServices/CoreServices.h>
#endif

#if PLATFORM(IOS_FAMILY)
// FIXME: get kUTTypePNG from MobileCoreServices on iOS
static const CFStringRef kUTTypePNG = CFSTR("public.png");
#endif

namespace WTR {

static RetainPtr<CGContextRef> createCGContextFromCGImage(CGImageRef image)
{
    size_t pixelsWide = CGImageGetWidth(image);
    size_t pixelsHigh = CGImageGetHeight(image);
    size_t rowBytes = (4 * pixelsWide + 63) & ~63;

    // Creating this bitmap in the device color space should prevent any color conversion when the image of the web view is drawn into it.
    auto colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    auto context = adoptCF(CGBitmapContextCreate(0, pixelsWide, pixelsHigh, 8, rowBytes, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) | static_cast<uint32_t>(kCGBitmapByteOrder32Host)));
    if (!context)
        return nullptr;

    CGContextDrawImage(context.get(), CGRectMake(0, 0, pixelsWide, pixelsHigh), image);
    return context;
}

static RetainPtr<CGContextRef> createCGContextFromImage(WKImageRef wkImage)
{
    auto image = adoptCF(WKImageCreateCGImage(wkImage));
    return createCGContextFromCGImage(image.get());
}

static std::optional<std::string> computeSHA1HashStringForContext(CGContextRef bitmapContext)
{
    if (!bitmapContext) {
        WTFLogAlways("computeSHA1HashStringForContext: context is null\n");
        return { };
    }
    ASSERT(CGBitmapContextGetBitsPerPixel(bitmapContext) == 32); // ImageDiff assumes 32 bit RGBA, we must as well.
    size_t pixelsHigh = CGBitmapContextGetHeight(bitmapContext);
    size_t pixelsWide = CGBitmapContextGetWidth(bitmapContext);
    size_t bytesPerRow = CGBitmapContextGetBytesPerRow(bitmapContext);
    
    // We need to swap the bytes to ensure consistent hashes independently of endianness
    SHA1 sha1;
    unsigned char* bitmapData = static_cast<unsigned char*>(CGBitmapContextGetData(bitmapContext));
#if PLATFORM(COCOA)
    if ((CGBitmapContextGetBitmapInfo(bitmapContext) & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Big) {
        for (unsigned row = 0; row < pixelsHigh; row++) {
            Vector<uint8_t> buffer(4 * pixelsWide);
            for (unsigned column = 0; column < pixelsWide; column++)
                buffer[column] = OSReadLittleInt32(bitmapData, 4 * column);
            sha1.addBytes(buffer);
            bitmapData += bytesPerRow;
        }
    } else
#endif
    {
        for (unsigned row = 0; row < pixelsHigh; row++) {
            sha1.addBytes(bitmapData, 4 * pixelsWide);
            bitmapData += bytesPerRow;
        }
    }

    auto hexString = sha1.computeHexDigest();
    
    auto result = hexString.toStdString();
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return toASCIILower(c); });
    return result;
}

#if PLATFORM(MAC)
static bool operator==(const WKSize& a, const WKSize& b)
{
    return a.width == b.width && a.height == b.height;
}

static std::optional<std::string> hashForBlackImageOfSize(WKSize size)
{
    // Hardcode some well-known sizes for performance.
    if (size == WKSize { 800, 600 })
        return "6c0b5c4dc61390fd6a45eebdebf7301828ef0e56";
    if (size == WKSize { 1600, 1200 })
        return "11d642624de9935a659bcd042d6ff83f5a917504";
    if (size == WKSize { 480, 360 })
        return "feed54e9caa58b83b0c644fc037f0137828b2c76";

    size_t pixelsWide = size.width;
    size_t pixelsHigh = size.height;
    size_t rowBytes = (4 * pixelsWide + 63) & ~63;

    auto colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    auto context = adoptCF(CGBitmapContextCreate(nullptr, pixelsWide, pixelsHigh, 8, rowBytes, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) | static_cast<uint32_t>(kCGBitmapByteOrder32Host)));
    if (!context)
        return { };

    CGContextSetRGBFillColor(context.get(), 0, 0, 0, 1);

    CGRect bounds = CGRectMake(0, 0, pixelsWide, pixelsHigh);
    CGContextFillRect(context.get(), bounds);

    return computeSHA1HashStringForContext(context.get());
}
#endif // PLATFORM(MAC)

static void dumpBitmap(CGContextRef bitmapContext, const std::string& checksum, WKSize imageSize, WKSize windowSize)
{
    auto image = adoptCF(CGBitmapContextCreateImage(bitmapContext));
    auto imageData = adoptCF(CFDataCreateMutable(0, 0));
    auto imageDest = adoptCF(CGImageDestinationCreateWithData(imageData.get(), kUTTypePNG, 1, 0));

    auto propertiesDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    double resolutionWidth = 72.0 * imageSize.width / windowSize.width;
    double resolutionHeight = 72.0 * imageSize.height / windowSize.height;
    auto resolutionWidthNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &resolutionWidth));
    auto resolutionHeightNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &resolutionHeight));
    CFDictionarySetValue(propertiesDictionary.get(), kCGImagePropertyDPIWidth, resolutionWidthNumber.get());
    CFDictionarySetValue(propertiesDictionary.get(), kCGImagePropertyDPIHeight, resolutionHeightNumber.get());

    CGImageDestinationAddImage(imageDest.get(), image.get(), propertiesDictionary.get());
    CGImageDestinationFinalize(imageDest.get());

    const unsigned char* data = CFDataGetBytePtr(imageData.get());
    const size_t dataLength = CFDataGetLength(imageData.get());

    printPNG(data, dataLength, checksum.c_str());
}

static void paintRepaintRectOverlay(CGContextRef context, WKSize imageSize, WKArrayRef repaintRects)
{
    CGContextSaveGState(context);

    // Using a transparency layer is easier than futzing with clipping.
    CGContextBeginTransparencyLayer(context, 0);
    
    // Flip the context.
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -imageSize.height);
    
    CGContextSetRGBFillColor(context, 0, 0, 0, static_cast<CGFloat>(0.66));
    CGContextFillRect(context, CGRectMake(0, 0, imageSize.width, imageSize.height));

    // Clear the repaint rects.
    size_t count = WKArrayGetSize(repaintRects);
    for (size_t i = 0; i < count; ++i) {
        WKRect rect = WKRectGetValue(static_cast<WKRectRef>(WKArrayGetItemAtIndex(repaintRects, i)));
        CGRect cgRect = CGRectMake(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
        CGContextClearRect(context, cgRect);
    }
    
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

void TestInvocation::dumpPixelsAndCompareWithExpected(SnapshotResultType snapshotType, WKArrayRef repaintRects, WKImageRef wkImage)
{
    RetainPtr<CGContextRef> context;
    WKSize imageSize;
    std::string snapshotHash;

    auto windowSize = TestController::singleton().mainWebView()->windowFrame().size;

    auto generateSnapshot = [&]() -> bool {

        switch (snapshotType) {
        case SnapshotResultType::WebContents:
            if (!wkImage) {
                WTFLogAlways("dumpPixelsAndCompareWithExpected: image is null\n");
                return false;
            }
            context = createCGContextFromImage(wkImage);
            imageSize = WKImageGetSize(wkImage);
            break;
        case SnapshotResultType::WebView:
            auto image = TestController::singleton().mainWebView()->windowSnapshotImage();
            if (!image) {
                WTFLogAlways("dumpPixelsAndCompareWithExpected: image is null\n");
                return false;
            }
            context = createCGContextFromCGImage(image.get());
            imageSize = WKSizeMake(CGImageGetWidth(image.get()), CGImageGetHeight(image.get()));
            break;
        }

        if (!context) {
            WTFLogAlways("dumpPixelsAndCompareWithExpected: context is null\n");
            return false;
        }

        // A non-null repaintRects array means we're doing a repaint test.
        if (repaintRects)
            paintRepaintRectOverlay(context.get(), imageSize, repaintRects);

        auto computedHash = computeSHA1HashStringForContext(context.get());
        if (!computedHash)
            return false;

        snapshotHash = *computedHash;
        return true;
    };

    bool gotBlackSnapshot = false;
    constexpr unsigned maxNumTries = 3;
    unsigned numTries = 1;
    do {
        bool snapshotSucceeded = generateSnapshot();
        if (!snapshotSucceeded)
            return;

#if PLATFORM(MAC)
        // Detect rdar://89840327 and retry.
        auto blackSnapshotHash = hashForBlackImageOfSize(imageSize);
        if (!blackSnapshotHash) {
            WTFLogAlways("Failed to compute hash for black snapshot of size %.0f x %.0f", imageSize.width, imageSize.height);
            return;
        }

        gotBlackSnapshot = snapshotHash == *blackSnapshotHash;
        if (gotBlackSnapshot)
            WTFLogAlways("dumpPixelsAndCompareWithExpected: got all black snapshot (try %u of %u)", numTries, maxNumTries);
#endif
    } while (gotBlackSnapshot && numTries++ < maxNumTries);

    if (!compareActualHashToExpectedAndDumpResults(snapshotHash))
        dumpBitmap(context.get(), snapshotHash, imageSize, windowSize);
}

} // namespace WTR
