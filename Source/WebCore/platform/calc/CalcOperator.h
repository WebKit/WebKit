/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>
#include <wtf/MathExtras.h>

namespace WebCore {

// Don't change these values; parsing uses them.
enum class CalcOperator : uint8_t {
    Add = '+',
    Subtract = '-',
    Multiply = '*',
    Divide = '/',
    Min = 0,
    Max,
    Clamp,
    Pow,
    Sqrt,
    Hypot,
    Sin,
    Cos,
    Tan,
    Exp,
    Log,
    Asin,
    Acos,
    Atan,
    Atan2,
    Abs,
    Sign,
    Mod,
    Rem,
    Round,
    Nearest,
    Up,
    Down,
    ToZero,
};

TextStream& operator<<(TextStream&, CalcOperator);

template <typename T, typename Function>
double evaluateCalcExpression(CalcOperator calcOperator, const Vector<T>& children, Function&& evaluate)
{
    auto getNearestMultiples = [](double a, double b) -> std::pair<double, double> {
        if (!std::fmod(a, b))
            return { a, a };
        double lower = std::floor(a / std::abs(b)) * std::abs(b);
        double upper = lower + std::abs(b);
        return { lower, upper };
    };

    switch (calcOperator) {
    case CalcOperator::Add: {
        double sum = 0;
        for (auto& child : children)
            sum += evaluate(child);
        return sum;
    }
    case CalcOperator::Subtract:
        ASSERT(children.size() == 2);
        return evaluate(children[0]) - evaluate(children[1]);
    case CalcOperator::Multiply: {
        double product = 1;
        for (auto& child : children)
            product *= evaluate(child);
        return product;
    }
    case CalcOperator::Divide:
        ASSERT(children.size() == 1 || children.size() == 2);
        if (children.size() == 1)
            return std::numeric_limits<double>::quiet_NaN();
        return evaluate(children[0]) / evaluate(children[1]);
    case CalcOperator::Min: {
        if (children.isEmpty())
            return std::numeric_limits<double>::quiet_NaN();
        auto minimum = evaluate(children[0]);
        for (auto& child : children) {
            auto value = evaluate(child);
            if (std::isnan(value))
                return value;
            minimum = std::min(minimum, value);
        }
        return minimum;
    }
    case CalcOperator::Max: {
        if (children.isEmpty())
            return std::numeric_limits<double>::quiet_NaN();
        auto maximum = evaluate(children[0]);
        for (auto& child : children) {
            auto value = evaluate(child);
            if (std::isnan(value))
                return value;
            maximum = std::max(maximum, value);
        }
        return maximum;
    }
    case CalcOperator::Clamp: {
        if (children.size() != 3)
            return std::numeric_limits<double>::quiet_NaN();
        double min = evaluate(children[0]);
        double value = evaluate(children[1]);
        double max = evaluate(children[2]);
        if (std::isnan(min) || std::isnan(value) || std::isnan(max))
            return std::numeric_limits<double>::quiet_NaN();
        return std::max(min, std::min(value, max));
    }
    case CalcOperator::Pow:
        if (children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        return std::pow(evaluate(children[0]), evaluate(children[1]));
    case CalcOperator::Sqrt: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return std::sqrt(evaluate(children[0]));
    }
    case CalcOperator::Hypot: {
        if (children.isEmpty())
            return std::numeric_limits<double>::quiet_NaN();
        if (children.size() == 1)
            return std::abs(evaluate(children[0]));
        double sum = 0;
        for (auto& child : children) {
            auto value = evaluate(child);
            sum += (value * value);
        }
        return std::sqrt(sum);
    }
    case CalcOperator::Sin: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return std::sin(evaluate(children[0]));
    }
    case CalcOperator::Cos: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return std::cos(evaluate(children[0]));
    }
    case CalcOperator::Tan: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        double x = std::fmod(evaluate(children[0]), piDouble * 2);
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
    case CalcOperator::Log: {
        if (children.size() != 1 && children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        if (children.size() == 1)
            return std::log(evaluate(children[0]));
        return std::log(evaluate(children[0])) / std::log(evaluate(children[1]));
    }
    case CalcOperator::Exp: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return std::exp(evaluate(children[0]));
    }
    case CalcOperator::Asin: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return rad2deg(std::asin(evaluate(children[0])));
    }
    case CalcOperator::Acos: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return rad2deg(std::acos(evaluate(children[0])));
    }
    case CalcOperator::Atan: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return rad2deg(std::atan(evaluate(children[0])));
    }
    case CalcOperator::Atan2: {
        if (children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        return rad2deg(atan2(evaluate(children[0]), evaluate(children[1])));
    }
    case CalcOperator::Abs: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return std::abs(evaluate(children[0]));
    }
    case CalcOperator::Sign: {
        if (children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        auto value = evaluate(children[0]);
        if (value > 0)
            return 1;
        if (value < 0)
            return -1;
        return value;
    }
    case CalcOperator::Mod: {
        if (children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto left = evaluate(children[0]);
        auto right = evaluate(children[1]);
        // In mod(A, B) only, if B is infinite and A has opposite sign to B
        // (including an oppositely-signed zero), the result is NaN.
        // https://drafts.csswg.org/css-values/#round-infinities
        if (std::isinf(right) && std::signbit(left) != std::signbit(right))
            return std::numeric_limits<double>::quiet_NaN();
        auto result = std::fmod(left, right);
        // If the result is on opposite side of zero from B,
        // put it between 0 and B.
        // https://drafts.csswg.org/css-values/#round-func
        if (std::signbit(result) != std::signbit(right))
            result += right;
        return result;
    }
    case CalcOperator::Rem: {
        if (children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto left = evaluate(children[0]);
        auto right = evaluate(children[1]);
        if (!right)
            return std::numeric_limits<double>::quiet_NaN();
        return std::fmod(left, right);
    }
    case CalcOperator::Round:
        return std::numeric_limits<double>::quiet_NaN();
    case CalcOperator::Up: {
        if (children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto valueToRound = evaluate(children[0]);
        auto roundingInterval = evaluate(children[1]);
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval)) {
            if (!valueToRound)
                return valueToRound;
            return std::signbit(valueToRound) ? -0.0 : std::numeric_limits<double>::infinity();
        }
        return getNearestMultiples(valueToRound, roundingInterval).second;
    }
    case CalcOperator::Down: {
        if (children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto valueToRound = evaluate(children[0]);
        auto roundingInterval = evaluate(children[1]);
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval)) {
            if (!valueToRound)
                return valueToRound;
            return std::signbit(valueToRound) ? -std::numeric_limits<double>::infinity() : +0.0;
        }
        return getNearestMultiples(valueToRound, roundingInterval).first;
    }
    case CalcOperator::Nearest: {
        if (children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto valueToRound = evaluate(children[0]);
        auto roundingInterval = evaluate(children[1]);
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval))
            return std::signbit(valueToRound) ? -0.0 : +0.0;
        auto [lower, upper] = getNearestMultiples(valueToRound, roundingInterval);
        return std::abs(upper - valueToRound) <= std::abs(roundingInterval) / 2 ? upper : lower;
    }
    case CalcOperator::ToZero: {
        if (children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto valueToRound = evaluate(children[0]);
        auto roundingInterval = evaluate(children[1]);
        if (!std::isinf(valueToRound) && std::isinf(roundingInterval))
            return std::signbit(valueToRound) ? -0.0 : +0.0;
        auto [lower, upper] = getNearestMultiples(valueToRound, roundingInterval);
        return std::abs(upper) < std::abs(lower) ? upper : lower;
    }
    }
    ASSERT_NOT_REACHED();
    return 0;
}

}
