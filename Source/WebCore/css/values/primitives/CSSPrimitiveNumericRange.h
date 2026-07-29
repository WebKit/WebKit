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

// Used to indicate how one of the bounds (upper or lower) of range should be interpreted
// at parse time.
enum class RangeParseTimeBehavior {
    // `Default` indicates that at parse time, out of range values invalidate the parse.
    Default,

    // `Clamp` indicates that parse time, out of range values should clamp instead
    // of invalidating the parse.
    Clamp,

    // `Ignore` indicates that parse time, out of range values should be ignored,
    // allowing them as if they were in range.
    Ignore,
};

// Representation for `CSS bracketed range notation`. Represents a closed range between (and including) `min` and `max`.
// https://drafts.csswg.org/css-values-4/#numeric-ranges
struct Range {
    // Convenience to allow for a shorter spelling of the appropriate infinity.
    static constexpr auto infinity = std::numeric_limits<double>::infinity();

    double min { -infinity };
    double max {  infinity };
    RangeParseTimeBehavior minParseTimeBehavior { RangeParseTimeBehavior::Default };
    RangeParseTimeBehavior maxParseTimeBehavior { RangeParseTimeBehavior::Default };

    constexpr Range(double min, double max, RangeParseTimeBehavior minParseTimeBehavior = RangeParseTimeBehavior::Default, RangeParseTimeBehavior maxParseTimeBehavior = RangeParseTimeBehavior::Default)
        : min { min }
        , max { max }
        , minParseTimeBehavior { minParseTimeBehavior }
        , maxParseTimeBehavior { maxParseTimeBehavior }
    {
    }

    constexpr bool operator==(const Range&) const = default;
};

// Constant value for `[−∞,∞]`.
inline constexpr auto All = Range { -Range::infinity, Range::infinity };

// Constant value for `[0,∞]`.
inline constexpr auto Nonnegative = Range { 0, Range::infinity };

// Constant value for `[1,∞]`.
inline constexpr auto Positive = Range { 1, Range::infinity };

// Constant value for `[-∞,-1]`.
inline constexpr auto Negative = Range { -Range::infinity, -1 };

// Constant value for `[0,1]`.
inline constexpr auto ClosedUnitRange = Range { 0, 1 };

// Constant value for `[0,1(clamp upper)]`.
inline constexpr auto ClosedUnitRangeClampUpper = Range { 0, 1, RangeParseTimeBehavior::Default, RangeParseTimeBehavior::Clamp };

// Constant value for `[0,1(clamp both)]`.
inline constexpr auto ClosedUnitRangeClampBoth = Range { 0, 1, RangeParseTimeBehavior::Clamp, RangeParseTimeBehavior::Clamp };

// Constant value for `[0,1(ignore both)]`.
inline constexpr auto ClosedUnitRangeIgnoreBoth = Range { 0, 1, RangeParseTimeBehavior::Ignore, RangeParseTimeBehavior::Ignore };

// Constant value for `[0,100]`.
inline constexpr auto ClosedPercentageRange = Range { 0, 100 };

// Constant value for `[0,100(clamp upper)]`.
inline constexpr auto ClosedPercentageRangeClampUpper = Range { 0, 100, RangeParseTimeBehavior::Default, RangeParseTimeBehavior::Clamp };

// Constant value for `[0,100(clamp both)]`.
inline constexpr auto ClosedPercentageRangeClampBoth = Range { 0, 100, RangeParseTimeBehavior::Clamp, RangeParseTimeBehavior::Clamp };

// Constant value for `[0,100(ignore both)]`.
inline constexpr auto ClosedPercentageRangeIgnoreBoth = Range { 0, 100, RangeParseTimeBehavior::Ignore, RangeParseTimeBehavior::Ignore };

// Special Range constants that restrict down to what LayoutUnit supports.

// Max/min values for CSS, needs to be slightly smaller/larger than the true max/min values
// supported by LayoutUnit to allow for rounding without overflowing.
constexpr double minValueForCssLength = static_cast<double>((INT_MIN / (1 << 6)) + 2);
constexpr double maxValueForCssLength = static_cast<double>((INT_MAX / (1 << 6)) - 2);

// Constant value for `[0,∞]` limited to LayoutUnit restrictions.
inline constexpr auto AllLayoutUnitClamped = Range { minValueForCssLength, maxValueForCssLength, RangeParseTimeBehavior::Ignore, RangeParseTimeBehavior::Ignore };

// Constant value for `[0,∞]` limited to LayoutUnit restrictions.
inline constexpr auto NonnegativeLayoutUnitClamped = Range { 0, maxValueForCssLength, RangeParseTimeBehavior::Default, RangeParseTimeBehavior::Ignore };

// Returns whether range `a` is equal to or is a subrange of range `b`.
consteval bool isEqualOrSubrange(Range a, Range b)
{
    return a.min >= b.min && a.max <= b.max;
}

// Returns whether range `a` is a strict subrange of range `b`.
consteval bool isSubrange(Range a, Range b)
{
    return isEqualOrSubrange(a, b) && (a.min != b.min || a.max != b.max);
}

template<Range a, Range b> concept IsSubrange = isSubrange(a, b);

// Clamps a floating point value to within static `range`.
template<Range range, std::floating_point T, typename U> constexpr T clampToRange(U value)
{
    return clampTo<T>(
        value,
        std::max<T>(range.min, -std::numeric_limits<T>::max()),
        std::min<T>(range.max,  std::numeric_limits<T>::max())
    );
}

// Clamps a floating point value to within dynamic `range`.
template<std::floating_point T, typename U> constexpr T clampToRange(U value, Range range)
{
    return clampTo<T>(
        value,
        std::max<T>(range.min, -std::numeric_limits<T>::max()),
        std::min<T>(range.max,  std::numeric_limits<T>::max())
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

// Clamps a value to within `range` of the specified numeric type.
template<typename Numeric, Range range = Numeric::range, typename T = typename Numeric::ResolvedValueType, typename U> constexpr T clampToRangeOf(U value)
{
    return clampToRange<range, T, U>(value);
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
