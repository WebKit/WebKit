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

#if ENABLE(SHADOW_DOM)

#include "HTMLSlotElement.h"
#include "TypedElementDescendantIterator.h"

namespace WebCore {

using namespace HTMLNames;

static const AtomicString& treatNullAsEmpty(const AtomicString& name)
{
    return name == nullAtom ? emptyAtom : name;
}

HTMLSlotElement* SlotAssignment::findAssignedSlot(const Node& node, ShadowRoot& shadowRoot)
{
    const AtomicString& name = is<Element>(node) ? downcast<Element>(node).fastGetAttribute(slotAttr) : emptyAtom;

    auto it = m_slots.find(treatNullAsEmpty(name));
    if (it == m_slots.end())
        return nullptr;

    return findFirstSlotElement(*it->value, shadowRoot);
}

void SlotAssignment::addSlotElementByName(const AtomicString& name, HTMLSlotElement& slotElement)
{
#ifndef NDEBUG
    ASSERT(!m_slotElementsForConsistencyCheck.contains(&slotElement));
    m_slotElementsForConsistencyCheck.add(&slotElement);
#endif

    auto addResult = m_slots.add(treatNullAsEmpty(name), std::unique_ptr<SlotInfo>());
    if (addResult.isNewEntry) {
        addResult.iterator->value = std::make_unique<SlotInfo>(slotElement);
        return;
    }

    auto& slotInfo = *addResult.iterator->value;

    if (!slotInfo.hasSlotElements())
        slotInfo.element = &slotElement;
    else {
        slotInfo.element = nullptr;
#ifndef NDEBUG
        m_needsToResolveSlotElements = true;
#endif
    }
    slotInfo.elementCount++;
}

void SlotAssignment::removeSlotElementByName(const AtomicString& name, HTMLSlotElement& slotElement)
{
#ifndef NDEBUG
    ASSERT(m_slotElementsForConsistencyCheck.contains(&slotElement));
    m_slotElementsForConsistencyCheck.remove(&slotElement);
#endif

    auto it = m_slots.find(treatNullAsEmpty(name));
    RELEASE_ASSERT(it != m_slots.end());

    auto& slotInfo = *it->value;
    RELEASE_ASSERT(slotInfo.hasSlotElements());

    slotInfo.elementCount--;
    if (slotInfo.element == &slotElement) {
        slotInfo.element = nullptr;
#ifndef NDEBUG
        m_needsToResolveSlotElements = true;
#endif
    }
    ASSERT(slotInfo.element || m_needsToResolveSlotElements);
}

const Vector<Node*>* SlotAssignment::assignedNodesForSlot(const HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
    if (!m_slotAssignmentsIsValid)
        assignSlots(shadowRoot);

    const AtomicString& slotName = slotElement.fastGetAttribute(nameAttr);
    auto it = m_slots.find(treatNullAsEmpty(slotName));
    if (it == m_slots.end())
        return nullptr;

    auto& slotInfo = *it->value;

    if (!slotInfo.assignedNodes.size())
        return nullptr;

    RELEASE_ASSERT(slotInfo.hasSlotElements());
    if (slotInfo.hasDuplicatedSlotElements() && findFirstSlotElement(slotInfo, shadowRoot) != &slotElement)
        return nullptr;

    return &slotInfo.assignedNodes;
}

HTMLSlotElement* SlotAssignment::findFirstSlotElement(SlotInfo& slotInfo, ShadowRoot& shadowRoot)
{
    if (slotInfo.shouldResolveSlotElement())
        resolveAllSlotElements(shadowRoot);

#ifndef NDEBUG
    ASSERT(!slotInfo.element || m_slotElementsForConsistencyCheck.contains(slotInfo.element));
#endif

    return slotInfo.element;
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
        const AtomicString& slotName = slotElement.fastGetAttribute(nameAttr);

        auto it = m_slots.find(treatNullAsEmpty(slotName));
        RELEASE_ASSERT(it != m_slots.end());

        SlotInfo& slotInfo = *it->value;
        bool hasSeenSlotWithSameName = !!slotInfo.element;
        if (hasSeenSlotWithSameName)
            continue;

        slotInfo.element = &slotElement;
        slotCount--;
        if (!slotCount)
            break;
    }
}

void SlotAssignment::assignSlots(ShadowRoot& shadowRoot)
{
    ASSERT(!m_slotAssignmentsIsValid);
    m_slotAssignmentsIsValid = true;

    auto* host = shadowRoot.host();
    RELEASE_ASSERT(host);

    for (auto& entry : m_slots)
        entry.value->assignedNodes.shrink(0);

    for (Node* child = host->firstChild(); child; child = child->nextSibling()) {
        if (is<Element>(child)) {
            auto& slotName = downcast<Element>(*child).fastGetAttribute(slotAttr);
            if (!slotName.isNull()) {
                auto addResult = m_slots.add(treatNullAsEmpty(slotName), std::make_unique<SlotInfo>());
                addResult.iterator->value->assignedNodes.append(child);
                continue;
            }
        }
        auto defaultSlotEntry = m_slots.find(emptyAtom);
        if (defaultSlotEntry != m_slots.end())
            defaultSlotEntry->value->assignedNodes.append(child);
    }

    for (auto& entry : m_slots)
        entry.value->assignedNodes.shrinkToFit();
}

}

#endif

