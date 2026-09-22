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

#include "Helpers/GraphicsTestUtilities.h"
#include "Helpers/Test.h"
#include "Helpers/WebCoreTestUtilities.h"
#include <WebCore/Color.h>
#include <WebCore/ColorConversion.h>
#include <WebCore/ColorSpace.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferAllocator.h>
#include <WebCore/IntRect.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/PixelBufferConversion.h>
#include <array>
#include <wtf/Float16.h>
#include <wtf/StdLibExtras.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/ColorSpaceCG.h>
#endif

namespace TestWebKitAPI {
using namespace WebCore;

namespace {

// A quadrant of the test pattern and the unpremultiplied color it is filled with. The
// components of the color are the contents of the pixel buffer, so they are interpreted in the
// color space of the buffer.
struct TestPattern {
    FloatRect unitRect;
    Color color;
};

// An ImageBuffer the images are drawn to.
struct DrawTarget {
    RenderingMode renderingMode;
    PixelFormat pixelFormat;
    // Whether the target preserves the alpha of the contents drawn to it.
    bool hasAlpha;
    ASCIILiteral name;
};

}

// The colors differ in their red and blue components, so that a mixup of the component order is
// detected. The components are 0, 128 or 255, so that premultiplying them with the pattern alpha
// and unpremultiplying them back is lossless.
static const TestPattern g_testPattern[] = {
    { { 0.0f, 0.0f, 0.5f, 0.5f }, Color::red },
    { { 0.5f, 0.0f, 0.5f, 0.5f }, Color { SRGBA<uint8_t> { 0, 0, 255, 128 } } },
    { { 0.0f, 0.5f, 0.5f, 0.5f }, Color::yellow },
    { { 0.5f, 0.5f, 0.5f, 0.5f }, Color::transparentBlack },
};

// The Display P3 test is not run on cairo: cairo image surfaces have no color space, so the
// cairo backend cannot color match the contents of the pixel buffer.
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3) && !USE(CAIRO)
// The test pattern for a pixel buffer that is tagged as Display P3. The colors are inside the
// sRGB gamut when their components are interpreted in Display P3, so that color matching them
// to a sRGB destination changes the components instead of clamping them back to the values the
// buffer holds. The colors are opaque, so that they look the same regardless of whether the
// image has alpha.
static const TestPattern g_displayP3TestPattern[] = {
    { { 0.0f, 0.0f, 0.5f, 0.5f }, Color { SRGBA<uint8_t> { 200, 100, 100 } } },
    { { 0.5f, 0.0f, 0.5f, 0.5f }, Color { SRGBA<uint8_t> { 100, 200, 100 } } },
    { { 0.0f, 0.5f, 0.5f, 0.5f }, Color { SRGBA<uint8_t> { 100, 100, 200 } } },
    { { 0.5f, 0.5f, 0.5f, 0.5f }, Color { SRGBA<uint8_t> { 180, 140, 90 } } },
};
#endif

// The ImageBuffer pixel formats that can be drawn to. The unaccelerated backends support only
// the formats that have an alpha channel.
static const DrawTarget g_drawTargets[] = {
    { RenderingMode::Unaccelerated, PixelFormat::BGRA8, true, "Unaccelerated_BGRA8"_s },
    { RenderingMode::Accelerated, PixelFormat::BGRA8, true, "Accelerated_BGRA8"_s },
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    { RenderingMode::Unaccelerated, PixelFormat::RGBA16F, true, "Unaccelerated_RGBA16F"_s },
    { RenderingMode::Accelerated, PixelFormat::RGBA16F, true, "Accelerated_RGBA16F"_s },
#endif
#if USE(CG)
    { RenderingMode::Accelerated, PixelFormat::BGRX8, false, "Accelerated_BGRX8"_s },
    { RenderingMode::Accelerated, PixelFormat::RGBX8, false, "Accelerated_RGBX8"_s },
#if ENABLE(PIXEL_FORMAT_RGB10)
    { RenderingMode::Accelerated, PixelFormat::RGB10, false, "Accelerated_RGB10"_s },
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    { RenderingMode::Accelerated, PixelFormat::RGB10A8, true, "Accelerated_RGB10A8"_s },
#endif
#endif
};

// The pixel formats NativeImage::create(Ref<PixelBuffer>&&) supports. The other formats are
// rejected instead of being drawn with the components in the wrong order.
static bool isSupportedImagePixelFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case PixelFormat::RGBX8:
    case PixelFormat::RGBA8:
    case PixelFormat::BGRX8:
    case PixelFormat::BGRA8:
        return true;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
