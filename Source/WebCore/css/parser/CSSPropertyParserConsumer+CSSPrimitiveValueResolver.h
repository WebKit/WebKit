/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+MetaResolver.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include <wtf/RefPtr.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: Resolver for users that want to get a CSSPrimitiveValue result.

/// Non-template base type for code sharing.
struct CSSPrimitiveValueResolverBase {
    static RefPtr<CSSPrimitiveValue> resolve(AngleRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static RefPtr<CSSPrimitiveValue> resolve(LengthRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static RefPtr<CSSPrimitiveValue> resolve(NumberRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static RefPtr<CSSPrimitiveValue> resolve(PercentRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static RefPtr<CSSPrimitiveValue> resolve(ResolutionRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static RefPtr<CSSPrimitiveValue> resolve(TimeRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static RefPtr<CSSPrimitiveValue> resolve(NoneRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static RefPtr<CSSPrimitiveValue> resolve(SymbolRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);

    template<typename IntType, IntegerValueRange integerRange>
    static RefPtr<CSSPrimitiveValue> resolve(IntegerRaw<IntType, integerRange> value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
    {
        return CSSPrimitiveValue::createInteger(value.value);
    }

    template<typename RawType>
    static RefPtr<CSSPrimitiveValue> resolve(UnevaluatedCalc<RawType> value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
    {
        return CSSPrimitiveValue::create(value.calc);
    }

    template<typename IntType, IntegerValueRange integerRange>
    static RefPtr<CSSPrimitiveValue> resolve(UnevaluatedCalc<IntegerRaw<IntType, integerRange>> calc, const CSSCalcSymbolTable& symbolTable, CSSPropertyParserOptions)
    {
        // FIXME: This should not be eagerly resolving the calc. Instead, callers
        // should resolve and round at style resolution.

        // https://drafts.csswg.org/css-values-4/#integers
        // Rounding to the nearest integer requires rounding in the direction of +∞ when the fractional portion is exactly 0.5.
        auto value = clampTo<IntType>(std::floor(std::max(calc.calc->doubleValue(symbolTable), computeMinimumValue(integerRange)) + 0.5));
        return CSSPrimitiveValue::createInteger(value);
    }
};

template<typename... Ts>
struct CSSPrimitiveValueResolver : MetaResolver<RefPtr<CSSPrimitiveValue>, CSSPrimitiveValueResolverBase, Ts...> {
    using MetaResolver<RefPtr<CSSPrimitiveValue>, CSSPrimitiveValueResolverBase, Ts...>::resolve;
    using MetaResolver<RefPtr<CSSPrimitiveValue>, CSSPrimitiveValueResolverBase, Ts...>::consumeAndResolve;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
