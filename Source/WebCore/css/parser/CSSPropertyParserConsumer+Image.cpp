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
#include "CSSColor.h"
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
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+MetaResolver.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Position.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserOptions.h"
#include "CSSPropertyParserState.h"
#include "CSSValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CSSVariableData.h"
#include "StyleImage.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// Used by radial gradient consumers internally.
enum class ShapeKeyword : bool { Circle, Ellipse };

// MARK: Deprecated <gradient> values

template<CSSValueID zeroValue, CSSValueID oneHundredValue> static std::optional<CSS::NumberOrPercentage<>> consumeDeprecatedGradientPositionComponent(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().type() == IdentToken) {
        if (consumeIdent<zeroValue>(range))
            return CSS::NumberOrPercentage<> { CSS::PercentageRaw<> { 0 } };
        if (consumeIdent<oneHundredValue>(range))
            return CSS::NumberOrPercentage<> { CSS::PercentageRaw<> { 100 } };
        if (consumeIdent<CSSValueCenter>(range))
            return CSS::NumberOrPercentage<> { CSS::PercentageRaw<> { 50 } };
        return std::nullopt;
    }
    return MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(range, state);
}

static std::optional<CSS::DeprecatedGradientPosition> consumeDeprecatedGradientPosition(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto horizontal = consumeDeprecatedGradientPositionComponent<CSSValueLeft, CSSValueRight>(range, state);
    if (!horizontal)
        return std::nullopt;

    auto vertical = consumeDeprecatedGradientPositionComponent<CSSValueTop, CSSValueBottom>(range, state);
    if (!vertical)
        return std::nullopt;

    return { { WTFMove(*horizontal), WTFMove(*vertical) } };
}

static std::optional<CSS::Color> consumeDeprecatedGradientStopColor(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().id() == CSSValueCurrentcolor)
        return std::nullopt;
    return consumeUnresolvedColor(range, state);
}

static std::optional<CSS::GradientDeprecatedColorStop> consumeDeprecatedGradientColorStop(CSSParserTokenRange& range, CSS::PropertyParserState& state)
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
    std::optional<CSS::GradientDeprecatedColorStopPosition> position;
    switch (id) {
    case CSSValueFrom:
        position = CSS::NumberRaw<> { 0 };
        break;
    case CSSValueTo:
        position = CSS::NumberRaw<> { 1 };
        break;
    case CSSValueColorStop: {
        auto numberOrPercentage = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, state);
        if (!numberOrPercentage)
            return std::nullopt;
        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;
        position = WTFMove(*numberOrPercentage);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    auto color = consumeDeprecatedGradientStopColor(args, state);
    if (!color || !args.atEnd())
        return std::nullopt;

    return CSS::GradientDeprecatedColorStop {
        .color = WTFMove(*color),
        .position = WTFMove(*position)
    };
}

static std::optional<CSS::GradientDeprecatedColorStopList> consumeDeprecatedGradientColorStops(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    CSS::GradientDeprecatedColorStopList::Container stops;
    while (consumeCommaIncludingWhitespace(range)) {
        auto stop = consumeDeprecatedGradientColorStop(range, state);
        if (!stop)
            return std::nullopt;
        stops.append(WTFMove(*stop));
    }
    stops.shrinkToFit();

    return { { WTFMove(stops) } };
}

static RefPtr<CSSValue> consumeDeprecatedLinearGradient(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto first = consumeDeprecatedGradientPosition(range, state);
    if (!first)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto second = consumeDeprecatedGradientPosition(range, state);
    if (!second)
        return nullptr;

    auto stops = consumeDeprecatedGradientColorStops(range, state);
    if (!stops)
        return nullptr;

    return CSSGradientValue::create(
        FunctionNotation<CSSValueWebkitGradient, CSS::DeprecatedLinearGradient> {
            .parameters = {
                .colorInterpolationMethod = CSS::GradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
                .gradientLine = { WTFMove(*first), WTFMove(*second) },
                .stops = WTFMove(*stops)
            }
        }
    );
}

static RefPtr<CSSValue> consumeDeprecatedRadialGradient(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto first = consumeDeprecatedGradientPosition(range, state);
    if (!first)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto firstRadius = MetaConsumer<CSS::Number<CSS::Nonnegative>>::consume(range, state);
    if (!firstRadius)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto second = consumeDeprecatedGradientPosition(range, state);
    if (!second)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto secondRadius = MetaConsumer<CSS::Number<CSS::Nonnegative>>::consume(range, state);
    if (!secondRadius)
        return nullptr;

    auto stops = consumeDeprecatedGradientColorStops(range, state);
    if (!stops)
        return nullptr;

    return CSSGradientValue::create(
        FunctionNotation<CSSValueWebkitGradient, CSS::DeprecatedRadialGradient> {
            .parameters = {
                .colorInterpolationMethod = CSS::GradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
                .gradientBox = {
                    .first = WTFMove(*first),
                    .firstRadius = WTFMove(*firstRadius),
                    .second = WTFMove(*second),
                    .secondRadius = WTFMove(*secondRadius),
                },
                .stops = WTFMove(*stops)
            }
        }
    );
}

