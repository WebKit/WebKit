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
#include "NodeRenderStyle.h"
#include "RenderView.h"
#include "StyleRule.h"
#include "StyleScope.h"

namespace WebCore::Style {

struct ContainerQueryEvaluator::SelectedContainer {
    const RenderBox* renderer { nullptr };
    CSSToLengthConversionData conversionData;
};

ContainerQueryEvaluator::ContainerQueryEvaluator(const Element& element, SelectionMode selectionMode, ScopeOrdinal scopeOrdinal, SelectorMatchingState* selectorMatchingState)
    : m_element(element)
    , m_selectionMode(selectionMode)
    , m_scopeOrdinal(scopeOrdinal)
    , m_selectorMatchingState(selectorMatchingState)
{
}

bool ContainerQueryEvaluator::evaluate(const CQ::ContainerQuery& containerQuery) const
{
    auto container = selectContainer(containerQuery);
    if (!container)
        return false;

    return evaluateCondition(containerQuery.condition, *container) == MQ::EvaluationResult::True;
}

auto ContainerQueryEvaluator::selectContainer(const CQ::ContainerQuery& containerQuery) const -> std::optional<SelectedContainer>
{
    // "For each element, the query container to be queried is selected from among the element’s
    // ancestor query containers that have a valid container-type for all the container features
    // in the <container-condition>. The optional <container-name> filters the set of query containers
    // considered to just those with a matching query container name."
    // https://drafts.csswg.org/css-contain-3/#container-rule

    auto makeSelectedContainer = [](const Element& element) -> SelectedContainer {
        auto* renderer = dynamicDowncast<RenderBox>(element.renderer());
        if (!renderer)
            return { };
        return {
            renderer,
            CSSToLengthConversionData { renderer->style(), element.document().documentElement()->renderStyle(), nullptr, &renderer->view() }
        };
    };

    auto* cachedQueryContainers = m_selectorMatchingState ? &m_selectorMatchingState->queryContainers : nullptr;

    auto* container = selectContainer(containerQuery.axisFilter, containerQuery.name, m_element.get(), m_selectionMode, m_scopeOrdinal, cachedQueryContainers);
    if (!container)
        return { };

    return makeSelectedContainer(*container);
}

const Element* ContainerQueryEvaluator::selectContainer(OptionSet<CQ::Axis> axes, const String& name, const Element& element, SelectionMode selectionMode, ScopeOrdinal scopeOrdinal, const CachedQueryContainers* cachedQueryContainers)
{
    // "For each element, the query container to be queried is selected from among the element’s
    // ancestor query containers that have a valid container-type for all the container features
    // in the <container-condition>. The optional <container-name> filters the set of query containers
    // considered to just those with a matching query container name."
    // https://drafts.csswg.org/css-contain-3/#container-rule

    auto isValidContainerForRequiredAxes = [&](ContainerType containerType, const RenderElement* principalBox) {
        switch (containerType) {
        case ContainerType::Size:
            return true;
        case ContainerType::InlineSize:
            // Without a principal box the container matches but the query against it will evaluate to Unknown.
            if (!principalBox)
                return true;
            if (axes.contains(CQ::Axis::Block))
                return false;
            return !axes.contains(principalBox->isHorizontalWritingMode() ? CQ::Axis::Height : CQ::Axis::Width);
        case ContainerType::Normal:
            return false;
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto isContainerForQuery = [&](const Element& element) {
        auto* style = element.existingComputedStyle();
        if (!style)
            return false;
        if (!isValidContainerForRequiredAxes(style->containerType(), element.renderer()))
            return false;
        if (name.isEmpty())
            return true;
        return style->containerNames().contains(name);
    };

    auto findOriginatingElement = [&]() -> const Element* {
        // ::part() selectors can query its originating host, but not internal query containers inside the shadow tree.
        if (scopeOrdinal <= ScopeOrdinal::ContainingHost)
            return hostForScopeOrdinal(element, scopeOrdinal);
        // ::slotted() selectors can query containers inside the shadow tree, including the slot itself.
        if (scopeOrdinal >= ScopeOrdinal::FirstSlot && scopeOrdinal <= ScopeOrdinal::SlotLimit)
            return assignedSlotForScopeOrdinal(element, scopeOrdinal);
        return nullptr;
    };

    if (auto* originatingElement = findOriginatingElement()) {
        // For selectors with pseudo elements, query containers can be established by the shadow-including inclusive ancestors of the ultimate originating element.
        for (auto* ancestor = originatingElement; ancestor; ancestor = ancestor->parentOrShadowHostElement()) {
            if (isContainerForQuery(*ancestor))
                return ancestor;
        }
        return nullptr;
    }

    if (selectionMode == SelectionMode::PseudoElement) {
        if (isContainerForQuery(element))
            return &element;
    }

    if (cachedQueryContainers) {
        for (auto& container : makeReversedRange(*cachedQueryContainers)) {
            if (isContainerForQuery(container))
                return container.ptr();
        }
        return { };
    }

    for (auto* ancestor = element.parentOrShadowHostElement(); ancestor; ancestor = ancestor->parentOrShadowHostElement()) {
        if (isContainerForQuery(*ancestor))
            return ancestor;
    }
    return { };
}

auto ContainerQueryEvaluator::evaluateQueryInParens(const CQ::QueryInParens& queryInParens, const SelectedContainer& container) const -> MQ::EvaluationResult
{
    return WTF::switchOn(queryInParens, [&](const CQ::ContainerCondition& containerCondition) {
        return evaluateCondition(containerCondition, container);
    }, [&](const CQ::SizeFeature& sizeFeature) {
        return evaluateSizeFeature(sizeFeature, container);
    }, [&](const MQ::GeneralEnclosed&) {
        return MQ::EvaluationResult::Unknown;
    });
}

auto ContainerQueryEvaluator::evaluateSizeFeature(const CQ::SizeFeature& sizeFeature, const SelectedContainer& container) const -> MQ::EvaluationResult
{
    // "If the query container does not have a principal box, or the principal box is not a layout containment box,
    // or the query container does not support container size queries on the relevant axes, then the result of
    // evaluating the size feature is unknown."
    // https://drafts.csswg.org/css-contain-3/#size-container
    if (!container.renderer)
        return MQ::EvaluationResult::Unknown;

    auto& renderer = *container.renderer;

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

    if (sizeFeature.name == CQ::FeatureNames::width())
        return evaluateLengthFeature(sizeFeature, renderer.contentWidth(), container.conversionData);

    if (sizeFeature.name == CQ::FeatureNames::height())
        return evaluateLengthFeature(sizeFeature, renderer.contentHeight(), container.conversionData);

    if (sizeFeature.name == CQ::FeatureNames::inlineSize())
        return evaluateLengthFeature(sizeFeature, renderer.contentLogicalWidth(), container.conversionData);

    if (sizeFeature.name == CQ::FeatureNames::blockSize())
        return evaluateLengthFeature(sizeFeature, renderer.contentLogicalHeight(), container.conversionData);

    if (sizeFeature.name == CQ::FeatureNames::aspectRatio()) {
        auto boxRatio = renderer.contentWidth().toDouble() / renderer.contentHeight().toDouble();
        return evaluateRatioFeature(sizeFeature, boxRatio);
    }

    if (sizeFeature.name == CQ::FeatureNames::orientation()) {
        bool isPortrait = renderer.contentHeight() >= renderer.contentWidth();
        return evaluateDiscreteFeature(sizeFeature, isPortrait ? CSSValuePortrait : CSSValueLandscape);
    }

    return MQ::EvaluationResult::Unknown;
}

}
