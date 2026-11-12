/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Helpers/Test.h"
#include <WebCore/Color.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/IntSize.h>
#include <WebCore/MeshGradient.h>
#include <WebCore/MeshGradientRendererCPU.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/PixelBufferConversion.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <wtf/Float16.h>

#if USE(METAL)
#include <WebCore/MeshGradientRendererMetal.h>
#endif

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#include <wtf/RetainPtr.h>
#endif

namespace TestWebKitAPI {

using namespace WebCore;

static constexpr ColorInterpolationMethod srgb { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Premultiplied };

// A mesh whose points are evenly spaced over a box of the given size, with straight edges.
static Ref<MeshGradient> makeMeshGradient(ColorInterpolationMethod method, IntSize size, unsigned rows, unsigned columns, Vector<Color> colors, const Color& backgroundColor = Color::transparentBlack)
{
    float columnThird = static_cast<float>(size.width()) / (columns - 1) / 3;
    float rowThird = static_cast<float>(size.height()) / (rows - 1) / 3;

    Vector<MeshGradient::Point> points;
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned column = 0; column < columns; ++column) {
            FloatPoint position { static_cast<float>(size.width()) * column / (columns - 1), static_cast<float>(size.height()) * row / (rows - 1) };
            points.append({
                .position = position,
                .color = colors[row * columns + column],
                .controlPoints = {
                    position - FloatSize { 0, rowThird },
                    position + FloatSize { columnThird, 0 },
                    position + FloatSize { 0, rowThird },
                    position - FloatSize { columnThird, 0 },
                },
            });
        }
    }
    return MeshGradient::create(method, rows, columns, WTF::move(points), backgroundColor);
}

// Points with the given positions, and straight edges to their neighbors.
static Vector<MeshGradient::Point> makePoints(unsigned rows, unsigned columns, const Vector<FloatPoint>& positions, const Color& color)
{
    Vector<MeshGradient::Point> points;
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned column = 0; column < columns; ++column) {
            auto position = positions[row * columns + column];
            auto towardNeighbor = [&](unsigned neighborRow, unsigned neighborColumn) {
                return position + (positions[neighborRow * columns + neighborColumn] - position) / 3;
            };
            points.append({
                .position = position,
                .color = color,
                .controlPoints = {
                    row ? towardNeighbor(row - 1, column) : position,
                    column < columns - 1 ? towardNeighbor(row, column + 1) : position,
                    row < rows - 1 ? towardNeighbor(row + 1, column) : position,
                    column ? towardNeighbor(row, column - 1) : position,
                },
            });
        }
    }
    return points;
}

struct RenderedPixels {
    IntSize size;
    Vector<uint8_t> bytes;
};

static std::optional<RenderedPixels> render(const MeshGradient& gradient, IntSize size)
{
    RenderedPixels pixels { size, Vector<uint8_t>(static_cast<size_t>(size.width()) * size.height() * 4) };
    PixelBufferConversionView destination { { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() }, static_cast<unsigned>(size.width()) * 4, pixels.bytes.mutableSpan() };
    if (!MeshGradientRendererCPU { }.render(gradient, destination, size, 1))
        return std::nullopt;
    return pixels;
}

static std::array<uint8_t, 4> pixelAt(const RenderedPixels& pixels, int x, int y)
{
    size_t offset = (static_cast<size_t>(y) * pixels.size.width() + x) * 4;
    return { pixels.bytes[offset], pixels.bytes[offset + 1], pixels.bytes[offset + 2], pixels.bytes[offset + 3] };
}

TEST(MeshGradientRenderer, CPUSolidColor)
{
    IntSize size { 32, 16 };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { Color::green, Color::green, Color::green, Color::green });

    auto pixels = render(gradient, size);
    ASSERT_TRUE(pixels);

    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            auto pixel = pixelAt(*pixels, x, y);
            EXPECT_EQ(pixel[0], 0);
            EXPECT_EQ(pixel[1], 255);
            EXPECT_EQ(pixel[2], 0);
            EXPECT_EQ(pixel[3], 255);
        }
    }
}

