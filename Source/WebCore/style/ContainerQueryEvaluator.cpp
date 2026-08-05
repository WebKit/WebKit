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
#include "ContainerNodeInlines.h"
#include "ContainerQueryFeatures.h"
#include "Document.h"
#include "MediaList.h"
#include "NodeDocument.h"
#include "NodeRenderStyle.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include <ranges>

namespace WebCore::Style {

ContainerQueryEvaluator::ContainerQueryEvaluator(const Element& element, SelectionMode selectionMode, ScopeOrdinal scopeOrdinal, ContainerQueryEvaluationState* evaluationState)
    : m_element(element)
    , m_selectionMode(selectionMode)
    , m_scopeOrdinal(scopeOrdinal)
    , m_evaluationState(evaluationState)
{
}

bool ContainerQueryEvaluator::evaluate(const CQ::ContainerQuery& containerQuery) const
{
    for (const auto& condition : containerQuery) {
        auto context = featureEvaluationContextForCondition(condition);
        if (!context)
            continue;

        if (condition.condition.queries.isEmpty() && !condition.name.isEmpty())
            return true;

        if (evaluateCondition(condition.condition, *context) == MQ::EvaluationResult::True)
            return true;
    };

    return false;
}

static const Style::ComputedStyle* styleForContainer(const Element& container, CQ::ContainerRequirements requirements, const ContainerQueryEvaluationState* evaluationState)
{
    // Queries that don't need a size container (style and scroll-state queries) resolve
    // against the container's style, which may not be committed to the render tree yet.
    // Look it up from the currently computed style update instead.
    if (!requirements.needsSizeContainer() && evaluationState && evaluationState->styleUpdate)
        return evaluationState->styleUpdate->elementStyle(container);

    return container.existingComputedStyle();
}

auto ContainerQueryEvaluator::featureEvaluationContextForCondition(const CQ::ContainerCondition& condition) const -> std::optional<MQ::FeatureEvaluationContext>
{
    // "For each element, the query container to be queried is selected from among the element’s
    // ancestor query containers that have a valid container-type for all the container features
    // in the <container-condition>. The optional <container-name> filters the set of query containers
    // considered to just those with a matching query container name."
    // https://drafts.csswg.org/css-contain-3/#container-rule

    // "If the <container-query> contains unknown or unsupported container features, no query container will be selected."
    if (condition.containsUnknownFeature == CQ::ContainsUnknownFeature::Yes)
        return { };

    Ref element = m_element;
    RefPtr container = selectContainer(condition.requirements, condition.name, element.get(), m_selectionMode, m_scopeOrdinal, m_evaluationState);
    if (!container)
        return { };

    CheckedPtr containerStyle = styleForContainer(*container.get(), condition.requirements, m_evaluationState);
    if (!containerStyle)
        return { };

    RefPtr containerParent = container->parentElementInComposedTree();
    CheckedPtr containerParentStyle = containerParent ? CheckedPtr { styleForContainer(*containerParent, condition.requirements, m_evaluationState) } : containerStyle;

    Ref document = element->document();

    CheckedPtr rootStyle = [&] () -> const Style::ComputedStyle* {
        RefPtr rootElement = document->documentElement();
        if (!rootElement)
            return nullptr;

        return styleForContainer(*rootElement, condition.requirements, m_evaluationState);
    }();

    return MQ::FeatureEvaluationContext {
        document.get(),
        CSSToLengthConversionData { *containerStyle, rootStyle.get(), containerParentStyle.get(), document->renderView(), container.get() },
        container->renderer()
    };
}

RefPtr<const Element> ContainerQueryEvaluator::selectContainer(CQ::ContainerRequirements requirements, const WTF::String& name, const Element& element, SelectionMode selectionMode, ScopeOrdinal scopeOrdinal, const ContainerQueryEvaluationState* evaluationState)
{
    // "For each element, the query container to be queried is selected from among the element’s
    // ancestor query containers that have a valid container-type for all the container features
    // in the <container-condition>. The optional <container-name> filters the set of query containers
    // considered to just those with a matching query container name."
    // https://drafts.csswg.org/css-contain-3/#container-rule

    auto isValidContainer = [&](const Style::ContainerType& containerType, const RenderElement* principalBox) {
        // A scroll-state query requires a scroll-state container.
        if (requirements.scrollState && !containerType.hasScrollState())
            return false;

        // No size container required: any container is valid (style query), or the
        // scroll-state requirement above has already been satisfied.
        if (!requirements.needsSizeContainer())
            return true;

        if (containerType.hasSize())
            return true;
        if (containerType.hasInlineSize()) {
            // Without a principal box the container matches but the query against it will evaluate to Unknown.
            if (!principalBox)
                return true;
            if (requirements.sizeAxes.contains(CQ::Axis::Block))
                return false;
            return !requirements.sizeAxes.contains(principalBox->isHorizontalWritingMode() ? CQ::Axis::Height : CQ::Axis::Width);
        }
        // A normal container, or a scroll-state-only container, provides no size
        // containment and so is not a valid container for a size query.
        if (containerType.isNormal() || containerType.hasScrollState())
            return false;
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto isContainerForQuery = [&](const Element& candidateElement, const Element* originatingElement = nullptr) {
        CheckedPtr style = styleForContainer(candidateElement, requirements, evaluationState);
        if (!style)
            return false;
        if (!isValidContainer(style->containerType(), candidateElement.renderer()))
            return false;
        if (name.isEmpty())
            return true;

        return style->containerNames().containsIf([&](auto& scopedName) {
            auto isNameFromAllowedScope = [&](auto& scopedName) {
                // Names from :host rules are allowed when the candidate is the host element.
                RefPtr host = originatingElement ? originatingElement->shadowHost() : element.shadowHost();
                auto isHost = host == &candidateElement;
                if (scopedName.scopeOrdinal == ScopeOrdinal::Shadow && isHost)
                    return true;
                // Otherwise names from the inner scopes are ignored.
                return scopedName.scopeOrdinal <= ScopeOrdinal::Element;
            };
            return isNameFromAllowedScope(scopedName) && scopedName.name == name;
        });
    };

    auto findOriginatingElement = [&]() -> RefPtr<const Element> {
        if (selectionMode == SelectionMode::PseudoElement)
            return &element;

        // ::part() selectors query the composed tree
        if (selectionMode == SelectionMode::PartPseudoElement)
            return element;

        // ::slotted() selectors can query containers inside the shadow tree, including the slot itself.
        if (scopeOrdinal >= ScopeOrdinal::FirstSlot && scopeOrdinal <= ScopeOrdinal::SlotLimit)
            return assignedSlotForScopeOrdinal(element, scopeOrdinal);

        if (scopeOrdinal == ScopeOrdinal::Element && element.assignedSlot())
            return element.assignedSlot();

        return nullptr;
    };

    if (RefPtr originatingElement = findOriginatingElement()) {
        // For the ::part() and ::slotted() pseudo-element selectors, which represent real elements in the DOM tree,
        // query containers can be established by flat tree ancestors of those elements.
        // For other pseudo-elements, query containers can be established by inclusive flat tree ancestors of their originating element.
        // https://drafts.csswg.org/css-conditional-5/#container-queries
        for (Ref ancestor : composedTreeLineage(*originatingElement)) {
            if (isContainerForQuery(ancestor, originatingElement.get()))
                return ancestor.ptr();
        }
        return nullptr;
    }

    if (evaluationState && requirements.needsSizeContainer()) {
        for (auto& container : evaluationState->sizeQueryContainers | std::views::reverse) {
            if (isContainerForQuery(container))
                return container.ptr();
        }
        return { };
    }

    for (Ref ancestor : composedTreeAncestors(element)) {
        if (isContainerForQuery(ancestor))
            return ancestor.ptr();
    }
    return { };
}

}
