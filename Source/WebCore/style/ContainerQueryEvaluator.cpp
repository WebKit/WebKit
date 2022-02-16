/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContainerQueryEvaluator.h"

#include "CSSPrimitiveValue.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueList.h"
#include "Document.h"
#include "MediaFeatureNames.h"
#include "MediaList.h"
#include "MediaQuery.h"
#include "RenderView.h"
#include "StyleRule.h"

namespace WebCore::Style {

struct ContainerQueryEvaluator::ResolvedContainer {
    const RenderBox& renderer;
    CSSToLengthConversionData conversionData;
};

ContainerQueryEvaluator::ContainerQueryEvaluator(const Vector<Ref<const Element>>& containers)
    : m_containers(containers)
{
}

bool ContainerQueryEvaluator::evaluate(const FilteredContainerQuery& filteredContainerQuery) const
{
    if (m_containers.isEmpty())
        return false;

    auto makeResolvedContainer = [&](const RenderBox& renderer) -> ResolvedContainer {
        auto& view = renderer.view();
        return {
            renderer,
            CSSToLengthConversionData { &renderer.style(), &view.style(), nullptr, &view, 1 }
        };
    };

    auto resolveContainer = [&]() -> std::optional<ResolvedContainer> {
        for (auto& container : makeReversedRange(m_containers)) {
            auto* renderer = dynamicDowncast<RenderBox>(container->renderer());
            if (!renderer)
                return { };
            if (filteredContainerQuery.nameFilter.isEmpty())
                return makeResolvedContainer(*renderer);
            // FIXME: Support type filter.
            if (renderer->style().containerNames().contains(filteredContainerQuery.nameFilter))
                return makeResolvedContainer(*renderer);
        }
        return { };
    };

    auto container = resolveContainer();
    if (!container)
        return { };

    return evaluateQuery(filteredContainerQuery.query, *container) == EvaluationResult::True;
}

auto ContainerQueryEvaluator::evaluateQuery(const CQ::ContainerQuery& containerQuery, const ResolvedContainer& container) const -> EvaluationResult
{
    return WTF::switchOn(containerQuery, [&](const CQ::ContainerCondition& containerCondition) {
        return evaluateCondition(containerCondition, container);
    }, [&](const CQ::SizeQuery& sizeQuery) {
        return evaluateQuery(sizeQuery, container);
    }, [&](const CQ::UnknownQuery&) {
        return EvaluationResult::Unknown;
    });
}

auto ContainerQueryEvaluator::evaluateQuery(const CQ::SizeQuery& sizeQuery, const ResolvedContainer& container) const -> EvaluationResult
{
    return WTF::switchOn(sizeQuery, [&](const CQ::SizeCondition& sizeCondition) {
        return evaluateCondition(sizeCondition, container);
    }, [&](const CQ::SizeFeature& sizeFeature) {
        return evaluateSizeFeature(sizeFeature, container);
    });
}

template<typename ConditionType>
auto ContainerQueryEvaluator::evaluateCondition(const ConditionType& condition, const ResolvedContainer& container) const -> EvaluationResult
{
    if (condition.queries.isEmpty())
        return EvaluationResult::Unknown;

    switch (condition.logicalOperator) {
    case CQ::LogicalOperator::Not: {
        switch (evaluateQuery(condition.queries.first(), container)) {
        case EvaluationResult::True:
            return EvaluationResult::False;
        case EvaluationResult::False:
            return EvaluationResult::True;
        case EvaluationResult::Unknown:
            return EvaluationResult::Unknown;
        }
    }
    case CQ::LogicalOperator::And: {
        auto result = EvaluationResult::True;
        for (auto query : condition.queries) {
            auto queryResult = evaluateQuery(query, container);
            if (queryResult == EvaluationResult::False)
                return EvaluationResult::False;
            if (queryResult == EvaluationResult::Unknown)
                result = EvaluationResult::Unknown;
        }
        return result;
    }
    case CQ::LogicalOperator::Or: {
        auto result = EvaluationResult::False;
        for (auto query : condition.queries) {
            auto queryResult = evaluateQuery(query, container);
            if (queryResult == EvaluationResult::True)
                return EvaluationResult::True;
            if (queryResult == EvaluationResult::Unknown)
                result = EvaluationResult::Unknown;
        }
        return result;
    }
    }
}

static std::optional<LayoutUnit> computeSize(const CSSValue* value, const CSSToLengthConversionData& conversionData)
{
    if (!is<CSSPrimitiveValue>(value))
        return { };
    auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);

    if (primitiveValue.isNumberOrInteger()) {
        if (primitiveValue.doubleValue())
            return { };
        return 0_lu;
    }

    if (!primitiveValue.isLength())
        return { };
    return primitiveValue.computeLength<LayoutUnit>(conversionData);
}

