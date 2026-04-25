/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "GenericMediaQueryParser.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParser.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+Ratio.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserState.h"
#include "CSSSubstitutionParser.h"
#include "MediaQueryParserContext.h"
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace MQ {

static AtomString consumeFeatureName(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullAtom();
    auto name = range.consumeIncludingWhitespace().value();
    if (isCustomPropertyName(name))
        return name.toAtomString();
    return name.convertToASCIILowercaseAtom();
}

std::optional<Feature> FeatureParser::consumeFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto rangeCopy = range;
    if (auto feature = consumeBooleanOrPlainFeature(range, context))
        return feature;

    range = rangeCopy;
    return consumeRangeFeature(range, context);
};

static std::optional<Value> consumeCustomPropertyValue(AtomString propertyName, CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto valueRange = range;
    range.consumeAll();

    // Syntax is that of a valid declaration so !important is allowed. It just gets ignored.
    CSSParser::consumeTrailingImportantAndWhitespace(valueRange);

    if (valueRange.atEnd())
        return Value { CSSCustomPropertyValue::createEmpty(propertyName) };

    auto declaration = CSSSubstitutionParser::parseDeclarationValue(propertyName, valueRange, context.context);
    if (!declaration)
        return std::nullopt;

    return Value { declaration.releaseNonNull() };
}

static std::optional<Value> consumeValue(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    using namespace CSSPropertyParserHelpers;

    if (range.atEnd())
        return std::nullopt;

    if (auto value = consumeUnresolvedIdent(range))
        return Value { WTF::move(*value) };

    auto parserState = CSS::PropertyParserState {
        .context = context.context,
    };

    if (auto value = consumeUnresolvedRatioWithBothNumeratorAndDenominator(range, parserState))
        return Value { WTF::move(*value) };
    if (auto value = MetaConsumer<CSS::Integer<>>::consume(range, parserState))
        return Value { WTF::move(*value) };
    if (auto value = MetaConsumer<CSS::Number<>>::consume(range, parserState))
        return Value { WTF::move(*value) };
    // FIXME: Figure out and document why overrideParserMode is explicitly set to HTMLStandardMode here.
    if (auto value = MetaConsumer<CSS::Length<>>::consume(range, parserState, { .overrideParserMode = HTMLStandardMode }))
        return Value { WTF::move(*value) };
    if (auto value = MetaConsumer<CSS::Resolution<>>::consume(range, parserState))
        return Value { WTF::move(*value) };

    return std::nullopt;
}

std::optional<Feature> FeatureParser::consumeBooleanOrPlainFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto consumePlainFeatureName = [&]() -> std::pair<AtomString, ComparisonOperator> {
        auto name = consumeFeatureName(range);
        if (name.isEmpty())
            return { };
        if (isCustomPropertyName(name))
            return { name, ComparisonOperator::Equal };
        if (name.startsWith("min-"_s))
            return { StringView(name).substring(4).toAtomString(), ComparisonOperator::GreaterThanOrEqual };
        if (name.startsWith("max-"_s))
            return { StringView(name).substring(4).toAtomString(), ComparisonOperator::LessThanOrEqual };
        if (name.startsWith("-webkit-min-"_s))
            return { makeAtomString("-webkit-"_s, StringView(name).substring(12)), ComparisonOperator::GreaterThanOrEqual };
        if (name.startsWith("-webkit-max-"_s))
            return { makeAtomString("-webkit-"_s, StringView(name).substring(12)), ComparisonOperator::LessThanOrEqual };

        return { name, ComparisonOperator::Equal };
    };

    auto [featureName, op] = consumePlainFeatureName();
    if (featureName.isEmpty())
        return { };

    range.consumeWhitespace();

    if (range.atEnd()) {
        if (op != ComparisonOperator::Equal)
            return { };

        return Feature { featureName, Syntax::Boolean, { }, { } };
    }

    if (range.peek().type() != ColonToken)
        return { };

    range.consumeIncludingWhitespace();

    auto value = isCustomPropertyName(featureName) ? consumeCustomPropertyValue(featureName, range, context) : consumeValue(range, context);

    if (!value)
        return { };

    if (!range.atEnd())
        return { };

    return Feature { featureName, Syntax::Plain, { }, Comparison { op, WTF::move(value) } };
}

