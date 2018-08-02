/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CalculationValue.h"

#include "LengthFunctions.h"
#include <limits>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<CalculationValue> CalculationValue::create(std::unique_ptr<CalcExpressionNode> value, ValueRange range)
{
    return adoptRef(*new CalculationValue(WTFMove(value), range));
}

float CalcExpressionNumber::evaluate(float) const
{
    return m_value;
}

void CalcExpressionNumber::dump(TextStream& ts) const
{
    ts << TextStream::FormatNumberRespectingIntegers(m_value);
}

bool CalcExpressionNumber::operator==(const CalcExpressionNode& other) const
{
    return other.type() == CalcExpressionNodeType::Number && *this == toCalcExpressionNumber(other);
}

float CalculationValue::evaluate(float maxValue) const
{
    float result = m_expression->evaluate(maxValue);
    // FIXME: This test was originally needed when we did not detect division by zero at parse time.
    // It's possible that this is now unneeded code and can be removed.
    if (std::isnan(result))
        return 0;
    return m_shouldClampToNonNegative && result < 0 ? 0 : result;
}

float CalcExpressionOperation::evaluate(float maxValue) const
{
    switch (m_operator) {
    case CalcOperator::Add: {
        ASSERT(m_children.size() == 2);
        float left = m_children[0]->evaluate(maxValue);
        float right = m_children[1]->evaluate(maxValue);
        return left + right;
    }
    case CalcOperator::Subtract: {
        ASSERT(m_children.size() == 2);
        float left = m_children[0]->evaluate(maxValue);
        float right = m_children[1]->evaluate(maxValue);
        return left - right;
    }
    case CalcOperator::Multiply: {
        ASSERT(m_children.size() == 2);
        float left = m_children[0]->evaluate(maxValue);
        float right = m_children[1]->evaluate(maxValue);
        return left * right;
    }
    case CalcOperator::Divide: {
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
    }
    ASSERT_NOT_REACHED();
    return std::numeric_limits<float>::quiet_NaN();
}

bool CalcExpressionOperation::operator==(const CalcExpressionNode& other) const
{
    return other.type() == CalcExpressionNodeType::Operation && *this == toCalcExpressionOperation(other);
}

bool operator==(const CalcExpressionOperation& a, const CalcExpressionOperation& b)
{
    if (a.getOperator() != b.getOperator())
        return false;
    // Maybe Vectors of unique_ptrs should always do deep compare?
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

float CalcExpressionLength::evaluate(float maxValue) const
{
    return floatValueForLength(m_length, maxValue);
}

bool CalcExpressionLength::operator==(const CalcExpressionNode& other) const
{
    return other.type() == CalcExpressionNodeType::Length && *this == toCalcExpressionLength(other);
}

void CalcExpressionLength::dump(TextStream& ts) const
{
    ts << m_length;
}

CalcExpressionBlendLength::CalcExpressionBlendLength(Length from, Length to, float progress)
    : CalcExpressionNode(CalcExpressionNodeType::BlendLength)
    , m_from(from)
    , m_to(to)
    , m_progress(progress)
{
    // Flatten nesting of CalcExpressionBlendLength as a speculative fix for rdar://problem/30533005.
    // CalcExpressionBlendLength is only used as a result of animation and they don't nest in normal cases.
    if (m_from.isCalculated() && m_from.calculationValue().expression().type() == CalcExpressionNodeType::BlendLength)
        m_from = toCalcExpressionBlendLength(m_from.calculationValue().expression()).from();
    if (m_to.isCalculated() && m_to.calculationValue().expression().type() == CalcExpressionNodeType::BlendLength)
        m_to = toCalcExpressionBlendLength(m_to.calculationValue().expression()).to();
}

float CalcExpressionBlendLength::evaluate(float maxValue) const
{
    return (1.0f - m_progress) * floatValueForLength(m_from, maxValue) + m_progress * floatValueForLength(m_to, maxValue);
}

bool CalcExpressionBlendLength::operator==(const CalcExpressionNode& other) const
{
    return other.type() == CalcExpressionNodeType::BlendLength && *this == toCalcExpressionBlendLength(other);
}

void CalcExpressionBlendLength::dump(TextStream& ts) const
{
    ts << "blend(" << m_from << ", " << m_to << ", " << m_progress << ")";
}

TextStream& operator<<(TextStream& ts, CalcOperator op)
{
    switch (op) {
    case CalcOperator::Add: ts << "+"; break;
    case CalcOperator::Subtract: ts << "-"; break;
    case CalcOperator::Multiply: ts << "*"; break;
    case CalcOperator::Divide: ts << "/"; break;
    case CalcOperator::Min: ts << "max"; break;
    case CalcOperator::Max: ts << "min"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const CalculationValue& value)
{
    ts << "calc(";
    ts << value.expression();
    ts << ")";
    return ts;
}

TextStream& operator<<(TextStream& ts, const CalcExpressionNode& expressionNode)
{
    expressionNode.dump(ts);
    return ts;
}

} // namespace WebCore
