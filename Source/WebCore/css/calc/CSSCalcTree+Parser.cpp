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
#include "CSSCalcTree+Parser.h"

#include "AnchorPositionEvaluator.h"
#include "CSSCalcOperator.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+Serialization.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSCalcTree.h"
#include "CSSParserContext.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSRandomKeyParser.h"
#include "CSSSerializationContext.h"
#include "CSSUnits.h"
#include "Logging.h"
#include <numbers>
#include <wtf/SortedArrayMap.h>

namespace WebCore {
namespace CSSCalc {

// MARK: - Constants

static constexpr int maxExpressionDepth = 100;

static std::optional<std::pair<Number, Type>> lookupConstantNumber(CSSValueID symbol)
{
    static constexpr SortedArrayMap constantMap { WTF::toArray<std::pair<CSSValueID, double>>({
        { CSSValueID::E,                     std::numbers::e                          },
        { CSSValueID::Pi,                    std::numbers::pi                         },
        { CSSValueID::Infinity,              std::numeric_limits<double>::infinity()  },
        { CSSValueID::NegativeInfinity, -1 * std::numeric_limits<double>::infinity()  },
        { CSSValueID::NaN,                   std::numeric_limits<double>::quiet_NaN() },
    }) };
    if (auto value = constantMap.tryGet(symbol))
        return std::make_pair(Number { .value = *value }, Type { });
    return std::nullopt;
}

// MARK: - Parser State

namespace {

enum class ParseStatus { Ok, TooDeep };

struct ParserState {
    // CSS::PropertyParserState used to initiate the parse.
    CSS::PropertyParserState& propertyParserState;

    // ParserOptions used to initiate the parse.
    const ParserOptions& parserOptions;

    // SimplificationOptions used to initiate the parse, if provided.
    const SimplificationOptions* simplificationOptions;

    // Tracks whether the parse tree contains any non-canonical dimension units that require conversion data (e.g. em, vh, etc.).
    bool requiresConversionData = false;
};

} // namespace (anonymous)

static ParseStatus NODELETE checkDepth(int depth)
{
    if (depth > maxExpressionDepth)
        return ParseStatus::TooDeep;
    return ParseStatus::Ok;
}

// MARK: - Parser

struct TypedChild {
    Child child;
    Type type;
};

static std::optional<TypedChild> parseCalcFunction(CSSParserTokenRange&, CSSValueID functionID, int depth, ParserState&);
static std::optional<TypedChild> parseCalcSum(CSSParserTokenRange&, int depth, ParserState&);
static std::optional<TypedChild> parseCalcProduct(CSSParserTokenRange&, int depth, ParserState&);
static std::optional<TypedChild> parseCalcValue(CSSParserTokenRange&, int depth, ParserState&);
static std::optional<TypedChild> parseCalcKeyword(const CSSParserToken&, ParserState&);
static std::optional<TypedChild> parseCalcNumber(const CSSParserToken&, ParserState&);
static std::optional<TypedChild> parseCalcPercentage(const CSSParserToken&, ParserState&);
static std::optional<TypedChild> parseCalcDimension(const CSSParserToken&, ParserState&);

std::optional<Tree> parseAndSimplify(CSSParserTokenRange& range, CSS::PropertyParserState& propertyParserState, const ParserOptions& parserOptions, const SimplificationOptions& simplificationOptions)
{
    auto function = range.peek().functionId();
    if (!isCalcFunction(function))
        return std::nullopt;

    auto tokens = CSSPropertyParserHelpers::consumeFunction(range);

    LOG_WITH_STREAM(Calc, stream << "Starting top level parse/simplification of function " << nameLiteralForSerialization(function) << "(" << tokens.serialize() << ") with expected type " << parserOptions.category);

    // -- Parsing --

    ParserState state {
        .propertyParserState = propertyParserState,
        .parserOptions = parserOptions,
        .simplificationOptions = &simplificationOptions
    };

    auto root = parseCalcFunction(tokens, function, 0, state);

    if (!root || !tokens.atEnd()) {
        LOG_WITH_STREAM(Calc, stream << "Failed top level parse/simplification of function '" << nameLiteralForSerialization(function) << "'");
        return std::nullopt;
    }

    // -- Type Checking --

    if (!root->type.matches(parserOptions.category)) {
        LOG_WITH_STREAM(Calc, stream << "Failed top level parse/simplification due to type check for function '" << nameLiteralForSerialization(function) << "', type=" << root->type << ", expected category=" << parserOptions.category);

        return std::nullopt;
    }

    auto result = Tree {
        .root = WTF::move(root->child),
        .type = root->type,
        .stage = CSSCalc::Stage::Specified,
        .requiresConversionData = state.requiresConversionData,
    };

    LOG_WITH_STREAM(Calc, stream << "Completed top level parse/simplification for function '" << nameLiteralForSerialization(function) << "': " << serializationForCSS(result, { parserOptions.range, CSS::defaultSerializationContext() }) << ", type: " << getType(result.root) << ", category=" << parserOptions.category << ", requires-conversion-data: " << result.requiresConversionData);

    return result;
}

std::optional<Tree> parseAndSimplifyCalcSum(CSSParserTokenRange& tokens, CSS::PropertyParserState& propertyParserState, const ParserOptions& parserOptions, const SimplificationOptions& simplificationOptions)
{
    ParserState state {
        .propertyParserState = propertyParserState,
        .parserOptions = parserOptions,
        .simplificationOptions = &simplificationOptions
    };

    auto root = parseCalcSum(tokens, 0, state);
    if (!root)
        return std::nullopt;

    if (!root->type.matches(parserOptions.category))
        return std::nullopt;

    return Tree {
        .root = WTF::move(root->child),
        .type = root->type,
        .stage = CSSCalc::Stage::Specified,
        .requiresConversionData = state.requiresConversionData,
    };
}

bool isCalcFunction(CSSValueID functionId)
{
    switch (functionId) {
    case CSSValueID::Calc:
    case CSSValueID::CalcMix:
    case CSSValueID::WebkitCalc:
    case CSSValueID::Min:
    case CSSValueID::Max:
    case CSSValueID::Clamp:
    case CSSValueID::Pow:
    case CSSValueID::Sqrt:
    case CSSValueID::Hypot:
    case CSSValueID::Sin:
    case CSSValueID::Cos:
    case CSSValueID::Tan:
    case CSSValueID::Exp:
    case CSSValueID::Log:
    case CSSValueID::Asin:
    case CSSValueID::Acos:
    case CSSValueID::Atan:
    case CSSValueID::Atan2:
    case CSSValueID::Abs:
    case CSSValueID::Sign:
    case CSSValueID::Round:
    case CSSValueID::Mod:
    case CSSValueID::Rem:
    case CSSValueID::Progress:
    case CSSValueID::Random:
    case CSSValueID::SiblingCount:
    case CSSValueID::SiblingIndex:
    case CSSValueID::Anchor:
    case CSSValueID::AnchorSize:
        return true;
    default:
        return false;
    }
    return false;
}

template<typename Op> static std::optional<TypedChild> consumeZeroArguments(CSSParserTokenRange& tokens, int, ParserState&)
{
    if (!tokens.atEnd()) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - extraneous tokens found");
        return std::nullopt;
    }

