/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "CalculationTree.h"
#include <numeric>
#include <wtf/Forward.h>
#include <wtf/MathExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {
namespace Calculation {

// This file contains an implementation of math operations used by Calculation::Tree and CSSCalc::Tree.

template<typename> struct OperatorExecutor;

template<typename Op, typename... Args> inline auto executeOperation(Args&&... args)
{
    return OperatorExecutor<Op>()(std::forward<Args>(args)...);
}

template<typename Container, typename Transform> class SizedTransformRange {
public:
    SizedTransformRange(const Container& container, Transform&& transform)
        : m_container(container)
        , m_transform(WTFMove(transform))
    {
    }

    struct const_iterator {
        const_iterator& operator++() { ++it; return *this; }
        auto operator*() const { return transform(*it); }

        bool operator==(const const_iterator& other) const { return it == other.it; }

        typename Container::const_iterator it;
        const Transform& transform;
    };

    using value_type = decltype(*std::declval<const_iterator>());

    auto size() const -> decltype(std::declval<Container>().size()) { return m_container.size(); }
    bool isEmpty() const { return m_container.isEmpty(); }

    const_iterator begin() const { return const_iterator { m_container.begin(), m_transform }; }
    const_iterator end() const { return const_iterator { m_container.end(), m_transform }; }

private:
    const Container& m_container;
    Transform m_transform;
};

template<typename T, std::invocable<const T&> Function> decltype(auto) makeSizedTransformRange(const Vector<T>& vector, Function&& evaluate)
{
    return SizedTransformRange { vector, std::forward<Function>(evaluate) };
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

template<> struct OperatorExecutor<Sum> {
    template<typename Range> double operator()(Range&& range)
    {
        double sum = 0;
        for (double value : range)
            sum += value;
        return sum;
    }

    template<typename T, std::invocable<const T&> Functor> double operator()(const Vector<T>& range, Functor&& functor)
    {
        return executeOperation<Sum>(makeSizedTransformRange(range, std::forward<Functor>(functor)));
    }

    double operator()(double a, double b)
    {
        return a + b;
    }
};

template<> struct OperatorExecutor<Negate> {
    double operator()(double a)
    {
        return -a;
    }
};

template<> struct OperatorExecutor<Product> {
    template<typename Range> double operator()(Range&& range)
    {
        double product = 1;
        for (double value : range)
            product *= value;
        return product;
    }

    template<typename T, std::invocable<const T&> Functor> double operator()(const Vector<T>& range, Functor&& functor)
    {
        return executeOperation<Product>(makeSizedTransformRange(range, std::forward<Functor>(functor)));
    }

    double operator()(double a, double b)
    {
        return a * b;
    }
};

template<> struct OperatorExecutor<Invert> {
    double operator()(double a)
    {
        return 1.0 / a;
    }
};

template<> struct OperatorExecutor<Min> {
    // NOTE: std::floating_point and related concepts can be used for `min`, as there is no precision loss from the operation staying in a lower precision.

    template<FloatingPointRange R> auto operator()(R&& range)
    {
        if (range.isEmpty())
            return std::numeric_limits<typename R::value_type>::quiet_NaN();

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
        return executeOperation<Min>(makeSizedTransformRange(range, std::forward<Functor>(functor)));
    }
};

template<> struct OperatorExecutor<Max> {
    // NOTE: std::floating_point and related concepts can be used for `max`, as there is no precision loss from the operation staying in a lower precision.

    template<FloatingPointRange R> auto operator()(R&& range)
    {
        if (range.isEmpty())
            return std::numeric_limits<typename R::value_type>::quiet_NaN();

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
        return executeOperation<Max>(makeSizedTransformRange(range, std::forward<Functor>(functor)));
    }
};

template<> struct OperatorExecutor<Clamp> {
    // NOTE: std::floating_point and related concepts can be used for `clamp`, as there is no precision loss from the operation staying in a lower precision.

    template<std::floating_point T> T operator()(T min, T val, T max)
    {
        if (std::isnan(min) || std::isnan(val) || std::isnan(max))
            return std::numeric_limits<T>::quiet_NaN();
        return std::max(min, std::min(val, max));
    }

    template<std::floating_point T> T operator()(std::variant<T, None> min, T val, std::variant<T, None> max)
    {
        bool minIsNone = std::holds_alternative<None>(min);
        bool maxIsNone = std::holds_alternative<None>(max);

        // - clamp(none, VAL, none) is equivalent to just calc(VAL).
        if (minIsNone && maxIsNone)
            return val;

        // - clamp(none, VAL, MAX) is equivalent to min(VAL, MAX)
        if (minIsNone)
            return executeOperation<Min>(val, std::get<T>(max));

        // - clamp(MIN, VAL, none) is equivalent to max(MIN, VAL)
        if (maxIsNone)
            return executeOperation<Max>(std::get<T>(min), val);

        return executeOperation<Clamp>(std::get<T>(min), val, std::get<T>(max));
    }
};

template<> struct OperatorExecutor<RoundNearest> {
    double operator()(double valueToRound, double roundingInterval)
    {
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval))
            return std::signbit(valueToRound) ? -0.0 : +0.0;
        auto [lower, upper] = getNearestMultiples(valueToRound, roundingInterval);
        return std::abs(upper - valueToRound) <= std::abs(roundingInterval) / 2 ? upper : lower;
    }

