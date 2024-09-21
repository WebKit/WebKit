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
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"

#include "CSSCalcSymbolTable.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename IntType, IntegerValueRange integerRange>
static RefPtr<CSSPrimitiveValue> consumeIntegerType(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return CSSPrimitiveValueResolver<IntegerRaw<IntType, integerRange>>::consumeAndResolve(range, context, { }, { }, { });
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeIntegerType<int, IntegerValueRange::All>(range, context);
}

RefPtr<CSSPrimitiveValue> consumeNonNegativeInteger(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeIntegerType<int, IntegerValueRange::NonNegative>(range, context);
}

RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeIntegerType<unsigned, IntegerValueRange::Positive>(range, context);
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, const CSSParserContext& context, IntegerValueRange valueRange)
{
    switch (valueRange) {
    case IntegerValueRange::All:
        return consumeInteger(range, context);
    case IntegerValueRange::Positive:
        return consumePositiveInteger(range, context);
    case IntegerValueRange::NonNegative:
        return consumeNonNegativeInteger(range, context);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