    auto child = Op { };
    auto type = getType(child);

    return TypedChild { makeChild(WTF::move(child)), type };
}

template<typename Op> static std::optional<TypedChild> consumeExactlyOneArgument(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    auto sum = parseCalcSum(tokens, depth, state);
    if (!sum) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument failed to parse");
        return std::nullopt;
    }

    if (!tokens.atEnd()) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - extraneous tokens found");
        return std::nullopt;
    }

    if (!validateType<Op::input>(sum->type)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument has invalid type: " << sum->type);
        return std::nullopt;
    }

    auto outputType = transformType<Op::output>(sum->type);
    if (!outputType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - output transform failed for type: " << sum->type);
        return std::nullopt;
    }

    Op op { WTF::move(sum->child) };

    // Sin, Cos, and Tan accept either a <number> (already in radians) or an <angle> (in the
    // canonical unit of degrees). Wrap angle arguments in a Deg2Rad node so that evaluation no
    // longer has to inspect types to decide whether to convert — the conversion is explicit in
    // the tree. Simplify the Deg2Rad eagerly so that fully-resolved angles collapse into a Number
    // (which then lets the trig simplification below reduce the whole expression to a Number).
    if constexpr (std::same_as<Op, Sin> || std::same_as<Op, Cos> || std::same_as<Op, Tan>) {
        if (sum->type.template matchesAny<Type::Match::Angle>({ .allowsPercentHint = true })) {
            Deg2Rad conversion { .angle = WTF::move(op.a) };
            if (auto* simplificationOptions = state.simplificationOptions) {
                if (auto replacement = simplify(conversion, *simplificationOptions))
                    op.a = WTF::move(*replacement);
                else
                    op.a = makeChild(WTF::move(conversion), Type { });
            } else
                op.a = makeChild(WTF::move(conversion), Type { });
        }
    }

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(op, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), *outputType };
    }
    return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
}

template<typename Op> static std::optional<TypedChild> consumeOneOrMoreArguments(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    std::optional<Type> mergedType;
    Vector<Child> children;

    bool requireComma = false;
    unsigned argumentCount = 0;

    while (!tokens.atEnd()) {
        tokens.consumeWhitespace();
        if (requireComma && !CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma");
            return std::nullopt;
        }

        auto sum = parseCalcSum(tokens, depth, state);
        if (!sum) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument #" << argumentCount);
            return std::nullopt;
        }

        if (!validateType<Op::input>(sum->type)) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #" << argumentCount << " has invalid type: " << sum->type);
            return std::nullopt;
        }

        if (!mergedType)
            mergedType = sum->type;
        else {
            auto mergeResult = mergeTypes<Op::merge>(*mergedType, sum->type);
            if (!mergeResult) {
                LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #" << argumentCount << " failed to merge type with other arguments: existing type " << *mergedType << " & argument type " << sum->type);
                return std::nullopt;
            }
            mergedType = *mergeResult;
        }

        ++argumentCount;
        children.append(WTF::move(sum->child));
        requireComma = true;
    }

    if (argumentCount < 1) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - no arguments found");
        return std::nullopt;
    }

    auto outputType = transformType<Op::output>(*mergedType);
    if (!outputType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - output transform failed for type: " << *mergedType);
        return std::nullopt;
    }

    Op op { WTF::move(children) };

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(op, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), *outputType };
    }
    return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
}

template<typename Op> static std::optional<TypedChild> consumeExactlyTwoArguments(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    auto sumA = parseCalcSum(tokens, depth, state);
    if (!sumA) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument #1");
        return std::nullopt;
    }

    if (!validateType<Op::input>(sumA->type)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #1 has invalid type: " << sumA->type);
        return std::nullopt;
    }

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma");
        return std::nullopt;
    }

    auto sumB = parseCalcSum(tokens, depth, state);
    if (!sumB) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument #2");
        return std::nullopt;
    }

    if (!tokens.atEnd()) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - extraneous tokens found");
        return std::nullopt;
    }

    if (!validateType<Op::input>(sumB->type)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #2 has invalid type:  " << sumB->type);
        return std::nullopt;
    }

    auto mergedType = mergeTypes<Op::merge>(sumA->type, sumB->type);
    if (!mergedType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge type with other arguments: argument #1 type " << sumA->type << " & argument #2 type" << sumB->type);
        return std::nullopt;
    }

    auto outputType = transformType<Op::output>(*mergedType);
    if (!outputType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - output transform failed for type: " << *mergedType);
        return std::nullopt;
    }

    Op op { WTF::move(sumA->child), WTF::move(sumB->child) };

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(op, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), *outputType };
    }
    return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
}

template<typename Op> static std::optional<TypedChild> consumeOneOrTwoArguments(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    auto sumA = parseCalcSum(tokens, depth, state);
    if (!sumA) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument #1");
        return std::nullopt;
    }

    if (!validateType<Op::input>(sumA->type)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #1 has invalid type: " << sumA->type);
        return std::nullopt;
    }

    if (tokens.atEnd()) {
        auto outputType = transformType<Op::output>(sumA->type);
        if (!outputType) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' (one argument) function - output transform failed for type: " << sumA->type);
            return std::nullopt;
        }

        Op op { WTF::move(sumA->child), std::nullopt };

        if (auto* simplificationOptions = state.simplificationOptions) {
            if (auto replacement = simplify(op, *simplificationOptions))
                return TypedChild { WTF::move(*replacement), *outputType };
        }
        return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
    }

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma");
        return std::nullopt;
    }

    auto sumB = parseCalcSum(tokens, depth, state);
    if (!sumB) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' (two arguments) function - failed parse of argument #2");
        return std::nullopt;
    }

    if (!tokens.atEnd()) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' (two arguments) function - extraneous tokens found");
        return std::nullopt;
    }

    if (!validateType<Op::input>(sumB->type)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' (two arguments) function - argument #2 has invalid type: " << sumB->type);
        return std::nullopt;
    }

    auto mergedType = mergeTypes<Op::merge>(sumA->type, sumB->type);
    if (!mergedType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' (two arguments) function - failed to merge type with other arguments: argument #1 type " << sumA->type << " & argument #2 type" << sumB->type);
        return std::nullopt;
    }

    auto outputType = transformType<Op::output>(*mergedType);
    if (!outputType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' (two arguments) function - output transform failed for type: " << *mergedType);
        return std::nullopt;
    }

    Op op { WTF::move(sumA->child), WTF::move(sumB->child) };

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(op, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), *outputType };
    }
    return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
}