#if USE(CAIRO)
        // cairo has no image surface format for the 16 bit float components.
        return false;
#else
        return true;
#endif
#endif
    default:
        return false;
    }
}

// Fills the pixel buffer with the test pattern, storing the components in the order and with the
// premultiplication that the format of the buffer specifies.
static void fillTestPattern(PixelBuffer& pixelBuffer, std::span<const TestPattern> testPattern)
{
    auto format = pixelBuffer.format();
    auto size = pixelBuffer.size();
    auto bytes = pixelBuffer.bytes();
    auto bytesPerPixel = PixelBuffer::bytesPerPixel(format.pixelFormat);
    bool hasAlpha = !pixelFormatIsOpaque(format.pixelFormat);
    bool isPremultiplied = hasAlpha && format.alphaFormat == AlphaPremultiplication::Premultiplied;
    bool storeOpaqueAlpha = false;
    if (!hasAlpha) {
        // Skia requires the contents of an image without alpha to be opaque, while
        // CoreGraphics and cairo ignore the alpha component of such contents.
#if USE(SKIA)
        storeOpaqueAlpha = true;
#endif
    }
    for (auto& pattern : testPattern) {
        std::array<uint8_t, 8> pixel { };
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        if (format.pixelFormat == PixelFormat::RGBA16F) {
            auto [red, green, blue, alpha] = pattern.color.toColorTypeLossy<SRGBA<float>>().resolved();
            if (storeOpaqueAlpha)
                alpha = 1.f;
            if (isPremultiplied) {
                red *= alpha;
                green *= alpha;
                blue *= alpha;
            }
            std::array<Float16, 4> components { red, green, blue, alpha };
            memcpySpan(std::span { pixel }, asByteSpan(components));
        } else
#endif
        {
            auto [red, green, blue, alpha] = pattern.color.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
            if (storeOpaqueAlpha)
                alpha = 255;
            if (isPremultiplied) {
                red = static_cast<uint8_t>(red * alpha / 255);
                green = static_cast<uint8_t>(green * alpha / 255);
                blue = static_cast<uint8_t>(blue * alpha / 255);
            }
            std::array<uint8_t, 4> components { red, green, blue, alpha };
            if (pixelComponentOrder(format.pixelFormat) == PixelComponentOrder::BGR)
                components = { blue, green, red, alpha };
            memcpySpan(std::span { pixel }.first(components.size()), std::span<const uint8_t> { components });
        }
        auto rect = pattern.unitRect;
        rect.scale(size);
        auto patternRect = enclosingIntRect(rect);
        for (int y = patternRect.y(); y < patternRect.maxY(); ++y) {
            for (int x = patternRect.x(); x < patternRect.maxX(); ++x)
                memcpySpan(bytes.subspan((y * size.width() + x) * bytesPerPixel, bytesPerPixel), std::span { pixel }.first(bytesPerPixel));
        }
    }
}

// The color a pattern quadrant is expected to have after the image has been drawn. An image
// without alpha ignores the alpha of its contents, so the contents appear opaque.
static Color expectedColor(const Color& patternColor, bool imageHasAlpha)
{
    if (imageHasAlpha)
        return patternColor;
    auto [red, green, blue, alpha] = patternColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    return Color { SRGBA<uint8_t> { red, green, blue, 255 } };
}

// NativeImage test fixture for tests that are variant to the format of the pixel buffer the
// image is created from.
class AnyPixelBufferFormatTest : public testing::TestWithParam<std::tuple<PixelFormat, AlphaPremultiplication>> {
protected:
    PixelFormat pixelFormat() const { return std::get<0>(GetParam()); }
    bool hasAlpha() const { return !pixelFormatIsOpaque(pixelFormat()); }
    AlphaPremultiplication alphaFormat() const { return std::get<1>(GetParam()); }
    RefPtr<PixelBuffer> createPixelBuffer(const IntSize&, const ColorSpace&) const;
    RefPtr<PixelBuffer> createTestPatternPixelBuffer(const IntSize&, const ColorSpace&, std::span<const TestPattern>) const;
};

RefPtr<PixelBuffer> AnyPixelBufferFormatTest::createPixelBuffer(const IntSize& size, const ColorSpace& colorSpace) const
{
    PixelBufferFormat format { alphaFormat(), pixelFormat(), colorSpace };
    return ImageBufferAllocator().createPixelBuffer(format, size);
}

