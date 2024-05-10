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
#include <wtf/Vector.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSCalcOperationNode);
class CSSCalcOperationNode final : public CSSCalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSCalcOperationNode);
public:
    enum class IsRoot : bool { No, Yes };
    
    static RefPtr<CSSCalcOperationNode> create(CalcOperator, RefPtr<CSSCalcExpressionNode>&& leftSide, RefPtr<CSSCalcExpressionNode>&& rightSide);
    static RefPtr<CSSCalcOperationNode> createSum(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createProduct(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createMinOrMaxOrClamp(CalcOperator, Vector<Ref<CSSCalcExpressionNode>>&& values, CalculationCategory destinationCategory);
    static RefPtr<CSSCalcOperationNode> createPowOrSqrt(CalcOperator, Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createHypot(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createTrig(CalcOperator, Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createLog(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createExp(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createInverseTrig(CalcOperator, Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createAtan2(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createSign(CalcOperator, Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createStep(CalcOperator, Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createRound(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createRoundConstant(CalcOperator);
    static Ref<CSSCalcExpressionNode> simplify(Ref<CSSCalcExpressionNode>&&);

    static void buildCSSText(const CSSCalcExpressionNode&, StringBuilder&);

    CalcOperator calcOperator() const { return m_operator; }
    bool isCalcSumNode() const { return m_operator == CalcOperator::Add; }
    bool isCalcProductNode() const { return m_operator == CalcOperator::Multiply; }
    bool isMinOrMaxNode() const { return m_operator == CalcOperator::Min || m_operator == CalcOperator::Max; }
    bool isTrigNode() const { return m_operator == CalcOperator::Sin || m_operator == CalcOperator::Cos || m_operator == CalcOperator::Tan; }
    bool isExpNode() const { return m_operator == CalcOperator::Exp || m_operator == CalcOperator::Log; }
    bool isInverseTrigNode() const { return m_operator == CalcOperator::Asin || m_operator == CalcOperator::Acos || m_operator == CalcOperator::Atan; }
    bool isAtan2Node() const { return m_operator == CalcOperator::Atan2; }
    bool isSignNode() const { return m_operator == CalcOperator::Sign; }
    bool isAbsOrSignNode() const { return m_operator == CalcOperator::Abs || isSignNode(); }
    bool shouldSortChildren() const { return isCalcSumNode() || isCalcProductNode(); }
    bool isSteppedNode() const { return m_operator == CalcOperator::Mod || m_operator == CalcOperator::Rem || m_operator == CalcOperator::Round; }
    bool isRoundOperation() const { return m_operator == CalcOperator::Down || m_operator == CalcOperator::Up || m_operator == CalcOperator::ToZero || m_operator == CalcOperator::Nearest; }
    bool isRoundConstant() const { return (isRoundOperation()) && !m_children.size(); }
    bool isHypotNode() const { return m_operator == CalcOperator::Hypot; }
    bool isSqrtNode() const { return m_operator == CalcOperator::Sqrt; }
    bool isPowOrSqrtNode() const { return m_operator == CalcOperator::Pow || isSqrtNode(); }
    bool isClampNode() const { return m_operator == CalcOperator::Clamp; }

    void hoistChildrenWithOperator(CalcOperator);
    void combineChildren();
    
    bool canCombineAllChildren() const;

    bool allowsNegativePercentageReference() const { return m_allowsNegativePercentageReference; }
    void setAllowsNegativePercentageReference() { m_allowsNegativePercentageReference = true; }

    bool isIdentity() const { return m_children.size() == 1 && (m_operator == CalcOperator::Min || m_operator == CalcOperator::Max || m_operator == CalcOperator::Add || m_operator == CalcOperator::Multiply); }

    const Vector<Ref<CSSCalcExpressionNode>>& children() const { return m_children; }
    Vector<Ref<CSSCalcExpressionNode>>& children() { return m_children; }

    static double convertToTopLevelValue(double value)
    {
        // If a top-level calculation would produce a value whose numeric part is NaN,
        // it instead act as though the numeric part is 0.
        if (std::isnan(value))
            value = 0;
        return value;
    }

private:
    CSSCalcOperationNode(CalculationCategory category, CalcOperator op, Ref<CSSCalcExpressionNode>&& leftSide, Ref<CSSCalcExpressionNode>&& rightSide)
        : CSSCalcExpressionNode(category)
        , m_operator(op)
        , m_children({ WTFMove(leftSide), WTFMove(rightSide) })
    {
    }

    CSSCalcOperationNode(CalculationCategory category, CalcOperator op, Vector<Ref<CSSCalcExpressionNode>>&& children)
        : CSSCalcExpressionNode(category)
        , m_operator(op)
        , m_children(WTFMove(children))
    {
    }

    Type type() const final { return CssCalcOperation; }

    bool isResolvable() const final;
    bool isZero() const final;
    bool equals(const CSSCalcExpressionNode&) const final;

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const final;

    CSSUnitType primitiveType() const final;
    double doubleValue(CSSUnitType, const CSSCalcSymbolTable&) const final;
    double computeLengthPx(const CSSToLengthConversionData&) const final;

    void collectComputedStyleDependencies(ComputedStyleDependencies&) const final;

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
        auto result = evaluateOperator(m_operator, children);
        return m_isRoot == IsRoot::No ? result : convertToTopLevelValue(result);
    }

    void makeTopLevelCalc();
    bool isNonCalcFunction() const { return isAbsOrSignNode() || isClampNode() || isMinOrMaxNode() || isExpNode() || isHypotNode() || isRoundOperation() || isSteppedNode() || isPowOrSqrtNode() || isInverseTrigNode() || isAtan2Node() || isTrigNode(); }
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
    bool m_allowsNegativePercentageReference = false;
    IsRoot m_isRoot = IsRoot::Yes;
};

}

SPECIALIZE_TYPE_TRAITS_CSSCALCEXPRESSION_NODE(CSSCalcOperationNode, type() == WebCore::CSSCalcExpressionNode::Type::CssCalcOperation)
