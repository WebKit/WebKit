/*
// Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+Transform.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParsing.h"
#include "CSSRotatePropertyValue.h"
#include "CSSScalePropertyValue.h"
#include "CSSToLengthConversionData.h"
#include "CSSTransformFunctionValue.h"
#include "CSSTransformListValue.h"
#include "CSSTransformPropertyValue.h"
#include "CSSTranslatePropertyValue.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleTransformProperty.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename T, unsigned Count> static std::optional<CSS::CommaSeparatedArray<T, Count>> consumeFixedNumberCommaSeparatedValuesOfType(CSSParserTokenRange& args, const CSSParserContext& context)
{
    std::array<std::optional<T>, Count> results;

    for (unsigned i = 0; i < Count; ++i) {
        results[i] = MetaConsumer<T>::consume(args, context, { }, { .parserMode = context.mode });
        if (!results[i])
            return { };

        if (i < (Count - 1) && !consumeCommaIncludingWhitespace(args))
            return { };
    }

    return WTF::apply([&](auto& ...x) { return CSS::CommaSeparatedArray<T, Count> { WTFMove(*x)... }; }, results);
}

// MARK: Matrix

static std::optional<CSS::Matrix> consumeMatrixFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-matrix
    // matrix() = matrix( <number>#{6} )

    auto numbers = consumeFixedNumberCommaSeparatedValuesOfType<CSS::Number<>, 6>(args, context);
    if (!numbers)
        return { };
    return CSS::Matrix { .value = WTFMove(*numbers) };
}

static std::optional<CSS::Matrix3D> consumeMatrix3dFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-matrix3d
    // matrix3d() = matrix3d( <number>#{16} )

    auto numbers = consumeFixedNumberCommaSeparatedValuesOfType<CSS::Number<>, 16>(args, context);
    if (!numbers)
        return { };
    return CSS::Matrix3D { .value = WTFMove(*numbers) };
}

// MARK: Rotate

static std::optional<CSS::Rotate> consumeRotateFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-rotate
    // rotate() = rotate( [ <angle> | <zero> ] )

    auto angle = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!angle)
        return { };
    return CSS::Rotate { .value = WTFMove(*angle) };
}

static std::optional<CSS::Rotate3D> consumeRotate3dFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotate3d
    // rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )

    auto x = MetaConsumer<CSS::Number<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!x)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto y = MetaConsumer<CSS::Number<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!y)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto z = MetaConsumer<CSS::Number<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!z)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto angle = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!angle)
        return { };

    return CSS::Rotate3D {
        .x = WTFMove(*x),
        .y = WTFMove(*y),
        .z = WTFMove(*z),
        .angle = WTFMove(*angle)
    };
}

static std::optional<CSS::RotateX> consumeRotateXFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatex
    // rotateX() = rotateX( [ <angle> | <zero> ] )

    auto angle = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!angle)
        return { };
    return CSS::RotateX { .value = WTFMove(*angle) };
}

static std::optional<CSS::RotateY> consumeRotateYFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatey
    // rotateY() = rotateY( [ <angle> | <zero> ] )

    auto angle = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!angle)
        return { };
    return CSS::RotateY { .value = WTFMove(*angle) };
}

static std::optional<CSS::RotateZ> consumeRotateZFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatez
    // rotateZ() = rotateZ( [ <angle> | <zero> ] )

    auto angle = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!angle)
        return { };
    return CSS::RotateZ { .value = WTFMove(*angle) };
}

// MARK: Skew

static std::optional<CSS::Skew> consumeSkewFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skew
    // skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )
    // NOTE: Second parameter defaults to `0`.

    auto x = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!x)
        return { };

    std::optional<CSS::Angle<>> y;
    if (consumeCommaIncludingWhitespace(args)) {
        auto yValue = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
        if (!yValue)
            return { };
        y = WTFMove(yValue);
    }

    return CSS::Skew {
        .x = WTFMove(*x),
        .y = WTFMove(y)
    };
}

static std::optional<CSS::SkewX> consumeSkewXFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewx
    // skewX() = skewX( [ <angle> | <zero> ] )

    auto angle = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!angle)
        return { };
    return CSS::SkewX { .value = WTFMove(*angle) };
}

static std::optional<CSS::SkewY> consumeSkewYFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewy
    // skewY() = skewY( [ <angle> | <zero> ] )

    auto angle = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!angle)
        return { };
    return CSS::SkewY { .value = WTFMove(*angle) };
}

// MARK: Scale

static std::optional<CSS::Scale> consumeScaleFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale
    // scale() = scale( [ <number> | <percentage> ]#{1,2} )
    // NOTE: Second parameter defaults to first parameter's value.

    auto xValue = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!xValue)
        return { };
    auto x = CSS::NumberOrPercentageResolvedToNumber { WTFMove(*xValue) };

    std::optional<CSS::NumberOrPercentageResolvedToNumber> y;
    if (consumeCommaIncludingWhitespace(args)) {
        auto yValue = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });
        if (!yValue)
            return { };
        y = CSS::NumberOrPercentageResolvedToNumber { WTFMove(*yValue) };
    }

    return CSS::Scale {
        .x = WTFMove(x),
        .y = WTFMove(y)
    };
}

static std::optional<CSS::Scale3D> consumeScale3dFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale3d
    // scale3d() = scale3d( [ <number> | <percentage> ]#{3} )

    auto x = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!x)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto y = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!y)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto z = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!z)
        return { };

    return CSS::Scale3D {
        .x = { WTFMove(*x) },
        .y = { WTFMove(*y) },
        .z = { WTFMove(*z) },
    };
}

static std::optional<CSS::ScaleX> consumeScaleXFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalex
    // scaleX() = scaleX( [ <number> | <percentage> ] )

    auto value = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!value)
        return { };

    return CSS::ScaleX { .value = { WTFMove(*value) } };
}

static std::optional<CSS::ScaleY> consumeScaleYFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scaley
    // scaleY() = scaleY( [ <number> | <percentage> ] )

    auto value = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!value)
        return { };

    return CSS::ScaleY { .value = { WTFMove(*value) } };
}

static std::optional<CSS::ScaleZ> consumeScaleZFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalez
    // scaleZ() = scaleZ( [ <number> | <percentage> ] )

    auto value = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!value)
        return { };

    return CSS::ScaleZ { .value = { WTFMove(*value) } };
}

// MARK: Translate

static std::optional<CSS::Translate> consumeTranslateFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translate
    // translate() = translate( <length-percentage> , <length-percentage>? )
    // NOTE: Second parameter defaults to `0`.

    auto x = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!x)
        return { };

    std::optional<CSS::LengthPercentage<>> y;
    if (consumeCommaIncludingWhitespace(args)) {
        auto yValue = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
        if (!yValue)
            return { };

        // Don't set `y` if the value parsed is `0`.
        if (!yValue->isKnownZero())
            y = WTFMove(yValue);
    }

    return CSS::Translate {
        .x = WTFMove(*x),
        .y = WTFMove(y)
    };
}

static std::optional<CSS::Translate3D> consumeTranslate3dFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translate3d
    // translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )

    auto x = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!x)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto y = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!y)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto z = MetaConsumer<CSS::Length<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!z)
        return { };

    return CSS::Translate3D {
        .x = WTFMove(*x),
        .y = WTFMove(*y),
        .z = WTFMove(*z),
    };
}

static std::optional<CSS::TranslateX> consumeTranslateXFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatex
    // translateX() = translateX( <length-percentage> )

    auto value = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!value)
        return { };

    return CSS::TranslateX { .value = WTFMove(*value) };
}

static std::optional<CSS::TranslateY> consumeTranslateYFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatey
    // translateY() = translateY( <length-percentage> )

    auto value = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!value)
        return { };

    return CSS::TranslateY { .value = WTFMove(*value) };
}

static std::optional<CSS::TranslateZ> consumeTranslateZFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translatez
    // translateZ() = translateZ( <length> )

    auto value = MetaConsumer<CSS::Length<>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!value)
        return { };

    return CSS::TranslateZ { .value = WTFMove(*value) };
}

// MARK: Perspective

static std::optional<CSS::Perspective> consumePerspectiveFunctionArguments(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-perspective
    // perspective() = perspective( [ <length [0,∞]> | none ] )

    if (consumeIdentRaw<CSSValueNone>(args))
        return CSS::Perspective { .value = CSS::None { } };

    auto length = MetaConsumer<CSS::Length<CSS::Nonnegative>>::consume(args, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (length)
        return CSS::Perspective { .value = WTFMove(*length) };

    // FIXME: Non-standard addition.
    auto number = MetaConsumer<CSS::Number<CSS::Nonnegative>>::consume(args, context, { }, { .parserMode = context.mode });
    if (number) {
        if (auto raw = number->raw())
            return CSS::Perspective { .value = { CSS::LengthRaw<CSS::Nonnegative> { CSSUnitType::CSS_PX, raw->value } } };
    }

    return { };
}

// MARK: <transform-function>

template<CSSValueID Name, typename T> static CSS::TransformFunction toTransformFunction(T&& parameters)
{
    return CSS::TransformFunction { CSS::FunctionNotation<Name, T> { WTFMove(parameters) } };
}

template<CSSValueID Name, typename T> static std::optional<CSS::TransformFunction> toTransformFunction(std::optional<T>&& parameters)
{
    if (!parameters)
        return { };
    return toTransformFunction<Name>(WTFMove(*parameters));
}

std::optional<CSS::TransformFunction> consumeUnresolvedTransformFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#typedef-transform-function
    // <transform-function> = <matrix()> | <translate()> | <translateX()> | <translateY()> | <scale()> | <scaleX()> | <scaleY()> | <rotate()> | <skew()> | <skewX()> | <skewY()> | <matrix3d()> | <translate3d()> | <translateZ()> | <scale3d()> | <scaleZ()> | <rotate3d()> | <rotateX()> | <rotateY()> | <rotateZ()> | <perspective()

    auto functionId = range.peek().functionId();
    if (functionId == CSSValueInvalid)
        return { };

    auto args = consumeFunction(range);
    if (args.atEnd())
        return { };

    std::optional<CSS::TransformFunction> result;
    switch (functionId) {
    case CSSValueMatrix:
        result = toTransformFunction<CSSValueMatrix>(consumeMatrixFunctionArguments(args, context));
        break;
    case CSSValueMatrix3d:
        result = toTransformFunction<CSSValueMatrix3d>(consumeMatrix3dFunctionArguments(args, context));
        break;
    case CSSValueRotate:
        result = toTransformFunction<CSSValueRotate>(consumeRotateFunctionArguments(args, context));
        break;
    case CSSValueRotate3d:
        result = toTransformFunction<CSSValueRotate3d>(consumeRotate3dFunctionArguments(args, context));
        break;
    case CSSValueRotateX:
        result = toTransformFunction<CSSValueRotateX>(consumeRotateXFunctionArguments(args, context));
        break;
    case CSSValueRotateY:
        result = toTransformFunction<CSSValueRotateY>(consumeRotateYFunctionArguments(args, context));
        break;
    case CSSValueRotateZ:
        result = toTransformFunction<CSSValueRotateZ>(consumeRotateZFunctionArguments(args, context));
        break;
    case CSSValueSkew:
        result = toTransformFunction<CSSValueSkew>(consumeSkewFunctionArguments(args, context));
        break;
    case CSSValueSkewX:
        result = toTransformFunction<CSSValueSkewX>(consumeSkewXFunctionArguments(args, context));
        break;
    case CSSValueSkewY:
        result = toTransformFunction<CSSValueSkewY>(consumeSkewYFunctionArguments(args, context));
        break;
    case CSSValueScale:
        result = toTransformFunction<CSSValueScale>(consumeScaleFunctionArguments(args, context));
        break;
    case CSSValueScale3d:
        result = toTransformFunction<CSSValueScale3d>(consumeScale3dFunctionArguments(args, context));
        break;
    case CSSValueScaleX:
        result = toTransformFunction<CSSValueScaleX>(consumeScaleXFunctionArguments(args, context));
        break;
    case CSSValueScaleY:
        result = toTransformFunction<CSSValueScaleY>(consumeScaleYFunctionArguments(args, context));
        break;
    case CSSValueScaleZ:
        result = toTransformFunction<CSSValueScaleZ>(consumeScaleZFunctionArguments(args, context));
        break;
    case CSSValueTranslate:
        result = toTransformFunction<CSSValueTranslate>(consumeTranslateFunctionArguments(args, context));
        break;
    case CSSValueTranslate3d:
        result = toTransformFunction<CSSValueTranslate3d>(consumeTranslate3dFunctionArguments(args, context));
        break;
    case CSSValueTranslateX:
        result = toTransformFunction<CSSValueTranslateX>(consumeTranslateXFunctionArguments(args, context));
        break;
    case CSSValueTranslateY:
        result = toTransformFunction<CSSValueTranslateY>(consumeTranslateYFunctionArguments(args, context));
        break;
    case CSSValueTranslateZ:
        result = toTransformFunction<CSSValueTranslateZ>(consumeTranslateZFunctionArguments(args, context));
        break;
    case CSSValuePerspective:
        result = toTransformFunction<CSSValuePerspective>(consumePerspectiveFunctionArguments(args, context));
        break;
    default:
        return { };
    }

    if (!result || !args.atEnd())
        return { };

    return result;
}

std::optional<CSS::TransformList> consumeUnresolvedTransformList(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#typedef-transform-list
    // <transform-list> = <transform-function>+

    CSS::TransformList::List::VariantList list;
    do {
        auto transformFunction = consumeUnresolvedTransformFunction(range, context);
        if (!transformFunction)
            return { };
        WTF::switchOn(WTFMove(*transformFunction), [&](auto&& alternative) { list.append(WTFMove(alternative)); });
    } while (!range.atEnd());

    return CSS::TransformList { WTFMove(list) };
}

std::optional<CSS::TransformProperty> consumeUnresolvedTransform(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#transform-property
    // none | <transform-list>

    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::TransformProperty { CSS::None { } };
    }
    if (auto transformList = consumeUnresolvedTransformList(range, context))
        return CSS::TransformProperty { WTFMove(*transformList) };
    return { };
}

std::optional<CSS::TranslateProperty> consumeUnresolvedTranslate(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-translate
    // none | <length-percentage> [ <length-percentage> <length>? ]?

    // The translate property accepts 1-3 values, each specifying a translation against one axis, in the order X, Y, then Z.
    // If only one or two values are given, this specifies a 2d translation, equivalent to the translate() function. If the second
    // value is missing, it defaults to 0px. If three values are given, this specifies a 3d translation, equivalent to the
    // translate3d() function.

    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::TranslateProperty { CSS::None { } };
    }

    auto x = MetaConsumer<CSS::LengthPercentage<>>::consume(range, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!x)
        return { };

    range.consumeWhitespace();
    if (range.atEnd()) {
        return CSS::TranslateProperty { CSS::Translate {
            WTFMove(*x),
            CSS::LengthPercentage<> { CSS::LengthPercentageRaw<> { CSSUnitType::CSS_PX, 0 } },
        } };
    }

    auto y = MetaConsumer<CSS::LengthPercentage<>>::consume(range, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!y)
        return { };

    range.consumeWhitespace();
    if (range.atEnd()) {
        return CSS::TranslateProperty { CSS::Translate {
            WTFMove(*x),
            WTFMove(*y),
        } };
    }

    auto z = MetaConsumer<CSS::Length<>>::consume(range, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
    if (!z)
        return { };

    return CSS::TranslateProperty { CSS::Translate3D { WTFMove(*x), WTFMove(*y), WTFMove(*z) } };
}

std::optional<CSS::ScaleProperty> consumeUnresolvedScale(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-scale
    // none | [ <number> | <percentage> ]{1,3}

    // The scale property accepts 1-3 values, each specifying a scale along one axis, in order X, Y, then Z.
    //
    // If only the X value is given, the Y value defaults to the same value.
    //
    // If one or two values are given, this specifies a 2d scaling, equivalent to the scale() function.
    // If three values are given, this specifies a 3d scaling, equivalent to the scale3d() function.

    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::ScaleProperty { CSS::None { } };
    }

    auto x = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(range, context, { }, { .parserMode = context.mode });
    if (!x)
        return { };

    range.consumeWhitespace();
    if (range.atEnd()) {
        auto y = x;
        return CSS::ScaleProperty { CSS::Scale { {
            WTFMove(*x) },
            CSS::NumberOrPercentageResolvedToNumber { WTFMove(*y) },
        } };
    }
    auto y = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(range, context, { }, { .parserMode = context.mode });
    if (!y)
        return { };

    range.consumeWhitespace();
    if (range.atEnd()) {
        return CSS::ScaleProperty { CSS::Scale { {
            WTFMove(*x) },
            CSS::NumberOrPercentageResolvedToNumber { WTFMove(*y) },
        } };
    }

    auto z = MetaConsumer<CSS::Number<>, CSS::Percentage<>>::consume(range, context, { }, { .parserMode = context.mode });
    if (!z)
        return { };

    return CSS::ScaleProperty { CSS::Scale3D { { WTFMove(*x) }, { WTFMove(*y) }, { WTFMove(*z) } } };
}

std::optional<CSS::RotateProperty> consumeUnresolvedRotate(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-rotate
    // none | <angle> | [ x | y | z | <number>{3} ] && <angle>

    // The rotate property accepts an angle to rotate an element, and optionally an axis to rotate it around.
    //
    // If the axis is omitted, this specifies a 2d rotation, equivalent to the rotate() function.
    //
    // Otherwise, it specifies a 3d rotation: if x, y, or z is given, it specifies a rotation around that axis,
    // equivalent to the rotateX()/etc 3d transform functions. Alternately, the axis can be specified explicitly
    // by giving three numbers representing the x, y, and z components of an origin-centered vector, equivalent
    // to the rotate3d() function.

    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::RotateProperty { CSS::None { } };
    }

    Vector<CSS::Number<>> list;
    std::optional<CSS::Angle<>> angle;
    std::optional<CSSValueID> axisIdentifier;

    while (!range.atEnd()) {
        // First, attempt to parse a number, which might be in a series of 3 specifying the rotation axis.
        auto parsedNumber = MetaConsumer<CSS::Number<>>::consume(range, context, { }, { .parserMode = context.mode });
        if (parsedNumber) {
            // If we've encountered an axis identifier, then this value is invalid.
            if (axisIdentifier)
                return { };
            list.append(WTFMove(*parsedNumber));
            range.consumeWhitespace();
            continue;
        }

        // Then, attempt to parse an angle. We try this as a fallback rather than the first option because
        // a unitless 0 angle would be consumed as an angle.
        auto parsedAngle = MetaConsumer<CSS::Angle<>>::consume(range, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
        if (parsedAngle) {
            // If we had already parsed an angle or numbers but not 3 in a row, this value is invalid.
            if (angle || (!list.isEmpty() && list.size() != 3))
                return { };
            angle = WTFMove(*parsedAngle);
            range.consumeWhitespace();
            continue;
        }

        // Finally, attempt to parse one of the axis identifiers.
        auto parsedAxisIdentifier = consumeIdentRaw<CSSValueX, CSSValueY, CSSValueZ>(range);

        // If we failed to find one of those identifiers or one was already specified, or we'd previously
        // encountered numbers to specify a rotation axis, then this value is invalid.
        if (!parsedAxisIdentifier || axisIdentifier || !list.isEmpty())
            return { };
        axisIdentifier = WTFMove(*parsedAxisIdentifier);
        range.consumeWhitespace();
    }

    // We must have an angle to have a valid value.
    if (!angle)
        return { };

    if (list.size() == 3) {
        // The first valid case is if we have 3 items in the list, meaning we parsed three consecutive number values
        // to specify the rotation axis. In that case, we must not also have encountered an axis identifier.
        ASSERT(!axisIdentifier);

        return CSS::RotateProperty { CSS::Rotate3D { WTFMove(list[0]), WTFMove(list[1]), WTFMove(list[2]), WTFMove(*angle) } };
    }

    if (list.isEmpty()) {
        // The second valid case is if we have no item in the list, meaning we have either an optional rotation axis
        // using an identifier.
        if (axisIdentifier) {
            switch (*axisIdentifier) {
            case CSSValueX:
                return CSS::RotateProperty { CSS::Rotate3D { { CSS::NumberRaw<> { 1 } }, CSS::NumberRaw<> { 0 }, CSS::NumberRaw<> { 0 }, WTFMove(*angle) } };
            case CSSValueY:
                return CSS::RotateProperty { CSS::Rotate3D { CSS::NumberRaw<> { 0 }, CSS::NumberRaw<> { 1 }, CSS::NumberRaw<> { 0 }, WTFMove(*angle) } };
            default:
                ASSERT(*axisIdentifier == CSSValueZ);
                break;
            }
        }

        return CSS::RotateProperty { CSS::Rotate { WTFMove(*angle) } };
    }

    return { };
}

// MARK: - CSSValue Consumers

RefPtr<CSSValue> consumeTransformFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#typedef-transform-function
    // <transform-function> = <matrix()> | <translate()> | <translateX()> | <translateY()> | <scale()> | <scaleX()> | <scaleY()> | <rotate()> | <skew()> | <skewX()> | <skewY()> | <matrix3d()> | <translate3d()> | <translateZ()> | <scale3d()> | <scaleZ()> | <rotate3d()> | <rotateX()> | <rotateY()> | <rotateZ()> | <perspective()

    auto function = consumeUnresolvedTransformFunction(range, context);
    if (!function)
        return nullptr;
    return CSSTransformFunctionValue::create(WTFMove(*function));
}

RefPtr<CSSValue> consumeTransformList(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#typedef-transform-list
    // <transform-list> = <transform-function>+

    CSSValueListBuilder list;
    do {
        auto transformFunction = consumeTransformFunction(range, context);
        if (!transformFunction)
            return nullptr;
        list.append(transformFunction.releaseNonNull());
    } while (!range.atEnd());

    return CSSTransformListValue::create(WTFMove(list));
}

RefPtr<CSSValue> consumeTransform(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#transform-property
    // none | <transform-list>

    auto transform = consumeUnresolvedTransform(range, context);
    if (!transform)
        return nullptr;
    return CSSTransformPropertyValue::create(WTFMove(*transform));
}

RefPtr<CSSValue> consumeTranslate(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-translate
    // none | <length-percentage> [ <length-percentage> <length>? ]?

    auto translate = consumeUnresolvedTranslate(range, context);
    if (!translate)
        return nullptr;
    return CSSTranslatePropertyValue::create(WTFMove(*translate));
}

RefPtr<CSSValue> consumeRotate(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-rotate
    // none | <angle> | [ x | y | z | <number>{3} ] && <angle>

    auto rotate = consumeUnresolvedRotate(range, context);
    if (!rotate)
        return nullptr;
    return CSSRotatePropertyValue::create(WTFMove(*rotate));
}

RefPtr<CSSValue> consumeScale(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-scale
    // none | [ <number> | <percentage> ]{1,3}

    auto scale = consumeUnresolvedScale(range, context);
    if (!scale)
        return nullptr;
    return CSSScalePropertyValue::create(WTFMove(*scale));
}

std::optional<Style::TransformProperty> parseTransformRaw(const String& string, const CSSParserContext& context, const CSSToLengthConversionData& conversionData)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto transform = consumeUnresolvedTransform(range, context);
    if (!transform)
        return { };

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    auto dependencies = CSS::collectComputedStyleDependencies(*transform);
    if (!dependencies.canResolveDependenciesWithConversionData(conversionData))
        return { };

    return Style::toStyle(*transform, conversionData);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
