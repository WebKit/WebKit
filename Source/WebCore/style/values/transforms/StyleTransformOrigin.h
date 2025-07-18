/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "StylePosition.h"
#include "StylePrimitiveNumeric.h"

namespace WebCore {
namespace Style {

using TransformOriginX = PositionX;
using TransformOriginY = PositionY;
using TransformOriginZ = Length<>;
using TransformOriginXY = Position;

// https://www.w3.org/TR/css-transforms-1/#propdef-transform-origin
struct TransformOrigin {
    using X = TransformOriginX;
    using Y = TransformOriginY;
    using Z = TransformOriginZ;
    using XY = TransformOriginXY;

    TransformOriginX x;
    TransformOriginY y;
    TransformOriginZ z;

    TransformOriginXY xy() const { return { x, y }; }

    bool operator==(const TransformOrigin&) const = default;
};

template<size_t I> const auto& get(const TransformOrigin& value)
{
    if constexpr (!I)
        return value.x;
    else if constexpr (I == 1)
        return value.y;
    else if constexpr (I == 2)
        return value.z;
}

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::TransformOrigin, 3);
