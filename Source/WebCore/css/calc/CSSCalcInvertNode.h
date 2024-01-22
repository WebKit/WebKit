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

namespace WebCore {

class CSSCalcInvertNode final : public CSSCalcExpressionNode {
public:
    static Ref<CSSCalcInvertNode> create(Ref<CSSCalcExpressionNode>&& child)
    {
        return adoptRef(*new CSSCalcInvertNode(WTFMove(child)));
    }

    const CSSCalcExpressionNode& child() const { return m_child.get(); }
    CSSCalcExpressionNode& child() { return m_child.get(); }
    Ref<CSSCalcExpressionNode> protectedChild() const { return m_child; }

    void setChild(Ref<CSSCalcExpressionNode>&& child) { m_child = WTFMove(child); }

private:
    CSSCalcInvertNode(Ref<CSSCalcExpressionNode>&& child)
        : CSSCalcExpressionNode(child->category())
        , m_child(WTFMove(child))
    {
    }

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const final;

    bool isZero() const final { return m_child->isZero(); }
    double doubleValue(CSSUnitType) const final;
    double computeLengthPx(const CSSToLengthConversionData&) const final;
    Type type() const final { return Type::CssCalcInvert; }
    CSSUnitType primitiveType() const final { return m_child->primitiveType(); }

    void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const final { m_child->collectComputedStyleDependencies(dependencies); }

    void dump(TextStream&) const final;

    Ref<CSSCalcExpressionNode> m_child;
};

}

SPECIALIZE_TYPE_TRAITS_CSSCALCEXPRESSION_NODE(CSSCalcInvertNode, type() == WebCore::CSSCalcExpressionNode::Type::CssCalcInvert)
