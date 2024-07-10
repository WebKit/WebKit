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

#include "config.h"
#include "CSSPropertyParserConsumer+Filter.h"

#include "CSSFilterFunctionDescriptor.h"
#include "CSSFunctionValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Angle.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+Percent.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSTokenizer.h"
#include "CSSValueKeywords.h"
#include "StyleFilterOperations.h"
#include "StyleFilterOperationsBuilder.h"
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

template<CSSValueID filterFunction> static RefPtr<CSSValue> consumeNumberOrPercentFilterParameter(CSSParserTokenRange& args, const CSSParserContext&)
{
    RefPtr primitiveValue = consumePercent(args, ValueRange::NonNegative);
    if (!primitiveValue)
        primitiveValue = consumeNumber(args, ValueRange::NonNegative);

    if (primitiveValue && !filterFunctionAllowsValuesGreaterThanOne<filterFunction>()) {
        bool isPercentage = primitiveValue->isPercentage();
        double maxAllowed = isPercentage ? 100.0 : 1.0;
        if (primitiveValue->doubleValue() > maxAllowed)
            primitiveValue = CSSPrimitiveValue::create(maxAllowed, isPercentage ? CSSUnitType::CSS_PERCENTAGE : CSSUnitType::CSS_NUMBER);
    }

    return primitiveValue;
}


static RefPtr<CSSFunctionValue> consumeFilterFunctionBlur(CSSParserTokenRange& args, const CSSParserContext&, AllowedFilterFunctions allowedFunctions)
{
    // blur() = blur( <length>? )

    constexpr CSSValueID function = CSSValueBlur;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeLength(args, HTMLStandardMode, ValueRange::NonNegative);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionBrightness(CSSParserTokenRange& args, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // brightness() = brightness( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueBrightness;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionContrast(CSSParserTokenRange& args, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // contrast() = contrast( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueContrast;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionDropShadow(CSSParserTokenRange& args, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // drop-shadow() = drop-shadow( [ <color>? && <length>{2,3} ] )

    constexpr CSSValueID function = CSSValueDropShadow;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    auto parsedValue = consumeSingleShadow(args, context, false, false);
    if (!parsedValue || !args.atEnd())
        return nullptr;

    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionGrayscale(CSSParserTokenRange& args, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // grayscale() = grayscale( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueGrayscale;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionHueRotate(CSSParserTokenRange& args, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // hue-rotate() = hue-rotate( [ <angle> | <zero> ]? )

    constexpr CSSValueID function = CSSValueHueRotate;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
    if (!parsedValue || !args.atEnd())
        return nullptr;

    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionInvert(CSSParserTokenRange& args, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // invert() = invert( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueInvert;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionOpacity(CSSParserTokenRange& args, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // opacity() = opacity( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueOpacity;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionSaturate(CSSParserTokenRange& args, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // saturate() = saturate( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueSaturate;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionSepia(CSSParserTokenRange& args, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    // sepia() = sepia( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueSepia;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

    if (args.atEnd())
        return CSSFunctionValue::create(function);

    auto parsedValue = consumeNumberOrPercentFilterParameter<function>(args, context);
    if (!parsedValue || !args.atEnd())
        return nullptr;
    return CSSFunctionValue::create(function, parsedValue.releaseNonNull());
}

static RefPtr<CSSFunctionValue> consumeFilterFunctionAppleInvertLightness(CSSParserTokenRange& args, const CSSParserContext&, AllowedFilterFunctions allowedFunctions)
{
    // <-apple-invert-lightness()> = -apple-invert-lightness()

    constexpr CSSValueID function = CSSValueAppleInvertLightness;

    if (!isAllowed<function>(allowedFunctions))
        return nullptr;

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

    auto functionId = rangeCopy.peek().functionId();
    auto args = consumeFunction(rangeCopy);

    RefPtr<CSSFunctionValue> function;
    switch (functionId) {
    case CSSValueBlur:
        function = consumeFilterFunctionBlur(args, context, allowedFunctions);
        break;
    case CSSValueBrightness:
        function = consumeFilterFunctionBrightness(args, context, allowedFunctions);
        break;
    case CSSValueContrast:
        function = consumeFilterFunctionContrast(args, context, allowedFunctions);
        break;
    case CSSValueDropShadow:
        function = consumeFilterFunctionDropShadow(args, context, allowedFunctions);
        break;
    case CSSValueGrayscale:
        function = consumeFilterFunctionGrayscale(args, context, allowedFunctions);
        break;
    case CSSValueHueRotate:
        function = consumeFilterFunctionHueRotate(args, context, allowedFunctions);
        break;
    case CSSValueInvert:
        function = consumeFilterFunctionInvert(args, context, allowedFunctions);
        break;
    case CSSValueOpacity:
        function = consumeFilterFunctionOpacity(args, context, allowedFunctions);
        break;
    case CSSValueSaturate:
        function = consumeFilterFunctionSaturate(args, context, allowedFunctions);
        break;
    case CSSValueSepia:
        function = consumeFilterFunctionSepia(args, context, allowedFunctions);
        break;
    case CSSValueAppleInvertLightness:
        function = consumeFilterFunctionAppleInvertLightness(args, context, allowedFunctions);
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

static std::optional<FilterOperations> consumeFilterValueListOrNoneRaw(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions, const CSSUnresolvedFilterResolutionContext& eagerResolutionContext)
{
    if (RefPtr filter = consumeFilterValueListOrNone(range, context, allowedFunctions))
        return Style::createFilterOperations(*filter, eagerResolutionContext);
    return { };
}

std::optional<FilterOperations> parseFilterValueListOrNoneRaw(const String& string, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions, const CSSUnresolvedFilterResolutionContext& eagerResolutionContext)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto result = consumeFilterValueListOrNoneRaw(range, context, allowedFunctions, eagerResolutionContext);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return result;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
