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

#include "config.h"
#include "CalcExpressionOperation.h"

#include <cmath>
#include <wtf/text/TextStream.h>

namespace WebCore {

static std::pair<double, double> getNearestMultiples(double a, double b)
{
    auto absB = std::abs(b);
    double lowerB = std::floor(a / absB) * absB;
    double upperB = lowerB + absB;
    return std::make_pair(lowerB, upperB);
}

float CalcExpressionOperation::evaluate(float maxValue) const
{
    switch (m_operator) {
    case CalcOperator::Add: {
        float sum = 0;
        for (auto& child : m_children)
            sum += child->evaluate(maxValue);
        return sum;
    }
    case CalcOperator::Subtract: {
        // FIXME
        ASSERT(m_children.size() == 2);
        float left = m_children[0]->evaluate(maxValue);
        float right = m_children[1]->evaluate(maxValue);
        return left - right;
    }
    case CalcOperator::Multiply: {
        float product = 1;
        for (auto& child : m_children)
            product *= child->evaluate(maxValue);
        return product;
    }
    case CalcOperator::Divide: {
        // FIXME
        ASSERT(m_children.size() == 1 || m_children.size() == 2);
        if (m_children.size() == 1)
            return std::numeric_limits<float>::quiet_NaN();
        float left = m_children[0]->evaluate(maxValue);
        float right = m_children[1]->evaluate(maxValue);
        return left / right;
    }
    case CalcOperator::Min: {
        if (m_children.isEmpty())
            return std::numeric_limits<float>::quiet_NaN();
        float minimum = m_children[0]->evaluate(maxValue);
        for (auto& child : m_children)
            minimum = std::min(minimum, child->evaluate(maxValue));
        return minimum;
    }
    case CalcOperator::Max: {
        if (m_children.isEmpty())
            return std::numeric_limits<float>::quiet_NaN();
        float maximum = m_children[0]->evaluate(maxValue);
        for (auto& child : m_children)
            maximum = std::max(maximum, child->evaluate(maxValue));
        return maximum;
    }
    case CalcOperator::Clamp: {
        if (m_children.size() != 3)
            return std::numeric_limits<float>::quiet_NaN();

        float min = m_children[0]->evaluate(maxValue);
        float value = m_children[1]->evaluate(maxValue);
        float max = m_children[2]->evaluate(maxValue);
        return std::max(min, std::min(value, max));
    }
    case CalcOperator::Pow: {
        if (m_children.size() != 2)
            return std::numeric_limits<float>::quiet_NaN();
        float base = m_children[0]->evaluate(maxValue);
        float power = m_children[1]->evaluate(maxValue);
        return std::pow(base, power);
    }
    case CalcOperator::Sqrt: {
        if (m_children.size() != 1)
            return std::numeric_limits<float>::quiet_NaN();
        return std::sqrt(m_children[0]->evaluate(maxValue));
    }
    case CalcOperator::Hypot: {
        if (m_children.isEmpty())
            return std::numeric_limits<float>::quiet_NaN();
        if (m_children.size() == 1)
            return std::abs(m_children[0]->evaluate(maxValue));
        float sum = 0;
        for (auto& child : m_children) {
            float value = child->evaluate(maxValue);
            sum += (value * value);
        }
        return std::sqrt(sum);
    }
    case CalcOperator::Sin: {
        if (m_children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return std::sin(m_children[0]->evaluate(maxValue));
    }
    case CalcOperator::Cos: {
        if (m_children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return std::cos(m_children[0]->evaluate(maxValue));
    }
    case CalcOperator::Tan: {
        if (m_children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        return std::tan(m_children[0]->evaluate(maxValue));
    }
    case CalcOperator::Log: {
        if (m_children.size() != 1 && m_children.size() != 2)
            return std::numeric_limits<float>::quiet_NaN();
        if (m_children.size() == 1)
            return std::log(m_children[0]->evaluate(maxValue));
        return std::log(m_children[0]->evaluate(maxValue)) / std::log(m_children[1]->evaluate(maxValue));
    }
    case CalcOperator::Exp: {
        if (m_children.size() != 1)
            return std::numeric_limits<float>::quiet_NaN();
        return std::exp(m_children[0]->evaluate(maxValue));
    }
    case CalcOperator::Asin: {
        if (m_children.size() != 1)
            return std::numeric_limits<float>::quiet_NaN();
        return rad2deg(std::asin(m_children[0]->evaluate(maxValue)));
    }
    case CalcOperator::Acos: {
        if (m_children.size() != 1)
            return std::numeric_limits<float>::quiet_NaN();
        return rad2deg(std::acos(m_children[0]->evaluate(maxValue)));
    }
    case CalcOperator::Atan: {
        if (m_children.size() != 1)
            return std::numeric_limits<float>::quiet_NaN();
        return rad2deg(std::atan(m_children[0]->evaluate(maxValue)));
    }
    case CalcOperator::Atan2: {
        if (m_children.size() != 2)
            return std::numeric_limits<float>::quiet_NaN();
        return rad2deg(atan2(m_children[0]->evaluate(maxValue), m_children[1]->evaluate(maxValue)));
    }
    case CalcOperator::Abs: {
        if (m_children.size() != 1)
            return std::numeric_limits<float>::quiet_NaN();
        return std::abs(m_children[0]->evaluate(maxValue));
    }
    case CalcOperator::Sign: {
        if (m_children.size() != 1)
            return std::numeric_limits<double>::quiet_NaN();
        if (m_children[0]->evaluate(maxValue) > 0)
            return 1;
        if (m_children[0]->evaluate(maxValue) < 0)
            return -1;
        return m_children[0]->evaluate(maxValue);
    }
    case CalcOperator::Mod: {
        if (m_children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        float left = m_children[0]->evaluate(maxValue);
        float right = m_children[1]->evaluate(maxValue);
        if (!right)
            return std::numeric_limits<double>::quiet_NaN();
        if ((left < 0) == (right < 0))
            return std::fmod(left, right);
        return std::remainder(left, right);
    }
    case CalcOperator::Rem: {
        if (m_children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        float left = m_children[0]->evaluate(maxValue);
        float right = m_children[1]->evaluate(maxValue);
        if (!right)
            return std::numeric_limits<double>::quiet_NaN();
        return std::fmod(left, right);
    }
    case CalcOperator::Round:
        return std::numeric_limits<double>::quiet_NaN();
    case CalcOperator::Up: {
        if (m_children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto ret = getNearestMultiples(m_children[0]->evaluate(maxValue), m_children[1]->evaluate(maxValue));
        return ret.second;
    }
    case CalcOperator::Down: {
        if (m_children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto ret = getNearestMultiples(m_children[0]->evaluate(maxValue), m_children[1]->evaluate(maxValue));
        return ret.first;
    }
    case CalcOperator::Nearest: {
        if (m_children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto a = m_children[0]->evaluate(maxValue);
        auto b = m_children[1]->evaluate(maxValue);
        auto ret = getNearestMultiples(a, b);
        auto upperB = ret.second;
        auto lowerB = ret.first;
        return std::abs(upperB - a) <= std::abs(b) / 2 ? upperB : lowerB;
    }
    case CalcOperator::ToZero: {
        if (m_children.size() != 2)
            return std::numeric_limits<double>::quiet_NaN();
        auto ret = getNearestMultiples(m_children[0]->evaluate(maxValue), m_children[1]->evaluate(maxValue));
        auto upperB = ret.second;
        auto lowerB = ret.first;
        return std::abs(upperB) < std::abs(lowerB) ? upperB : lowerB;
    }
    }
    ASSERT_NOT_REACHED();
    return std::numeric_limits<float>::quiet_NaN();
}

bool CalcExpressionOperation::operator==(const CalcExpressionNode& other) const
{
    return is<CalcExpressionOperation>(other) && *this == downcast<CalcExpressionOperation>(other);
}

bool operator==(const CalcExpressionOperation& a, const CalcExpressionOperation& b)
{
    if (a.getOperator() != b.getOperator() || a.destinationCategory() != b.destinationCategory())
        return false;
    if (a.children().size() != b.children().size())
        return false;
    for (unsigned i = 0; i < a.children().size(); ++i) {
        if (!(*a.children()[i] == *b.children()[i]))
            return false;
    }
    return true;
}

void CalcExpressionOperation::dump(TextStream& ts) const
{
    if (m_operator == CalcOperator::Min || m_operator == CalcOperator::Max) {
        ts << m_operator << "(";
        size_t childrenCount = m_children.size();
        for (size_t i = 0; i < childrenCount; i++) {
            ts << m_children[i].get();
            if (i < childrenCount - 1)
                ts << ", ";
        }
        ts << ")";
    } else
        ts << m_children[0].get() << " " << m_operator << " " << m_children[1].get();
}

}
