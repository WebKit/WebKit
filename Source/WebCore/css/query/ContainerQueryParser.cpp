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
#include "ContainerQueryParser.h"

#include "CSSCustomPropertyValue.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSSubstitutionParser.h"
#include "ContainerQueryFeatures.h"
#include "MediaQueryParserContext.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace CQ {

using namespace MQ;

// Parses a <style-range> per css-conditional-5 §6.2:
//   <style-range> = <style-range-value> <mf-comparison> <style-range-value>
//                 | <style-range-value> <mf-lt> <style-range-value> <mf-lt> <style-range-value>
//                 | <style-range-value> <mf-gt> <style-range-value> <mf-gt> <style-range-value>
//   <style-range-value> = <custom-property-name> | <style-feature-value>
// Each operand is captured as an unresolved declaration value and resolved against the query
// container during evaluation; a bare <custom-property-name> is detected there and treated as var().
static std::optional<Feature> consumeStyleRangeFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto isComparisonDelimiter = [](const CSSParserToken& token) {
        return token.type() == DelimiterToken
            && (token.delimiter() == '<' || token.delimiter() == '>' || token.delimiter() == '=');
    };

    // A <style-feature-value> can't contain comparison tokens, so any comparison delimiter at the
    // top level of the block separates operands. consumeComponentValue() skips over whole blocks
    // (calc(), var(), attr(), ...), keeping their inner tokens out of the split.
    auto consumeOperandRange = [&]() -> CSSParserTokenRange {
        auto start = range;
        while (!range.atEnd() && !isComparisonDelimiter(range.peek()))
            range.consumeComponentValue();
        auto operand = start.rangeUntil(range);
        operand.consumeWhitespace();
        operand.trimTrailingWhitespace();
        return operand;
    };

    struct Operand {
        AtomString customPropertyName;
        Ref<CSSCustomPropertyValue> value;
    };

    auto consumeOperand = [&]() -> std::optional<Operand> {
        auto operandRange = consumeOperandRange();
        if (operandRange.atEnd())
            return { };

        auto customPropertyName = MQ::bareCustomPropertyName(operandRange.span());

        // A non-name operand (literal, calc(), var(), attr(), ...) still needs a valid custom-property
        // name for parsing/resolution; it is never looked up as a real property.
        static MainThreadNeverDestroyed<const AtomString> operandName("--style-range-operand"_s);
        auto name = customPropertyName.isNull() ? operandName.get() : customPropertyName;
        auto value = CSSSubstitutionParser::parseDeclarationValue(name, operandRange, context.context);
        if (!value)
            return { };

        return Operand { customPropertyName, value.releaseNonNull() };
    };

    auto firstOperand = consumeOperand();
    if (!firstOperand)
        return { };

    auto firstOperator = FeatureParser::consumeRangeComparisonOperator(range);
    if (!firstOperator)
        return { };

    auto secondOperand = consumeOperand();
    if (!secondOperand)
        return { };

    std::optional<ComparisonOperator> secondOperator;
    std::optional<Operand> thirdOperand;
    if (!range.atEnd()) {
        secondOperator = FeatureParser::consumeRangeComparisonOperator(range);
        if (!secondOperator)
            return { };
        thirdOperand = consumeOperand();
        if (!thirdOperand)
            return { };
    }

    if (!range.atEnd())
        return { };

    if (secondOperator && !isConsistentThreeWayComparison(*firstOperator, *secondOperator))
        return { };

    // The center operand is the second one for three-operand ranges, the first otherwise. A bare
    // <custom-property-name> center is stored as the feature name (matching the plain/size model, so
    // serialization and custom-property invalidation keep working); other centers go in `subject`.
    auto setCenter = [](Feature& feature, Operand&& center) {
        if (!center.customPropertyName.isNull())
            feature.name = center.customPropertyName;
        else
            feature.subject = Value { WTF::move(center.value) };
    };

    Feature feature;
    feature.syntax = Syntax::Range;

    if (!thirdOperand) {
        setCenter(feature, WTF::move(*firstOperand));
        feature.rightComparison = Comparison { *firstOperator, Value { WTF::move(secondOperand->value) } };
        return feature;
    }

    feature.leftComparison = Comparison { *firstOperator, Value { WTF::move(firstOperand->value) } };
    setCenter(feature, WTF::move(*secondOperand));
    feature.rightComparison = Comparison { *secondOperator, Value { WTF::move(thirdOperand->value) } };
    return feature;
}