RefPtr<PixelBuffer> AnyPixelBufferFormatTest::createTestPatternPixelBuffer(const IntSize& size, const ColorSpace& colorSpace, std::span<const TestPattern> testPattern) const
{
    RefPtr pixelBuffer = createPixelBuffer(size, colorSpace);
    if (!pixelBuffer)
        return nullptr;
    if (isSupportedImagePixelFormat(pixelFormat()))
        fillTestPattern(*pixelBuffer, testPattern);
    return pixelBuffer;
}

// Test that a NativeImage created from a PixelBuffer draws the contents of the buffer,
// interpreting the component order and the alpha of the contents the way the format of the
// buffer describes them.
TEST_P(AnyPixelBufferFormatTest, CreateFromPixelBufferDraws)
{
    constexpr IntSize testSize { 16, 16 };
    RefPtr pixelBuffer = createTestPatternPixelBuffer(testSize, ColorSpace::SRGB(), std::span { g_testPattern });
    ASSERT_NE(pixelBuffer, nullptr);

    RefPtr image = NativeImage::create(pixelBuffer.releaseNonNull());
    if (!isSupportedImagePixelFormat(pixelFormat())) {
        EXPECT_EQ(image, nullptr);
        return;
    }
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->size(), testSize);
    EXPECT_EQ(image->hasAlpha(), hasAlpha());
    EXPECT_TRUE(image->colorSpace() == ColorSpace::SRGB());

    for (auto& target : g_drawTargets) {
        SCOPED_TRACE(target.name.characters());
        auto buffer = ImageBuffer::create(testSize, target.renderingMode, RenderingPurpose::Unspecified, 1.f, ColorSpace::SRGB(), target.pixelFormat);
        ASSERT_NE(buffer, nullptr);
        // ImageBuffer::create() falls back to another backend if the requested one cannot be had,
        // which would silently drop the coverage this target is here for.
        EXPECT_EQ(buffer->renderingMode(), target.renderingMode);
        EXPECT_EQ(buffer->pixelFormat(), target.pixelFormat);
        buffer->context().drawNativeImage(*image, FloatRect { { }, testSize }, FloatRect { { }, testSize }, { CompositeOperator::Copy });
        for (auto& pattern : g_testPattern) {
            auto expected = expectedColor(pattern.color, hasAlpha());
            // Drawing contents that are not opaque to a target that has no alpha is not what
            // this test is about.
            if (!target.hasAlpha && !expected.isOpaque())
                continue;
            auto rect = pattern.unitRect;
            rect.scale(testSize);
            EXPECT_TRUE(imageBufferPixelIs(expected, *buffer, rect.center()));
        }
    }
}

// Test that a PixelBuffer without pixels is rejected instead of being turned into an image that
// has no area.
TEST_P(AnyPixelBufferFormatTest, CreateFromEmptyPixelBufferFails)
{
    static constexpr IntSize emptySizes[] = { { 0, 0 }, { 0, 16 }, { 16, 0 } };
    for (auto& size : emptySizes) {
        SCOPED_TRACE(::testing::Message() << "size: " << size.width() << "x" << size.height());
        RefPtr pixelBuffer = createPixelBuffer(size, ColorSpace::SRGB());
        ASSERT_NE(pixelBuffer, nullptr);
        EXPECT_EQ(NativeImage::create(pixelBuffer.releaseNonNull()), nullptr);
    }
}

#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3) && !USE(CAIRO)

// The color a quadrant of the Display P3 test pattern is expected to have once the image has
// been color matched to a sRGB destination.
static Color displayP3PatternColorInSRGB(const Color& patternColor)
{
    auto [red, green, blue, alpha] = patternColor.toColorTypeLossy<SRGBA<float>>().resolved();
    return Color { convertColor<SRGBA<float>>(DisplayP3<float> { red, green, blue, alpha }) };
}