static RefPtr<CSSValue> consumeDeprecatedGradient(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    switch (range.consumeIncludingWhitespace().id()) {
    case CSSValueLinear:
        return consumeDeprecatedLinearGradient(range, state);
    case CSSValueRadial:
        return consumeDeprecatedRadialGradient(range, state);
    default:
        return nullptr;
    }
}

// MARK: <color-stop-list> | <angular-color-stop-list>
// https://drafts.csswg.org/css-images-4/#typedef-color-stop-list

enum class SupportsColorHints : bool { No, Yes };

static std::optional<CSS::Color> consumeStopColor(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    return consumeUnresolvedColor(range, state);
}

template<SupportsColorHints supportsColorHints, typename Stop, typename Consumer> static std::optional<CSS::GradientColorStopList<Stop>> consumeColorStopList(CSSParserTokenRange& range, CSS::PropertyParserState& state, Consumer&& consumeStopPosition)
{
    typename CSS::GradientColorStopList<Stop>::Container stops;

    // The first color stop cannot be a color hint.
    bool previousStopWasColorHint = true;
    do {
        Stop stop { consumeStopColor(range, state), consumeStopPosition(range) };
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

    // Must have at least one stop to be valid.
    if (stops.isEmpty())
        return std::nullopt;

    stops.shrinkToFit();

    return { WTFMove(stops) };
}

template<SupportsColorHints supportsColorHints> static std::optional<CSS::GradientLinearColorStopList> consumeLinearColorStopList(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    return consumeColorStopList<supportsColorHints, CSS::GradientLinearColorStop>(range, state, [&](auto& range) {
        return MetaConsumer<CSS::LengthPercentage<>>::consume(range, state);
    });
}

template<SupportsColorHints supportsColorHints> static std::optional<CSS::GradientAngularColorStopList> consumeAngularColorStopList(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // NOTE: Angular color stops accept unitless zero values.
    // https://drafts.csswg.org/css-images-4/#typedef-color-stop-angle

    return consumeColorStopList<supportsColorHints, CSS::GradientAngularColorStop>(range, state, [&](auto& range) {
        return MetaConsumer<CSS::AnglePercentage<>>::consume(range, state, { .unitlessZeroAngle = UnitlessZeroQuirk::Allow });
    });
}

static bool stopColorIs8Bit(const CSS::Color& color)
{
    return color.isKeyword() || color.absoluteColor().tryGetAsSRGBABytes();
}

static bool stopColorIs8Bit(const Markable<CSS::Color>& color)
{
    return !color || stopColorIs8Bit(*color);
}

template<typename Stop> static CSS::GradientColorInterpolationMethod computeGradientColorInterpolationMethod(std::optional<ColorInterpolationMethod> parsedColorInterpolationMethod, const CSS::GradientColorStopList<Stop>& stops)
{
    // We detect whether stops use legacy vs. non-legacy CSS color syntax using the following rules:
    //  - A CSSValueID is always considered legacy since all keyword based colors are considered legacy by the spec.
    //  - An actual Color value is considered legacy if it is stored as 8-bit sRGB.
    //
    // While this is accurate now, we should consider a more robust mechanism to detect this at parse
    // time, perhaps keeping this information in the CSSPrimitiveValue itself.

    auto defaultColorInterpolationMethod = CSS::GradientColorInterpolationMethod::Default::SRGB;
    for (auto& stop : stops) {
        if (stopColorIs8Bit(stop.color))
            continue;

        defaultColorInterpolationMethod = CSS::GradientColorInterpolationMethod::Default::OKLab;
        break;
    }

    if (parsedColorInterpolationMethod)
        return { *parsedColorInterpolationMethod, defaultColorInterpolationMethod };

    switch (defaultColorInterpolationMethod) {
    case CSS::GradientColorInterpolationMethod::Default::SRGB:
        return { { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Premultiplied }, defaultColorInterpolationMethod };

    case CSS::GradientColorInterpolationMethod::Default::OKLab:
        return { { ColorInterpolationMethod::OKLab { }, AlphaPremultiplication::Premultiplied }, defaultColorInterpolationMethod };
    }

    ASSERT_NOT_REACHED();
    return { { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Premultiplied }, defaultColorInterpolationMethod };
}

// MARK: Compat <gradient> values
// https://compat.spec.whatwg.org/#css-gradient-fns

// MARK: <-webkit-linear-gradient()> | <-webkit-repeating-linear-gradient()>
// https://compat.spec.whatwg.org/#css-gradients-webkit-linear-gradient

template<CSSValueID Name> static RefPtr<CSSValue> consumePrefixedLinearGradient(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // https://compat.spec.whatwg.org/#css-gradients-webkit-linear-gradient/ states that -webkit-linear-gradient() and
    // -webkit-repeating-linear-gradient() must be "treated as an alias of linear-gradient as defined in [css3-images-20110217]."
    // In [css3-images-20110217] the grammar was defined as:
    //
    //   <linear-gradient> = linear-gradient([ [ [top | bottom] || [left | right] ] | <angle> ,]? <color-stop>[, <color-stop>]+);
    //
    // see https://www.w3.org/TR/2011/WD-css3-images-20110217/#linear-gradients.

    static constexpr std::pair<CSSValueID, CSS::Vertical> verticalMappings[] {
        { CSSValueTop, CSS::Vertical { CSS::Keyword::Top { } } },
        { CSSValueBottom, CSS::Vertical { CSS::Keyword::Bottom { } } },
    };
    static constexpr SortedArrayMap verticalMap { verticalMappings };

    static constexpr std::pair<CSSValueID, CSS::Horizontal> horizontalMappings[] {
        { CSSValueLeft, CSS::Horizontal { CSS::Keyword::Left { } } },
        { CSSValueRight, CSS::Horizontal { CSS::Keyword::Right { } } },
    };
    static constexpr SortedArrayMap horizontalMap { horizontalMappings };

    auto consumeKeywordGradientLineKnownHorizontal = [&](CSSParserTokenRange& range, CSS::Horizontal knownHorizontal) -> CSS::PrefixedLinearGradient::GradientLine {
        if (auto vertical = consumeIdentUsingMapping(range, verticalMap))
            return SpaceSeparatedTuple { knownHorizontal, *vertical };
        return knownHorizontal;
    };

    auto consumeKeywordGradientLineKnownVertical = [&](CSSParserTokenRange& range, CSS::Vertical knownVertical) -> CSS::PrefixedLinearGradient::GradientLine {
        if (auto horizontal = consumeIdentUsingMapping(range, horizontalMap))
            return SpaceSeparatedTuple { *horizontal, knownVertical };
        return knownVertical;
    };

    auto consumeKeywordGradientLine = [&](CSSParserTokenRange& range) -> std::optional<CSS::PrefixedLinearGradient::GradientLine> {
        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSS::Horizontal { CSS::Keyword::Left { } });
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSS::Horizontal { CSS::Keyword::Right { } });
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSS::Vertical { CSS::Keyword::Top { } });
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSS::Vertical { CSS::Keyword::Bottom { } });
        default:
            return { };
        }
    };

    std::optional<CSS::PrefixedLinearGradient::GradientLine> gradientLine;

    // NOTE: Linear gradient <angle> specifiers accept unitless zero values.
    // https://drafts.csswg.org/css-images-4/#typedef-linear-gradient-syntax

    if (auto angle = MetaConsumer<CSS::Angle<>>::consume(range, state, { .unitlessZeroAngle = UnitlessZeroQuirk::Allow })) {
        gradientLine = WTF::switchOn(WTFMove(*angle), [](auto&& value) -> CSS::PrefixedLinearGradient::GradientLine { return value; });
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    } else if (auto keywordGradientLine = consumeKeywordGradientLine(range)) {
        gradientLine = WTFMove(*keywordGradientLine);
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    auto stops = consumeLinearColorStopList<SupportsColorHints::No>(range, state);
    if (!stops)
        return nullptr;

    return CSSGradientValue::create(
        FunctionNotation<Name, CSS::PrefixedLinearGradient> {
            .parameters = {
                .colorInterpolationMethod = CSS::GradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
                .gradientLine = gradientLine.value_or(CSS::Vertical { CSS::Keyword::Top { } }),
                .stops = WTFMove(*stops)
            }
        }
    );
}

