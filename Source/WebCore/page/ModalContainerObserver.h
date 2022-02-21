/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Timer.h"
#include <wtf/FastMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class Element;
class FrameView;
class HTMLElement;
class HTMLFrameOwnerElement;

class ModalContainerObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static bool isNeededFor(const Document&);

    ModalContainerObserver();
    ~ModalContainerObserver();

    bool shouldMakeVerticallyScrollable(const Element&) const;
    inline bool shouldHide(const Element&) const;
    void updateModalContainerIfNeeded(const FrameView&);

    inline void overrideSearchTermForTesting(const String&);

private:
    friend class ModalContainerPolicyDecisionScope;

    void scheduleClickableElementCollection();
    void collectClickableElementsTimerFired();
    void revealModalContainer();
    void setContainer(Element&, HTMLFrameOwnerElement* = nullptr);
    void searchForModalContainerOnBehalfOfFrameOwnerIfNeeded(HTMLFrameOwnerElement&);

    void makeBodyAndDocumentElementScrollableIfNeeded();
    void clearScrollabilityOverrides(Document&);

    Element* container() const;
    HTMLFrameOwnerElement* frameOwnerForControls() const;

    std::pair<Vector<WeakPtr<HTMLElement>>, Vector<String>> collectClickableElements();
    void hideUserInteractionBlockingElementIfNeeded();

    WeakHashSet<Element> m_elementsToIgnoreWhenSearching;
    std::pair<WeakPtr<Element>, WeakPtr<HTMLFrameOwnerElement>> m_containerAndFrameOwnerForControls;
    WeakHashMap<HTMLFrameOwnerElement, WeakPtr<Element>> m_frameOwnersAndContainersToSearchAgain;
    WeakPtr<Element> m_userInteractionBlockingElement;
    AtomString m_overrideSearchTermForTesting;
    Timer m_collectClickableElementsTimer;
    bool m_collectingClickableElements { false };
    bool m_hasAttemptedToFulfillPolicy { false };
    bool m_makeBodyElementScrollable { false };
    bool m_makeDocumentElementScrollable { false };
};

inline void ModalContainerObserver::overrideSearchTermForTesting(const String& searchTerm)
{
    m_overrideSearchTermForTesting = searchTerm;
}

inline bool ModalContainerObserver::shouldHide(const Element& element) const
{
    if (m_collectingClickableElements)
        return false;

    return m_userInteractionBlockingElement == &element || container() == &element;
}

} // namespace WebCore
