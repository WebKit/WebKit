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

#include "NodeRenderStyle.h"
#include "ShadowRoot.h"
#include "SlotAssignment.h"

namespace WebCore::Style {

ChildChangeInvalidation::ChildChangeInvalidation(ContainerNode& container, const ContainerNode::ChildChange& childChange)
    : m_parentElement(is<Element>(container) ? downcast<Element>(&container) : nullptr)
    , m_isEnabled(m_parentElement ? m_parentElement->needsStyleInvalidation() : false)
    , m_childChange(childChange)
{
    // FIXME: Do smarter invalidation similat to ClassChangeInvalidation.
}

ChildChangeInvalidation::~ChildChangeInvalidation()
{
    if (!m_isEnabled)
        return;

    invalidateAfterChange();
}

void ChildChangeInvalidation::invalidateAfterChange()
{
    checkForEmptyStyleChange();

    if (m_childChange.type == ContainerNode::ChildChange::Type::FinishedParsingChildren) {
        checkForSiblingStyleChanges();
        return;
    }

    if (m_childChange.source == ContainerNode::ChildChange::Source::Parser)
        return;

    checkForSiblingStyleChanges();
}

void ChildChangeInvalidation::checkForEmptyStyleChange()
{
    auto& element = parentElement();
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
        // Find the first element node following |afterChange|

        // This is the insert/append case.
        if (newFirstElement != elementAfterChange) {
            auto* style = elementAfterChange->renderStyle();
            if (!style || style->firstChildState())
                elementAfterChange->invalidateStyleForSubtreeInternal();
        }

        // We also have to handle node removal.
        if (m_childChange.type == ContainerNode::ChildChange::Type::ElementRemoved && newFirstElement == elementAfterChange && newFirstElement) {
            auto* style = newFirstElement->renderStyle();
            if (!style || !style->firstChildState())
                newFirstElement->invalidateStyleForSubtreeInternal();
        }
    }

    // :last-child. In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (parent.childrenAffectedByLastChildRules() && elementBeforeChange) {
        // Find our new last child.
        RefPtr<Element> newLastElement = ElementTraversal::lastChild(parent);

        if (newLastElement != elementBeforeChange) {
            auto* style = elementBeforeChange->renderStyle();
            if (!style || style->lastChildState())
                elementBeforeChange->invalidateStyleForSubtreeInternal();
        }

        // We also have to handle node removal. The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        bool removedOrFinished = m_childChange.type == ContainerNode::ChildChange::Type::ElementRemoved || m_childChange.type == ContainerNode::ChildChange::Type::FinishedParsingChildren;
        if (removedOrFinished && newLastElement == elementBeforeChange && newLastElement) {
            auto* style = newLastElement->renderStyle();
            if (!style || !style->lastChildState())
                newLastElement->invalidateStyleForSubtreeInternal();
        }
    }

    invalidateForSiblingCombinators(elementAfterChange);

    invalidateForForwardPositionalRules(parent, elementAfterChange);
    invalidateForBackwardPositionalRules(parent, elementBeforeChange);
}

}
