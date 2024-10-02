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
#include "CSSPropertyParserConsumer+Image.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCanvasValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFilterImageValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetOptionValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSNamedImageValue.h"
#include "CSSPaintImageValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+AnglePercentageDefinitions.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+ColorInterpolationMethod.h"
#include "CSSPropertyParserConsumer+Filter.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+MetaResolver.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Position.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserOptions.h"
#include "CSSValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "StyleImage.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: Deprecated <gradient> values

template<CSSValueID zeroValue, CSSValueID oneHundredValue> static std::optional<CSS::PercentageOrNumber> consumeDeprecatedGradientPositionComponent(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().type() == IdentToken) {
        if (consumeIdent<zeroValue>(range))
            return CSS::PercentageOrNumber { CSS::Percentage { CSS::PercentageRaw { 0 } } };
        if (consumeIdent<oneHundredValue>(range))
            return CSS::PercentageOrNumber { CSS::Percentage { CSS::PercentageRaw { 100 } } };
        if (consumeIdent<CSSValueCenter>(range))
            return CSS::PercentageOrNumber { CSS::Percentage { CSS::PercentageRaw { 50 } } };
        return std::nullopt;
    }
    return MetaConsumer<CSS::Percentage, CSS::Number>::consume(range, context, { }, { });
}

static std::optional<CSSDeprecatedGradientPosition> consumeDeprecatedGradientPosition(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto horizontal = consumeDeprecatedGradientPositionComponent<CSSValueLeft, CSSValueRight>(range, context);
    if (!horizontal)
        return std::nullopt;

    auto vertical = consumeDeprecatedGradientPositionComponent<CSSValueTop, CSSValueBottom>(range, context);
    if (!vertical)
        return std::nullopt;

    return { { WTFMove(*horizontal), WTFMove(*vertical) } };
}

static RefPtr<CSSPrimitiveValue> consumeDeprecatedGradientStopColor(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueCurrentcolor)
        return nullptr;
    return consumeColor(range, context);
}

static std::optional<CSSGradientDeprecatedColorStop> consumeDeprecatedGradientColorStop(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto id = range.peek().functionId();
    switch (id) {
    case CSSValueFrom:
    case CSSValueTo:
    case CSSValueColorStop:
        break;

    default:
        return std::nullopt;
    }

    auto args = consumeFunction(range);
    std::optional<CSSGradientDeprecatedColorStopPosition> position;
    switch (id) {
    case CSSValueFrom:
        position = CSS::NumberRaw { 0 };
        break;
    case CSSValueTo:
        position = CSS::NumberRaw { 1 };
        break;
    case CSSValueColorStop:
        position = MetaConsumer<CSS::Percentage, CSS::Number>::consume(args, context, { }, { });
        if (!position)
            return std::nullopt;
        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;
        break;
    default:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    auto color = consumeDeprecatedGradientStopColor(args, context);
    if (!color || !args.atEnd())
        return std::nullopt;

    return CSSGradientDeprecatedColorStop {
        .color = WTFMove(color),
        .position = WTFMove(*position)
    };
}

static std::optional<CSSGradientDeprecatedColorStopList> consumeDeprecatedGradientColorStops(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSGradientDeprecatedColorStopList stops;
    while (consumeCommaIncludingWhitespace(range)) {
        auto stop = consumeDeprecatedGradientColorStop(range, context);
        if (!stop)
            return std::nullopt;
        stops.append(WTFMove(*stop));
    }
    stops.shrinkToFit();

    return { WTFMove(stops) };
}

static RefPtr<CSSValue> consumeDeprecatedLinearGradient(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto first = consumeDeprecatedGradientPosition(range, context);
    if (!first)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto second = consumeDeprecatedGradientPosition(range, context);
    if (!second)
        return nullptr;

    auto stops = consumeDeprecatedGradientColorStops(range, context);
    if (!stops)
        return nullptr;

    return CSSDeprecatedLinearGradientValue::create({
            WTFMove(*first),
            WTFMove(*second),
        },
        CSSGradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
        WTFMove(*stops)
    );
}

static RefPtr<CSSValue> consumeDeprecatedRadialGradient(CSSParserTokenRange& range, const CSSParserContext& context)
{
    constexpr auto radiusConsumeOptions = CSSPropertyParserOptions {
        .valueRange = ValueRange::NonNegative
    };

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto first = consumeDeprecatedGradientPosition(range, context);
    if (!first)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto firstRadius = MetaConsumer<CSS::Number>::consume(range, context, { }, radiusConsumeOptions);
    if (!firstRadius)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto second = consumeDeprecatedGradientPosition(range, context);
    if (!second)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto secondRadius = MetaConsumer<CSS::Number>::consume(range, context, { }, radiusConsumeOptions);
    if (!secondRadius)
        return nullptr;

    auto stops = consumeDeprecatedGradientColorStops(range, context);
    if (!stops)
        return nullptr;

    return CSSDeprecatedRadialGradientValue::create({
            WTFMove(*first),
            WTFMove(*second),
            WTFMove(*firstRadius),
            WTFMove(*secondRadius)
        },
        CSSGradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
        WTFMove(*stops)
    );
}

static RefPtr<CSSValue> consumeDeprecatedGradient(CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (range.consumeIncludingWhitespace().id()) {
    case CSSValueLinear:
        return consumeDeprecatedLinearGradient(range, context);
    case CSSValueRadial:
        return consumeDeprecatedRadialGradient(range, context);
    default:
        return nullptr;
    }
}

// MARK: <color-stop-list> | <angular-color-stop-list>
// https://drafts.csswg.org/css-images-4/#typedef-color-stop-list

enum class SupportsColorHints : bool { No, Yes };

template<SupportsColorHints supportsColorHints, typename Stop, typename Consumer> static std::optional<CSSGradientColorStopList<Stop>> consumeColorStopList(CSSParserTokenRange& range, const CSSParserContext& context, Consumer&& consumeStopPosition)
{
    CSSGradientColorStopList<Stop> stops;

    // The first color stop cannot be a color hint.
    bool previousStopWasColorHint = true;
    do {
        Stop stop { consumeColor(range, context), consumeStopPosition(range) };
        if (!stop.color && !stop.position)
            return std::nullopt;

        // Two color hints in a row are not allowed.
        if (!stop.color && (supportsColorHints == SupportsColorHints::No || previousStopWasColorHint))
            return std::nullopt;
        previousStopWasColorHint = !stop.color;

        // Stops with both a color and a position can have a second position, which shares the same color.
        if (stop.color && stop.position) {
            if (auto secondPosition = consumeStopPosition(range)) {
                stops.append(stop);
                stop.position = WTFMove(secondPosition);
            }
        }
        stops.append(WTFMove(stop));
    } while (consumeCommaIncludingWhitespace(range));

    // The last color stop cannot be a color hint.
    if (previousStopWasColorHint)
        return std::nullopt;

    // Must have two or more stops to be valid.
    if (stops.size() < 2)
        return std::nullopt;

    stops.shrinkToFit();

    return { WTFMove(stops) };
}

template<SupportsColorHints supportsColorHints> static std::optional<CSSGradientLinearColorStopList> consumeLinearColorStopList(CSSParserTokenRange& range, const CSSParserContext& context)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitless = UnitlessQuirk::Forbid,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    return consumeColorStopList<supportsColorHints, CSSGradientLinearColorStop>(range, context, [&](auto& range) {
        return MetaConsumer<CSS::LengthPercentage>::consume(range, context, { }, options);
    });
}

