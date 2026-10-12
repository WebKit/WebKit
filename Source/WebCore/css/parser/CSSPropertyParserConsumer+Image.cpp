/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSColorImageValue.h"
#include "CSSColorInterpolationMethod.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFilterImageValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetOptionValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSLightDarkImageValue.h"
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
#include "CSSStringValue.h"
#include "CSSValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CSSVariableData.h"
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
        if (consumeIdent<CSSValueID::Center>(range))
            return CSS::NumberOrPercentage<> { CSS::PercentageRaw<> { 50 } };
        return std::nullopt;
    }
    return MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(range, state);
}

static std::optional<CSS::DeprecatedGradientPosition> consumeDeprecatedGradientPosition(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto horizontal = consumeDeprecatedGradientPositionComponent<CSSValueID::Left, CSSValueID::Right>(range, state);
    if (!horizontal)
        return std::nullopt;

    auto vertical = consumeDeprecatedGradientPositionComponent<CSSValueID::Top, CSSValueID::Bottom>(range, state);
    if (!vertical)
        return std::nullopt;

    return { { WTF::move(*horizontal), WTF::move(*vertical) } };
}

static std::optional<CSS::Color> consumeDeprecatedGradientStopColor(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().id() == CSSValueID::Currentcolor)
        return std::nullopt;
    return consumeUnresolvedColor(range, state);
}

static std::optional<CSS::GradientDeprecatedColorStop> consumeDeprecatedGradientColorStop(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto id = range.peek().functionId();
    switch (id) {
    case CSSValueID::From:
    case CSSValueID::To:
    case CSSValueID::ColorStop:
        break;

    default:
        return std::nullopt;
    }

    auto args = consumeFunction(range);
    std::optional<CSS::GradientDeprecatedColorStopPosition> position;
    switch (id) {
    case CSSValueID::From:
        position = CSS::NumberRaw<> { 0 };
        break;
    case CSSValueID::To:
        position = CSS::NumberRaw<> { 1 };
        break;
    case CSSValueID::ColorStop: {
        auto numberOrPercentage = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, state);
        if (!numberOrPercentage)
            return std::nullopt;
        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;
        position = WTF::move(*numberOrPercentage);
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
        .color = WTF::move(*color),
        .position = WTF::move(*position)
    };
}

static std::optional<CSS::GradientDeprecatedColorStopList> consumeDeprecatedGradientColorStops(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    CSS::GradientDeprecatedColorStopList::Container stops;
    while (consumeCommaIncludingWhitespace(range)) {
        auto stop = consumeDeprecatedGradientColorStop(range, state);
        if (!stop)
            return std::nullopt;
        stops.append(WTF::move(*stop));
    }
    stops.shrinkToFit();

    return { { WTF::move(stops) } };
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
        FunctionNotation<CSSValueID::WebkitGradient, CSS::DeprecatedLinearGradient> {
            .parameters = {
                .colorInterpolationMethod = CSS::GradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
                .gradientLine = { WTF::move(*first), WTF::move(*second) },
                .stops = WTF::move(*stops)
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
        FunctionNotation<CSSValueID::WebkitGradient, CSS::DeprecatedRadialGradient> {
            .parameters = {
                .colorInterpolationMethod = CSS::GradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Premultiplied),
                .gradientBox = {
                    .first = WTF::move(*first),
                    .firstRadius = WTF::move(*firstRadius),
                    .second = WTF::move(*second),
                    .secondRadius = WTF::move(*secondRadius),
                },
                .stops = WTF::move(*stops)
            }
        }
    );
}