static std::optional<TypedChild> consumeClamp(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // <clamp()> = clamp( [ <calc-sum> | none ], <calc-sum>, [ <calc-sum> | none ] )

    using Op = Clamp;

    struct TypedChildOrNone {
        ChildOrNone child;
        Type type;
    };
    auto parseCalcSumOrNone = [](auto& tokens, auto depth, auto& state) -> std::optional<TypedChildOrNone> {
        if (tokens.peek().id() == CSSValueID::None) {
            tokens.consumeIncludingWhitespace();
            return TypedChildOrNone { ChildOrNone { CSS::Keyword::None { } }, Type { } };
        }
        auto sum = parseCalcSum(tokens, depth, state);
        if (!sum)
            return std::nullopt;

        return TypedChildOrNone { ChildOrNone { WTF::move(sum->child) }, sum->type };
    };

    auto min = parseCalcSumOrNone(tokens, depth, state);
    if (!min) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument 'min' failed to parse");
        return std::nullopt;
    }

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma after argument 'min'");
        return std::nullopt;
    }

    auto val = parseCalcSum(tokens, depth, state);
    if (!val) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument 'val' failed to parse");
        return std::nullopt;
    }

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma after argument 'val'");
        return std::nullopt;
    }

    auto max = parseCalcSumOrNone(tokens, depth, state);
    if (!max) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument 'max' failed to parse");
        return std::nullopt;
    }

    if (!tokens.atEnd()) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - extraneous tokens found");
        return std::nullopt;
    }

    auto computeType = [&] -> std::optional<Type> {
        bool minIsNone = WTF::holdsAlternative<CSS::Keyword::None>(min->child);
        bool maxIsNone = WTF::holdsAlternative<CSS::Keyword::None>(max->child);

        if (minIsNone && maxIsNone)
            return val->type;

        if (minIsNone) {
            auto valAndMaxType = mergeTypes<MergePolicy::Consistent>(val->type, max->type);
            if (!valAndMaxType) {
                LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge argument 'val' type " << val->type << " & argument 'max' type " << max->type);
                return std::nullopt;
            }
            return *valAndMaxType;
        }

        if (maxIsNone) {
            auto minAndValType = mergeTypes<Op::merge>(min->type, val->type);
            if (!minAndValType) {
                LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge argument 'min' type " << min->type << " & argument 'val' type " << val->type);
                return std::nullopt;
            }
            return *minAndValType;
        }

        auto minAndValType = mergeTypes<Op::merge>(min->type, val->type);
        if (!minAndValType) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge argument 'min' type " << min->type << " & argument 'val' type " << val->type);
            return std::nullopt;
        }
        auto minAndValAndMaxType = mergeTypes<Op::merge>(*minAndValType, max->type);
        if (!minAndValAndMaxType) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge already merged type " << *minAndValAndMaxType << " & argument 'max' type " << max->type);
            return std::nullopt;
        }
        return *minAndValAndMaxType;
    };

    auto outputType = computeType();
    if (!outputType)
        return std::nullopt;

    Op op { WTF::move(min->child), WTF::move(val->child), WTF::move(max->child) };

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(op, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), *outputType };
    }
    return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
}

template<typename Op> static std::optional<TypedChild> consumeRoundArguments(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    auto sumA = parseCalcSum(tokens, depth, state);
    if (!sumA) {
        LOG_WITH_STREAM(Calc, stream << "Failed 'round(" << nameLiteralForSerialization(Op::id) << ")' function - failed parse of argument #1");
        return std::nullopt;
    }

    if (tokens.atEnd()) {
        if (!validateType<AllowedTypes::Number>(sumA->type)) {
            LOG_WITH_STREAM(Calc, stream << "Failed 'round(" << nameLiteralForSerialization(Op::id) << ")' function - argument #1 has invalid type: " << sumA->type);
            return std::nullopt;
        }

        auto outputType = transformType<Op::output>(sumA->type);
        if (!outputType) {
            LOG_WITH_STREAM(Calc, stream << "Failed 'round(" << nameLiteralForSerialization(Op::id) << ")' (one argument) function - output transform failed for type: " << sumA->type);
            return std::nullopt;
        }

        Op op { WTF::move(sumA->child), std::nullopt };

        if (auto* simplificationOptions = state.simplificationOptions) {
            if (auto replacement = simplify(op, *simplificationOptions))
                return TypedChild { WTF::move(*replacement), *outputType };
        }
        return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
    }

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed 'round(" << nameLiteralForSerialization(Op::id) << ")' function - missing comma");
        return std::nullopt;
    }

    auto sumB = parseCalcSum(tokens, depth, state);
    if (!sumB) {
        LOG_WITH_STREAM(Calc, stream << "Failed 'round(" << nameLiteralForSerialization(Op::id) << ")' (two arguments) function - failed parse of argument #2");
        return std::nullopt;
    }

    if (!tokens.atEnd()) {
        LOG_WITH_STREAM(Calc, stream << "Failed 'round(" << nameLiteralForSerialization(Op::id) << ")' (two arguments) function - extraneous tokens found");
        return std::nullopt;
    }

    auto mergedType = mergeTypes<Op::merge>(sumA->type, sumB->type);
    if (!mergedType) {
        LOG_WITH_STREAM(Calc, stream << "Failed 'round(" << nameLiteralForSerialization(Op::id) << ")' (two arguments) function - failed to merge type with other arguments: argument #1 type " << sumA->type << " & argument #2 type" << sumB->type);
        return std::nullopt;
    }

    auto outputType = transformType<Op::output>(*mergedType);
    if (!outputType) {
        LOG_WITH_STREAM(Calc, stream << "Failed 'round(" << nameLiteralForSerialization(Op::id) << ")' (two arguments) function - output transform failed for type: " << *mergedType);
        return std::nullopt;
    }

    Op op { WTF::move(sumA->child), WTF::move(sumB->child) };

    LOG_WITH_STREAM(Calc, stream << "Succeeded 'round(" << nameLiteralForSerialization(Op::id) << ")' (two arguments) function: type is " << *outputType);

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(op, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), *outputType };
    }

    return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
}

