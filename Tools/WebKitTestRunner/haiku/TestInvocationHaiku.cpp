/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *           (C) 2011 Brent Fulgham <bfulgham@webkit.org>. All rights reserved.
 *           (C) 2010, 2011 Igalia S.L
 *           (C) 2012 Intel Corporation. All rights reserved.
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

#include <Bitmap.h>
#include "NotImplemented.h"
#include "PixelDumpSupport.h"
#include "PlatformWebView.h"
#include "TestController.h"
#include <WebKit/WKImageCairo.h>
#include <cstdio>
#include <wtf/Assertions.h>
#include <wtf/MD5.h>
#include <wtf/StringExtras.h>

namespace WTR {

void computeMD5HashStringForCairoSurface(BBitmap* surface, char hashString[33])
{
    ASSERT(surface->ColorSpace() == B_RGB32); // ImageDiff assumes 32 bit RGBA, we must as well.

    BRect r = surface->Bounds();
    size_t pixelsHigh = r.Height();
    size_t pixelsWide = r.Width();
    size_t bytesPerRow = surface->BytesPerRow();

    MD5 md5Context;
    unsigned char* bitmapData = static_cast<unsigned char*>(surface->Bits());
    for (size_t row = 0; row < pixelsHigh; ++row) {
        md5Context.addBytes(bitmapData, 4 * pixelsWide);
        bitmapData += bytesPerRow;
    }
    MD5::Digest hash;
    md5Context.checksum(hash);

    snprintf(hashString, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
        hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
}

#if 0
static status_t writeFunction(void* closure, const unsigned char* data, unsigned length)
{
    Vector<unsigned char>* in = reinterpret_cast<Vector<unsigned char>*>(closure);
    in->append(data, length);
    return B_OK;
}
#endif

static void dumpBitmap(BBitmap* surface, const char* checksum)
{
    notImplemented();
#if 0
    Vector<unsigned char> pixelData;
    cairo_surface_write_to_png_stream(surface, writeFunction, &pixelData);
    const size_t dataLength = pixelData.size();
    const unsigned char* data = pixelData.data();

    printPNG(data, dataLength, checksum);
#endif
}

void TestInvocation::dumpPixelsAndCompareWithExpected(WKImageRef wkImage, WKArrayRef repaintRects)
{
    notImplemented();
#if 0
#if PLATFORM(EFL) || PLATFORM(GTK)
    UNUSED_PARAM(wkImage);
    cairo_surface_t* surface = WKImageCreateCairoSurface(TestController::shared().mainWebView()->windowSnapshotImage().get());
#else
    cairo_surface_t* surface = WKImageCreateCairoSurface(wkImage);
#endif

    if (repaintRects)
        paintRepaintRectOverlay(surface, repaintRects);

    char actualHash[33];
    computeMD5HashStringForCairoSurface(surface, actualHash);
    if (!compareActualHashToExpectedAndDumpResults(actualHash))
        dumpBitmap(surface, actualHash);

    cairo_surface_destroy(surface);
#endif
}

} // namespace WTR

