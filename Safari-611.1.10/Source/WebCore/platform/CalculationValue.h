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

#pragma once

#include "Length.h"
#include <memory>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

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
};

enum class CalcExpressionNodeType : uint8_t {
    Undefined,
    Number,
    Length,
    Operation,
    Negation,
    Inversion,
    BlendLength,
};

class CalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CalcExpressionNode(CalcExpressionNodeType = CalcExpressionNodeType::Undefined);
    virtual ~CalcExpressionNode() = default;

    CalcExpressionNodeType type() const { return m_type; }

    virtual float evaluate(float maxValue) const = 0;
    virtual bool operator==(const CalcExpressionNode&) const = 0;
    virtual void dump(WTF::TextStream&) const = 0;

private:
    CalcExpressionNodeType m_type;
};

class CalcExpressionNumber final : public CalcExpressionNode {
public:
    explicit CalcExpressionNumber(float);

    float value() const { return m_value; }

private:
    float evaluate(float) const override;
    bool operator==(const CalcExpressionNode&) const override;
    void dump(WTF::TextStream&) const override;

    float m_value;
};

class CalcExpressionLength final : public CalcExpressionNode {
public:
    explicit CalcExpressionLength(Length);

    const Length& length() const { return m_length; }

private:
    float evaluate(float maxValue) const override;
    bool operator==(const CalcExpressionNode&) const override;
    void dump(WTF::TextStream&) const override;

    Length m_length;
};

class CalcExpressionNegation final : public CalcExpressionNode {
public:
    CalcExpressionNegation(std::unique_ptr<CalcExpressionNode>&& node)
        : CalcExpressionNode(CalcExpressionNodeType::Negation)
        , m_child(WTFMove(node))
    {
        ASSERT(m_child);
    }

    float evaluate(float maxValue) const final;
    bool operator==(const CalcExpressionNode&) const final;
    void dump(WTF::TextStream&) const final;
    
    const CalcExpressionNode* child() const { return m_child.get(); }

private:
    std::unique_ptr<CalcExpressionNode> m_child;
};

bool operator==(const CalcExpressionNegation&, const CalcExpressionNegation&);

class CalcExpressionInversion final : public CalcExpressionNode {
public:
    CalcExpressionInversion(std::unique_ptr<CalcExpressionNode>&& node)
        : CalcExpressionNode(CalcExpressionNodeType::Inversion)
        , m_child(WTFMove(node))
    {
        ASSERT(m_child);
    }

    float evaluate(float maxValue) const final;
    bool operator==(const CalcExpressionNode&) const final;
    void dump(WTF::TextStream&) const final;

    const CalcExpressionNode* child() const { return m_child.get(); }

private:
    std::unique_ptr<CalcExpressionNode> m_child;
};

bool operator==(const CalcExpressionInversion&, const CalcExpressionInversion&);

class CalcExpressionOperation final : public CalcExpressionNode {
public:
    CalcExpressionOperation(Vector<std::unique_ptr<CalcExpressionNode>>&& children, CalcOperator);

    CalcOperator getOperator() const { return m_operator; }

    const Vector<std::unique_ptr<CalcExpressionNode>>& children() const { return m_children; }

private:
    float evaluate(float maxValue) const override;
    bool operator==(const CalcExpressionNode&) const override;
    void dump(WTF::TextStream&) const override;

    Vector<std::unique_ptr<CalcExpressionNode>> m_children;
    CalcOperator m_operator;
};

class CalcExpressionBlendLength final : public CalcExpressionNode {
public:
    CalcExpressionBlendLength(Length from, Length to, float progress);

    const Length& from() const { return m_from; }
    const Length& to() const { return m_to; }
    float progress() const { return m_progress; }

private:
    float evaluate(float maxValue) const override;
    bool operator==(const CalcExpressionNode&) const override;
    void dump(WTF::TextStream&) const override;

    Length m_from;
    Length m_to;
    float m_progress;
};

class CalculationValue : public RefCounted<CalculationValue> {
public:
    WEBCORE_EXPORT static Ref<CalculationValue> create(std::unique_ptr<CalcExpressionNode>, ValueRange);
    float evaluate(float maxValue) const;

    bool shouldClampToNonNegative() const { return m_shouldClampToNonNegative; }
    const CalcExpressionNode& expression() const { return *m_expression; }

private:
    CalculationValue(std::unique_ptr<CalcExpressionNode>, ValueRange);

    std::unique_ptr<CalcExpressionNode> m_expression;
    bool m_shouldClampToNonNegative;
};

inline CalcExpressionNode::CalcExpressionNode(CalcExpressionNodeType type)
    : m_type(type)
{
}

inline CalculationValue::CalculationValue(std::unique_ptr<CalcExpressionNode> expression, ValueRange range)
    : m_expression(WTFMove(expression))
    , m_shouldClampToNonNegative(range == ValueRangeNonNegative)
{
}

inline bool operator==(const CalculationValue& a, const CalculationValue& b)
{
    return a.expression() == b.expression();
}

inline CalcExpressionNumber::CalcExpressionNumber(float value)
    : CalcExpressionNode(CalcExpressionNodeType::Number)
    , m_value(value)
{
}

inline bool operator==(const CalcExpressionNumber& a, const CalcExpressionNumber& b)
{
    return a.value() == b.value();
}

inline CalcExpressionLength::CalcExpressionLength(Length length)
    : CalcExpressionNode(CalcExpressionNodeType::Length)
    , m_length(length)
{
}

inline bool operator==(const CalcExpressionLength& a, const CalcExpressionLength& b)
{
    return a.length() == b.length();
}

inline CalcExpressionOperation::CalcExpressionOperation(Vector<std::unique_ptr<CalcExpressionNode>>&& children, CalcOperator op)
    : CalcExpressionNode(CalcExpressionNodeType::Operation)
    , m_children(WTFMove(children))
    , m_operator(op)
{
}

bool operator==(const CalcExpressionOperation&, const CalcExpressionOperation&);

inline bool operator==(const CalcExpressionBlendLength& a, const CalcExpressionBlendLength& b)
{
    return a.progress() == b.progress() && a.from() == b.from() && a.to() == b.to();
}

WTF::TextStream& operator<<(WTF::TextStream&, const CalculationValue&);
WTF::TextStream& operator<<(WTF::TextStream&, const CalcExpressionNode&);
WTF::TextStream& operator<<(WTF::TextStream&, CalcOperator);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CalcExpressionNode& node) { return node.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(CalcExpressionNumber, type() == WebCore::CalcExpressionNodeType::Number)
SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(CalcExpressionLength, type() == WebCore::CalcExpressionNodeType::Length)
SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(CalcExpressionOperation, type() == WebCore::CalcExpressionNodeType::Operation)
SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(CalcExpressionNegation, type() == WebCore::CalcExpressionNodeType::Negation)
SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(CalcExpressionInversion, type() == WebCore::CalcExpressionNodeType::Inversion)
SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(CalcExpressionBlendLength, type() == WebCore::CalcExpressionNodeType::BlendLength)