static std::optional<TypedChild> consumeRound(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // <round()> = round( <rounding-strategy>?, <calc-sum>, <calc-sum>? )

    auto roundingStrategy = CSSPropertyParserHelpers::consumeIdentRaw<CSSValueID::Nearest, CSSValueID::ToZero, CSSValueID::Up, CSSValueID::Down>(tokens);
    if (!roundingStrategy)
        return consumeRoundArguments<RoundNearest>(tokens, depth, state);

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed 'round(" << nameLiteralForSerialization(*roundingStrategy) << ") function - missing comma after <rounding-strategy>");
        return std::nullopt;
    }

    switch (*roundingStrategy) {
    case CSSValueID::Nearest:
        return consumeRoundArguments<RoundNearest>(tokens, depth, state);
    case CSSValueID::ToZero:
        return consumeRoundArguments<RoundToZero>(tokens, depth, state);
    case CSSValueID::Up:
        return consumeRoundArguments<RoundUp>(tokens, depth, state);
    case CSSValueID::Down:
        return consumeRoundArguments<RoundDown>(tokens, depth, state);
    default:
        break;
    }

    return std::nullopt;
}

static std::optional<TypedChild> consumeRandom(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // <random()> = random( <random-key>? , <calc-sum>, <calc-sum>, <calc-sum>? )

    if (!state.propertyParserState.context.cssRandomFunctionEnabled)
        return { };

    if (state.propertyParserState.currentRule != StyleRuleType::Style && state.propertyParserState.currentRule != StyleRuleType::Keyframe)
        return { };
    if (state.propertyParserState.currentProperty == CSSPropertyID::Invalid)
        return { };

    if (state.propertyParserState.randomFunctionsDisallowed)
        return { };

    using Op = Random;

    auto keySource = CSSPropertyParserHelpers::RandomKeySource {
        .property = { state.propertyParserState.currentProperty, state.propertyParserState.currentCustomPropertyName, RandomFunction::Random },
        .autoElementScoped = CSS::Keyword::ElementScoped { }
    };

    std::optional<Random::Sharing> sharing;
    if (auto optionalSharing = CSSPropertyParserHelpers::consumeUnresolvedRandomKey(tokens, state.propertyParserState, keySource, [&] {
        return state.propertyParserState.cssRandomFunctionCount;
    })) {
        if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma after <random-key>");
            return { };
        }

        sharing = WTF::move(optionalSharing);
    } else
        sharing = CSSPropertyParserHelpers::randomSharingAuto(keySource, state.propertyParserState.cssRandomFunctionCount);

    // Increment the random function count early, but after processing the the sharing production to
    // ensure that any nested random() functions in the <calc-sum> productions have an incremented value.
    ++state.propertyParserState.cssRandomFunctionCount;

    auto min = parseCalcSum(tokens, depth, state);
    if (!min) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument `min`");
        return { };
    }

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma after argument `min`");
        return { };
    }

    auto max = parseCalcSum(tokens, depth, state);
    if (!max) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument `max`");
        return { };
    }

    if (tokens.atEnd()) {
        // - Validate arguments

        if (!validateType<Op::input>(min->type)) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument `min` has invalid type: " << min->type);
            return { };
        }

        if (!validateType<Op::input>(max->type)) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument `max` has invalid type: " << max->type);
            return { };
        }

        // - Merge arguments

        auto mergedType = mergeTypes<Op::merge>(min->type, max->type);
        if (!mergedType) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge types");
            return { };
        }

        auto outputType = transformType<Op::output>(*mergedType);
        if (!outputType) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - output transform failed for type: " << *mergedType);
            return { };
        }

        state.requiresConversionData = true;

        Op op { WTF::move(*sharing), WTF::move(min->child), WTF::move(max->child), std::nullopt };

        if (auto* simplificationOptions = state.simplificationOptions) {
            if (auto replacement = simplify(op, *simplificationOptions))
                return TypedChild { WTF::move(*replacement), *outputType };
        }

        return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
    }

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma after argument `max`");
        return { };
    }

    auto step = parseCalcSum(tokens, depth, state);
    if (!step) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument `step`");
        return { };
    }

    if (!tokens.atEnd()) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - extraneous tokens found");
        return { };
    }

    // - Validate arguments

    if (!validateType<Op::input>(step->type)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument `step` has invalid type: " << step->type);
        return { };
    }

    // - Merge arguments

    auto mergedType = mergeTypes<Op::merge>(min->type, max->type);
    if (!mergedType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge types");
        return { };
    }

    mergedType = mergeTypes<Op::merge>(*mergedType, step->type);
    if (!mergedType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge types");
        return { };
    }

    auto outputType = transformType<Op::output>(*mergedType);
    if (!outputType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - output transform failed for type: " << *mergedType);
        return { };
    }

    state.requiresConversionData = true;

    Op op { WTF::move(*sharing), WTF::move(min->child), WTF::move(max->child), WTF::move(step->child) };

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(op, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), *outputType };
    }
    return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
}

template<typename Op>
static std::optional<TypedChild> consumeProgressImpl(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    auto value = parseCalcSum(tokens, depth, state);
    if (!value) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument #1");
        return std::nullopt;
    }

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma");
        return std::nullopt;
    }

    auto start = parseCalcSum(tokens, depth, state);
    if (!start) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument #2");
        return std::nullopt;
    }

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma");
        return std::nullopt;
    }

    auto end = parseCalcSum(tokens, depth, state);
    if (!end) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument #3");
        return std::nullopt;
    }

    if (!tokens.atEnd()) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - extraneous tokens found");
        return std::nullopt;
    }

    // - Validate arguments

    if (!validateType<Op::input>(value->type)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #1 has invalid type: " << value->type);
        return std::nullopt;
    }

    if (!validateType<Op::input>(start->type)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #2 has invalid type: " << start->type);
        return std::nullopt;
    }

    if (!validateType<Op::input>(end->type)) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #3 has invalid type: " << end->type);
        return std::nullopt;
    }

    // - Merge arguments

    auto mergedType = mergeTypes<Op::merge>(value->type, start->type);
    if (!mergedType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge types: argument #1 type " << value->type << ", argument #2 type" << start->type << ", argument #3 type" << end->type);
        return std::nullopt;
    }

    mergedType = mergeTypes<Op::merge>(*mergedType, end->type);
    if (!mergedType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed to merge types: argument #1 type " << value->type << ", argument #2 type" << start->type << ", argument #3 type" << end->type);
        return std::nullopt;
    }

    auto outputType = transformType<Op::output>(*mergedType);
    if (!outputType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - output transform failed for type: " << *mergedType);
        return std::nullopt;
    }

    Op op { WTF::move(value->child), WTF::move(start->child), WTF::move(end->child) };

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(op, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), *outputType };
    }
    return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
}

static std::optional<TypedChild> consumeProgress(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // <progress()> = progress( no-clamp? <calc-sum>, <calc-sum>, <calc-sum> )

    if (CSSPropertyParserHelpers::consumeIdentRaw<CSSValueID::NoClamp>(tokens))
        return consumeProgressImpl<ProgressNoClamp>(tokens, depth, state);
    return consumeProgressImpl<Progress>(tokens, depth, state);
}

