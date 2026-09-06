/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+CalcSize.h"

#include "CSSCalcSizeValue.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcTree+Parser.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSCalcValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSPrimitiveNumericRange.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSUnits.h"
#include "CSSValueKeywords.h"
#include <wtf/UniqueRef.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// The basis accepts the sizing keywords the property itself accepts, so `auto` is only a valid
// basis for the properties that have it.
static bool isValidBasisKeyword(CSSValueID keyword, CSSPropertyID property)
{
    switch (keyword) {
    case CSSValueAny:
    case CSSValueStretch:
    case CSSValueIntrinsic:
    case CSSValueMinIntrinsic:
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
    case CSSValueWebkitFillAvailable:
    case CSSValueFitContent:
    case CSSValueWebkitFitContent:
        return true;
    case CSSValueAuto:
        switch (property) {
        case CSSPropertyMaxWidth:
        case CSSPropertyMaxHeight:
        case CSSPropertyMaxBlockSize:
        case CSSPropertyMaxInlineSize:
            return false;
        default:
            return true;
        }
    case CSSValueContent:
        return property == CSSPropertyFlexBasis;
    default:
        return false;
    }
}

// Both arguments are `<calc-sum>` productions. The range is unrestricted even for the properties
// that only accept non-negative sizes: the calculation is allowed to go negative internally and is
// clamped when it is resolved.
static RefPtr<CSSCalc::Value> consumeCalcSum(CSSParserTokenRange& args, CSS::PropertyParserState& state, bool sizeKeywordAllowed)
{
    auto parserOptions = CSSCalc::ParserOptions {
        .category = CSS::Category::LengthPercentage,
        .range = CSS::All,
        .allowedSymbols = sizeKeywordAllowed ? CSSCalcSymbolsAllowed { { CSSValueSize, CSSUnitType::Px } } : CSSCalcSymbolsAllowed { },
        .propertyOptions = { }
    };
    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = CSS::Category::LengthPercentage,
        .range = CSS::All,
        .conversionData = std::nullopt,
        .symbolTable = { },
        .allowZeroValueLengthRemovalFromSum = false,
    };

    auto tree = CSSCalc::parseAndSimplifyCalcSum(args, state, parserOptions, simplificationOptions);
    if (!tree)
        return { };

    return CSSCalc::Value::create(CSS::Category::LengthPercentage, CSS::All, WTF::move(*tree));
}

static std::optional<CSS::CalcSize> consumeCalcSizeFunction(CSSParserTokenRange&, CSS::PropertyParserState&);

static std::optional<CSS::CalcSizeBasis> consumeCalcSizeBasis(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <calc-size-basis> = [ <size-keyword> | <calc-sum> | <calc-size()> | any ]

    if (args.peek().type() == IdentToken && isValidBasisKeyword(args.peek().id(), state.currentProperty))
        return CSS::CalcSizeBasis { args.consumeIncludingWhitespace().id() };

    if (args.peek().functionId() == CSSValueCalcSize) {
        auto nested = consumeCalcSizeFunction(args, state);
        if (!nested)
            return { };
        return CSS::CalcSizeBasis { makeUniqueRef<CSS::CalcSize>(WTF::move(*nested)) };
    }

    RefPtr basis = consumeCalcSum(args, state, false);
    if (!basis)
        return { };
    return CSS::CalcSizeBasis { basis.releaseNonNull() };
}

static std::optional<CSS::CalcSize> consumeCalcSizeFunction(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <calc-size()> = calc-size( <calc-size-basis>, <calc-sum> )

    ASSERT(range.peek().functionId() == CSSValueCalcSize);

    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);

    auto basis = consumeCalcSizeBasis(args, state);
    if (!basis)
        return { };

    if (!consumeCommaIncludingWhitespace(args))
        return { };

    // `size` is a syntax error when the basis is `any`, but is allowed when the basis is a nested
    // calc-size() whose own basis is `any`.
    bool sizeKeywordAllowed = !(std::holds_alternative<CSSValueID>(*basis) && std::get<CSSValueID>(*basis) == CSSValueAny);

    RefPtr calculation = consumeCalcSum(args, state, sizeKeywordAllowed);
    if (!calculation || !args.atEnd())
        return { };

    range = rangeCopy;
    return CSS::CalcSize { WTF::move(*basis), calculation.releaseNonNull() };
}

RefPtr<CSSValue> consumeCalcSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (!state.context.cssCalcSizeFunctionEnabled)
        return { };

    if (range.peek().functionId() != CSSValueCalcSize)
        return { };

    auto calcSize = consumeCalcSizeFunction(range, state);
    if (!calcSize)
        return { };

    return CSSCalcSizeValue::create(WTF::move(*calcSize));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
