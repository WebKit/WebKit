/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+TimingFunction.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Integer.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueKeywords.h"
#include "TimingFunction.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: - <steps()>
// https://drafts.csswg.org/css-easing-1/#funcdef-step-easing-function-steps

static RefPtr<CSSValue> consumeSteps(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <steps-easing-function> = steps(<integer>, <step-position>?)
    // <step-position> = jump-start | jump-end | jump-none | jump-both | start | end

    // with range constraints, this is:

    // <steps-easing-function> = steps(<integer [1,∞]>, <step-position-except-none>?)
    //                         | steps(<integer [2,∞]>, jump-none)
    // <step-position-except-none> = jump-start | jump-end | jump-both | start | end


    ASSERT(range.peek().functionId() == CSSValueSteps);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    auto stepsValue = consumePositiveInteger(args, context);
    if (!stepsValue)
        return nullptr;

    std::optional<StepsTimingFunction::StepPosition> stepPosition;
    if (consumeCommaIncludingWhitespace(args)) {
        switch (args.consumeIncludingWhitespace().id()) {
        case CSSValueJumpStart:
            stepPosition = StepsTimingFunction::StepPosition::JumpStart;
            break;
        case CSSValueJumpEnd:
            stepPosition = StepsTimingFunction::StepPosition::JumpEnd;
            break;
        case CSSValueJumpNone:
            stepPosition = StepsTimingFunction::StepPosition::JumpNone;
            break;
        case CSSValueJumpBoth:
            stepPosition = StepsTimingFunction::StepPosition::JumpBoth;
            break;
        case CSSValueStart:
            stepPosition = StepsTimingFunction::StepPosition::Start;
            break;
        case CSSValueEnd:
            stepPosition = StepsTimingFunction::StepPosition::End;
            break;
        default:
            return nullptr;
        }
    }

    if (!args.atEnd())
        return nullptr;

    // "The first parameter specifies the number of intervals in the function. It must be a
    //  positive integer greater than 0 unless the second parameter is jump-none in which
    //  case it must be a positive integer greater than 1."

    if (stepPosition == StepsTimingFunction::StepPosition::JumpNone && stepsValue->isOne().value_or(false))
        return nullptr;

    range = rangeCopy;
    return CSSStepsTimingFunctionValue::create(stepsValue.releaseNonNull(), stepPosition);
}

// MARK: - <linear()>
// https://drafts.csswg.org/css-easing-2/#the-linear-easing-function

static std::optional<CSSLinearTimingFunctionValue::LinearStop::Length> consumeLinearStopLength(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <linear-stop-length> = <percentage>{1,2}

    auto input = consumePercentage(args, context);
    if (!input)
        return std::nullopt;

    auto extra = consumePercentage(args, context);

    return CSSLinearTimingFunctionValue::LinearStop::Length {
        .input = input.releaseNonNull(),
        .extra = WTFMove(extra)
    };
}

static std::optional<CSSLinearTimingFunctionValue::LinearStop> consumeLinearStop(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <linear-stop> = <number> && <linear-stop-length>?

    auto output = consumeNumber(args, context);
    if (!output)
        return std::nullopt;

    auto input = consumeLinearStopLength(args, context);

    return CSSLinearTimingFunctionValue::LinearStop {
        .output = output.releaseNonNull(),
        .input = WTFMove(input)
    };
}

static RefPtr<CSSValue> consumeLinear(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <linear-easing-function> = linear(<linear-stop-list>)
    // <linear-stop-list> = [ <linear-stop> ]#
    // <linear-stop> = <number> && <linear-stop-length>?
    // <linear-stop-length> = <percentage>{1,2}

    ASSERT(range.peek().functionId() == CSSValueLinear);
    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);

    Vector<CSSLinearTimingFunctionValue::LinearStop> stops;

    while (true) {
        auto linearStop = consumeLinearStop(args, context);
        if (!linearStop)
            break;

        stops.append(WTFMove(*linearStop));

        if (!consumeCommaIncludingWhitespace(args))
            break;
    }

    if (!args.atEnd() || stops.size() < 2)
        return nullptr;

    range = rangeCopy;
    return CSSLinearTimingFunctionValue::create(WTFMove(stops));
}

