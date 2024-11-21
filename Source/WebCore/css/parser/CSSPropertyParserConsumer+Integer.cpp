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
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename T> static RefPtr<CSSPrimitiveValue> consumeIntegerType(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return CSSPrimitiveValueResolver<T>::consumeAndResolve(range, context, { }, { }, { });
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeIntegerType<CSS::Integer<CSS::All, int>>(range, context);
}

RefPtr<CSSPrimitiveValue> consumeNonNegativeInteger(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeIntegerType<CSS::Integer<CSS::Range{0, CSS::Range::infinity}, int>>(range, context);
}

RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeIntegerType<CSS::Integer<CSS::Range{1, CSS::Range::infinity}, unsigned>>(range, context);
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, const CSSParserContext& context, const CSS::Range& valueRange)
{
    if (valueRange == CSS::All)
        return consumeInteger(range, context);
    if (valueRange == CSS::Range{0, CSS::Range::infinity})
        return consumeNonNegativeInteger(range, context);
    if (valueRange == CSS::Range{1, CSS::Range::infinity})
        return consumePositiveInteger(range, context);

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
