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
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"

#include "CSSCalcSymbolTable.h"
#include "CSSParserContext.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, const CSSParserContext& context, ValueRange valueRange, UnitlessQuirk unitless)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitless = unitless,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    if (valueRange == ValueRange::All)
        return CSSPrimitiveValueResolver<CSS::Length<CSS::All>>::consumeAndResolve(range, context, { }, { }, options);
    return CSSPrimitiveValueResolver<CSS::Length<CSS::Nonnegative>>::consumeAndResolve(range, context, { }, { }, options);
}

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, const CSSParserContext& context, CSSParserMode overrideParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = overrideParserMode,
        .unitless = unitless,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    if (valueRange == ValueRange::All)
        return CSSPrimitiveValueResolver<CSS::Length<CSS::All>>::consumeAndResolve(range, context, { }, { }, options);
    return CSSPrimitiveValueResolver<CSS::Length<CSS::Nonnegative>>::consumeAndResolve(range, context, { }, { }, options);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