template<SupportsColorHints supportsColorHints> static std::optional<CSSGradientAngularColorStopList> consumeAngularColorStopList(CSSParserTokenRange& range, const CSSParserContext& context)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitless = UnitlessQuirk::Forbid,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    return consumeColorStopList<supportsColorHints, CSSGradientAngularColorStop>(range, context, [&](auto& range) {
        return MetaConsumer<CSS::AnglePercentage>::consume(range, context, { }, options);
    });
}

template<typename Stop> static CSSGradientColorInterpolationMethod computeGradientColorInterpolationMethod(std::optional<ColorInterpolationMethod> parsedColorInterpolationMethod, const CSSGradientColorStopList<Stop>& stops)
{
    // We detect whether stops use legacy vs. non-legacy CSS color syntax using the following rules:
    //  - A CSSValueID is always considered legacy since all keyword based colors are considered legacy by the spec.
    //  - An actual Color value is considered legacy if it is stored as 8-bit sRGB.
    //
    // While this is accurate now, we should consider a more robust mechanism to detect this at parse
    // time, perhaps keeping this information in the CSSPrimitiveValue itself.

    auto defaultColorInterpolationMethod = CSSGradientColorInterpolationMethod::Default::SRGB;
    for (auto& stop : stops) {
        if (!stop.color)
            continue;
        if (stop.color->isValueID())
            continue;
        if (stop.color->isColor() && stop.color->color().tryGetAsSRGBABytes())
            continue;

        defaultColorInterpolationMethod = CSSGradientColorInterpolationMethod::Default::OKLab;
        break;
    }

    if (parsedColorInterpolationMethod)
        return { *parsedColorInterpolationMethod, defaultColorInterpolationMethod };

    switch (defaultColorInterpolationMethod) {
    case CSSGradientColorInterpolationMethod::Default::SRGB:
        return { { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Premultiplied }, defaultColorInterpolationMethod };

    case CSSGradientColorInterpolationMethod::Default::OKLab:
        return { { ColorInterpolationMethod::OKLab { }, AlphaPremultiplication::Premultiplied }, defaultColorInterpolationMethod };
    }

    ASSERT_NOT_REACHED();
    return { { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Premultiplied }, defaultColorInterpolationMethod };
}

// MARK: Compat <gradient> values
// https://compat.spec.whatwg.org/#css-gradient-fns

// MARK: <-webkit-linear-gradient()> | <-webkit-repeating-linear-gradient()>
// https://compat.spec.whatwg.org/#css-gradients-webkit-linear-gradient