static std::optional<TypedChild> consumeValueWithoutSimplifyingRootCalc(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // Complex arguments need to be surrounded by a math function.
    if (tokens.peek().type() == LeftParenthesisToken)
        return { };

    auto isFunction = tokens.peek().functionId() != CSSValueID::Invalid;

    auto typedValue = parseCalcValue(tokens, depth, state);
    if (!typedValue)
        return { };

    auto isLeafValue = WTF::switchOn(typedValue->child,
        [](Leaf auto&) { return true; },
        [](auto&) { return false; }
    );

    if (isFunction && isLeafValue) {
        // Wrap in Sum to keep top level calc() function in serialization.
        Vector<Child> children;
        children.append(WTF::move(typedValue->child));

        return TypedChild { makeChild(Sum { WTF::move(children) }, typedValue->type), typedValue->type };
    }

    return typedValue;
}

static std::optional<TypedChild> consumeCalcMix(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // <calc-mix()> = calc-mix( [ <calc-sum> <percentage [0,100]>? ]# )

    using Op = CalcMix;

    if (!state.propertyParserState.context.cssCalcMixEnabled)
        return { };

    std::optional<Type> mergedType;
    Vector<CalcMix::Item> children;

    bool requireComma = false;
    unsigned argumentCount = 0;

    while (!tokens.atEnd()) {
        tokens.consumeWhitespace();
        if (requireComma && !CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - missing comma");
            return std::nullopt;
        }

        auto sum = parseCalcSum(tokens, depth, state);
        if (!sum) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - failed parse of argument #" << argumentCount);
            return std::nullopt;
        }

        if (!validateType<Op::input>(sum->type)) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #" << argumentCount << " has invalid type: " << sum->type);
            return std::nullopt;
        }

        if (!mergedType)
            mergedType = sum->type;
        else {
            auto mergeResult = mergeTypes<Op::merge>(*mergedType, sum->type);
            if (!mergeResult) {
                LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - argument #" << argumentCount << " failed to merge type with other arguments: existing type " << *mergedType << " & argument type " << sum->type);
                return std::nullopt;
            }
            mergedType = *mergeResult;
        }

        std::optional<CalcMix::Item::Weight> weight;
        if (!tokens.atEnd() && tokens.peek().type() != CommaToken) {
            weight = CSSPropertyParserHelpers::MetaConsumer<CalcMix::Item::Weight>::consume(tokens, state.propertyParserState);
            if (!weight)
                return { };
        }

        ++argumentCount;
        children.append(CalcMix::Item { .value = WTF::move(sum->child), .weight = WTF::move(weight) });
        requireComma = true;
    }

    if (argumentCount < 1) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - no arguments found");
        return std::nullopt;
    }

    auto outputType = transformType<Op::output>(*mergedType);
    if (!outputType) {
        LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(Op::id) << "' function - output transform failed for type: " << *mergedType);
        return std::nullopt;
    }

    Op op { WTF::move(children) };

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(op, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), *outputType };
    }
    return TypedChild { makeChild(WTF::move(op), *outputType), *outputType };
}

// Parse the fallback value specified in anchor() and anchor-size() as a <length> or
// <length-percentage>. Additionally, unitless zero is allowed and gets treated as 0px.
static std::optional<TypedChild> consumeAnchorFallback(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    auto typedFallback = consumeValueWithoutSimplifyingRootCalc(tokens, depth, state);
    if (!typedFallback)
        return { };

    auto category = typedFallback->type.calculationCategory();
    if (!category)
        return { };

    switch (*category) {
    case CSS::Category::Length:
    case CSS::Category::LengthPercentage:
        return typedFallback;

    case CSS::Category::Number: {
        if (state.parserOptions.propertyOptions.unitlessZeroLength != UnitlessZeroQuirk::Allow)
            return { };

        // Allow unitless 0, but only as a bare <number> leaf. A math function
        // such as calc(0) or min(0, 0) also has number type but is wrapped in a
        // Sum node, so it is not a valid unitless-zero fallback.
        auto* number = std::get_if<Number>(&typedFallback->child.value);
        if (!number || number->value)
            return { };

        return TypedChild { makeNumeric(0, CSSUnitType::Px), Type::makeLength() };
    }

    default:
        return { };
    }
}

static std::optional<TypedChild> consumeAnchor(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // <anchor()> = anchor( <anchor-element>? && <anchor-side>, <length-percentage>? )

    if (state.parserOptions.propertyOptions.anchorPolicy != AnchorPolicy::Allow)
        return { };

    auto anchorElement = CSSPropertyParserHelpers::consumeUnresolvedDashedIdent(tokens, state.propertyParserState);

    // <anchor-side> = inside | outside | top | left | right | bottom | start | end | self-start | self-end | <percentage> | center
    auto anchorSide = [&]() -> std::optional<AnchorSide> {
        auto sideIdent = CSSPropertyParserHelpers::consumeIdentRaw<CSSValueID::Inside, CSSValueID::Outside, CSSValueID::Top, CSSValueID::Left, CSSValueID::Right, CSSValueID::Bottom, CSSValueID::Start, CSSValueID::End, CSSValueID::SelfStart, CSSValueID::SelfEnd, CSSValueID::Center>(tokens);
        if (sideIdent)
            return AnchorSide { *sideIdent };

        auto percentageOptions = ParserOptions {
            .category = CSS::Category::Percentage,
            .range = CSS::All,
            .allowedSymbols = { },
            .propertyOptions = { },
        };
        auto percentageState = ParserState {
            .propertyParserState = state.propertyParserState,
            .parserOptions = percentageOptions,
            .simplificationOptions = { },
        };

        auto percentage = consumeValueWithoutSimplifyingRootCalc(tokens, depth, percentageState);
        if (!percentage)
            return { };

        auto category = percentage->type.calculationCategory();
        if (!category || category != CSS::Category::Percentage)
            return { };

        return AnchorSide { WTF::move(percentage->child) };
    }();

    if (!anchorSide)
        return { };

    if (!anchorElement)
        anchorElement = CSSPropertyParserHelpers::consumeUnresolvedDashedIdent(tokens, state.propertyParserState);

    auto type = Type::makeLength();
    std::optional<Child> fallback;

    if (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
        auto maybeFallback = consumeAnchorFallback(tokens, depth, state);
        if (!maybeFallback)
            return { };

        fallback = WTF::move(maybeFallback->child);

        auto category = maybeFallback->type.calculationCategory();
        ASSERT(category && (category == CSS::Category::Length || category == CSS::Category::LengthPercentage));

        type.percentHint = Type::determinePercentHint(*category);
    }

    state.requiresConversionData = true;

    auto anchor = Anchor {
        .elementName = WTF::move(anchorElement),
        .side = WTF::move(*anchorSide),
        .fallback = WTF::move(fallback)
    };

    return TypedChild { makeChild(WTF::move(anchor), type), type };
}