static RefPtr<CSSValue> consumeDeprecatedGradient(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    switch (range.consumeIncludingWhitespace().id()) {
    case CSSValueID::Linear:
        return consumeDeprecatedLinearGradient(range, state);
    case CSSValueID::Radial:
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

template<SupportsColorHints supportsColorHints, typename Stop, typename Consumer> static std::optional<CSS::GradientColorStopList<Stop>> consumeColorStopList(CSSParserTokenRange& range, CSS::PropertyParserState& state, NOESCAPE const Consumer& consumeStopPosition)
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
                stop.position = WTF::move(secondPosition);
            }
        }
        stops.append(WTF::move(stop));
    } while (consumeCommaIncludingWhitespace(range));

    // The last color stop cannot be a color hint.
    if (previousStopWasColorHint)
        return std::nullopt;

    // Must have at least one stop to be valid.
    if (stops.isEmpty())
        return std::nullopt;

    stops.shrinkToFit();

    return { WTF::move(stops) };
}

template<SupportsColorHints supportsColorHints> static std::optional<CSS::GradientLinearColorStopList> consumeLinearColorStopList(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    return consumeColorStopList<supportsColorHints, CSS::GradientLinearColorStop>(range, state, [&](auto& range) {
        return MetaConsumer<CSS::LengthPercentage<CSS::AllLayoutUnitClamped>>::consume(range, state);
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

template<typename Stop> static CSS::GradientColorInterpolationMethod computeGradientColorInterpolationMethod(std::optional<CSS::ColorInterpolationMethod> parsedColorInterpolationMethod, const CSS::GradientColorStopList<Stop>& stops)
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
        return { .method = *parsedColorInterpolationMethod, .defaultMethod = defaultColorInterpolationMethod };

    switch (defaultColorInterpolationMethod) {
    case CSS::GradientColorInterpolationMethod::Default::SRGB:
        return { .method = { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Premultiplied }, .defaultMethod = defaultColorInterpolationMethod };

    case CSS::GradientColorInterpolationMethod::Default::OKLab:
        return { .method = { ColorInterpolationMethod::OKLab { }, AlphaPremultiplication::Premultiplied }, .defaultMethod = defaultColorInterpolationMethod };
    }

    ASSERT_NOT_REACHED();
    return { .method = { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Premultiplied }, .defaultMethod = defaultColorInterpolationMethod };
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

    static constexpr SortedArrayMap verticalMap { WTF::toArray<std::pair<CSSValueID, CSS::Vertical>>({
        { CSSValueID::Top, CSS::Vertical { CSS::Keyword::Top { } } },
        { CSSValueID::Bottom, CSS::Vertical { CSS::Keyword::Bottom { } } },
    }) };

    static constexpr SortedArrayMap horizontalMap { WTF::toArray<std::pair<CSSValueID, CSS::Horizontal>>({
        { CSSValueID::Left, CSS::Horizontal { CSS::Keyword::Left { } } },
        { CSSValueID::Right, CSS::Horizontal { CSS::Keyword::Right { } } },
    }) };

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
        case CSSValueID::Left:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSS::Horizontal { CSS::Keyword::Left { } });
        case CSSValueID::Right:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSS::Horizontal { CSS::Keyword::Right { } });
        case CSSValueID::Top:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSS::Vertical { CSS::Keyword::Top { } });
        case CSSValueID::Bottom:
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
        gradientLine = WTF::switchOn(WTF::move(*angle), [](auto&& value) -> CSS::PrefixedLinearGradient::GradientLine { return value; });
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    } else if (auto keywordGradientLine = consumeKeywordGradientLine(range)) {
        gradientLine = WTF::move(*keywordGradientLine);
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
                .stops = WTF::move(*stops)
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

    static constexpr SortedArrayMap shapeMap { WTF::toArray<std::pair<CSSValueID, ShapeKeyword>>({
        { CSSValueID::Circle, ShapeKeyword::Circle },
        { CSSValueID::Ellipse, ShapeKeyword::Ellipse },
    }) };

    static constexpr SortedArrayMap extentMap { WTF::toArray<std::pair<CSSValueID, CSS::PrefixedRadialGradient::Extent>>({
        { CSSValueID::Contain, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::Contain { } } },
        { CSSValueID::Cover, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::Cover { } } },
        { CSSValueID::ClosestSide, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::ClosestSide { } } },
        { CSSValueID::ClosestCorner, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::ClosestCorner { } } },
        { CSSValueID::FarthestSide, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::FarthestSide { } } },
        { CSSValueID::FarthestCorner, CSS::PrefixedRadialGradient::Extent { CSS::Keyword::FarthestCorner { } } },
    }) };

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
                    .position = WTF::move(position),
                };

            case ShapeKeyword::Circle:
                return CSS::PrefixedRadialGradient::Circle {
                    .size = *extentKeyword,
                    .position = WTF::move(position),
                };
            }
        }

        if (shapeKeyword) {
            switch (*shapeKeyword) {
            case ShapeKeyword::Ellipse:
                return CSS::PrefixedRadialGradient::Ellipse {
                    .size = std::nullopt,
                    .position = WTF::move(position),
                };

            case ShapeKeyword::Circle:
                return CSS::PrefixedRadialGradient::Circle {
                    .size = std::nullopt,
                    .position = WTF::move(position),
                };
            }
        }

        if (extentKeyword) {
            return CSS::PrefixedRadialGradient::Ellipse {
                .size = *extentKeyword,
                .position = WTF::move(position),
            };
        }

        if (auto length1 = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state)) {
            auto length2 = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state);
            if (!length2)
                return std::nullopt;
            if (!consumeCommaIncludingWhitespace(range))
                return std::nullopt;
            return CSS::PrefixedRadialGradient::Ellipse {
                .size = SpaceSeparatedArray { WTF::move(*length1), WTF::move(*length2) },
                .position = WTF::move(position),
            };
        }

        // If no size is provided, default to an ellipse.
        return CSS::PrefixedRadialGradient::Ellipse {
            .size = std::nullopt,
            .position = WTF::move(position)
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
                .gradientBox = WTF::move(*gradientBox),
                .stops = WTF::move(*stops)
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

    static constexpr SortedArrayMap verticalMap { WTF::toArray<std::pair<CSSValueID, CSS::Vertical>>({
        { CSSValueID::Top, CSS::Vertical { CSS::Keyword::Top { } } },
        { CSSValueID::Bottom, CSS::Vertical { CSS::Keyword::Bottom { } } },
    }) };

    static constexpr SortedArrayMap horizontalMap { WTF::toArray<std::pair<CSSValueID, CSS::Horizontal>>({
        { CSSValueID::Left, CSS::Horizontal { CSS::Keyword::Left { } } },
        { CSSValueID::Right, CSS::Horizontal { CSS::Keyword::Right { } } },
    }) };

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
        ASSERT(range.peek().id() == CSSValueID::To);
        range.consumeIncludingWhitespace();

        switch (range.peek().id()) {
        case CSSValueID::Left:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSS::Horizontal { CSS::Keyword::Left { } });
        case CSSValueID::Right:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSS::Horizontal { CSS::Keyword::Right { } });
        case CSSValueID::Top:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSS::Vertical { CSS::Keyword::Top { } });
        case CSSValueID::Bottom:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSS::Vertical { CSS::Keyword::Bottom { } });
        default:
            return { };
        }
    };

    std::optional<CSS::ColorInterpolationMethod> colorInterpolationMethod;

    if (range.peek().id() == CSSValueID::In) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, state);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    std::optional<CSS::LinearGradient::GradientLine> gradientLine;

    // NOTE: Linear gradient <angle> specifiers accept unitless zero values.
    // https://drafts.csswg.org/css-images-4/#typedef-linear-gradient-syntax

    if (auto angle = MetaConsumer<CSS::Angle<>>::consume(range, state, { .unitlessZeroAngle = UnitlessZeroQuirk::Allow }))
        gradientLine = WTF::move(angle);
    else if (range.peek().id() == CSSValueID::To) {
        auto keywordGradientLine = consumeKeywordGradientLine(range);
        if (!keywordGradientLine)
            return nullptr;
        gradientLine = WTF::move(*keywordGradientLine);
    }

    if (gradientLine && !colorInterpolationMethod && range.peek().id() == CSSValueID::In) {
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
                .stops = WTF::move(*stops)
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

    static constexpr SortedArrayMap shapeMap { WTF::toArray<std::pair<CSSValueID, ShapeKeyword>>({
        { CSSValueID::Circle, ShapeKeyword::Circle },
        { CSSValueID::Ellipse, ShapeKeyword::Ellipse },
    }) };

    static constexpr SortedArrayMap extentMap { WTF::toArray<std::pair<CSSValueID, CSS::RadialGradient::Extent>>({
        { CSSValueID::ClosestSide, CSS::RadialGradient::Extent { CSS::Keyword::ClosestSide { } } },
        { CSSValueID::ClosestCorner, CSS::RadialGradient::Extent { CSS::Keyword::ClosestCorner { } } },
        { CSSValueID::FarthestSide, CSS::RadialGradient::Extent { CSS::Keyword::FarthestSide { } } },
        { CSSValueID::FarthestCorner, CSS::RadialGradient::Extent { CSS::Keyword::FarthestCorner { } } },
    }) };

    static constexpr auto defaultExtent = CSS::RadialGradient::Extent { CSS::Keyword::FarthestCorner { } };

    std::optional<CSS::ColorInterpolationMethod> colorInterpolationMethod;

    if (range.peek().id() == CSSValueID::In) {
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
                size = SpaceSeparatedArray { WTF::move(*length1), WTF::move(*length2) };
                range = rangeCopy;

                // Additional increment is necessary since we consumed a second token.
                ++i;
            } else {
                // Reset to before the first length-percentage, and re-parse to make sure it is a valid <length [0,∞]> production.
                rangeCopy = range;
                auto length = MetaConsumer<CSS::Length<CSS::Nonnegative>>::consume(rangeCopy, state);
                if (!length)
                    return nullptr;
                size = WTF::move(*length);
                range = rangeCopy;
            }
        }
    }

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueID::At>(range)) {
        position = consumePositionUnresolved(range, state);
        if (!position)
            return nullptr;
    }

    if ((shape || size || position) && !colorInterpolationMethod && range.peek().id() == CSSValueID::In) {
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
                return WTF::switchOn(WTF::move(*size),
                    [&](CSS::RadialGradient::Extent&& extent) -> std::optional<CSS::RadialGradient::GradientBox> {
                        return CSS::RadialGradient::Ellipse {
                            .size = WTF::move(extent),
                            .position = WTF::move(position),
                        };
                    },
                    [&](CSS::Length<CSS::Nonnegative>&&) -> std::optional<CSS::RadialGradient::GradientBox> {
                        // Ellipses must have two length-percentages specified.
                        return std::nullopt;
                    },
                    [&](SpaceSeparatedArray<CSS::LengthPercentage<CSS::Nonnegative>, 2>&& size) -> std::optional<CSS::RadialGradient::GradientBox> {
                        return CSS::RadialGradient::Ellipse {
                            .size = WTF::move(size),
                            .position = WTF::move(position),
                        };
                    }
                );

            case ShapeKeyword::Circle:
                return WTF::switchOn(WTF::move(*size),
                    [&](CSS::RadialGradient::Extent&& extent) -> std::optional<CSS::RadialGradient::GradientBox> {
                        return CSS::RadialGradient::Circle {
                            .size = WTF::move(extent),
                            .position = WTF::move(position),
                        };
                    },
                    [&](CSS::Length<CSS::Nonnegative>&& length) -> std::optional<CSS::RadialGradient::GradientBox> {
                        return CSS::RadialGradient::Circle {
                            .size = WTF::move(length),
                            .position = WTF::move(position),
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
                    .position = WTF::move(position),
                };

            case ShapeKeyword::Circle:
                return CSS::RadialGradient::Circle {
                    .size = defaultExtent,
                    .position = WTF::move(position),
                };
            }
        }

        if (size) {
            return WTF::switchOn(WTF::move(*size),
                [&](CSS::RadialGradient::Extent&& extent) -> std::optional<CSS::RadialGradient::GradientBox> {
                    return CSS::RadialGradient::Ellipse {
                        .size = WTF::move(extent),
                        .position = WTF::move(position),
                    };
                },
                [&](CSS::Length<CSS::Nonnegative>&& length) -> std::optional<CSS::RadialGradient::GradientBox> {
                    return CSS::RadialGradient::Circle {
                        .size = WTF::move(length),
                        .position = WTF::move(position),
                    };
                },
                [&](SpaceSeparatedArray<CSS::LengthPercentage<CSS::Nonnegative>, 2>&& size) -> std::optional<CSS::RadialGradient::GradientBox> {
                    return CSS::RadialGradient::Ellipse {
                        .size = WTF::move(size),
                        .position = WTF::move(position),
                    };
                }
            );
        }

        // If no size is provided, default to an ellipse.
        return CSS::RadialGradient::Ellipse {
            .size = defaultExtent,
            .position = WTF::move(position),
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
                .gradientBox = WTF::move(*gradientBox),
                .stops = WTF::move(*stops)
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

    std::optional<CSS::ColorInterpolationMethod> colorInterpolationMethod;

    if (range.peek().id() == CSSValueID::In) {
        colorInterpolationMethod = consumeColorInterpolationMethod(range, state);
        if (!colorInterpolationMethod)
            return nullptr;
    }

    // NOTE: Conic gradient <angle> specifiers accept unitless zero values.
    // https://drafts.csswg.org/css-images-4/#typedef-conic-gradient-syntax

    std::optional<CSS::Angle<>> angle;
    if (consumeIdent<CSSValueID::From>(range)) {
        angle = MetaConsumer<CSS::Angle<>>::consume(range, state, { .unitlessZeroAngle = UnitlessZeroQuirk::Allow });
        if (!angle)
            return nullptr;
    }

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueID::At>(range)) {
        position = consumePositionUnresolved(range, state);
        if (!position)
            return nullptr;
    }

    if ((angle || position) && !colorInterpolationMethod && range.peek().id() == CSSValueID::In) {
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
                    .angle = WTF::move(angle),
                    .position = WTF::move(position),
                },
                .stops = WTF::move(*stops),
            }
        }
    );
}