// A style() feature queries a single custom property, in boolean (style(--foo)), plain
// (style(--foo: value)) or <style-range> form. Unlike size features, style features are always
// custom properties, so this is simpler than the generic boolean/plain/range path.
static std::optional<Feature> consumeStyleFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto rangeForStyleRange = range;

    auto name = FeatureParser::consumeFeatureName(range);
    if (isCustomPropertyName(name)) {
        range.consumeWhitespace();
        if (range.atEnd())
            return Feature { .name = name, .syntax = Syntax::Boolean };

        if (range.peek().type() == ColonToken) {
            range.consumeIncludingWhitespace();
            auto value = FeatureParser::consumeCustomPropertyValue(name, range, context);
            if (!value || !range.atEnd())
                return { };
            return Feature {
                .name = name,
                .syntax = Syntax::Plain,
                .rightComparison = Comparison { ComparisonOperator::Equal, WTF::move(value) }
            };
        }
    }

    range = rangeForStyleRange;
    return consumeStyleRangeFeature(range, context);
}

Vector<const MQ::FeatureSchema*> ContainerQueryParser::featureSchemas()
{
    return Features::allSchemas();
}

std::optional<ContainerQuery> ContainerQueryParser::consumeContainerQuery(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    Vector<ContainerCondition> queries;

    do {
        auto query = CQ::ContainerQueryParser::consumeContainerCondition(range, context);
        if (!query)
            return std::nullopt;

        queries.append(WTF::move(*query));
    } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));

    if (!range.atEnd())
        return std::nullopt;

    ASSERT(!queries.isEmpty());

    return queries;
}

std::optional<ContainerCondition> ContainerQueryParser::consumeContainerCondition(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto consumeName = [&] {
        if (range.peek().type() == LeftParenthesisToken || range.peek().type() == FunctionToken)
            return nullAtom();

        return CSSPropertyParserHelpers::consumeEagerlyResolvableCustomIdentRawExcluding(range, { CSSValueNone, CSSValueAnd, CSSValueOr, CSSValueNot }).toAtomString();
    };

    auto name = consumeName();

    auto condition = consumeCondition(range, context);

    if (!condition) {
        if (name.isEmpty())
            return { };
        // it's valid to have a named container query without a condition, like "@container --name {}"
        condition = MQ::Condition { };
    }

    ContainerRequirements requirements;
    auto containsUnknownFeature = ContainsUnknownFeature::No;

    traverseFeatures(*condition, [&](auto& feature) {
        requirements.sizeAxes.add(requiredAxesForFeature(feature));
        if (Features::isScrollStateFeature(feature.schema))
            requirements.scrollState = true;
        if (!feature.schema)
            containsUnknownFeature = ContainsUnknownFeature::Yes;
    });

    return ContainerCondition { name, *condition, requirements, containsUnknownFeature };
}

bool ContainerQueryParser::isValidFunctionId(CSSValueID functionId)
{
    return functionId == CSSValueStyle || functionId == CSSValueScrollState;
}

const MQ::FeatureSchema* ContainerQueryParser::schemaForFeatureName(const AtomString& name, const MediaQueryParserContext& context, State& state)
{
    if (state.inFunctionId == CSSValueStyle)
        return &Features::style();

    if (state.inFunctionId == CSSValueScrollState) {
        if (!context.context.cssScrollStateContainerQueriesEnabled)
            return nullptr;
        return Features::scrollState(name);
    }

    return GenericMediaQueryParser<ContainerQueryParser>::schemaForFeatureName(name, context, state);
}

std::optional<MQ::Feature> ContainerQueryParser::consumeAndValidateFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context, State& state)
{
    // style() features (boolean, plain and <style-range>) are all custom-property queries.
    if (state.inFunctionId == CSSValueStyle) {
        auto feature = consumeStyleFeature(range, context);
        if (!feature)
            return { };

        feature->schema = &Features::style();
        return feature;
    }

    return GenericMediaQueryParser<ContainerQueryParser>::consumeAndValidateFeature(range, context, state);
}

}
}
