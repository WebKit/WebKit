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
#include "ContainerQueryFeatures.h"

#include "BoxSides.h"
#include "CSSCustomPropertyValue.h"
#include "CSSParserContext.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+FrequencyDefinitions.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserConsumer+TimeDefinitions.h"
#include "CSSPropertyParserState.h"
#include "ComputedStyleDependencies.h"
#include "ContainerQueryEvaluator.h"
#include "GenericMediaQueryParser.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "StyleBuilder.h"
#include "StyleCustomProperty.h"
#include "StyleCustomPropertyRegistry.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleZoomPrimitivesInlines.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore::CQ {

using namespace MQ;

struct SizeFeatureSchema : public FeatureSchema {
    SizeFeatureSchema(const AtomString& name, Type type, ValueType valueType, OptionSet<MediaQueryDynamicDependency> dependencies, FixedVector<CSSValueID>&& valueIdentifiers = { })
        : FeatureSchema(name, type, valueType, dependencies, WTF::move(valueIdentifiers))
    {
    }

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const
    {
        // "If the query container does not have a principal box, or the principal box is not a layout containment box,
        // or the query container does not support container size queries on the relevant axes, then the result of
        // evaluating the size feature is unknown."
        // https://drafts.csswg.org/css-contain-3/#size-container
        CheckedPtr renderer = dynamicDowncast<RenderBox>(context.renderer.get());
        if (!renderer)
            return EvaluationResult::Unknown;

        if (!renderer->hasEligibleContainmentForSizeQuery())
            return EvaluationResult::Unknown;

        return evaluate(feature, *renderer, context.conversionData);
    }

    virtual EvaluationResult evaluate(const Feature&, const RenderBox&, const CSSToLengthConversionData&) const = 0;
};

namespace Features {

struct WidthFeatureSchema : public SizeFeatureSchema {
    WidthFeatureSchema()
        : SizeFeatureSchema("width"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length, MediaQueryDynamicDependency::Viewport)
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        auto width = Style::adjustForAbsoluteZoom(renderer.contentBoxWidth(), renderer);
        return evaluateLengthFeature(feature, width, conversionData);
    }
};

struct HeightFeatureSchema : public SizeFeatureSchema {
    HeightFeatureSchema()
        : SizeFeatureSchema("height"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length, MediaQueryDynamicDependency::Viewport)
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        auto height = Style::adjustForAbsoluteZoom(renderer.contentBoxHeight(), renderer);
        return evaluateLengthFeature(feature, height, conversionData);
    }
};

struct InlineSizeFeatureSchema : public SizeFeatureSchema {
    InlineSizeFeatureSchema()
        : SizeFeatureSchema("inline-size"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length, MediaQueryDynamicDependency::Viewport)
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        auto logicalWidth = Style::adjustForAbsoluteZoom(renderer.contentBoxLogicalWidth(), renderer);
        return evaluateLengthFeature(feature, logicalWidth, conversionData);
    }
};

struct BlockSizeFeatureSchema : public SizeFeatureSchema {
    BlockSizeFeatureSchema()
        : SizeFeatureSchema("block-size"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length, MediaQueryDynamicDependency::Viewport)
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        auto logicalHeight = Style::adjustForAbsoluteZoom(renderer.contentBoxLogicalHeight(), renderer);
        return evaluateLengthFeature(feature, logicalHeight, conversionData);
    }
};

struct AspectRatioFeatureSchema : public SizeFeatureSchema {
    AspectRatioFeatureSchema()
        : SizeFeatureSchema("aspect-ratio"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Ratio, MediaQueryDynamicDependency::Viewport)
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        return evaluateRatioFeature(feature, renderer.contentBoxSize(), conversionData);
    }
};

struct OrientationFeatureSchema : public SizeFeatureSchema {
    OrientationFeatureSchema()
        : SizeFeatureSchema("orientation"_s, FeatureSchema::Type::Discrete, FeatureSchema::ValueType::Identifier, MediaQueryDynamicDependency::Viewport, { CSSValuePortrait, CSSValueLandscape })
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        bool isPortrait = renderer.contentBoxHeight() >= renderer.contentBoxWidth();
        return evaluateIdentifierFeature(feature, isPortrait ? CSSValuePortrait : CSSValueLandscape, conversionData);
    }
};

