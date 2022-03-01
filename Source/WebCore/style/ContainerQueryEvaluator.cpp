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
#include "ComposedTreeAncestorIterator.h"
#include "Document.h"
#include "MediaFeatureNames.h"
#include "MediaList.h"
#include "MediaQuery.h"
#include "RenderView.h"
#include "StyleRule.h"

namespace WebCore::Style {

struct ContainerQueryEvaluator::ResolvedContainer {
    const RenderBox* renderer { nullptr };
    CSSToLengthConversionData conversionData;
};

ContainerQueryEvaluator::ContainerQueryEvaluator(const Element& element, PseudoId pseudoId, SelectorMatchingState* selectorMatchingState)
    : m_element(element)
    , m_pseudoId(pseudoId)
    , m_selectorMatchingState(selectorMatchingState)
{
}

bool ContainerQueryEvaluator::evaluate(const FilteredContainerQuery& filteredContainerQuery) const
{
    auto container = resolveContainer(filteredContainerQuery);
    if (!container)
        return false;

    return evaluateQuery(filteredContainerQuery.query, *container) == EvaluationResult::True;
}

auto ContainerQueryEvaluator::resolveContainer(const FilteredContainerQuery& filteredContainerQuery) const -> std::optional<ResolvedContainer>
{
    auto makeResolvedContainer = [](const Element& element) -> ResolvedContainer {
        auto* renderer = dynamicDowncast<RenderBox>(element.renderer());
        if (!renderer)
            return { };
        auto& view = renderer->view();
        return ResolvedContainer {
            renderer,
            CSSToLengthConversionData { &renderer->style(), &view.style(), nullptr, &view, 1 }
        };
    };

    auto isContainerForQuery = [&](const Element& element) {
        auto* style = element.existingComputedStyle();
        if (!style)
            return false;
        if (style->containerType() == ContainerType::None)
            return false;
        if (filteredContainerQuery.nameFilter.isEmpty())
            return true;
        return style->containerNames().contains(filteredContainerQuery.nameFilter);
    };

    if (m_selectorMatchingState) {
        for (auto& container : makeReversedRange(m_selectorMatchingState->queryContainers)) {
            if (isContainerForQuery(container))
                return makeResolvedContainer(container);
        }
        return { };
    }

    if (m_pseudoId != PseudoId::None) {
        if (isContainerForQuery(m_element))
            return makeResolvedContainer(m_element);
    }

    for (auto& ancestor : composedTreeAncestors(const_cast<Element&>(m_element.get()))) {
        if (isContainerForQuery(ancestor))
            return makeResolvedContainer(ancestor);
    }
    return { };
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
    case CQ::LogicalOperator::Not:
        return !evaluateQuery(condition.queries.first(), container);
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
    RELEASE_ASSERT_NOT_REACHED();
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
    // "If the query container does not have a principal box ... then the result of evaluating the size feature is unknown."
    // https://drafts.csswg.org/css-contain-3/#size-container
    if (!container.renderer)
        return EvaluationResult::Unknown;

    auto& renderer = *container.renderer;

    auto compare = [](CQ::ComparisonOperator op, auto left, auto right) {
        switch (op) {
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
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    enum class Side : uint8_t { Left, Right };
    auto evaluateSizeComparison = [&](LayoutUnit size, const std::optional<CQ::Comparison>& comparison, Side side) {
        if (!comparison)
            return EvaluationResult::True;
        auto expressionSize = computeSize(comparison->value.get(), container.conversionData);
        if (!expressionSize)
            return EvaluationResult::Unknown;
        auto left = side == Side::Left ? *expressionSize : size;
        auto right = side == Side::Left ? size : *expressionSize;

        return toEvaluationResult(compare(comparison->op, left, right));
    };

    auto evaluateSize = [&](LayoutUnit size) {
        if (!sizeFeature.leftComparison && !sizeFeature.rightComparison)
            return toEvaluationResult(!!size);

        auto leftResult = evaluateSizeComparison(size, sizeFeature.leftComparison, Side::Left);
        auto rightResult = evaluateSizeComparison(size, sizeFeature.rightComparison, Side::Right);

        return leftResult & rightResult;
    };

    auto evaluateAspectRatioComparison = [&](double aspectRatio, const std::optional<CQ::Comparison>& comparison, Side side) {
        if (!comparison)
            return EvaluationResult::True;

        if (!is<CSSValueList>(comparison->value))
            return EvaluationResult::Unknown;

        auto& ratioList = downcast<CSSValueList>(*comparison->value);
        if (ratioList.length() != 2)
            return EvaluationResult::Unknown;

        auto first = dynamicDowncast<CSSPrimitiveValue>(ratioList.item(0));
        auto second = dynamicDowncast<CSSPrimitiveValue>(ratioList.item(1));

        if (!first || !second || !first->isNumberOrInteger() || !second->isNumberOrInteger())
            return EvaluationResult::Unknown;

        auto expressionRatio = first->doubleValue() / second->doubleValue();

        auto left = side == Side::Left ? expressionRatio : aspectRatio;
        auto right = side == Side::Left ? aspectRatio : expressionRatio;

        return toEvaluationResult(compare(comparison->op, left, right));
    };

    enum class Axis : uint8_t { Both, Block, Inline, Width, Height };
    auto containerSupportsRequiredAxis = [&](Axis axis) {
        switch (renderer.style().containerType()) {
        case ContainerType::Size:
            return true;
        case ContainerType::InlineSize:
            if (axis == Axis::Width)
                return renderer.isHorizontalWritingMode();
            if (axis == Axis::Height)
                return !renderer.isHorizontalWritingMode();
            return axis == Axis::Inline;
        case ContainerType::None:
            RELEASE_ASSERT_NOT_REACHED();
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    if (sizeFeature.name == CQ::FeatureNames::width()) {
        if (!containerSupportsRequiredAxis(Axis::Width))
            return EvaluationResult::Unknown;

        return evaluateSize(renderer.contentWidth());
    }

    if (sizeFeature.name == CQ::FeatureNames::height()) {
        if (!containerSupportsRequiredAxis(Axis::Height))
            return EvaluationResult::Unknown;

        return evaluateSize(renderer.contentHeight());
    }

    if (sizeFeature.name == CQ::FeatureNames::inlineSize()) {
        if (!containerSupportsRequiredAxis(Axis::Inline))
            return EvaluationResult::Unknown;

        return evaluateSize(renderer.contentLogicalWidth());
    }

    if (sizeFeature.name == CQ::FeatureNames::blockSize()) {
        if (!containerSupportsRequiredAxis(Axis::Block))
            return EvaluationResult::Unknown;

        return evaluateSize(renderer.contentLogicalHeight());
    }

    if (sizeFeature.name == CQ::FeatureNames::aspectRatio()) {
        if (!containerSupportsRequiredAxis(Axis::Both))
            return EvaluationResult::Unknown;

        auto boxRatio = renderer.contentWidth().toDouble() / renderer.contentHeight().toDouble();
        
        if (!sizeFeature.leftComparison && !sizeFeature.rightComparison)
            return toEvaluationResult(!!boxRatio);

        auto leftResult = evaluateAspectRatioComparison(boxRatio, sizeFeature.leftComparison, Side::Left);
        auto rightResult = evaluateAspectRatioComparison(boxRatio, sizeFeature.rightComparison, Side::Right);

        return leftResult & rightResult;
    }

    if (sizeFeature.name == CQ::FeatureNames::orientation()) {
        if (!containerSupportsRequiredAxis(Axis::Both))
            return EvaluationResult::Unknown;

        if (!sizeFeature.rightComparison)
            return EvaluationResult::Unknown;

        auto& comparison = *sizeFeature.rightComparison;

        if (!is<CSSPrimitiveValue>(comparison.value) || comparison.op != CQ::ComparisonOperator::Equal)
            return EvaluationResult::Unknown;

        auto& value = downcast<CSSPrimitiveValue>(*sizeFeature.rightComparison->value);

        bool isPortrait = renderer.contentHeight() >= renderer.contentWidth();
        if (value.valueID() == CSSValuePortrait)
            return toEvaluationResult(isPortrait);
        if (value.valueID() == CSSValueLandscape)
            return toEvaluationResult(!isPortrait);

        return EvaluationResult::Unknown;
    }

    return EvaluationResult::Unknown;
}

}
