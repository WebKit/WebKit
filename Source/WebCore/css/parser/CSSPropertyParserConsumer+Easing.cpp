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
#include "CSSPropertyParserConsumer+Easing.h"

#include "CSSEasingFunctionValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSValueKeywords.h"
#include "StyleEasingFunction.h"
#include "TimingFunction.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: - <steps()>

static std::optional<CSS::EasingFunction> consumeUnresolvedStepsEasingFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <steps-easing-function> = steps( <integer>, <steps-easing-function-position>? )
    // <steps-easing-function-position> = jump-start | jump-end | jump-none | jump-both | start | end
    //
    // with range constraints, this is:
    //
    // <steps-easing-function> = steps( <integer [1,∞]>, jump-start )
    //                         | steps( <integer [1,∞]>, jump-end )
    //                         | steps( <integer [1,∞]>, jump-both )
    //                         | steps( <integer [1,∞]>, start )
    //                         | steps( <integer [1,∞]>, end )
    //                         | steps( <integer [2,∞]>, jump-none )
    // https://drafts.csswg.org/css-easing-2/#funcdef-steps

    ASSERT(range.peek().functionId() == CSSValueSteps);
    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);

    // Stash args so we can re-parse if we get `jump-none`.
    auto stashedArgs = args;

    auto steps = MetaConsumer<CSS::Integer<CSS::Range{1,CSS::Range::infinity}>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!steps)
        return { };

    std::optional<CSS::StepsEasingParameters> parameters;

    if (consumeCommaIncludingWhitespace(args)) {
        switch (args.consumeIncludingWhitespace().id()) {
        case CSSValueJumpStart:
            parameters = { CSS::StepsEasingParameters::JumpStart { WTFMove(*steps) } };
            break;
        case CSSValueJumpEnd:
            parameters = { CSS::StepsEasingParameters::JumpEnd { WTFMove(*steps) } };
            break;
        case CSSValueJumpNone: {
            // "The first parameter specifies the number of intervals in the function. It must be a
            //  positive integer greater than 0 unless the second parameter is jump-none in which
            //  case it must be a positive integer greater than 1."

            // Re-parse `steps` to account for different type requirement.
            auto stepsJumpNone = MetaConsumer<CSS::Integer<CSS::Range{2,CSS::Range::infinity}>>::consume(stashedArgs, context, { }, { .parserMode = context.mode });
            if (!stepsJumpNone)
                return { };

            parameters = { CSS::StepsEasingParameters::JumpNone { WTFMove(*stepsJumpNone) } };
            break;
        }

        case CSSValueJumpBoth:
            parameters = { CSS::StepsEasingParameters::JumpBoth { WTFMove(*steps) } };
            break;
        case CSSValueStart:
            parameters = { CSS::StepsEasingParameters::Start { WTFMove(*steps) } };
            break;
        case CSSValueEnd:
            parameters = { CSS::StepsEasingParameters::End { WTFMove(*steps) } };
            break;
        default:
            return { };
        }
    } else
        parameters = { CSS::StepsEasingParameters::End { WTFMove(*steps) } };

    if (!args.atEnd())
        return { };

    range = rangeCopy;

    return CSS::EasingFunction {
        CSS::StepsEasingFunction {
            .parameters = WTFMove(*parameters)
        }
    };
}

// MARK: - <linear()>

static std::optional<CSS::LinearEasingParameters::Stop::Length> consumeUnresolvedLinearEasingFunctionStopLength(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <linear-easing-function-stop-length> = <percentage>{0,2}

    auto input = MetaConsumer<CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!input)
        return { };
    auto extra = MetaConsumer<CSS::Percentage<>>::consume(args, context, { }, { .parserMode = context.mode });

    return CSS::LinearEasingParameters::Stop::Length {
        .input = WTFMove(*input),
        .extra = WTFMove(extra)
    };
}

static std::optional<CSS::LinearEasingParameters::Stop> consumeUnresolvedLinearEasingFunctionStop(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <linear-easing-function-stop> = <number> && <percentage>{0,2}

    auto output = MetaConsumer<CSS::Number<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!output)
        return { };
    auto input = consumeUnresolvedLinearEasingFunctionStopLength(args, context);

    return CSS::LinearEasingParameters::Stop {
        .output = WTFMove(*output),
        .input = WTFMove(input)
    };
}

static std::optional<CSS::EasingFunction> consumeUnresolvedLinearEasingFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <linear()> = linear( [ <number> && <percentage>{0,2} ]# )
    // https://drafts.csswg.org/css-easing-2/#funcdef-linear

    ASSERT(range.peek().functionId() == CSSValueLinear);
    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);

    Vector<CSS::LinearEasingParameters::Stop> stops;

    while (true) {
        auto stop = consumeUnresolvedLinearEasingFunctionStop(args, context);
        if (!stop)
            break;

        stops.append(WTFMove(*stop));

        if (!consumeCommaIncludingWhitespace(args))
            break;
    }

    if (!args.atEnd() || stops.size() < 2)
        return { };

    range = rangeCopy;

    return CSS::EasingFunction {
        CSS::LinearEasingFunction {
            .parameters = {
                .stops = { WTFMove(stops) }
            }
        }
    };
}

// MARK: - <cubic-bezier()>

