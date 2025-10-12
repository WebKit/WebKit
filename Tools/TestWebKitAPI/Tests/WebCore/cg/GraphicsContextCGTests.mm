/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if USE(CG)

#import "Test.h"
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DestinationColorSpace.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurface.h>
#import <WebCore/NativeImage.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

@interface TestDrawingLayer : CALayer
@property (nonatomic) bool didDraw;
@property (nonatomic) WebCore::RenderingMode lastRenderingMode;
@end

@implementation TestDrawingLayer

- (void)drawInContext:(CGContextRef)cgContext
{
    WebCore::GraphicsContextCG context { cgContext, WebCore::GraphicsContextCG::CGContextFromCALayer };
    self.didDraw = true;
    self.lastRenderingMode = context.renderingMode();
}

@end

namespace TestWebKitAPI {
using namespace WebCore;

constexpr CGFloat contextWidth = 1;
constexpr CGFloat contextHeight = 1;

RetainPtr<CGImageRef> greenImage()
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG ctx(cgContext.get());
    ctx.fillRect(FloatRect(0, 0, contextWidth, contextHeight), Color::green);
    return adoptCF(CGBitmapContextCreateImage(cgContext.get()));
}

TEST(GraphicsContextTests, DrawNativeImageDoesNotLeakCompositeOperator)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG ctx(cgContext.get());

    EXPECT_EQ(ctx.compositeOperation(), CompositeOperator::SourceOver);
    EXPECT_EQ(ctx.blendMode(), BlendMode::Normal);

    auto greenPixelCG = greenImage();
    auto greenPixelNative = NativeImage::create(greenPixelCG.get());
    FloatRect rect(0, 0, contextWidth, contextHeight);
    FloatRect sourceRect(0, 0, contextWidth / 2, contextHeight / 2);
    ctx.drawNativeImage(*greenPixelNative, rect, sourceRect, { WebCore::CompositeOperator::Copy });

    CGContextSetRGBFillColor(cgContext.get(), 0, 0, 0, 0);
    CGContextFillRect(cgContext.get(), rect);

    // The context should have one green pixel.
    CGContextFlush(cgContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(cgContext.get()));
    EXPECT_EQ(primaryData[0], 0);
    EXPECT_EQ(primaryData[1], 255);
    EXPECT_EQ(primaryData[2], 0);

    EXPECT_EQ(ctx.compositeOperation(), CompositeOperator::SourceOver);
    EXPECT_EQ(ctx.blendMode(), BlendMode::Normal);
}

TEST(GraphicsContextTests, CGBitmapRenderingModeIsUnaccelerated)
{
    auto srgb = DestinationColorSpace::SRGB();
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, 3, 3, 8, 4 * 3, srgb.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    ASSERT_NE(cgContext.get(), nullptr);
    GraphicsContextCG context(cgContext.get());
    EXPECT_EQ(context.renderingMode(), RenderingMode::Unaccelerated);
}

TEST(GraphicsContextTests, IOSurfaceRenderingModeIsAccelerated)
{
    auto srgb = DestinationColorSpace::SRGB();
    IntSize size { 3, 3 };
    auto surface = WebCore::IOSurface::create(nullptr, size, srgb);
    ASSERT_NE(surface, nullptr);
    auto bitsPerPixel = 32;
    auto bitsPerComponent = 8;
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Host);
    auto cgContext = adoptCF(CGIOSurfaceContextCreate(surface->surface(), size.width(), size.height(), bitsPerComponent, bitsPerPixel, srgb.platformColorSpace(), bitmapInfo));
    GraphicsContextCG context(cgContext.get());
    EXPECT_EQ(context.renderingMode(), RenderingMode::Accelerated);
}

TEST(GraphicsContextTests, SmallLayerRenderingModeIsUnaccelerated)
{
    auto device = adoptNS(MTLCreateSystemDefaultDevice());
    if (!device)
        return;
#if !PLATFORM(IOS_FAMILY)
    uint32_t displayMask = CGDisplayIDToOpenGLDisplayMask(CGMainDisplayID());
    auto registryID = [device registryID];
    if (!registryID || !displayMask)
        return; // Skip if no GPU.
#endif
    [CATransaction begin];
    CAContext* context = [CAContext localContext];
#if !PLATFORM(IOS_FAMILY)
    context.displayMask = displayMask;
    context.GPURegistryID = registryID;
#endif
    TestDrawingLayer* drawingLayer = [TestDrawingLayer layer];
    drawingLayer.drawsAsynchronously = TRUE;
    context.layer = drawingLayer;
    [CATransaction commit];
    for (int i = 0; i < 10; ++i) {
        [CATransaction begin];
        drawingLayer.frame = CGRectMake(0, 0, 10 + i, 10 + i);
        [drawingLayer setNeedsDisplay];
        [drawingLayer displayIfNeeded];
        EXPECT_TRUE(drawingLayer.didDraw);
        EXPECT_EQ(drawingLayer.lastRenderingMode, WebCore::RenderingMode::Unaccelerated);
        [CATransaction commit];
        drawingLayer.didDraw = false;
    }
}

