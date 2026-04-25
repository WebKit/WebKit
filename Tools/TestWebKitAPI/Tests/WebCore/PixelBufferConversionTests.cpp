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

} // namespace TestWebKitAPI
