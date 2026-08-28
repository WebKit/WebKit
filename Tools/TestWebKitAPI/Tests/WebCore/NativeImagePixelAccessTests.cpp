/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "Test.h"

#include <WebCore/Color.h>
#include <WebCore/ColorSpaceCG.h>
#include <WebCore/IntRect.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/PixelBufferConversion.h>
#include <wtf/StdLibExtras.h>

#if USE(CG)

#include <CoreGraphics/CoreGraphics.h>

namespace TestWebKitAPI {
using namespace WebCore;

// Creates a BGRA8 premultiplied image whose rows are padded to `bytesPerRow`, with the blue
// channel of each pixel set to a recognisable value and alpha fully opaque.
static RetainPtr<CGImageRef> createPaddedTestImage(int width, int height, unsigned bytesPerRow)
{
    size_t dataSize = bytesPerRow * height;
    auto* data = new uint8_t[dataSize];
    std::span<uint8_t> bytes { data, dataSize };
    zeroSpan(bytes);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto pixel = bytes.subspan(y * bytesPerRow + x * 4, 4);
            pixel[0] = static_cast<uint8_t>(y * width + x); // Blue, in BGRA order.
            pixel[3] = 255;
        }
    }
    RetainPtr provider = adoptCF(CGDataProviderCreateWithData(data, data, dataSize, [](void* context, const void*, size_t) {
        delete[] static_cast<uint8_t*>(context);
    }));
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst);
    return adoptCF(CGImageCreate(width, height, 8, 32, bytesPerRow, sRGBColorSpaceSingleton(), bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault));
}

TEST(NativeImagePixelAccessTests, borrowsPaddedRowsWithoutCopying)
{
    constexpr unsigned bytesPerRow = 64; // 4x4 pixels need only 16.
    RefPtr image = NativeImage::create(createPaddedTestImage(4, 4, bytesPerRow));
    ASSERT_TRUE(image);

    auto info = image->pixelSourceInfo();
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(image->size(), IntSize(4, 4));
    EXPECT_EQ(info->bytesPerRow, bytesPerRow);
    EXPECT_EQ(info->format.pixelFormat, PixelFormat::BGRA8);
    EXPECT_EQ(info->format.alphaFormat, AlphaPremultiplication::Premultiplied);

    // Asking for the format the image already holds must borrow: same stride, and the view
    // must expose the image's own bytes rather than a repacked copy.
    bool called = false;
    EXPECT_TRUE(image->withPixels(info->format, [&](const ConstPixelBufferConversionView& view) {
        called = true;
        EXPECT_EQ(view.bytesPerRow, bytesPerRow);
        EXPECT_EQ(view.rows[0], 0u);
        EXPECT_EQ(view.rows[4], 1u);
        EXPECT_EQ(view.rows[bytesPerRow], 4u); // Second row, first pixel.
        // Narrowed to the minimum: the last row's padding is not exposed.
        EXPECT_EQ(view.rows.size(), bytesPerRow * 3 + 16);
    }));
    EXPECT_TRUE(called);
}

TEST(NativeImagePixelAccessTests, borrowsSubimageAtAnOffset)
{
    constexpr unsigned bytesPerRow = 64;
    RefPtr parent = NativeImage::create(createPaddedTestImage(4, 4, bytesPerRow));
    ASSERT_TRUE(parent);

    // CGImageCreateWithImageInRect may return a sub-image sharing the parent's data provider.
    RetainPtr subimage = adoptCF(CGImageCreateWithImageInRect(parent->platformImage().get(), CGRectMake(1, 2, 2, 2)));
    ASSERT_TRUE(subimage);
    RefPtr image = NativeImage::create(WTF::move(subimage));
    ASSERT_TRUE(image);
    EXPECT_EQ(image->size(), IntSize(2, 2));

    auto info = image->pixelSourceInfo();
    ASSERT_TRUE(info.has_value());
    // size() is the sub-image's, but the stride is the parent's. If this ever reports 8
    // instead, CG stopped sharing the parent's provider and the sub-image path below is no
    // longer being exercised by this test.
    EXPECT_EQ(info->bytesPerRow, bytesPerRow);

    bool called = false;
    EXPECT_TRUE(image->withPixels(info->format, [&](const ConstPixelBufferConversionView& view) {
        called = true;
        // Whether CG produced a sub-image or a fresh image, the first pixel must be the one
        // at (1, 2) of the parent: y * width + x == 2 * 4 + 1.
        EXPECT_EQ(view.rows[0], 9u);
        EXPECT_EQ(view.rows[4], 10u);
        EXPECT_EQ(view.rows[view.bytesPerRow], 13u);
    }));
    EXPECT_TRUE(called);
}

