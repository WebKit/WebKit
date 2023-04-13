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

#include "CSSCalcExpressionNode.h"
#include "CSSCalcValue.h"
#include "CSSMathOperator.h"
#include "CSSNumericArray.h"
#include "CSSNumericValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSStyleValue.h"

namespace WebCore {

class CSSMathValue : public CSSNumericValue {
public:
    CSSMathValue(CSSNumericType type)
        : CSSNumericValue(WTFMove(type)) { }
    virtual CSSMathOperator getOperator() const = 0;

    template <typename T>
    bool equalsImpl(const CSSNumericValue& other) const
    {
        // https://drafts.css-houdini.org/css-typed-om/#equal-numeric-value
        auto* otherT = dynamicDowncast<T>(other);
        if (!otherT)
            return false;

        ASSERT(getType() == other.getType());
        auto& thisValues = static_cast<const T*>(this)->values();
        auto& otherValues = otherT->values();
        auto length = thisValues.length();
        if (length != otherValues.length())
            return false;

        for (size_t i = 0 ; i < length; ++i) {
            if (!thisValues.array()[i]->equals(otherValues.array()[i].get()))
                return false;
        }

        return true;
    }

    RefPtr<CSSValue> toCSSValue() const final
    {
        auto node = toCalcExpressionNode();
        if (!node)
            return nullptr;
        return CSSPrimitiveValue::create(CSSCalcValue::create(node.releaseNonNull()));
    }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSMathValue)
static bool isType(const WebCore::CSSStyleValue& styleValue) { return WebCore::isCSSMathValue(styleValue.getType()); }
SPECIALIZE_TYPE_TRAITS_END()