std::optional<Feature> FeatureParser::consumeRangeFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto consumeRangeOperator = [&]() -> std::optional<ComparisonOperator> {
        if (range.atEnd())
            return { };
        auto opToken = range.consume();
        if (range.atEnd() || opToken.type() != DelimiterToken)
            return { };

        switch (opToken.delimiter()) {
        case '=':
            range.consumeWhitespace();
            return ComparisonOperator::Equal;
        case '<':
            if (range.peek().type() == DelimiterToken && range.peek().delimiter() == '=') {
                range.consumeIncludingWhitespace();
                return ComparisonOperator::LessThanOrEqual;
            }
            range.consumeWhitespace();
            return ComparisonOperator::LessThan;
        case '>':
            if (range.peek().type() == DelimiterToken && range.peek().delimiter() == '=') {
                range.consumeIncludingWhitespace();
                return ComparisonOperator::GreaterThanOrEqual;
            }
            range.consumeWhitespace();
            return ComparisonOperator::GreaterThan;
        default:
            return { };
        }
    };

    bool didFailParsing = false;

    auto consumeLeftComparison = [&]() -> std::optional<Comparison> {
        if (range.peek().type() == IdentToken)
            return { };
        auto value = consumeValue(range, context);
        if (!value)
            return { };
        auto op = consumeRangeOperator();
        if (!op) {
            didFailParsing = true;
            return { };
        }

        return Comparison { *op, WTF::move(value) };
    };

    auto consumeRightComparison = [&]() -> std::optional<Comparison> {
        auto op = consumeRangeOperator();
        if (!op)
            return { };
        auto value = consumeValue(range, context);
        if (!value) {
            didFailParsing = true;
            return { };
        }

        return Comparison { *op, WTF::move(value) };
    };

    auto leftComparison = consumeLeftComparison();

    auto featureName = consumeFeatureName(range);
    if (featureName.isEmpty())
        return { };

    auto rightComparison = consumeRightComparison();

    auto validateComparisons = [&] {
        if (didFailParsing)
            return false;
        if (!leftComparison && !rightComparison)
            return false;
        if (!leftComparison || !rightComparison)
            return true;
        // Disallow comparisons like (a=b=c), (a=b<c).
        if (leftComparison->op == ComparisonOperator::Equal || rightComparison->op == ComparisonOperator::Equal)
            return false;
        // Disallow comparisons like (a<b>c).
        bool leftIsLess = leftComparison->op == ComparisonOperator::LessThan || leftComparison->op == ComparisonOperator::LessThanOrEqual;
        bool rightIsLess = rightComparison->op == ComparisonOperator::LessThan || rightComparison->op == ComparisonOperator::LessThanOrEqual;
        return leftIsLess == rightIsLess;
    };

    if (!range.atEnd() || !validateComparisons())
        return { };

    return Feature { WTF::move(featureName), Syntax::Range, WTF::move(leftComparison), WTF::move(rightComparison) };
}

bool FeatureParser::validateFeatureAgainstSchema(Feature& feature, const FeatureSchema& schema)
{
    auto isValid = [&] {
        auto validateValue = [&](std::optional<Value>& value) -> bool {
            if (!value)
                return false;

            switch (schema.valueType) {
            case FeatureSchema::ValueType::Integer:
                return WTF::holdsAlternative<CSS::Integer<>>(*value);

            case FeatureSchema::ValueType::Number:
                return WTF::holdsAlternative<CSS::Number<>>(*value)
                    || WTF::holdsAlternative<CSS::Integer<>>(*value);

            case FeatureSchema::ValueType::Length:
                if (auto* integerValue = std::get_if<CSS::Integer<>>(&*value)) {
                    return WTF::switchOn(*integerValue,
                        [](const CSS::Integer<>::Raw& raw) {
                            return !raw.value;
                        },
                        [](const CSS::Integer<>::Calc&) {
                            // FIXME: Document why any integer calc() expression is valid for <length> schemas or change this.
                            return true;
                        }
                    );
                }
                return WTF::holdsAlternative<CSS::Length<>>(*value);

            case FeatureSchema::ValueType::Resolution:
                return WTF::holdsAlternative<CSS::Resolution<>>(*value);

            case FeatureSchema::ValueType::Identifier:
                if (auto* keyword = std::get_if<CSS::Keyword>(&*value))
                    return keyword && schema.valueIdentifiers.contains(keyword->value);
                return false;

            case FeatureSchema::ValueType::Ratio:
                return WTF::switchOn(*value,
                    [&](const CSS::Ratio&) {
                        return true;
                    },
                    [&](const CSS::Integer<>& integer) {
                        auto resolved = WTF::switchOn(integer,
                            [](const CSS::Integer<>::Raw& raw) {
                                return raw.value;
                            },
                            [](const CSS::Integer<>::Calc& calc) {
                                return protect(calc.calcValue())->doubleValueDeprecated();
                            }
                        );
                        if (resolved < 0)
                            return false;
                        value = CSS::Ratio { resolved, 1.0 };
                        return true;
                    },
                    [&](const CSS::Number<>& number) {
                        double resolved = WTF::switchOn(number,
                            [](const CSS::Number<>::Raw& raw) {
                                return raw.value;
                            },
                            [](const CSS::Number<>::Calc& calc) {
                                return protect(calc.calcValue())->doubleValueDeprecated();
                            }
                        );
                        if (resolved < 0)
                            return false;
                        value = CSS::Ratio { resolved, 1.0 };
                        return true;
                    },
                    [](const auto&) {
                        return false;
                    }
                );

            case FeatureSchema::ValueType::CustomProperty:
                return WTF::holdsAlternative<Ref<CSSCustomPropertyValue>>(*value);
            }
            ASSERT_NOT_REACHED();
            return false;
        };

        if (schema.type == FeatureSchema::Type::Discrete) {
            if (feature.syntax == Syntax::Range)
                return false;
            if (feature.rightComparison && feature.rightComparison->op != ComparisonOperator::Equal)
                return false;
        }
        if (schema.valueType == FeatureSchema::ValueType::CustomProperty) {
            if (!isCustomPropertyName(feature.name))
                return false;
        }
        if (feature.leftComparison) {
            if (!validateValue(feature.leftComparison->value))
                return false;
        }
        if (feature.rightComparison) {
            if (!validateValue(feature.rightComparison->value))
                return false;
        }
        return true;
    }();

    feature.schema = isValid ? &schema : nullptr;
    return isValid;
}

}
}
