/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSCalcOperator.h"
#include "CSSValueKeywords.h"
#include <numbers>
#include <numeric>
#include <ranges>
#include <wtf/Forward.h>
#include <wtf/MathExtras.h>

namespace WebCore {
namespace CSSCalc {

// This file contains an implementation of math operations used by CSSCalc::Tree and Style::Calculation::Tree.

template<auto> struct OperatorExecutor;

template<auto Op, typename... Args> inline auto executeOperation(Args&&... args)
{
    return OperatorExecutor<Op>()(std::forward<Args>(args)...);
}

// Helper for rounding functions.
inline std::pair<double, double> getNearestMultiples(double a, double b)
{
    if (!std::fmod(a, b))
        return { a, a };
    double lower = std::floor(a / std::abs(b)) * std::abs(b);
    double upper = lower + std::abs(b);
    return { lower, upper };
}

template<typename Range> concept FloatingPointRange = requires(Range range) {
    { *range.begin() } -> std::floating_point;
    { *range.end() } -> std::floating_point;
};

template<> struct OperatorExecutor<Operator::Sum> {
    template<typename Range> double operator()(Range&& range)
    {
        double sum = 0;
        for (double value : range)
            sum += value;
        return sum;
    }

    template<typename T, std::invocable<const T&> Functor> double operator()(const Vector<T>& range, Functor&& functor)
    {
        return executeOperation<Operator::Sum>(range | std::views::transform(std::forward<Functor>(functor)));
    }

    double operator()(double a, double b)
    {
        return a + b;
    }
};

template<> struct OperatorExecutor<Operator::Negate> {
    double operator()(double a)
    {
        return -a;
    }
};

template<> struct OperatorExecutor<Operator::Product> {
    template<typename Range> double operator()(Range&& range)
    {
        double product = 1;
        for (double value : range)
            product *= value;
        return product;
    }

    template<typename T, std::invocable<const T&> Functor> double operator()(const Vector<T>& range, Functor&& functor)
    {
        return executeOperation<Operator::Product>(range | std::views::transform(std::forward<Functor>(functor)));
    }

    double operator()(double a, double b)
    {
        return a * b;
    }
};

template<> struct OperatorExecutor<Operator::Invert> {
    double operator()(double a)
    {
        return 1.0 / a;
    }
};

template<> struct OperatorExecutor<Operator::Min> {
    // NOTE: std::floating_point and related concepts can be used for `min`, as there is no precision loss from the operation staying in a lower precision.

    template<FloatingPointRange R> auto operator()(R&& range)
    {
        if (range.empty())
            return std::numeric_limits<std::ranges::range_value_t<R>>::quiet_NaN();

        auto&& it = range.begin();
        auto&& end = range.end();

        auto minimum = *it;
        for (++it; it != end; ++it) {
            auto value = *it;
            if (std::isnan(value))
                return value;
            minimum = std::min(minimum, value);
        }
        return minimum;
    }

    template<std::floating_point T> T operator()(T val, T min)
    {
        if (std::isnan(val))
            return val;
        if (std::isnan(min))
            return min;
        return std::min(val, min);
    }

    template<typename T, std::invocable<const T&> Functor> double operator()(const Vector<T>& range, Functor&& functor)
    {
        return executeOperation<Operator::Min>(range | std::views::transform(std::forward<Functor>(functor)));
    }
};

template<> struct OperatorExecutor<Operator::Max> {
    // NOTE: std::floating_point and related concepts can be used for `max`, as there is no precision loss from the operation staying in a lower precision.

    template<FloatingPointRange R> auto operator()(R&& range)
    {
        if (range.empty())
            return std::numeric_limits<std::ranges::range_value_t<R>>::quiet_NaN();

        auto&& it = range.begin();
        auto&& end = range.end();

        auto maximum = *it;
        for (++it; it != end; ++it) {
            auto value = *it;
            if (std::isnan(value))
                return value;
            maximum = std::max(maximum, value);
        }
        return maximum;
    }

    template<std::floating_point T> T operator()(T val, T max)
    {
        if (std::isnan(val))
            return val;
        if (std::isnan(max))
            return max;
        return std::max(val, max);
    }

    template<typename T, std::invocable<const T&> Functor> double operator()(const Vector<T>& range, Functor&& functor)
    {
        return executeOperation<Operator::Max>(range | std::views::transform(std::forward<Functor>(functor)));
    }
};

template<> struct OperatorExecutor<Operator::Clamp> {
    // NOTE: std::floating_point and related concepts can be used for `clamp`, as there is no precision loss from the operation staying in a lower precision.

    template<std::floating_point T> T operator()(T min, T val, T max)
    {
        if (std::isnan(min) || std::isnan(val) || std::isnan(max))
            return std::numeric_limits<T>::quiet_NaN();
        return std::max(min, std::min(val, max));
    }

    template<std::floating_point T> T operator()(Variant<T, CSS::Keyword::None> min, T val, Variant<T, CSS::Keyword::None> max)
    {
        bool minIsNone = std::holds_alternative<CSS::Keyword::None>(min);
        bool maxIsNone = std::holds_alternative<CSS::Keyword::None>(max);

        // - clamp(none, VAL, none) is equivalent to just calc(VAL).
        if (minIsNone && maxIsNone)
            return val;

        // - clamp(none, VAL, MAX) is equivalent to min(VAL, MAX)
        if (minIsNone)
            return executeOperation<Operator::Min>(val, std::get<T>(max));

        // - clamp(MIN, VAL, none) is equivalent to max(MIN, VAL)
        if (maxIsNone)
            return executeOperation<Operator::Max>(std::get<T>(min), val);

        return executeOperation<Operator::Clamp>(std::get<T>(min), val, std::get<T>(max));
    }
};

template<> struct OperatorExecutor<Operator::RoundNearest> {
    double operator()(double valueToRound, double roundingInterval)
    {
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval))
            return std::signbit(valueToRound) ? -0.0 : +0.0;
        auto [lower, upper] = getNearestMultiples(valueToRound, roundingInterval);
        return std::abs(upper - valueToRound) <= std::abs(roundingInterval) / 2 ? upper : lower;
    }