// MARK: <-webkit-radial-gradient()> | <-webkit-repeating-radial-gradient()>
// https://compat.spec.whatwg.org/#css-gradients-webkit-radial-gradient

template<CSSValueID Name> static RefPtr<CSSValue> consumePrefixedRadialGradient(CSSParserTokenRange& range, CSS::PropertyParserState& state)
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

    static constexpr std::pair<CSSValueID, ShapeKeyword> shapeMappings[] {
        { CSSValueCircle, ShapeKeyword::Circle },
        { CSSValueEllipse, ShapeKeyword::Ellipse },
    };
    static constexpr SortedArrayMap shapeMap { shapeMappings };

    static constexpr std::pair<CSSValueID, CSS::PrefixedRadialGradient::Extent> extentMappings[] {
        { CSSValueContain, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::Contain { } } },
        { CSSValueCover, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::Cover { } } },
        { CSSValueClosestSide, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::ClosestSide { } } },
        { CSSValueClosestCorner, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::ClosestCorner { } } },
        { CSSValueFarthestSide, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::FarthestSide { } } },
        { CSSValueFarthestCorner, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::FarthestCorner { } } },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    auto position = consumeOneOrTwoComponentPositionUnresolved(range, state);
    if (position) {
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    std::optional<ShapeKeyword> shapeKeyword;
    std::optional<CSS::PrefixedRadialGradient::Extent> extentKeyword;
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

    auto consumeGradientBox = [&] -> std::optional<CSS::PrefixedRadialGradient::GradientBox> {
        if (shapeKeyword && extentKeyword) {
            switch (*shapeKeyword) {
            case ShapeKeyword::Ellipse:
                return CSS::PrefixedRadialGradient::Ellipse {
                    .size = *extentKeyword,
                    .position = WTFMove(position),
                };

            case ShapeKeyword::Circle:
                return CSS::PrefixedRadialGradient::Circle {
                    .size = *extentKeyword,
                    .position = WTFMove(position),
                };
            }
        }

        if (shapeKeyword) {
            switch (*shapeKeyword) {
            case ShapeKeyword::Ellipse:
                return CSS::PrefixedRadialGradient::Ellipse {
                    .size = std::nullopt,
                    .position = WTFMove(position),
                };

            case ShapeKeyword::Circle:
                return CSS::PrefixedRadialGradient::Circle {
                    .size = std::nullopt,
                    .position = WTFMove(position),
                };
            }
        }

        if (extentKeyword) {
            return CSS::PrefixedRadialGradient::Ellipse {
                .size = *extentKeyword,
                .position = WTFMove(position),
            };
        }

        if (auto length1 = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state)) {
            auto length2 = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state);
            if (!length2)
                return std::nullopt;
            if (!consumeCommaIncludingWhitespace(range))
                return std::nullopt;
            return CSS::PrefixedRadialGradient::Ellipse {
                .size = SpaceSeparatedArray { WTFMove(*length1), WTFMove(*length2) },
                .position = WTFMove(position),
            };
        }

        // If no size is provided, default to an ellipse.
        return CSS::PrefixedRadialGradient::Ellipse {
            .size = std::nullopt,
            .position = WTFMove(position)
        };
    };

    auto gradientBox = consumeGradientBox();
    if (!gradientBox)
        return nullptr;

    auto stops = consumeLinearColorStopList<SupportsColorHints::No>(range, state);
    if (!stops)
        return nullptr;

    return CSSGradientValue::create(
        FunctionNotation<Name, CSS::PrefixedRadialGradient> {
            .parameters = {
                .colorInterpolationMethod = CSS::GradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
                .gradientBox = WTFMove(*gradientBox),
                .stops = WTFMove(*stops)
            }
        }
    );
}

