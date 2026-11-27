/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ColorGamutMapping.h"

#include "ColorConversion.h"

namespace WebCore {

// Where the ray from `start` through `end` crosses the surface of the cube [0, 1].
static std::optional<ColorComponents<float, 3>> castRayThroughLinearRGBGamut(const ColorComponents<float, 3>& start, const ColorComponents<float, 3>& end)
{
    // https://drafts.csswg.org/css-color-4/#pseudo-raytrace

    // The specification's value for 32-bit floats.
    constexpr auto rayLengthEpsilon = 1e-6f;

    // 1. The lower and upper bounds of the gamut are 0 and 1 for each component.
    constexpr auto bmin = 0.0f;
    constexpr auto bmax = 1.0f;

    // 2. Let tfar be infinity.
    auto tfar = std::numeric_limits<float>::infinity();

    // 3. Let tnear be negative infinity.
    auto tnear = -std::numeric_limits<float>::infinity();

    // 4. Let direction be an empty array of 3 elements.
    ColorComponents<float, 3> direction;

    // 5. Let length be 0.
    float length = 0;

    // 6. Let magnitude be 0.
    float magnitude = 0;

    // 7. For each component i of start and end:
    for (size_t i = 0; i < 3; ++i) {
        // Let a be start[i] and b be end[i].
        auto a = start[i];
        auto b = end[i];

        // Let d be b - a, and set direction[i] to d.
        auto d = b - a;
        direction[i] = d;

        // Set length to the larger of length and |d|, and magnitude to the largest of magnitude, |a| and |b|.
        length = std::max(length, std::abs(d));
        magnitude = std::max({ magnitude, std::abs(a), std::abs(b) });

        if (d) {
            // If d is not 0, find where the ray meets the planes of the lower and upper bounds, and narrow tnear and tfar to them.
            auto t1 = (bmin - a) / d;
            auto t2 = (bmax - a) / d;
            tnear = std::max(std::min(t1, t2), tnear);
            tfar = std::min(std::max(t1, t2), tfar);
        } else if (a < bmin || a > bmax) {
            // Otherwise, the ray is parallel to those planes, and if a is outside of them, there is no intersection.
            return std::nullopt;
        }
    }

    // 8. If length is too small relative to magnitude, there is no intersection.
    if (length <= magnitude * rayLengthEpsilon)
        return std::nullopt;

    // 9. If tnear is greater than tfar, or tfar is less than 0, there is no intersection.
    if (tnear > tfar || tfar < 0)
        return std::nullopt;

    // 10. If tnear is less than 0, set tnear to tfar.
    if (tnear < 0)
        tnear = tfar;

    // 11. If tnear is still infinite, there is no intersection.
    if (std::isinf(tnear))
        return std::nullopt;

    // 12. Let result be the point start + direction * tnear.
    // 13. Return result.
    return ColorComponents<float, 3> { start[0] + direction[0] * tnear, start[1] + direction[1] * tnear, start[2] + direction[2] * tnear };
}

ColorComponents<float, 3> RayTraceGamutMapping::map(const RGBGamut& gamut, const ColorComponents<float, 3>& linearRGB)
{
    // https://drafts.csswg.org/css-color-4/#pseudo-raytrace

    // The specification's value for 32-bit floats.
    constexpr auto anchorEpsilon = 1e-5f;

    // Keeps rounding error from mapping colors that are in gamut.
    constexpr auto inGamutTolerance = 1e-5f;

    constexpr unsigned iterationCount = 4;

    auto toOKLCH = [&](const ColorComponents<float, 3>& linear) {
        auto [x, y, z] = gamut.linearRGBToXYZD65.transformedColorComponents(linear);
        return convertColor<OKLCHA<float>>(XYZA<float, WhitePoint::D65> { x, y, z, 1.0f }).resolved();
    };
    auto toLinear = [&](const OKLCHA<float>& oklch) {
        auto [x, y, z, alpha] = convertColor<XYZA<float, WhitePoint::D65>>(oklch).resolved();
        return gamut.xyzD65ToLinearRGB.transformedColorComponents(ColorComponents<float, 3> { x, y, z });
    };
    auto isBetween = [](const ColorComponents<float, 3>& linear, float low, float high) {
        auto [r, g, b] = linear;
        return r >= low && r <= high && g >= low && g <= high && b >= low && b <= high;
    };

    // 10. Let clip(color) be color with each component clamped to the range of the destination.
    auto clip = [&](const ColorComponents<float, 3>& linear) {
        return ClipGamutMapping::map(gamut, linear);
    };

    // 1. The destination is an RGB color space, so it has gamut limits.

    // 2. Let origin_OkLCh be origin converted to OkLCh.
    auto origin = toOKLCH(linearRGB);

    // 3. If the lightness of origin_OkLCh is 100% or more, return white in the destination.
    if (origin.lightness >= 1)
        return { 1.0f, 1.0f, 1.0f };

    // 4. If the lightness of origin_OkLCh is 0% or less, return black in the destination.
    if (origin.lightness <= 0)
        return { 0.0f, 0.0f, 0.0f };

    // 5. Let l_origin be the lightness of origin_OkLCh.
    // 6. Let h_origin be the hue of origin_OkLCh.
    auto lOrigin = origin.lightness;
    auto hOrigin = origin.hue;

    // 7. Let anchor be the achromatic color with lightness l_origin, in the linear-light destination, clipped to its gamut.
    auto anchor = clip(toLinear({ lOrigin, 0, hOrigin, 1.0f }));

    // 8. Let origin_rgb be origin in the linear-light destination.
    auto originRGB = linearRGB;

    // 9. If origin_rgb is not in gamut:
    if (!isBetween(originRGB, -inGamutTolerance, 1 + inGamutTolerance)) {
        // Let low be a small epsilon, and high be 1 minus it.
        constexpr auto low = anchorEpsilon;
        constexpr auto high = 1 - anchorEpsilon;

        // Let last be origin_rgb.
        auto last = originRGB;

        // For each iteration i:
        for (unsigned i = 0; i < iterationCount; ++i) {
            // If i is not 0, correct origin_rgb back to the chroma reduction curve, with lightness l_origin and hue h_origin.
            if (i)
                originRGB = toLinear({ lOrigin, toOKLCH(originRGB).chroma, hOrigin, 1.0f });

            // Let intersection be where the ray from anchor through origin_rgb crosses the surface of the gamut.
            auto intersection = castRayThroughLinearRGBGamut(anchor, originRGB);

            // If there is no intersection, set origin_rgb to last and stop.
            if (!intersection) {
                originRGB = last;
                break;
            }

            // If i is not 0 and each component of origin_rgb is between low and high, set anchor to origin_rgb.
            if (i && isBetween(originRGB, low, high))
                anchor = originRGB;

            // Set origin_rgb and last to intersection.
            originRGB = *intersection;
            last = *intersection;
        }
    }

    // 11. Let clipped be clip(origin_rgb).
    // 12. Return clipped.
    return clip(originRGB);
}

} // namespace WebCore