TEST(MeshGradientRenderer, CPUHorizontalBlend)
{
    // Every row has the same colors, so the color only changes along the rows. With two columns, mirroring the colors
    // past the edges makes the slope zero at both ends, so the color eases in and out: 3t^2 - 2t^3.
    IntSize size { 100, 10 };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { Color::black, Color::white, Color::black, Color::white });

    auto pixels = render(gradient, size);
    ASSERT_TRUE(pixels);

    for (int x = 0; x < size.width(); ++x) {
        float t = (x + 0.5f) / size.width();
        auto expected = std::lround((3 * t * t - 2 * t * t * t) * 255);
        auto pixel = pixelAt(*pixels, x, size.height() / 2);
        EXPECT_NEAR(pixel[0], expected, 2);
        EXPECT_NEAR(pixel[1], expected, 2);
        EXPECT_NEAR(pixel[2], expected, 2);
        EXPECT_EQ(pixel[3], 255);
    }
}

TEST(MeshGradientRenderer, CPUColorsStayBetweenTheirPoints)
{
    // The colors along a row go from 64 to 192 and back to 128. Between two points, the color must stay between their
    // colors. Slopes estimated from the neighbors, as in Catmull-Rom splines, would go past 192.
    IntSize size { 90, 10 };
    auto gray = [](uint8_t value) {
        return Color { SRGBA<uint8_t> { value, value, value } };
    };
    auto gradient = makeMeshGradient(srgb, size, 2, 3, { gray(64), gray(192), gray(128), gray(64), gray(192), gray(128) });

    auto pixels = render(gradient, size);
    ASSERT_TRUE(pixels);

    for (int x = 0; x < size.width(); ++x) {
        auto value = pixelAt(*pixels, x, size.height() / 2)[0];
        if (x < size.width() / 2) {
            EXPECT_GE(value, 64 - 1);
            EXPECT_LE(value, 192 + 1);
        } else {
            EXPECT_GE(value, 128 - 1);
            EXPECT_LE(value, 192 + 1);
        }
    }

    // The interior of a patch can still go a little past its colors, and so past the range of the components.
    IntSize meshSize { 100, 100 };
    ColorInterpolationMethod oklab { ColorInterpolationMethod::OKLab { }, AlphaPremultiplication::Premultiplied };
    ColorInterpolationMethod hslLongerHue { ColorInterpolationMethod::HSL { HueInterpolationMethod::Longer }, AlphaPremultiplication::Premultiplied };
    ColorInterpolationMethod oklch { ColorInterpolationMethod::OKLCH { }, AlphaPremultiplication::Premultiplied };
    Color translucentBlue { SRGBA<uint8_t> { 0, 0, 255, 128 } };
    Vector<Color> colors { Color::red, Color::yellow, Color::green, Color::blue, Color::white, Color::black, Color::cyan, Color::magenta, Color::transparentBlack };

    for (auto& gradient : { makeMeshGradient(oklab, meshSize, 3, 3, colors), makeMeshGradient(hslLongerHue, meshSize, 3, 3, colors), makeMeshGradient(oklch, meshSize, 3, 3, { Color::red, Color::white, Color::green, Color::black, translucentBlue, Color::white, Color::cyan, Color::black, Color::magenta }) }) {
        auto pixels = render(gradient, meshSize);
        ASSERT_TRUE(pixels);

        for (int y = 0; y < meshSize.height(); ++y) {
            for (int x = 0; x < meshSize.width(); ++x) {
                auto pixel = pixelAt(*pixels, x, y);
                EXPECT_LE(std::max({ pixel[0], pixel[1], pixel[2] }), pixel[3]);
            }
        }
    }
}

TEST(MeshGradientRenderer, CPUBackground)
{
    // The mesh covers the top left quarter of the pixel buffer, and the background shows everywhere else.
    IntSize size { 40, 40 };
    auto gradient = makeMeshGradient(srgb, { 20, 20 }, 2, 2, { Color::red, Color::red, Color::red, Color::red }, Color::blue);

    auto pixels = render(gradient, size);
    ASSERT_TRUE(pixels);

    auto inside = pixelAt(*pixels, 10, 10);
    EXPECT_EQ(inside, (std::array<uint8_t, 4> { 255, 0, 0, 255 }));

    for (auto [x, y] : { std::pair { 30, 10 }, { 10, 30 }, { 30, 30 } }) {
        auto outside = pixelAt(*pixels, x, y);
        EXPECT_EQ(outside, (std::array<uint8_t, 4> { 0, 0, 255, 255 }));
    }
}

