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

#include "ContainerQueryEvaluator.h"
#include "RenderBox.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore::CQ::Features {

using namespace MQ;

struct SizeFeatureSchema : public FeatureSchema {
    SizeFeatureSchema(const AtomString& name, Type type, ValueType valueType, Vector<CSSValueID>&& valueIdentifiers = { })
        : FeatureSchema(name, type, valueType, WTFMove(valueIdentifiers))
    { }

    EvaluationResult evaluate(const MQ::Feature& feature, const FeatureEvaluationContext& context) const override
    {
        // "If the query container does not have a principal box, or the principal box is not a layout containment box,
        // or the query container does not support container size queries on the relevant axes, then the result of
        // evaluating the size feature is unknown."
        // https://drafts.csswg.org/css-contain-3/#size-container
        if (!is<RenderBox>(context.renderer))
            return MQ::EvaluationResult::Unknown;

        auto& renderer = downcast<RenderBox>(*context.renderer);

        auto hasEligibleContainment = [&] {
            if (!renderer.shouldApplyLayoutContainment())
                return false;
            switch (renderer.style().containerType()) {
            case ContainerType::InlineSize:
                return renderer.shouldApplyInlineSizeContainment();
            case ContainerType::Size:
                return renderer.shouldApplySizeContainment();
            case ContainerType::Normal:
                return true;
            }
            RELEASE_ASSERT_NOT_REACHED();
        };

        if (!hasEligibleContainment())
            return MQ::EvaluationResult::Unknown;

        return evaluate(feature, renderer, context.conversionData);
    }

    virtual EvaluationResult evaluate(const MQ::Feature&, const RenderBox&, const CSSToLengthConversionData&) const = 0;
};

const FeatureSchema& width()
{
    struct Schema : public SizeFeatureSchema {
        Schema()
            : SizeFeatureSchema("width"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length)
        { }

        EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
        {
            return evaluateLengthFeature(feature, renderer.contentWidth(), conversionData);
        }
    };

    static MainThreadNeverDestroyed<Schema> schema;
    return schema;
}

const FeatureSchema& height()
{
    struct Schema : public SizeFeatureSchema {
        Schema()
            : SizeFeatureSchema("height"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length)
        { }

        EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
        {
            return evaluateLengthFeature(feature, renderer.contentHeight(), conversionData);
        }
    };

    static MainThreadNeverDestroyed<Schema> schema;
    return schema;
}

const FeatureSchema& inlineSize()
{
    struct Schema : public SizeFeatureSchema {
        Schema()
            : SizeFeatureSchema("inline-size"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length)
        { }

        EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
        {
            return evaluateLengthFeature(feature, renderer.contentLogicalWidth(), conversionData);
        }
    };

    static MainThreadNeverDestroyed<Schema> schema;
    return schema;
}

const FeatureSchema& blockSize()
{
    struct Schema : public SizeFeatureSchema {
        Schema()
            : SizeFeatureSchema("block-size"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length)
        { }

        EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData& conversionData) const override
        {
            return evaluateLengthFeature(feature, renderer.contentLogicalHeight(), conversionData);
        }
    };

    static MainThreadNeverDestroyed<Schema> schema;
    return schema;
}

const FeatureSchema& aspectRatio()
{
    struct Schema : public SizeFeatureSchema {
        Schema()
            : SizeFeatureSchema("aspect-ratio"_s, FeatureSchema::Type::Range, FeatureSchema::ValueType::Ratio)
        { }

        EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData&) const override
        {
            auto boxRatio = renderer.contentWidth().toDouble() / renderer.contentHeight().toDouble();
            return evaluateRatioFeature(feature, boxRatio);
        }
    };

    static MainThreadNeverDestroyed<Schema> schema;
    return schema;
}

const FeatureSchema& orientation()
{
    struct Schema : public SizeFeatureSchema {
        Schema()
            : SizeFeatureSchema("orientation"_s, FeatureSchema::Type::Discrete, FeatureSchema::ValueType::Identifier, { CSSValuePortrait, CSSValueLandscape })
        { }

        EvaluationResult evaluate(const MQ::Feature& feature, const RenderBox& renderer, const CSSToLengthConversionData&) const override
        {
            bool isPortrait = renderer.contentHeight() >= renderer.contentWidth();
            return evaluateIdentifierFeature(feature, isPortrait ? CSSValuePortrait : CSSValueLandscape);
        }
    };

    static MainThreadNeverDestroyed<Schema> schema;
    return schema;
}

}