static RefPtr<CSSValue> consumePrefixedLinearGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // https://compat.spec.whatwg.org/#css-gradients-webkit-linear-gradient/ states that -webkit-linear-gradient() and
    // -webkit-repeating-linear-gradient() must be "treated as an alias of linear-gradient as defined in [css3-images-20110217]."
    // In [css3-images-20110217] the grammar was defined as:
    //
    //   <linear-gradient> = linear-gradient([ [ [top | bottom] || [left | right] ] | <angle> ,]? <color-stop>[, <color-stop>]+);
    //
    // see https://www.w3.org/TR/2011/WD-css3-images-20110217/#linear-gradients.

    static constexpr std::pair<CSSValueID, CSSPrefixedLinearGradientValue::Vertical> verticalMappings[] {
        { CSSValueTop, CSSPrefixedLinearGradientValue::Vertical::Top },
        { CSSValueBottom, CSSPrefixedLinearGradientValue::Vertical::Bottom },
    };
    static constexpr SortedArrayMap verticalMap { verticalMappings };

    static constexpr std::pair<CSSValueID, CSSPrefixedLinearGradientValue::Horizontal> horizontalMappings[] {
        { CSSValueLeft, CSSPrefixedLinearGradientValue::Horizontal::Left },
        { CSSValueRight, CSSPrefixedLinearGradientValue::Horizontal::Right },
    };
    static constexpr SortedArrayMap horizontalMap { horizontalMappings };

    auto consumeKeywordGradientLineKnownHorizontal = [&](CSSParserTokenRange& range, CSSPrefixedLinearGradientValue::Horizontal knownHorizontal) -> CSSPrefixedLinearGradientValue::GradientLine {
        if (auto vertical = consumeIdentUsingMapping(range, verticalMap))
            return std::make_pair(knownHorizontal, *vertical);
        return knownHorizontal;
    };

    auto consumeKeywordGradientLineKnownVertical = [&](CSSParserTokenRange& range, CSSPrefixedLinearGradientValue::Vertical knownVertical) -> CSSPrefixedLinearGradientValue::GradientLine {
        if (auto horizontal = consumeIdentUsingMapping(range, horizontalMap))
            return std::make_pair(*horizontal, knownVertical);
        return knownVertical;
    };

    auto consumeKeywordGradientLine = [&](CSSParserTokenRange& range) -> std::optional<CSSPrefixedLinearGradientValue::GradientLine> {
        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSSPrefixedLinearGradientValue::Horizontal::Left);
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSSPrefixedLinearGradientValue::Horizontal::Right);
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSSPrefixedLinearGradientValue::Vertical::Top);
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSSPrefixedLinearGradientValue::Vertical::Bottom);
        default:
            return { };
        }
    };

    std::optional<CSSPrefixedLinearGradientValue::GradientLine> gradientLine;

    const auto angleConsumeOptions = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitless = UnitlessQuirk::Forbid,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    if (auto angle = MetaConsumer<CSS::Angle>::consume(range, context, { }, angleConsumeOptions)) {
        gradientLine = WTF::switchOn(WTFMove(angle->value), [](auto&& value) -> CSSPrefixedLinearGradientValue::GradientLine { return value; });
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    } else if (auto keywordGradientLine = consumeKeywordGradientLine(range)) {
        gradientLine = WTFMove(*keywordGradientLine);
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    auto stops = consumeLinearColorStopList<SupportsColorHints::No>(range, context);
    if (!stops)
        return nullptr;

    return CSSPrefixedLinearGradientValue::create(
        CSSPrefixedLinearGradientValue::Data { gradientLine.value_or(std::monostate { }) },
        repeating,
        CSSGradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
        WTFMove(*stops)
    );
}

// MARK: <-webkit-radial-gradient()> | <-webkit-repeating-radial-gradient()>
// https://compat.spec.whatwg.org/#css-gradients-webkit-radial-gradient

