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

#include "CSSFunctionValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Angle.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+LengthPercentage.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParsing.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "TransformOperations.h"
#include "TransformOperationsBuilder.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumeRotate3dFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotate3d
    // rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )

    auto consumeParameters = [](auto& args, auto& context) -> std::optional<CSSValueListBuilder> {
        auto firstValue = consumeNumber(args, context);
        if (!firstValue)
            return { };

        if (!consumeCommaIncludingWhitespace(args))
            return { };

        auto secondValue = consumeNumber(args, context);
        if (!secondValue)
            return { };

        if (!consumeCommaIncludingWhitespace(args))
            return { };

        auto thirdValue = consumeNumber(args, context);
        if (!thirdValue)
            return { };

        if (!consumeCommaIncludingWhitespace(args))
            return { };

        auto angle = consumeAngle(args, context, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (!angle)
            return { };

        CSSValueListBuilder parameters;
        parameters.append(firstValue.releaseNonNull());
        parameters.append(secondValue.releaseNonNull());
        parameters.append(thirdValue.releaseNonNull());
        parameters.append(angle.releaseNonNull());
        return { WTFMove(parameters) };
    };

    auto functionId = range.peek().functionId();
    if (functionId != CSSValueRotate3d)
        return { };

    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);
    if (args.atEnd())
        return { };

    auto parameters = consumeParameters(args, context);
    if (!parameters || !args.atEnd())
        return { };

    range = rangeCopy;
    return CSSFunctionValue::create(functionId, WTFMove(*parameters));
}

RefPtr<CSSValue> consumeTranslateFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translate
    // translate() = translate( <length-percentage> , <length-percentage>? )

    auto consumeParameters = [](auto& args, auto& context) -> std::optional<CSSValueListBuilder> {
        CSSValueListBuilder arguments;

        auto firstValue = consumeLengthPercentage(args, context);
        if (!firstValue)
            return { };
        arguments.append(firstValue.releaseNonNull());

        if (consumeCommaIncludingWhitespace(args)) {
            auto secondValue = consumeLengthPercentage(args, context);
            if (!secondValue)
                return { };
            // A second value of `0` is the same as no second argument, so there is no need to store one if we know it is `0`.
            if (secondValue->isZero() != true)
                arguments.append(secondValue.releaseNonNull());
        }

        return { WTFMove(arguments) };
    };

    auto functionId = range.peek().functionId();
    if (functionId != CSSValueTranslate)
        return { };

    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);
    if (args.atEnd())
        return { };

    auto parameters = consumeParameters(args, context);
    if (!parameters || !args.atEnd())
        return { };

    range = rangeCopy;
    return CSSFunctionValue::create(functionId, WTFMove(*parameters));
}

RefPtr<CSSValue> consumeTranslate3dFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translate3d
    // translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )

    auto consumeParameters = [](auto& args, auto& context) -> std::optional<CSSValueListBuilder> {
        auto firstValue = consumeLengthPercentage(args, context);
        if (!firstValue)
            return { };

        if (!consumeCommaIncludingWhitespace(args))
            return { };

        auto secondValue = consumeLengthPercentage(args, context);
        if (!secondValue)
            return { };

        if (!consumeCommaIncludingWhitespace(args))
            return { };

        auto thirdValue = consumeLength(args, context);
        if (!thirdValue)
            return { };

        CSSValueListBuilder parameters;
        parameters.append(firstValue.releaseNonNull());
        parameters.append(secondValue.releaseNonNull());
        parameters.append(thirdValue.releaseNonNull());
        return { WTFMove(parameters) };
    };

    auto functionId = range.peek().functionId();
    if (functionId != CSSValueTranslate3d)
        return { };

    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);
    if (args.atEnd())
        return { };

    auto parameters = consumeParameters(args, context);
    if (!parameters || !args.atEnd())
        return { };

    range = rangeCopy;
    return CSSFunctionValue::create(functionId, WTFMove(*parameters));
}

