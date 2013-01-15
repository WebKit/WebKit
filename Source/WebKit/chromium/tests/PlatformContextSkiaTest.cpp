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
#include "ImageBuffer.h"
#include "NativeImageSkia.h"
#include "SkCanvas.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

#define EXPECT_PIXELS_MATCH(bitmap, opaqueRect) \
{ \
    SkAutoLockPixels locker(bitmap); \
    for (int y = opaqueRect.y(); y < opaqueRect.maxY(); ++y) \
        for (int x = opaqueRect.x(); x < opaqueRect.maxX(); ++x) { \
            int alpha = *bitmap.getAddr32(x, y) >> 24; \
            EXPECT_EQ(255, alpha); \
        } \
}

#define EXPECT_PIXELS_MATCH_EXACT(bitmap, opaqueRect) \
{ \
    SkAutoLockPixels locker(bitmap); \
    for (int y = 0; y < bitmap.height(); ++y) \
        for (int x = 0; x < bitmap.width(); ++x) {     \
            int alpha = *bitmap.getAddr32(x, y) >> 24; \
            bool opaque = opaqueRect.contains(x, y); \
            EXPECT_EQ(opaque, alpha == 255); \
        } \
}

TEST(PlatformContextSkiaTest, trackOpaqueTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 400, 400);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
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