static std::optional<Style::AnchorSizeDimension> NODELETE cssValueIDToAnchorSizeDimension(CSSValueID value)
{
    switch (value) {
    case CSSValueID::Width:
        return Style::AnchorSizeDimension::Width;
    case CSSValueID::Height:
        return Style::AnchorSizeDimension::Height;
    case CSSValueID::Block:
        return Style::AnchorSizeDimension::Block;
    case CSSValueID::Inline:
        return Style::AnchorSizeDimension::Inline;
    case CSSValueID::SelfBlock:
        return Style::AnchorSizeDimension::SelfBlock;
    case CSSValueID::SelfInline:
        return Style::AnchorSizeDimension::SelfInline;
    default:
        return { };
    }
}

static std::optional<TypedChild> consumeAnchorSize(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // anchor-size() = anchor-size( [ <anchor-element> || <anchor-size> ]? , <length-percentage>? )
    // <anchor-element> = <dashed-ident>
    // <anchor-size> = width | height | block | inline | self-block | self-inline

    if (state.parserOptions.propertyOptions.anchorSizePolicy != AnchorSizePolicy::Allow)
        return { };

    // parse <anchor-element>
    auto maybeAnchorElement = CSSPropertyParserHelpers::consumeUnresolvedDashedIdent(tokens, state.propertyParserState);

    // then parse <anchor-size>
    auto maybeAnchorSize = CSSPropertyParserHelpers::consumeIdentRaw<CSSValueID::Width, CSSValueID::Height, CSSValueID::Block, CSSValueID::Inline, CSSValueID::SelfBlock, CSSValueID::SelfInline>(tokens);

    // if we could parse <anchor-size> but not <anchor-element>, it's possible <anchor-element> is specified
    // after <anchor-size>, so re-parse <anchor-element>
    if (maybeAnchorSize && !maybeAnchorElement)
        maybeAnchorElement = CSSPropertyParserHelpers::consumeUnresolvedDashedIdent(tokens, state.propertyParserState);

    std::optional<TypedChild> fallback;

    // if either <anchor-element> or <anchor-size> is present
    if (maybeAnchorSize || maybeAnchorElement) {
        // if a comma follows...
        if (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens)) {
            // it must be followed by the fallback value.
            fallback = consumeAnchorFallback(tokens, depth, state);
            if (!fallback)
                return { };
        }
        // if a comma does not follow, then there's no fallback value.
    } else {
        // if <anchor-element> and <anchor-size> is not present
        // then an optional fallback value follows
        fallback = consumeAnchorFallback(tokens, depth, state);
    }

    // Return type of this function. It's a <length> if it can be resolved, otherwise the
    // <length-percentage> fallback is resolved, which could be a percentage.
    auto type = Type::makeLength();
    if (fallback) {
        auto category = fallback->type.calculationCategory();
        ASSERT(category && (category == CSS::Category::Length || category == CSS::Category::LengthPercentage));

        type.percentHint = Type::determinePercentHint(*category);
    }

    state.requiresConversionData = true;

    auto anchorSize = AnchorSize {
        .elementName = WTF::move(maybeAnchorElement),
        .dimension = maybeAnchorSize ? cssValueIDToAnchorSizeDimension(*maybeAnchorSize) : std::nullopt,
        .fallback = fallback ? std::make_optional(WTF::move(fallback->child)) : std::nullopt
    };

    return TypedChild {
        .child = makeChild(WTF::move(anchorSize), type),
        .type = type
    };
}