// MARK: <-webkit-cross-fade()>

static RefPtr<CSSValue> consumeWebkitCrossfade(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <-webkit-cross-fade()> = -webkit-cross-fade( [ <image> | none ] , [ <image> | none ] , [ <number [0,1]> | <percentage [0,100]> ] )

    auto from = consumeUnresolvedImageOrNone(args, state);
    if (!from || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    auto to = consumeUnresolvedImageOrNone(args, state);
    if (!to || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    auto progress = MetaConsumer<CSS::Number<CSS::ClosedUnitRangeClampBoth>, CSS::Percentage<CSS::ClosedPercentageRangeClampBoth>>::consume(args, state);
    if (!progress)
        return nullptr;

    return CSSCrossfadeValue::create(CSS::WebkitCrossfadeFunction {
        .parameters = CSS::WebkitCrossfade {
            .from = WTF::move(*from),
            .to = WTF::move(*to),
            .progress = CSS::WebkitCrossfade::Progress { WTF::move(*progress) },
        }
    });
}

static std::optional<CSS::Crossfade::Component> consumeCrossfadeComponent(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <cross-fade-component> = [ <image> | <color> ] && <percentage [0,100]>?
    // https://drafts.csswg.org/css-images-4/#funcdef-cross-fade

    // FIXME: Add support for <color> parameter.

    auto percentage = MetaConsumer<CSS::Crossfade::Component::Percentage>::consume(args, state);

    RefPtr image = consumeImage(args, state);
    if (!image)
        return std::nullopt;

    if (!percentage)
        percentage = MetaConsumer<CSS::Crossfade::Component::Percentage>::consume(args, state);

    return CSS::Crossfade::Component {
        .image = { image.releaseNonNull() },
        .percentage = WTF::move(percentage)
    };
}

static RefPtr<CSSValue> consumeCrossfade(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <cross-fade()> = cross-fade( <cross-fade-component># )
    // https://drafts.csswg.org/css-images-4/#funcdef-cross-fade

    CommaSeparatedVector<CSS::Crossfade::Component> components;
    do {
        auto component = consumeCrossfadeComponent(args, state);
        if (!component)
            return nullptr;
        components.value.append(WTF::move(*component));
    } while (consumeCommaIncludingWhitespace(args));

    if (!args.atEnd())
        return nullptr;

    return CSSCrossfadeValue::create(CSS::CrossfadeFunction {
        .parameters = CSS::Crossfade {
            .components = WTF::move(components)
        }
    });
}

// MARK: <-webkit-canvas()>

static RefPtr<CSSValue> consumeWebkitCanvas(CSSParserTokenRange& args)
{
    if (args.peek().type() != IdentToken)
        return nullptr;
    return CSSCanvasValue::create(CSS::CustomIdent { args.consumeIncludingWhitespace().value().toAtomString() });
}

// MARK: <-webkit-named-image()>

static RefPtr<CSSValue> consumeWebkitNamedImage(CSSParserTokenRange& args)
{
    if (args.peek().type() != IdentToken)
        return nullptr;
    return CSSNamedImageValue::create(CSS::CustomIdent { args.consumeIncludingWhitespace().value().toAtomString() });
}

// MARK: <image()>
// https://drafts.csswg.org/css-images-4/#funcdef-image

static RefPtr<CSSValue> consumeColorImage(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    auto color = consumeUnresolvedColor(args, state);
    if (!color)
        return nullptr;
    return CSSColorImageValue::create(WTF::move(*color));
}

// MARK: light-dark() for images
// https://drafts.csswg.org/css-color-5/#light-dark

static RefPtr<CSSValue> consumeLightDarkImage(CSSParserTokenRange& args, CSS::PropertyParserState& state, OptionSet<AllowedImageType> allowedImageTypes)
{
    auto lightValueOrNone = consumeImageOrNone(args, state, allowedImageTypes);
    if (!lightValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    auto darkValueOrNone = consumeImageOrNone(args, state, allowedImageTypes);
    if (!darkValueOrNone)
        return nullptr;
    return CSSLightDarkImageValue::create(lightValueOrNone.releaseNonNull(), darkValueOrNone.releaseNonNull());
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
    return CSSFilterImageValue::create(imageValueOrNone.releaseNonNull(), WTF::move(*filter));
}

// MARK: <paint()>
// https://drafts.css-houdini.org/css-paint-api/#funcdef-paint

static RefPtr<CSSValue> consumeCustomPaint(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    if (!state.context.cssPaintingAPIEnabled)
        return nullptr;
    if (args.peek().type() != IdentToken)
        return nullptr;
    auto name = args.consumeIncludingWhitespace().value().toAtomString();

    if (!args.atEnd() && args.peek() != CommaToken)
        return nullptr;
    if (!args.atEnd())
        args.consume();

    auto argumentList = CSSVariableData::create(args.consumeAll());
    return CSSPaintImageValue::create(CSS::CustomIdent { WTF::move(name) }, WTF::move(argumentList));
}

// MARK: <image-set()>
// https://w3c.github.io/csswg-drafts/css-images-4/#image-set-notation

struct ImageSetTypeFunctionRaw {
    FunctionNotation<CSSValueID::Type, CSS::String> value;

    bool operator==(const ImageSetTypeFunctionRaw&) const = default;
};
using ImageSetTypeFunction = ImageSetTypeFunctionRaw;

struct ImageSetTypeFunctionRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;

    static std::optional<ImageSetTypeFunctionRaw> consume(CSSParserTokenRange& range, CSS::PropertyParserState&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions)
    {
        ASSERT(range.peek().type() == FunctionToken);
        if (range.peek().functionId() != CSSValueID::Type)
            return { };

        auto rangeCopy = range;
        auto typeArg = consumeFunction(rangeCopy);
        auto result = consumeUnresolvedString(typeArg);

        if (!result || !typeArg.atEnd())
            return { };

        range = rangeCopy;
        return ImageSetTypeFunctionRaw { FunctionNotation<CSSValueID::Type, CSS::String> { WTF::move(*result) } };
    }
};

template<> struct ConsumerDefinition<ImageSetTypeFunction> {
    using FunctionToken = ImageSetTypeFunctionRawKnownTokenTypeFunctionConsumer;
};

// MARK: Image Set Resolution + Type Function

static RefPtr<CSSImageSetOptionValue> consumeImageSetOption(CSSParserTokenRange& range, CSS::PropertyParserState& state, OptionSet<AllowedImageType> allowedImageTypes)
{
    // <image-set()> = image-set( <image-set-option># )
    // <image-set-option> = [ <image> | <string> ] [ <resolution> || type(<string>) ]?
    // https://w3c.github.io/csswg-drafts/css-images-4/#image-set-notation

    auto image = consumeImage(range, state, allowedImageTypes);
    if (!image)
        return nullptr;

    std::optional<CSS::Resolution<>> resolution;
    std::optional<FunctionNotation<CSSValueID::Type, CSS::String>> type;

    // Optional resolution and type in any order.
    for (size_t i = 0; i < 2 && !range.atEnd(); ++i) {
        if (auto optionalArgument = MetaConsumer<CSS::Resolution<>, ImageSetTypeFunction>::consume(range, state)) {
            bool success = WTF::switchOn(WTF::move(*optionalArgument),
                [&](CSS::Resolution<>&& parsedResolution) {
                    if (resolution)
                        return false;
                    resolution = WTF::move(parsedResolution);
                    return true;
                },
                [&](ImageSetTypeFunction&& parsedType) {
                    if (type)
                        return false;
                    type = WTF::move(parsedType.value);
                    return true;
                }
            );
            if (!success)
                return nullptr;
            continue;
        }
        break;
    }

    if (!range.atEnd() && range.peek().type() != CommaToken)
        return nullptr;

    return CSSImageSetOptionValue::create(image.releaseNonNull(), WTF::move(resolution), WTF::move(type));
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

    return CSSImageSetValue::create(WTF::move(imageSet));
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
        return CSSImageValue::create(WTF::move(*imageURL));
    }

    if (range.peek().type() == FunctionToken) {
        auto consumeImageFunction = [&](auto consumer) -> RefPtr<CSSValue> {
            auto rangeCopy = range;
            auto args = consumeFunction(rangeCopy);
            RefPtr result = consumer(args);
            if (!result || !args.atEnd())
                return nullptr;
            range = rangeCopy;
            return result;
        };

        auto consumeGeneratedImage = [&](auto consumer) -> RefPtr<CSSValue> {
            if (!allowedImageTypes.contains(AllowedImageType::GeneratedImage))
                return nullptr;
            return consumeImageFunction(consumer);
        };

        auto consumeImageSetImage = [&](auto consumer) -> RefPtr<CSSValue> {
            if (!allowedImageTypes.contains(AllowedImageType::ImageSet))
                return nullptr;
            return consumeImageFunction(consumer);
        };

        auto functionId = range.peek().functionId();
        switch (functionId) {
        case CSSValueID::RadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeRadialGradient<CSSValueID::RadialGradient>(args, state); });
        case CSSValueID::RepeatingRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeRadialGradient<CSSValueID::RepeatingRadialGradient>(args, state); });
        case CSSValueID::WebkitLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedLinearGradient<CSSValueID::WebkitLinearGradient>(args, state); });
        case CSSValueID::WebkitRepeatingLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedLinearGradient<CSSValueID::WebkitRepeatingLinearGradient>(args, state); });
        case CSSValueID::RepeatingLinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeLinearGradient<CSSValueID::RepeatingLinearGradient>(args, state); });
        case CSSValueID::LinearGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeLinearGradient<CSSValueID::LinearGradient>(args, state); });
        case CSSValueID::WebkitGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeDeprecatedGradient(args, state); });
        case CSSValueID::WebkitRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedRadialGradient<CSSValueID::WebkitRadialGradient>(args, state); });
        case CSSValueID::WebkitRepeatingRadialGradient:
            return consumeGeneratedImage([&](auto& args) { return consumePrefixedRadialGradient<CSSValueID::WebkitRepeatingRadialGradient>(args, state); });
        case CSSValueID::ConicGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeConicGradient<CSSValueID::ConicGradient>(args, state); });
        case CSSValueID::RepeatingConicGradient:
            return consumeGeneratedImage([&](auto& args) { return consumeConicGradient<CSSValueID::RepeatingConicGradient>(args, state); });
        case CSSValueID::WebkitCrossFade:
            return consumeGeneratedImage([&](auto& args) { return consumeWebkitCrossfade(args, state); });
        case CSSValueID::CrossFade:
            return consumeGeneratedImage([&](auto& args) { return consumeCrossfade(args, state); });
        case CSSValueID::WebkitCanvas:
            return consumeGeneratedImage([&](auto& args) { return consumeWebkitCanvas(args); });
        case CSSValueID::WebkitNamedImage:
            return consumeGeneratedImage([&](auto& args) { return consumeWebkitNamedImage(args); });
        case CSSValueID::Image:
            return consumeGeneratedImage([&](auto& args) { return consumeColorImage(args, state); });
        case CSSValueID::LightDark:
            return consumeImageFunction([&](auto& args) { return consumeLightDarkImage(args, state, allowedImageTypes); });
        case CSSValueID::WebkitFilter:
        case CSSValueID::Filter:
            return consumeGeneratedImage([&](auto& args) { return consumeFilterImage(args, state); });
        case CSSValueID::Paint:
            return consumeGeneratedImage([&](auto& args) { return consumeCustomPaint(args, state); });
        case CSSValueID::ImageSet:
        case CSSValueID::WebkitImageSet:
            return consumeImageSetImage([&](auto& args) { return consumeImageSet(args, state, (allowedImageTypes | AllowedImageType::RawStringAsURL) - AllowedImageType::ImageSet); });
        default:
            break;
        }
    }

    if (allowedImageTypes.contains(AllowedImageType::URLFunction)) {
        if (auto imageURL = consumeURLRaw(range, state, { AllowedURLModifiers::CrossOrigin, AllowedURLModifiers::ReferrerPolicy, AllowedURLModifiers::Param }))
            return CSSImageValue::create(WTF::move(*imageURL));
    }

    return nullptr;
}

// MARK: <image> | none

RefPtr<CSSValue> consumeImageOrNone(CSSParserTokenRange& range, CSS::PropertyParserState& state, OptionSet<AllowedImageType> allowedImageTypes)
{
    if (range.peek().id() == CSSValueID::None && allowedImageTypes.contains(AllowedImageType::GeneratedImage))
        return consumeIdent(range);
    return consumeImage(range, state, allowedImageTypes);
}

std::optional<CSS::ImageOrNone> consumeUnresolvedImageOrNone(CSSParserTokenRange& range, CSS::PropertyParserState& state, OptionSet<AllowedImageType> allowedImageTypes)
{
    if (range.peek().id() == CSSValueID::None && allowedImageTypes.contains(AllowedImageType::GeneratedImage)) {
        range.consumeIncludingWhitespace();
        return CSS::ImageOrNone { CSS::Keyword::None { } };
    }
    RefPtr image = consumeImage(range, state, allowedImageTypes);
    if (!image)
        return std::nullopt;
    return CSS::ImageOrNone { CSS::ImageWrapper { image.releaseNonNull() } };
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
