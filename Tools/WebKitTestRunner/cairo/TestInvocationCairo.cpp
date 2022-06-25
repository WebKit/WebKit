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

#include "PixelDumpSupport.h"
#include "PlatformWebView.h"
#include "TestController.h"
#include <WebKit/WKImageCairo.h>
#include <cairo/cairo.h>
#include <cstdio>
#include <wtf/Assertions.h>
#include <wtf/SHA1.h>
#include <wtf/StringExtras.h>

namespace WTR {

static void computeSHA1HashStringForCairoSurface(cairo_surface_t* surface, char hashString[33])
{
    ASSERT(cairo_image_surface_get_format(surface) == CAIRO_FORMAT_ARGB32 || cairo_image_surface_get_format(surface) == CAIRO_FORMAT_RGB24);

    size_t pixelsHigh = cairo_image_surface_get_height(surface);
    size_t pixelsWide = cairo_image_surface_get_width(surface);
    size_t bytesPerRow = cairo_image_surface_get_stride(surface);

    SHA1 sha1;
    unsigned char* bitmapData = static_cast<unsigned char*>(cairo_image_surface_get_data(surface));
    for (size_t row = 0; row < pixelsHigh; ++row) {
        sha1.addBytes(bitmapData, 4 * pixelsWide);
        bitmapData += bytesPerRow;
    }
    SHA1::Digest hash;
    sha1.computeHash(hash);

    snprintf(hashString, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
        hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
}

static cairo_status_t writeFunction(void* closure, const unsigned char* data, unsigned length)
{
    Vector<unsigned char>* in = reinterpret_cast<Vector<unsigned char>*>(closure);
    in->append(data, length);
    return CAIRO_STATUS_SUCCESS;
}

static void dumpBitmap(cairo_surface_t* surface, const char* checksum)
{
    Vector<unsigned char> pixelData;
    cairo_surface_write_to_png_stream(surface, writeFunction, &pixelData);
    const size_t dataLength = pixelData.size();
    const unsigned char* data = pixelData.data();

    printPNG(data, dataLength, checksum);
}

static void paintRepaintRectOverlay(cairo_surface_t* surface, WKArrayRef repaintRects)
{
    cairo_t* context = cairo_create(surface);

    cairo_push_group(context);

    // Paint the gray mask over the original image.
    cairo_set_source_rgba(context, 0, 0, 0, 0.66);
    cairo_paint(context);

    // Paint transparent rectangles over the mask to show the repainted regions.
    cairo_set_source_rgba(context, 0, 0, 0, 0);
    cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
    size_t count = WKArrayGetSize(repaintRects);
    for (size_t i = 0; i < count; ++i) {
        WKRect rect = WKRectGetValue(static_cast<WKRectRef>(WKArrayGetItemAtIndex(repaintRects, i)));
        cairo_rectangle(context, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
        cairo_fill(context);
    }

    cairo_pop_group_to_source(context);
    cairo_paint(context);

    cairo_destroy(context);
}

void TestInvocation::dumpPixelsAndCompareWithExpected(SnapshotResultType snapshotType, WKArrayRef repaintRects, WKImageRef image)
{
    cairo_surface_t* surface = nullptr;
    switch (snapshotType) {
    case SnapshotResultType::WebContents:
        surface = WKImageCreateCairoSurface(image);
        break;
    case SnapshotResultType::WebView:
        surface = TestController::singleton().mainWebView()->windowSnapshotImage();
        break;
    }

    if (repaintRects)
        paintRepaintRectOverlay(surface, repaintRects);

    char actualHash[33];
    computeSHA1HashStringForCairoSurface(surface, actualHash);
    if (!compareActualHashToExpectedAndDumpResults(actualHash))
        dumpBitmap(surface, actualHash);

    cairo_surface_destroy(surface);
}

} // namespace WTR