// Test that a NativeImage created from a PixelBuffer is tagged with the color space of the
// buffer, so that the contents are color matched when they are drawn to a destination that has
// a different color space.
TEST_P(AnyPixelBufferFormatTest, CreateFromPixelBufferUsesPixelBufferColorSpace)
{
    if (!isSupportedImagePixelFormat(pixelFormat()))
        return;
    constexpr IntSize testSize { 16, 16 };
    RefPtr pixelBuffer = createTestPatternPixelBuffer(testSize, ColorSpace::DisplayP3(), std::span { g_displayP3TestPattern });
    ASSERT_NE(pixelBuffer, nullptr);

    RefPtr image = NativeImage::create(pixelBuffer.releaseNonNull());
    ASSERT_NE(image, nullptr);
    EXPECT_TRUE(image->colorSpace() == ColorSpace::DisplayP3());

    // The color matching of the platform rounds slightly differently than the color conversion
    // the expectation is computed with.
    constexpr unsigned tolerance = 2;
    for (auto& target : g_drawTargets) {
        SCOPED_TRACE(target.name.characters());
        auto buffer = ImageBuffer::create(testSize, target.renderingMode, RenderingPurpose::Unspecified, 1.f, ColorSpace::SRGB(), target.pixelFormat);
        ASSERT_NE(buffer, nullptr);
        // ImageBuffer::create() falls back to another backend if the requested one cannot be had,
        // which would silently drop the coverage this target is here for.
        EXPECT_EQ(buffer->renderingMode(), target.renderingMode);
        EXPECT_EQ(buffer->pixelFormat(), target.pixelFormat);
        buffer->context().drawNativeImage(*image, FloatRect { { }, testSize }, FloatRect { { }, testSize }, { CompositeOperator::Copy });
        for (auto& pattern : g_displayP3TestPattern) {
            auto rect = pattern.unitRect;
            rect.scale(testSize);
            EXPECT_TRUE(imageBufferPixelIs(displayP3PatternColorInSRGB(pattern.color), *buffer, rect.center(), tolerance));
        }
    }
}

#endif

static std::string anyPixelBufferFormatTestName(const testing::TestParamInfo<AnyPixelBufferFormatTest::ParamType>& info)
{
    auto [pixelFormat, alphaFormat] = info.param;
    std::string name;
    switch (pixelFormat) {
    case PixelFormat::RGBX8:
        name = "RGBX8";
        break;
    case PixelFormat::RGBA8:
        name = "RGBA8";
        break;
    case PixelFormat::BGRX8:
        name = "BGRX8";
        break;
    case PixelFormat::BGRA8:
        name = "BGRA8";
        break;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
        name = "RGBA16F";
        break;
#endif
    default:
        name = "UnknownPixelFormat";
        break;
    }
    switch (alphaFormat) {
    case AlphaPremultiplication::Unpremultiplied:
        name += "_Unpremultiplied";
        break;
    case AlphaPremultiplication::Premultiplied:
        name += "_Premultiplied";
        break;
    }
    return name;
}

// The pixel formats a PixelBuffer can have. The formats without a pixel buffer representation,
// e.g. RGB10, cannot be tested as sources.
static auto testedPixelFormats()
{
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    return testing::Values(PixelFormat::RGBX8, PixelFormat::RGBA8, PixelFormat::BGRX8, PixelFormat::BGRA8, PixelFormat::RGBA16F);
#else
    return testing::Values(PixelFormat::RGBX8, PixelFormat::RGBA8, PixelFormat::BGRX8, PixelFormat::BGRA8);
#endif
}

INSTANTIATE_TEST_SUITE_P(NativeImageTests,
    AnyPixelBufferFormatTest,
    testing::Combine(
        testedPixelFormats(),
        testing::Values(AlphaPremultiplication::Unpremultiplied, AlphaPremultiplication::Premultiplied)),
    anyPixelBufferFormatTestName);

#if USE(CG)

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