TEST(NativeImagePixelAccessTests, convertsWhenTheRequestedFormatDiffers)
{
    RefPtr image = NativeImage::create(createPaddedTestImage(4, 4, 64));
    ASSERT_TRUE(image);

    // RGBA8 is not the image's own format, so copyPixels() must convert. Blue was set to
    // y * width + x, so after the BGRA -> RGBA swap it lands in the third component.
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    RefPtr pixelBuffer = image->getPixelBuffer(format, { 0, 0, 4, 4 });
    ASSERT_TRUE(pixelBuffer);
    auto bytes = pixelBuffer->bytes();
    ASSERT_EQ(bytes.size(), 4u * 4 * 4);
    EXPECT_EQ(bytes[2], 0u);
    EXPECT_EQ(bytes[3], 255u);
    EXPECT_EQ(bytes[6], 1u);
    EXPECT_EQ(bytes[4 * 4 + 2], 4u); // Second row, first pixel: tightly packed now.
}

TEST(NativeImagePixelAccessTests, copiesASubRectangle)
{
    RefPtr image = NativeImage::create(createPaddedTestImage(4, 4, 64));
    ASSERT_TRUE(image);

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    RefPtr pixelBuffer = image->getPixelBuffer(format, { 1, 2, 2, 2 });
    ASSERT_TRUE(pixelBuffer);
    EXPECT_EQ(pixelBuffer->size(), IntSize(2, 2));
    auto bytes = pixelBuffer->bytes();
    ASSERT_EQ(bytes.size(), 2u * 2 * 4);
    EXPECT_EQ(bytes[0], 9u); // (1, 2)
    EXPECT_EQ(bytes[4], 10u); // (2, 2)
    EXPECT_EQ(bytes[8], 13u); // (1, 3)
}

TEST(NativeImagePixelAccessTests, singlePixelSolidColorUnpremultiplies)
{
    // A 1x1 half-transparent red, premultiplied: BGRA order, so B=0, G=0, R=128, A=128.
    auto* data = new uint8_t[4] { 0, 0, 128, 128 };
    RetainPtr provider = adoptCF(CGDataProviderCreateWithData(data, data, 4, [](void* context, const void*, size_t) {
        delete[] static_cast<uint8_t*>(context);
    }));
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst);
    RefPtr image = NativeImage::create(adoptCF(CGImageCreate(1, 1, 8, 32, 4, sRGBColorSpaceSingleton(), bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault)));
    ASSERT_TRUE(image);

    // 128 premultiplied over alpha 128 unpremultiplies to full red.
    Color expected { SRGBA<uint8_t> { 255, 0, 0, 128 } };
    EXPECT_EQ(image->singlePixelSolidColor(), expected);
}

TEST(NativeImagePixelAccessTests, singlePixelSolidColorTransparentBlack)
{
    auto* data = new uint8_t[4] { 0, 0, 0, 0 };
    RetainPtr provider = adoptCF(CGDataProviderCreateWithData(data, data, 4, [](void* context, const void*, size_t) {
        delete[] static_cast<uint8_t*>(context);
    }));
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst);
    RefPtr image = NativeImage::create(adoptCF(CGImageCreate(1, 1, 8, 32, 4, sRGBColorSpaceSingleton(), bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault)));
    ASSERT_TRUE(image);

    EXPECT_EQ(image->singlePixelSolidColor(), Color::transparentBlack);
}

TEST(NativeImagePixelAccessTests, singlePixelSolidColorRejectsLargerImages)
{
    RefPtr image = NativeImage::create(createPaddedTestImage(2, 2, 8));
    ASSERT_TRUE(image);
    EXPECT_FALSE(image->singlePixelSolidColor().has_value());
}

TEST(NativeImagePixelAccessTests, readsBackAnIndexedImageByDrawing)
{
    // An indexed color space is a layout PixelBufferFormat cannot name, so the borrow must
    // fail and withPixels() must fall back to a draw.
    constexpr std::array<uint8_t, 6> palette { 255, 0, 0, 0, 0, 255 };
    RetainPtr colorSpace = adoptCF(CGColorSpaceCreateIndexed(sRGBColorSpaceSingleton(), 1, palette.data()));
    ASSERT_TRUE(colorSpace);

    auto* data = new uint8_t[4] { 0, 1, 1, 0 };
    RetainPtr provider = adoptCF(CGDataProviderCreateWithData(data, data, 4, [](void* context, const void*, size_t) {
        delete[] static_cast<uint8_t*>(context);
    }));
    RefPtr image = NativeImage::create(adoptCF(CGImageCreate(2, 2, 8, 8, 2, colorSpace.get(), kCGImageAlphaNone, provider.get(), nullptr, false, kCGRenderingIntentDefault)));
    ASSERT_TRUE(image);

    EXPECT_FALSE(image->pixelSourceInfo().has_value());

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    bool called = false;
    EXPECT_TRUE(image->withPixels(format, [&](const ConstPixelBufferConversionView& view) {
        called = true;
        // The fallback format is honoured, and the result is tightly packed.
        EXPECT_EQ(view.format.pixelFormat, PixelFormat::RGBA8);
        EXPECT_EQ(view.bytesPerRow, 2u * 4);
        // Palette index 0 is red, index 1 is blue.
        EXPECT_EQ(view.rows[0], 255u);
        EXPECT_EQ(view.rows[2], 0u);
        EXPECT_EQ(view.rows[4], 0u);
        EXPECT_EQ(view.rows[6], 255u);
    }));
    EXPECT_TRUE(called);
}

} // namespace TestWebKitAPI

#endif // USE(CG)
