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

namespace WebCore {

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

HTMLSlotElement::InsertionNotificationRequest HTMLSlotElement::insertedInto(ContainerNode& insertionPoint)
{
    auto insertionResult = HTMLElement::insertedInto(insertionPoint);
    ASSERT_UNUSED(insertionResult, insertionResult == InsertionDone);

    // This function could be called when this element's shadow root's host or its ancestor is inserted.
    // This element is new to the shadow tree (and its tree scope) only if the parent into which this element
    // or its ancestor is inserted belongs to the same tree scope as this element's.
    if (insertionPoint.isInShadowTree() && isInShadowTree() && &insertionPoint.treeScope() == &treeScope()) {
        if (auto shadowRoot = containingShadowRoot())
            shadowRoot->addSlotElementByName(attributeWithoutSynchronization(nameAttr), *this);
    }

    return InsertionDone;
}

void HTMLSlotElement::removedFrom(ContainerNode& insertionPoint)
{
    // ContainerNode::removeBetween always sets the removed child's tree scope to Document's but InShadowRoot flag is unset in Node::removedFrom.
    // So if InShadowRoot flag is set but this element's tree scope is Document's, this element has just been removed from a shadow root.
    if (insertionPoint.isInShadowTree() && isInShadowTree() && &treeScope() == &document()) {
        auto* oldShadowRoot = insertionPoint.containingShadowRoot();
        ASSERT(oldShadowRoot);
        oldShadowRoot->removeSlotElementByName(attributeWithoutSynchronization(nameAttr), *this);
    }

    HTMLElement::removedFrom(insertionPoint);
}

void HTMLSlotElement::attributeChanged(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason reason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, reason);

    if (isInShadowTree() && name == nameAttr) {
        if (auto* shadowRoot = containingShadowRoot()) {
            shadowRoot->removeSlotElementByName(oldValue, *this);
            shadowRoot->addSlotElementByName(newValue, *this);
        }
    }
}

const Vector<Node*>* HTMLSlotElement::assignedNodes() const
{
    auto* shadowRoot = containingShadowRoot();
    if (!shadowRoot)
        return nullptr;

    return shadowRoot->assignedNodesForSlot(*this);
}

static void flattenAssignedNodes(Vector<Node*>& nodes, const Vector<Node*>& assignedNodes)
{
    for (Node* node : assignedNodes) {
        if (is<HTMLSlotElement>(*node)) {
            if (auto* innerAssignedNodes = downcast<HTMLSlotElement>(*node).assignedNodes())
                flattenAssignedNodes(nodes, *innerAssignedNodes);
            continue;
        }
        nodes.append(node);
    }
}

Vector<Node*> HTMLSlotElement::assignedNodes(const AssignedNodesOptions& options) const
{
    auto* assignedNodes = this->assignedNodes();
    if (!assignedNodes)
        return { };

    if (!options.flatten)
        return *assignedNodes;

    Vector<Node*> nodes;
    flattenAssignedNodes(nodes, *assignedNodes);
    return nodes;
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

    bool bubbles = false;
    bool cancelable = false;
    Ref<Event> event = Event::create(eventNames().slotchangeEvent, bubbles, cancelable);
    event->setTarget(this);
    dispatchEvent(event);
}

}

