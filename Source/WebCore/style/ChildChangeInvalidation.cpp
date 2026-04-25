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
#include "PseudoClassChangeInvalidation.h"
#include "RenderElement.h"
#include "RenderStyle+GettersInlines.h"
#include "ShadowRoot.h"
#include "SlotAssignment.h"
#include "StyleResolver.h"
#include "StyleScopeRuleSets.h"
#include "TypedElementDescendantIteratorInlines.h"

namespace WebCore::Style {

static bool rightmostCompoundContainsEmpty(const CSSSelector& selector)
{
    for (auto* simple = &selector; simple; simple = simple->precedingInCompound()) {
        if (simple->match() == CSSSelector::Match::PseudoClass && simple->pseudoClass() == CSSSelector::PseudoClass::Empty)
            return true;
        if (auto* selectorList = simple->selectorList()) {
            for (auto& inner : *selectorList) {
                if (rightmostCompoundContainsEmpty(inner))
                    return true;
            }
        }
    }
    return false;
}

static bool isSiblingHasRelation(const MatchElement& matchElement)
{
    if (!matchElement.hasRelation)
        return false;
    switch (*matchElement.hasRelation) {
    case MatchElement::HasRelation::DirectSibling:
    case MatchElement::HasRelation::IndirectSibling:
    case MatchElement::HasRelation::SiblingChild:
    case MatchElement::HasRelation::SiblingDescendant:
        return true;
    case MatchElement::HasRelation::Child:
    case MatchElement::HasRelation::Descendant:
    case MatchElement::HasRelation::ScopeBreaking:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void ChildChangeInvalidation::invalidateForChangedElement(Element& changedElement, MatchingHasSelectors& matchingHasSelectors, ChangedElementRelation changedElementRelation, EmptyInvalidation emptyInvalidation)
{
    auto& ruleSets = parentElement().styleResolver().ruleSets();

    Invalidator::MatchElementRuleSets matchElementRuleSets;

    bool isChild = changedElement.parentElement() == &parentElement();

    auto canAffectElementsWithStyle = [&](const InvalidationRuleSet& ruleSet) {
        if (!ruleSet.matchElement.hasRelation)
            return true;
        switch (*ruleSet.matchElement.hasRelation) {
        case MatchElement::HasRelation::Child:
        case MatchElement::HasRelation::DirectSibling:
        case MatchElement::HasRelation::IndirectSibling:
            return isChild;
        case MatchElement::HasRelation::Descendant:
        case MatchElement::HasRelation::SiblingChild:
        case MatchElement::HasRelation::SiblingDescendant:
            return true;
        default:
            return true;
        }
    };

    auto hasAlreadyMatchedAndMutationIsIrrelevant = [&](const InvalidationRuleSet& invalidationRuleSet) {
        // For the first changed element at the mutation point, check if a neighbor already matches the
        // :has() argument. If so, adding/removing one more matching element doesn't change the :has() result.
        // This doesn't apply inside :not() (inverted logic) or for sibling :has() arguments (direction matters).
        if (!isChild || changedElementRelation != ChangedElementRelation::SelfOrDescendant)
            return false;
        if (m_childChange.previousSiblingElement != changedElement.previousElementSibling())
            return false;
        if (invalidationRuleSet.isNegation == IsNegation::Yes)
            return false;
        if (isSiblingHasRelation(invalidationRuleSet.matchElement))
            return false;
        return true;
    };

    auto hasMatchingInvalidationSelector = [&](auto& invalidationRuleSet) {
        SelectorChecker selectorChecker(changedElement.document());
        SelectorChecker::CheckingContext checkingContext(SelectorChecker::Mode::StyleInvalidation);
        checkingContext.matchesAllHasScopes = true;

        for (auto& selector : invalidationRuleSet.invalidationSelectors) {
            // For :empty invalidation only look for :empty selectors to avoid invalidating unnecessarily.
            if (emptyInvalidation == EmptyInvalidation::Yes && !rightmostCompoundContainsEmpty(selector))
                continue;

            if (hasAlreadyMatchedAndMutationIsIrrelevant(invalidationRuleSet)) {
                // FIXME: We could cache this state across invalidations instead of just testing a single sibling.
                RefPtr sibling = m_childChange.previousSiblingElement ? m_childChange.previousSiblingElement : m_childChange.nextSiblingElement;
                if (sibling && selectorChecker.match(selector, *sibling, checkingContext)) {
                    matchingHasSelectors.add(&selector);
                    continue;
                }
            }

            if (matchingHasSelectors.contains(&selector))
                continue;

            if (selectorChecker.match(selector, changedElement, checkingContext)) {
                matchingHasSelectors.add(&selector);
                return true;
            }
        }
        return false;
    };

    auto addHasInvalidation = [&](const Vector<InvalidationRuleSet>* invalidationRuleSets) {
        if (!invalidationRuleSets)
            return;
        for (auto& invalidationRuleSet : *invalidationRuleSets) {
            if (!canAffectElementsWithStyle(invalidationRuleSet))
                continue;
            if (!hasMatchingInvalidationSelector(invalidationRuleSet))
                continue;
            Invalidator::addToMatchElementRuleSetsRespectingNegation(matchElementRuleSets, invalidationRuleSet);
        }
    };

    for (auto key : makePseudoClassInvalidationKeys(CSSSelector::PseudoClass::Has, changedElement))
        addHasInvalidation(ruleSets.hasPseudoClassInvalidationRuleSets(key));

    Invalidator::invalidateWithMatchElementRuleSets(changedElement, matchElementRuleSets);
}

void ChildChangeInvalidation::invalidateForChangeOutsideHasScope()
{
    // FIXME: This is a performance footgun. Any mutation will trigger a full document traversal.
    if (RefPtr invalidationRuleSet = parentElement().styleResolver().ruleSets().scopeBreakingHasPseudoClassInvalidationRuleSet())
        Invalidator::invalidateWithScopeBreakingHasPseudoClassRuleSet(parentElement(), invalidationRuleSet.get());
}

void ChildChangeInvalidation::invalidateForHasSiblings(MatchingHasSelectors& matchingHasSelectors, MutationPhase phase)
{
    bool affectedByBackwardSibling = parentElement().affectedByHasWithBackwardSiblingRelationship();
    bool affectedByForwardSibling = parentElement().affectedByHasWithForwardSiblingRelationship();
    bool affectedByAdjacentSibling = parentElement().affectedByHasWithAdjacentSiblingRelationship();

    auto invalidateSibling = [&](auto& changedElement) {
        invalidateForChangedElement(changedElement, matchingHasSelectors, ChangedElementRelation::Sibling);
    };
    if (affectedByBackwardSibling || affectedByAdjacentSibling) {
        for (RefPtr child = m_childChange.previousSiblingElement; child; child = child->previousElementSibling()) {
            invalidateSibling(*child);
            if (!affectedByBackwardSibling)
                break;
        }
    }
    if (affectedByForwardSibling || affectedByAdjacentSibling) {
        for (RefPtr child = m_childChange.nextSiblingElement; child; child = child->nextElementSibling()) {
            invalidateSibling(*child);
            if (!affectedByForwardSibling)
                break;
        }
    }

    // For insertion, the pre-mutation :first/:last-child state of the neighbor will stop matching.
    // For removal, the post-mutation state of the neighbor will start matching.
    bool checkNow = phase == MutationPhase::Before ? m_childChange.isInsertion() : !m_childChange.isInsertion();
    if (!checkNow)
        return;

    if (RefPtr next = m_childChange.nextSiblingElement; next && parentElement().childrenAffectedByFirstChildRules() && !next->previousElementSibling())
        invalidateForChangedElement(*next, matchingHasSelectors, ChangedElementRelation::Sibling);

    if (RefPtr previous = m_childChange.previousSiblingElement; previous && parentElement().childrenAffectedByLastChildRules() && !previous->nextElementSibling())
        invalidateForChangedElement(*previous, matchingHasSelectors, ChangedElementRelation::Sibling);
}

void ChildChangeInvalidation::invalidateForHasBeforeMutation()
{
    ASSERT(m_needsHasInvalidation);

    invalidateForChangeOutsideHasScope();

    MatchingHasSelectors matchingHasSelectors;

    traverseRemovedElements([&](auto& changedElement) {
        invalidateForChangedElement(changedElement, matchingHasSelectors, ChangedElementRelation::SelfOrDescendant);
    });

    auto emptyStateWillChange = [&] {
        bool isEmpty = !parentElement().hasChildNodes();
        return m_childChange.isInsertion() == isEmpty;
    };

    // :empty is special because insertions and removals can affect the matching state of the parent element.
    if (emptyStateWillChange())
        invalidateForChangedElement(parentElement(), matchingHasSelectors, ChangedElementRelation::SelfOrDescendant, EmptyInvalidation::Yes);

    invalidateForHasSiblings(matchingHasSelectors, MutationPhase::Before);
}

void ChildChangeInvalidation::invalidateForHasAfterMutation()
{
    ASSERT(m_needsHasInvalidation);

    invalidateForChangeOutsideHasScope();

    MatchingHasSelectors matchingHasSelectors;

    traverseAddedElements([&](auto& changedElement) {
        invalidateForChangedElement(changedElement, matchingHasSelectors, ChangedElementRelation::SelfOrDescendant);
    });

    auto emptyStateDidChange = [&] {
        bool isEmpty = !parentElement().hasChildNodes();
        return m_childChange.isInsertion() != isEmpty;
    };

    // :empty is special because insertions and removals can affect the matching state of the parent element.
    if (emptyStateDidChange())
        invalidateForChangedElement(parentElement(), matchingHasSelectors, ChangedElementRelation::SelfOrDescendant, EmptyInvalidation::Yes);

    invalidateForHasSiblings(matchingHasSelectors, MutationPhase::After);
}

static bool NODELETE needsDescendantTraversal(const RuleFeatureSet& features)
{
    // With the bundled MatchElement representation, any :has() with hasRelation=Descendant or SiblingDescendant
    // needs descendant traversal. Since we don't have per-value tracking for hasRelation,
    // use the usesHasPseudoClass flag as a conservative check.
    return features.usesHasPseudoClass;
};

template<typename Function>
void ChildChangeInvalidation::traverseRemovedElements(Function&& function)
{
    if (m_childChange.isInsertion() && m_childChange.type != ContainerNode::ChildChange::Type::AllChildrenReplaced)
        return;

    auto& features = parentElement().styleResolver().ruleSets().features();
    bool needsDescendantTraversal = Style::needsDescendantTraversal(features);

    RefPtr firstToRemove = m_childChange.previousSiblingElement ? m_childChange.previousSiblingElement->nextElementSibling() : parentElement().firstElementChild();

    for (RefPtr toRemove = firstToRemove; toRemove != m_childChange.nextSiblingElement; toRemove = toRemove->nextElementSibling()) {
        function(*toRemove);

        if (!needsDescendantTraversal)
            continue;

        for (Ref descendant : descendantsOfType<Element>(*toRemove))
            function(descendant);
    }
}

template<typename Function>
void ChildChangeInvalidation::traverseAddedElements(Function&& function)
{
    if (!m_childChange.isInsertion())
        return;

    auto callFunctionOnInclusiveDescendants = [&](Element& element) {
        function(element);

        auto& features = parentElement().styleResolver().ruleSets().features();
        if (!needsDescendantTraversal(features))
            return;

        for (Ref descendant : descendantsOfType<Element>(element))
            function(descendant);
    };

    if (RefPtr newElement = m_childChange.siblingChanged)
        callFunctionOnInclusiveDescendants(*newElement);
    else if (auto* children = m_childChange.insertedChildren) {
        for (Ref node : *children) {
            if (auto* element = dynamicDowncast<Element>(node.get()))
                callFunctionOnInclusiveDescendants(*element);
        }
    }
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

    for (RefPtr sibling = elementAfterChange; sibling; sibling = sibling->nextElementSibling()) {
        if (childrenAffected)
            sibling->invalidateStyleInternal();
        if (descendantsAffected) {
            for (RefPtr siblingChild = sibling->firstElementChild(); siblingChild; siblingChild = siblingChild->nextElementSibling())
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

    for (RefPtr sibling = elementBeforeChange; sibling; sibling = sibling->previousElementSibling()) {
        if (childrenAffected)
            sibling->invalidateStyleInternal();
        if (descendantsAffected) {
            for (RefPtr siblingChild = sibling->firstElementChild(); siblingChild; siblingChild = siblingChild->nextElementSibling())
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

    RefPtr lastChildElement = ElementTraversal::lastChild(parent);
    if (!lastChildElement)
        return;

    if (parent.childrenAffectedByLastChildRules())
        invalidateForLastChildState(*lastChildElement, false);

    invalidateForBackwardPositionalRules(parent, lastChildElement.get());
}

void ChildChangeInvalidation::checkForSiblingStyleChanges()
{
    Ref parent = parentElement();
    RefPtr elementBeforeChange = m_childChange.previousSiblingElement;
    RefPtr elementAfterChange = m_childChange.nextSiblingElement;

    // :first-child. In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    // |afterChange| is 0 in the parser case, so it works out that we'll skip this block.
    if (parent->childrenAffectedByFirstChildRules() && elementAfterChange) {
        // Find our new first child.
        RefPtr<Element> newFirstElement = ElementTraversal::firstChild(parent.get());

        // This is the insert/append case.
        if (newFirstElement != elementAfterChange)
            invalidateForFirstChildState(*elementAfterChange, true);

        // We also have to handle node removal.
        if (m_childChange.type == ContainerNode::ChildChange::Type::ElementRemoved && newFirstElement == elementAfterChange)
            invalidateForFirstChildState(*newFirstElement, false);
    }

    // :last-child. In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (parent->childrenAffectedByLastChildRules() && elementBeforeChange) {
        // Find our new last child.
        RefPtr<Element> newLastElement = ElementTraversal::lastChild(parent.get());

        if (newLastElement != elementBeforeChange)
            invalidateForLastChildState(*elementBeforeChange, true);

        // We also have to handle node removal.
        if (m_childChange.type == ContainerNode::ChildChange::Type::ElementRemoved && newLastElement == elementBeforeChange)
            invalidateForLastChildState(*newLastElement, false);
    }

    invalidateForSiblingCombinators(elementAfterChange.get());

    invalidateForForwardPositionalRules(parent, elementAfterChange.get());
    invalidateForBackwardPositionalRules(parent, elementBeforeChange.get());
}

}