// MARK: Standard <gradient> values

// MARK: <linear-gradient()> | <repeating-linear-gradient()>
// https://drafts.csswg.org/css-images-4/#linear-gradients

template<CSSValueID Name> static RefPtr<CSSValue> consumeLinearGradient(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <side-or-corner> = [left | right] || [top | bottom]
    // linear-gradient() = linear-gradient(
    //   [ <angle> | to <side-or-corner> ]? || <color-interpolation-method>,
    //   <color-stop-list>
    // )

    static constexpr std::pair<CSSValueID, CSS::Vertical> verticalMappings[] {
        { CSSValueTop, CSS::Vertical { CSS::Keyword::Top { } } },
        { CSSValueBottom, CSS::Vertical { CSS::Keyword::Bottom { } } },
    };
    static constexpr SortedArrayMap verticalMap { verticalMappings };

    static constexpr std::pair<CSSValueID, CSS::Horizontal> horizontalMappings[] {
        { CSSValueLeft, CSS::Horizontal { CSS::Keyword::Left { } } },
        { CSSValueRight, CSS::Horizontal { CSS::Keyword::Right { } } },
    };
    static constexpr SortedArrayMap horizontalMap { horizontalMappings };

    auto consumeKeywordGradientLineKnownHorizontal = [&](CSSParserTokenRange& range, CSS::Horizontal knownHorizontal) -> CSS::LinearGradient::GradientLine {
        if (auto vertical = consumeIdentUsingMapping(range, verticalMap))
            return SpaceSeparatedTuple { knownHorizontal, *vertical };
        return knownHorizontal;
    };

    auto consumeKeywordGradientLineKnownVertical = [&](CSSParserTokenRange& range, CSS::Vertical knownVertical) -> CSS::LinearGradient::GradientLine {
        if (auto horizontal = consumeIdentUsingMapping(range, horizontalMap))
            return SpaceSeparatedTuple { *horizontal, knownVertical };
        return knownVertical;
    };

    auto consumeKeywordGradientLine = [&](CSSParserTokenRange& range) -> std::optional<CSS::LinearGradient::GradientLine> {
        ASSERT(range.peek().id() == CSSValueTo);
        range.consumeIncludingWhitespace();

        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSS::Horizontal { CSS::Keyword::Left { } });
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSS::Horizontal { CSS::Keyword::Right { } });
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSS::Vertical { CSS::Keyword::Top { } });
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSS::Vertical { CSS::Keyword::Bottom { } });
        default:
            return { };
        }
    };

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, state);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    std::optional<CSS::LinearGradient::GradientLine> gradientLine;

    // NOTE: Linear gradient <angle> specifiers accept unitless zero values.
    // https://drafts.csswg.org/css-images-4/#typedef-linear-gradient-syntax

    if (auto angle = MetaConsumer<CSS::Angle<>>::consume(range, state, { .unitlessZeroAngle = UnitlessZeroQuirk::Allow }))
        gradientLine = WTFMove(angle);
    else if (range.peek().id() == CSSValueTo) {
        auto keywordGradientLine = consumeKeywordGradientLine(range);
        if (!keywordGradientLine)
            return nullptr;
        gradientLine = WTFMove(*keywordGradientLine);
    }

    if (gradientLine && !colorInterpolationMethod && range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, state);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    if (gradientLine || colorInterpolationMethod) {
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    auto stops = consumeLinearColorStopList<SupportsColorHints::Yes>(range, state);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(colorInterpolationMethod, *stops);

    return CSSGradientValue::create(
        FunctionNotation<Name, CSS::LinearGradient> {
            .parameters = {
                .colorInterpolationMethod = computedColorInterpolationMethod,
                .gradientLine = gradientLine.value_or(CSS::Vertical { CSS::Keyword::Bottom { } }),
                .stops = WTFMove(*stops)
            }
        }
    );
}

