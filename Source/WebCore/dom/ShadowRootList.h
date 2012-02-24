/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShadowRootList_h
#define ShadowRootList_h

#include "ShadowRoot.h"
#include <wtf/DoublyLinkedList.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class Node;
class Element;
class HTMLContentSelector;
class InsertionPoint;

class ShadowRootList {
public:
    ShadowRootList();
    ~ShadowRootList();

    bool hasShadowRoot() const;
    ShadowRoot* youngestShadowRoot() const;
    ShadowRoot* oldestShadowRoot() const;

    void pushShadowRoot(ShadowRoot*);
    ShadowRoot* popShadowRoot();

    void insertedIntoDocument();
    void removedFromDocument();
    void insertedIntoTree(bool deep);
    void removedFromTree(bool deep);
    void willRemove();

    void attach();
    void detach();
    void reattach();

    bool childNeedsStyleRecalc();
    bool needsStyleRecalc();
    void recalcShadowTreeStyle(Node::StyleChange);
    void setNeedsReattachHostChildrenAndShadow();
    void clearNeedsReattachHostChildrenAndShadow();
    bool needsReattachHostChildrenAndShadow();
    void reattachHostChildrenAndShadow();
    void hostChildrenChanged();

    InsertionPoint* insertionPointFor(Node*) const;

    HTMLContentSelector* selector() const;
    HTMLContentSelector* ensureSelector();

    bool isSelectorActive() const;

private:
    Element* host() const;

    DoublyLinkedList<ShadowRoot> m_shadowRoots;
    OwnPtr<HTMLContentSelector> m_selector;
    bool m_needsRecalculateContent : 1;
    WTF_MAKE_NONCOPYABLE(ShadowRootList);
};

inline bool ShadowRootList::hasShadowRoot() const
{
    return !m_shadowRoots.isEmpty();
}

inline ShadowRoot* ShadowRootList::youngestShadowRoot() const
{
    return m_shadowRoots.head();
}

inline ShadowRoot* ShadowRootList::oldestShadowRoot() const
{
    return m_shadowRoots.tail();
}

inline HTMLContentSelector* ShadowRootList::selector() const
{
    return m_selector.get();
}

inline void ShadowRootList::clearNeedsReattachHostChildrenAndShadow()
{
    m_needsRecalculateContent = false;
}

inline Element* ShadowRootList::host() const
{
    ASSERT(hasShadowRoot());
    return youngestShadowRoot()->host();
}

} // namespace

#endif
