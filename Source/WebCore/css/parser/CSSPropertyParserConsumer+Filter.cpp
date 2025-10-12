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

#include "CSSAppleColorFilterValue.h"
#include "CSSFilterFunctionDescriptor.h"
#include "CSSFilterValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+Background.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserState.h"
#include "CSSToLengthConversionData.h"
#include "CSSTokenizer.h"
#include "CSSValueKeywords.h"
#include "FilterOperations.h"
#include "StyleBuilderState.h"
#include "StyleFilter.h"
#include <wtf/text/StringView.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<CSSValueID filterFunction> static decltype(auto) consumeNumberOrPercentFilterParameter(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    if constexpr (filterFunctionAllowsValuesGreaterThanOne<filterFunction>()) {
        return MetaConsumer<
            CSS::Number<CSS::Nonnegative>,
            CSS::Percentage<CSS::Nonnegative>
        >::consume(args, state, { });
    } else {
        return MetaConsumer<
            CSS::Number<CSS::ClosedUnitRangeClampUpper>,
            CSS::Percentage<CSS::ClosedPercentageRangeClampUpper>
        >::consume(args, state, { });
    }
}

static std::optional<CSS::AppleInvertLightnessFunction> consumeFilterAppleInvertLightness(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <-apple-invert-lightness()> = -apple-invert-lightness()
    // Non-standard

    auto args = consumeFunction(range);
    if (!args.atEnd())
        return { };

    return CSS::AppleInvertLightnessFunction { .parameters = { } };
}

static std::optional<CSS::BlurFunction> consumeFilterBlur(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // blur() = blur( <length [0,∞]>? )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-blur

    auto args = consumeFunction(range);
    if (args.atEnd())
        return { CSS::BlurFunction { .parameters = { } } };

    auto parsedValue = MetaConsumer<CSS::Length<CSS::Nonnegative>>::consume(args, state);
    if (!parsedValue || !args.atEnd())
        return { };

    return CSS::BlurFunction { .parameters = { CSS::Blur::Parameter { WTFMove(*parsedValue) } } };
}

static std::optional<CSS::BrightnessFunction> consumeFilterBrightness(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // brightness() = brightness( [ <number [0,∞]> | <percentage [0,∞]> ]? )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-brightness

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSS::BrightnessFunction { .parameters = { } };

    auto parsedValue = consumeNumberOrPercentFilterParameter<CSS::BrightnessFunction::name>(args, state);
    if (!parsedValue || !args.atEnd())
        return { };

    return CSS::BrightnessFunction { .parameters = { CSS::Brightness::Parameter { WTFMove(*parsedValue) } } };
}

static std::optional<CSS::ContrastFunction> consumeFilterContrast(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // contrast() = contrast( [ <number [0,∞]> | <percentage [0,∞]> ]? )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-contrast

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSS::ContrastFunction { .parameters = { } };

    auto parsedValue = consumeNumberOrPercentFilterParameter<CSS::ContrastFunction::name>(args, state);
    if (!parsedValue || !args.atEnd())
        return { };

    return CSS::ContrastFunction { .parameters = { CSS::Contrast::Parameter { WTFMove(*parsedValue) } } };
}

static std::optional<CSS::DropShadowFunction> consumeFilterDropShadow(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // drop-shadow() = drop-shadow( [ <color>? && [<length>{2} <length [0,∞]>?] ] )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-drop-shadow

    auto args = consumeFunction(range);

    std::optional<CSS::Color> color;
    std::optional<CSS::Length<>> x;
    std::optional<CSS::Length<>> y;
    std::optional<CSS::Length<CSS::Nonnegative>> stdDeviation;

    auto consumeOptionalColor = [&] -> bool {
        if (color)
            return false;
        auto maybeColor = consumeUnresolvedColor(args, state);
        if (!maybeColor)
            return false;
        color = WTFMove(*maybeColor);
        return true;
    };

    auto consumeLengths = [&] -> bool {
        if (x)
            return false;
        x = MetaConsumer<CSS::Length<>>::consume(args, state);
        if (!x)
            return false;
        y = MetaConsumer<CSS::Length<>>::consume(args, state);
        if (!y)
            return false;

        stdDeviation = MetaConsumer<CSS::Length<CSS::Nonnegative>>::consume(args, state);
        return true;
    };

    while (!args.atEnd()) {
        if (consumeOptionalColor() || consumeLengths())
            continue;
        break;
    }

    if (!y || !args.atEnd())
        return { };

    return CSS::DropShadowFunction {
        .parameters = {
            .color = WTFMove(color),
            .location = { WTFMove(*x), WTFMove(*y) },
            .stdDeviation = WTFMove(stdDeviation)
        }
    };
}

