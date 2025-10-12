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
#include "CalculationCategory.h"
#include "ComputedStyleDependencies.h"
#include "ContainerQueryEvaluator.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "StyleBuilder.h"
#include "StyleCustomProperty.h"
#include "StyleCustomPropertyRegistry.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore::CQ {

using namespace MQ;

struct SizeFeatureSchema : public FeatureSchema {
    SizeFeatureSchema(const AtomString& name, Type type, ValueType valueType, OptionSet<MediaQueryDynamicDependency> dependencies, FixedVector<CSSValueID>&& valueIdentifiers = { })
        : FeatureSchema(name, type, valueType, dependencies, WTFMove(valueIdentifiers))
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
        return evaluateLengthFeature(feature, renderer.contentBoxWidth(), conversionData);
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
        return evaluateLengthFeature(feature, renderer.contentBoxHeight(), conversionData);
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
        return evaluateLengthFeature(feature, renderer.contentBoxLogicalWidth(), conversionData);
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
        return evaluateLengthFeature(feature, renderer.contentBoxLogicalHeight(), conversionData);
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

struct StyleFeatureSchema : public FeatureSchema {
    StyleFeatureSchema()
        : FeatureSchema("style"_s, FeatureSchema::Type::Discrete, FeatureSchema::ValueType::CustomProperty, { })
    {
    }

    // FeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const FeatureEvaluationContext& context) const override
    {
        if (!context.conversionData.style() || !context.conversionData.parentStyle())
            return EvaluationResult::False;

        auto& style = *context.conversionData.style();

        auto* customPropertyValue = style.customPropertyValue(feature.name);
        if (!feature.rightComparison)
            return toEvaluationResult(customPropertyValue && !customPropertyValue->isGuaranteedInvalid());

        auto resolvedFeatureValue = [&]() -> RefPtr<const Style::CustomProperty> {
            auto featureValue = dynamicDowncast<CSSCustomPropertyValue>(feature.rightComparison->value);
            ASSERT(featureValue);

            // Resolve the queried custom property value for var() references, css-wide keywords and registered properties.
            auto builderContext = Style::BuilderContext {
                context.document.get(),
                context.conversionData.parentStyle(),
                context.conversionData.rootStyle(),
                context.conversionData.elementForContainerUnitResolution()
            };

            auto dummyStyle = RenderStyle::clone(style);
            auto dummyMatchResult = Style::MatchResult::create();

            auto styleBuilder = Style::Builder { dummyStyle, WTFMove(builderContext), dummyMatchResult };
            return styleBuilder.resolveCustomPropertyForContainerQueries(*featureValue);
        }();

        if (!resolvedFeatureValue)
            return EvaluationResult::False;

        // Guaranteed-invalid values match guaranteed-invalid values.
        if (resolvedFeatureValue->isGuaranteedInvalid())
            return toEvaluationResult(!customPropertyValue || customPropertyValue->isGuaranteedInvalid());

        ASSERT(feature.rightComparison->op == ComparisonOperator::Equal);
        return toEvaluationResult(customPropertyValue && *customPropertyValue == *resolvedFeatureValue);
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