auto ContainerQueryEvaluator::evaluateSizeFeature(const CQ::SizeFeature& sizeFeature, const ResolvedContainer& container) const -> EvaluationResult
{
    auto toEvaluationResult = [](bool boolean) {
        return boolean ? EvaluationResult::True : EvaluationResult::False;
    };

    auto compare = [&](auto left, auto right) {
        switch (sizeFeature.comparisonOperator) {
        case CQ::ComparisonOperator::LessThan:
            return left < right;
        case CQ::ComparisonOperator::GreaterThan:
            return left > right;
        case CQ::ComparisonOperator::LessThanOrEqual:
            return left <= right;
        case CQ::ComparisonOperator::GreaterThanOrEqual:
            return left >= right;
        case CQ::ComparisonOperator::Equal:
            return left == right;
        case CQ::ComparisonOperator::True:
            ASSERT_NOT_REACHED();
            return false;
        }
    };

    auto evaluateSize = [&](LayoutUnit size) {
        if (sizeFeature.comparisonOperator == CQ::ComparisonOperator::True)
            return toEvaluationResult(!!size);

        auto expressionSize = computeSize(sizeFeature.value.get(), container.conversionData);
        if (!expressionSize)
            return EvaluationResult::Unknown;

        return toEvaluationResult(compare(size, *expressionSize));
    };

    enum class Axis : uint8_t { Both, Block, Inline, Width, Height };
    auto containerSupportsRequiredAxis = [&](Axis axis) {
        switch (container.renderer.style().containerType()) {
        case ContainerType::Size:
            return true;
        case ContainerType::InlineSize:
            if (axis == Axis::Width)
                return container.renderer.isHorizontalWritingMode();
            if (axis == Axis::Height)
                return !container.renderer.isHorizontalWritingMode();
            return axis == Axis::Inline;
        case ContainerType::None:
            ASSERT_NOT_REACHED();
            return false;
        }
    };

    if (sizeFeature.name == CQ::FeatureNames::width()) {
        if (!containerSupportsRequiredAxis(Axis::Width))
            return EvaluationResult::Unknown;

        return evaluateSize(container.renderer.contentWidth());
    }

    if (sizeFeature.name == CQ::FeatureNames::height()) {
        if (!containerSupportsRequiredAxis(Axis::Height))
            return EvaluationResult::Unknown;

        return evaluateSize(container.renderer.contentHeight());
    }

    if (sizeFeature.name == CQ::FeatureNames::inlineSize()) {
        if (!containerSupportsRequiredAxis(Axis::Inline))
            return EvaluationResult::Unknown;

        return evaluateSize(container.renderer.contentLogicalWidth());
    }

    if (sizeFeature.name == CQ::FeatureNames::blockSize()) {
        if (!containerSupportsRequiredAxis(Axis::Block))
            return EvaluationResult::Unknown;

        return evaluateSize(container.renderer.contentLogicalHeight());
    }

    if (sizeFeature.name == CQ::FeatureNames::aspectRatio()) {
        if (!containerSupportsRequiredAxis(Axis::Both))
            return EvaluationResult::Unknown;

        auto boxRatio = container.renderer.contentWidth().toDouble() / container.renderer.contentHeight().toDouble();
        if (sizeFeature.comparisonOperator == CQ::ComparisonOperator::True)
            return toEvaluationResult(!!boxRatio);

        if (!is<CSSValueList>(sizeFeature.value))
            return EvaluationResult::Unknown;

        auto& ratioList = downcast<CSSValueList>(*sizeFeature.value);
        if (ratioList.length() != 2)
            return EvaluationResult::Unknown;

        auto first = dynamicDowncast<CSSPrimitiveValue>(ratioList.item(0));
        auto second = dynamicDowncast<CSSPrimitiveValue>(ratioList.item(1));

        if (!first || !second || !first->isNumberOrInteger() || !second->isNumberOrInteger())
            return EvaluationResult::Unknown;

        auto expressionRatio = first->doubleValue() / second->doubleValue();

        return toEvaluationResult(compare(boxRatio, expressionRatio));
    }

    if (sizeFeature.name == CQ::FeatureNames::orientation()) {
        if (!containerSupportsRequiredAxis(Axis::Both))
            return EvaluationResult::Unknown;

        if (!is<CSSPrimitiveValue>(sizeFeature.value) || sizeFeature.comparisonOperator != CQ::ComparisonOperator::Equal)
            return EvaluationResult::Unknown;

        auto& value = downcast<CSSPrimitiveValue>(*sizeFeature.value);

        bool isPortrait = container.renderer.contentHeight() >= container.renderer.contentWidth();
        if (value.valueID() == CSSValuePortrait)
            return toEvaluationResult(isPortrait);
        if (value.valueID() == CSSValueLandscape)
            return toEvaluationResult(!isPortrait);

        return EvaluationResult::Unknown;
    }

    return EvaluationResult::Unknown;
}

}
