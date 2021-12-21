/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ChildChangeInvalidation.h"

#include "ElementTraversal.h"
#include "NodeRenderStyle.h"
#include "ShadowRoot.h"
#include "SlotAssignment.h"
#include "StyleResolver.h"
#include "StyleScopeRuleSets.h"
#include "TypedElementDescendantIterator.h"

namespace WebCore::Style {

void ChildChangeInvalidation::invalidateForChangedElement(Element& changedElement)
{
    auto& ruleSets = parentElement().styleResolver().ruleSets();

    Invalidator::MatchElementRuleSets matchElementRuleSets;

    bool isDescendant = changedElement.parentElement() != &parentElement();

    auto addHasInvalidation = [&](const Vector<InvalidationRuleSet>* invalidationRuleSets)  {
        if (!invalidationRuleSets)
            return;
        for (auto& invalidationRuleSet : *invalidationRuleSets) {
            if (!isHasPseudoClassMatchElement(invalidationRuleSet.matchElement))
                continue;
            if (isDescendant) {
                // Elements deeper in the tree can't affect anything except when :has() selector uses descendant combinator.
                if (invalidationRuleSet.matchElement != MatchElement::HasDescendant && invalidationRuleSet.matchElement != MatchElement::HasNonSubject)
                    continue;
            }
            Invalidator::addToMatchElementRuleSets(matchElementRuleSets, invalidationRuleSet);
        }
    };

    auto tagName = changedElement.localName().convertToASCIILowercase();
    addHasInvalidation(ruleSets.tagInvalidationRuleSets(tagName));

    if (changedElement.hasID())
        addHasInvalidation(ruleSets.idInvalidationRuleSets(changedElement.idForStyleResolution()));

    if (changedElement.hasAttributes()) {
        for (auto& attribute : changedElement.attributesIterator()) {
            auto attributeName = attribute.localName().convertToASCIILowercase();
            addHasInvalidation(ruleSets.attributeInvalidationRuleSets(attributeName));
        }
    }

    if (changedElement.hasClass()) {
        auto count = changedElement.classNames().size();
        for (size_t i = 0; i < count; ++i) {
            auto& className = changedElement.classNames()[i];
            addHasInvalidation(ruleSets.classInvalidationRuleSets(className));
        }
    }

    Invalidator::invalidateWithMatchElementRuleSets(changedElement, matchElementRuleSets);
}

void ChildChangeInvalidation::invalidateForHasBeforeMutation()
{
    ASSERT(m_needsHasInvalidation);

    if (m_childChange.isInsertion() && m_childChange.type != ContainerNode::ChildChange::Type::AllChildrenReplaced)
        return;

    traverseRemovedElements([&](auto& changedElement) {
        invalidateForChangedElement(changedElement);
    });
}

void ChildChangeInvalidation::invalidateForHasAfterMutation()
{
    ASSERT(m_needsHasInvalidation);

    if (!m_childChange.isInsertion())
        return;

    traverseAddedElements([&](auto& changedElement) {
        invalidateForChangedElement(changedElement);
    });
}

static bool needsDescendantTraversal(const RuleFeatureSet& features)
{
    if (features.usesMatchElement(MatchElement::HasNonSubject))
        return true;
    return features.usesMatchElement(MatchElement::HasDescendant) || features.usesMatchElement(MatchElement::HasSiblingDescendant);
};

template<typename Function>
void ChildChangeInvalidation::traverseRemovedElements(Function&& function)
{
    auto& features = parentElement().styleResolver().ruleSets().features();
    bool needsDescendantTraversal = Style::needsDescendantTraversal(features);

    auto* toRemove = m_childChange.previousSiblingElement ? m_childChange.previousSiblingElement->nextElementSibling() : parentElement().firstElementChild();
    for (; toRemove != m_childChange.nextSiblingElement; toRemove = toRemove->nextElementSibling()) {
        function(*toRemove);

        if (!needsDescendantTraversal)
            continue;

        for (auto& descendant : descendantsOfType<Element>(*toRemove))
            function(descendant);
    }
}

template<typename Function>
void ChildChangeInvalidation::traverseAddedElements(Function&& function)
{
    auto* newElement = [&] {
        auto* previous = m_childChange.previousSiblingElement;
        auto* candidate = previous ? ElementTraversal::nextSibling(*previous) : ElementTraversal::firstChild(parentElement());
        if (candidate == m_childChange.nextSiblingElement)
            candidate = nullptr;
        return candidate;
    }();

    if (!newElement)
        return;

    function(*newElement);

    auto& features = parentElement().styleResolver().ruleSets().features();
    if (!needsDescendantTraversal(features))
        return;

    for (auto& descendant : descendantsOfType<Element>(*newElement))
        function(descendant);
}

static void checkForEmptyStyleChange(Element& element)
{
    if (!element.styleAffectedByEmpty())
        return;

    auto* style = element.renderStyle();
    if (!style || (!style->emptyState() || element.hasChildNodes()))
        element.invalidateStyleForSubtree();
}

static void invalidateForForwardPositionalRules(Element& parent, Element* elementAfterChange)
{
    bool childrenAffected = parent.childrenAffectedByForwardPositionalRules();
    bool descendantsAffected = parent.descendantsAffectedByForwardPositionalRules();

    if (!childrenAffected && !descendantsAffected)
        return;

    for (auto* sibling = elementAfterChange; sibling; sibling = sibling->nextElementSibling()) {
        if (childrenAffected)
            sibling->invalidateStyleInternal();
        if (descendantsAffected) {
            for (auto* siblingChild = sibling->firstElementChild(); siblingChild; siblingChild = siblingChild->nextElementSibling())
                siblingChild->invalidateStyleForSubtreeInternal();
        }
    }
}

static void invalidateForBackwardPositionalRules(Element& parent, Element* elementBeforeChange)
{
    bool childrenAffected = parent.childrenAffectedByBackwardPositionalRules();
    bool descendantsAffected = parent.descendantsAffectedByBackwardPositionalRules();

    if (!childrenAffected && !descendantsAffected)
        return;

    for (auto* sibling = elementBeforeChange; sibling; sibling = sibling->previousElementSibling()) {
        if (childrenAffected)
            sibling->invalidateStyleInternal();
        if (descendantsAffected) {
            for (auto* siblingChild = sibling->firstElementChild(); siblingChild; siblingChild = siblingChild->nextElementSibling())
                siblingChild->invalidateStyleForSubtreeInternal();
        }
    }
}

static void invalidateForFirstChildState(Element& child, bool state)
{
    auto* style = child.renderStyle();
    if (!style || style->firstChildState() == state)
        child.invalidateStyleForSubtreeInternal();
}

static void invalidateForLastChildState(Element& child, bool state)
{
    auto* style = child.renderStyle();
    if (!style || style->lastChildState() == state)
        child.invalidateStyleForSubtreeInternal();
}

void ChildChangeInvalidation::invalidateAfterChange()
{
    checkForEmptyStyleChange(parentElement());

    if (m_childChange.source == ContainerNode::ChildChange::Source::Parser)
        return;

    checkForSiblingStyleChanges();
}

void ChildChangeInvalidation::invalidateAfterFinishedParsingChildren(Element& parent)
{
    if (!parent.needsStyleInvalidation())
        return;

    checkForEmptyStyleChange(parent);

    auto* lastChildElement = ElementTraversal::lastChild(parent);
    if (!lastChildElement)
        return;

    if (parent.childrenAffectedByLastChildRules())
        invalidateForLastChildState(*lastChildElement, false);

    invalidateForBackwardPositionalRules(parent, lastChildElement);
}

void ChildChangeInvalidation::checkForSiblingStyleChanges()
{
    auto& parent = parentElement();
    auto* elementBeforeChange = m_childChange.previousSiblingElement;
    auto* elementAfterChange = m_childChange.nextSiblingElement;

    // :first-child. In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    // |afterChange| is 0 in the parser case, so it works out that we'll skip this block.
    if (parent.childrenAffectedByFirstChildRules() && elementAfterChange) {
        // Find our new first child.
        RefPtr<Element> newFirstElement = ElementTraversal::firstChild(parent);

        // This is the insert/append case.
        if (newFirstElement != elementAfterChange)
            invalidateForFirstChildState(*elementAfterChange, true);

        // We also have to handle node removal.
        if (m_childChange.type == ContainerNode::ChildChange::Type::ElementRemoved && newFirstElement == elementAfterChange)
            invalidateForFirstChildState(*newFirstElement, false);
    }

    // :last-child. In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (parent.childrenAffectedByLastChildRules() && elementBeforeChange) {
        // Find our new last child.
        RefPtr<Element> newLastElement = ElementTraversal::lastChild(parent);

        if (newLastElement != elementBeforeChange)
            invalidateForLastChildState(*elementBeforeChange, true);

        // We also have to handle node removal.
        if (m_childChange.type == ContainerNode::ChildChange::Type::ElementRemoved && newLastElement == elementBeforeChange)
            invalidateForLastChildState(*newLastElement, false);
    }

    invalidateForSiblingCombinators(elementAfterChange);

    invalidateForForwardPositionalRules(parent, elementAfterChange);
    invalidateForBackwardPositionalRules(parent, elementBeforeChange);
}

}