std::optional<TypedChild> parseCalcFunction(CSSParserTokenRange& tokens, CSSValueID functionID, int depth, ParserState& state)
{
    if (checkDepth(depth) != ParseStatus::Ok)
        return std::nullopt;

    switch (functionID) {
    case CSSValueID::WebkitCalc:
    case CSSValueID::Calc:
        // <calc()>  = calc( <calc-sum> )
        return parseCalcSum(tokens, depth, state);

    case CSSValueID::Min:
        // <min()>   = min( <calc-sum># )
        //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
        //     - OUTPUT: consistent type
        return consumeOneOrMoreArguments<Min>(tokens, depth, state);

    case CSSValueID::Max:
        // <max()>   = max( <calc-sum># )
        //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
        //     - OUTPUT: consistent type
        return consumeOneOrMoreArguments<Max>(tokens, depth, state);

    case CSSValueID::Clamp:
        // <clamp()> = clamp( [ <calc-sum> | none ], <calc-sum>, [ <calc-sum> | none ] )
        //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
        //     - OUTPUT: consistent type
        return consumeClamp(tokens, depth, state);

    case CSSValueID::Round:
        // <round()> = round( <rounding-strategy>?, <calc-sum>, <calc-sum>? )
        //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
        //     - OUTPUT: consistent type
        return consumeRound(tokens, depth, state);

    case CSSValueID::Mod:
        // <mod()>   = mod( <calc-sum>, <calc-sum> )
        //     - INPUT: "same" <number>, <dimension>, or <percentage>
        //     - OUTPUT: same type
        return consumeExactlyTwoArguments<Mod>(tokens, depth, state);

    case CSSValueID::Rem:
        // <rem()>   = rem( <calc-sum>, <calc-sum> )
        //     - INPUT: "same" <number>, <dimension>, or <percentage>
        //     - OUTPUT: same type
        return consumeExactlyTwoArguments<Rem>(tokens, depth, state);

    case CSSValueID::Sin:
        // <sin()>   = sin( <calc-sum> )
        //     - INPUT: <number> or <angle>
        //     - OUTPUT: <number> "made consistent"
        return consumeExactlyOneArgument<Sin>(tokens, depth, state);

    case CSSValueID::Cos:
        // <cos()>   = cos( <calc-sum> )
        //     - INPUT: <number> or <angle>
        //     - OUTPUT: <number> "made consistent"
        return consumeExactlyOneArgument<Cos>(tokens, depth, state);

    case CSSValueID::Tan:
        // <tan()>   = tan( <calc-sum> )
        //     - INPUT: <number> or <angle>
        //     - OUTPUT: <number> "made consistent"
        return consumeExactlyOneArgument<Tan>(tokens, depth, state);

    case CSSValueID::Asin:
        // <asin()>  = asin( <calc-sum> )
        //     - INPUT: <number>
        //     - OUTPUT: <angle> "made consistent"
        return consumeExactlyOneArgument<Asin>(tokens, depth, state);

    case CSSValueID::Acos:
        // <acos()>  = acos( <calc-sum> )
        //     - INPUT: <number>
        //     - OUTPUT: <angle> "made consistent"
        return consumeExactlyOneArgument<Acos>(tokens, depth, state);

    case CSSValueID::Atan:
        // <atan()>  = atan( <calc-sum> )
        //     - INPUT: <number>
        //     - OUTPUT: <angle> "made consistent"
        return consumeExactlyOneArgument<Atan>(tokens, depth, state);

    case CSSValueID::Atan2:
        // <atan2()> = atan2( <calc-sum>, <calc-sum> )
        //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
        //     - OUTPUT: <angle> "made consistent"
        return consumeExactlyTwoArguments<Atan2>(tokens, depth, state);

    case CSSValueID::Pow:
        // <pow()>   = pow( <calc-sum>, <calc-sum> )
        //     - INPUT: "consistent" <number>
        //     - OUTPUT: consistent type
        return consumeExactlyTwoArguments<Pow>(tokens, depth, state);

    case CSSValueID::Sqrt:
        // <sqrt()>  = sqrt( <calc-sum> )
        //     - INPUT: <number>
        //     - OUTPUT: <number> "made consistent"
        return consumeExactlyOneArgument<Sqrt>(tokens, depth, state);

    case CSSValueID::Hypot:
        // <hypot()> = hypot( <calc-sum># )
        //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
        //     - OUTPUT: consistent type
        return consumeOneOrMoreArguments<Hypot>(tokens, depth, state);

    case CSSValueID::Log:
        // <log()>   = log( <calc-sum>, <calc-sum>? )
        //     - INPUT: <number>
        //     - OUTPUT: <number> "made consistent"
        return consumeOneOrTwoArguments<Log>(tokens, depth, state);

    case CSSValueID::Exp:
        // <exp()>   = exp( <calc-sum> )
        //     - INPUT: <number>
        //     - OUTPUT: <number> "made consistent"
        return consumeExactlyOneArgument<Exp>(tokens, depth, state);

    case CSSValueID::Abs:
        // <abs()>   = abs( <calc-sum> )
        //     - INPUT: any
        //     - OUTPUT: input type
        return consumeExactlyOneArgument<Abs>(tokens, depth, state);

    case CSSValueID::Sign:
        // <sign()>  = sign( <calc-sum> )
        //     - INPUT: any
        //     - OUTPUT: <number> "made consistent"
        return consumeExactlyOneArgument<Sign>(tokens, depth, state);

    case CSSValueID::Random:
        // <random()> = random( <random-key>? , <calc-sum>, <calc-sum>, <calc-sum>? )
        //     - INPUT: "same" <number>, <dimension>, or <percentage>
        //     - OUTPUT: same type
        return consumeRandom(tokens, depth, state);

    case CSSValueID::Progress:
        // <progress()> = progress( <calc-sum>, <calc-sum>, <calc-sum> )
        //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
        //     - OUTPUT: <number> "made consistent"
        return consumeProgress(tokens, depth, state);


    case CSSValueID::CalcMix:
        // <calc-mix()> = calc-mix( [ <calc-sum> <percentage [0,100]>? ]# )
        //     - INPUT: "consistent" <number>, <dimension>, or <percentage> (referring to <calc-sum> arguments)
        //     - OUTPUT: consistent type
        return consumeCalcMix(tokens, depth, state);

    case CSSValueID::SiblingCount:
        // <sibling-count()> = sibling-count()
        //     - INPUT: none
        //     - OUTPUT: <integer>
        if (!state.propertyParserState.context.cssTreeCountingFunctionsEnabled)
            return { };
        if (state.propertyParserState.currentRule != StyleRuleType::Style && state.propertyParserState.currentRule != StyleRuleType::Keyframe)
            return { };
        if (state.propertyParserState.currentProperty == CSSPropertyID::Invalid && !state.propertyParserState.treeCountingFunctionsAllowed)
            return { };
        state.requiresConversionData = true;
        return consumeZeroArguments<SiblingCount>(tokens, depth, state);

    case CSSValueID::SiblingIndex:
        // <sibling-index()> = sibling-index()
        //     - INPUT: none
        //     - OUTPUT: <integer>
        if (!state.propertyParserState.context.cssTreeCountingFunctionsEnabled)
            return { };
        if (state.propertyParserState.currentRule != StyleRuleType::Style && state.propertyParserState.currentRule != StyleRuleType::Keyframe)
            return { };
        if (state.propertyParserState.currentProperty == CSSPropertyID::Invalid && !state.propertyParserState.treeCountingFunctionsAllowed)
            return { };
        state.requiresConversionData = true;
        return consumeZeroArguments<SiblingIndex>(tokens, depth, state);

    case CSSValueID::Anchor:
        return consumeAnchor(tokens, depth, state);

    case CSSValueID::AnchorSize:
        return consumeAnchorSize(tokens, depth, state);

    default:
        break;
    }

    return std::nullopt;
}

std::optional<TypedChild> parseCalcSum(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // <calc-sum> = <calc-product> [ [ '+' | '-' ] <calc-product> ]*

    if (checkDepth(depth) != ParseStatus::Ok)
        return std::nullopt;

    auto originalTokens = tokens.span();

    auto firstValue = parseCalcProduct(tokens, depth, state);
    if (!firstValue)
        return std::nullopt;

    auto sumType = firstValue->type;
    Vector<Child> children;

    while (!tokens.atEnd()) {
        auto& token = tokens.peek();
        char operatorCharacter = token.type() == DelimiterToken ? token.delimiter() : 0;
        if (operatorCharacter != static_cast<char>(Operator::Sum) && operatorCharacter != static_cast<char>(Operator::Negate))
            break;

        auto previousToken = originalTokens[tokens.begin() - originalTokens.data() - 1];
        if (!CSSTokenizer::isWhitespace(previousToken.type()))
            return std::nullopt; // calc(1px+ 2px) is invalid

        tokens.consume();
        if (!CSSTokenizer::isWhitespace(tokens.peek().type()))
            return std::nullopt; // calc(1px +2px) is invalid

        tokens.consumeIncludingWhitespace();

        auto nextValue = parseCalcProduct(tokens, depth, state);
        if (!nextValue)
            return std::nullopt;

        if (operatorCharacter == static_cast<char>(Operator::Negate)) {
            auto negate = [](TypedChild& next, ParserState& state) -> std::optional<TypedChild> {
                Negate negate { WTF::move(next.child) };
                auto negateType = next.type;

                if (auto* simplificationOptions = state.simplificationOptions) {
                    if (auto replacement = simplify(negate, *simplificationOptions))
                        return TypedChild { WTF::move(*replacement), negateType };
                }
                return TypedChild { makeChild(WTF::move(negate), negateType), negateType };
            };

            nextValue = negate(*nextValue, state);
        }

        if (firstValue) {
            children.append(WTF::move(firstValue->child));
            firstValue = std::nullopt;
        }

        auto newType = Type::add(sumType, nextValue->type);
        if (!newType)
            return std::nullopt;

        sumType = *newType;
        children.append(WTF::move(nextValue->child));
    }

    if (children.isEmpty())
        return firstValue;

    Sum sum { WTF::move(children) };

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(sum, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), sumType };
    }

    return TypedChild { makeChild(WTF::move(sum), sumType), sumType };
}

