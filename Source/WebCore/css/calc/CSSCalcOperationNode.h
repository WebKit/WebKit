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

#include "CSSCalcExpressionNode.h"
#include "CalcOperator.h"

namespace WebCore {

class CSSCalcOperationNode final : public CSSCalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<CSSCalcOperationNode> create(CalcOperator, RefPtr<CSSCalcExpressionNode>&& leftSide, RefPtr<CSSCalcExpressionNode>&& rightSide);
    static RefPtr<CSSCalcOperationNode> createSum(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createProduct(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createMinOrMaxOrClamp(CalcOperator, Vector<Ref<CSSCalcExpressionNode>>&& values, CalculationCategory destinationCategory);

    static Ref<CSSCalcExpressionNode> simplify(Ref<CSSCalcExpressionNode>&&);

    static void buildCSSText(const CSSCalcExpressionNode&, StringBuilder&);

    CalcOperator calcOperator() const { return m_operator; }
    bool isCalcSumNode() const { return m_operator == CalcOperator::Add; }
    bool isCalcProductNode() const { return m_operator == CalcOperator::Multiply; }
    bool isMinOrMaxNode() const { return m_operator == CalcOperator::Min || m_operator == CalcOperator::Max; }
    bool shouldSortChildren() const { return isCalcSumNode() || isCalcProductNode(); }

    void hoistChildrenWithOperator(CalcOperator);
    void combineChildren();
    
    bool canCombineAllChildren() const;

    const Vector<Ref<CSSCalcExpressionNode>>& children() const { return m_children; }
    Vector<Ref<CSSCalcExpressionNode>>& children() { return m_children; }

private:
    CSSCalcOperationNode(CalculationCategory category, CalcOperator op, Ref<CSSCalcExpressionNode>&& leftSide, Ref<CSSCalcExpressionNode>&& rightSide)
        : CSSCalcExpressionNode(category)
        , m_operator(op)
    {
        m_children.reserveInitialCapacity(2);
        m_children.uncheckedAppend(WTFMove(leftSide));
        m_children.uncheckedAppend(WTFMove(rightSide));
    }

    CSSCalcOperationNode(CalculationCategory category, CalcOperator op, Vector<Ref<CSSCalcExpressionNode>>&& children)
        : CSSCalcExpressionNode(category)
        , m_operator(op)
        , m_children(WTFMove(children))
    {
    }

    Type type() const final { return CssCalcOperation; }

    bool isZero() const final
    {
        return !doubleValue(primitiveType());
    }

    bool equals(const CSSCalcExpressionNode&) const final;

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const final;

    CSSUnitType primitiveType() const final;
    double doubleValue(CSSUnitType) const final;
    double computeLengthPx(const CSSToLengthConversionData&) const final;

    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>&) const final;
    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>&) const final;

    void dump(TextStream&) const final;

    static CSSCalcExpressionNode* getNumberSide(CSSCalcExpressionNode& leftSide, CSSCalcExpressionNode& rightSide)
    {
        if (leftSide.category() == CalculationCategory::Number)
            return &leftSide;
        if (rightSide.category() == CalculationCategory::Number)
            return &rightSide;
        return nullptr;
    }

    double evaluate(const Vector<double>& children) const
    {
        return evaluateOperator(m_operator, children);
    }

    static double evaluateOperator(CalcOperator, const Vector<double>&);
    static Ref<CSSCalcExpressionNode> simplifyNode(Ref<CSSCalcExpressionNode>&&, int depth);
    static Ref<CSSCalcExpressionNode> simplifyRecursive(Ref<CSSCalcExpressionNode>&&, int depth);
    
    enum class GroupingParens {
        Omit,
        Include
    };
    static void buildCSSTextRecursive(const CSSCalcExpressionNode&, StringBuilder&, GroupingParens = GroupingParens::Include);

    CalcOperator m_operator;
    Vector<Ref<CSSCalcExpressionNode>> m_children;
};

}

SPECIALIZE_TYPE_TRAITS_CSSCALCEXPRESSION_NODE(CSSCalcOperationNode, type() == WebCore::CSSCalcExpressionNode::Type::CssCalcOperation)