// https://drafts.csswg.org/css-conditional-5/#typedef-style-range
enum class StyleRangeCategory : uint8_t { Number, Length, Percentage, Angle, Time, Frequency, Resolution };

struct StyleRangeValue {
    StyleRangeCategory category;
    double value; // In the canonical unit of the category.
    bool isUnitlessZero { false };
};

// Evaluates the (already substituted) computed value of a <style-range-value> into one of
// <number>, <length>, <percentage>, <angle>, <time>, <frequency> or <resolution>, computed
// with respect to the query container. Returns nullopt if it doesn't parse as a single value.
static std::optional<StyleRangeValue> evaluateStyleRangeValue(const Vector<CSSParserToken>& tokens, const FeatureEvaluationContext& context, const Style::ComputedStyle& style)
{
    using namespace CSSPropertyParserHelpers;

    CSSParserTokenRange range { tokens };
    range.consumeWhitespace();
    if (range.atEnd())
        return { };

    auto& conversionData = context.conversionData;
    auto parserState = CSS::PropertyParserState { .context = protect(context.document.get())->cssParserContext() };

    auto isFullyConsumed = [](CSSParserTokenRange consumed) {
        consumed.consumeWhitespace();
        return consumed.atEnd();
    };

    // <number> is tried first so that a unitless zero is categorized as a number (and can act as a
    // zero <length> when compared against a length).
    if (auto subrange = range; auto value = MetaConsumer<CSS::Number<>>::consume(subrange, parserState)) {
        if (isFullyConsumed(subrange)) {
            auto number = Style::evaluate<double>(Style::toStyle(*value, conversionData));
            return StyleRangeValue { StyleRangeCategory::Number, number, !number };
        }
    }
    // overrideParserMode matches MQ::consumeValue's <length> parsing so quirky/quirks-mode lengths behave
    // identically here. See the FIXME there.
    if (auto subrange = range; auto value = MetaConsumer<CSS::Length<CSS::AllUnzoomed>>::consume(subrange, parserState, { .overrideParserMode = HTMLStandardMode })) {
        if (isFullyConsumed(subrange))
            return StyleRangeValue { StyleRangeCategory::Length, Style::evaluate<double>(Style::toStyle(*value, conversionData), style.usedZoomForLength()) };
    }
    if (auto subrange = range; auto value = MetaConsumer<CSS::Percentage<>>::consume(subrange, parserState)) {
        if (isFullyConsumed(subrange))
            return StyleRangeValue { StyleRangeCategory::Percentage, Style::evaluate<double>(Style::toStyle(*value, conversionData)) };
    }
    if (auto subrange = range; auto value = MetaConsumer<CSS::Angle<>>::consume(subrange, parserState)) {
        if (isFullyConsumed(subrange))
            return StyleRangeValue { StyleRangeCategory::Angle, Style::evaluate<double>(Style::toStyle(*value, conversionData)) };
    }
    if (auto subrange = range; auto value = MetaConsumer<CSS::Time<>>::consume(subrange, parserState)) {
        if (isFullyConsumed(subrange))
            return StyleRangeValue { StyleRangeCategory::Time, Style::evaluate<double>(Style::toStyle(*value, conversionData)) };
    }
    if (auto subrange = range; auto value = MetaConsumer<CSS::Frequency<>>::consume(subrange, parserState)) {
        if (isFullyConsumed(subrange))
            return StyleRangeValue { StyleRangeCategory::Frequency, Style::evaluate<double>(Style::toStyle(*value, conversionData)) };
    }
    if (auto subrange = range; auto value = MetaConsumer<CSS::Resolution<>>::consume(subrange, parserState)) {
        if (isFullyConsumed(subrange))
            return StyleRangeValue { StyleRangeCategory::Resolution, Style::evaluate<double>(Style::toStyle(*value, conversionData)) };
    }

    return { };
}

