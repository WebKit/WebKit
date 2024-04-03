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

class CSSPrimitiveValue;
class CSSToLengthConversionData;

enum CSSPropertyID : uint16_t;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSCalcPrimitiveValueNode);
class CSSCalcPrimitiveValueNode final : public CSSCalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSCalcPrimitiveValueNode);
public:
    static Ref<CSSCalcPrimitiveValueNode> create(Ref<CSSPrimitiveValue>&&);
    static RefPtr<CSSCalcPrimitiveValueNode> create(double value, CSSUnitType);

    String customCSSText() const;

    CSSUnitType primitiveType() const final;

    bool isNumericValue() const;
    bool isNegative() const;

    void negate();
    void invert();

    enum class UnitConversion {
        Invalid,
        Preserve,
        Canonicalize
    };
    void add(const CSSCalcPrimitiveValueNode&, UnitConversion = UnitConversion::Preserve);
    void multiply(double);

    void convertToUnitType(CSSUnitType);
    void canonicalizeUnit();

    const CSSPrimitiveValue& value() const { return m_value.get(); }
    Ref<CSSPrimitiveValue> protectedValue() const { return m_value; }

    double doubleValue(CSSUnitType) const final;

private:
    bool isZero() const final;
    bool equals(const CSSCalcExpressionNode& other) const final;
    Type type() const final { return CssCalcPrimitiveValue; }

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const final;

    double computeLengthPx(const CSSToLengthConversionData&) const final;
    void collectComputedStyleDependencies(ComputedStyleDependencies&) const final;

    void dump(TextStream&) const final;

private:
    explicit CSSCalcPrimitiveValueNode(Ref<CSSPrimitiveValue>&&);

    Ref<CSSPrimitiveValue> m_value;
};

}

SPECIALIZE_TYPE_TRAITS_CSSCALCEXPRESSION_NODE(CSSCalcPrimitiveValueNode, type() == WebCore::CSSCalcExpressionNode::Type::CssCalcPrimitiveValue)
