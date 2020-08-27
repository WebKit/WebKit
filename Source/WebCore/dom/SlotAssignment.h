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

#pragma once

#include "RenderTreeUpdater.h"
#include "ShadowRoot.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class Element;
class HTMLSlotElement;
class Node;

class SlotAssignment {
    WTF_MAKE_NONCOPYABLE(SlotAssignment); WTF_MAKE_FAST_ALLOCATED;
public:
    SlotAssignment();
    virtual ~SlotAssignment();

    static const AtomString& defaultSlotName() { return emptyAtom(); }

    HTMLSlotElement* findAssignedSlot(const Node&, ShadowRoot&);

    void renameSlotElement(HTMLSlotElement&, const AtomString& oldName, const AtomString& newName, ShadowRoot&);
    void addSlotElementByName(const AtomString&, HTMLSlotElement&, ShadowRoot&);
    void removeSlotElementByName(const AtomString&, HTMLSlotElement&, ContainerNode* oldParentOfRemovedTreeForRemoval, ShadowRoot&);
    void slotFallbackDidChange(HTMLSlotElement&, ShadowRoot&);
    void resolveSlotsBeforeNodeInsertionOrRemoval(ShadowRoot&);
    void willRemoveAllChildren(ShadowRoot&);

    void didChangeSlot(const AtomString&, ShadowRoot&);
    void enqueueSlotChangeEvent(const AtomString&, ShadowRoot&);

    const Vector<WeakPtr<Node>>* assignedNodesForSlot(const HTMLSlotElement&, ShadowRoot&);

    virtual void hostChildElementDidChange(const Element&, ShadowRoot&);

private:
    struct Slot {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Slot() { }

        bool hasSlotElements() { return !!elementCount; }
        bool hasDuplicatedSlotElements() { return elementCount > 1; }
        bool shouldResolveSlotElement() { return !element && elementCount; }

        WeakPtr<HTMLSlotElement> element;
        WeakPtr<HTMLSlotElement> oldElement;
        unsigned elementCount { 0 };
        bool seenFirstElement { false };
        Vector<WeakPtr<Node>> assignedNodes;
    };

    bool hasAssignedNodes(ShadowRoot&, Slot&);
    enum class SlotMutationType { Insertion, Removal };
    void resolveSlotsAfterSlotMutation(ShadowRoot&, SlotMutationType, ContainerNode* oldParentOfRemovedTree = nullptr);

    virtual const AtomString& slotNameForHostChild(const Node&) const;

    HTMLSlotElement* findFirstSlotElement(Slot&, ShadowRoot&);
    void resolveAllSlotElements(ShadowRoot&);

    void assignSlots(ShadowRoot&);
    void assignToSlot(Node& child, const AtomString& slotName);

    HashMap<AtomString, std::unique_ptr<Slot>> m_slots;

#if ASSERT_ENABLED
    HashSet<HTMLSlotElement*> m_slotElementsForConsistencyCheck;
#endif

    bool m_needsToResolveSlotElements { false };
    bool m_slotAssignmentsIsValid { false };
    bool m_willBeRemovingAllChildren { false };
    unsigned m_slotMutationVersion { 0 };
    unsigned m_slotResolutionVersion { 0 };
};

inline void ShadowRoot::resolveSlotsBeforeNodeInsertionOrRemoval()
{
    if (UNLIKELY(shouldFireSlotchangeEvent() && m_slotAssignment))
        m_slotAssignment->resolveSlotsBeforeNodeInsertionOrRemoval(*this);
}

inline void ShadowRoot::willRemoveAllChildren(ContainerNode&)
{
    if (UNLIKELY(shouldFireSlotchangeEvent() && m_slotAssignment))
        m_slotAssignment->willRemoveAllChildren(*this);
}

inline void ShadowRoot::didRemoveAllChildrenOfShadowHost()
{
    if (m_slotAssignment) // FIXME: This is incorrect when there were no elements or text nodes removed.
        m_slotAssignment->didChangeSlot(nullAtom(), *this);
}

inline void ShadowRoot::didChangeDefaultSlot()
{
    if (m_slotAssignment)
        m_slotAssignment->didChangeSlot(nullAtom(), *this);
}

inline void ShadowRoot::hostChildElementDidChange(const Element& childElement)
{
    if (m_slotAssignment)
        m_slotAssignment->hostChildElementDidChange(childElement, *this);
}

inline void ShadowRoot::hostChildElementDidChangeSlotAttribute(Element& element, const AtomString& oldValue, const AtomString& newValue)
{
    if (!m_slotAssignment)
        return;
    m_slotAssignment->didChangeSlot(oldValue, *this);
    m_slotAssignment->didChangeSlot(newValue, *this);
    RenderTreeUpdater::tearDownRenderers(element);
}

} // namespace WebCore