TEST(NativeImageTests, BorrowsPaddedRowsWithoutCopying)
{
    constexpr unsigned bytesPerRow = 64; // 4x4 pixels need only 16.
    RefPtr image = NativeImage::create(createPaddedTestImage(4, 4, bytesPerRow));
    ASSERT_TRUE(image);

    auto format = image->pixelSourceFormat();
    ASSERT_TRUE(format.has_value());
    EXPECT_EQ(image->size(), IntSize(4, 4));
    EXPECT_EQ(format->pixelFormat, PixelFormat::BGRA8);
    EXPECT_EQ(format->alphaFormat, AlphaPremultiplication::Premultiplied);

    // Asking for the format the image already holds must borrow: same stride, and the view
    // must expose the image's own bytes rather than a repacked copy.
    bool called = false;
    EXPECT_TRUE(image->withPixels(*format, [&](const ConstPixelBufferConversionView& view) {
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

TEST(NativeImageTests, BorrowsAnOpaqueRGBXImage)
{
    constexpr unsigned bytesPerRow = 2 * 4;
    auto* data = new uint8_t[bytesPerRow * 2];
    std::span<uint8_t> bytes { data, bytesPerRow * 2 };
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            auto pixel = bytes.subspan(y * bytesPerRow + x * 4, 4);
            pixel[0] = static_cast<uint8_t>(y * 2 + x); // Red, in RGBX order.
            pixel[1] = 0;
            pixel[2] = 0;
            pixel[3] = 0x7f; // The skipped byte holds an undefined value.
        }
    }
    RetainPtr provider = adoptCF(CGDataProviderCreateWithData(data, data, bytes.size(), [](void* context, const void*, size_t) {
        delete[] static_cast<uint8_t*>(context);
    }));
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrderDefault) | static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipLast);
    RefPtr image = NativeImage::create(adoptCF(CGImageCreate(2, 2, 8, 32, bytesPerRow, sRGBColorSpaceSingleton(), bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault)));
    ASSERT_TRUE(image);
    EXPECT_FALSE(image->hasAlpha());

    auto sourceFormat = image->pixelSourceFormat();
    ASSERT_TRUE(sourceFormat.has_value());
    EXPECT_EQ(sourceFormat->pixelFormat, PixelFormat::RGBX8);

    bool called = false;
    EXPECT_TRUE(image->withPixels(*sourceFormat, [&](const ConstPixelBufferConversionView& view) {
        called = true;
        EXPECT_EQ(view.bytesPerRow, bytesPerRow);
        EXPECT_EQ(view.rows[0], 0u);
        EXPECT_EQ(view.rows[4], 1u);
        EXPECT_EQ(view.rows[bytesPerRow], 2u); // Second row, first pixel.
    }));
    EXPECT_TRUE(called);

    // Converting to a format that has an alpha channel must produce opaque pixels rather
    // than propagate the undefined byte.
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    std::array<uint8_t, 2 * 2 * 4> converted;
    PixelBufferConversionView destination { format, 2 * 4, converted };
    ASSERT_TRUE(image->copyPixels({ 0, 0, 2, 2 }, destination));
    EXPECT_EQ(converted[0], 0u);
    EXPECT_EQ(converted[3], 255u);
    EXPECT_EQ(converted[4], 1u);
    EXPECT_EQ(converted[7], 255u);
}

TEST(NativeImageTests, BorrowsSubimageAtAnOffset)
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

    auto format = image->pixelSourceFormat();
    ASSERT_TRUE(format.has_value());

    bool called = false;
    EXPECT_TRUE(image->withPixels(*format, [&](const ConstPixelBufferConversionView& view) {
        called = true;
        // size() is the sub-image's, but the stride is the parent's. If this ever reports 8
        // instead, CG stopped sharing the parent's provider and the sub-image path is no
        // longer being exercised by this test.
        EXPECT_EQ(view.bytesPerRow, bytesPerRow);
        // Whether CG produced a sub-image or a fresh image, the first pixel must be the one
        // at (1, 2) of the parent: y * width + x == 2 * 4 + 1.
        EXPECT_EQ(view.rows[0], 9u);
        EXPECT_EQ(view.rows[4], 10u);
        EXPECT_EQ(view.rows[view.bytesPerRow], 13u);
    }));
    EXPECT_TRUE(called);
}

TEST(NativeImageTests, ConvertsWhenTheRequestedFormatDiffers)
{
    RefPtr image = NativeImage::create(createPaddedTestImage(4, 4, 64));
    ASSERT_TRUE(image);

    // RGBA8 is not the image's own format, so copyPixels() must convert. Blue was set to
    // y * width + x, so after the BGRA -> RGBA swap it lands in the third component.
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    std::array<uint8_t, 4 * 4 * 4> bytes;
    PixelBufferConversionView destination { format, 4 * 4, bytes };
    ASSERT_TRUE(image->copyPixels({ 0, 0, 4, 4 }, destination));
    EXPECT_EQ(bytes[2], 0u);
    EXPECT_EQ(bytes[3], 255u);
    EXPECT_EQ(bytes[6], 1u);
    EXPECT_EQ(bytes[4 * 4 + 2], 4u); // Second row, first pixel: tightly packed now.
}

TEST(NativeImageTests, CopiesASubRectangle)
{
    RefPtr image = NativeImage::create(createPaddedTestImage(4, 4, 64));
    ASSERT_TRUE(image);

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, ColorSpace::SRGB() };
    std::array<uint8_t, 2 * 2 * 4> bytes;
    PixelBufferConversionView destination { format, 2 * 4, bytes };
    ASSERT_TRUE(image->copyPixels({ 1, 2, 2, 2 }, destination));
    EXPECT_EQ(bytes[0], 9u); // (1, 2)
    EXPECT_EQ(bytes[4], 10u); // (2, 2)
    EXPECT_EQ(bytes[8], 13u); // (1, 3)
}

TEST(NativeImageTests, SinglePixelSolidColorUnpremultiplies)
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

TEST(NativeImageTests, SinglePixelSolidColorTransparentBlack)
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

TEST(NativeImageTests, SinglePixelSolidColorRejectsLargerImages)
{
    RefPtr image = NativeImage::create(createPaddedTestImage(2, 2, 8));
    ASSERT_TRUE(image);
    EXPECT_FALSE(image->singlePixelSolidColor().has_value());
}