// "If each <style-range-value> from the range have the same type" — a unitless zero is treated as a
// zero <length> when compared against a <length>.
static bool styleRangeValuesShareCategory(std::initializer_list<const std::optional<StyleRangeValue>*> operands)
{
    std::optional<StyleRangeCategory> category;
    bool hasUnitlessZero = false;
    for (auto* operand : operands) {
        if (!*operand)
            continue;
        if ((*operand)->isUnitlessZero) {
            hasUnitlessZero = true;
            continue;
        }
        if (!category)
            category = (*operand)->category;
        else if (*category != (*operand)->category)
            return false;
    }
    if (hasUnitlessZero && category && *category != StyleRangeCategory::Number && *category != StyleRangeCategory::Length)
        return false;
    return true;
}

struct StyleFeatureSchema : public FeatureSchema {
    StyleFeatureSchema()
        : FeatureSchema("style"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::CustomProperty, { })
    {
    }

    // FeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const FeatureEvaluationContext& context) const override
    {
        CheckedPtr style = context.conversionData.style();
        if (!style || !context.conversionData.parentStyle())
            return EvaluationResult::False;

        if (feature.syntax == Syntax::Range)
            return evaluateRange(feature, context, *style);

        RefPtr customPropertyValue = style->customPropertyValue(feature.name);
        if (!feature.rightComparison)
            return toEvaluationResult(customPropertyValue && !customPropertyValue->isGuaranteedInvalid());

        auto resolvedFeatureValue = [&] -> RefPtr<const Style::CustomProperty> {
            auto* featureValue = std::get_if<Ref<CSSCustomPropertyValue>>(&*feature.rightComparison->value);
            ASSERT(featureValue);

            // Resolve the queried custom property value for var() references, css-wide keywords and registered properties.
            auto builderContext = Style::BuilderContext {
                context.document.get(),
                context.conversionData.parentStyle(),
                context.conversionData.rootStyle(),
                context.conversionData.elementForContainerUnitResolution()
            };

            auto dummyStyle = Style::ComputedStyle::clone(*style);
            auto dummyMatchResult = Style::MatchResult::create();

            auto styleBuilder = Style::Builder { dummyStyle, WTF::move(builderContext), dummyMatchResult };
            return styleBuilder.resolveCustomPropertyForContainerQueries(*featureValue);
        }();

        if (!resolvedFeatureValue)
            return EvaluationResult::False;

        // Guaranteed-invalid values match guaranteed-invalid values.
        if (resolvedFeatureValue->isGuaranteedInvalid())
            return toEvaluationResult(!customPropertyValue || customPropertyValue->isGuaranteedInvalid());

        ASSERT(feature.rightComparison->op == ComparisonOperator::Equal);
        return toEvaluationResult(customPropertyValue && customPropertyValue->valueEquals(*resolvedFeatureValue));
    }

