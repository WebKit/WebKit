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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FloatSize.h>
#include <WebCore/ImageOrientation.h>
#include <optional>
#include <wtf/MathExtras.h>

namespace WebCore {

// https://drafts.csswg.org/css-images-3/#natural-dimensions
struct NaturalDimensions {
    std::optional<float> width;
    std::optional<float> height;
    std::optional<FloatSize> aspectRatio;

    // https://html.spec.whatwg.org/multipage/images.html#density-corrected-intrinsic-width-and-height
    constexpr NaturalDimensions densityCorrected(float imageDevicePixelRatio) const
    {
        if (imageDevicePixelRatio == 1)
            return *this;
        auto scaled = [&](auto dimension) -> std::optional<float> {
            if (!dimension)
                return std::nullopt;
            return *dimension * imageDevicePixelRatio;
        };
        return { scaled(width), scaled(height), aspectRatio };
    }

    constexpr NaturalDimensions oriented(ImageOrientation orientation) const
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(orientation.orientation() != ImageOrientation::Orientation::FromImage);
        if (!orientation.usesWidthAsHeight())
            return *this;
        return { height, width, aspectRatio ? std::optional { aspectRatio->transposedSize() } : std::nullopt };
    }

    constexpr bool hasUsableAspectRatio() const
    {
        // "If an object has a degenerate natural aspect ratio (at least one part being zero or
        // infinity), it is treated as having no natural aspect ratio."
        // https://drafts.csswg.org/css-images-3/#natural-aspect-ratio

        return aspectRatio
            && aspectRatio->width() > 0 && aspectRatio->height() > 0
            && std::isfinite(aspectRatio->width()) && std::isfinite(aspectRatio->height());
    }

    constexpr bool isNone() const { return !width && !height && !aspectRatio; }

    constexpr static NaturalDimensions none() { return { .width = std::nullopt, .height = std::nullopt, .aspectRatio = std::nullopt }; }
    constexpr static NaturalDimensions zero() { return fixed(0, 0); }
    constexpr static NaturalDimensions fixed(FloatSize size) { return { .width = size.width(), .height = size.height(), .aspectRatio = size }; }
    constexpr static NaturalDimensions fixed(float width, float height) { return fixed(FloatSize { width, height }); }

    constexpr bool operator==(const NaturalDimensions&) const = default;
};

} // namespace WebCore