// MARK: - <cubic-bezier()>
// https://drafts.csswg.org/css-easing-1/#funcdef-cubic-bezier-easing-function-cubic-bezier

static RefPtr<CSSValue> consumeCubicBezier(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // cubic-bezier(<number [0,1]>, <number>, <number [0,1]>, <number>)

    ASSERT(range.peek().functionId() == CSSValueCubicBezier);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    auto x1 = consumeNumber(args, context, ValueRange::NonNegative);
    if (!x1 || (!x1->isCalculated() && x1->resolveAsNumberNoConversionDataRequired() > 1))
        return nullptr;

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto y1 = consumeNumber(args, context);
    if (!y1)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto x2 = consumeNumber(args, context, ValueRange::NonNegative);
    if (!x2 || (!x2->isCalculated() && x2->resolveAsNumberNoConversionDataRequired() > 1))
        return nullptr;

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto y2 = consumeNumber(args, context);
    if (!y2)
        return nullptr;

    if (!args.atEnd())
        return nullptr;

    range = rangeCopy;
    return CSSCubicBezierTimingFunctionValue::create(x1.releaseNonNull(), y1.releaseNonNull(), x2.releaseNonNull(), y2.releaseNonNull());
}

// MARK: - <spring()>
// Non-standard

static RefPtr<CSSValue> consumeSpringFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // spring(<number [>0,∞]>, <number [>0,∞]>, <number [0,∞]>, <number>)

    ASSERT(range.peek().functionId() == CSSValueSpring);

    if (!context.springTimingFunctionEnabled)
        return nullptr;

    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    // FIXME: Contexts that allow calc() should not be defined using a closed interval - https://drafts.csswg.org/css-values-4/#calc-range
    // If spring() ever goes further with standardization, the allowable ranges for `mass` and `stiffness` should be reconsidered.

    // Mass must be greater than 0.
    auto mass = consumeNumber(args, context);
    if (!mass || !mass->isPositive().value_or(false))
        return nullptr;

    // Stiffness must be greater than 0.
    auto stiffness = consumeNumber(args, context);
    if (!stiffness || !stiffness->isPositive().value_or(false))
        return nullptr;

    // Damping coefficient must be greater than or equal to 0.
    auto damping = consumeNumber(args, context, ValueRange::NonNegative);
    if (!damping)
        return nullptr;

    // Initial velocity may have any value.
    auto initialVelocity = consumeNumber(args, context);
    if (!initialVelocity)
        return nullptr;

    if (!args.atEnd())
        return nullptr;

    range = rangeCopy;
    return CSSSpringTimingFunctionValue::create(mass.releaseNonNull(), stiffness.releaseNonNull(), damping.releaseNonNull(), initialVelocity.releaseNonNull());
}

// MARK: - <timing-function>

RefPtr<CSSValue> consumeTimingFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (range.peek().id()) {
    case CSSValueLinear:
    case CSSValueEase:
    case CSSValueEaseIn:
    case CSSValueEaseOut:
    case CSSValueEaseInOut:
        return consumeIdent(range);

    case CSSValueStepStart:
        range.consumeIncludingWhitespace();
        return CSSStepsTimingFunctionValue::create(CSSPrimitiveValue::createInteger(1), StepsTimingFunction::StepPosition::Start);

    case CSSValueStepEnd:
        range.consumeIncludingWhitespace();
        return CSSStepsTimingFunctionValue::create(CSSPrimitiveValue::createInteger(1), StepsTimingFunction::StepPosition::End);

    default:
        break;
    }

    switch (range.peek().functionId()) {
    case CSSValueLinear:
        return consumeLinear(range, context);

    case CSSValueCubicBezier:
        return consumeCubicBezier(range, context);

    case CSSValueSteps:
        return consumeSteps(range, context);

    case CSSValueSpring:
        return consumeSpringFunction(range, context);

    default:
        break;
    }

    return nullptr;
}

RefPtr<TimingFunction> parseTimingFunction(const String& string, const CSSParserContext& context)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto result = consumeTimingFunction(range, context);
    if (!result)
        return { };

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return createTimingFunction(*result);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