TEST(MeshGradientRenderer, CPUNoGapsBetweenPatchesOfDifferentSizes)
{
    auto expectOnlyRed = [](const MeshGradient& gradient, IntSize size) {
        auto pixels = render(gradient, size);
        ASSERT_TRUE(pixels);

        for (int y = 0; y < size.height(); ++y) {
            for (int x = 0; x < size.width(); ++x)
                EXPECT_EQ(pixelAt(*pixels, x, y), (std::array<uint8_t, 4> { 255, 0, 0, 255 }));
        }
    };

    // A small patch next to a large one, with a curved edge between them. If each patch chose how finely to split the
    // edge, the two sides would be different polylines, and the background would show between them.
    auto columns = makePoints(2, 3, { { 0, 0 }, { 8, 0 }, { 408, 0 }, { 0, 32 }, { 8, 32 }, { 408, 32 } }, Color::red);
    columns[1].controlPoints.setBottom({ 28, 11 });
    columns[4].controlPoints.setTop({ 28, 21 });
    expectOnlyRed(MeshGradient::create(srgb, 2, 3, WTF::move(columns), Color::blue), { 408, 32 });

    // The same mesh with rows and columns swapped.
    auto rows = makePoints(3, 2, { { 0, 0 }, { 32, 0 }, { 0, 8 }, { 32, 8 }, { 0, 408 }, { 32, 408 } }, Color::red);
    rows[2].controlPoints.setRight({ 11, 28 });
    rows[3].controlPoints.setLeft({ 21, 28 });
    expectOnlyRed(MeshGradient::create(srgb, 3, 2, WTF::move(rows), Color::blue), { 32, 408 });
}

TEST(MeshGradientRenderer, CPUNonFinitePositions)
{
    // The last of these is finite, but overflows when computing the patch. Nothing can be drawn, so only the background
    // shows.
    IntSize size { 20, 20 };
    for (float value : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max() }) {
        auto points = makePoints(2, 2, { { 0, 0 }, { 20, 0 }, { 0, 20 }, { 20, 20 } }, Color::red);
        points[3].position = { value, value };
        auto gradient = MeshGradient::create(srgb, 2, 2, WTF::move(points), Color::blue);

        auto pixels = render(gradient, size);
        ASSERT_TRUE(pixels);

        for (int y = 0; y < size.height(); ++y) {
            for (int x = 0; x < size.width(); ++x)
                EXPECT_EQ(pixelAt(*pixels, x, y), (std::array<uint8_t, 4> { 0, 0, 255, 255 }));
        }
    }
}

#if ENABLE(PIXEL_FORMAT_RGBA16F) && ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB) && ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)

TEST(MeshGradientRenderer, CPUExtendedRange)
{
    // Display P3 green is outside of sRGB, so in extended sRGB its red and blue are negative.
    IntSize size { 8, 8 };
    Color green { DisplayP3<float> { 0, 1, 0, 1 } };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { green, green, green, green }, green);

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, ColorSpace::ExtendedSRGB() };
    Vector<uint8_t> bytes(static_cast<size_t>(size.width()) * size.height() * 8);
    ASSERT_TRUE(MeshGradientRendererCPU { }.render(gradient, { format, static_cast<unsigned>(size.width()) * 8, bytes.mutableSpan() }, size, 1));

    auto components = spanReinterpretCast<const uint16_t>(bytes.span());
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[0]), -0.51f, 0.01f);
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[1]), 1.02f, 0.01f);
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[2]), -0.31f, 0.01f);
    EXPECT_EQ(WTF::convertFloat16ToFloat32(components[3]), 1.0f);
}