// A 2x2 image in an indexed color space, a layout PixelBufferFormat cannot name, so its pixels
// can never be borrowed. Palette index 0 is red and index 1 is blue; the top-left and
// bottom-right pixels are red, the other two blue.
static RetainPtr<CGImageRef> createIndexedTestImage()
{
    constexpr std::array<uint8_t, 6> palette { 255, 0, 0, 0, 0, 255 };
    RetainPtr colorSpace = adoptCF(CGColorSpaceCreateIndexed(sRGBColorSpaceSingleton(), 1, palette.data()));
    if (!colorSpace)
        return nullptr;

    auto* data = new uint8_t[4] { 0, 1, 1, 0 };
    RetainPtr provider = adoptCF(CGDataProviderCreateWithData(data, data, 4, [](void* context, const void*, size_t) {
        delete[] static_cast<uint8_t*>(context);
    }));
    return adoptCF(CGImageCreate(2, 2, 8, 8, 2, colorSpace.get(), kCGImageAlphaNone, provider.get(), nullptr, false, kCGRenderingIntentDefault));
}

TEST(NativeImageTests, ReadsBackAnIndexedImageByDrawing)
{
    RefPtr image = NativeImage::create(createIndexedTestImage());
    ASSERT_TRUE(image);

    // The borrow must fail, so withPixels() falls back to a draw.
    EXPECT_FALSE(image->pixelSourceFormat().has_value());

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    bool called = false;
    EXPECT_TRUE(image->withPixels(format, [&](const ConstPixelBufferConversionView& view) {
        called = true;
        // The fallback format is honoured, and the result is tightly packed.
        EXPECT_EQ(view.format.pixelFormat, PixelFormat::RGBA8);
        EXPECT_EQ(view.bytesPerRow, 2u * 4);
        EXPECT_EQ(view.rows[0], 255u);
        EXPECT_EQ(view.rows[2], 0u);
        EXPECT_EQ(view.rows[4], 0u);
        EXPECT_EQ(view.rows[6], 255u);
    }));
    EXPECT_TRUE(called);
}

TEST(NativeImageTests, DrawsTheWholeImageStraightIntoAPaddedDestination)
{
    RefPtr image = NativeImage::create(createIndexedTestImage());
    ASSERT_TRUE(image);
    ASSERT_FALSE(image->pixelSourceFormat().has_value());

    // The destination takes the whole image in a format the draw can write, so copyPixels()
    // draws into it rather than into a scratch buffer it then converts. The destination's own
    // stride is honoured.
    constexpr unsigned bytesPerRow = 16; // A row of 2 pixels needs only 8.
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    std::array<uint8_t, bytesPerRow * 2> bytes;
    PixelBufferConversionView destination { format, bytesPerRow, bytes };
    ASSERT_TRUE(image->copyPixels({ 0, 0, 2, 2 }, destination));
    EXPECT_EQ(bytes[0], 255u); // (0, 0) is red.
    EXPECT_EQ(bytes[6], 255u); // (1, 0) is blue.
    EXPECT_EQ(bytes[bytesPerRow + 2], 255u); // (0, 1) is blue.
    EXPECT_EQ(bytes[bytesPerRow + 4], 255u); // (1, 1) is red.
}

TEST(NativeImageTests, DrawsASubRectangleStraightIntoTheDestination)
{
    RefPtr image = NativeImage::create(createIndexedTestImage());
    ASSERT_TRUE(image);

    // readPixels() renders the source rect alone, so a sub-rectangle needs no scratch buffer
    // either. (1, 1) is red while both of its neighbours are blue, so an off-by-one in either
    // axis, or a flipped one, would show up here.
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    std::array<uint8_t, 4> bytes { };
    PixelBufferConversionView destination { format, bytes.size(), bytes };
    ASSERT_TRUE(image->copyPixels({ 1, 1, 1, 1 }, destination));
    EXPECT_EQ(bytes[0], 255u);
    EXPECT_EQ(bytes[2], 0u);
}

TEST(NativeImageTests, DrawsTheBottomRowOfAnImage)
{
    RefPtr image = NativeImage::create(createIndexedTestImage());
    ASSERT_TRUE(image);

    // The row order of the result is top down, whatever the platform's own origin is: this is
    // the bottom row, blue then red, not the top one.
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    std::array<uint8_t, 2 * 4> bytes { };
    PixelBufferConversionView destination { format, bytes.size(), bytes };
    ASSERT_TRUE(image->copyPixels({ 0, 1, 2, 1 }, destination));
    EXPECT_EQ(bytes[0], 0u); // (0, 1) is blue.
    EXPECT_EQ(bytes[2], 255u);
    EXPECT_EQ(bytes[4], 255u); // (1, 1) is red.
    EXPECT_EQ(bytes[6], 0u);
}

