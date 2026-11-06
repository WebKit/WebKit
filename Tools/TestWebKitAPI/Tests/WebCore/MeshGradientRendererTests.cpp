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
#include <WebCore/IntSize.h>
#include <WebCore/MeshGradient.h>
#include <WebCore/MeshGradientRendererCPU.h>
#include <WebCore/PixelBufferConversion.h>
#include <algorithm>
#include <cmath>

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

} // namespace TestWebKitAPI
