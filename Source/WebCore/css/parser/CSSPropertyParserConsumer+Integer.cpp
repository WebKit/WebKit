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

#include "config.h"
#include "CSSPropertyParserConsumer+Integer.h"

#include "CSSCalcSymbolTable.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+RawResolver.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<IntegerRawValueRange R>
static std::optional<typename IntegerRaw<R>::IntType> consumeIntegerTypeRaw(CSSParserTokenRange& range)
{
    if (auto result = RawResolver<IntegerRaw<R>>::consumeAndResolve(range, { }, { }, { }))
        return result->value;
    return std::nullopt;
}

template<IntegerRawValueRange R>
static RefPtr<CSSPrimitiveValue> consumeIntegerType(CSSParserTokenRange& range)
{
    return CSSPrimitiveValueResolver<IntegerRaw<R>>::consumeAndResolve(range, { }, { }, { });
}

std::optional<int> consumeIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<IntegerRawValueRange::All>(range);
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range)
{
    return consumeIntegerType<IntegerRawValueRange::All>(range);
}

std::optional<int> consumeNonNegativeIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<IntegerRawValueRange::NonNegative>(range);
}

RefPtr<CSSPrimitiveValue> consumeNonNegativeInteger(CSSParserTokenRange& range)
{
    return consumeIntegerType<IntegerRawValueRange::NonNegative>(range);
}

std::optional<unsigned> consumePositiveIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<IntegerRawValueRange::Positive>(range);
}

RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range)
{
    return consumeIntegerType<IntegerRawValueRange::Positive>(range);
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, IntegerRawValueRange valueRange)
{
    switch (valueRange) {
    case IntegerRawValueRange::All:
        return consumeInteger(range);
    case IntegerRawValueRange::Positive:
        return consumePositiveInteger(range);
    case IntegerRawValueRange::NonNegative:
        return consumeNonNegativeInteger(range);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
