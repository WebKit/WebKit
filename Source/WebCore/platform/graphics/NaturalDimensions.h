/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

namespace WebCore {

// https://drafts.csswg.org/css-images-3/#natural-dimensions
struct NaturalDimensions {
    std::optional<float> width;
    std::optional<float> height;
    std::optional<FloatSize> aspectRatio;

    NaturalDimensions oriented(ImageOrientation orientation) const
    {
        ASSERT(orientation.orientation() != ImageOrientation::Orientation::FromImage);
        if (!orientation.usesWidthAsHeight())
            return *this;
        return { height, width, aspectRatio ? std::make_optional(aspectRatio->transposedSize()) : std::nullopt };
    }

    static NaturalDimensions none() { return { }; }
    static NaturalDimensions fixed(FloatSize size) { return { size.width(), size.height(), size }; }
    static NaturalDimensions fixed(float width, float height) { return fixed(FloatSize { width, height }); }

    bool operator==(const NaturalDimensions&) const = default;
};

} // namespace WebCore