static std::optional<CSS::GrayscaleFunction> consumeFilterGrayscale(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // grayscale() = grayscale( [ <number [0,1(clamp upper)] > | <percentage [0,100(clamp upper)]> ]? )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-grayscale

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSS::GrayscaleFunction { .parameters = { } };

    auto parsedValue = consumeNumberOrPercentFilterParameter<CSS::GrayscaleFunction::name>(args, state);
    if (!parsedValue || !args.atEnd())
        return { };

    return CSS::GrayscaleFunction { .parameters = { CSS::Grayscale::Parameter { WTFMove(*parsedValue) } } };
}

static std::optional<CSS::HueRotateFunction> consumeFilterHueRotate(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // hue-rotate() = hue-rotate( [ <angle> | <zero> ]? )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-hue-rotate

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSS::HueRotateFunction { .parameters = { } };

    auto parsedValue = MetaConsumer<CSS::Angle<>>::consume(args, state, { .unitlessZeroAngle = UnitlessZeroQuirk::Allow });
    if (!parsedValue || !args.atEnd())
        return { };

    return CSS::HueRotateFunction { .parameters = { CSS::HueRotate::Parameter { WTFMove(*parsedValue) } } };
}

static std::optional<CSS::InvertFunction> consumeFilterInvert(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // invert() = invert( [ <number [0,1(clamp upper)] > | <percentage [0,100(clamp upper)]> ]? )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-invert

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSS::InvertFunction { .parameters = { } };

    auto parsedValue = consumeNumberOrPercentFilterParameter<CSS::InvertFunction::name>(args, state);
    if (!parsedValue || !args.atEnd())
        return { };

    return CSS::InvertFunction { .parameters = { CSS::Invert::Parameter { WTFMove(*parsedValue) } } };
}

static std::optional<CSS::OpacityFunction> consumeFilterOpacity(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // opacity() = opacity( [ <number [0,1(clamp upper)] > | <percentage [0,100(clamp upper)]> ]? )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-opacity

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSS::OpacityFunction { .parameters = { } };

    auto parsedValue = consumeNumberOrPercentFilterParameter<CSS::OpacityFunction::name>(args, state);
    if (!parsedValue || !args.atEnd())
        return { };

    return CSS::OpacityFunction { .parameters = { CSS::Opacity::Parameter { WTFMove(*parsedValue) } } };
}

static std::optional<CSS::SaturateFunction> consumeFilterSaturate(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // saturate() = saturate( [ <number [0,∞]> | <percentage [0,∞]> ]? )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-saturate

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSS::SaturateFunction { .parameters = { } };

    auto parsedValue = consumeNumberOrPercentFilterParameter<CSS::SaturateFunction::name>(args, state);
    if (!parsedValue || !args.atEnd())
        return { };

    return CSS::SaturateFunction { .parameters = { CSS::Saturate::Parameter { WTFMove(*parsedValue) } } };
}

static std::optional<CSS::SepiaFunction> consumeFilterSepia(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // sepia() = sepia( [ <number [0,1(clamp upper)] > | <percentage [0,100(clamp upper)]> ]? )
    // https://drafts.fxtf.org/filter-effects/#funcdef-filter-sepia

    auto args = consumeFunction(range);
    if (args.atEnd())
        return CSS::SepiaFunction { .parameters = { } };

    auto parsedValue = consumeNumberOrPercentFilterParameter<CSS::SepiaFunction::name>(args, state);
    if (!parsedValue || !args.atEnd())
        return { };

    return CSS::SepiaFunction { .parameters = { CSS::Sepia::Parameter { WTFMove(*parsedValue) } } };
}

static std::optional<CSS::FilterValueList> consumeUnresolvedFilterValueList(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <filter-value-list> = [ <filter-function> | <url> ]+
    // <filter-function> = <blur()> | <brightness()> | <contrast()> | <drop-shadow()> | <grayscale()> | <hue-rotate()> | <invert()> | <opacity()> | <sepia()> | <saturate()>
    // https://drafts.fxtf.org/filter-effects/#typedef-filter-value-list

    auto rangeCopy = range;

    CSS::FilterValueList list;

    auto appendOnSuccess = [&](auto&& value) -> bool {
        if (!value)
            return false;
        list.value.append(WTFMove(*value));
        return true;
    };

    do {
        if (auto url = consumeURLRaw(rangeCopy, state, { })) {
            list.value.append(CSS::FilterReference { WTFMove(*url) });
            continue;
        }

        switch (rangeCopy.peek().functionId()) {
        case CSSValueBlur:
            if (!appendOnSuccess(consumeFilterBlur(rangeCopy, state)))
                return { };
            break;
        case CSSValueBrightness:
            if (!appendOnSuccess(consumeFilterBrightness(rangeCopy, state)))
                return { };
            break;
        case CSSValueContrast:
            if (!appendOnSuccess(consumeFilterContrast(rangeCopy, state)))
                return { };
            break;
        case CSSValueDropShadow:
            if (!appendOnSuccess(consumeFilterDropShadow(rangeCopy, state)))
                return { };
            break;
        case CSSValueGrayscale:
            if (!appendOnSuccess(consumeFilterGrayscale(rangeCopy, state)))
                return { };
            break;
        case CSSValueHueRotate:
            if (!appendOnSuccess(consumeFilterHueRotate(rangeCopy, state)))
                return { };
            break;
        case CSSValueInvert:
            if (!appendOnSuccess(consumeFilterInvert(rangeCopy, state)))
                return { };
            break;
        case CSSValueOpacity:
            if (!appendOnSuccess(consumeFilterOpacity(rangeCopy, state)))
                return { };
            break;
        case CSSValueSaturate:
            if (!appendOnSuccess(consumeFilterSaturate(rangeCopy, state)))
                return { };
            break;
        case CSSValueSepia:
            if (!appendOnSuccess(consumeFilterSepia(rangeCopy, state)))
                return { };
            break;
        default:
            return { };
        }
    } while (!rangeCopy.atEnd());

    range = rangeCopy;

    return { WTFMove(list) };
}

