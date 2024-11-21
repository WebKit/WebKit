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

#include <algorithm>
#include <limits>

namespace WebCore {
namespace CSS {

// Representation for `CSS bracketed range notation`. Represents a closed range between (and including) `min` and `max`.
// https://drafts.csswg.org/css-values-4/#numeric-ranges
struct Range {
    // Convenience to allow for a shorter spelling of the appropriate infinity.
    static constexpr auto infinity = std::numeric_limits<double>::infinity();

    double min { -infinity };
    double max {  infinity };

    constexpr bool operator==(const Range&) const = default;
};

// Constant value for `[−∞,∞]`.
inline constexpr auto All = Range { -Range::infinity, Range::infinity };

// Constant value for `[0,∞]`.
inline constexpr auto Nonnegative = Range { 0, Range::infinity };

// Clamps a floating point value to within `range`.
template<Range range, std::floating_point T> constexpr T clampToRange(T value)
{
    return std::clamp<T>(value, range.min, range.max);
}

} // namespace CSS
} // namespace WebCore
