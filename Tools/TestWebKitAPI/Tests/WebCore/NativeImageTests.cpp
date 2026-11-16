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
#include <WebCore/NativeImage.h>
#include <WebCore/PixelBuffer.h>
#include <array>
#include <wtf/Float16.h>
#include <wtf/StdLibExtras.h>

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

}