    double operator()(double valueToRound, std::optional<double> roundingInterval)
    {
        return executeOperation<Operator::RoundNearest>(valueToRound, roundingInterval.value_or(1.0));
    }
};

template<> struct OperatorExecutor<Operator::RoundUp> {
    double operator()(double valueToRound, double roundingInterval)
    {
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval)) {
            if (!valueToRound)
                return valueToRound;
            return std::signbit(valueToRound) ? -0.0 : std::numeric_limits<double>::infinity();
        }
        return getNearestMultiples(valueToRound, roundingInterval).second;
    }

    double operator()(double valueToRound, std::optional<double> roundingInterval)
    {
        return executeOperation<Operator::RoundUp>(valueToRound, roundingInterval.value_or(1.0));
    }
};

template<> struct OperatorExecutor<Operator::RoundDown> {
    double operator()(double valueToRound, double roundingInterval)
    {
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval)) {
            if (!valueToRound)
                return valueToRound;
            return std::signbit(valueToRound) ? -std::numeric_limits<double>::infinity() : +0.0;
        }
        return getNearestMultiples(valueToRound, roundingInterval).first;
    }

    double operator()(double valueToRound, std::optional<double> roundingInterval)
    {
        return executeOperation<Operator::RoundDown>(valueToRound, roundingInterval.value_or(1.0));
    }
};

template<> struct OperatorExecutor<Operator::RoundToZero> {
    double operator()(double valueToRound, double roundingInterval)
    {
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval))
            return std::signbit(valueToRound) ? -0.0 : +0.0;
        auto [lower, upper] = getNearestMultiples(valueToRound, roundingInterval);
        return std::abs(upper) < std::abs(lower) ? upper : lower;
    }

    double operator()(double valueToRound, std::optional<double> roundingInterval)
    {
        return executeOperation<Operator::RoundToZero>(valueToRound, roundingInterval.value_or(1.0));
    }
};

template<> struct OperatorExecutor<Operator::Mod> {
    double operator()(double a, double b)
    {
        // In mod(A, B) only, if B is infinite and A has opposite sign to B
        // (including an oppositely-signed zero), the result is NaN.
        // https://drafts.csswg.org/css-values/#round-infinities
        if (std::isinf(b) && std::signbit(a) != std::signbit(b))
            return std::numeric_limits<double>::quiet_NaN();
        auto result = std::fmod(a, b);
        // If the result is on opposite side of zero from B,
        // put it between 0 and B.
        // https://drafts.csswg.org/css-values/#round-func
        if (std::signbit(result) != std::signbit(b))
            result += b;
        return result;
    }
};

template<> struct OperatorExecutor<Operator::Rem> {
    double operator()(double a, double b)
    {
        if (!b)
            return std::numeric_limits<double>::quiet_NaN();
        return std::fmod(a, b);
    }
};

template<> struct OperatorExecutor<Operator::Sin> {
    double operator()(double a)
    {
        return std::sin(a);
    }
};

template<> struct OperatorExecutor<Operator::Cos> {
    double operator()(double a)
    {
        return std::cos(a);
    }
};

template<> struct OperatorExecutor<Operator::Tan> {
    double operator()(double a)
    {
        double x = std::fmod(a, std::numbers::pi * 2);
        // std::fmod can return negative values.
        x = x < 0 ? std::numbers::pi * 2 + x : x;
        ASSERT(!(x < 0));
        ASSERT(!(x > std::numbers::pi * 2));
        if (x == piOverTwoDouble)
            return std::numeric_limits<double>::infinity();
        if (x == 3 * piOverTwoDouble)
            return -std::numeric_limits<double>::infinity();
        return std::tan(x);
    }
};