static RefPtr<CSSValue> consumePrefixedRadialGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // https://compat.spec.whatwg.org/#css-gradients-webkit-radial-gradient/ states that -webkit-radial-gradient() and
    // -webkit-repeating-radial-gradient() must be "treated as an alias of radial-gradient as defined in [css3-images-20110217]."
    // In [css3-images-20110217] the grammar was defined as:
    //
    //   <radial-gradient> = radial-gradient([<bg-position>,]? [ [<shape> || <size>] | [<length> | <percentage>]{2} ,]? <color-stop>[, <color-stop>]+);
    //   <shape> = [ circle | ellipse ]
    //   <size> = [ closest-side | closest-corner | farthest-side | farthest-corner | contain | cover ]
    //
    //      defaults to ‘ellipse cover’.
    //
    // see https://www.w3.org/TR/2011/WD-css3-images-20110217/#radial-gradients.

    static constexpr std::pair<CSSValueID, CSSPrefixedRadialGradientValue::ShapeKeyword> shapeMappings[] {
        { CSSValueCircle, CSSPrefixedRadialGradientValue::ShapeKeyword::Circle },
        { CSSValueEllipse, CSSPrefixedRadialGradientValue::ShapeKeyword::Ellipse },
    };
    static constexpr SortedArrayMap shapeMap { shapeMappings };

    static constexpr std::pair<CSSValueID, CSSPrefixedRadialGradientValue::ExtentKeyword> extentMappings[] {
        { CSSValueContain, CSSPrefixedRadialGradientValue::ExtentKeyword::Contain },
        { CSSValueCover, CSSPrefixedRadialGradientValue::ExtentKeyword::Cover },
        { CSSValueClosestSide, CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestSide },
        { CSSValueClosestCorner, CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestCorner },
        { CSSValueFarthestSide, CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestSide },
        { CSSValueFarthestCorner, CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestCorner },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    auto position = consumeOneOrTwoComponentPositionUnresolved(range, context);
    if (position) {
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    std::optional<CSSPrefixedRadialGradientValue::ShapeKeyword> shapeKeyword;
    std::optional<CSSPrefixedRadialGradientValue::ExtentKeyword> extentKeyword;
    if (range.peek().type() == IdentToken) {
        shapeKeyword = consumeIdentUsingMapping(range, shapeMap);
        extentKeyword = consumeIdentUsingMapping(range, extentMap);
        if (!shapeKeyword)
            shapeKeyword = consumeIdentUsingMapping(range, shapeMap);
        if (shapeKeyword || extentKeyword) {
            if (!consumeCommaIncludingWhitespace(range))
                return nullptr;
        }
    }

    CSSPrefixedRadialGradientValue::GradientBox gradientBox = std::monostate { };

    if (shapeKeyword && extentKeyword) {
        gradientBox = CSSPrefixedRadialGradientValue::ShapeAndExtent { *shapeKeyword, *extentKeyword };
    } else if (shapeKeyword) {
        gradientBox = *shapeKeyword;
    } else if (extentKeyword) {
        gradientBox = *extentKeyword;
    } else {
        const auto options = CSSPropertyParserOptions {
            .parserMode = context.mode,
            .valueRange = ValueRange::NonNegative,
            .unitlessZero = UnitlessZeroQuirk::Allow
        };

        if (auto length1 = MetaConsumer<CSS::LengthPercentage>::consume(range, context, { }, options)) {
            auto length2 = MetaConsumer<CSS::LengthPercentage>::consume(range, context, { }, options);
            if (!length2)
                return nullptr;
            if (!consumeCommaIncludingWhitespace(range))
                return nullptr;
            gradientBox = CSSPrefixedRadialGradientValue::MeasuredSize { { WTFMove(*length1), WTFMove(*length2) } };
        }
    }

    auto stops = consumeLinearColorStopList<SupportsColorHints::No>(range, context);
    if (!stops)
        return nullptr;

    return CSSPrefixedRadialGradientValue::create({
            WTFMove(gradientBox),
            WTFMove(position)
        },
        repeating,
        CSSGradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
        WTFMove(*stops)
    );
}

// MARK: Standard <gradient> values

// MARK: <linear-gradient()> | <repeating-linear-gradient()>
// https://drafts.csswg.org/css-images-4/#linear-gradients

static RefPtr<CSSValue> consumeLinearGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // <side-or-corner> = [left | right] || [top | bottom]
    // linear-gradient() = linear-gradient(
    //   [ <angle> | to <side-or-corner> ]? || <color-interpolation-method>,
    //   <color-stop-list>
    // )

    static constexpr std::pair<CSSValueID, CSSLinearGradientValue::Vertical> verticalMappings[] {
        { CSSValueTop, CSSLinearGradientValue::Vertical::Top },
        { CSSValueBottom, CSSLinearGradientValue::Vertical::Bottom },
    };
    static constexpr SortedArrayMap verticalMap { verticalMappings };

    static constexpr std::pair<CSSValueID, CSSLinearGradientValue::Horizontal> horizontalMappings[] {
        { CSSValueLeft, CSSLinearGradientValue::Horizontal::Left },
        { CSSValueRight, CSSLinearGradientValue::Horizontal::Right },
    };
    static constexpr SortedArrayMap horizontalMap { horizontalMappings };

    auto consumeKeywordGradientLineKnownHorizontal = [&](CSSParserTokenRange& range, CSSLinearGradientValue::Horizontal knownHorizontal) -> CSSLinearGradientValue::GradientLine {
        if (auto vertical = consumeIdentUsingMapping(range, verticalMap))
            return std::make_pair(knownHorizontal, *vertical);
        return knownHorizontal;
    };

    auto consumeKeywordGradientLineKnownVertical = [&](CSSParserTokenRange& range, CSSLinearGradientValue::Vertical knownVertical) -> CSSLinearGradientValue::GradientLine {
        if (auto horizontal = consumeIdentUsingMapping(range, horizontalMap))
            return std::make_pair(*horizontal, knownVertical);
        return knownVertical;
    };

    auto consumeKeywordGradientLine = [&](CSSParserTokenRange& range) -> std::optional<CSSLinearGradientValue::GradientLine> {
        ASSERT(range.peek().id() == CSSValueTo);
        range.consumeIncludingWhitespace();

        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSSLinearGradientValue::Horizontal::Left);
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSSLinearGradientValue::Horizontal::Right);
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSSLinearGradientValue::Vertical::Top);
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSSLinearGradientValue::Vertical::Bottom);
        default:
            return { };
        }
    };

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, context);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    std::optional<CSSLinearGradientValue::GradientLine> gradientLine;

    const auto angleConsumeOptions = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitless = UnitlessQuirk::Forbid,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    if (auto angle = MetaConsumer<CSS::Angle>::consume(range, context, { }, angleConsumeOptions))
        gradientLine = WTF::switchOn(WTFMove(angle->value), [](auto&& value) -> CSSLinearGradientValue::GradientLine { return value; });
    else if (range.peek().id() == CSSValueTo) {
        auto keywordGradientLine = consumeKeywordGradientLine(range);
        if (!keywordGradientLine)
            return nullptr;
        gradientLine = WTFMove(*keywordGradientLine);
    }

    if (gradientLine && !colorInterpolationMethod && range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, context);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    if (gradientLine || colorInterpolationMethod) {
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    auto stops = consumeLinearColorStopList<SupportsColorHints::Yes>(range, context);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(colorInterpolationMethod, *stops);

    return CSSLinearGradientValue::create(
        CSSLinearGradientValue::Data { gradientLine.value_or(std::monostate { }) },
        repeating,
        computedColorInterpolationMethod,
        WTFMove(*stops)
    );
}

