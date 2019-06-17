/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLSlotElement.h"

#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "MutationObserver.h"
#include "ShadowRoot.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLSlotElement);

using namespace HTMLNames;

Ref<HTMLSlotElement> HTMLSlotElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLSlotElement(tagName, document));
}

HTMLSlotElement::HTMLSlotElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(slotTag));
}

HTMLSlotElement::InsertedIntoAncestorResult HTMLSlotElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto insertionResult = HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    ASSERT_UNUSED(insertionResult, insertionResult == InsertedIntoAncestorResult::Done);

    if (insertionType.treeScopeChanged && isInShadowTree()) {
        if (auto* shadowRoot = containingShadowRoot())
            shadowRoot->addSlotElementByName(attributeWithoutSynchronization(nameAttr), *this);
    }

    return InsertedIntoAncestorResult::Done;
}

void HTMLSlotElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.treeScopeChanged && oldParentOfRemovedTree.isInShadowTree()) {
        auto* oldShadowRoot = oldParentOfRemovedTree.containingShadowRoot();
        ASSERT(oldShadowRoot);
        oldShadowRoot->removeSlotElementByName(attributeWithoutSynchronization(nameAttr), *this, oldParentOfRemovedTree);
    }

    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

void HTMLSlotElement::childrenChanged(const ChildChange& childChange)
{
    HTMLElement::childrenChanged(childChange);

    if (isInShadowTree()) {
        if (auto* shadowRoot = containingShadowRoot())
            shadowRoot->slotFallbackDidChange(*this);
    }
}

void HTMLSlotElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason reason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, reason);

    if (isInShadowTree() && name == nameAttr) {
        if (auto shadowRoot = makeRefPtr(containingShadowRoot()))
            shadowRoot->renameSlotElement(*this, oldValue, newValue);
    }
}

const Vector<Node*>* HTMLSlotElement::assignedNodes() const
{
    auto shadowRoot = makeRefPtr(containingShadowRoot());
    if (!shadowRoot)
        return nullptr;

    return shadowRoot->assignedNodesForSlot(*this);
}

static void flattenAssignedNodes(Vector<Ref<Node>>& nodes, const HTMLSlotElement& slot)
{
    auto* assignedNodes = slot.assignedNodes();
    if (!assignedNodes) {
        for (RefPtr<Node> child = slot.firstChild(); child; child = child->nextSibling()) {
            if (is<HTMLSlotElement>(*child))
                flattenAssignedNodes(nodes, downcast<HTMLSlotElement>(*child));
            else if (is<Text>(*child) || is<Element>(*child))
                nodes.append(*child);
        }
        return;
    }
    for (RefPtr<Node> node : *assignedNodes) {
        if (is<HTMLSlotElement>(*node))
            flattenAssignedNodes(nodes, downcast<HTMLSlotElement>(*node));
        else
            nodes.append(*node);
    }
}

Vector<Ref<Node>> HTMLSlotElement::assignedNodes(const AssignedNodesOptions& options) const
{
    if (options.flatten) {
        if (!isInShadowTree())
            return { };
        Vector<Ref<Node>> nodes;
        flattenAssignedNodes(nodes, *this);
        return nodes;
    }
    auto* assignedNodes = this->assignedNodes();
    if (!assignedNodes)
        return { };
    return assignedNodes->map([] (Node* node) { return makeRef(*node); });
}

Vector<Ref<Element>> HTMLSlotElement::assignedElements(const AssignedNodesOptions& options) const
{
    auto nodes = assignedNodes(options);

    Vector<Ref<Element>> elements;
    elements.reserveCapacity(nodes.size());
    for (auto& node : nodes) {
        if (is<Element>(node))
            elements.uncheckedAppend(static_reference_cast<Element>(WTFMove(node)));
    }

    return elements;
}

void HTMLSlotElement::enqueueSlotChangeEvent()
{
    // https://dom.spec.whatwg.org/#signal-a-slot-change
    if (m_inSignalSlotList)
        return;
    m_inSignalSlotList = true;
    MutationObserver::enqueueSlotChangeEvent(*this);
}

void HTMLSlotElement::dispatchSlotChangeEvent()
{
    m_inSignalSlotList = false;

    Ref<Event> event = Event::create(eventNames().slotchangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No);
    event->setTarget(this);
    dispatchEvent(event);
}

}

