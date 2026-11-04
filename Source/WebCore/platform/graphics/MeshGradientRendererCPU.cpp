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
#include "MeshGradientRendererCPU.h"

#include "ColorConversion.h"
#include "IntSize.h"
#include "MeshGradient.h"
#include "PixelBufferConversion.h"

namespace WebCore {

static constexpr unsigned bytesPerPixel = 4;

static void writePixel(const PixelBufferConversionView& destination, int x, int y, SRGBA<uint8_t> color)
{
    auto pixel = destination.rows.subspan(static_cast<size_t>(y) * destination.bytesPerRow + static_cast<size_t>(x) * bytesPerPixel, bytesPerPixel);
    auto [red, green, blue, alpha] = color.resolved();
    pixel[0] = red;
    pixel[1] = green;
    pixel[2] = blue;
    pixel[3] = alpha;
}

bool MeshGradientRendererCPU::render(const MeshGradient& gradient, const PixelBufferConversionView& destination, const IntSize& size, float)
{
    ASSERT(destination.format.alphaFormat == AlphaPremultiplication::Premultiplied);
    ASSERT(destination.format.pixelFormat == PixelFormat::RGBA8);
    ASSERT(destination.format.colorSpace == ColorSpace::SRGB());

    if (size.isEmpty())
        return true;

    if (destination.bytesPerRow < size.width() * bytesPerPixel || destination.rows.size() / destination.bytesPerRow < static_cast<size_t>(size.height()))
        return false;

    auto background = convertColor<SRGBA<uint8_t>>(gradient.premultipliedSRGBBackgroundColor());
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x)
            writePixel(destination, x, y, background);
    }

    // FIXME: Draw the mesh.

    return true;
}

} // namespace WebCore
