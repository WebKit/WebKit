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
#include "CalculationCategory.h"
#include "ComputedStyleDependencies.h"
#include "ContainerQueryEvaluator.h"
#include "CustomPropertyRegistry.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "StyleBuilder.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore::CQ {

using namespace MQ;

ContainerProgressProviding::~ContainerProgressProviding() = default;

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

static double lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis logicalAxis, const FloatSize& size, const RenderStyle& style)
{
    switch (mapAxisLogicalToPhysical(style.writingMode(), logicalAxis)) {
    case BoxAxis::Horizontal:
        return size.width();

    case BoxAxis::Vertical:
        return size.height();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

struct WidthFeatureSchema : public SizeFeatureSchema, public ContainerProgressProviding {
    WidthFeatureSchema()
        : SizeFeatureSchema("width"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length, MediaQueryDynamicDependency::Viewport)
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        return evaluateLengthFeature(feature, renderer.contentWidth(), conversionData);
    }


    // ContainerProgressProviding conformance

    AtomString name() const override
    {
        return static_cast<const FeatureSchema*>(this)->name;
    }

    Calculation::Category category() const override
    {
        return Calculation::Category::Length;
    }

    void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const override
    {
        dependencies.containerDimensions = true;
        dependencies.viewportDimensions = true;
    }

    double valueInCanonicalUnits(const RenderBox& renderer) const override
    {
        return renderer.contentWidth();
    }

    double valueInCanonicalUnits(const RenderView& view, const RenderStyle&) const override
    {
        return view.sizeForCSSSmallViewportUnits().width();
    }
};

struct HeightFeatureSchema : public SizeFeatureSchema, public ContainerProgressProviding {
    HeightFeatureSchema()
        : SizeFeatureSchema("height"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length, MediaQueryDynamicDependency::Viewport)
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        return evaluateLengthFeature(feature, renderer.contentHeight(), conversionData);
    }

    // ContainerProgressProviding conformance

    AtomString name() const override
    {
        return static_cast<const FeatureSchema*>(this)->name;
    }

    Calculation::Category category() const override
    {
        return Calculation::Category::Length;
    }

    void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const override
    {
        dependencies.containerDimensions = true;
        dependencies.viewportDimensions = true;
    }

    double valueInCanonicalUnits(const RenderBox& renderer) const override
    {
        return renderer.contentHeight();
    }

    double valueInCanonicalUnits(const RenderView& view, const RenderStyle&) const override
    {
        return view.sizeForCSSSmallViewportUnits().height();
    }
};

struct InlineSizeFeatureSchema : public SizeFeatureSchema, public ContainerProgressProviding {
    InlineSizeFeatureSchema()
        : SizeFeatureSchema("inline-size"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length, MediaQueryDynamicDependency::Viewport)
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        return evaluateLengthFeature(feature, renderer.contentLogicalWidth(), conversionData);
    }

    // ContainerProgressProviding conformance

    AtomString name() const override
    {
        return static_cast<const FeatureSchema*>(this)->name;
    }

    Calculation::Category category() const override
    {
        return Calculation::Category::Length;
    }

    void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const override
    {
        dependencies.containerDimensions = true;
        dependencies.viewportDimensions = true;
    }

    double valueInCanonicalUnits(const RenderBox& renderer) const override
    {
        return renderer.contentLogicalWidth();
    }

    double valueInCanonicalUnits(const RenderView& view, const RenderStyle& style) const override
    {
        return lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, view.sizeForCSSSmallViewportUnits(), style);
    }
};

struct BlockSizeFeatureSchema : public SizeFeatureSchema, public ContainerProgressProviding {
    BlockSizeFeatureSchema()
        : SizeFeatureSchema("block-size"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length, MediaQueryDynamicDependency::Viewport)
    {
    }

    // SizeFeatureSchema conformance

    EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
    {
        return evaluateLengthFeature(feature, renderer.contentLogicalHeight(), conversionData);
    }

    // ContainerProgressProviding conformance

    AtomString name() const override
    {
        return static_cast<const FeatureSchema*>(this)->name;
    }

    Calculation::Category category() const override
    {
        return Calculation::Category::Length;
    }

    void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const override
    {
        dependencies.containerDimensions = true;
        dependencies.viewportDimensions = true;
    }

    double valueInCanonicalUnits(const RenderBox& renderer) const override
    {
        return renderer.contentLogicalHeight();
    }

    double valueInCanonicalUnits(const RenderView& view, const RenderStyle& style) const override
    {
        return lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, view.sizeForCSSSmallViewportUnits(), style);
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
        return evaluateRatioFeature(feature, renderer.contentSize(), conversionData);
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
        bool isPortrait = renderer.contentHeight() >= renderer.contentWidth();
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
            return toEvaluationResult(customPropertyValue && !customPropertyValue->isInvalid());

        auto resolvedFeatureValue = [&]() -> RefPtr<const CSSCustomPropertyValue> {
            auto featureValue = dynamicDowncast<CSSCustomPropertyValue>(feature.rightComparison->value);
            ASSERT(featureValue);

            // Resolve the queried custom property value for var() references, css-wide keywords and registered properties.
            auto builderContext = Style::BuilderContext {
                context.document.get(),
                *context.conversionData.parentStyle(),
                context.conversionData.rootStyle(),
                context.conversionData.elementForContainerUnitResolution()
            };

            auto dummyStyle = RenderStyle::clone(style);

            auto styleBuilder = Style::Builder { dummyStyle, WTFMove(builderContext), { }, { } };
            return styleBuilder.resolveCustomPropertyForContainerQueries(*featureValue);
        }();

        if (!resolvedFeatureValue)
            return EvaluationResult::False;

        // Guaranteed-invalid values match guaranteed-invalid values.
        if (resolvedFeatureValue->isInvalid())
            return toEvaluationResult(!customPropertyValue || customPropertyValue->isInvalid());

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

Vector<const ContainerProgressProviding*> allContainerProgressProvidingSchemas()
{
    return {
        &Features::widthFeatureSchema(),
        &Features::heightFeatureSchema(),
        &Features::inlineSizeFeatureSchema(),
        &Features::blockSizeFeatureSchema(),
    };
}

} // namespace Features
} // namespace WebCore::CQ