TEST(MeshGradientRenderer, CPUEightBitsInDestinationColorSpace)
{
    IntSize size { 8, 8 };
    Color green { DisplayP3<float> { 0, 1, 0, 1 } };
    Color halfGreen { DisplayP3<float> { 0, 1, 0, 0.5f } };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { green, green, green, green }, green);

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::DisplayP3() };
    auto renderedPixel = [&](const MeshGradient& mesh) -> std::array<int, 4> {
        Vector<uint8_t> bytes(static_cast<size_t>(size.width()) * size.height() * 4);
        EXPECT_TRUE(MeshGradientRendererCPU { }.render(mesh, { format, static_cast<unsigned>(size.width()) * 4, bytes.mutableSpan() }, size, 1));
        return { bytes[0], bytes[1], bytes[2], bytes[3] };
    };

    auto pixel = renderedPixel(gradient);
    EXPECT_LE(pixel[0], 1);
    EXPECT_GE(pixel[1], 254);
    EXPECT_LE(pixel[2], 1);
    EXPECT_EQ(pixel[3], 255);

    // Components are premultiplied after they are converted, or the transfer function would change their ratio to alpha.
    pixel = renderedPixel(makeMeshGradient(srgb, size, 2, 2, { halfGreen, halfGreen, halfGreen, halfGreen }));
    EXPECT_LE(pixel[0], 1);
    EXPECT_NEAR(pixel[1], 128, 1);
    EXPECT_LE(pixel[2], 1);
    EXPECT_NEAR(pixel[3], 128, 1);
}

#if USE(CG)

TEST(MeshGradientRenderer, DrawInColorSpaceWithoutBuiltInConversion)
{
    IntSize size { 8, 8 };
    Color green { ExtendedSRGBA<float> { 0, 1, 0, 1 } };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { green, green, green, green });

    // sRGB green is well inside Rec. 2020, so 8 bits in that color space have to hold it without any clipping.
    ColorSpace colorSpace { adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020_sRGBGamma)) };
    RefPtr imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, colorSpace, PixelFormat::BGRA8);
    MeshGradientRendererCPU { }.draw(imageBuffer->context(), gradient, FloatRect { { }, size });

    RefPtr pixels = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() }, { { }, size });
    auto bytes = pixels->bytes();
    EXPECT_LE(bytes[0], 2);
    EXPECT_GE(bytes[1], 253);
    EXPECT_LE(bytes[2], 2);
    EXPECT_EQ(bytes[3], 255);
}

#endif // USE(CG)

TEST(MeshGradientRenderer, DrawInDestinationColorSpace)
{
    IntSize size { 8, 8 };
    Color green { DisplayP3<float> { 0, 1, 0, 1 } };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { green, green, green, green });

    auto draw = [&](const ColorSpace& colorSpace) -> std::array<uint8_t, 4> {
        RefPtr imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, colorSpace, PixelFormat::BGRA8);
        MeshGradientRendererCPU { }.draw(imageBuffer->context(), gradient, FloatRect { { }, size });
        RefPtr pixels = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, colorSpace }, { { }, size });
        auto bytes = pixels->bytes();
        return { bytes[0], bytes[1], bytes[2], bytes[3] };
    };

    // sRGB can't show this color, so it clips. Display P3 can, and must not get the image cached for sRGB.
    EXPECT_EQ(draw(ColorSpace::SRGB()), (std::array<uint8_t, 4> { 0, 255, 0, 255 }));

    auto pixel = draw(ColorSpace::DisplayP3());
    EXPECT_LE(pixel[0], 2);
    EXPECT_GE(pixel[1], 253);
    EXPECT_LE(pixel[2], 2);
    EXPECT_EQ(pixel[3], 255);
}

