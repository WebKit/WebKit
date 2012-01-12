/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "PlatformContextSkia.h"

#include "BitmapImageSingleFrameSkia.h"
#include "GraphicsContext.h"
#include "NativeImageSkia.h"
#include "SkCanvas.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.maxX(), b.maxX()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.maxY(), b.maxY());

#define EXPECT_PIXELS_MATCH(bitmap, opaqueRect) \
{ \
    SkAutoLockPixels locker(bitmap); \
    for (int y = opaqueRect.y(); y < opaqueRect.maxY(); ++y) \
        for (int x = opaqueRect.x(); x < opaqueRect.maxX(); ++x) { \
            int alpha = *bitmap.getAddr32(x, y) >> 24; \
            EXPECT_EQ(255, alpha); \
        } \
}

TEST(PlatformContextSkiaTest, trackOpaqueTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 400, 400);
    bitmap.allocPixels();
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(10, 10, 90, 90), alpha, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(99, 13, 10, 90), opaque, ColorSpaceDeviceRGB, CompositePlusLighter);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(99, 13, 10, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceIn);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(99, 13, 10, 90), alpha, ColorSpaceDeviceRGB, CompositeSourceIn);
    EXPECT_EQ_RECT(IntRect(10, 10, 89, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(8, 8, 3, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOut);
    EXPECT_EQ_RECT(IntRect(11, 10, 88, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(30, 30, 290, 290), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(30, 30, 290, 290), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(40, 20, 290, 50), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(30, 30, 290, 290), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(10, 10, 390, 50), opaque, ColorSpaceDeviceRGB, CompositeSourceIn);
    EXPECT_EQ_RECT(IntRect(30, 30, 290, 290), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(10, 10, 390, 50), alpha, ColorSpaceDeviceRGB);
    EXPECT_EQ_RECT(IntRect(30, 30, 290, 290), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(10, 10, 390, 50), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(30, 10, 290, 310), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackOpaqueJoinTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 400, 400);
    bitmap.allocPixels();
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    context.fillRect(FloatRect(20, 20, 10, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(20, 20, 10, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    // Doesn't join
    context.fillRect(FloatRect(31, 20, 10, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(20, 20, 10, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    // Does join
    context.fillRect(FloatRect(30, 20, 10, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(20, 20, 20, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    // Doesn't join
    context.fillRect(FloatRect(20, 31, 20, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(20, 20, 20, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    // Does join
    context.fillRect(FloatRect(20, 30, 20, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(20, 20, 20, 20), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    // Doesn't join
    context.fillRect(FloatRect(9, 20, 10, 20), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(20, 20, 20, 20), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    // Does join
    context.fillRect(FloatRect(10, 20, 10, 20), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 20, 30, 20), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    // Doesn't join
    context.fillRect(FloatRect(10, 9, 30, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 20, 30, 20), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    // Does join
    context.fillRect(FloatRect(10, 10, 30, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 30, 30), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackOpaqueLineTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    context.setShouldAntialias(false);
    context.setMiterLimit(0);
    context.setStrokeThickness(4);
    context.setLineCap(SquareCap);
    context.setStrokeStyle(SolidStroke);
    context.setCompositeOperation(CompositeSourceOver);

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setCompositeOperation(CompositeSourceIn);

    context.save();
    context.setStrokeColor(alpha, ColorSpaceDeviceRGB);
    context.drawLine(IntPoint(0, 0), IntPoint(100, 0));
    context.restore();
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.save();
    context.setStrokeColor(opaque, ColorSpaceDeviceRGB);
    context.drawLine(IntPoint(0, 10), IntPoint(100, 10));
    context.restore();
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.save();
    context.setStrokeColor(alpha, ColorSpaceDeviceRGB);
    context.drawLine(IntPoint(0, 10), IntPoint(100, 10));
    context.restore();
    EXPECT_EQ_RECT(IntRect(10, 13, 90, 87), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.save();
    context.setStrokeColor(alpha, ColorSpaceDeviceRGB);
    context.drawLine(IntPoint(0, 11), IntPoint(100, 11));
    context.restore();
    EXPECT_EQ_RECT(IntRect(10, 14, 90, 86), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setShouldAntialias(true);
    context.setCompositeOperation(CompositeSourceOver);

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setCompositeOperation(CompositeSourceIn);

    context.save();
    context.setStrokeColor(alpha, ColorSpaceDeviceRGB);
    context.drawLine(IntPoint(0, 0), IntPoint(100, 0));
    context.restore();
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setShouldAntialias(false);
    context.save();
    context.setStrokeColor(opaque, ColorSpaceDeviceRGB);
    context.drawLine(IntPoint(0, 10), IntPoint(100, 10));
    context.restore();
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setShouldAntialias(true);
    context.save();
    context.setStrokeColor(opaque, ColorSpaceDeviceRGB);
    context.drawLine(IntPoint(0, 10), IntPoint(100, 10));
    context.restore();
    EXPECT_EQ_RECT(IntRect(10, 13, 90, 87), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.save();
    context.setStrokeColor(alpha, ColorSpaceDeviceRGB);
    context.drawLine(IntPoint(0, 11), IntPoint(100, 11));
    context.restore();
    EXPECT_EQ_RECT(IntRect(10, 14, 90, 86), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackOpaquePathTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setShouldAntialias(false);
    context.setMiterLimit(1);
    context.setStrokeThickness(5);
    context.setLineCap(SquareCap);
    context.setStrokeStyle(SolidStroke);
    context.setCompositeOperation(CompositeSourceIn);

    Path path;

    context.setFillColor(alpha, ColorSpaceDeviceRGB);
    path.moveTo(FloatPoint(0, 0));
    path.addLineTo(FloatPoint(100, 0));
    context.fillPath(path);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    path.clear();

    context.setFillColor(opaque, ColorSpaceDeviceRGB);
    path.moveTo(FloatPoint(0, 10));
    path.addLineTo(FloatPoint(100, 13));
    context.fillPath(path);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    path.clear();

    context.setFillColor(alpha, ColorSpaceDeviceRGB);
    path.moveTo(FloatPoint(0, 10));
    path.addLineTo(FloatPoint(100, 13));
    context.fillPath(path);
    EXPECT_EQ_RECT(IntRect(10, 13, 90, 87), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    path.clear();

    context.setFillColor(alpha, ColorSpaceDeviceRGB);
    path.moveTo(FloatPoint(0, 14));
    path.addLineTo(FloatPoint(100, 10));
    context.fillPath(path);
    EXPECT_EQ_RECT(IntRect(10, 14, 90, 86), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    path.clear();
}

TEST(PlatformContextSkiaTest, trackOpaqueImageTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    SkBitmap drawBitmap;
    drawBitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    drawBitmap.allocPixels();

    drawBitmap.setIsOpaque(true);
    for (int y = 0; y < drawBitmap.height(); ++y)
        for (int x = 0; x < drawBitmap.width(); ++x)
            *drawBitmap.getAddr32(x, y) = 0xFFFFFFFF;
    RefPtr<BitmapImageSingleFrameSkia> opaqueImage = BitmapImageSingleFrameSkia::create(drawBitmap, true);
    EXPECT_FALSE(opaqueImage->currentFrameHasAlpha());

    drawBitmap.setIsOpaque(false);
    for (int y = 0; y < drawBitmap.height(); ++y)
        for (int x = 0; x < drawBitmap.width(); ++x)
            *drawBitmap.getAddr32(x, y) = 0x00000000;
    RefPtr<BitmapImageSingleFrameSkia> alphaImage = BitmapImageSingleFrameSkia::create(drawBitmap, true);
    EXPECT_TRUE(alphaImage->currentFrameHasAlpha());

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.drawImage(opaqueImage.get(), ColorSpaceDeviceRGB, IntPoint(0, 0));
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    context.drawImage(alphaImage.get(), ColorSpaceDeviceRGB, IntPoint(0, 0));
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.drawImage(opaqueImage.get(), ColorSpaceDeviceRGB, IntPoint(5, 5));
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    context.drawImage(alphaImage.get(), ColorSpaceDeviceRGB, IntPoint(5, 5));
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.drawImage(opaqueImage.get(), ColorSpaceDeviceRGB, IntPoint(10, 10));
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    context.drawImage(alphaImage.get(), ColorSpaceDeviceRGB, IntPoint(10, 10));
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.drawImage(alphaImage.get(), ColorSpaceDeviceRGB, IntPoint(20, 10), CompositeSourceIn);
    EXPECT_EQ_RECT(IntRect(10, 20, 90, 80), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.save();
    context.setAlpha(0.5);
    context.drawImage(opaqueImage.get(), ColorSpaceDeviceRGB, IntPoint(25, 15), CompositeSourceIn);
    context.restore();
    EXPECT_EQ_RECT(IntRect(10, 25, 90, 75), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.drawImage(alphaImage.get(), ColorSpaceDeviceRGB, IntPoint(10, 20), CompositeSourceIn);
    EXPECT_EQ_RECT(IntRect(20, 10, 80, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.save();
    context.setAlpha(0.5);
    context.drawImage(opaqueImage.get(), ColorSpaceDeviceRGB, IntPoint(15, 25), CompositeSourceIn);
    context.restore();
    EXPECT_EQ_RECT(IntRect(25, 10, 75, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackOpaqueOvalTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.drawEllipse(IntRect(10, 10, 90, 90));
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setCompositeOperation(CompositeSourceIn);

    context.setShouldAntialias(false);

    context.setFillColor(opaque, ColorSpaceDeviceRGB);
    context.drawEllipse(IntRect(10, 10, 50, 30));
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setFillColor(alpha, ColorSpaceDeviceRGB);
    context.drawEllipse(IntRect(10, 10, 30, 50));
    EXPECT_EQ_RECT(IntRect(40, 10, 60, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setShouldAntialias(true);

    context.setFillColor(opaque, ColorSpaceDeviceRGB);
    context.drawEllipse(IntRect(10, 10, 50, 30));
    EXPECT_EQ_RECT(IntRect(40, 41, 60, 59), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setFillColor(alpha, ColorSpaceDeviceRGB);
    context.drawEllipse(IntRect(20, 10, 30, 50));
    EXPECT_EQ_RECT(IntRect(51, 41, 49, 59), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

} // namespace