    // https://drafts.csswg.org/css-conditional-5/#ref-for-typedef-style-range
    EvaluationResult evaluateRange(const MQ::Feature& feature, const FeatureEvaluationContext& context, const Style::ComputedStyle& style) const
    {
        // A builder is only needed to resolve var()/attr() substitutions and css-wide keywords against
        // the container. It is created lazily so literal-only ranges like style(10px < 10em) skip it.
        std::unique_ptr<Style::ComputedStyle> dummyStyle;
        RefPtr<Style::MatchResult> dummyMatchResult;
        std::optional<Style::Builder> styleBuilder;
        auto ensureBuilder = [&]() -> Style::Builder& {
            if (!styleBuilder) {
                auto builderContext = Style::BuilderContext {
                    context.document.get(),
                    context.conversionData.parentStyle(),
                    context.conversionData.rootStyle(),
                    context.conversionData.elementForContainerUnitResolution()
                };
                dummyStyle = Style::ComputedStyle::clonePtr(style);
                dummyMatchResult = Style::MatchResult::create();
                styleBuilder.emplace(protect(*dummyStyle), WTF::move(builderContext), *dummyMatchResult);
            }
            return *styleBuilder;
        };

        auto resolveValue = [&](const CSSCustomPropertyValue& value) -> RefPtr<const Style::CustomProperty> {
            // A bare <custom-property-name> is substituted as if wrapped in var(): read it from the container.
            if (auto name = MQ::bareCustomPropertyName(value.tokens().span()); !name.isNull())
                return style.customPropertyValue(name);
            // Plain values (literals, calc()) are already final; only var()/attr()/css-wide keywords need resolution.
            if (auto* data = std::get_if<Ref<CSSVariableData>>(&value.value()))
                return Style::CustomProperty::createForVariableData(value.name(), data->copyRef());
            return ensureBuilder().resolveCustomPropertyForContainerQueries(value);
        };

        auto resolveOperand = [&](const std::optional<Value>& operandValue, const AtomString& centerName) -> std::optional<StyleRangeValue> {
            RefPtr<const Style::CustomProperty> customProperty;
            if (!centerName.isNull())
                customProperty = style.customPropertyValue(centerName);
            else {
                if (!operandValue)
                    return { };
                auto* value = std::get_if<Ref<CSSCustomPropertyValue>>(&*operandValue);
                if (!value)
                    return { };
                customProperty = resolveValue(value->get());
            }
            if (!customProperty || customProperty->isGuaranteedInvalid())
                return { };
            return evaluateStyleRangeValue(customProperty->tokens(), context, style);
        };

        auto center = resolveOperand(feature.subject, feature.name);
        if (!center)
            return EvaluationResult::False;

        std::optional<StyleRangeValue> leftValue;
        if (feature.leftComparison) {
            leftValue = resolveOperand(feature.leftComparison->value, nullAtom());
            if (!leftValue)
                return EvaluationResult::False;
        }

        std::optional<StyleRangeValue> rightValue;
        if (feature.rightComparison) {
            rightValue = resolveOperand(feature.rightComparison->value, nullAtom());
            if (!rightValue)
                return EvaluationResult::False;
        }

        if (!styleRangeValuesShareCategory({ &center, &leftValue, &rightValue }))
            return EvaluationResult::False;

        if (feature.leftComparison && !compare(feature.leftComparison->op, leftValue->value, center->value))
            return EvaluationResult::False;
        if (feature.rightComparison && !compare(feature.rightComparison->op, center->value, rightValue->value))
            return EvaluationResult::False;

        return EvaluationResult::True;
    }
};

// Scroll-state query features: scroll-state(scrollable | scrolled | stuck | snapped [: <keyword>]).
// https://drafts.csswg.org/css-conditional-5/#scroll-state-container
struct ScrollStateFeatureSchema : public FeatureSchema {
    ScrollStateFeatureSchema(const AtomString& name, FixedVector<CSSValueID>&& valueIdentifiers)
        : FeatureSchema(name, FeatureSchema::Type::Discrete, FeatureSchema::ValueType::Identifier, { }, WTF::move(valueIdentifiers))
    {
    }

    EvaluationResult evaluate(const MQ::Feature&, const FeatureEvaluationContext&) const override
    {
        // FIXME: Evaluate the actual scroll state. For now the feature is recognized and
        // evaluates to false (no active scroll state); real evaluation is a follow-up.
        return EvaluationResult::False;
    }
};

// MARK: - Singleton readonly instances of FeatureSchemas

static const WidthFeatureSchema& widthFeatureSchema()
{
    static MainThreadNeverDestroyed<WidthFeatureSchema> schema;
    return schema;
}

static const HeightFeatureSchema& heightFeatureSchema()
{
    static MainThreadNeverDestroyed<HeightFeatureSchema> schema;
    return schema;
}

static const InlineSizeFeatureSchema& inlineSizeFeatureSchema()
{
    static MainThreadNeverDestroyed<InlineSizeFeatureSchema> schema;
    return schema;
}

static const BlockSizeFeatureSchema& blockSizeFeatureSchema()
{
    static MainThreadNeverDestroyed<BlockSizeFeatureSchema> schema;
    return schema;
}