// MARK: <radial-gradient()> | <repeating-radial-gradient()>
// https://drafts.csswg.org/css-images-4/#radial-gradients

template<CSSValueID Name> static RefPtr<CSSValue> consumeRadialGradient(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // radial-gradient() = radial-gradient(
    //   [[ <ending-shape> || <size> ]? [ at <position> ]? ] || <color-interpolation-method>,
    //   <color-stop-list>
    // )

    static constexpr std::pair<CSSValueID, ShapeKeyword> shapeMappings[] {
        { CSSValueCircle, ShapeKeyword::Circle },
        { CSSValueEllipse, ShapeKeyword::Ellipse },
    };
    static constexpr SortedArrayMap shapeMap { shapeMappings };

    static constexpr std::pair<CSSValueID, CSS::RadialGradient::Extent> extentMappings[] {
        { CSSValueClosestSide, CSS::RadialGradient::Extent { CSS::Keyword::ClosestSide { } } },
        { CSSValueClosestCorner, CSS::RadialGradient::Extent { CSS::Keyword::ClosestCorner { } } },
        { CSSValueFarthestSide, CSS::RadialGradient::Extent { CSS::Keyword::FarthestSide { } } },
        { CSSValueFarthestCorner, CSS::RadialGradient::Extent { CSS::Keyword::FarthestCorner { } } },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    static constexpr auto defaultExtent = CSS::RadialGradient::Extent { CSS::Keyword::FarthestCorner { } };

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, state);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    std::optional<ShapeKeyword> shape;

    using Size = Variant<CSS::RadialGradient::Extent, CSS::Length<CSS::Nonnegative>, SpaceSeparatedArray<CSS::LengthPercentage<CSS::Nonnegative>, 2>>;
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
            auto rangeCopy = range;
            auto length1 = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(rangeCopy, state);
            if (!length1)
                break;
            if (size)
                return nullptr;
            if (auto length2 = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(rangeCopy, state)) {
                size = SpaceSeparatedArray { WTFMove(*length1), WTFMove(*length2) };
                range = rangeCopy;

                // Additional increment is necessary since we consumed a second token.
                ++i;
            } else {
                // Reset to before the first length-percentage, and re-parse to make sure it is a valid <length [0,∞]> production.
                rangeCopy = range;
                auto length = MetaConsumer<CSS::Length<CSS::Nonnegative>>::consume(rangeCopy, state);
                if (!length)
                    return nullptr;
                size = WTFMove(*length);
                range = rangeCopy;
            }
        }
    }

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueAt>(range)) {
        position = consumePositionUnresolved(range, state);
        if (!position)
            return nullptr;
    }

    if ((shape || size || position) && !colorInterpolationMethod && range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, state);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    if ((shape || size || position || colorInterpolationMethod) && !consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto stops = consumeLinearColorStopList<SupportsColorHints::Yes>(range, state);
    if (!stops)
        return nullptr;

    auto consumeGradientBox = [&] -> std::optional<CSS::RadialGradient::GradientBox> {
        if (shape && size) {
            switch (*shape) {
            case ShapeKeyword::Ellipse:
                return WTF::switchOn(WTFMove(*size),
                    [&](CSS::RadialGradient::Extent&& extent) -> std::optional<CSS::RadialGradient::GradientBox> {
                        return CSS::RadialGradient::Ellipse {
                            .size = WTFMove(extent),
                            .position = WTFMove(position),
                        };
                    },
                    [&](CSS::Length<CSS::Nonnegative>&&) -> std::optional<CSS::RadialGradient::GradientBox> {
                        // Ellipses must have two length-percentages specified.
                        return std::nullopt;
                    },
                    [&](SpaceSeparatedArray<CSS::LengthPercentage<CSS::Nonnegative>, 2>&& size) -> std::optional<CSS::RadialGradient::GradientBox> {
                        return CSS::RadialGradient::Ellipse {
                            .size = WTFMove(size),
                            .position = WTFMove(position),
                        };
                    }
                );

            case ShapeKeyword::Circle:
                return WTF::switchOn(WTFMove(*size),
                    [&](CSS::RadialGradient::Extent&& extent) -> std::optional<CSS::RadialGradient::GradientBox> {
                        return CSS::RadialGradient::Circle {
                            .size = WTFMove(extent),
                            .position = WTFMove(position),
                        };
                    },
                    [&](CSS::Length<CSS::Nonnegative>&& length) -> std::optional<CSS::RadialGradient::GradientBox> {
                        return CSS::RadialGradient::Circle {
                            .size = WTFMove(length),
                            .position = WTFMove(position),
                        };
                    },
                    [&](SpaceSeparatedArray<CSS::LengthPercentage<CSS::Nonnegative>, 2>&&) -> std::optional<CSS::RadialGradient::GradientBox> {
                        // Circles must have a maximum of only one length specified.
                        return std::nullopt;
                    }
                );
            }
        }

        if (shape) {
            switch (*shape) {
            case ShapeKeyword::Ellipse:
                return CSS::RadialGradient::Ellipse {
                    .size = defaultExtent,
                    .position = WTFMove(position),
                };

            case ShapeKeyword::Circle:
                return CSS::RadialGradient::Circle {
                    .size = defaultExtent,
                    .position = WTFMove(position),
                };
            }
        }

        if (size) {
            return WTF::switchOn(WTFMove(*size),
                [&](CSS::RadialGradient::Extent&& extent) -> std::optional<CSS::RadialGradient::GradientBox> {
                    return CSS::RadialGradient::Ellipse {
                        .size = WTFMove(extent),
                        .position = WTFMove(position),
                    };
                },
                [&](CSS::Length<CSS::Nonnegative>&& length) -> std::optional<CSS::RadialGradient::GradientBox> {
                    return CSS::RadialGradient::Circle {
                        .size = WTFMove(length),
                        .position = WTFMove(position),
                    };
                },
                [&](SpaceSeparatedArray<CSS::LengthPercentage<CSS::Nonnegative>, 2>&& size) -> std::optional<CSS::RadialGradient::GradientBox> {
                    return CSS::RadialGradient::Ellipse {
                        .size = WTFMove(size),
                        .position = WTFMove(position),
                    };
                }
            );
        }

        // If no size is provided, default to an ellipse.
        return CSS::RadialGradient::Ellipse {
            .size = defaultExtent,
            .position = WTFMove(position),
        };
    };

    auto gradientBox = consumeGradientBox();
    if (!gradientBox)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(colorInterpolationMethod, *stops);

    return CSSGradientValue::create(
        FunctionNotation<Name, CSS::RadialGradient> {
            .parameters = {
                .colorInterpolationMethod = computedColorInterpolationMethod,
                .gradientBox = WTFMove(*gradientBox),
                .stops = WTFMove(*stops)
            }
        }
    );
}

