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

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

class HTMLSlotElement;
class Node;
class ShadowRoot;

class SlotAssignment {
    WTF_MAKE_NONCOPYABLE(SlotAssignment);
public:
    using SlotNameFunction = std::function<AtomicString (const Node& child)>;

    SlotAssignment();
    SlotAssignment(SlotNameFunction);
    ~SlotAssignment() { }

    static const AtomicString& defaultSlotName() { return emptyAtom; }

    HTMLSlotElement* findAssignedSlot(const Node&, ShadowRoot&);

    void addSlotElementByName(const AtomicString&, HTMLSlotElement&, ShadowRoot&);
    void removeSlotElementByName(const AtomicString&, HTMLSlotElement&, ShadowRoot&);

    const Vector<Node*>* assignedNodesForSlot(const HTMLSlotElement&, ShadowRoot&);

    void invalidate(ShadowRoot&);
    void invalidateDefaultSlot(ShadowRoot&);

private:
    struct SlotInfo {
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

    HTMLSlotElement* findFirstSlotElement(SlotInfo&, ShadowRoot&);
    void resolveAllSlotElements(ShadowRoot&);

    void assignSlots(ShadowRoot&);
    void assignToSlot(Node& child, const AtomicString& slotName);

    SlotNameFunction m_slotNameFunction;

    HashMap<AtomicString, std::unique_ptr<SlotInfo>> m_slots;

#ifndef NDEBUG
    HashSet<HTMLSlotElement*> m_slotElementsForConsistencyCheck;
    bool m_needsToResolveSlotElements { false };
#endif

    bool m_slotAssignmentsIsValid { false };
};

}

#endif

#endif /* SlotAssignment_h */