TEST(MeshGradientRenderer, DrawInExtendedRangeDestination)
{
    IntSize size { 8, 8 };
    Color green { DisplayP3<float> { 0, 1, 0, 1 } };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { green, green, green, green });

    RefPtr imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, ColorSpace::ExtendedSRGB(), PixelFormat::RGBA16F);
    MeshGradientRendererCPU { }.draw(imageBuffer->context(), gradient, FloatRect { { }, size });

    RefPtr pixels = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, ColorSpace::ExtendedSRGB() }, { { }, size });
    auto components = spanReinterpretCast<const uint16_t>(pixels->bytes());
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[0]), -0.51f, 0.01f);
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[1]), 1.02f, 0.01f);
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[2]), -0.31f, 0.01f);
    EXPECT_EQ(WTF::convertFloat16ToFloat32(components[3]), 1.0f);
}

#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_REC_2020)

TEST(MeshGradientRenderer, DrawInDestinationTheImageBufferConvertsTo)
{
    IntSize size { 8, 8 };
    Color green { DisplayP3<float> { 0, 1, 0, 1 } };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { green, green, green, green });

    EXPECT_FALSE(MeshGradient::renderingColorSpace(ColorSpace::ExtendedRec2020()));

    RefPtr imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, ColorSpace::ExtendedRec2020(), PixelFormat::RGBA16F);
    MeshGradientRendererCPU { }.draw(imageBuffer->context(), gradient, FloatRect { { }, size });

    RefPtr pixels = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, ColorSpace::ExtendedSRGB() }, { { }, size });
    auto components = spanReinterpretCast<const uint16_t>(pixels->bytes());
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[0]), -0.51f, 0.01f);
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[1]), 1.02f, 0.01f);
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[2]), -0.31f, 0.01f);
    EXPECT_EQ(WTF::convertFloat16ToFloat32(components[3]), 1.0f);
}

#endif // ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_REC_2020)

#endif // ENABLE(PIXEL_FORMAT_RGBA16F) && ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB) && ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)

static Ref<MeshGradient> makeGradientMissingRed(IntSize size)
{
    Color missingRed { SRGBA<float> { std::numeric_limits<float>::quiet_NaN(), 1, 0, 1 } };
    return makeMeshGradient(srgb, size, 2, 2, { missingRed, Color { SRGBA<uint8_t> { 255, 255, 0 } }, Color { SRGBA<uint8_t> { 0, 255, 0 } }, Color { SRGBA<uint8_t> { 128, 0, 255 } } });
}

static Ref<MeshGradient> makeGradientMissingAlpha(IntSize size)
{
    Color red { SRGBA<float> { 1, 0, 0, std::numeric_limits<float>::quiet_NaN() } };
    Color blue { SRGBA<float> { 0, 0, 1, 0.5 } };
    return makeMeshGradient(srgb, size, 2, 2, { red, blue, red, blue });
}

static Ref<MeshGradient> makeGradientMissingHue(IntSize size)
{
    ColorInterpolationMethod oklch { ColorInterpolationMethod::OKLCH { HueInterpolationMethod::Shorter }, AlphaPremultiplication::Premultiplied };
    Color missingHue { OKLCHA<float> { 0.7f, 0.1f, std::numeric_limits<float>::quiet_NaN(), 1 } };
    Color orange { OKLCHA<float> { 0.7f, 0.1f, 50, 1 } };
    return makeMeshGradient(oklch, size, 2, 2, { missingHue, orange, orange, orange });
}

TEST(MeshGradientRenderer, CPUMissingComponent)
{
    // The top left point has no red. Along the top edge it takes the red of the top right point, and along the left
    // edge it takes the red of the bottom left point, as interpolating each pair of colors would.
    IntSize size { 100, 100 };
    auto pixels = render(makeGradientMissingRed(size), size);
    ASSERT_TRUE(pixels);

    // Pixel centers are half a pixel inside the edges, so they pick up a little of the other points.
    for (int x : { 20, 50, 80 })
        EXPECT_NEAR(pixelAt(*pixels, x, 0)[0], 255, 6);
    for (int y : { 20, 50, 80 })
        EXPECT_NEAR(pixelAt(*pixels, 0, y)[0], 0, 6);
}

