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
    return other.type() == CalcExpressionNodeNumber && *this == toCalcExpressionNumber(other);
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

float CalcExpressionBinaryOperation::evaluate(float maxValue) const
{
    float left = m_leftSide->evaluate(maxValue);
    float right = m_rightSide->evaluate(maxValue);
    switch (m_operator) {
    case CalcAdd:
        return left + right;
    case CalcSubtract:
        return left - right;
    case CalcMultiply:
        return left * right;
    case CalcDivide:
        if (!right)
            return std::numeric_limits<float>::quiet_NaN();
        return left / right;
    }
    ASSERT_NOT_REACHED();
    return std::numeric_limits<float>::quiet_NaN();
}

bool CalcExpressionBinaryOperation::operator==(const CalcExpressionNode& other) const
{
    return other.type() == CalcExpressionNodeBinaryOperation && *this == toCalcExpressionBinaryOperation(other);
}

void CalcExpressionBinaryOperation::dump(TextStream& ts) const
{
    ts << *m_leftSide << " " << m_operator << " " << *m_rightSide;
}

float CalcExpressionLength::evaluate(float maxValue) const
{
    return floatValueForLength(m_length, maxValue);
}

bool CalcExpressionLength::operator==(const CalcExpressionNode& other) const
{
    return other.type() == CalcExpressionNodeLength && *this == toCalcExpressionLength(other);
}

void CalcExpressionLength::dump(TextStream& ts) const
{
    ts << m_length;
}

float CalcExpressionBlendLength::evaluate(float maxValue) const
{
    return (1.0f - m_progress) * floatValueForLength(m_from, maxValue) + m_progress * floatValueForLength(m_to, maxValue);
}

bool CalcExpressionBlendLength::operator==(const CalcExpressionNode& other) const
{
    return other.type() == CalcExpressionNodeBlendLength && *this == toCalcExpressionBlendLength(other);
}

void CalcExpressionBlendLength::dump(TextStream& ts) const
{
    ts << "blend(" << m_from << ", " << m_to << ", " << m_progress << ")";
}

TextStream& operator<<(TextStream& ts, CalcOperator op)
{
    switch (op) {
    case CalcAdd: ts << "+"; break;
    case CalcSubtract: ts << "-"; break;
    case CalcMultiply: ts << "*"; break;
    case CalcDivide: ts << "/"; break;
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
