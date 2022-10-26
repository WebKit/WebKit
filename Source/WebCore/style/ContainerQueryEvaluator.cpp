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
#include "ContainerQueryFeatures.h"
#include "Document.h"
#include "MediaFeatureNames.h"
#include "MediaList.h"
#include "NodeRenderStyle.h"
#include "RenderView.h"
#include "StyleRule.h"
#include "StyleScope.h"

namespace WebCore::Style {

ContainerQueryEvaluator::ContainerQueryEvaluator(const Element& element, SelectionMode selectionMode, ScopeOrdinal scopeOrdinal, SelectorMatchingState* selectorMatchingState)
    : m_element(element)
    , m_selectionMode(selectionMode)
    , m_scopeOrdinal(scopeOrdinal)
    , m_selectorMatchingState(selectorMatchingState)
{
}

bool ContainerQueryEvaluator::evaluate(const CQ::ContainerQuery& containerQuery) const
{
    auto context = featureEvaluationContextForQuery(containerQuery);
    if (!context)
        return false;

    return evaluateCondition(containerQuery.condition, *context) == MQ::EvaluationResult::True;
}

auto ContainerQueryEvaluator::featureEvaluationContextForQuery(const CQ::ContainerQuery& containerQuery) const -> std::optional<MQ::FeatureEvaluationContext>
{
    // "For each element, the query container to be queried is selected from among the element’s
    // ancestor query containers that have a valid container-type for all the container features
    // in the <container-condition>. The optional <container-name> filters the set of query containers
    // considered to just those with a matching query container name."
    // https://drafts.csswg.org/css-contain-3/#container-rule

    auto* cachedQueryContainers = m_selectorMatchingState ? &m_selectorMatchingState->queryContainers : nullptr;

    auto* container = selectContainer(containerQuery.axisFilter, containerQuery.name, m_element.get(), m_selectionMode, m_scopeOrdinal, cachedQueryContainers);
    if (!container)
        return { };

    if (!container->renderer())
        return MQ::FeatureEvaluationContext { m_element->document() };

    auto& renderer = *container->renderer();

    return MQ::FeatureEvaluationContext {
        m_element->document(),
        CSSToLengthConversionData { renderer.style(), m_element->document().documentElement()->renderStyle(), nullptr, &renderer.view() },
        &renderer
    };
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

}