// MARK: <radial-gradient()> | <repeating-radial-gradient()>
// https://drafts.csswg.org/css-images-4/#radial-gradients

static RefPtr<CSSValue> consumeRadialGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // radial-gradient() = radial-gradient(
    //   [[ <ending-shape> || <size> ]? [ at <position> ]? ] || <color-interpolation-method>,
    //   <color-stop-list>
    // )

    static constexpr std::pair<CSSValueID, CSSRadialGradientValue::ShapeKeyword> shapeMappings[] {
        { CSSValueCircle, CSSRadialGradientValue::ShapeKeyword::Circle },
        { CSSValueEllipse, CSSRadialGradientValue::ShapeKeyword::Ellipse },
    };
    static constexpr SortedArrayMap shapeMap { shapeMappings };

    static constexpr std::pair<CSSValueID, CSSRadialGradientValue::ExtentKeyword> extentMappings[] {
        { CSSValueClosestSide, CSSRadialGradientValue::ExtentKeyword::ClosestSide },
        { CSSValueClosestCorner, CSSRadialGradientValue::ExtentKeyword::ClosestCorner },
        { CSSValueFarthestSide, CSSRadialGradientValue::ExtentKeyword::FarthestSide },
        { CSSValueFarthestCorner, CSSRadialGradientValue::ExtentKeyword::FarthestCorner },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, context);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    std::optional<CSSRadialGradientValue::ShapeKeyword> shape;

    using Size = std::variant<CSSRadialGradientValue::ExtentKeyword, CSS::Length, CSS::SpaceSeparatedTuple<CSS::LengthPercentage, CSS::LengthPercentage>>;
    std::optional<Size> size;

    // First part of grammar, the size/shape clause:
    //
    //   [ [ circle               || <length [0,∞]> ]                          [ at <position> ]? , |
    //     [ ellipse              || <length-percentage [0,∞]>{2} ]            [ at <position> ]? , |
    //     [ [ circle | ellipse ] || <extent-keyword> ]                        [ at <position> ]? , |
    //     at <position> ,
    //   ]?
    for (int i = 0; i < 3; ++i) {
        if (range.peek().type() == IdentToken) {
            if (auto peekedShape = peekIdentUsingMapping(range, shapeMap)) {
                if (shape)
                    return nullptr;
                shape = *peekedShape;
                range.consumeIncludingWhitespace();
            } else if (auto peekedExtent = peekIdentUsingMapping(range, extentMap)) {
                if (size)
                    return nullptr;
                size = *peekedExtent;
                range.consumeIncludingWhitespace();
            }

            if (!shape && !size)
                break;
        } else {
            const auto options = CSSPropertyParserOptions {
                .parserMode = context.mode,
                .valueRange = ValueRange::NonNegative,
                .unitlessZero = UnitlessZeroQuirk::Allow
            };
            auto rangeCopy = range;
            auto length1 = MetaConsumer<CSS::LengthPercentage>::consume(rangeCopy, context, { }, options);
            if (!length1)
                break;
            if (size)
                return nullptr;
            if (auto length2 = MetaConsumer<CSS::LengthPercentage>::consume(rangeCopy, context, { }, options)) {
                size = CSS::SpaceSeparatedTuple(WTFMove(*length1), WTFMove(*length2));
                range = rangeCopy;

                // Additional increment is necessary since we consumed a second token.
                ++i;
            } else {
                // Reset to before the first length-percentage, and re-parse to make sure it is a valid <length [0,∞]> production.
                rangeCopy = range;
                auto length = MetaConsumer<CSS::Length>::consume(rangeCopy, context, { }, options);
                if (!length)
                    return nullptr;
                size = WTFMove(*length);
                range = rangeCopy;
            }
        }
    }

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueAt>(range)) {
        position = consumePositionUnresolved(range, context);
        if (!position)
            return nullptr;
    }

    if ((shape || size || position) && !colorInterpolationMethod && range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, context);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    if ((shape || size || position || colorInterpolationMethod) && !consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto stops = consumeLinearColorStopList<SupportsColorHints::Yes>(range, context);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(colorInterpolationMethod, *stops);

    CSSRadialGradientValue::Data data;
    if (shape) {
        switch (*shape) {
        case CSSRadialGradientValue::ShapeKeyword::Circle:
            if (!size)
                data.gradientBox = CSSRadialGradientValue::Shape { CSSRadialGradientValue::ShapeKeyword::Circle, WTFMove(position) };
            else {
                bool validSize = WTF::switchOn(WTFMove(*size),
                    [&](CSSRadialGradientValue::ExtentKeyword&& extent) -> bool {
                        data.gradientBox = CSSRadialGradientValue::CircleOfExtent { WTFMove(extent), WTFMove(position) };
                        return true;
                    },
                    [&](CSS::Length&& length) -> bool {
                        data.gradientBox = CSSRadialGradientValue::CircleOfLength { WTFMove(length), WTFMove(position) };
                        return true;
                    },
                    [&](CSS::SpaceSeparatedTuple<CSS::LengthPercentage, CSS::LengthPercentage>&&) -> bool {
                        return false;
                    }
                );
                if (!validSize)
                    return nullptr;
            }
            break;

        case CSSRadialGradientValue::ShapeKeyword::Ellipse:
            if (!size)
                data.gradientBox = CSSRadialGradientValue::Shape { CSSRadialGradientValue::ShapeKeyword::Ellipse, WTFMove(position) };
            else {
                bool validSize = WTF::switchOn(WTFMove(*size),
                    [&](CSSRadialGradientValue::ExtentKeyword&& extent) -> bool {
                        data.gradientBox = CSSRadialGradientValue::EllipseOfExtent { WTFMove(extent), WTFMove(position) };
                        return true;
                    },
                    [&](CSS::Length&&) -> bool {
                        // Ellipses must have two length-percentages specified.
                        return false;
                    },
                    [&](CSS::SpaceSeparatedTuple<CSS::LengthPercentage, CSS::LengthPercentage>&& size) -> bool {
                        data.gradientBox = CSSRadialGradientValue::EllipseOfSize { WTFMove(size), WTFMove(position) };
                        return true;
                    }
                );
                if (!validSize)
                    return nullptr;
            }
            break;
        }
    } else {
        if (!size) {
            if (position)
                data.gradientBox = CSSRadialGradientValue::GradientBox { std::in_place_type<CSS::Position>, WTFMove(*position) };
            else
                data.gradientBox = std::monostate { };
        } else {
            WTF::switchOn(WTFMove(*size),
                [&](CSSRadialGradientValue::ExtentKeyword&& extent) {
                    data.gradientBox = CSSRadialGradientValue::Extent { WTFMove(extent), WTFMove(position) };
                },
                [&](CSS::Length&& length) {
                    data.gradientBox = CSSRadialGradientValue::Length { WTFMove(length), WTFMove(position) };
                },
                [&](CSS::SpaceSeparatedTuple<CSS::LengthPercentage, CSS::LengthPercentage>&& size) {
                    data.gradientBox = CSSRadialGradientValue::Size { WTFMove(size), WTFMove(position) };
                }
            );
        }
    }

    return CSSRadialGradientValue::create(
        WTFMove(data),
        repeating,
        computedColorInterpolationMethod,
        WTFMove(*stops)
    );
}