// MARK: <conic-gradient()> | <repeating-conic-gradient()>
// https://drafts.csswg.org/css-images-4/#conic-gradient-syntax

template<CSSValueID Name> static RefPtr<CSSValue> consumeConicGradient(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // conic-gradient() = conic-gradient(
    //   [ [ from <angle> ]? [ at <position> ]? ] || <color-interpolation-method>,
    //   <angular-color-stop-list>
    // )

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, state);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    // NOTE: Conic gradient <angle> specifiers accept unitless zero values.
    // https://drafts.csswg.org/css-images-4/#typedef-conic-gradient-syntax

    std::optional<CSS::Angle<>> angle;
    if (consumeIdent<CSSValueFrom>(range)) {
        angle = MetaConsumer<CSS::Angle<>>::consume(range, state, { .unitlessZeroAngle = UnitlessZeroQuirk::Allow });
        if (!angle)
            return nullptr;
    }

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueAt>(range)) {
        position = consumePositionUnresolved(range, state);
        if (!position)
            return nullptr;
    }

    if ((angle || position) && !colorInterpolationMethod && range.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, state);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    if (angle || position || colorInterpolationMethod) {
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    auto stops = consumeAngularColorStopList<SupportsColorHints::Yes>(range, state);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(colorInterpolationMethod, *stops);

    return CSSGradientValue::create(
        FunctionNotation<Name, CSS::ConicGradient> {
            .parameters = {
                .colorInterpolationMethod = computedColorInterpolationMethod,
                .gradientBox = {
                    .angle = WTFMove(angle),
                    .position = WTFMove(position),
                },
                .stops = WTFMove(*stops),
            }
        }
    );
}

