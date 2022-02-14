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

#include "CSSToLengthConversionData.h"
#include "Document.h"
#include "MediaFeatureNames.h"
#include "MediaList.h"
#include "MediaQuery.h"
#include "RenderView.h"
#include "StyleRule.h"

namespace WebCore::Style {

struct ContainerQueryEvaluator::EvaluationContext {
    RenderBox& renderer;
    CSSToLengthConversionData conversionData;
};

ContainerQueryEvaluator::ContainerQueryEvaluator(const Vector<Ref<const Element>>& containers)
    : m_containers(containers)
{
}

static std::optional<LayoutUnit> computeSize(const CSSPrimitiveValue& value, const CSSToLengthConversionData& conversionData)
{
    if (value.isNumberOrInteger()) {
        if (value.doubleValue())
            return { };
        return 0_lu;
    }

    if (!value.isLength())
        return { };
    return value.computeLength<LayoutUnit>(conversionData);
}

bool ContainerQueryEvaluator::evaluate(const FilteredContainerQuery& filteredContainerQuery) const
{
    if (m_containers.isEmpty())
        return false;

    auto containerRendererForFilter = [&]() -> RenderBox* {
        for (auto& container : makeReversedRange(m_containers)) {
            auto* renderer = dynamicDowncast<RenderBox>(container->renderer());
            if (!renderer)
                return nullptr;
            if (filteredContainerQuery.nameFilter.isEmpty())
                return renderer;
            // FIXME: Support type filter.
            if (renderer->style().containerNames().contains(filteredContainerQuery.nameFilter))
                return renderer;
        }
        return nullptr;
    };

    auto* renderer = containerRendererForFilter();
    if (!renderer)
        return false;

    auto& view = renderer->view();
    CSSToLengthConversionData { &renderer->style(), &view.style(), nullptr, &view, 1 };

    EvaluationContext evaluationContext {
        *renderer,
        CSSToLengthConversionData { &renderer->style(), &view.style(), nullptr, &view, 1 }
    };

    return evaluateQuery(filteredContainerQuery.query, evaluationContext) == EvaluationResult::True;
}

auto ContainerQueryEvaluator::evaluateQuery(const CQ::ContainerQuery& containerQuery, const EvaluationContext& context) const -> EvaluationResult
{
    return WTF::switchOn(containerQuery, [&](const CQ::ContainerCondition& containerCondition) {
        return evaluateCondition(containerCondition, context);
    }, [&](const CQ::SizeQuery& sizeQuery) {
        return evaluateQuery(sizeQuery, context);
    }, [&](const CQ::UnknownQuery&) {
        return EvaluationResult::Unknown;
    });
}

auto ContainerQueryEvaluator::evaluateQuery(const CQ::SizeQuery& sizeQuery, const EvaluationContext& context) const -> EvaluationResult
{
    return WTF::switchOn(sizeQuery, [&](const CQ::SizeCondition& sizeCondition) {
        return evaluateCondition(sizeCondition, context);
    }, [&](const CQ::SizeFeature& sizeFeature) {
        return evaluateSizeFeature(sizeFeature, context);
    });
}

template<typename ConditionType>
auto ContainerQueryEvaluator::evaluateCondition(const ConditionType& condition, const EvaluationContext& context) const -> EvaluationResult
{
    if (condition.queries.isEmpty())
        return EvaluationResult::Unknown;

    switch (condition.logicalOperator) {
    case CQ::LogicalOperator::Not: {
        switch (evaluateQuery(condition.queries.first(), context)) {
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
            auto queryResult = evaluateQuery(query, context);
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
            auto queryResult = evaluateQuery(query, context);
            if (queryResult == EvaluationResult::True)
                return EvaluationResult::True;
            if (queryResult == EvaluationResult::Unknown)
                result = EvaluationResult::Unknown;
        }
        return result;
    }
    }
}

auto ContainerQueryEvaluator::evaluateSizeFeature(const CQ::SizeFeature& sizeFeature, const EvaluationContext& context) const -> EvaluationResult
{
    auto evaluateSize = [&](LayoutUnit size) {
        std::optional<LayoutUnit> expressionSize;

        if (sizeFeature.comparisonOperator != CQ::ComparisonOperator::True) {
            if (!sizeFeature.value)
                return false;
            expressionSize = computeSize(*sizeFeature.value, context.conversionData);
            if (!expressionSize)
                return false;
        }

        switch (sizeFeature.comparisonOperator) {
        case CQ::ComparisonOperator::LessThan:
            return size < *expressionSize;
        case CQ::ComparisonOperator::GreaterThan:
            return size > *expressionSize;
        case CQ::ComparisonOperator::LessThanOrEqual:
            return size <= *expressionSize;
        case CQ::ComparisonOperator::GreaterThanOrEqual:
            return size >= *expressionSize;
        case CQ::ComparisonOperator::Equal:
            return size == *expressionSize;
        case CQ::ComparisonOperator::True:
            return !!size;
        }
    };

    // FIXME: Support all features.
    if (sizeFeature.name == MediaFeatureNames::width)
        return evaluateSize(context.renderer.width()) ? EvaluationResult::True : EvaluationResult::False;
    if (sizeFeature.name == MediaFeatureNames::height)
        return evaluateSize(context.renderer.height()) ? EvaluationResult::True : EvaluationResult::False;

    return EvaluationResult::Unknown;
}

}