// MARK: <conic-gradient()> | <repeating-conic-gradient()>
// https://drafts.csswg.org/css-images-4/#conic-gradient-syntax

static RefPtr<CSSValue> consumeConicGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // conic-gradient() = conic-gradient(
    //   [ [ from <angle> ]? [ at <position> ]? ] || <color-interpolation-method>,
    //   <angular-color-stop-list>
    // )

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, context);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    std::optional<CSS::Angle> angle;

    if (consumeIdent<CSSValueFrom>(range)) {
        // FIXME: Unlike linear-gradient, conic-gradients are not specified to allow unitless 0 angles - https://www.w3.org/TR/css-images-4/#valdef-conic-gradient-angle.
        const auto angleConsumeOptions = CSSPropertyParserOptions {
            .parserMode = context.mode,
            .unitless = UnitlessQuirk::Forbid,
            .unitlessZero = UnitlessZeroQuirk::Allow
        };

        auto angleValue = MetaConsumer<CSS::Angle>::consume(range, context, { }, angleConsumeOptions);
        if (!angleValue)
            return nullptr;
        angle = WTF::switchOn(WTFMove(angleValue->value), [](auto&& value) -> CSS::Angle { return value; });
    }

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueAt>(range)) {
        position = consumePositionUnresolved(range, context);
        if (!position)
            return nullptr;
    }

    if ((angle || position) && !colorInterpolationMethod && range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, context);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    if (angle || position || colorInterpolationMethod) {
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    auto stops = consumeAngularColorStopList<SupportsColorHints::Yes>(range, context);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(colorInterpolationMethod, *stops);

    return CSSConicGradientValue::create({
            WTFMove(angle),
            WTFMove(position)
        },
        repeating,
        computedColorInterpolationMethod,
        WTFMove(*stops)
    );
}

// MARK: <cross-fade()>