TEST(NativeImageTests, DrawsThroughAScratchBufferWhenTheDestinationCannotTakeTheStride)
{
    RefPtr image = NativeImage::create(createIndexedTestImage());
    ASSERT_TRUE(image);

    // A destination narrowed to its last row's pixels cannot take a draw, which may write the
    // whole stride, so this one goes through the scratch buffer and a conversion. Neither the
    // row padding nor anything past the narrowed end may be touched.
    constexpr unsigned bytesPerRow = 16; // A row of 2 pixels needs only 8.
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    std::array<uint8_t, bytesPerRow * 2> bytes;
    bytes.fill(0xAB);
    PixelBufferConversionView destination { format, bytesPerRow, std::span { bytes }.first(bytesPerRow + 2 * 4) };
    ASSERT_TRUE(image->copyPixels({ 0, 0, 2, 2 }, destination));
    EXPECT_EQ(bytes[0], 255u); // (0, 0) is red.
    EXPECT_EQ(bytes[4], 0u); // (1, 0) is blue.
    EXPECT_EQ(bytes[6], 255u);
    EXPECT_EQ(bytes[bytesPerRow], 0u); // (0, 1) is blue.
    EXPECT_EQ(bytes[bytesPerRow + 2], 255u);
    EXPECT_EQ(bytes[bytesPerRow + 4], 255u); // (1, 1) is red.
    EXPECT_EQ(bytes[8], 0xABu); // The first row's padding is left alone.
    EXPECT_EQ(bytes[bytesPerRow + 8], 0xABu); // As is everything past the narrowed end.
    EXPECT_EQ(bytes[bytes.size() - 1], 0xABu);
}

// Reads the indexed test image back in `format`, tightly packed. Its pixels can never be
// borrowed, so every one of these goes through readPixels(), one destination format each.
static Vector<uint8_t> readBackIndexedTestImage(const PixelBufferFormat& format)
{
    RefPtr image = NativeImage::create(createIndexedTestImage());
    if (!image)
        return { };
    auto bytesPerRow = PixelBuffer::computeBytesPerRow(format.pixelFormat, 2);
    if (bytesPerRow.hasOverflowed())
        return { };
    Vector<uint8_t> bytes(bytesPerRow.value() * 2);
    PixelBufferConversionView destination { format, bytesPerRow.value(), bytes.mutableSpan() };
    if (!image->copyPixels({ 0, 0, 2, 2 }, destination))
        return { };
    return bytes;
}

TEST(NativeImageTests, DrawsIntoAnRGBA8Destination)
{
    auto bytes = readBackIndexedTestImage({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() });
    ASSERT_EQ(bytes.size(), 2u * 2 * 4);
    EXPECT_EQ(bytes[0], 255u); // (0, 0) is red.
    EXPECT_EQ(bytes[2], 0u);
    EXPECT_EQ(bytes[3], 255u);
    EXPECT_EQ(bytes[4], 0u); // (1, 0) is blue.
    EXPECT_EQ(bytes[6], 255u);
    EXPECT_EQ(bytes[8], 0u); // (0, 1) is blue.
    EXPECT_EQ(bytes[10], 255u);
    EXPECT_EQ(bytes[12], 255u); // (1, 1) is red.
    EXPECT_EQ(bytes[14], 0u);
}

TEST(NativeImageTests, DrawsIntoAnRGBX8Destination)
{
    auto bytes = readBackIndexedTestImage({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBX8, ColorSpace::SRGB() });
    ASSERT_EQ(bytes.size(), 2u * 2 * 4);
    // The contents of the skipped byte are undefined, so it is not asserted on.
    EXPECT_EQ(bytes[0], 255u); // (0, 0) is red.
    EXPECT_EQ(bytes[2], 0u);
    EXPECT_EQ(bytes[4], 0u); // (1, 0) is blue.
    EXPECT_EQ(bytes[6], 255u);
    EXPECT_EQ(bytes[8], 0u); // (0, 1) is blue.
    EXPECT_EQ(bytes[10], 255u);
    EXPECT_EQ(bytes[12], 255u); // (1, 1) is red.
    EXPECT_EQ(bytes[14], 0u);
}

