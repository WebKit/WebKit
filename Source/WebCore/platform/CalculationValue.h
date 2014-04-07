/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef CalculationValue_h
#define CalculationValue_h

#include "Length.h"
#include "LengthFunctions.h"
#include <memory>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

enum CalcOperator {
    CalcAdd = '+',
    CalcSubtract = '-',
    CalcMultiply = '*',
    CalcDivide = '/'
};

enum CalculationPermittedValueRange {
    CalculationRangeAll,
    CalculationRangeNonNegative
};

enum CalcExpressionNodeType {
    CalcExpressionNodeUndefined,
    CalcExpressionNodeNumber,
    CalcExpressionNodeLength,
    CalcExpressionNodeBinaryOperation,
    CalcExpressionNodeBlendLength,
};

class CalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CalcExpressionNode(CalcExpressionNodeType = CalcExpressionNodeUndefined);
    virtual ~CalcExpressionNode() { }

    CalcExpressionNodeType type() const { return m_type; }

    virtual float evaluate(float maxValue) const = 0;
    virtual bool operator==(const CalcExpressionNode&) const = 0;

private:
    CalcExpressionNodeType m_type;
};

class CalcExpressionNumber final : public CalcExpressionNode {
public:
    explicit CalcExpressionNumber(float);

    float value() const { return m_value; }

private:
    virtual float evaluate(float) const override;
    virtual bool operator==(const CalcExpressionNode&) const override;

    float m_value;
};

class CalcExpressionLength final : public CalcExpressionNode {
public:
    explicit CalcExpressionLength(Length);

    const Length& length() const { return m_length; }

private:
    virtual float evaluate(float maxValue) const override;
    virtual bool operator==(const CalcExpressionNode&) const override;

    Length m_length;
};

class CalcExpressionBinaryOperation final : public CalcExpressionNode {
public:
    CalcExpressionBinaryOperation(std::unique_ptr<CalcExpressionNode> leftSide, std::unique_ptr<CalcExpressionNode> rightSide, CalcOperator);

    const CalcExpressionNode& leftSide() const { return *m_leftSide; }
    const CalcExpressionNode& rightSide() const { return *m_rightSide; }
    CalcOperator getOperator() const { return m_operator; }

private:
    virtual float evaluate(float maxValue) const override;
    virtual bool operator==(const CalcExpressionNode&) const override;

    std::unique_ptr<CalcExpressionNode> m_leftSide;
    std::unique_ptr<CalcExpressionNode> m_rightSide;
    CalcOperator m_operator;
};

class CalcExpressionBlendLength final : public CalcExpressionNode {
public:
    CalcExpressionBlendLength(Length from, Length to, float progress);

    const Length& from() const { return m_from; }
    const Length& to() const { return m_to; }
    float progress() const { return m_progress; }

private:
    virtual float evaluate(float maxValue) const override;
    virtual bool operator==(const CalcExpressionNode&) const override;

    Length m_from;
    Length m_to;
    float m_progress;
};

class CalculationValue : public RefCounted<CalculationValue> {
public:
    static PassRef<CalculationValue> create(std::unique_ptr<CalcExpressionNode>, CalculationPermittedValueRange);
    float evaluate(float maxValue) const;

    bool shouldClampToNonNegative() const { return m_shouldClampToNonNegative; }
    const CalcExpressionNode& expression() const { return *m_expression; }

private:
    CalculationValue(std::unique_ptr<CalcExpressionNode>, CalculationPermittedValueRange);

    std::unique_ptr<CalcExpressionNode> m_expression;
    bool m_shouldClampToNonNegative;
};

inline CalcExpressionNode::CalcExpressionNode(CalcExpressionNodeType type)
    : m_type(type)
{
}

inline CalculationValue::CalculationValue(std::unique_ptr<CalcExpressionNode> expression, CalculationPermittedValueRange range)
    : m_expression(std::move(expression))
    , m_shouldClampToNonNegative(range == CalculationRangeNonNegative)
{
}

inline bool operator==(const CalculationValue& a, const CalculationValue& b)
{
    return a.expression() == b.expression();
}

inline CalcExpressionNumber::CalcExpressionNumber(float value)
    : CalcExpressionNode(CalcExpressionNodeNumber)
    , m_value(value)
{
}

inline bool operator==(const CalcExpressionNumber& a, const CalcExpressionNumber& b)
{
    return a.value() == b.value();
}

inline const CalcExpressionNumber& toCalcExpressionNumber(const CalcExpressionNode& value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value.type() == CalcExpressionNodeNumber);
    return static_cast<const CalcExpressionNumber&>(value);
}

inline CalcExpressionLength::CalcExpressionLength(Length length)
    : CalcExpressionNode(CalcExpressionNodeLength)
    , m_length(length)
{
}

inline bool operator==(const CalcExpressionLength& a, const CalcExpressionLength& b)
{
    return a.length() == b.length();
}

inline const CalcExpressionLength& toCalcExpressionLength(const CalcExpressionNode& value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value.type() == CalcExpressionNodeLength);
    return static_cast<const CalcExpressionLength&>(value);
}

inline CalcExpressionBinaryOperation::CalcExpressionBinaryOperation(std::unique_ptr<CalcExpressionNode> leftSide, std::unique_ptr<CalcExpressionNode> rightSide, CalcOperator op)
    : CalcExpressionNode(CalcExpressionNodeBinaryOperation)
    , m_leftSide(std::move(leftSide))
    , m_rightSide(std::move(rightSide))
    , m_operator(op)
{
}

inline bool operator==(const CalcExpressionBinaryOperation& a, const CalcExpressionBinaryOperation& b)
{
    return a.getOperator() == b.getOperator() && a.leftSide() == b.leftSide() && a.rightSide() == b.rightSide();
}

inline const CalcExpressionBinaryOperation& toCalcExpressionBinaryOperation(const CalcExpressionNode& value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value.type() == CalcExpressionNodeBinaryOperation);
    return static_cast<const CalcExpressionBinaryOperation&>(value);
}

inline CalcExpressionBlendLength::CalcExpressionBlendLength(Length from, Length to, float progress)
    : CalcExpressionNode(CalcExpressionNodeBlendLength)
    , m_from(from)
    , m_to(to)
    , m_progress(progress)
{
}

inline bool operator==(const CalcExpressionBlendLength& a, const CalcExpressionBlendLength& b)
{
    return a.progress() == b.progress() && a.from() == b.from() && a.to() == b.to();
}

inline const CalcExpressionBlendLength& toCalcExpressionBlendLength(const CalcExpressionNode& value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value.type() == CalcExpressionNodeBlendLength);
    return static_cast<const CalcExpressionBlendLength&>(value);
}

} // namespace WebCore

#endif // CalculationValue_h