static std::optional<CSS::EasingFunction> consumeUnresolvedCubicBezierEasingFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <cubic-bezier()> = cubic-bezier( [ <number [0,1]>, <number> ]#{2} )
    // https://drafts.csswg.org/css-easing-2/#funcdef-cubic-bezier

    ASSERT(range.peek().functionId() == CSSValueCubicBezier);
    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);

    auto x1 = MetaConsumer<CSS::Number<CSS::ClosedUnitRange>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!x1)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto y1 = MetaConsumer<CSS::Number<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!y1)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto x2 = MetaConsumer<CSS::Number<CSS::ClosedUnitRange>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!x2)
        return { };
    if (!consumeCommaIncludingWhitespace(args))
        return { };
    auto y2 = MetaConsumer<CSS::Number<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!y2)
        return { };

    if (!args.atEnd())
        return { };

    range = rangeCopy;

    return CSS::EasingFunction {
        CSS::CubicBezierEasingFunction {
            .parameters = {
                .value = {
                    CSS::CubicBezierEasingParameters::Coordinate { WTFMove(*x1), WTFMove(*y1) },
                    CSS::CubicBezierEasingParameters::Coordinate { WTFMove(*x2), WTFMove(*y2) },
                }
            }
        }
    };
}

// MARK: - <spring()>

static std::optional<CSS::EasingFunction> consumeUnresolvedSpringEasingFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <spring()> = spring( <number [>0,∞]> <number [>0,∞]> <number [0,∞]> <number> )
    // Non-standard

    ASSERT(range.peek().functionId() == CSSValueSpring);

    if (!context.springTimingFunctionEnabled)
        return { };

    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);

    auto mass = MetaConsumer<CSS::Number<CSS::SpringEasingParameters::Positive>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!mass)
        return { };
    auto stiffness = MetaConsumer<CSS::Number<CSS::SpringEasingParameters::Positive>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!stiffness)
        return { };
    auto damping = MetaConsumer<CSS::Number<CSS::Nonnegative>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!damping)
        return { };
    auto initialVelocity = MetaConsumer<CSS::Number<>>::consume(args, context, { }, { .parserMode = context.mode });
    if (!initialVelocity)
        return { };

    if (!args.atEnd())
        return { };

    range = rangeCopy;

    return CSS::EasingFunction {
        CSS::SpringEasingFunction {
            .parameters = {
                .mass = WTFMove(*mass),
                .stiffness = WTFMove(*stiffness),
                .damping = WTFMove(*damping),
                .initialVelocity = WTFMove(*initialVelocity),
            }
        }
    };
}

// MARK: - <easing-function>

std::optional<CSS::EasingFunction> consumeUnresolvedEasingFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <easing-function> = linear | ease | ease-in | ease-out | ease-in-out | step-start | step-end | <linear()> | <cubic-bezier()> | <steps()>
    // NOTE: also includes non-standard <spring()>.
    // https://drafts.csswg.org/css-easing/#typedef-easing-function

    switch (range.peek().id()) {
    case CSSValueLinear:
        range.consumeIncludingWhitespace();
        return CSS::EasingFunction { CSS::Keyword::Linear { } };
    case CSSValueEase:
        range.consumeIncludingWhitespace();
        return CSS::EasingFunction { CSS::Keyword::Ease { } };
    case CSSValueEaseIn:
        range.consumeIncludingWhitespace();
        return CSS::EasingFunction { CSS::Keyword::EaseIn { } };
    case CSSValueEaseOut:
        range.consumeIncludingWhitespace();
        return CSS::EasingFunction { CSS::Keyword::EaseOut { } };
    case CSSValueEaseInOut:
        range.consumeIncludingWhitespace();
        return CSS::EasingFunction { CSS::Keyword::EaseInOut { } };

    case CSSValueStepStart:
        range.consumeIncludingWhitespace();
        return CSS::EasingFunction {
            CSS::StepsEasingFunction {
                .parameters = { CSS::StepsEasingParameters::Start { CSS::Integer<CSS::Range{1,CSS::Range::infinity}> { 1 } } }
            }
        };

    case CSSValueStepEnd:
        range.consumeIncludingWhitespace();
        return CSS::EasingFunction {
            CSS::StepsEasingFunction {
                .parameters = { CSS::StepsEasingParameters::End { CSS::Integer<CSS::Range{1,CSS::Range::infinity}> { 1 } } }
            }
        };

    default:
        break;
    }

    switch (range.peek().functionId()) {
    case CSSValueLinear:
        return consumeUnresolvedLinearEasingFunction(range, context);

    case CSSValueCubicBezier:
        return consumeUnresolvedCubicBezierEasingFunction(range, context);

    case CSSValueSteps:
        return consumeUnresolvedStepsEasingFunction(range, context);

    case CSSValueSpring:
        return consumeUnresolvedSpringEasingFunction(range, context);

    default:
        break;
    }

    return { };
}

RefPtr<CSSValue> consumeEasingFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // Avoid allocation of a CSSEasingFunctionValue when the result is a just a value ID.
    switch (range.peek().id()) {
    case CSSValueLinear:
    case CSSValueEase:
    case CSSValueEaseIn:
    case CSSValueEaseOut:
    case CSSValueEaseInOut:
        return consumeIdent(range);
    default:
        break;
    }

    if (auto value = consumeUnresolvedEasingFunction(range, context))
        return CSSEasingFunctionValue::create(WTFMove(*value));
    return { };
}

RefPtr<TimingFunction> parseEasingFunctionDeprecated(const String& string, const CSSParserContext& context)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto result = consumeUnresolvedEasingFunction(range, context);
    if (!result)
        return { };

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return Style::createTimingFunctionDeprecated(*result);
}

RefPtr<TimingFunction> parseEasingFunction(const String& string, const CSSParserContext& context, const CSSToLengthConversionData& conversionData)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto result = consumeUnresolvedEasingFunction(range, context);
    if (!result)
        return { };

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return Style::createTimingFunction(*result, conversionData);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
