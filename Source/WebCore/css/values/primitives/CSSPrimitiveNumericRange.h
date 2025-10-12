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
#include <wtf/MathExtras.h>

namespace WebCore {
namespace CSS {

// Options to indicate how the range should be interpreted.
enum class RangeClampOptions {
    // `Default` indicates that at parse time, out of range values invalidate the parse.
    // Out of range values at style building always clamp.
    Default,

    // `ClampLower` indicates that parse time, an out of range lower value should clamp
    // instead of invalidating the parse. An out of range upper value will still invalidate
    // the parse. Out of range values at style building always clamp.
    ClampLower,

    // `ClampUpper` indicates that parse time, an out of range upper value should clamp
    // instead of invalidating the parse. An out of range lower value will still invalidate
    // the parse. Out of range values at style building always clamp.
    ClampUpper,

    // `ClampBoth` indicates that parse time, an out of range lower or upper value should
    // clamp instead of invalidating the parse. Out of range values at style building
    // always clamp.
    ClampBoth
};

// Options to indicate how the primitive should consider its value with regards to zoom.
// NOTE: This option is only meaningful for Style::Length`.
// FIXME: These options are temporary while `zoom` is moving from style building time to use time.
enum class RangeZoomOptions {
    // `Default` indicates the value held in the primitive has had zoom applied to it.
    Default,

    // `Unzoomed` indicates the value held in the primitive has NOT had zoom applied to it.
    Unzoomed
};

// Representation for `CSS bracketed range notation`. Represents a closed range between (and including) `min` and `max`.
// https://drafts.csswg.org/css-values-4/#numeric-ranges
struct Range {
    // Convenience to allow for a shorter spelling of the appropriate infinity.
    static constexpr auto infinity = std::numeric_limits<double>::infinity();

    double min { -infinity };
    double max {  infinity };
    RangeClampOptions clampOptions { RangeClampOptions::Default };
    RangeZoomOptions zoomOptions { RangeZoomOptions::Default };

    constexpr Range(double min, double max, RangeClampOptions clampOptions = RangeClampOptions::Default, RangeZoomOptions zoomOptions = RangeZoomOptions::Default)
        : min { min }
        , max { max }
        , clampOptions { clampOptions }
        , zoomOptions { zoomOptions }
    {
    }