std::optional<CSS::Filter> consumeUnresolvedFilter(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::Filter { CSS::Keyword::None { } };
    }
    if (auto filterValueList = consumeUnresolvedFilterValueList(range, state))
        return CSS::Filter { WTFMove(*filterValueList) };
    return { };
}

RefPtr<CSSValue> consumeFilter(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto property = consumeUnresolvedFilter(range, state))
        return CSSFilterValue::create({ WTFMove(*property) });
    return nullptr;
}

// MARK: - <-apple-color-filter>

static std::optional<CSS::AppleColorFilter::List> consumeUnresolvedAppleColorFilterValueList(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <-apple-color-filter-value-list = <-apple-color-filter-function>+
    // <-apple-color-filter-function> = <-apple-invert-lightness() | <brightness()> | <contrast()> | <grayscale()> | <hue-rotate()> | <invert()> | <opacity()> | <sepia()> | <saturate()>

    auto rangeCopy = range;

    CSS::AppleColorFilter::List list;

    auto appendOnSuccess = [&](auto&& value) -> bool {
        if (!value)
            return false;
        list.value.append(WTFMove(*value));
        return true;
    };

    do {
        switch (rangeCopy.peek().functionId()) {
        case CSSValueAppleInvertLightness:
            if (!appendOnSuccess(consumeFilterAppleInvertLightness(rangeCopy, state)))
                return { };
            break;
        case CSSValueBrightness:
            if (!appendOnSuccess(consumeFilterBrightness(rangeCopy, state)))
                return { };
            break;
        case CSSValueContrast:
            if (!appendOnSuccess(consumeFilterContrast(rangeCopy, state)))
                return { };
            break;
        case CSSValueGrayscale:
            if (!appendOnSuccess(consumeFilterGrayscale(rangeCopy, state)))
                return { };
            break;
        case CSSValueHueRotate:
            if (!appendOnSuccess(consumeFilterHueRotate(rangeCopy, state)))
                return { };
            break;
        case CSSValueInvert:
            if (!appendOnSuccess(consumeFilterInvert(rangeCopy, state)))
                return { };
            break;
        case CSSValueOpacity:
            if (!appendOnSuccess(consumeFilterOpacity(rangeCopy, state)))
                return { };
            break;
        case CSSValueSaturate:
            if (!appendOnSuccess(consumeFilterSaturate(rangeCopy, state)))
                return { };
            break;
        case CSSValueSepia:
            if (!appendOnSuccess(consumeFilterSepia(rangeCopy, state)))
                return { };
            break;
        default:
            return { };
        }
    } while (!rangeCopy.atEnd());

    range = rangeCopy;

    return { WTFMove(list) };
}

std::optional<CSS::AppleColorFilter> consumeUnresolvedAppleColorFilter(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::AppleColorFilter { CSS::Keyword::None { } };
    }
    if (auto filterValueList = consumeUnresolvedAppleColorFilterValueList(range, state))
        return CSS::AppleColorFilter { WTFMove(*filterValueList) };
    return { };
}

RefPtr<CSSValue> consumeAppleColorFilter(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto property = consumeUnresolvedAppleColorFilter(range, state))
        return CSSAppleColorFilterValue::create({ WTFMove(*property) });
    return nullptr;
}

std::optional<Style::Filter> parseFilterValueListOrNoneRaw(const String& string, const CSSParserContext& context, const Document& document, RenderStyle& style)
{
    auto tokenizer = CSSTokenizer(string);
    auto range = tokenizer.tokenRange();

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto state = CSS::PropertyParserState { .context = context };
    auto filter = consumeUnresolvedFilter(range, state);
    if (!filter)
        return { };

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    auto builderState = Style::BuilderState::create(style, Style::BuilderContext { document });
    return Style::toStyle(*filter, *CheckedPtr { builderState.ptr() });
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
