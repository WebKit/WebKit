/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#include "CalculationCategory.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class CSSToLengthConversionData;
class CalcExpressionNode;

struct ComputedStyleDependencies;

enum CSSPropertyID : uint16_t;

enum class CSSUnitType : uint8_t;

class CSSCalcExpressionNode : public RefCounted<CSSCalcExpressionNode> {
public:
    enum Type {
        CssCalcPrimitiveValue = 1,
        CssCalcOperation,
        CssCalcNegate,
        CssCalcInvert,
    };

    virtual ~CSSCalcExpressionNode() = default;
    virtual bool isZero() const = 0;
    virtual std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const = 0;
    virtual double doubleValue(CSSUnitType) const = 0;
    virtual double computeLengthPx(const CSSToLengthConversionData&) const = 0;
    virtual bool equals(const CSSCalcExpressionNode& other) const { return m_category == other.m_category; }
    virtual Type type() const = 0;
    virtual CSSUnitType primitiveType() const = 0;

    virtual void collectComputedStyleDependencies(ComputedStyleDependencies&) const = 0;

    CalculationCategory category() const { return m_category; }

    virtual void dump(TextStream&) const = 0;

protected:
    CSSCalcExpressionNode(CalculationCategory category)
        : m_category(category)
    {
    }

private:
    CalculationCategory m_category;
};

TextStream& operator<<(TextStream&, const CSSCalcExpressionNode&);

#if !LOG_DISABLED
String prettyPrintNode(const CSSCalcExpressionNode&);
String prettyPrintNodes(const Vector<Ref<CSSCalcExpressionNode>>&);
#endif

}

#define SPECIALIZE_TYPE_TRAITS_CSSCALCEXPRESSION_NODE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CSSCalcExpressionNode& node) { return node.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