TEST(MeshGradientRenderer, CPUMissingAlpha)
{
    // The points on the left have no alpha, so they are premultiplied by the alpha of the points on the right. The
    // alpha is the same everywhere.
    IntSize size { 100, 10 };
    auto pixels = render(makeGradientMissingAlpha(size), size);
    ASSERT_TRUE(pixels);

    for (int x = 0; x < size.width(); x += 10) {
        float t = (x + 0.5f) / size.width();
        float s = 3 * t * t - 2 * t * t * t;
        auto pixel = pixelAt(*pixels, x, size.height() / 2);
        EXPECT_NEAR(pixel[0], std::lround(0.5f * (1 - s) * 255), 2);
        EXPECT_NEAR(pixel[2], std::lround(0.5f * s * 255), 2);
        EXPECT_NEAR(pixel[3], 128, 1);
    }
}

TEST(MeshGradientRenderer, CPUMissingHue)
{
    // The top left point has no hue, so it takes the hue of the others, and the mesh is a solid color.
    IntSize size { 20, 20 };
    auto pixels = render(makeGradientMissingHue(size), size);
    ASSERT_TRUE(pixels);

    auto expected = pixelAt(*pixels, 19, 19);
    for (auto [x, y] : { std::pair { 0, 0 }, { 10, 10 } }) {
        auto pixel = pixelAt(*pixels, x, y);
        for (size_t i = 0; i < 4; ++i)
            EXPECT_NEAR(pixel[i], expected[i], 1);
    }
}

TEST(MeshGradient, GeometryOnlySubdividesForCurves)
{
    auto rectangle = makeMeshGradient(srgb, { 400, 300 }, 2, 2, { Color::red, Color::blue, Color::green, Color::white });
    EXPECT_EQ(rectangle->triangles(2).size(), 2u);

    auto points = makePoints(2, 2, { { 0, 0 }, { 400, 0 }, { 0, 300 }, { 400, 300 } }, Color::red);
    points[0].controlPoints.setRight({ 133, 100 });
    points[1].controlPoints.setLeft({ 266, 100 });
    auto curved = MeshGradient::create(srgb, 2, 2, WTF::move(points));
    EXPECT_GT(curved->triangles(2).size(), 2u);
}

#if USE(METAL)

// The other Metal tests skip when there is no renderer. Fail here instead, since the usual reason is that the shaders
// don't compile.
TEST(MeshGradientRenderer, MetalRendererIsAvailable)
{
    EXPECT_NE(MeshGradientRendererMetal::singleton(), nullptr);
}

TEST(MeshGradientRenderer, MetalDeclinesTexturesLargerThanTheMaximum)
{
    auto* renderer = MeshGradientRendererMetal::singleton();
    if (!renderer)
        GTEST_SKIP() << "Metal is not available.";

    IntSize size { 1, 16385 };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { Color::red, Color::red, Color::red, Color::red });

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    Vector<uint8_t> bytes(static_cast<size_t>(size.width()) * size.height() * 4);
    EXPECT_FALSE(renderer->render(gradient, { format, static_cast<unsigned>(size.width()) * 4, bytes.mutableSpan() }, size, 1));
}

static void expectMetalMatchesCPU(MeshGradientRenderer& renderer, ColorInterpolationMethod method, const char* name, const PixelBufferFormat& format)
{
    SCOPED_TRACE(name);

    IntSize size { 64, 48 };
    auto gradient = makeMeshGradient(method, size, 3, 3, {
        Color::red, Color::yellow, Color::green,
        Color::blue, Color::white, Color::black,
        Color::cyan, Color::magenta, Color::transparentBlack,
    }, Color::white);

    unsigned bytesPerRow = size.width() * PixelBuffer::bytesPerPixel(format.pixelFormat);
    Vector<uint8_t> cpuBytes(static_cast<size_t>(bytesPerRow) * size.height());
    Vector<uint8_t> metalBytes(static_cast<size_t>(bytesPerRow) * size.height());
    ASSERT_TRUE(MeshGradientRendererCPU { }.render(gradient, { format, bytesPerRow, cpuBytes.mutableSpan() }, size, 1));
    ASSERT_TRUE(renderer.render(gradient, { format, bytesPerRow, metalBytes.mutableSpan() }, size, 1));

    int maximumDifference = 0;
    for (size_t i = 0; i < cpuBytes.size(); ++i)
        maximumDifference = std::max(maximumDifference, std::abs(cpuBytes[i] - metalBytes[i]));
    EXPECT_LE(maximumDifference, 2);
}