    double operator()(double valueToRound, std::optional<double> roundingInterval)
    {
        return executeOperation<RoundNearest>(valueToRound, roundingInterval.value_or(1.0));
    }
};

template<> struct OperatorExecutor<RoundUp> {
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
        return executeOperation<RoundUp>(valueToRound, roundingInterval.value_or(1.0));
    }
};

template<> struct OperatorExecutor<RoundDown> {
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
        return executeOperation<RoundDown>(valueToRound, roundingInterval.value_or(1.0));
    }
};

template<> struct OperatorExecutor<RoundToZero> {
    double operator()(double valueToRound, double roundingInterval)
    {
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval))
            return std::signbit(valueToRound) ? -0.0 : +0.0;
        auto [lower, upper] = getNearestMultiples(valueToRound, roundingInterval);
        return std::abs(upper) < std::abs(lower) ? upper : lower;
    }

    double operator()(double valueToRound, std::optional<double> roundingInterval)
    {
        return executeOperation<RoundToZero>(valueToRound, roundingInterval.value_or(1.0));
    }
};

template<> struct OperatorExecutor<Mod> {
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

template<> struct OperatorExecutor<Rem> {
    double operator()(double a, double b)
    {
        if (!b)
            return std::numeric_limits<double>::quiet_NaN();
        return std::fmod(a, b);
    }
};

template<> struct OperatorExecutor<Sin> {
    double operator()(double a)
    {
        return std::sin(a);
    }
};

template<> struct OperatorExecutor<Cos> {
    double operator()(double a)
    {
        return std::cos(a);
    }
};

template<> struct OperatorExecutor<Tan> {
    double operator()(double a)
    {
        double x = std::fmod(a, piDouble * 2);
        // std::fmod can return negative values.
        x = x < 0 ? piDouble * 2 + x : x;
        ASSERT(!(x < 0));
        ASSERT(!(x > piDouble * 2));
        if (x == piOverTwoDouble)
            return std::numeric_limits<double>::infinity();
        if (x == 3 * piOverTwoDouble)
            return -std::numeric_limits<double>::infinity();
        return std::tan(x);
    }
};

template<> struct OperatorExecutor<Asin> {
    double operator()(double a)
    {
        return rad2deg(std::asin(a));
    }
};

template<> struct OperatorExecutor<Acos> {
    double operator()(double a)
    {
        return rad2deg(std::acos(a));
    }
};

template<> struct OperatorExecutor<Atan> {
    double operator()(double a)
    {
        return rad2deg(std::atan(a));
    }
};

template<> struct OperatorExecutor<Atan2> {
    double operator()(double a, double b)
    {
        return rad2deg(atan2(a, b));
    }
};

template<> struct OperatorExecutor<Pow> {
    double operator()(double a, double b)
    {
        return std::pow(a, b);
    }
};

template<> struct OperatorExecutor<Sqrt> {
    double operator()(double a)
    {
        return std::sqrt(a);
    }
};

template<> struct OperatorExecutor<Hypot> {
    template<typename Range> double operator()(Range&& range)
    {
        if (range.isEmpty())
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
        return executeOperation<Hypot>(makeSizedTransformRange(range, std::forward<Functor>(functor)));
    }
};

template<> struct OperatorExecutor<Log> {
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
            return executeOperation<Log>(a, *b);
        return executeOperation<Log>(a);
    }
};

template<> struct OperatorExecutor<Exp> {
    double operator()(double a)
    {
        return std::exp(a);
    }
};

template<> struct OperatorExecutor<Abs> {
    double operator()(double a)
    {
        return std::abs(a);
    }
};

template<> struct OperatorExecutor<Sign> {
    double operator()(double a)
    {
        if (a > 0)
            return 1;
        if (a < 0)
            return -1;
        return a;
    }
};

template<> struct OperatorExecutor<Progress> {
    double operator()(double progress, double from, double to)
    {
        // (progress value - start value) / (end value - start value)
        return (progress - from) / (to - from);
    }
};

} // namespace Calculation
} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
