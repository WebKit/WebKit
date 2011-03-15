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

#import "config.h"
#import "TestInvocation.h"

#import "PlatformWebView.h"
#import "TestController.h"
#import <ImageIO/CGImageDestination.h>
#import <LaunchServices/UTCoreTypes.h>
#import <WebKit2/WKPage.h>
#import <wtf/RetainPtr.h>

#define COMMON_DIGEST_FOR_OPENSSL
#import <CommonCrypto/CommonDigest.h>

namespace WTR {

static CGContextRef createCGContextFromPlatformView(PlatformWebView* platformWebView)
{
    WKView* view = platformWebView->platformView();
    [view display];

    NSSize webViewSize = [view frame].size;
    size_t pixelsWide = static_cast<size_t>(webViewSize.width);
    size_t pixelsHigh = static_cast<size_t>(webViewSize.height);
    size_t rowBytes = (4 * pixelsWide + 63) & ~63;
    void* buffer = calloc(pixelsHigh, rowBytes);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    CGContextRef context = CGBitmapContextCreate(buffer, pixelsWide, pixelsHigh, 8, rowBytes, colorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(colorSpace);
    
    CGImageRef image = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, [[view window] windowNumber], kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque);
    CGContextDrawImage(context, CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image)), image);
    CGImageRelease(image);

    return context;
}

void computeMD5HashStringForContext(CGContextRef bitmapContext, char hashString[33])
{
    ASSERT(CGBitmapContextGetBitsPerPixel(bitmapContext) == 32); // ImageDiff assumes 32 bit RGBA, we must as well.
    size_t pixelsHigh = CGBitmapContextGetHeight(bitmapContext);
    size_t pixelsWide = CGBitmapContextGetWidth(bitmapContext);
    size_t bytesPerRow = CGBitmapContextGetBytesPerRow(bitmapContext);
    
    // We need to swap the bytes to ensure consistent hashes independently of endianness
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    unsigned char* bitmapData = static_cast<unsigned char*>(CGBitmapContextGetData(bitmapContext));
    if ((CGBitmapContextGetBitmapInfo(bitmapContext) & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Big) {
        for (unsigned row = 0; row < pixelsHigh; row++) {
            uint32_t buffer[pixelsWide];
            for (unsigned column = 0; column < pixelsWide; column++)
                buffer[column] = OSReadLittleInt32(bitmapData, 4 * column);
            MD5_Update(&md5Context, buffer, 4 * pixelsWide);
            bitmapData += bytesPerRow;
        }
    } else {
        for (unsigned row = 0; row < pixelsHigh; row++) {
            MD5_Update(&md5Context, bitmapData, 4 * pixelsWide);
            bitmapData += bytesPerRow;
        }
    }

    unsigned char hash[16];
    MD5_Final(hash, &md5Context);
    
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

static void forceRepaintFunction(WKErrorRef, void* context)
{
    *static_cast<bool*>(context) = true;
}

void TestInvocation::dumpPixelsAndCompareWithExpected()
{
    WKPageForceRepaint(TestController::shared().mainWebView()->page(), &m_gotRepaint, forceRepaintFunction);
    TestController::shared().runUntil(m_gotRepaint, TestController::LongTimeout);
    
    CGContextRef context = createCGContextFromPlatformView(TestController::shared().mainWebView());
    
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
