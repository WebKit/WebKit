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

#include "CSSCalcValue.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+MetaResolver.h"
#include "CSSPropertyParserOptions.h"
#include "CSSUnevaluatedCalc.h"
#include <wtf/RefPtr.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: Resolver for users that want to get a CSSPrimitiveValue result.

/// Non-template base type for code sharing.
struct CSSPrimitiveValueResolverBase {
    static RefPtr<CSSPrimitiveValue> resolve(CSS::NumericRaw auto value, CSSPropertyParserOptions)
    {
        return CSSPrimitiveValue::create(value.value, CSS::toCSSUnitType(value.unit));
    }

    template<CSS::Range R, typename IntType> static RefPtr<CSSPrimitiveValue> resolve(CSS::IntegerRaw<R, IntType> value, CSSPropertyParserOptions)
    {
        return CSSPrimitiveValue::createInteger(value.value);
    }

    static RefPtr<CSSPrimitiveValue> resolve(CSS::Calc auto value, CSSPropertyParserOptions)
    {
        return CSSPrimitiveValue::create(value.protectedCalc());
    }

    static RefPtr<CSSPrimitiveValue> resolve(CSS::Numeric auto value, CSSPropertyParserOptions options)
    {
        return WTF::switchOn(WTFMove(value), [&](auto&& value) { return resolve(WTFMove(value), options); });
    }
};

template<typename... Ts>
struct CSSPrimitiveValueResolver : MetaResolver<RefPtr<CSSPrimitiveValue>, CSSPrimitiveValueResolverBase, Ts...> {
    using MetaResolver<RefPtr<CSSPrimitiveValue>, CSSPrimitiveValueResolverBase, Ts...>::resolve;
    using MetaResolver<RefPtr<CSSPrimitiveValue>, CSSPrimitiveValueResolverBase, Ts...>::consumeAndResolve;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
