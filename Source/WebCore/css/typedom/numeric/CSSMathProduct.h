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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSMathValue.h"
#include "CSSNumericValue.h"

namespace WebCore {

class CSSNumericArray;

class CSSMathProduct final : public CSSMathValue {
    WTF_MAKE_ISO_ALLOCATED(CSSMathProduct);
public:
    static ExceptionOr<Ref<CSSMathProduct>> create(FixedVector<CSSNumberish>);
    static ExceptionOr<Ref<CSSMathProduct>> create(Vector<Ref<CSSNumericValue>>);
    const CSSNumericArray& values() const { return m_values.get(); }

    RefPtr<CSSCalcExpressionNode> toCalcExpressionNode() const final;

private:
    CSSMathOperator getOperator() const final { return CSSMathOperator::Product; }
    CSSStyleValueType getType() const final { return CSSStyleValueType::CSSMathProduct; }
    void serialize(StringBuilder&, OptionSet<SerializationArguments>) const;
    std::optional<SumValue> toSumValue() const final;
    bool equals(const CSSNumericValue& other) const final { return equalsImpl<CSSMathProduct>(other); }

    CSSMathProduct(Vector<Ref<CSSNumericValue>>, CSSNumericType);
    Ref<CSSNumericArray> m_values;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSMathProduct)
static bool isType(const WebCore::CSSStyleValue& styleValue) { return styleValue.getType() == WebCore::CSSStyleValueType::CSSMathProduct; }
static bool isType(const WebCore::CSSNumericValue& numericValue) { return numericValue.getType() == WebCore::CSSStyleValueType::CSSMathProduct; }
static bool isType(const WebCore::CSSMathValue& mathValue) { return mathValue.getType() == WebCore::CSSStyleValueType::CSSMathProduct; }
SPECIALIZE_TYPE_TRAITS_END()
