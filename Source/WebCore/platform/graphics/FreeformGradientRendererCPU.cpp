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
#include "FreeformGradientRendererCPU.h"

#include "ColorConversion.h"
#include "FreeformGradient.h"
#include "IntSize.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"
#include <wtf/Float16.h>

namespace WebCore {

static bool isSupportedFormat(const PixelBufferFormat& format)
{
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (format.pixelFormat == PixelFormat::RGBA16F)
        return true;
#endif
    return format.pixelFormat == PixelFormat::RGBA8;
}

static void writePixel(const PixelBufferConversionView& destination, int x, int y, const ResolvedPremultiplied<ColorComponents<float, 4>>& color)
{
    auto bytesPerPixel = PixelBuffer::bytesPerPixel(destination.format.pixelFormat);
    auto pixel = destination.rows.subspan(static_cast<size_t>(y) * destination.bytesPerRow + static_cast<size_t>(x) * bytesPerPixel, bytesPerPixel);

#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (destination.format.pixelFormat == PixelFormat::RGBA16F) {
        auto [red, green, blue, alpha] = color;
        std::array components { WTF::convertFloat32ToFloat16(red), WTF::convertFloat32ToFloat16(green), WTF::convertFloat32ToFloat16(blue), WTF::convertFloat32ToFloat16(alpha) };
        memcpySpan(pixel, asByteSpan(std::span { components }));
        return;
    }
#endif

    auto components = convertComponentType<uint8_t>(clampedToAlpha(color)).resolved().components;
    memcpySpan(pixel, asByteSpan(std::span { components }));
}

bool FreeformGradientRendererCPU::render(const FreeformGradient& gradient, const PixelBufferConversionView& destination, const IntSize& size, float scale)
{
    ASSERT(destination.format.alphaFormat == AlphaPremultiplication::Premultiplied);

    auto colorSpace = FreeformGradient::renderingColorSpace(destination.format.colorSpace);
    if (!isSupportedFormat(destination.format) || !colorSpace) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (size.isEmpty())
        return true;

    if (destination.bytesPerRow < size.width() * PixelBuffer::bytesPerPixel(destination.format.pixelFormat) || destination.rows.size() / destination.bytesPerRow < static_cast<size_t>(size.height()))
        return false;

    // Each pixel is the color at its center.
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            FloatPoint center { (x + 0.5f) / scale, (y + 0.5f) / scale };
            writePixel(destination, x, y, gradient.premultipliedColor(center, *colorSpace));
        }
    }

    return true;
}

} // namespace WebCore