    constexpr bool operator==(const Range&) const = default;
};

// Constant value for `[−∞,∞]`.
inline constexpr auto All = Range { -Range::infinity, Range::infinity };
inline constexpr auto AllUnzoomed = Range { -Range::infinity, Range::infinity, RangeClampOptions::Default, RangeZoomOptions::Unzoomed };

// Constant value for `[0,∞]`.
inline constexpr auto Nonnegative = Range { 0, Range::infinity };
inline constexpr auto NonnegativeUnzoomed = Range { 0, Range::infinity, RangeClampOptions::Default, RangeZoomOptions::Unzoomed };

// Constant value for `[1,∞]`.
inline constexpr auto Positive = Range { 1, Range::infinity };
inline constexpr auto PositiveUnzoomed = Range { 1, Range::infinity, RangeClampOptions::Default, RangeZoomOptions::Unzoomed };

// Constant value for `[0,1]`.
inline constexpr auto ClosedUnitRange = Range { 0, 1 };
inline constexpr auto ClosedUnitRangeUnzoomed = Range { 0, 1, RangeClampOptions::Default, RangeZoomOptions::Unzoomed };

// Constant value for `[0,1(clamp upper)]`.
inline constexpr auto ClosedUnitRangeClampUpper = Range { 0, 1, RangeClampOptions::ClampUpper };
inline constexpr auto ClosedUnitRangeClampUpperUnzoomed = Range { 0, 1, RangeClampOptions::ClampUpper, RangeZoomOptions::Unzoomed };

// Constant value for `[0,1(clamp both)]`.
inline constexpr auto ClosedUnitRangeClampBoth = Range { 0, 1, RangeClampOptions::ClampBoth };
inline constexpr auto ClosedUnitRangeClampBothUnzoomed = Range { 0, 1, RangeClampOptions::ClampBoth, RangeZoomOptions::Unzoomed };

// Constant value for `[0,100]`.
inline constexpr auto ClosedPercentageRange = Range { 0, 100 };
inline constexpr auto ClosedPercentageRangeUnzoomed = Range { 0, 100, RangeClampOptions::Default, RangeZoomOptions::Unzoomed };

// Constant value for `[0,100(clamp upper)]`.
inline constexpr auto ClosedPercentageRangeClampUpper = Range { 0, 100, RangeClampOptions::ClampUpper };
inline constexpr auto ClosedPercentageRangeClampUpperUnzoomed = Range { 0, 100, RangeClampOptions::ClampUpper, RangeZoomOptions::Unzoomed };

// Clamps a floating point value to within `range`.
template<Range range, std::floating_point T, typename U> constexpr T clampToRange(U value)
{
    return clampTo<T>(
        value,
        std::max<T>(range.min, -std::numeric_limits<T>::max()),
        std::min<T>(range.max,  std::numeric_limits<T>::max())
    );
}

// Clamps a floating point value to within `range` and within additional provided range.
template<Range range, std::floating_point T, typename U> constexpr T clampToRange(U value, T additionalMinimum, T additionalMaximum)
{
    return clampTo<T>(
        value,
        std::max<T>(std::max<T>(range.min, -std::numeric_limits<T>::max()), additionalMinimum),
        std::min<T>(std::min<T>(range.max,  std::numeric_limits<T>::max()), additionalMaximum)
    );
}

// Clamps an unsigned integral value to within `range`.
template<Range range, std::unsigned_integral T, typename U> constexpr T clampToRange(U value)
{
    static_assert(range.min >= 0);

    if constexpr (range.max == Range::infinity) {
        return clampTo<T>(
            value,
            range.min,
            std::numeric_limits<T>::max()
        );
    } else {
        return clampTo<T>(
            value,
            range.min,
            std::min<T>(range.max,  std::numeric_limits<T>::max())
        );
    }
}

// Clamps a signed integral value to within `range`.
template<Range range, std::signed_integral T, typename U> constexpr T clampToRange(U value)
{
    if constexpr (range.min == -Range::infinity && range.max == Range::infinity) {
        return clampTo<T>(
            value,
            std::numeric_limits<T>::min(),
            std::numeric_limits<T>::max()
        );
    } else if constexpr (range.min == -Range::infinity) {
        return clampTo<T>(
            value,
            std::numeric_limits<T>::min(),
            std::min<T>(range.max, std::numeric_limits<T>::max())
        );
    } else if constexpr (range.max == Range::infinity) {
        return clampTo<T>(
            value,
            std::max<T>(range.min, std::numeric_limits<T>::min()),
            std::numeric_limits<T>::max()
        );
    } else {
        return clampTo<T>(
            value,
            std::max<T>(range.min, std::numeric_limits<T>::min()),
            std::min<T>(range.max, std::numeric_limits<T>::max())
        );
    }
}

// Checks if a floating point value is within `range`.
template<Range range, std::floating_point T> constexpr bool isWithinRange(T value)
{
    return !std::isnan(value)
        && value >= std::max<T>(range.min, -std::numeric_limits<T>::max())
        && value <= std::min<T>(range.max,  std::numeric_limits<T>::max());
}

// Checks if a signed integral value is within `range`.
template<Range range, std::signed_integral T> constexpr bool isWithinRange(T value)
{
    if constexpr (range.min == -Range::infinity && range.max == Range::infinity) {
        return value >= std::numeric_limits<T>::min()
            && value <= std::numeric_limits<T>::max();
    } else if constexpr (range.min == -Range::infinity) {
        return value >= std::numeric_limits<T>::min()
            && value <= std::min<T>(range.max, std::numeric_limits<T>::max());
    } else if constexpr (range.max == Range::infinity) {
        return value >= std::max<T>(range.min, std::numeric_limits<T>::min())
            && value <= std::numeric_limits<T>::max();
    } else {
        return value >= std::max<T>(range.min, std::numeric_limits<T>::min())
            && value <= std::min<T>(range.max, std::numeric_limits<T>::max());
    }
}

// Checks if an unsigned integral value is within `range`.
template<Range range, std::unsigned_integral T> constexpr bool isWithinRange(T value)
{
    static_assert(range.min >= 0);

    if constexpr (range.max == Range::infinity) {
        return value >= std::max<T>(range.min, std::numeric_limits<T>::min())
            && value <= std::numeric_limits<T>::max();
    } else {
        return value >= std::max<T>(range.min, std::numeric_limits<T>::min())
            && value <= std::min<T>(range.max, std::numeric_limits<T>::max());
    }
}

} // namespace CSS
} // namespace WebCore