std::optional<TypedChild> parseCalcProduct(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // <calc-product> = <calc-value> [ [ '*' | '/' ] <calc-value> ]*

    if (checkDepth(depth) != ParseStatus::Ok)
        return std::nullopt;

    auto firstValue = parseCalcValue(tokens, depth, state);
    if (!firstValue)
        return std::nullopt;

    auto productType = firstValue->type;
    Vector<Child> children;

    while (!tokens.atEnd()) {
        auto& token = tokens.peek();
        char operatorCharacter = token.type() == DelimiterToken ? token.delimiter() : 0;
        if (operatorCharacter != static_cast<char>(Operator::Product) && operatorCharacter != static_cast<char>(Operator::Invert))
            break;
        tokens.consumeIncludingWhitespace();

        auto nextValue = parseCalcValue(tokens, depth, state);
        if (!nextValue)
            return std::nullopt;

        if (operatorCharacter == static_cast<char>(Operator::Invert)) {
            auto invert = [](TypedChild& next, ParserState& state) -> std::optional<TypedChild> {
                Invert invert { WTF::move(next.child) };
                auto invertType = Type::invert(next.type);

                if (auto* simplificationOptions = state.simplificationOptions) {
                    if (auto replacement = simplify(invert, *simplificationOptions))
                        return TypedChild { WTF::move(*replacement), invertType };
                }
                return TypedChild { makeChild(WTF::move(invert), invertType), invertType };
            };

            nextValue = invert(*nextValue, state);
        }

        if (firstValue) {
            children.append(WTF::move(firstValue->child));
            firstValue = std::nullopt;
        }

        auto newType = Type::multiply(productType, nextValue->type);
        if (!newType)
            return std::nullopt;

        productType = *newType;
        children.append(WTF::move(nextValue->child));
    }

    if (children.isEmpty())
        return firstValue;

    Product product { WTF::move(children) };

    if (auto* simplificationOptions = state.simplificationOptions) {
        if (auto replacement = simplify(product, *simplificationOptions))
            return TypedChild { WTF::move(*replacement), productType };
    }
    return TypedChild { makeChild(WTF::move(product), productType), productType };
}

std::optional<TypedChild> parseCalcValue(CSSParserTokenRange& tokens, int depth, ParserState& state)
{
    // <calc-value> = <number> | <dimension> | <percentage> | <calc-keyword> | ( <calc-sum> )
    // <calc-keyword> = e | pi | infinity | -infinity | NaN
    //
    // NOTE: <calc-keyword> is extended for identifiers specified via CSSCalcSymbolsAllowed.

    if (checkDepth(depth) != ParseStatus::Ok)
        return std::nullopt;

    auto findBlock = [&](auto& tokens) -> std::optional<CSSValueID> {
        if (tokens.peek().type() == LeftParenthesisToken) {
            // Simple blocks (e.g. parenthesis around additional expressions) can be treated just like a nested calc().
            return CSSValueID::Calc;
        }

        if (auto functionId = tokens.peek().functionId(); isCalcFunction(functionId))
            return functionId;
        return std::nullopt;
    };

    if (auto functionId = findBlock(tokens)) {
        CSSParserTokenRange innerRange = tokens.consumeBlock();
        tokens.consumeWhitespace();
        innerRange.consumeWhitespace();

        auto function = parseCalcFunction(innerRange, *functionId, depth + 1, state);
        if (!function)
            return std::nullopt;

        if (!innerRange.atEnd()) {
            LOG_WITH_STREAM(Calc, stream << "Failed '" << nameLiteralForSerialization(*functionId) << "' function - extraneous tokens found");
            return std::nullopt;
        }

        return function;
    }

    auto token = tokens.consumeIncludingWhitespace();

    switch (token.type()) {
    case IdentToken:
        return parseCalcKeyword(token, state);
    case NumberToken:
        return parseCalcNumber(token, state);
    case PercentageToken:
        return parseCalcPercentage(token, state);
    case DimensionToken:
        return parseCalcDimension(token, state);
    default:
        break;
    }

    return std::nullopt;
}

std::optional<TypedChild> parseCalcKeyword(const CSSParserToken& token, ParserState& state)
{
    if (auto unit = state.parserOptions.allowedSymbols.get(token.id())) {
        auto child = Symbol { token.id(), *unit };
        auto type = Type::determineType(*unit);

        if (conversionToCanonicalUnitRequiresConversionData(*unit)) {
            if (state.propertyParserState.absoluteLengthUnitsOnly)
                return std::nullopt;
            state.requiresConversionData = true;
        }

        if (auto* simplificationOptions = state.simplificationOptions) {
            if (auto replacement = simplify(child, *simplificationOptions))
                return TypedChild { WTF::move(*replacement), type };
        }

        return TypedChild { makeChild(WTF::move(child)), type };
    }

    if (auto constant = lookupConstantNumber(token.id())) {
        auto [child, type] = *constant;
        return TypedChild { makeChild(WTF::move(child)), type };
    }

    return std::nullopt;
}

std::optional<TypedChild> parseCalcNumber(const CSSParserToken& token, ParserState&)
{
    auto child = Number { .value = token.numericValue() };
    auto type = Type { };

    return TypedChild { makeChild(WTF::move(child)), type };
}

std::optional<TypedChild> parseCalcPercentage(const CSSParserToken& token, ParserState& state)
{
    auto child = Percentage { .value = token.numericValue(), .hint = Type::determinePercentHint(state.parserOptions.category) };
    auto type = getType(child);

    return TypedChild { makeChild(WTF::move(child)), type };
}

std::optional<TypedChild> parseCalcDimension(const CSSParserToken& token, ParserState& state)
{
    if (token.unitType() == CSSUnitType::Unknown)
        return std::nullopt;

    auto child = makeNumeric(token.numericValue(), token.unitType());
    auto type = Type::determineType(token.unitType());

    if (conversionToCanonicalUnitRequiresConversionData(token.unitType())) {
        if (state.propertyParserState.absoluteLengthUnitsOnly)
            return std::nullopt;
        state.requiresConversionData = true;
    }

    if (auto* simplificationOptions = state.simplificationOptions)
        return TypedChild { copyAndSimplify(WTF::move(child), *simplificationOptions), type };
    return TypedChild { WTF::move(child), type };
}

} // namespace CSSCalc
} // namespace WebCore
