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
#include "GeometryUtilities.h"
#include "IntSize.h"
#include "MeshGradient.h"
#include "PixelBufferConversion.h"

namespace WebCore {

static constexpr unsigned bytesPerPixel = 4;

static void writePixel(const PixelBufferConversionView& destination, int x, int y, ResolvedPremultiplied<SRGBA<uint8_t>> color)
{
    auto pixel = destination.rows.subspan(static_cast<size_t>(y) * destination.bytesPerRow + static_cast<size_t>(x) * bytesPerPixel, bytesPerPixel);
    auto [red, green, blue, alpha] = color;
    pixel[0] = red;
    pixel[1] = green;
    pixel[2] = blue;
    pixel[3] = alpha;
}

// Twice the signed area of the triangle (from, to, point). The sign says which side of that edge the point is on.
//
// Juan Pineda, "A Parallel Algorithm for Polygon Rasterization" (1988)
// https://doi.org/10.1145/378456.378457
static float edgeFunction(FloatPoint from, FloatPoint to, FloatPoint point)
{
    // Two triangles sharing an edge go along it in opposite directions, so rounding could put a pixel center on the
    // edge outside of both. Always evaluating the edge in the same direction gives both the same answer.
    if (to.x() < from.x() || (to.x() == from.x() && to.y() < from.y()))
        return -edgeFunction(to, from, point);

    return (to - from).cross(point - from);
}

static void drawTriangle(const PixelBufferConversionView& destination, const IntSize& size, const MeshGradient::Triangle& triangle)
{
    auto& [a, b, c] = triangle;

    // If a position isn't finite, neither is the area. This also covers positions that overflowed.
    float area = edgeFunction(a.position, b.position, c.position);
    if (!area || !std::isfinite(area))
        return;

    auto colorA = asColorComponents(a.color);
    auto colorB = asColorComponents(b.color);
    auto colorC = asColorComponents(c.color);

    // Only visit the pixels within the triangle's bounds, clipped to the destination.
    auto clamped = [](float value, int maximum) {
        return static_cast<int>(std::clamp(value, 0.0f, static_cast<float>(maximum)));
    };
    int minimumX = clamped(std::floor(min3(a.position.x(), b.position.x(), c.position.x())), size.width() - 1);
    int maximumX = clamped(std::ceil(max3(a.position.x(), b.position.x(), c.position.x())), size.width() - 1);
    int minimumY = clamped(std::floor(min3(a.position.y(), b.position.y(), c.position.y())), size.height() - 1);
    int maximumY = clamped(std::ceil(max3(a.position.y(), b.position.y(), c.position.y())), size.height() - 1);

    for (int y = minimumY; y <= maximumY; ++y) {
        for (int x = minimumX; x <= maximumX; ++x) {
            FloatPoint center { x + 0.5f, y + 0.5f };

            // A vertex's barycentric weight is the area of the triangle formed by the pixel's center and the other two
            // vertices, divided by the area of the whole triangle. The center is inside when no weight is negative.
            //
            // Where a patch folds over itself, a triangle's vertices go the other way around. Dividing by the signed
            // area keeps the check working.
            float weightA = edgeFunction(b.position, c.position, center) / area;
            float weightB = edgeFunction(c.position, a.position, center) / area;
            float weightC = edgeFunction(a.position, b.position, center) / area;
            if (weightA < 0 || weightB < 0 || weightC < 0)
                continue;

            // FIXME: Pixels at the edges of the mesh are not antialiased.
            auto color = colorA * weightA;
            color += colorB * weightB;
            color += colorC * weightC;

            writePixel(destination, x, y, convertComponentType<uint8_t>(Premultiplied { makeFromComponentsClamping<SRGBA<float>>(color).resolved() }));
        }
    }
}

bool MeshGradientRendererCPU::render(const MeshGradient& gradient, const PixelBufferConversionView& destination, const IntSize& size, float scale)
{
    ASSERT(destination.format.alphaFormat == AlphaPremultiplication::Premultiplied);
    ASSERT(destination.format.pixelFormat == PixelFormat::RGBA8);
    ASSERT(destination.format.colorSpace == ColorSpace::SRGB());

    if (size.isEmpty())
        return true;

    if (destination.bytesPerRow < size.width() * bytesPerPixel || destination.rows.size() / destination.bytesPerRow < static_cast<size_t>(size.height()))
        return false;

    auto background = convertComponentType<uint8_t>(gradient.premultipliedSRGBBackgroundColor());
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x)
            writePixel(destination, x, y, background);
    }

    for (auto& triangle : gradient.triangles(scale))
        drawTriangle(destination, size, triangle);

    return true;
}

} // namespace WebCore
