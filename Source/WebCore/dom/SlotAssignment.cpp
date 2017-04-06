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
    return value == nullAtom ? SlotAssignment::defaultSlotName() : value;
}

static const AtomicString& slotNameFromSlotAttribute(const Node& child)
{
    if (is<Text>(child))
        return SlotAssignment::defaultSlotName();

    return slotNameFromAttributeValue(downcast<Element>(child).attributeWithoutSynchronization(slotAttr));
}

SlotAssignment::SlotAssignment()
{
}

SlotAssignment::~SlotAssignment()
{
}

HTMLSlotElement* SlotAssignment::findAssignedSlot(const Node& node, ShadowRoot& shadowRoot)
{
    if (!is<Text>(node) && !is<Element>(node))
        return nullptr;

    auto slotName = slotNameForHostChild(node);
    auto it = m_slots.find(slotName);
    if (it == m_slots.end())
        return nullptr;

    return findFirstSlotElement(*it->value, shadowRoot);
}

void SlotAssignment::addSlotElementByName(const AtomicString& name, HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
#ifndef NDEBUG
    ASSERT(!m_slotElementsForConsistencyCheck.contains(&slotElement));
    m_slotElementsForConsistencyCheck.add(&slotElement);
#endif

    // FIXME: We should be able to do a targeted reconstruction.
    shadowRoot.host()->setNeedsStyleRecalc(ReconstructRenderTree);

    const AtomicString& slotName = slotNameFromAttributeValue(name);
    auto addResult = m_slots.add(slotName, std::unique_ptr<SlotInfo>());
    if (addResult.isNewEntry) {
        addResult.iterator->value = std::make_unique<SlotInfo>(slotElement);
        if (slotName == defaultSlotName()) // Because assignSlots doesn't collect nodes assigned to the default slot as an optimzation.
            m_slotAssignmentsIsValid = false;
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

void SlotAssignment::removeSlotElementByName(const AtomicString& name, HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
#ifndef NDEBUG
    ASSERT(m_slotElementsForConsistencyCheck.contains(&slotElement));
    m_slotElementsForConsistencyCheck.remove(&slotElement);
#endif

    if (auto* host = shadowRoot.host()) // FIXME: We should be able to do a targeted reconstruction.
        host->setNeedsStyleRecalc(ReconstructRenderTree);

    auto it = m_slots.find(slotNameFromAttributeValue(name));
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

static void recursivelyFireSlotChangeEvent(HTMLSlotElement& slotElement)
{
    slotElement.enqueueSlotChangeEvent();

    auto* slotParent = slotElement.parentElement();
    if (!slotParent)
        return;

    auto* shadowRootOfSlotParent = slotParent->shadowRoot();
    if (!shadowRootOfSlotParent)
        return;

    shadowRootOfSlotParent->innerSlotDidChange(slotElement.attributeWithoutSynchronization(slotAttr));
}

void SlotAssignment::didChangeSlot(const AtomicString& slotAttrValue, ChangeType changeType, ShadowRoot& shadowRoot)
{
    auto& slotName = slotNameFromAttributeValue(slotAttrValue);
    auto it = m_slots.find(slotName);
    if (it == m_slots.end())
        return;
    
    it->value->assignedNodes.clear();
    m_slotAssignmentsIsValid = false;

    HTMLSlotElement* slotElement = findFirstSlotElement(*it->value, shadowRoot);
    if (!slotElement)
        return;

    if (changeType == ChangeType::DirectChild)
        shadowRoot.host()->setNeedsStyleRecalc(ReconstructRenderTree);

    if (shadowRoot.mode() == ShadowRoot::Mode::UserAgent)
        return;

    recursivelyFireSlotChangeEvent(*slotElement);
}

void SlotAssignment::hostChildElementDidChange(const Element& childElement, ShadowRoot& shadowRoot)
{
    didChangeSlot(childElement.attributeWithoutSynchronization(slotAttr), ChangeType::DirectChild, shadowRoot);
}

const Vector<Node*>* SlotAssignment::assignedNodesForSlot(const HTMLSlotElement& slotElement, ShadowRoot& shadowRoot)
{
    ASSERT(slotElement.containingShadowRoot() == &shadowRoot);
    const AtomicString& slotName = slotNameFromAttributeValue(slotElement.attributeWithoutSynchronization(nameAttr));
    auto it = m_slots.find(slotName);
    RELEASE_ASSERT(it != m_slots.end());

    auto& slotInfo = *it->value;
    if (!m_slotAssignmentsIsValid)
        assignSlots(shadowRoot);

    if (!slotInfo.assignedNodes.size())
        return nullptr;

    RELEASE_ASSERT(slotInfo.hasSlotElements());
    if (slotInfo.hasDuplicatedSlotElements() && findFirstSlotElement(slotInfo, shadowRoot) != &slotElement)
        return nullptr;

    return &slotInfo.assignedNodes;
}

const AtomicString& SlotAssignment::slotNameForHostChild(const Node& child) const
{
    return slotNameFromSlotAttribute(child);
}

HTMLSlotElement* SlotAssignment::findFirstSlotElement(SlotInfo& slotInfo, ShadowRoot& shadowRoot)
{
    if (slotInfo.shouldResolveSlotElement())
        resolveAllSlotElements(shadowRoot);

#ifndef NDEBUG
    ASSERT(!slotInfo.element || m_slotElementsForConsistencyCheck.contains(slotInfo.element));
    ASSERT(!!slotInfo.element == !!slotInfo.elementCount);
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
        auto& slotName = slotNameFromAttributeValue(slotElement.attributeWithoutSynchronization(nameAttr));

        auto it = m_slots.find(slotName);
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

    auto addResult = m_slots.add(slotName, std::make_unique<SlotInfo>());
    addResult.iterator->value->assignedNodes.append(&child);
}

}


