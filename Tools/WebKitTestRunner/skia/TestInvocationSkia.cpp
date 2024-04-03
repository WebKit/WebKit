/*
 * Copyright (C) 2024 Igalia S.L.
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

#if USE(SKIA)
#include "PixelDumpSupport.h"
#include "PlatformWebView.h"
#include "TestController.h"
#include <WebKit/WKImageSkia.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkStream.h>
IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/encode/SkPngEncoder.h>
IGNORE_CLANG_WARNINGS_END
#include <wtf/ASCIICType.h>
#include <wtf/SHA1.h>

namespace WTR {

static std::string computeSHA1HashStringForPixmap(const SkPixmap& pixmap)
{
    size_t pixelsWidth = pixmap.width();
    size_t pixelsHight = pixmap.height();
    size_t bytesPerRow = pixmap.info().minRowBytes();

    SHA1 sha1;
    const auto* bitmapData = pixmap.addr8();
    for (size_t row = 0; row < pixelsHight; ++row) {
        sha1.addBytes(std::span { bitmapData, 4 * pixelsWidth });
        bitmapData += bytesPerRow;
    }
    auto hexString = sha1.computeHexDigest();

    auto result = hexString.toStdString();
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return toASCIILower(c); });
    return result;
}

static void dumpPixmap(const SkPixmap& pixmap, const std::string& checksum)
{
    SkDynamicMemoryWStream stream;
    SkPngEncoder::Encode(&stream, pixmap, { });
    auto data = stream.detachAsData();
    printPNG(data->bytes(), data->size(), checksum.c_str());
}

void TestInvocation::dumpPixelsAndCompareWithExpected(SnapshotResultType snapshotType, WKArrayRef repaintRects, WKImageRef webImage)
{
    sk_sp<SkImage> image;
    switch (snapshotType) {
    case SnapshotResultType::WebContents:
        image.reset(WKImageCreateSkImage(webImage));
        break;
    case SnapshotResultType::WebView:
        image.reset(TestController::singleton().mainWebView()->windowSnapshotImage());
        break;
    }

    SkPixmap pixmap;
    if (!image->peekPixels(&pixmap))
        return;

    if (repaintRects) {
        auto canvas = SkCanvas::MakeRasterDirect(image->imageInfo(), pixmap.writable_addr(), image->imageInfo().minRowBytes());
        canvas->saveLayer(SkRect::MakeWH(image->width(), image->height()), nullptr);
        canvas->drawColor(SkColor4f { 0, 0, 0, 0.66 });

        SkPaint paint;
        paint.setColor(SkColor4f { 0, 0, 0, 0 });
        paint.setBlendMode(SkBlendMode::kSrc);

        size_t count = WKArrayGetSize(repaintRects);
        for (size_t i = 0; i < count; ++i) {
            WKRect rect = WKRectGetValue(static_cast<WKRectRef>(WKArrayGetItemAtIndex(repaintRects, i)));
            canvas->drawRect(SkRect::MakeXYWH(static_cast<float>(rect.origin.x), static_cast<float>(rect.origin.y), static_cast<float>(rect.size.width), static_cast<float>(rect.size.height)), paint);
        }
        canvas->restore();
    }

    auto snapshotHash = computeSHA1HashStringForPixmap(pixmap);
    if (!compareActualHashToExpectedAndDumpResults(snapshotHash))
        dumpPixmap(pixmap, snapshotHash);
}

} // namespace WTR

#endif // USE(SKIA)
