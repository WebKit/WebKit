/*
 * Copyright (C) 2025 Sony Interactive Entertainment Inc.
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

#include <WebCore/IntSize.h>
#include <WebCore/PixelBufferConversion.h>
#include <vector>
#include <wtf/Float16.h>
#include <wtf/StdLibExtras.h>

namespace TestWebKitAPI {
using namespace WebCore;

TEST(PixelBufferConversionTests, convertImagePixels)
{
    auto convert = [&](PixelFormat sourceFormat, PixelFormat destinationFormat) -> std::vector<uint8_t> {
        const PixelBufferFormat sourcePixelBufferFormat { AlphaPremultiplication::Unpremultiplied, sourceFormat, DestinationColorSpace::SRGB() };
        const PixelBufferFormat destinationPixelBufferFormat { AlphaPremultiplication::Unpremultiplied, destinationFormat, DestinationColorSpace::SRGB() };
        const std::vector<uint8_t> sourceBytes = { 1, 2, 3, 4 };
        std::vector<uint8_t> destinationBytes(4);
        constexpr int bytesPerRow = 4;
        const PixelBufferConversionView destination { sourcePixelBufferFormat, bytesPerRow, destinationBytes };
        const ConstPixelBufferConversionView source { destinationPixelBufferFormat, bytesPerRow, sourceBytes };
        const IntSize size { 1, 1 };

        convertImagePixels(source, destination, size);
        return destinationBytes;
    };

    EXPECT_EQ(convert(PixelFormat::RGBA8, PixelFormat::RGBA8), (std::vector<uint8_t> { 1, 2, 3, 4 }));
    EXPECT_EQ(convert(PixelFormat::RGBA8, PixelFormat::BGRA8), (std::vector<uint8_t> { 3, 2, 1, 4 }));
    EXPECT_EQ(convert(PixelFormat::RGBA8, PixelFormat::BGRX8), (std::vector<uint8_t> { 3, 2, 1, 4 }));

    EXPECT_EQ(convert(PixelFormat::BGRA8, PixelFormat::RGBA8), (std::vector<uint8_t> { 3, 2, 1, 4 }));
    EXPECT_EQ(convert(PixelFormat::BGRA8, PixelFormat::BGRA8), (std::vector<uint8_t> { 1, 2, 3, 4 }));
    EXPECT_EQ(convert(PixelFormat::BGRA8, PixelFormat::BGRX8), (std::vector<uint8_t> { 3, 2, 1, 4 }));
}

TEST(PixelBufferConversionTests, convertImagePixels2)
{
    auto convert = [&](PixelFormat sourceFormat, PixelFormat destinationFormat) -> std::vector<uint8_t> {
        const PixelBufferFormat sourcePixelBufferFormat { AlphaPremultiplication::Unpremultiplied, sourceFormat, DestinationColorSpace::SRGB() };
        const PixelBufferFormat destinationPixelBufferFormat { AlphaPremultiplication::Unpremultiplied, destinationFormat, DestinationColorSpace::SRGB() };
        const std::vector<uint8_t> sourceBytes = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
        std::vector<uint8_t> destinationBytes(8);
        constexpr int destinationBytesPerRow = 4;
        const PixelBufferConversionView destination { sourcePixelBufferFormat, destinationBytesPerRow, destinationBytes };
        constexpr int sourceBytesPerRow = 8;
        const ConstPixelBufferConversionView source { destinationPixelBufferFormat, sourceBytesPerRow, sourceBytes };
        const IntSize size { 1, 2 };

        convertImagePixels(source, destination, size);
        return destinationBytes;
    };

    EXPECT_EQ(convert(PixelFormat::RGBA8, PixelFormat::RGBA8), (std::vector<uint8_t> { 1, 2, 3, 4, 9, 10, 11, 12 }));
    EXPECT_EQ(convert(PixelFormat::RGBA8, PixelFormat::BGRA8), (std::vector<uint8_t> { 3, 2, 1, 4, 11, 10, 9, 12 }));
}

#if ENABLE(PIXEL_FORMAT_RGBA16F)

static std::vector<uint8_t> byteVectorFromFloat16s(const std::vector<Float16>& components)
{
    std::vector<uint8_t> bytes(components.size() * sizeof(Float16));
    memcpySpan(std::span { bytes }, asByteSpan(std::span { components }));
    return bytes;
}

static std::vector<Float16> float16sFromByteVector(const std::vector<uint8_t>& bytes)
{
    std::vector<Float16> components(bytes.size() / sizeof(Float16));
    memcpySpan(asMutableByteSpan(std::span { components }), std::span { bytes });
    return components;
}

static std::vector<Float16> convertFloat16s(AlphaPremultiplication sourceAlphaFormat, DestinationColorSpace sourceColorSpace, AlphaPremultiplication destinationAlphaFormat, DestinationColorSpace destinationDestinationColorSpace, const std::vector<Float16>& components)
{
    RELEASE_ASSERT(!(components.size() % 4));
    IntSize size(components.size() / 4, 1);
    auto sourceBytes = byteVectorFromFloat16s(components);
    unsigned bytesPerRow = sourceBytes.size();
    std::vector<uint8_t> destinationBytes(bytesPerRow);
    ConstPixelBufferConversionView source { PixelBufferFormat { .alphaFormat = sourceAlphaFormat, .pixelFormat = PixelFormat::RGBA16F, .colorSpace = sourceColorSpace }, bytesPerRow, sourceBytes };
    PixelBufferConversionView destination { PixelBufferFormat { .alphaFormat = destinationAlphaFormat, .pixelFormat = PixelFormat::RGBA16F, .colorSpace = destinationDestinationColorSpace }, bytesPerRow, destinationBytes };

    convertImagePixels(source, destination, size);

    return float16sFromByteVector(destinationBytes);
};

// Half-float comparison with a tolerance, since color space conversions are not bit-exact.
static void expectFloat16sNear(const std::vector<Float16>& actual, const std::vector<float>& expected, float tolerance = 1.f / 2048.f)
{
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
        EXPECT_NEAR(static_cast<float>(actual[i]), expected[i], tolerance) << "component " << i;
}

TEST(PixelBufferConversionTests, convertImagePixelsFloat16Identical)
{
    std::vector<Float16> sourceFloat16s { 3.0, 0.5, 0.25, 1.0, 0.125, -0.25, 0.5, 1.0 };
    EXPECT_EQ(convertFloat16s(AlphaPremultiplication::Premultiplied, DestinationColorSpace::SRGB(), AlphaPremultiplication::Premultiplied, DestinationColorSpace::SRGB(), sourceFloat16s), sourceFloat16s);
}

TEST(PixelBufferConversionTests, convertImagePixelsFloat16AlphaOnly)
{
    // Alpha-only changes within RGBA16F use the vImage half-float premultiply entry points.
    auto convert = [](AlphaPremultiplication sourceAlphaFormat, AlphaPremultiplication destinationAlphaFormat, const std::vector<Float16>& components) {
        return convertFloat16s(sourceAlphaFormat, DestinationColorSpace::SRGB(), destinationAlphaFormat, DestinationColorSpace::SRGB(), components);
    };

    expectFloat16sNear(convert(AlphaPremultiplication::Unpremultiplied, AlphaPremultiplication::Premultiplied, { 1.0, 0.5, 0.25, 0.5 }), { 0.5, 0.25, 0.125, 0.5 });
    expectFloat16sNear(convert(AlphaPremultiplication::Premultiplied, AlphaPremultiplication::Unpremultiplied, { 0.5, 0.25, 0.125, 0.5 }), { 1.0, 0.5, 0.25, 0.5 });

    // A zero alpha must not divide by zero.
    expectFloat16sNear(convert(AlphaPremultiplication::Premultiplied, AlphaPremultiplication::Unpremultiplied, { 0.0, 0.0, 0.0, 0.0 }), { 0.0, 0.0, 0.0, 0.0 });
}

TEST(PixelBufferConversionTests, convertImagePixelsFloat16ColorSpaceConversion)
{
    // Orange rgb(255, 165, 0) ~ color(srgb 1 0.6471 0) -> color(display-p3 0.9497 0.6629 0.2330)
    std::vector<Float16> sourceFloat16s { 1.0, 0.6471, 0.0, 1.0 };
    expectFloat16sNear(convertFloat16s(AlphaPremultiplication::Premultiplied, DestinationColorSpace::SRGB(), AlphaPremultiplication::Premultiplied, DestinationColorSpace::ExtendedDisplayP3(), sourceFloat16s), { 0.9497, 0.6629, 0.2330, 1.0 });
}

TEST(PixelBufferConversionTests, convertImagePixelsFloat16ToAndFromByte)
{
    const auto colorSpace = DestinationColorSpace::SRGB();
    constexpr int float16BytesPerRow = 4 * sizeof(Float16);
    constexpr int u8BytesPerRow = 4;

    // RGBA16F to BGRA8: components are scaled to [0, 255] and the channel order is swapped.
    {
        const auto sourceBytes = byteVectorFromFloat16s({ 1.0, 0.5, 0.0, 1.0 });
        std::vector<uint8_t> destinationBytes(4);
        const ConstPixelBufferConversionView source { { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, colorSpace }, float16BytesPerRow, sourceBytes };
        const PixelBufferConversionView destination { { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, colorSpace }, u8BytesPerRow, destinationBytes };

        convertImagePixels(source, destination, { 1, 1 });

        EXPECT_EQ(destinationBytes[0], 0);
        EXPECT_NEAR(destinationBytes[1], 128U, 1);
        EXPECT_EQ(destinationBytes[2], 255);
        EXPECT_EQ(destinationBytes[3], 255);
    }

    // BGRA8 to RGBA16F round-trips back to the same values.
    {
        const std::vector<uint8_t> sourceBytes { 0, 128, 255, 255 };
        std::vector<uint8_t> destinationBytes(4 * sizeof(Float16));
        const ConstPixelBufferConversionView source { { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, colorSpace }, u8BytesPerRow, sourceBytes };
        const PixelBufferConversionView destination { { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, colorSpace }, float16BytesPerRow, destinationBytes };

        convertImagePixels(source, destination, { 1, 1 });

        expectFloat16sNear(float16sFromByteVector(destinationBytes), { 1.0, 128.0 / 255.0, 0.0, 1.0 });
    }
}

TEST(PixelBufferConversionTests, convertImagePixelsFloat16PaddedRows)
{
    const auto colorSpace = DestinationColorSpace::SRGB();
    const auto sourceBytes = byteVectorFromFloat16s({
        1.0, 0.0, 0.0, 1.0, /* padding: */ 0.0, 0.0,
        0.0, 1.0, 0.0, 1.0, /* padding: */ 0.0, 0.0,
    });
    std::vector<uint8_t> destinationBytes(2 * 6);
    const ConstPixelBufferConversionView source { { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, colorSpace }, 6 * sizeof(Float16), sourceBytes };
    const PixelBufferConversionView destination { { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, colorSpace }, 6, destinationBytes };

    convertImagePixels(source, destination, { 1, 2 });

    // Row 0 is red, row 1 is green, each at the start of its (padded) row.
    EXPECT_EQ(destinationBytes[0], 255);
    EXPECT_EQ(destinationBytes[1], 0);
    EXPECT_EQ(destinationBytes[2], 0);
    EXPECT_EQ(destinationBytes[3], 255);
    EXPECT_EQ(destinationBytes[6], 0);
    EXPECT_EQ(destinationBytes[7], 255);
    EXPECT_EQ(destinationBytes[8], 0);
    EXPECT_EQ(destinationBytes[9], 255);
}

#endif // ENABLE(PIXEL_FORMAT_RGBA16F)

} // namespace TestWebKitAPI
