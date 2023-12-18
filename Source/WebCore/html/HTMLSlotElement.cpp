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

#include "ElementInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "MutationObserver.h"
#include "ShadowRoot.h"
#include "SlotAssignment.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>

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
    SetForScope isInInsertedIntoAncestor { m_isInInsertedIntoAncestor, true };

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
        if (RefPtr shadowRoot = containingShadowRoot())
            shadowRoot->renameSlotElement(*this, oldValue, newValue);
    }
}

const Vector<WeakPtr<Node, WeakPtrImplWithEventTargetData>>* HTMLSlotElement::assignedNodes() const
{
    RefPtr shadowRoot = containingShadowRoot();
    if (!shadowRoot)
        return nullptr;

    return shadowRoot->assignedNodesForSlot(*this);
}

static void flattenAssignedNodes(Vector<Ref<Node>>& nodes, const HTMLSlotElement& slot)
{
    if (!slot.containingShadowRoot())
        return;

    auto* assignedNodes = slot.assignedNodes();
    if (!assignedNodes) {
        for (RefPtr<Node> child = slot.firstChild(); child; child = child->nextSibling()) {
            if (auto* slot = dynamicDowncast<HTMLSlotElement>(*child))
                flattenAssignedNodes(nodes, *slot);
            else if (is<Text>(*child) || is<Element>(*child))
                nodes.append(*child);
        }
        return;
    }
    for (auto& nodeWeakPtr : *assignedNodes) {
        auto* node = nodeWeakPtr.get();
        if (UNLIKELY(!node)) {
            ASSERT_NOT_REACHED();
            continue;
        }
        if (auto* slot = dynamicDowncast<HTMLSlotElement>(*node); slot && slot->containingShadowRoot())
            flattenAssignedNodes(nodes, *slot);
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

    if (auto* nodes = assignedNodes()) {
        return compactMap(*nodes, [](auto& nodeWeakPtr) -> RefPtr<Node> {
            return nodeWeakPtr.get();
        });
    }

    return { };
}

Vector<Ref<Element>> HTMLSlotElement::assignedElements(const AssignedNodesOptions& options) const
{
    return compactMap(assignedNodes(options), [](Ref<Node>&& node) -> RefPtr<Element> {
        return dynamicDowncast<Element>(WTFMove(node));
    });
}

void HTMLSlotElement::assign(FixedVector<ElementOrText>&& nodes)
{
    RefPtr shadowRoot = containingShadowRoot();
    RefPtr host = shadowRoot ? shadowRoot->host() : nullptr;
    for (auto& node : m_manuallyAssignedNodes) {
        if (RefPtr protectedNode = node.get())
            protectedNode->setManuallyAssignedSlot(nullptr);
    }

    auto previous = std::exchange(m_manuallyAssignedNodes, { });
    HashSet<RefPtr<Node>> seenNodes;
    m_manuallyAssignedNodes = WTF::compactMap(nodes, [&seenNodes](ElementOrText& node) -> std::optional<WeakPtr<Node, WeakPtrImplWithEventTargetData>> {
        auto mapper = [&seenNodes]<typename T>(RefPtr<T>& node) -> std::optional<WeakPtr<Node, WeakPtrImplWithEventTargetData>> {
            if (seenNodes.contains(node))
                return std::nullopt;
            seenNodes.add(node);
            return WeakPtr { node };
        };

        return WTF::switchOn(node,
            [&mapper](RefPtr<Element>& node) { return mapper(node); },
            [&mapper](RefPtr<Text>& node) { return mapper(node); }
        );
    });

    if (RefPtr shadowRoot = containingShadowRoot(); shadowRoot && shadowRoot->slotAssignmentMode() == SlotAssignmentMode::Manual)
        shadowRoot->slotManualAssignmentDidChange(*this, previous, m_manuallyAssignedNodes);
    else {
        for (auto& node : m_manuallyAssignedNodes) {
            if (auto previousSlot = node->manuallyAssignedSlot()) {
                previousSlot->removeManuallyAssignedNode(*node);
                if (RefPtr shadowRootOfPreviousSlot = previousSlot->containingShadowRoot(); shadowRootOfPreviousSlot && node->parentNode() == shadowRootOfPreviousSlot->host())
                    shadowRootOfPreviousSlot->didRemoveManuallyAssignedNode(*previousSlot, *node);
            }
            node->setManuallyAssignedSlot(this);
        }
    }
}

void HTMLSlotElement::removeManuallyAssignedNode(Node& node)
{
    m_manuallyAssignedNodes.removeFirst(&node);
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
    event->setTarget(Ref { *this });
    dispatchEvent(event);
}

}