template<typename MethodColorSpace> static ColorInterpolationMethod interpolationMethod(MethodColorSpace colorSpace)
{
    return { colorSpace, AlphaPremultiplication::Premultiplied };
}

static void expectMetalMatchesCPU(const PixelBufferFormat& format)
{
    auto* renderer = MeshGradientRendererMetal::singleton();
    if (!renderer)
        GTEST_SKIP() << "Metal is not available.";

    using Method = ColorInterpolationMethod;
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::SRGB { }), "srgb", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::SRGBLinear { }), "srgb-linear", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::DisplayP3 { }), "display-p3", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::DisplayP3Linear { }), "display-p3-linear", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::A98RGB { }), "a98-rgb", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::ProPhotoRGB { }), "prophoto-rgb", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::Rec2020 { }), "rec2020", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::Lab { }), "lab", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::OKLab { }), "oklab", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::XYZD50 { }), "xyz-d50", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::XYZD65 { }), "xyz-d65", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::LCH { HueInterpolationMethod::Shorter }), "lch", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::OKLCH { HueInterpolationMethod::Shorter }), "oklch", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::HSL { HueInterpolationMethod::Shorter }), "hsl", format);
    expectMetalMatchesCPU(*renderer, interpolationMethod(Method::HWB { HueInterpolationMethod::Shorter }), "hwb", format);
}

TEST(MeshGradientRenderer, MetalMatchesCPUInSRGB)
{
    expectMetalMatchesCPU({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() });
}

TEST(MeshGradientRenderer, MetalMatchesCPUInDisplayP3)
{
    expectMetalMatchesCPU({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::DisplayP3() });
}

#if ENABLE(PIXEL_FORMAT_RGBA16F)

TEST(MeshGradientRenderer, MetalMatchesCPUInExtendedRange)
{
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, ColorSpace::ExtendedSRGB() };

    auto* renderer = MeshGradientRendererMetal::singleton();
    if (!renderer)
        GTEST_SKIP() << "Metal is not available.";

    IntSize size { 8, 8 };
    Color green { DisplayP3<float> { 0, 1, 0, 1 } };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { green, green, green, green }, green);

    Vector<uint8_t> bytes(static_cast<size_t>(size.width()) * size.height() * 8);
    ASSERT_TRUE(renderer->render(gradient, { format, static_cast<unsigned>(size.width()) * 8, bytes.mutableSpan() }, size, 1));

    auto components = spanReinterpretCast<const uint16_t>(bytes.span());
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[0]), -0.51f, 0.01f);
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[1]), 1.02f, 0.01f);
    EXPECT_NEAR(WTF::convertFloat16ToFloat32(components[2]), -0.31f, 0.01f);
    EXPECT_EQ(WTF::convertFloat16ToFloat32(components[3]), 1.0f);
}

#endif // ENABLE(PIXEL_FORMAT_RGBA16F)

TEST(MeshGradientRenderer, MetalMatchesCPUWithMissingComponents)
{
    auto* renderer = MeshGradientRendererMetal::singleton();
    if (!renderer)
        GTEST_SKIP() << "Metal is not available.";

    IntSize size { 64, 48 };
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    for (auto [name, gradient] : { std::pair { "red", makeGradientMissingRed(size) }, { "alpha", makeGradientMissingAlpha(size) }, { "hue", makeGradientMissingHue(size) } }) {
        SCOPED_TRACE(name);

        unsigned bytesPerRow = size.width() * 4;
        Vector<uint8_t> cpuBytes(static_cast<size_t>(bytesPerRow) * size.height());
        Vector<uint8_t> metalBytes(static_cast<size_t>(bytesPerRow) * size.height());
        ASSERT_TRUE(MeshGradientRendererCPU { }.render(gradient, { format, bytesPerRow, cpuBytes.mutableSpan() }, size, 1));
        ASSERT_TRUE(renderer->render(gradient, { format, bytesPerRow, metalBytes.mutableSpan() }, size, 1));

        int maximumDifference = 0;
        for (size_t i = 0; i < cpuBytes.size(); ++i)
            maximumDifference = std::max(maximumDifference, std::abs(cpuBytes[i] - metalBytes[i]));
        EXPECT_LE(maximumDifference, 2);
    }
}