static const AspectRatioFeatureSchema& aspectRatioFeatureSchema()
{
    static MainThreadNeverDestroyed<AspectRatioFeatureSchema> schema;
    return schema;
}

static const OrientationFeatureSchema& orientationFeatureSchema()
{
    static MainThreadNeverDestroyed<OrientationFeatureSchema> schema;
    return schema;
}

static const StyleFeatureSchema& styleFeatureSchema()
{
    static MainThreadNeverDestroyed<StyleFeatureSchema> schema;
    return schema;
}

static const ScrollStateFeatureSchema& scrollableFeatureSchema()
{
    static MainThreadNeverDestroyed<ScrollStateFeatureSchema> schema { "scrollable"_s, FixedVector<CSSValueID> { CSSValueNone, CSSValueTop, CSSValueRight, CSSValueBottom, CSSValueLeft, CSSValueBlockStart, CSSValueBlockEnd, CSSValueInlineStart, CSSValueInlineEnd, CSSValueBlock, CSSValueInline, CSSValueX, CSSValueY } };
    return schema;
}

static const ScrollStateFeatureSchema& scrolledFeatureSchema()
{
    static MainThreadNeverDestroyed<ScrollStateFeatureSchema> schema { "scrolled"_s, FixedVector<CSSValueID> { CSSValueNone, CSSValueTop, CSSValueRight, CSSValueBottom, CSSValueLeft, CSSValueBlockStart, CSSValueBlockEnd, CSSValueInlineStart, CSSValueInlineEnd, CSSValueBlock, CSSValueInline, CSSValueX, CSSValueY } };
    return schema;
}

static const ScrollStateFeatureSchema& stuckFeatureSchema()
{
    static MainThreadNeverDestroyed<ScrollStateFeatureSchema> schema { "stuck"_s, FixedVector<CSSValueID> { CSSValueNone, CSSValueTop, CSSValueRight, CSSValueBottom, CSSValueLeft, CSSValueBlockStart, CSSValueBlockEnd, CSSValueInlineStart, CSSValueInlineEnd } };
    return schema;
}

static const ScrollStateFeatureSchema& snappedFeatureSchema()
{
    static MainThreadNeverDestroyed<ScrollStateFeatureSchema> schema { "snapped"_s, FixedVector<CSSValueID> { CSSValueNone, CSSValueX, CSSValueY, CSSValueBlock, CSSValueInline, CSSValueBoth } };
    return schema;
}

// MARK: - Type erased exposed schemas

const MQ::FeatureSchema& width()
{
    return widthFeatureSchema();
}

const MQ::FeatureSchema& height()
{
    return heightFeatureSchema();
}

const MQ::FeatureSchema& inlineSize()
{
    return inlineSizeFeatureSchema();
}

const MQ::FeatureSchema& blockSize()
{
    return blockSizeFeatureSchema();
}

const MQ::FeatureSchema& aspectRatio()
{
    return aspectRatioFeatureSchema();
}

const MQ::FeatureSchema& orientation()
{
    return orientationFeatureSchema();
}

const MQ::FeatureSchema& style()
{
    return styleFeatureSchema();
}

const MQ::FeatureSchema* scrollState(const AtomString& name)
{
    if (name == "scrollable"_s)
        return &scrollableFeatureSchema();
    if (name == "scrolled"_s)
        return &scrolledFeatureSchema();
    if (name == "stuck"_s)
        return &stuckFeatureSchema();
    if (name == "snapped"_s)
        return &snappedFeatureSchema();
    return nullptr;
}

bool isScrollStateFeature(const MQ::FeatureSchema* schema)
{
    return schema == &scrollableFeatureSchema()
        || schema == &scrolledFeatureSchema()
        || schema == &stuckFeatureSchema()
        || schema == &snappedFeatureSchema();
}

Vector<const MQ::FeatureSchema*> allSchemas()
{
    return {
        &Features::width(),
        &Features::height(),
        &Features::inlineSize(),
        &Features::blockSize(),
        &Features::aspectRatio(),
        &Features::orientation(),
    };
}

} // namespace Features
} // namespace WebCore::CQ