static RefPtr<CSSValue> consumeCrossFade(CSSParserTokenRange& args, const CSSParserContext& context, CSSValueID functionId)
{
    // FIXME: The current CSS Images spec has a pretty different construction than is being parsed here:
    //
    //    cross-fade() = cross-fade( <cf-image># )
    //    <cf-image> = <percentage [0,100]>? && [ <image> | <color> ]
    //
    //  https://drafts.csswg.org/css-images-4/#funcdef-cross-fade

    auto fromImageValueOrNone = consumeImageOrNone(args, context);
    if (!fromImageValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    auto toImageValueOrNone = consumeImageOrNone(args, context);
    if (!toImageValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto value = consumePercentageDividedBy100OrNumber(args, context);
    if (!value)
        return nullptr;

    if (value->isNumber()) {
        if (auto numberValue = value->resolveAsNumberIfNotCalculated(); numberValue && (*numberValue < 0 || *numberValue > 1))
            value = CSSPrimitiveValue::create(clampTo<double>(*numberValue, 0, 1));
    }
    return CSSCrossfadeValue::create(fromImageValueOrNone.releaseNonNull(), toImageValueOrNone.releaseNonNull(), value.releaseNonNull(), functionId == CSSValueWebkitCrossFade);
}

// MARK: <-webkit-canvas()>

static RefPtr<CSSValue> consumeWebkitCanvas(CSSParserTokenRange& args)
{
    if (args.peek().type() != IdentToken)
        return nullptr;
    return CSSCanvasValue::create(args.consumeIncludingWhitespace().value().toString());
}

// MARK: <-webkit-named-image()>

static RefPtr<CSSValue> consumeWebkitNamedImage(CSSParserTokenRange& args)
{
    if (args.peek().type() != IdentToken)
        return nullptr;
    return CSSNamedImageValue::create(args.consumeIncludingWhitespace().value().toString());
}

// MARK: <filter()>

static RefPtr<CSSValue> consumeFilterImage(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // FIXME: The current Filter Effects spec has a different construction than is being parsed here:
    //
    //    filter() = filter( [ <image> | <string> ], <filter-value-list> )
    //
    //  https://drafts.fxtf.org/filter-effects/#funcdef-filter
    //
    // Importantly, `none` is not a valid value for the first parameter.

    auto imageValueOrNone = consumeImageOrNone(args, context);
    if (!imageValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    auto filterValue = consumeFilterValueListOrNone(args, context, AllowedFilterFunctions::PixelFilters);
    if (!filterValue)
        return nullptr;
    return CSSFilterImageValue::create(imageValueOrNone.releaseNonNull(), filterValue.releaseNonNull());
}

// MARK: <paint()>
// https://drafts.css-houdini.org/css-paint-api/#funcdef-paint

static RefPtr<CSSValue> consumeCustomPaint(CSSParserTokenRange& args, const CSSParserContext& context)
{
    if (!context.cssPaintingAPIEnabled)
        return nullptr;
    if (args.peek().type() != IdentToken)
        return nullptr;
    auto name = args.consumeIncludingWhitespace().value().toString();

    if (!args.atEnd() && args.peek() != CommaToken)
        return nullptr;
    if (!args.atEnd())
        args.consume();

    auto argumentList = CSSVariableData::create(args.consumeAll());
    return CSSPaintImageValue::create(name, WTFMove(argumentList));
}

// MARK: <image-set()>

struct ImageSetTypeFunctionRaw {
    String value;

    bool operator==(const ImageSetTypeFunctionRaw&) const = default;
};
using ImageSetTypeFunction = ImageSetTypeFunctionRaw;

struct ImageSetTypeFunctionRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;

    static std::optional<ImageSetTypeFunctionRaw> consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions)
    {
        ASSERT(range.peek().type() == FunctionToken);
        if (range.peek().functionId() != CSSValueType)
            return { };

        auto rangeCopy = range;
        auto typeArg = consumeFunction(rangeCopy);
        auto result = consumeStringRaw(typeArg);

        if (result.isNull() || !typeArg.atEnd())
            return { };

        range = rangeCopy;
        return { { result.toString() } };
    }
};

template<> struct ConsumerDefinition<ImageSetTypeFunction> {
    using FunctionToken = ImageSetTypeFunctionRawKnownTokenTypeFunctionConsumer;
};

// MARK: Image Set Resolution + Type Function

static RefPtr<CSSPrimitiveValue> consumeImageSetResolutionOrTypeFunction(CSSParserTokenRange& range, const CSSParserContext& context, ValueRange valueRange)
{
    // [ <resolution> || type(<string>) ]
    //
    //   as part of
    //
    // <image-set()> = image-set( <image-set-option># )
    // <image-set-option> = [ <image> | <string> ] [ <resolution> || type(<string>) ]?

    const auto options = CSSPropertyParserOptions {
        .valueRange = valueRange,
        .unitless = UnitlessQuirk::Allow,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    auto result = MetaConsumer<CSS::Resolution, ImageSetTypeFunction>::consume(range, context, { }, options);
    if (!result)
        return { };

    return WTF::switchOn(*result,
        [&](const ImageSetTypeFunction& typeFunction) -> RefPtr<CSSPrimitiveValue> {
            return CSSPrimitiveValue::create(typeFunction.value);
        },
        [&](const auto& resolution) -> RefPtr<CSSPrimitiveValue> {
            return CSSPrimitiveValueResolverBase::resolve(resolution, CSSCalcSymbolTable { }, options);
        }
    );
}

// https://w3c.github.io/csswg-drafts/css-images-4/#image-set-notation
static RefPtr<CSSImageSetOptionValue> consumeImageSetOption(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<AllowedImageType> allowedImageTypes)
{
    auto image = consumeImage(range, context, allowedImageTypes);
    if (!image)
        return nullptr;

    auto result = CSSImageSetOptionValue::create(image.releaseNonNull());

    RefPtr<CSSPrimitiveValue> resolution;
    RefPtr<CSSPrimitiveValue> type;

    // Optional resolution and type in any order.
    for (size_t i = 0; i < 2 && !range.atEnd(); ++i) {
        if (auto optionalArgument = consumeImageSetResolutionOrTypeFunction(range, context, ValueRange::NonNegative)) {
            if ((resolution && optionalArgument->isResolution()) || (type && optionalArgument->isString()))
                return nullptr;

            if (optionalArgument->isResolution()) {
                resolution = optionalArgument;
                result->setResolution(optionalArgument.releaseNonNull());
                continue;
            }

            if (optionalArgument->isString()) {
                type = optionalArgument;
                result->setType(type->stringValue());
                continue;
            }
        }
        break;
    }

    if (!range.atEnd() && range.peek().type() != CommaToken)
        return nullptr;
    return result;
}


static RefPtr<CSSValue> consumeImageSet(CSSParserTokenRange& args, const CSSParserContext& context, OptionSet<AllowedImageType> allowedImageTypes)
{
    CSSValueListBuilder imageSet;
    do {
        if (auto option = consumeImageSetOption(args, context, allowedImageTypes))
            imageSet.append(option.releaseNonNull());
        else
            return nullptr;
    } while (consumeCommaIncludingWhitespace(args));

    return CSSImageSetValue::create(WTFMove(imageSet));
}

// MARK: <image>
// https://drafts.csswg.org/css-images-4/#image-values

RefPtr<CSSValue> consumeImage(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<AllowedImageType> allowedImageTypes)
{
    if (range.peek().type() == StringToken && allowedImageTypes.contains(AllowedImageType::RawStringAsURL)) {
        return CSSImageValue::create(context.completeURL(range.consumeIncludingWhitespace().value().toAtomString().string()),
            context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
    }

    if (range.peek().type() == FunctionToken) {
        auto consumeGeneratedImage = [&](auto consumer) -> RefPtr<CSSValue> {
            if (!allowedImageTypes.contains(AllowedImageType::GeneratedImage))
                return nullptr;
            CSSParserTokenRange rangeCopy = range;
            CSSParserTokenRange args = consumeFunction(rangeCopy);
            RefPtr result = consumer(args);
            if (!result || !args.atEnd())
                return nullptr;
            range = rangeCopy;
            return result;
        };

        auto consumeImageSetImage = [&](auto consumer) -> RefPtr<CSSValue> {
            if (!allowedImageTypes.contains(AllowedImageType::ImageSet))
                return nullptr;
            CSSParserTokenRange rangeCopy = range;
            CSSParserTokenRange args = consumeFunction(rangeCopy);
            RefPtr result = consumer(args);
            if (!result || !args.atEnd())
                return nullptr;
            range = rangeCopy;
            return result;
        };

        auto functionId = range.peek().functionId();
        switch (functionId) {
        case CSSValueRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeRadialGradient(args, context, CSSGradientRepeat::NonRepeating); });
        case CSSValueRepeatingRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeRadialGradient(args, context, CSSGradientRepeat::Repeating); });
        case CSSValueWebkitLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedLinearGradient(args, context, CSSGradientRepeat::NonRepeating); });
        case CSSValueWebkitRepeatingLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedLinearGradient(args, context, CSSGradientRepeat::Repeating); });
        case CSSValueRepeatingLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeLinearGradient(args, context, CSSGradientRepeat::Repeating); });
        case CSSValueLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeLinearGradient(args, context, CSSGradientRepeat::NonRepeating); });
        case CSSValueWebkitGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeDeprecatedGradient(args, context); });
        case CSSValueWebkitRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedRadialGradient(args, context, CSSGradientRepeat::NonRepeating); });
        case CSSValueWebkitRepeatingRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedRadialGradient(args, context, CSSGradientRepeat::Repeating); });
        case CSSValueConicGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeConicGradient(args, context, CSSGradientRepeat::NonRepeating); });
        case CSSValueRepeatingConicGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeConicGradient(args, context, CSSGradientRepeat::Repeating); });
        case CSSValueWebkitCrossFade:
            return consumeGeneratedImage([&](auto& args) { return consumeCrossFade(args, context, functionId); });
        case CSSValueCrossFade:
            return consumeGeneratedImage([&](auto& args) { return consumeCrossFade(args, context, functionId); });
        case CSSValueWebkitCanvas:
            return consumeGeneratedImage([&](auto& args) { return consumeWebkitCanvas(args); });
        case CSSValueWebkitNamedImage:
            return consumeGeneratedImage([&](auto& args) { return consumeWebkitNamedImage(args); });
        case CSSValueWebkitFilter:
        case CSSValueFilter:
            return consumeGeneratedImage([&](auto& args) { return consumeFilterImage(args, context); });
        case CSSValuePaint:
            return consumeGeneratedImage([&](auto& args) { return consumeCustomPaint(args, context); });
        case CSSValueImageSet:
        case CSSValueWebkitImageSet:
            return consumeImageSetImage([&](auto& args) { return consumeImageSet(args, context, (allowedImageTypes | AllowedImageType::RawStringAsURL) - AllowedImageType::ImageSet); });
        default:
            break;
        }
    }

    if (allowedImageTypes.contains(AllowedImageType::URLFunction)) {
        if (auto string = consumeURLRaw(range); !string.isNull()) {
            return CSSImageValue::create(context.completeURL(string.toAtomString().string()),
                context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
        }
    }

    return nullptr;
}

// MARK: <image> | none

RefPtr<CSSValue> consumeImageOrNone(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<AllowedImageType> allowedImageTypes)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeImage(range, context, allowedImageTypes);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
