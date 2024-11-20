/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSMathValue.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSUnitValue.h"

namespace WebCore {
namespace CSS {

// MARK: - Conversion to strongly typed `CSS::` value types from "type CSSOM"  types.

template<typename CSSType> struct CSSOMValueConversion;
template<typename CSSType> std::optional<CSSType> convertFromCSSOMValue(Ref<CSSNumericValue> numeric)
{
    return CSSOMValueConversion<CSSType>{}(WTFMove(numeric));
}

template<RawNumeric RawType> struct CSSOMValueConversion<PrimitiveNumeric<RawType>> {
    std::optional<PrimitiveNumeric<RawType>> operator()(Ref<CSSNumericValue> numeric)
    {
        if (RefPtr mathValue = dynamicDowncast<CSSMathValue>(numeric))
            return mathValue->toCSS<PrimitiveNumeric<RawType>>();

        Ref unitValue = downcast<CSSUnitValue>(numeric);
        return unitValue->toCSS<PrimitiveNumeric<RawType>>();
    }
};

} // namespace CSS
} // namespace WebCore
