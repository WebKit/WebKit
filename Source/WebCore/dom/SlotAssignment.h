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

#ifndef SlotAssignment_h
#define SlotAssignment_h

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)

#include "ShadowRoot.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

class Element;
class HTMLSlotElement;
class Node;

class SlotAssignment {
    WTF_MAKE_NONCOPYABLE(SlotAssignment); WTF_MAKE_FAST_ALLOCATED;
public:
    SlotAssignment();
    virtual ~SlotAssignment();

    static const AtomicString& defaultSlotName() { return emptyAtom; }

    HTMLSlotElement* findAssignedSlot(const Node&, ShadowRoot&);

    void addSlotElementByName(const AtomicString&, HTMLSlotElement&, ShadowRoot&);
    void removeSlotElementByName(const AtomicString&, HTMLSlotElement&, ShadowRoot&);

    enum class ChangeType { DirectChild, InnerSlot };
    void didChangeSlot(const AtomicString&, ChangeType, ShadowRoot&);
    void enqueueSlotChangeEvent(const AtomicString&, ShadowRoot&);

    const Vector<Node*>* assignedNodesForSlot(const HTMLSlotElement&, ShadowRoot&);

    virtual void hostChildElementDidChange(const Element&, ShadowRoot&);

private:
    struct SlotInfo {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        SlotInfo() { }
        SlotInfo(HTMLSlotElement& slotElement)
            : element(&slotElement)
            , elementCount(1)
        { }

        bool hasSlotElements() { return !!elementCount; }
        bool hasDuplicatedSlotElements() { return elementCount > 1; }
        bool shouldResolveSlotElement() { return !element && elementCount; }

        HTMLSlotElement* element { nullptr };
        unsigned elementCount { 0 };
        Vector<Node*> assignedNodes;
    };
    
    virtual const AtomicString& slotNameForHostChild(const Node&) const;

    HTMLSlotElement* findFirstSlotElement(SlotInfo&, ShadowRoot&);
    void resolveAllSlotElements(ShadowRoot&);

    void assignSlots(ShadowRoot&);
    void assignToSlot(Node& child, const AtomicString& slotName);

    HashMap<AtomicString, std::unique_ptr<SlotInfo>> m_slots;

#ifndef NDEBUG
    HashSet<HTMLSlotElement*> m_slotElementsForConsistencyCheck;
    bool m_needsToResolveSlotElements { false };
#endif

    bool m_slotAssignmentsIsValid { false };
};

inline void ShadowRoot::didRemoveAllChildrenOfShadowHost()
{
    if (m_slotAssignment) // FIXME: This is incorrect when there were no elements or text nodes removed.
        m_slotAssignment->didChangeSlot(nullAtom, SlotAssignment::ChangeType::DirectChild, *this);
}

inline void ShadowRoot::didChangeDefaultSlot()
{
    if (m_slotAssignment)
        m_slotAssignment->didChangeSlot(nullAtom, SlotAssignment::ChangeType::DirectChild, *this);
}

inline void ShadowRoot::hostChildElementDidChange(const Element& childElement)
{
    if (m_slotAssignment)
        m_slotAssignment->hostChildElementDidChange(childElement, *this);
}

inline void ShadowRoot::hostChildElementDidChangeSlotAttribute(const AtomicString& oldValue, const AtomicString& newValue)
{
    if (m_slotAssignment) {
        m_slotAssignment->didChangeSlot(oldValue, SlotAssignment::ChangeType::DirectChild, *this);
        m_slotAssignment->didChangeSlot(newValue, SlotAssignment::ChangeType::DirectChild, *this);
    }
}

inline void ShadowRoot::innerSlotDidChange(const AtomicString& name)
{
    if (m_slotAssignment)
        m_slotAssignment->didChangeSlot(name, SlotAssignment::ChangeType::InnerSlot, *this);
}

}

#endif

#endif /* SlotAssignment_h */