RefPtr<CSSValue> consumeTranslate(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-translate
    // none | <length-percentage> [ <length-percentage> <length>? ]?

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    // https://drafts.csswg.org/css-transforms-2/#propdef-translate
    //
    // The translate property accepts 1-3 values, each specifying a translation against one axis, in the order X, Y, then Z.
    // If only one or two values are given, this specifies a 2d translation, equivalent to the translate() function. If the second
    // value is missing, it defaults to 0px. If three values are given, this specifies a 3d translation, equivalent to the
    // translate3d() function.

    auto x = consumeLengthPercentage(range, context);
    if (!x)
        return nullptr;

    range.consumeWhitespace();

    if (range.atEnd())
        return CSSValueList::createSpaceSeparated(x.releaseNonNull());

    auto y = consumeLengthPercentage(range, context);
    if (!y)
        return nullptr;

    range.consumeWhitespace();

    // If we have a calc() or non-zero y value, we can directly add it to the list. We only
    // want to add a zero y value if a non-zero z value is specified.
    // Always include 0% in serialization per-spec.
    bool haveNonZeroY = y->isCalculated() || y->isPercentage() || !*y->isZero();

    if (range.atEnd()) {
        if (!haveNonZeroY)
            return CSSValueList::createSpaceSeparated(x.releaseNonNull());
        return CSSValueList::createSpaceSeparated(x.releaseNonNull(), y.releaseNonNull());
    }

    auto z = consumeLength(range, context);
    if (!z)
        return nullptr;

    // If the z value is a zero value and not a percent value, we have nothing left to add to the list.
    bool haveNonZeroZ = z && (z->isCalculated() || z->isPercentage() || !*z->isZero());

    if (!haveNonZeroY && !haveNonZeroZ)
        return CSSValueList::createSpaceSeparated(x.releaseNonNull());
    if (!haveNonZeroZ)
        return CSSValueList::createSpaceSeparated(x.releaseNonNull(), y.releaseNonNull());
    return CSSValueList::createSpaceSeparated(x.releaseNonNull(), y.releaseNonNull(), z.releaseNonNull());
}

RefPtr<CSSValue> consumeRotate(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-rotate
    // none | <angle> | [ x | y | z | <number>{3} ] && <angle>

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    // https://www.w3.org/TR/css-transforms-2/#propdef-rotate
    //
    // The rotate property accepts an angle to rotate an element, and optionally an axis to rotate it around.
    //
    // If the axis is omitted, this specifies a 2d rotation, equivalent to the rotate() function.
    //
    // Otherwise, it specifies a 3d rotation: if x, y, or z is given, it specifies a rotation around that axis,
    // equivalent to the rotateX()/etc 3d transform functions. Alternately, the axis can be specified explicitly
    // by giving three numbers representing the x, y, and z components of an origin-centered vector, equivalent
    // to the rotate3d() function.

    CSSValueListBuilder list;
    RefPtr<CSSPrimitiveValue> angle;
    RefPtr<CSSPrimitiveValue> axisIdentifier;

    while (!range.atEnd()) {
        // First, attempt to parse a number, which might be in a series of 3 specifying the rotation axis.
        auto parsedValue = consumeNumber(range, context);
        if (parsedValue) {
            // If we've encountered an axis identifier, then this value is invalid.
            if (axisIdentifier)
                return nullptr;
            list.append(parsedValue.releaseNonNull());
            range.consumeWhitespace();
            continue;
        }

        // Then, attempt to parse an angle. We try this as a fallback rather than the first option because
        // a unitless 0 angle would be consumed as an angle.
        parsedValue = consumeAngle(range, context, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (parsedValue) {
            // If we had already parsed an angle or numbers but not 3 in a row, this value is invalid.
            if (angle || (!list.isEmpty() && list.size() != 3))
                return nullptr;
            angle = WTFMove(parsedValue);
            range.consumeWhitespace();
            continue;
        }

        // Finally, attempt to parse one of the axis identifiers.
        parsedValue = consumeIdent<CSSValueX, CSSValueY, CSSValueZ>(range);
        // If we failed to find one of those identifiers or one was already specified, or we'd previously
        // encountered numbers to specify a rotation axis, then this value is invalid.
        if (!parsedValue || axisIdentifier || !list.isEmpty())
            return nullptr;
        axisIdentifier = WTFMove(parsedValue);
        range.consumeWhitespace();
    }

    // We must have an angle to have a valid value.
    if (!angle)
        return nullptr;

    auto knownToBeZero = [](std::optional<bool> value) -> bool {
        return !value ? false : *value == true;
    };

    auto knownToBeNotZero = [](std::optional<bool> value) -> bool {
        return !value ? false : *value == false;
    };

    if (list.size() == 3) {
        // The first valid case is if we have 3 items in the list, meaning we parsed three consecutive number values
        // to specify the rotation axis. In that case, we must not also have encountered an axis identifier.
        ASSERT(!axisIdentifier);

        // Now we must check the values since if we have a vector in the x, y or z axis alone we must serialize to the
        // matching identifier.
        auto xIsZero = downcast<CSSPrimitiveValue>(list[0].get()).isZero();
        auto yIsZero = downcast<CSSPrimitiveValue>(list[1].get()).isZero();
        auto zIsZero = downcast<CSSPrimitiveValue>(list[2].get()).isZero();

        if (knownToBeNotZero(xIsZero) && knownToBeZero(yIsZero) && knownToBeZero(zIsZero))
            return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueX), angle.releaseNonNull());
        if (knownToBeZero(xIsZero) && knownToBeNotZero(yIsZero) && knownToBeZero(zIsZero))
            return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueY), angle.releaseNonNull());
        if (knownToBeZero(xIsZero) && knownToBeZero(yIsZero) && knownToBeNotZero(zIsZero))
            return CSSValueList::createSpaceSeparated(angle.releaseNonNull());

        list.append(angle.releaseNonNull());
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    }

    if (list.isEmpty()) {
        // The second valid case is if we have no item in the list, meaning we have either an optional rotation axis
        // using an identifier. In that case, we must add the axis identifier is specified and then add the angle.
        if (axisIdentifier && axisIdentifier->valueID() != CSSValueZ)
            return CSSValueList::createSpaceSeparated(axisIdentifier.releaseNonNull(), angle.releaseNonNull());
        return CSSValueList::createSpaceSeparated(angle.releaseNonNull());
    }

    return nullptr;
}