TEST(NativeImageTests, DrawsIntoABGRA8Destination)
{
    auto bytes = readBackIndexedTestImage({ AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, ColorSpace::SRGB() });
    ASSERT_EQ(bytes.size(), 2u * 2 * 4);
    EXPECT_EQ(bytes[0], 0u); // (0, 0) is red, and blue comes first.
    EXPECT_EQ(bytes[2], 255u);
    EXPECT_EQ(bytes[3], 255u);
    EXPECT_EQ(bytes[4], 255u); // (1, 0) is blue.
    EXPECT_EQ(bytes[6], 0u);
    EXPECT_EQ(bytes[8], 255u); // (0, 1) is blue.
    EXPECT_EQ(bytes[10], 0u);
    EXPECT_EQ(bytes[12], 0u); // (1, 1) is red.
    EXPECT_EQ(bytes[14], 255u);
}

TEST(NativeImageTests, DrawsIntoABGRX8Destination)
{
    auto bytes = readBackIndexedTestImage({ AlphaPremultiplication::Premultiplied, PixelFormat::BGRX8, ColorSpace::SRGB() });
    ASSERT_EQ(bytes.size(), 2u * 2 * 4);
    // The contents of the skipped byte are undefined, so it is not asserted on.
    EXPECT_EQ(bytes[0], 0u); // (0, 0) is red, and blue comes first.
    EXPECT_EQ(bytes[2], 255u);
    EXPECT_EQ(bytes[4], 255u); // (1, 0) is blue.
    EXPECT_EQ(bytes[6], 0u);
    EXPECT_EQ(bytes[8], 255u); // (0, 1) is blue.
    EXPECT_EQ(bytes[10], 0u);
    EXPECT_EQ(bytes[12], 0u); // (1, 1) is red.
    EXPECT_EQ(bytes[14], 255u);
}

#if ENABLE(PIXEL_FORMAT_RGBA16F)

TEST(NativeImageTests, DrawsIntoAnRGBA16FDestination)
{
    auto bytes = readBackIndexedTestImage({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, ColorSpace::SRGB() });
    ASSERT_EQ(bytes.size(), 2u * 2 * 8);
    // Fully saturated and fully unsaturated components come out the same whatever transfer
    // function the float destination uses.
    auto components = spanReinterpretCast<const Float16>(bytes.span());
    EXPECT_FLOAT_EQ(components[0], 1.f); // (0, 0) is red.
    EXPECT_FLOAT_EQ(components[2], 0.f);
    EXPECT_FLOAT_EQ(components[3], 1.f);
    EXPECT_FLOAT_EQ(components[4], 0.f); // (1, 0) is blue.
    EXPECT_FLOAT_EQ(components[6], 1.f);
    EXPECT_FLOAT_EQ(components[8], 0.f); // (0, 1) is blue.
    EXPECT_FLOAT_EQ(components[10], 1.f);
    EXPECT_FLOAT_EQ(components[12], 1.f); // (1, 1) is red.
    EXPECT_FLOAT_EQ(components[14], 0.f);
}

#endif

// 16 bits per component is another layout PixelBufferFormat cannot name, so this image can
// never be borrowed either, and unlike the indexed one it has an alpha channel: a single
// fully saturated red pixel at half alpha, premultiplied.
static RetainPtr<CGImageRef> createDeepHalfTransparentTestImage()
{
    auto* data = new uint16_t[4] { 0x8000, 0, 0, 0x8000 };
    RetainPtr provider = adoptCF(CGDataProviderCreateWithData(data, data, 8, [](void* context, const void*, size_t) {
        delete[] static_cast<uint16_t*>(context);
    }));
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast);
    return adoptCF(CGImageCreate(1, 1, 16, 64, 8, sRGBColorSpaceSingleton(), bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault));
}

TEST(NativeImageTests, DrawsIntoAnUnpremultipliedDestination)
{
    RefPtr image = NativeImage::create(createDeepHalfTransparentTestImage());
    ASSERT_TRUE(image);
    ASSERT_FALSE(image->pixelSourceFormat().has_value());

    // The draw itself always premultiplies, so readPixels() has to unpremultiply what it
    // produced. Red is fully saturated, so it survives the round trip exactly.
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    std::array<uint8_t, 4> bytes { };
    PixelBufferConversionView destination { format, bytes.size(), bytes };
    ASSERT_TRUE(image->copyPixels({ 0, 0, 1, 1 }, destination));
    EXPECT_EQ(bytes[0], 255u);
    EXPECT_EQ(bytes[1], 0u);
    EXPECT_EQ(bytes[2], 0u);
    EXPECT_NEAR(bytes[3], 128, 1);
}

#endif // USE(CG)

}