TEST(GraphicsContextTests, LargeLayerRenderingModeIsExpected)
{
    WebCore::RenderingMode expected = WebCore::RenderingMode::Accelerated;
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    expected = WebCore::RenderingMode::Unaccelerated;
#endif
    auto device = adoptNS(MTLCreateSystemDefaultDevice());
    if (!device)
        return;
#if !PLATFORM(IOS_FAMILY)
    uint32_t displayMask = CGDisplayIDToOpenGLDisplayMask(CGMainDisplayID());
    auto registryID = [device registryID];
    if (!registryID || !displayMask)
        return; // Skip if no GPU.
#endif
    [CATransaction begin];
    CAContext* context = [CAContext localContext];
#if !PLATFORM(IOS_FAMILY)
    context.displayMask = displayMask;
    context.GPURegistryID = registryID;
#endif
    TestDrawingLayer* drawingLayer = [TestDrawingLayer layer];
    drawingLayer.drawsAsynchronously = TRUE;
    context.layer = drawingLayer;
    [CATransaction commit];
    for (int i = 0; i < 10; ++i) {
        [CATransaction begin];
        drawingLayer.frame = CGRectMake(0, 0, 260 + i, 260 + i);
        [drawingLayer setNeedsDisplay];
        [drawingLayer displayIfNeeded];
        EXPECT_TRUE(drawingLayer.didDraw);
        EXPECT_EQ(drawingLayer.lastRenderingMode, expected);
        [CATransaction commit];
        drawingLayer.didDraw = false;
    }
}

TEST(GraphicsContextTests, DrawsReportHasDrawn)
{
    auto srgb = DestinationColorSpace::SRGB();
    IntSize size { 3, 3 };
    auto surface = WebCore::IOSurface::create(nullptr, size, srgb);
    ASSERT_NE(surface, nullptr);
    auto bitsPerPixel = 32;
    auto bitsPerComponent = 8;
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Host);
    auto cgContext = adoptCF(CGIOSurfaceContextCreate(surface->surface(), size.width(), size.height(), bitsPerComponent, bitsPerPixel, srgb.platformColorSpace(), bitmapInfo));
    GraphicsContextCG context(cgContext.get());

    // Context starts saying has drawn, conservative estimate.
    EXPECT_TRUE(context.consumeHasDrawn());
    EXPECT_FALSE(context.consumeHasDrawn());

    context.fillRect({ 0, 0, 1, 1 });
    EXPECT_TRUE(context.consumeHasDrawn());
    EXPECT_FALSE(context.consumeHasDrawn());
}

TEST(GraphicsContextTests, OutOfGamutSRGBNotDrawn)
{
    auto colorSpace = DestinationColorSpace::ExtendedSRGB();
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGBitmapFloatComponents);
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 16, 8 * contextWidth, colorSpace.platformColorSpace(), bitmapInfo));
    GraphicsContextCG ctx(cgContext.get());

    // Draw an out of gamut white
    ctx.fillRect(FloatRect(0, 0, contextWidth, contextHeight), { ExtendedSRGBA<float> { 5.0, 5.0, 5.0, 1.0 } });

    // Copy the first pixel as bytes (4 float16s = 8 bytes)
    CGContextFlush(cgContext.get());
    std::array<uint8_t, 8> firstPixel;
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(cgContext.get()));
    memcpySpan(std::span { firstPixel }, std::span(primaryData, 8));

    // Draw normal SDR white.
    ctx.fillRect(FloatRect(0, 0, contextWidth, contextHeight), Color::white);

    CGContextFlush(cgContext.get());
    primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(cgContext.get()));

    // Compare the bytes to confirm that out of gamut was restricted
    // to the SDR range.
    for (size_t i = 0; i < 8; i++)
        EXPECT_EQ(firstPixel[i], primaryData[i]);
}

} // namespace TestWebKitAPI

#endif // USE(CG)