RefPtr<CSSValue> consumeScale(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-scale
    // none | [ <number> | <percentage> ]{1,3}

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    // https://www.w3.org/TR/css-transforms-2/#propdef-scale
    //
    // The scale property accepts 1-3 values, each specifying a scale along one axis, in order X, Y, then Z.
    //
    // If only the X value is given, the Y value defaults to the same value.
    //
    // If one or two values are given, this specifies a 2d scaling, equivalent to the scale() function.
    // If three values are given, this specifies a 3d scaling, equivalent to the scale3d() function.

    auto x = consumePercentageDividedBy100OrNumber(range, context);
    if (!x)
        return nullptr;

    range.consumeWhitespace();

    if (range.atEnd())
        return CSSValueList::createSpaceSeparated(x.releaseNonNull());

    auto y = consumePercentageDividedBy100OrNumber(range, context);
    if (!y)
        return nullptr;

    range.consumeWhitespace();

    auto xValue = x->resolveAsNumberIfNotCalculated();
    auto yValue = y->resolveAsNumberIfNotCalculated();

    if (range.atEnd()) {
        if (!xValue || !yValue || *xValue != *yValue)
            return CSSValueList::createSpaceSeparated(x.releaseNonNull(), y.releaseNonNull());

        return CSSValueList::createSpaceSeparated(x.releaseNonNull());
    }

    auto z = consumePercentageDividedBy100OrNumber(range, context);
    if (!z)
        return nullptr;

    auto zValue = z->resolveAsNumberIfNotCalculated();

    if (zValue != 1.0)
        return CSSValueList::createSpaceSeparated(x.releaseNonNull(), y.releaseNonNull(), z.releaseNonNull());

    if (!xValue || !yValue || *xValue != *yValue)
        return CSSValueList::createSpaceSeparated(x.releaseNonNull(), y.releaseNonNull());

    return CSSValueList::createSpaceSeparated(x.releaseNonNull());
}

std::optional<TransformOperations> parseTransformRaw(const String& string, const CSSParserContext& context, const CSSToLengthConversionData& conversionData)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto parsedValue = CSSPropertyParsing::consumeTransform(range, context);
    if (!parsedValue)
        return { };

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    if (!parsedValue->canResolveDependenciesWithConversionData(conversionData))
        return { };

    return Style::createTransformOperations(*parsedValue, conversionData);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