TEST(MeshGradientRenderer, MetalAntialiasesEdges)
{
    auto* renderer = MeshGradientRendererMetal::singleton();
    if (!renderer)
        GTEST_SKIP() << "Metal is not available.";

    // The top edge is slanted, so pixels along it are partly covered.
    IntSize size { 32, 32 };
    auto points = makePoints(2, 2, { { 0, 4 }, { 32, 12 }, { 0, 32 }, { 32, 32 } }, Color::white);
    auto gradient = MeshGradient::create(srgb, 2, 2, WTF::move(points), Color::black);

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    unsigned bytesPerRow = size.width() * 4;
    Vector<uint8_t> bytes(static_cast<size_t>(bytesPerRow) * size.height());
    ASSERT_TRUE(renderer->render(gradient, { format, bytesPerRow, bytes.mutableSpan() }, size, 1));

    bool foundPartialCoverage = false;
    for (size_t i = 0; i < bytes.size(); i += 4)
        foundPartialCoverage |= bytes[i] > 0 && bytes[i] < 255;
    EXPECT_TRUE(foundPartialCoverage);
}

TEST(MeshGradientRenderer, MetalDeclinesUnknownColorSpace)
{
    auto* renderer = MeshGradientRendererMetal::singleton();
    if (!renderer)
        GTEST_SKIP() << "Metal is not available.";

    IntSize size { 8, 8 };
    auto gradient = makeMeshGradient(srgb, size, 2, 2, { Color::red, Color::red, Color::red, Color::red });

    ColorSpace colorSpace { adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020_sRGBGamma)) };
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, colorSpace };
    Vector<uint8_t> bytes(static_cast<size_t>(size.width()) * size.height() * 4);
    EXPECT_FALSE(renderer->render(gradient, { format, static_cast<unsigned>(size.width()) * 4, bytes.mutableSpan() }, size, 1));
}

#if ENABLE(PIXEL_FORMAT_RGBA16F) && ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)

TEST(MeshGradientRenderer, MetalDrawsToColorSpaceTheShaderCannotConvertTo)
{
    auto* metalRenderer = MeshGradientRendererMetal::singleton();
    if (!metalRenderer)
        GTEST_SKIP() << "Metal is not available.";

    IntSize size { 32, 32 };
    ColorSpace colorSpace { adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020_sRGBGamma)) };

    auto draw = [&](MeshGradientRenderer& renderer, float alpha) {
        auto color = [&](float red, float green, float blue) {
            return Color { SRGBA<float> { red, green, blue, alpha } };
        };
        auto gradient = makeMeshGradient(srgb, size, 2, 2, { color(1, 0, 0), color(0, 1, 0), color(0, 0, 1), color(1, 1, 0) });

        RefPtr imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, colorSpace, PixelFormat::BGRA8);
        EXPECT_TRUE(renderer.draw(imageBuffer->context(), gradient, FloatRect { { }, size }));
        return imageBuffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, colorSpace }, { { }, size });
    };

    for (float alpha : { 1.0f, 0.5f }) {
        MeshGradientRendererCPU cpuRenderer;
        RefPtr cpu = draw(cpuRenderer, alpha);
        RefPtr metal = draw(*metalRenderer, alpha);

        auto cpuBytes = cpu->bytes();
        auto metalBytes = metal->bytes();
        ASSERT_EQ(cpuBytes.size(), metalBytes.size());

        int worst = 0;
        for (size_t i = 0; i < cpuBytes.size(); ++i)
            worst = std::max(worst, std::abs(cpuBytes[i] - metalBytes[i]));
        EXPECT_LE(worst, 2) << "alpha " << alpha;
    }
}

#endif // ENABLE(PIXEL_FORMAT_RGBA16F) && ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)

#endif // USE(METAL)

} // namespace TestWebKitAPI
