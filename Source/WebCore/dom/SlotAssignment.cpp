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


#include "HTMLSlotElement.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIterator.h"

namespace WebCore {

using namespace HTMLNames;

static const AtomicString& slotNameFromAttributeValue(const AtomicString& value)
{
    return value == nullAtom() ? SlotAssignment::defaultSlotName() : value;
}

static const AtomicString& slotNameFromSlotAttribute(const Node& child)
{
    if (is<Text>(child))
        return SlotAssignment::defaultSlotName();

    return slotNameFromAttributeValue(downcast<Element>(child).attributeWithoutSynchronization(slotAttr));
}

SlotAssignment::SlotAssignment() = default;

SlotAssignment::~SlotAssignment() = default;

HTMLSlotElement* SlotAssignment::findAssignedSlot(const Node& node, ShadowRoot& shadowRoot)
{
    if (!is<Text>(node) && !is<Element>(node))
        return nullptr;

    auto* slot = m_slots.get(slotNameForHostChild(node));
    if (!slot)
        return nullptr;

    return findFirstSlotElement(*slot, shadowRoot);
}

void SlotAssignment::addSlotElementByName(const AtomicString& name, HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
#ifndef NDEBUG
    ASSERT(!m_slotElementsForConsistencyCheck.contains(&slotElement));
    m_slotElementsForConsistencyCheck.add(&slotElement);
#endif

    // FIXME: We should be able to do a targeted reconstruction.
    shadowRoot.host()->invalidateStyleAndRenderersForSubtree();

    const AtomicString& slotName = slotNameFromAttributeValue(name);
    auto addResult = m_slots.ensure(slotName, [&] {
        // Unlike named slots, assignSlots doesn't collect nodes assigned to the default slot
        // to avoid always having a vector of all child nodes of a shadow host.
        if (slotName == defaultSlotName())
            m_slotAssignmentsIsValid = false;

        return std::make_unique<Slot>();
    });

    auto& slot = *addResult.iterator->value;
    if (!slot.hasSlotElements())
        slot.element = makeWeakPtr(slotElement);
    else {
        slot.element = nullptr;
#ifndef NDEBUG
        m_needsToResolveSlotElements = true;
#endif
    }
    slot.elementCount++;
}

void SlotAssignment::removeSlotElementByName(const AtomicString& name, HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
#ifndef NDEBUG
    ASSERT(m_slotElementsForConsistencyCheck.contains(&slotElement));
    m_slotElementsForConsistencyCheck.remove(&slotElement);
#endif

    if (auto* host = shadowRoot.host()) // FIXME: We should be able to do a targeted reconstruction.
        host->invalidateStyleAndRenderersForSubtree();

    auto* slot = m_slots.get(slotNameFromAttributeValue(name));
    RELEASE_ASSERT(slot && slot->hasSlotElements());

    slot->elementCount--;
    if (slot->element == &slotElement) {
        slot->element = nullptr;
#ifndef NDEBUG
        m_needsToResolveSlotElements = true;
#endif
    }
    ASSERT(slot->element || m_needsToResolveSlotElements);
}

void SlotAssignment::slotFallbackDidChange(HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
    if (shadowRoot.mode() == ShadowRootMode::UserAgent)
        return;

    bool usesFallbackContent = !assignedNodesForSlot(slotElement, shadowRoot);
    if (usesFallbackContent)
        slotElement.enqueueSlotChangeEvent();
}

void SlotAssignment::didChangeSlot(const AtomicString& slotAttrValue, ShadowRoot& shadowRoot)
{
    auto& slotName = slotNameFromAttributeValue(slotAttrValue);
    auto* slot = m_slots.get(slotName);
    if (!slot)
        return;
    
    slot->assignedNodes.clear();
    m_slotAssignmentsIsValid = false;

    auto slotElement = makeRefPtr(findFirstSlotElement(*slot, shadowRoot));
    if (!slotElement)
        return;

    shadowRoot.host()->invalidateStyleAndRenderersForSubtree();

    if (shadowRoot.mode() == ShadowRootMode::UserAgent)
        return;

    slotElement->enqueueSlotChangeEvent();
}

void SlotAssignment::hostChildElementDidChange(const Element& childElement, ShadowRoot& shadowRoot)
{
    didChangeSlot(childElement.attributeWithoutSynchronization(slotAttr), shadowRoot);
}

const Vector<Node*>* SlotAssignment::assignedNodesForSlot(const HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
    ASSERT(slotElement.containingShadowRoot() == &shadowRoot);
    const AtomicString& slotName = slotNameFromAttributeValue(slotElement.attributeWithoutSynchronization(nameAttr));
    auto* slot = m_slots.get(slotName);
    RELEASE_ASSERT(slot);

    if (!m_slotAssignmentsIsValid)
        assignSlots(shadowRoot);

    if (slot->assignedNodes.isEmpty())
        return nullptr;

    RELEASE_ASSERT(slot->hasSlotElements());
    if (slot->hasDuplicatedSlotElements() && findFirstSlotElement(*slot, shadowRoot) != &slotElement)
        return nullptr;

    return &slot->assignedNodes;
}

const AtomicString& SlotAssignment::slotNameForHostChild(const Node& child) const
{
    return slotNameFromSlotAttribute(child);
}

HTMLSlotElement* SlotAssignment::findFirstSlotElement(Slot& slot, ShadowRoot& shadowRoot)
{
    if (slot.shouldResolveSlotElement())
        resolveAllSlotElements(shadowRoot);

#ifndef NDEBUG
    ASSERT(!slot.element || m_slotElementsForConsistencyCheck.contains(slot.element.get()));
    ASSERT(!!slot.element == !!slot.elementCount);
#endif

    return slot.element.get();
}

void SlotAssignment::resolveAllSlotElements(ShadowRoot& shadowRoot)
{
#ifndef NDEBUG
    ASSERT(m_needsToResolveSlotElements);
    m_needsToResolveSlotElements = false;
#endif

    // FIXME: It's inefficient to reset all values. We should be able to void this in common case.
    for (auto& entry : m_slots)
        entry.value->element = nullptr;

    unsigned slotCount = m_slots.size();
    for (auto& slotElement : descendantsOfType<HTMLSlotElement>(shadowRoot)) {
        auto& slotName = slotNameFromAttributeValue(slotElement.attributeWithoutSynchronization(nameAttr));

        auto* slot = m_slots.get(slotName);
        RELEASE_ASSERT(slot); // slot must have been created when a slot was inserted.

        bool hasSeenSlotWithSameName = !!slot->element;
        if (hasSeenSlotWithSameName)
            continue;

        slot->element = makeWeakPtr(slotElement);
        slotCount--;
        if (!slotCount)
            break;
    }
}

void SlotAssignment::assignSlots(ShadowRoot& shadowRoot)
{
    ASSERT(!m_slotAssignmentsIsValid);
    m_slotAssignmentsIsValid = true;

    for (auto& entry : m_slots)
        entry.value->assignedNodes.shrink(0);

    auto& host = *shadowRoot.host();
    for (auto* child = host.firstChild(); child; child = child->nextSibling()) {
        if (!is<Text>(*child) && !is<Element>(*child))
            continue;
        auto slotName = slotNameForHostChild(*child);
        assignToSlot(*child, slotName);
    }

    for (auto& entry : m_slots)
        entry.value->assignedNodes.shrinkToFit();
}

void SlotAssignment::assignToSlot(Node& child, const AtomicString& slotName)
{
    ASSERT(!slotName.isNull());
    if (slotName == defaultSlotName()) {
        auto defaultSlotEntry = m_slots.find(defaultSlotName());
        if (defaultSlotEntry != m_slots.end())
            defaultSlotEntry->value->assignedNodes.append(&child);
        return;
    }

    auto addResult = m_slots.ensure(slotName, [] {
        return std::make_unique<Slot>();
    });
    addResult.iterator->value->assignedNodes.append(&child);
}

}


