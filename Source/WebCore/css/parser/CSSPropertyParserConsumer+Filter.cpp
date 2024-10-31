/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSPropertyParserConsumer+Filter.h"

#include "CSSFilterFunctionDescriptor.h"
#include "CSSFunctionValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Angle.h"
#include "CSSPropertyParserConsumer+Background.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSShadowValue.h"
#include "CSSToLengthConversionData.h"
#include "CSSTokenizer.h"
#include "CSSValueKeywords.h"
#include "FilterOperations.h"
#include "FilterOperationsBuilder.h"
#include <wtf/text/StringView.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<CSSValueID filterFunction> static constexpr bool isAllowed(AllowedFilterFunctions allowedFunctions)
{
    switch (allowedFunctions) {
    case AllowedFilterFunctions::PixelFilters:
        return isPixelFilterFunction<filterFunction>();
    case AllowedFilterFunctions::ColorFilters:
        return isColorFilterFunction<filterFunction>();
    }

    ASSERT_NOT_REACHED();
    return false;
}

template<CSSValueID filterFunction> static RefPtr<CSSValue> consumeNumberOrPercentFilterParameter(CSSParserTokenRange& args, const CSSParserContext& context)
{
    if (RefPtr percentage = consumePercentage(args, context, ValueRange::NonNegative)) {
        if (!filterFunctionAllowsValuesGreaterThanOne<filterFunction>() && percentage->resolveAsPercentageIfNotCalculated() > 100.0)
            percentage = CSSPrimitiveValue::create(100.0, CSSUnitType::CSS_PERCENTAGE);
        return percentage;
    } else if (RefPtr number = consumeNumber(args, context, ValueRange::NonNegative)) {
        if (!filterFunctionAllowsValuesGreaterThanOne<filterFunction>() && number->resolveAsNumberIfNotCalculated() > 1.0)
            number = CSSPrimitiveValue::create(1.0, CSSUnitType::CSS_NUMBER);
        return number;
    }
    return nullptr;
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionBlur(CSSParserTokenRange& range, const CSSParserContext&, AllowedFilterFunctions allowedFunctions)
{
    // blur() = blur( <length>? )

    constexpr CSSValueID function = CSSValueBlur;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeLength(args, HTMLStandardMode, ValueRange::NonNegative);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionBrightness(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // brightness() = brightness( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueBrightness;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionContrast(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // contrast() = contrast( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueContrast;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSValue> consumeDropShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <drop-shadow> = [ <color>? && <length>{2,3} ]

    RefPtr<CSSPrimitiveValue> color;
    RefPtr<CSSPrimitiveValue> horizontalOffset;
    RefPtr<CSSPrimitiveValue> verticalOffset;
    RefPtr<CSSPrimitiveValue> standardDeviation;

    auto consumeOptionalColor = [&] -> bool {
        if (color)
            return false;
        color = consumeColor(range, context);
        return !!color;
    };

    auto consumeLengths = [&] -> bool {
        if (horizontalOffset)
            return false;
        horizontalOffset = consumeLength(range, context);
        if (!horizontalOffset)
            return false;
        verticalOffset = consumeLength(range, context);
        if (!verticalOffset)
            return false;
        // FIXME: the CSS filters spec does not say this should be non-negative, but tests currently rely on this.
        standardDeviation = consumeLength(range, context, ValueRange::NonNegative);
        return true;
    };

    while (!range.atEnd()) {
        if (consumeOptionalColor() || consumeLengths())
            continue;
        break;
    }

    if (!verticalOffset)
        return nullptr;
    return CSSShadowValue::create(WTFMove(horizontalOffset), WTFMove(verticalOffset), WTFMove(standardDeviation), nullptr, nullptr, WTFMove(color));
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionDropShadow(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // drop-shadow() = drop-shadow( [ <color>? && <length>{2,3} ] )

    constexpr CSSValueID function = CSSValueDropShadow;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);

    auto parsedValue = consumeDropShadow(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;

    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionGrayscale(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // grayscale() = grayscale( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueGrayscale;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionHueRotate(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // hue-rotate() = hue-rotate( [ <angle> | <zero> ]? )

    constexpr CSSValueID function = CSSValueHueRotate;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeAngle(args, context, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
    if (!parsedValue || !args.atEnd())
        return nullptr;

    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionInvert(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // invert() = invert( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueInvert;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionOpacity(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // opacity() = opacity( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueOpacity;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionSaturate(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // saturate() = saturate( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueSaturate;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionSepia(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // sepia() = sepia( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueSepia;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionAppleInvertLightness(CSSParserTokenRange& range, const CSSParserContext&, AllowedFilterFunctions allowedFunctions)
{
    // <-apple-invert-lightness()> = -apple-invert-lightness()

    constexpr CSSValueID function = CSSValueAppleInvertLightness;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto args = consumeFunction(range);
    if (!args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function);
}

// MARK: <filter-function>
// https://drafts.fxtf.org/filter-effects/#typedef-filter-function

static RefPtr<CSSFunctionValue> consumeFilterFunction(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // <filter-function> = <blur()> | <brightness()> | <contrast()> | <drop-shadow()> | <grayscale()> | <hue-rotate()> | <invert()> | <opacity()> | <sepia()> | <saturate()>
    //
    // On top of the standard functions above, we add `... | <-apple-invert-lightness()>`.

    auto rangeCopy = range;

    RefPtr<CSSFunctionValue> function;
    switch (rangeCopy.peek().functionId()) {
    case CSSValueBlur:
        function = consumeFilterFunctionBlur(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueBrightness:
        function = consumeFilterFunctionBrightness(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueContrast:
        function = consumeFilterFunctionContrast(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueDropShadow:
        function = consumeFilterFunctionDropShadow(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueGrayscale:
        function = consumeFilterFunctionGrayscale(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueHueRotate:
        function = consumeFilterFunctionHueRotate(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueInvert:
        function = consumeFilterFunctionInvert(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueOpacity:
        function = consumeFilterFunctionOpacity(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueSaturate:
        function = consumeFilterFunctionSaturate(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueSepia:
        function = consumeFilterFunctionSepia(rangeCopy, context, allowedFunctions);
        break;
    case CSSValueAppleInvertLightness:
        function = consumeFilterFunctionAppleInvertLightness(rangeCopy, context, allowedFunctions);
        break;
    default:
        return nullptr;
    }

    if (function)
        range = rangeCopy;
    return function;
}

// MARK: <filter-value-list>
// https://drafts.fxtf.org/filter-effects/#typedef-filter-value-list

RefPtr<CSSValue> consumeFilterValueList(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // <filter-value-list> = [ <filter-function> | <url> ]+

    auto rangeCopy = range;

    bool referenceFiltersAllowed = allowedFunctions == AllowedFilterFunctions::PixelFilters;
    CSSValueListBuilder list;
    do {
        RefPtr<CSSValue> filterValue = referenceFiltersAllowed ? consumeURL(rangeCopy) : nullptr;
        if (!filterValue) {
            filterValue = consumeFilterFunction(rangeCopy, context, allowedFunctions);
            if (!filterValue)
                return nullptr;
        }
        list.append(filterValue.releaseNonNull());
    } while (!rangeCopy.atEnd());

    range = rangeCopy;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeFilterValueListOrNone(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeFilterValueList(range, context, allowedFunctions);
}

std::optional<FilterOperations> parseFilterValueListOrNoneRaw(const String& string, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions, const Document& document, RenderStyle& style)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto parsedValue = consumeFilterValueListOrNone(range, context, allowedFunctions);
    if (!parsedValue)
        return { };

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    CSSToLengthConversionData conversionData { style, nullptr, nullptr, nullptr };
    return Style::createFilterOperations(document, style, conversionData, *parsedValue);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
