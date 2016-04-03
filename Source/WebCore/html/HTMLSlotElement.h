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

#ifndef HTMLSlotElement_h
#define HTMLSlotElement_h

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)

#include "HTMLElement.h"
#include "Range.h"

namespace WebCore {

class HTMLSlotElement final : public HTMLElement {
public:
    static Ref<HTMLSlotElement> create(const QualifiedName&, Document&);

    const Vector<Node*>* assignedNodes() const;

    void enqueueSlotChangeEvent();

private:
    HTMLSlotElement(const QualifiedName&, Document&);

    InsertionNotificationRequest insertedInto(ContainerNode&) override;
    void removedFrom(ContainerNode&) override;
    void attributeChanged(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason) override;

    bool dispatchEvent(Event&) override;

    bool m_hasEnqueuedSlotChangeEvent { false };
};

// Slots have implicit display:contents until it is supported for reals.
inline bool hasImplicitDisplayContents(const Element& element) { return is<HTMLSlotElement>(element); }

}

#else

namespace WebCore {

inline bool hasImplicitDisplayContents(const Element&) { return false; }

}

#endif
#endif