template<> struct OperatorExecutor<Operator::Asin> {
    double operator()(double a)
    {
        return rad2deg(std::asin(a));
    }
};

template<> struct OperatorExecutor<Operator::Acos> {
    double operator()(double a)
    {
        return rad2deg(std::acos(a));
    }
};

template<> struct OperatorExecutor<Operator::Atan> {
    double operator()(double a)
    {
        return rad2deg(std::atan(a));
    }
};

template<> struct OperatorExecutor<Operator::Atan2> {
    double operator()(double a, double b)
    {
        return rad2deg(atan2(a, b));
    }
};

template<> struct OperatorExecutor<Operator::Pow> {
    double operator()(double a, double b)
    {
        return std::pow(a, b);
    }
};

template<> struct OperatorExecutor<Operator::Sqrt> {
    double operator()(double a)
    {
        return std::sqrt(a);
    }
};

template<> struct OperatorExecutor<Operator::Hypot> {
    template<typename Range> double operator()(Range&& range)
    {
        if (range.empty())
            return std::numeric_limits<double>::quiet_NaN();
        if (range.size() == 1)
            return std::abs(*range.begin());
        double sum = 0;
        for (double value : range) {
            sum += (value * value);
        }
        return std::sqrt(sum);
    }

    template<typename T, std::invocable<const T&> Functor> double operator()(const Vector<T>& range, Functor&& functor)
    {
        return executeOperation<Operator::Hypot>(range | std::views::transform(std::forward<Functor>(functor)));
    }
};

template<> struct OperatorExecutor<Operator::Log> {
    double operator()(double a)
    {
        return std::log(a);
    }

    double operator()(double a, double b)
    {
        return std::log(a) / std::log(b);
    }

    double operator()(double a, std::optional<double> b)
    {
        if (b)
            return executeOperation<Operator::Log>(a, *b);
        return executeOperation<Operator::Log>(a);
    }
};

template<> struct OperatorExecutor<Operator::Exp> {
    double operator()(double a)
    {
        return std::exp(a);
    }
};

template<> struct OperatorExecutor<Operator::Abs> {
    double operator()(double a)
    {
        return std::abs(a);
    }
};

template<> struct OperatorExecutor<Operator::Sign> {
    double operator()(double a)
    {
        if (a > 0)
            return 1;
        if (a < 0)
            return -1;
        return a;
    }
};

template<> struct OperatorExecutor<Operator::Progress> {
    double operator()(double progress, double from, double to)
    {
        // (progress value - start value) / (end value - start value)
        return executeOperation<Operator::Clamp>(0.0, (progress - from) / (to - from), 1.0);
    }
};

template<> struct OperatorExecutor<Operator::Random> {
    double operator()(double randomBaseValue, double min, double max, std::optional<double> step)
    {
        if (std::isnan(min) || std::isnan(max))
            return std::numeric_limits<double>::quiet_NaN();

        if (std::isinf(min))
            return min;

        // If the maximum value is less than the minimum value, it behaves as if it’s equal to the minimum value.
        if (max < min)
            max = min;

        auto range = max - min;
        if (std::isinf(range))
            return std::numeric_limits<double>::quiet_NaN();

        if (!step)
            return min + randomBaseValue * range;

        if (std::isnan(*step))
            return std::numeric_limits<double>::quiet_NaN();

        if (*step <= 0)
            return min + randomBaseValue * range;

        // Let epsilon be step / 1000, or the smallest representable value greater than zero in the numeric type being used if epsilon would round to zero.
        auto epsilon = *step / 1000.0;

        // Let N be the largest integer such that min + N * step is less than or equal to max
        auto N = std::floor(range / *step);
        if (std::isinf(N))
            return min + randomBaseValue * range;

        // If N produces a value that is not within epsilon of max, but N+1 would produce a value within epsilon of max, set N to N+1.
        auto distanceToMax = max - (min + (N * *step));
        if (std::abs(distanceToMax) > epsilon) {
            auto distanceToMaxPlus1 = max - (min + ((N + 1) * *step));
            if (std::abs(distanceToMaxPlus1) < epsilon)
                N = N + 1;
        }

        // Let step index be a random integer less than N+1, given R.
        auto stepIndex = OperatorExecutor<Operator::RoundDown>{}(randomBaseValue * (N + 1.0), 1.0);

        // Let value be min + step index * step.
        auto value = min + stepIndex * *step;

        // If step index is N and value is within epsilon of max, return max
        if (stepIndex == N && std::abs(max - value) < epsilon)
            return max;

        // Otherwise, return value.
        return value;
    }
};

} // namespace CSSCalc
} // namespace WebCore
