/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "DumpRenderTree.h"
#include "PixelDumpSupportCG.h"

#include "LayoutTestController.h"
#include <ImageIO/CGImageDestination.h>
#include <wtf/Assertions.h>
#include <wtf/RetainPtr.h>
#include <wtf/StringExtras.h>

#if PLATFORM(WIN)
#include "MD5.h"
#elif PLATFORM(MAC)
#include <LaunchServices/UTCoreTypes.h>
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>
#endif

#if PLATFORM(WIN)
static const CFStringRef kUTTypePNG = CFSTR("public.png");
#endif

static void printPNG(CGImageRef image)
{
    RetainPtr<CFMutableDataRef> imageData(AdoptCF, CFDataCreateMutable(0, 0));
    RetainPtr<CGImageDestinationRef> imageDest(AdoptCF, CGImageDestinationCreateWithData(imageData.get(), kUTTypePNG, 1, 0));
    CGImageDestinationAddImage(imageDest.get(), image, 0);
    CGImageDestinationFinalize(imageDest.get());
    printf("Content-length: %lu\n", CFDataGetLength(imageData.get()));
    fwrite(CFDataGetBytePtr(imageData.get()), 1, CFDataGetLength(imageData.get()), stdout);
}

static void getMD5HashStringForBitmap(CGContextRef bitmap, char string[33])
{
    MD5_CTX md5Context;
    unsigned char hash[16];

    size_t bitsPerPixel = CGBitmapContextGetBitsPerPixel(bitmap);
    ASSERT(bitsPerPixel == 32); // ImageDiff assumes 32 bit RGBA, we must as well.
    size_t bytesPerPixel = bitsPerPixel / 8;
    size_t pixelsHigh = CGBitmapContextGetHeight(bitmap);
    size_t pixelsWide = CGBitmapContextGetWidth(bitmap);
    size_t bytesPerRow = CGBitmapContextGetBytesPerRow(bitmap);
    ASSERT(bytesPerRow >= (pixelsWide * bytesPerPixel));

    MD5_Init(&md5Context);
    unsigned char* bitmapData = static_cast<unsigned char*>(CGBitmapContextGetData(bitmap));
    for (unsigned row = 0; row < pixelsHigh; row++) {
        MD5_Update(&md5Context, bitmapData, static_cast<unsigned>(pixelsWide * bytesPerPixel));
        bitmapData += bytesPerRow;
    }
    MD5_Final(hash, &md5Context);

    string[0] = '\0';
    for (int i = 0; i < 16; i++)
        snprintf(string, 33, "%s%02x", string, hash[i]);
}

void drawSelectionRect(CGContextRef context, const CGRect& rect)
{
    CGContextSaveGState(context);
    CGContextSetRGBStrokeColor(context, 1.0, 0.0, 0.0, 1.0);
    CGContextStrokeRect(context, rect);
    CGContextRestoreGState(context);
}

void dumpWebViewAsPixelsAndCompareWithExpected(const char* /*currentTest*/, bool /*forceAllTestsToDumpPixels*/)
{
    RetainPtr<CGContextRef> context = getBitmapContextFromWebView();

#if PLATFORM(MAC)
    if (layoutTestController->testRepaint())
        repaintWebView(context.get(), layoutTestController->testRepaintSweepHorizontally());
    else 
        paintWebView(context.get());

    if (layoutTestController->dumpSelectionRect())
        drawSelectionRect(context.get(), getSelectionRect());
#endif

    // Compute the actual hash to compare to the expected image's hash.
    char actualHash[33];
    getMD5HashStringForBitmap(context.get(), actualHash);
    printf("\nActualHash: %s\n", actualHash);

    // FIXME: We should compare the actualHash to the expected hash here and
    // only set dumpImage to true if they don't match, but DRT doesn't have
    // enough information currently to find the expected checksum file.
    bool dumpImage = true;

    if (dumpImage) {
        RetainPtr<CGImageRef> image(AdoptCF, CGBitmapContextCreateImage(context.get()));
        printPNG(image.get());
    }

    printf("#EOF\n");
}
