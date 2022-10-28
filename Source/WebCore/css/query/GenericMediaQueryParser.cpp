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

#include "CSSPropertyParserHelpers.h"
#include "CSSValue.h"
#include "MediaQueryParserContext.h"

namespace WebCore {
namespace MQ {

static AtomString consumeFeatureName(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullAtom();
    return range.consumeIncludingWhitespace().value().convertToASCIILowercaseAtom();
}

std::optional<Feature> GenericMediaQueryParserBase::consumeFeature(CSSParserTokenRange& range)
{
    auto rangeCopy = range;
    if (auto feature = consumeBooleanOrPlainFeature(range))
        return feature;

    range = rangeCopy;
    return consumeRangeFeature(range);
};

std::optional<Feature> GenericMediaQueryParserBase::consumeBooleanOrPlainFeature(CSSParserTokenRange& range)
{
    auto consumePlainFeatureName = [&]() -> std::pair<AtomString, ComparisonOperator> {
        auto name = consumeFeatureName(range);
        if (name.isEmpty())
            return { };
        if (name.startsWith("min-"_s))
            return { StringView(name).substring(4).toAtomString(), ComparisonOperator::GreaterThanOrEqual };
        if (name.startsWith("max-"_s))
            return { StringView(name).substring(4).toAtomString(), ComparisonOperator::LessThanOrEqual };
        if (name.startsWith("-webkit-min-"_s))
            return { "-webkit-"_s + StringView(name).substring(12), ComparisonOperator::GreaterThanOrEqual };
        if (name.startsWith("-webkit-max-"_s))
            return { "-webkit-"_s + StringView(name).substring(12), ComparisonOperator::LessThanOrEqual };

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
    if (range.atEnd())
        return { };

    auto value = consumeValue(range);
    if (!value)
        return { };

    if (!range.atEnd())
        return { };

    return Feature { featureName, Syntax::Plain, { }, Comparison { op, WTFMove(value) } };
}

std::optional<Feature> GenericMediaQueryParserBase::consumeRangeFeature(CSSParserTokenRange& range)
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
        auto value = consumeValue(range);
        if (!value)
            return { };
        auto op = consumeRangeOperator();
        if (!op) {
            didFailParsing = true;
            return { };
        }

        return Comparison { *op, WTFMove(value) };
    };

    auto consumeRightComparison = [&]() -> std::optional<Comparison> {
        auto op = consumeRangeOperator();
        if (!op)
            return { };
        auto value = consumeValue(range);
        if (!value) {
            didFailParsing = true;
            return { };
        }

        return Comparison { *op, WTFMove(value) };
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

    return Feature { WTFMove(featureName), Syntax::Range, WTFMove(leftComparison), WTFMove(rightComparison) };
}

RefPtr<CSSValue> GenericMediaQueryParserBase::consumeValue(CSSParserTokenRange& range)
{
    if (range.atEnd())
        return nullptr;
    if (auto value = CSSPropertyParserHelpers::consumeIdent(range))
        return value;
    if (auto value = CSSPropertyParserHelpers::consumeLength(range, m_context.mode, ValueRange::All))
        return value;
    if (auto value = CSSPropertyParserHelpers::consumeAspectRatioValue(range))
        return value;
    if (auto value = CSSPropertyParserHelpers::consumeResolution(range))
        return value;

    return nullptr;
}

bool GenericMediaQueryParserBase::validateFeatureAgainstSchema(Feature& feature, const FeatureSchema& schema)
{
    auto valueTypeForValue = [](auto& value) -> std::optional<FeatureSchema::ValueType> {
        if (value.isInteger())
            return FeatureSchema::ValueType::Integer;
        if (value.isNumber())
            return FeatureSchema::ValueType::Number;
        if (value.isLength())
            return FeatureSchema::ValueType::Length;
        if (value.isResolution())
            return FeatureSchema::ValueType::Resolution;
        return { };
    };

    auto validateValue = [&](auto& value) {
        if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
            if (primitiveValue->isValueID())
                return schema.valueIdentifiers.contains(primitiveValue->valueID());
            auto valueType = valueTypeForValue(*primitiveValue);
            return valueType && schema.valueTypes.contains(*valueType);
        }
        if (auto* list = dynamicDowncast<CSSValueList>(value)) {
            if (!schema.valueTypes.contains(FeatureSchema::ValueType::Ratio))
                return false;
            if (list->length() != 2 || list->separator() != CSSValue::SlashSeparator)
                return false;
            auto first = dynamicDowncast<CSSPrimitiveValue>(list->item(0));
            auto second = dynamicDowncast<CSSPrimitiveValue>(list->item(1));
            return first && second && first->isNumberOrInteger() && second->isNumberOrInteger();
        }
        return false;
    };

    auto isValid = [&] {
        if (schema.type == FeatureSchema::Type::Discrete) {
            if (feature.syntax == Syntax::Range)
                return false;
            if (feature.rightComparison && feature.rightComparison->op != ComparisonOperator::Equal)
                return false;
        }

        if (feature.leftComparison) {
            if (!validateValue(*feature.leftComparison->value))
                return false;
        }
        if (feature.rightComparison) {
            if (!validateValue(*feature.rightComparison->value))
                return false;
        }
        return true;
    }();

    feature.schema = isValid ? &schema : nullptr;
    return isValid;
}

}
}
