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
#include "SlotAssignment.h"

#include "ElementInlines.h"
#include "HTMLSlotElement.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIterator.h"

namespace WebCore {

using namespace HTMLNames;

static const AtomString& slotNameFromAttributeValue(const AtomString& value)
{
    return value == nullAtom() ? SlotAssignment::defaultSlotName() : value;
}

static const AtomString& slotNameFromSlotAttribute(const Node& child)
{
    if (is<Text>(child))
        return SlotAssignment::defaultSlotName();

    return slotNameFromAttributeValue(downcast<Element>(child).attributeWithoutSynchronization(slotAttr));
}

#if ASSERT_ENABLED
static HTMLSlotElement* findSlotElement(ShadowRoot& shadowRoot, const AtomString& slotName)
{
    for (auto& slotElement : descendantsOfType<HTMLSlotElement>(shadowRoot)) {
        if (slotNameFromAttributeValue(slotElement.attributeWithoutSynchronization(nameAttr)) == slotName)
            return &slotElement;
    }
    return nullptr;
}
#endif // ASSERT_ENABLED

static HTMLSlotElement* nextSlotElementSkippingSubtree(ContainerNode& startingNode, ContainerNode* skippedSubtree)
{
    Node* node = &startingNode;
    do {
        if (UNLIKELY(node == skippedSubtree))
            node = NodeTraversal::nextSkippingChildren(*node);
        else
            node = NodeTraversal::next(*node);
    } while (node && !is<HTMLSlotElement>(node));
    return downcast<HTMLSlotElement>(node);
}

SlotAssignment::SlotAssignment() = default;

SlotAssignment::~SlotAssignment() = default;

HTMLSlotElement* SlotAssignment::findAssignedSlot(const Node& node)
{
    if (!is<Text>(node) && !is<Element>(node))
        return nullptr;

    auto* slot = m_slots.get(slotNameForHostChild(node));
    if (!slot)
        return nullptr;

    return findFirstSlotElement(*slot);
}

inline bool SlotAssignment::hasAssignedNodes(ShadowRoot& shadowRoot, Slot& slot)
{
    if (!m_slotAssignmentsIsValid)
        assignSlots(shadowRoot);
    return !slot.assignedNodes.isEmpty();
}

void SlotAssignment::renameSlotElement(HTMLSlotElement& slotElement, const AtomString& oldName, const AtomString& newName, ShadowRoot& shadowRoot)
{
    ASSERT(m_slotElementsForConsistencyCheck.contains(&slotElement));

    m_slotMutationVersion++;

    removeSlotElementByName(oldName, slotElement, nullptr, shadowRoot);
    addSlotElementByName(newName, slotElement, shadowRoot);
}

void SlotAssignment::addSlotElementByName(const AtomString& name, HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
#if ASSERT_ENABLED
    ASSERT(!m_slotElementsForConsistencyCheck.contains(&slotElement));
    m_slotElementsForConsistencyCheck.add(&slotElement);
#endif

    // FIXME: We should be able to do a targeted reconstruction.
    shadowRoot.host()->invalidateStyleAndRenderersForSubtree();

    if (!m_slotElementCount)
        shadowRoot.host()->setHasShadowRootContainingSlots(true);
    m_slotElementCount++;

    auto& slotName = slotNameFromAttributeValue(name);
    auto addResult = m_slots.ensure(slotName, [&] {
        m_slotAssignmentsIsValid = false;
        return makeUnique<Slot>();
    });
    auto& slot = *addResult.iterator->value;

    if (!m_slotAssignmentsIsValid)
        assignSlots(shadowRoot);

    slot.elementCount++;
    if (slot.elementCount == 1) {
        slot.element = slotElement;
        if (shadowRoot.shouldFireSlotchangeEvent() && hasAssignedNodes(shadowRoot, slot))
            slotElement.enqueueSlotChangeEvent();
        return;
    }

    resolveSlotsAfterSlotMutation(shadowRoot, SlotMutationType::Insertion);
}

void SlotAssignment::removeSlotElementByName(const AtomString& name, HTMLSlotElement& slotElement, ContainerNode* oldParentOfRemovedTreeForRemoval, ShadowRoot& shadowRoot)
{
#if ASSERT_ENABLED
    ASSERT(m_slotElementsForConsistencyCheck.contains(&slotElement));
    m_slotElementsForConsistencyCheck.remove(&slotElement);
#endif

    ASSERT(m_slotElementCount > 0);
    m_slotElementCount--;

    if (RefPtr host = shadowRoot.host()) {
        // FIXME: We should be able to do a targeted reconstruction.
        host->invalidateStyleAndRenderersForSubtree();
        if (!m_slotElementCount)
            host->setHasShadowRootContainingSlots(false);
    }

    auto* slot = m_slots.get(slotNameFromAttributeValue(name));
    RELEASE_ASSERT(slot && slot->hasSlotElements());

    slot->elementCount--;
    if (!slot->elementCount) {
        slot->element = nullptr;
        bool hasNotResolvedAllSlots = m_slotResolutionVersion != m_slotMutationVersion;
        if (shadowRoot.shouldFireSlotchangeEvent() && hasAssignedNodes(shadowRoot, *slot) && hasNotResolvedAllSlots)
            slotElement.enqueueSlotChangeEvent();
        return;
    }

    bool elementWasRenamed = !oldParentOfRemovedTreeForRemoval;
    if (elementWasRenamed && slot->element == &slotElement)
        slotElement.enqueueSlotChangeEvent();

    if (slot->element) {
        resolveSlotsAfterSlotMutation(shadowRoot, elementWasRenamed ? SlotMutationType::Insertion : SlotMutationType::Removal,
            m_willBeRemovingAllChildren ? oldParentOfRemovedTreeForRemoval : nullptr);
    } else {
        // A previous invocation to resolveSlotsAfterSlotMutation during this removal has updated this slot.
        ASSERT(m_slotResolutionVersion == m_slotMutationVersion && !findSlotElement(shadowRoot, name));
    }

    if (slot->oldElement == &slotElement) {
        ASSERT(shadowRoot.shouldFireSlotchangeEvent());
        slotElement.enqueueSlotChangeEvent();
        slot->oldElement = nullptr;
    }
}

void SlotAssignment::resolveSlotsAfterSlotMutation(ShadowRoot& shadowRoot, SlotMutationType mutationType, ContainerNode* subtreeToSkip)
{
    if (m_slotResolutionVersion == m_slotMutationVersion)
        return;
    m_slotResolutionVersion = m_slotMutationVersion;

    ASSERT(!subtreeToSkip || mutationType == SlotMutationType::Removal);

    for (auto& slot : m_slots.values())
        slot->seenFirstElement = false;

    unsigned slotCount = 0;
    HTMLSlotElement* currentElement = nextSlotElementSkippingSubtree(shadowRoot, subtreeToSkip);
    for (; currentElement; currentElement = nextSlotElementSkippingSubtree(*currentElement, subtreeToSkip)) {
        auto& currentSlotName = slotNameFromAttributeValue(currentElement->attributeWithoutSynchronization(nameAttr));
        auto* currentSlot = m_slots.get(currentSlotName);
        if (!currentSlot) {
            // A new slot may have been inserted with this node but appears later in the tree order.
            // Such a slot would go through the fast path in addSlotElementByName,
            // and any subsequently inserted slot of the same name would not result in any slotchange or invokation of this function.
            ASSERT(mutationType == SlotMutationType::Insertion);
            continue;
        }
        if (currentSlot->seenFirstElement) {
            if (mutationType == SlotMutationType::Insertion && currentSlot->oldElement == currentElement) {
                ASSERT(shadowRoot.shouldFireSlotchangeEvent());
                currentElement->enqueueSlotChangeEvent();
                currentSlot->oldElement = nullptr;
            }
            continue;
        }
        currentSlot->seenFirstElement = true;
        slotCount++;
        if (currentSlot->element != currentElement) {
            if (shadowRoot.shouldFireSlotchangeEvent() && hasAssignedNodes(shadowRoot, *currentSlot)) {
                currentSlot->oldElement = WTFMove(currentSlot->element);
                currentElement->enqueueSlotChangeEvent();
            }
            currentSlot->element = *currentElement;
        }
    }

    if (slotCount == m_slots.size())
        return;

    if (mutationType == SlotMutationType::Insertion) {
        // This code path is taken only when continue above for !currentSlot is taken.
        // i.e. there is a new slot being inserted into the tree but we have yet to invoke addSlotElementByName on it.
#if ASSERT_ENABLED
        for (auto& entry : m_slots)
            ASSERT(entry.value->seenFirstElement || !findSlotElement(shadowRoot, entry.key));
#endif
        return;
    }

    for (auto& slot : m_slots.values()) {
        if (slot->seenFirstElement)
            continue;
        if (!slot->elementCount) {
            // Taken the fast path for removal.
            ASSERT(!slot->element);
            continue;
        }
        // All slot elements have been removed for this slot.
        slot->seenFirstElement = true;
        ASSERT(slot->element);
        if (hasAssignedNodes(shadowRoot, *slot))
            slot->oldElement = WTFMove(slot->element);
        slot->element = nullptr;
    }
}

void SlotAssignment::slotFallbackDidChange(HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
    if (shadowRoot.mode() == ShadowRootMode::UserAgent)
        return;

    bool usesFallbackContent = !assignedNodesForSlot(slotElement, shadowRoot);
    if (usesFallbackContent)
        slotElement.enqueueSlotChangeEvent();
}

void SlotAssignment::didChangeSlot(const AtomString& slotAttrValue, ShadowRoot& shadowRoot)
{
    auto& slotName = slotNameFromAttributeValue(slotAttrValue);
    auto* slot = m_slots.get(slotName);
    if (!slot)
        return;

    RenderTreeUpdater::tearDownRenderersAfterSlotChange(*shadowRoot.host());
    shadowRoot.host()->invalidateStyleForSubtree();

    slot->assignedNodes.clear();
    m_slotAssignmentsIsValid = false;

    RefPtr slotElement { findFirstSlotElement(*slot) };
    if (!slotElement)
        return;

    if (shadowRoot.shouldFireSlotchangeEvent())
        slotElement->enqueueSlotChangeEvent();
}

void SlotAssignment::hostChildElementDidChange(const Element& childElement, ShadowRoot& shadowRoot)
{
    didChangeSlot(childElement.attributeWithoutSynchronization(slotAttr), shadowRoot);
}

const Vector<WeakPtr<Node>>* SlotAssignment::assignedNodesForSlot(const HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
    ASSERT(slotElement.containingShadowRoot() == &shadowRoot);
    const AtomString& slotName = slotNameFromAttributeValue(slotElement.attributeWithoutSynchronization(nameAttr));
    auto* slot = m_slots.get(slotName);

    bool hasNotAddedSlotInInsertedIntoAncestorYet = shadowRoot.isConnected() && (!slotElement.isConnected() || slotElement.isInInsertedIntoAncestor());
    if (hasNotAddedSlotInInsertedIntoAncestorYet)
        return nullptr;
    RELEASE_ASSERT(slot);

    if (!m_slotAssignmentsIsValid)
        assignSlots(shadowRoot);

    if (slot->assignedNodes.isEmpty())
        return nullptr;

    RELEASE_ASSERT(slot->hasSlotElements());
    if (slot->hasDuplicatedSlotElements() && findFirstSlotElement(*slot) != &slotElement)
        return nullptr;

    return &slot->assignedNodes;
}

void SlotAssignment::willRemoveAssignedNode(const Node& node)
{
    if (!m_slotAssignmentsIsValid)
        return;

    if (!is<Text>(node) && !is<Element>(node))
        return;

    auto* slot = m_slots.get(slotNameForHostChild(node));
    if (!slot || slot->assignedNodes.isEmpty())
        return;

    slot->assignedNodes.removeFirstMatching([&node](const auto& item) {
        return item.get() == &node;
    });
}

const AtomString& SlotAssignment::slotNameForHostChild(const Node& child) const
{
    return slotNameFromSlotAttribute(child);
}

HTMLSlotElement* SlotAssignment::findFirstSlotElement(Slot& slot)
{
    ASSERT(!slot.element || m_slotElementsForConsistencyCheck.contains(slot.element.get()));

    return slot.element.get();
}

void SlotAssignment::assignSlots(ShadowRoot& shadowRoot)
{
    ASSERT(!m_slotAssignmentsIsValid);
    m_slotAssignmentsIsValid = true;

    for (auto& entry : m_slots)
        entry.value->assignedNodes.shrink(0);

    if (auto* host = shadowRoot.host()) {
        for (auto* child = host->firstChild(); child; child = child->nextSibling()) {
            if (!is<Text>(*child) && !is<Element>(*child))
                continue;
            auto slotName = slotNameForHostChild(*child);
            assignToSlot(*child, slotName);
        }
    }

    for (auto& entry : m_slots)
        entry.value->assignedNodes.shrinkToFit();
}

void SlotAssignment::assignToSlot(Node& child, const AtomString& slotName)
{
    ASSERT(!slotName.isNull());
    if (slotName == defaultSlotName()) {
        auto defaultSlotEntry = m_slots.find(defaultSlotName());
        if (defaultSlotEntry != m_slots.end())
            defaultSlotEntry->value->assignedNodes.append(child);
        return;
    }

    auto addResult = m_slots.ensure(slotName, [] {
        return makeUnique<Slot>();
    });
    addResult.iterator->value->assignedNodes.append(child);
}

}