// MARK: <cross-fade()>

static RefPtr<CSSValue> consumeCrossFade(CSSParserTokenRange& args, CSS::PropertyParserState& state, CSSValueID functionId)
{
    // FIXME: The current CSS Images spec has a pretty different construction than is being parsed here:
    //
    //    cross-fade() = cross-fade( <cf-image># )
    //    <cf-image> = <percentage [0,100]>? && [ <image> | <color> ]
    //
    //  https://drafts.csswg.org/css-images-4/#funcdef-cross-fade

    auto fromImageValueOrNone = consumeImageOrNone(args, state);
    if (!fromImageValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    auto toImageValueOrNone = consumeImageOrNone(args, state);
    if (!toImageValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto value = consumePercentageDividedBy100OrNumber(args, state);
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

static RefPtr<CSSValue> consumeFilterImage(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // FIXME: The current Filter Effects spec has a different construction than is being parsed here:
    //
    //    filter() = filter( [ <image> | <string> ], <filter-value-list> )
    //
    //  https://drafts.fxtf.org/filter-effects/#funcdef-filter
    //
    // Importantly, `none` is not a valid value for either parameter.

    auto imageValueOrNone = consumeImageOrNone(args, state);
    if (!imageValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    auto filter = consumeUnresolvedFilter(args, state);
    if (!filter)
        return nullptr;
    return CSSFilterImageValue::create(imageValueOrNone.releaseNonNull(), WTFMove(*filter));
}

// MARK: <paint()>
// https://drafts.css-houdini.org/css-paint-api/#funcdef-paint

static RefPtr<CSSValue> consumeCustomPaint(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    if (!state.context.cssPaintingAPIEnabled)
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

    static std::optional<ImageSetTypeFunctionRaw> consume(CSSParserTokenRange& range, CSS::PropertyParserState&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions)
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

static RefPtr<CSSPrimitiveValue> consumeImageSetResolutionOrTypeFunction(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // [ <resolution> || type(<string>) ]
    //
    //   as part of
    //
    // <image-set()> = image-set( <image-set-option># )
    // <image-set-option> = [ <image> | <string> ] [ <resolution> || type(<string>) ]?

    return MetaConsumer<CSS::Resolution<>, ImageSetTypeFunction>::consume(range, state,
        [&](const ImageSetTypeFunction& typeFunction) -> RefPtr<CSSPrimitiveValue> {
            return CSSPrimitiveValue::create(typeFunction.value);
        },
        [&](const CSS::Resolution<>& resolution) -> RefPtr<CSSPrimitiveValue> {
            return CSSPrimitiveValueResolverBase::resolve(resolution);
        }
    ).value_or(nullptr);
}

// https://w3c.github.io/csswg-drafts/css-images-4/#image-set-notation
static RefPtr<CSSImageSetOptionValue> consumeImageSetOption(CSSParserTokenRange& range, CSS::PropertyParserState& state, OptionSet<AllowedImageType> allowedImageTypes)
{
    auto image = consumeImage(range, state, allowedImageTypes);
    if (!image)
        return nullptr;

    auto result = CSSImageSetOptionValue::create(image.releaseNonNull());

    RefPtr<CSSPrimitiveValue> resolution;
    RefPtr<CSSPrimitiveValue> type;

    // Optional resolution and type in any order.
    for (size_t i = 0; i < 2 && !range.atEnd(); ++i) {
        if (auto optionalArgument = consumeImageSetResolutionOrTypeFunction(range, state)) {
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


static RefPtr<CSSValue> consumeImageSet(CSSParserTokenRange& args, CSS::PropertyParserState& state, OptionSet<AllowedImageType> allowedImageTypes)
{
    CSSValueListBuilder imageSet;
    do {
        if (auto option = consumeImageSetOption(args, state, allowedImageTypes))
            imageSet.append(option.releaseNonNull());
        else
            return nullptr;
    } while (consumeCommaIncludingWhitespace(args));

    return CSSImageSetValue::create(WTFMove(imageSet));
}

// MARK: <image>
// https://drafts.csswg.org/css-images-4/#image-values

RefPtr<CSSValue> consumeImage(CSSParserTokenRange& range, CSS::PropertyParserState& state, OptionSet<AllowedImageType> allowedImageTypes)
{
    if (range.peek().type() == StringToken && allowedImageTypes.contains(AllowedImageType::RawStringAsURL)) {
        auto imageURL = CSS::completeURL(range.peek().value().toAtomString().string(), state.context);
        if (!imageURL)
            return nullptr;
        range.consumeIncludingWhitespace();
        return CSSImageValue::create(WTFMove(*imageURL));
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
            return consumeGeneratedImage([&](auto& args) { return consumeRadialGradient<CSSValueRadialGradient>(args, state); });
        case CSSValueRepeatingRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeRadialGradient<CSSValueRepeatingRadialGradient>(args, state); });
        case CSSValueWebkitLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedLinearGradient<CSSValueWebkitLinearGradient>(args, state); });
        case CSSValueWebkitRepeatingLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedLinearGradient<CSSValueWebkitRepeatingLinearGradient>(args, state); });
        case CSSValueRepeatingLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeLinearGradient<CSSValueRepeatingLinearGradient>(args, state); });
        case CSSValueLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeLinearGradient<CSSValueLinearGradient>(args, state); });
        case CSSValueWebkitGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeDeprecatedGradient(args, state); });
        case CSSValueWebkitRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedRadialGradient<CSSValueWebkitRadialGradient>(args, state); });
        case CSSValueWebkitRepeatingRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedRadialGradient<CSSValueWebkitRepeatingRadialGradient>(args, state); });
        case CSSValueConicGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeConicGradient<CSSValueConicGradient>(args, state); });
        case CSSValueRepeatingConicGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeConicGradient<CSSValueRepeatingConicGradient>(args, state); });
        case CSSValueWebkitCrossFade:
            return consumeGeneratedImage([&](auto& args) { return consumeCrossFade(args, state, functionId); });
        case CSSValueCrossFade:
            return consumeGeneratedImage([&](auto& args) { return consumeCrossFade(args, state, functionId); });
        case CSSValueWebkitCanvas:
            return consumeGeneratedImage([&](auto& args) { return consumeWebkitCanvas(args); });
        case CSSValueWebkitNamedImage:
            return consumeGeneratedImage([&](auto& args) { return consumeWebkitNamedImage(args); });
        case CSSValueWebkitFilter:
        case CSSValueFilter:
            return consumeGeneratedImage([&](auto& args) { return consumeFilterImage(args, state); });
        case CSSValuePaint:
            return consumeGeneratedImage([&](auto& args) { return consumeCustomPaint(args, state); });
        case CSSValueImageSet:
        case CSSValueWebkitImageSet:
            return consumeImageSetImage([&](auto& args) { return consumeImageSet(args, state, (allowedImageTypes | AllowedImageType::RawStringAsURL) - AllowedImageType::ImageSet); });
        default:
            break;
        }
    }

    if (allowedImageTypes.contains(AllowedImageType::URLFunction)) {
        if (auto imageURL = consumeURLRaw(range, state, { AllowedURLModifiers::CrossOrigin, AllowedURLModifiers::ReferrerPolicy }))
            return CSSImageValue::create(WTFMove(*imageURL));
    }

    return nullptr;
}

// MARK: <image> | none

RefPtr<CSSValue> consumeImageOrNone(CSSParserTokenRange& range, CSS::PropertyParserState& state, OptionSet<AllowedImageType> allowedImageTypes)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeImage(range, state, allowedImageTypes);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