TEST(PlatformContextSkiaTest, trackOpaqueClipTest)
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

    context.clearRect(FloatRect(10, 10, 90, 90));
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());

    context.save();
    context.clip(FloatRect(0, 0, 10, 10));
    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    context.restore();

    context.clearRect(FloatRect(10, 10, 90, 90));
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());

    context.save();
    context.clip(FloatRect(20, 20, 10, 10));
    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(20, 20, 10, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.clearRect(FloatRect(10, 10, 90, 90));
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());

    // The intersection of the two clips becomes empty.
    context.clip(FloatRect(30, 20, 10, 10));
    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    context.restore();

    context.clearRect(FloatRect(10, 10, 90, 90));
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());

    // The transform and the clip need to interact correctly (transform first)
    context.save();
    context.translate(10, 10);
    context.clip(FloatRect(20, 20, 10, 10));
    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(30, 30, 10, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    context.restore();

    context.clearRect(FloatRect(10, 10, 90, 90));
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());

    // The transform and the clip need to interact correctly (clip first)
    context.save();
    context.clip(FloatRect(20, 20, 10, 10));
    context.translate(10, 10);
    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(20, 20, 10, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    context.restore();

    context.clearRect(FloatRect(10, 10, 90, 90));
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());

    Path path;
    path.moveTo(FloatPoint(0, 0));
    path.addLineTo(FloatPoint(100, 0));

    // Non-rectangular clips just cause the paint to be considered non-opaque.
    context.save();
    context.clipPath(path, RULE_EVENODD);
    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    context.restore();

    // Another non-rectangular clip.
    context.save();
    context.clip(IntRect(30, 30, 20, 20));
    context.clipOut(IntRect(30, 30, 10, 10));
    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    context.restore();

    OwnPtr<ImageBuffer> alphaImage = ImageBuffer::create(IntSize(100, 100));
    alphaImage->context()->fillRect(IntRect(0, 0, 100, 100), alpha, ColorSpaceDeviceRGB);

    // Clipping with a non-opaque Image (there is no way to mark an ImageBuffer as opaque today).
    context.save();
    context.clipToImageBuffer(alphaImage.get(), FloatRect(30, 30, 10, 10));
    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    context.restore();
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackImageMask)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 400, 400);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    // Image masks are done by drawing a bitmap into a transparency layer that uses DstIn to mask
    // out a transparency layer below that is filled with the mask color. In the end this should
    // not be marked opaque.

    context.setCompositeOperation(CompositeSourceOver);
    context.beginTransparencyLayer(1);
    context.fillRect(FloatRect(10, 10, 10, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);

    context.setCompositeOperation(CompositeDestinationIn);
    context.beginTransparencyLayer(1);

    OwnPtr<ImageBuffer> alphaImage = ImageBuffer::create(IntSize(100, 100));
    alphaImage->context()->fillRect(IntRect(0, 0, 100, 100), alpha, ColorSpaceDeviceRGB);

    context.setCompositeOperation(CompositeSourceOver);
    context.drawImageBuffer(alphaImage.get(), ColorSpaceDeviceRGB, FloatRect(10, 10, 10, 10));

    context.endTransparencyLayer();
    context.endTransparencyLayer();

    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH_EXACT(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackImageMaskWithOpaqueRect)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 400, 400);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    // Image masks are done by drawing a bitmap into a transparency layer that uses DstIn to mask
    // out a transparency layer below that is filled with the mask color. In the end this should
    // not be marked opaque.

    context.setCompositeOperation(CompositeSourceOver);
    context.beginTransparencyLayer(1);
    context.fillRect(FloatRect(10, 10, 10, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);

    context.setCompositeOperation(CompositeDestinationIn);
    context.beginTransparencyLayer(1);

    OwnPtr<ImageBuffer> alphaImage = ImageBuffer::create(IntSize(100, 100));
    alphaImage->context()->fillRect(IntRect(0, 0, 100, 100), alpha, ColorSpaceDeviceRGB);

    context.setCompositeOperation(CompositeSourceOver);
    context.drawImageBuffer(alphaImage.get(), ColorSpaceDeviceRGB, FloatRect(10, 10, 10, 10));

    // We can't have an opaque mask actually, but we can pretend here like it would look if we did.
    context.fillRect(FloatRect(12, 12, 3, 3), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);

    context.endTransparencyLayer();
    context.endTransparencyLayer();

    EXPECT_EQ_RECT(IntRect(12, 12, 3, 3), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH_EXACT(bitmap, platformContext.opaqueRegion().asRect());
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
    bitmap.eraseColor(0);
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
    bitmap.eraseColor(0);
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

TEST(PlatformContextSkiaTest, trackOpaqueRoundedRectTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);
    IntSize radii(10, 10);

    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRoundedRect(IntRect(10, 10, 90, 90), radii, radii, radii, radii, opaque, ColorSpaceDeviceRGB);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.setCompositeOperation(CompositeSourceIn);
    context.setShouldAntialias(false);

    context.fillRoundedRect(IntRect(10, 10, 50, 30), radii, radii, radii, radii, opaque, ColorSpaceDeviceRGB);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRoundedRect(IntRect(10, 10, 30, 50), radii, radii, radii, radii, alpha, ColorSpaceDeviceRGB);
    EXPECT_EQ_RECT(IntRect(40, 10, 60, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRoundedRect(IntRect(10, 0, 50, 30), radii, radii, radii, radii, alpha, ColorSpaceDeviceRGB);
    EXPECT_EQ_RECT(IntRect(40, 30, 60, 70), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRoundedRect(IntRect(30, 0, 70, 50), radii, radii, radii, radii, opaque, ColorSpaceDeviceRGB);
    EXPECT_EQ_RECT(IntRect(40, 30, 60, 70), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackOpaqueIRectTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    SkPaint opaquePaint;
    opaquePaint.setColor(opaque.rgb());
    opaquePaint.setXfermodeMode(SkXfermode::kSrc_Mode);
    SkPaint alphaPaint;
    alphaPaint.setColor(alpha.rgb());
    alphaPaint.setXfermodeMode(SkXfermode::kSrc_Mode);

    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawIRect(SkIRect::MakeXYWH(10, 10, 90, 90), opaquePaint);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawIRect(SkIRect::MakeXYWH(0, 0, 100, 10), alphaPaint);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawIRect(SkIRect::MakeXYWH(0, 0, 100, 20), alphaPaint);
    EXPECT_EQ_RECT(IntRect(10, 20, 90, 80), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawIRect(SkIRect::MakeXYWH(50, 0, 50, 100), alphaPaint);
    EXPECT_EQ_RECT(IntRect(10, 20, 40, 80), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackOpaqueTextTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    SkPaint opaquePaint;
    opaquePaint.setColor(opaque.rgb());
    opaquePaint.setXfermodeMode(SkXfermode::kSrc_Mode);
    SkPaint alphaPaint;
    alphaPaint.setColor(alpha.rgb());
    alphaPaint.setXfermodeMode(SkXfermode::kSrc_Mode);

    SkPoint point = SkPoint::Make(0, 0);
    SkScalar pointX = 0;
    SkPath path;
    path.moveTo(SkPoint::Make(0, 0));
    path.lineTo(SkPoint::Make(100, 0));

    context.fillRect(FloatRect(50, 50, 50, 50), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(50, 50, 50, 50), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawPosText("A", 1, &point, opaquePaint);
    EXPECT_EQ_RECT(IntRect(50, 50, 50, 50), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawPosText("A", 1, &point, alphaPaint);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(50, 50, 50, 50), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(50, 50, 50, 50), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawPosTextH("A", 1, &pointX, 0, opaquePaint);
    EXPECT_EQ_RECT(IntRect(50, 50, 50, 50), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawPosTextH("A", 1, &pointX, 0, alphaPaint);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(50, 50, 50, 50), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(50, 50, 50, 50), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawTextOnPath("A", 1, path, 0, opaquePaint);
    EXPECT_EQ_RECT(IntRect(50, 50, 50, 50), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawTextOnPath("A", 1, path, 0, alphaPaint);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackOpaqueWritePixelsTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);

    SkBitmap opaqueBitmap;
    opaqueBitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    opaqueBitmap.allocPixels();
    opaqueBitmap.setIsOpaque(true);
    for (int y = 0; y < opaqueBitmap.height(); ++y)
        for (int x = 0; x < opaqueBitmap.width(); ++x)
            *opaqueBitmap.getAddr32(x, y) = 0xFFFFFFFF;

    SkBitmap alphaBitmap;
    alphaBitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    alphaBitmap.allocPixels();
    alphaBitmap.setIsOpaque(false);
    for (int y = 0; y < alphaBitmap.height(); ++y)
        for (int x = 0; x < alphaBitmap.width(); ++x)
            *alphaBitmap.getAddr32(x, y) = 0x00000000;

    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);

    platformContext.writePixels(opaqueBitmap, 50, 50);
    EXPECT_EQ_RECT(IntRect(50, 50, 10, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.writePixels(alphaBitmap, 10, 0);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.writePixels(alphaBitmap, 10, 1);
    EXPECT_EQ_RECT(IntRect(10, 11, 90, 89), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.writePixels(alphaBitmap, 0, 10);
    EXPECT_EQ_RECT(IntRect(10, 11, 90, 89), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.writePixels(alphaBitmap, 1, 10);
    EXPECT_EQ_RECT(IntRect(11, 11, 89, 89), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackOpaqueDrawBitmapTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);

    SkBitmap opaqueBitmap;
    opaqueBitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    opaqueBitmap.allocPixels();
    opaqueBitmap.setIsOpaque(true);
    for (int y = 0; y < opaqueBitmap.height(); ++y)
        for (int x = 0; x < opaqueBitmap.width(); ++x)
            *opaqueBitmap.getAddr32(x, y) = 0xFFFFFFFF;

    SkBitmap alphaBitmap;
    alphaBitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    alphaBitmap.allocPixels();
    alphaBitmap.setIsOpaque(false);
    for (int y = 0; y < alphaBitmap.height(); ++y)
        for (int x = 0; x < alphaBitmap.width(); ++x)
            *alphaBitmap.getAddr32(x, y) = 0x00000000;

    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);

    platformContext.drawBitmap(opaqueBitmap, 10, 10, &paint);
    EXPECT_EQ_RECT(IntRect(10, 10, 10, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    context.fillRect(FloatRect(10, 10, 90, 90), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawBitmap(alphaBitmap, 10, 0, &paint);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawBitmap(alphaBitmap, 10, 1, &paint);
    EXPECT_EQ_RECT(IntRect(10, 11, 90, 89), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawBitmap(alphaBitmap, 0, 10, &paint);
    EXPECT_EQ_RECT(IntRect(10, 11, 90, 89), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawBitmap(alphaBitmap, 1, 10, &paint);
    EXPECT_EQ_RECT(IntRect(11, 11, 89, 89), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, trackOpaqueDrawBitmapRectTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);

    SkBitmap opaqueBitmap;
    opaqueBitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    opaqueBitmap.allocPixels();
    opaqueBitmap.setIsOpaque(true);
    for (int y = 0; y < opaqueBitmap.height(); ++y)
        for (int x = 0; x < opaqueBitmap.width(); ++x)
            *opaqueBitmap.getAddr32(x, y) = 0xFFFFFFFF;

    SkBitmap alphaBitmap;
    alphaBitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    alphaBitmap.allocPixels();
    alphaBitmap.setIsOpaque(false);
    for (int y = 0; y < alphaBitmap.height(); ++y)
        for (int x = 0; x < alphaBitmap.width(); ++x)
            *alphaBitmap.getAddr32(x, y) = 0x00000000;

    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);

    platformContext.drawBitmapRect(opaqueBitmap, 0, SkRect::MakeXYWH(10, 10, 90, 90), &paint);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawBitmapRect(alphaBitmap, 0, SkRect::MakeXYWH(10, 0, 10, 10), &paint);
    EXPECT_EQ_RECT(IntRect(10, 10, 90, 90), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawBitmapRect(alphaBitmap, 0, SkRect::MakeXYWH(10, 0, 10, 11), &paint);
    EXPECT_EQ_RECT(IntRect(10, 11, 90, 89), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawBitmapRect(alphaBitmap, 0, SkRect::MakeXYWH(0, 10, 10, 10), &paint);
    EXPECT_EQ_RECT(IntRect(10, 11, 90, 89), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());

    platformContext.drawBitmapRect(alphaBitmap, 0, SkRect::MakeXYWH(0, 10, 11, 10), &paint);
    EXPECT_EQ_RECT(IntRect(11, 11, 89, 89), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, contextTransparencyLayerTest)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 400, 400);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);
    
    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    
    context.fillRect(FloatRect(20, 20, 10, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(20, 20, 10, 10), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
    
    context.clearRect(FloatRect(20, 20, 10, 10));
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());

    context.beginTransparencyLayer(0.5);
    context.save();
    context.fillRect(FloatRect(20, 20, 10, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    context.restore();
    context.endTransparencyLayer();
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());

    context.clearRect(FloatRect(20, 20, 10, 10));
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());

    context.beginTransparencyLayer(0.5);
    context.fillRect(FloatRect(20, 20, 10, 10), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    context.endTransparencyLayer();
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, UnboundedDrawsAreClipped)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 400, 400);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    Path path;
    context.setShouldAntialias(false);
    context.setMiterLimit(1);
    context.setStrokeThickness(5);
    context.setLineCap(SquareCap);
    context.setStrokeStyle(SolidStroke);

    // Make skia unable to compute fast bounds for our paths.
    DashArray dashArray;
    dashArray.append(1);
    dashArray.append(0);
    context.setLineDash(dashArray, 0);

    // Make the device opaque in 10,10 40x40.
    context.fillRect(FloatRect(10, 10, 40, 40), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 40, 40), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH_EXACT(bitmap, platformContext.opaqueRegion().asRect());

    // Clip to the left edge of the opaque area.
    context.clip(IntRect(10, 10, 10, 40));

    // Draw a path that gets clipped. This should destroy the opaque area but only inside the clip.
    context.setCompositeOperation(CompositeSourceOut);
    context.setFillColor(alpha, ColorSpaceDeviceRGB);
    path.moveTo(FloatPoint(10, 10));
    path.addLineTo(FloatPoint(40, 40));
    context.strokePath(path);

    EXPECT_EQ_RECT(IntRect(20, 10, 30, 40), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH(bitmap, platformContext.opaqueRegion().asRect());
}

TEST(PlatformContextSkiaTest, PreserveOpaqueOnlyMattersForFirstLayer)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 400, 400);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    PlatformContextSkia platformContext(&canvas);
    platformContext.setTrackOpaqueRegion(true);
    GraphicsContext context(&platformContext);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);

    Path path;
    context.setShouldAntialias(false);
    context.setMiterLimit(1);
    context.setStrokeThickness(5);
    context.setLineCap(SquareCap);
    context.setStrokeStyle(SolidStroke);

    // Make skia unable to compute fast bounds for our paths.
    DashArray dashArray;
    dashArray.append(1);
    dashArray.append(0);
    context.setLineDash(dashArray, 0);

    // Make the device opaque in 10,10 40x40.
    context.fillRect(FloatRect(10, 10, 40, 40), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);
    EXPECT_EQ_RECT(IntRect(10, 10, 40, 40), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH_EXACT(bitmap, platformContext.opaqueRegion().asRect());

    // Begin a layer that preserves opaque.
    context.setCompositeOperation(CompositeSourceOver);
    context.beginTransparencyLayer(0.5);

    // Begin a layer that does not preserve opaque.
    context.setCompositeOperation(CompositeSourceOut);
    context.beginTransparencyLayer(0.5);

    // This should not destroy the device opaqueness.
    context.fillRect(FloatRect(10, 10, 40, 40), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);

    // This should not destroy the device opaqueness either.
    context.setFillColor(opaque, ColorSpaceDeviceRGB);
    path.moveTo(FloatPoint(10, 10));
    path.addLineTo(FloatPoint(40, 40));
    context.strokePath(path);

    context.endTransparencyLayer();
    context.endTransparencyLayer();
    EXPECT_EQ_RECT(IntRect(10, 10, 40, 40), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH_EXACT(bitmap, platformContext.opaqueRegion().asRect());

    // Now begin a layer that does not preserve opaque and draw through it to the device.
    context.setCompositeOperation(CompositeSourceOut);
    context.beginTransparencyLayer(0.5);

    // This should destroy the device opaqueness.
    context.fillRect(FloatRect(10, 10, 40, 40), opaque, ColorSpaceDeviceRGB, CompositeSourceOver);

    context.endTransparencyLayer();
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH_EXACT(bitmap, platformContext.opaqueRegion().asRect());

    // Now we draw with a path for which it cannot compute fast bounds. This should destroy the entire opaque region.

    context.setCompositeOperation(CompositeSourceOut);
    context.beginTransparencyLayer(0.5);

    // This should nuke the device opaqueness.
    context.setFillColor(opaque, ColorSpaceDeviceRGB);
    path.moveTo(FloatPoint(10, 10));
    path.addLineTo(FloatPoint(40, 40));
    context.strokePath(path);

    context.endTransparencyLayer();
    EXPECT_EQ_RECT(IntRect(), platformContext.opaqueRegion().asRect());
    EXPECT_PIXELS_MATCH_EXACT(bitmap, platformContext.opaqueRegion().asRect());
}

#if defined(SK_SUPPORT_HINTING_SCALE_FACTOR)
TEST(PlatformContextSkiaTest, hintingScaleFactorAppliedOnPaint)
{
    SkBitmap bitmap;
    SkCanvas canvas(bitmap);

    SkScalar hintingScale = SkIntToScalar(2);
    PlatformContextSkia platformContext(&canvas);
    platformContext.setHintingScaleFactor(hintingScale); 

    SkPaint paint;
    EXPECT_EQ(paint.getHintingScaleFactor(), SK_Scalar1);
    platformContext.setupPaintCommon(&paint);
    EXPECT_EQ(paint.getHintingScaleFactor(), hintingScale);
}
#endif

} // namespace
