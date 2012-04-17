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

#ifndef ShadowTree_h
#define ShadowTree_h

#include "ExceptionCode.h"
#include "HTMLContentSelector.h"
#include "ShadowRoot.h"
#include <wtf/DoublyLinkedList.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Node;
class Element;
class InsertionPoint;
class TreeScope;
class HTMLContentSelection;

class ShadowTree {
public:
    ShadowTree();
    ~ShadowTree();

    Element* host() const;

    bool hasShadowRoot() const;
    ShadowRoot* youngestShadowRoot() const;
    ShadowRoot* oldestShadowRoot() const;

    void addShadowRoot(Element* shadowHost, PassRefPtr<ShadowRoot>, ExceptionCode&);
    void removeAllShadowRoots();

    void willRemove();

    void setParentTreeScope(TreeScope*);

    void attach();
    void detach();
    void reattach();
    void attachHost(Element*);
    void detachHost(Element*);

    bool childNeedsStyleRecalc();
    bool needsStyleRecalc();
    void recalcShadowTreeStyle(Node::StyleChange);
    void setNeedsReattachHostChildrenAndShadow();
    void clearNeedsReattachHostChildrenAndShadow();
    bool needsReattachHostChildrenAndShadow();
    void reattachHostChildrenAndShadow();
    void hostChildrenChanged();

    InsertionPoint* insertionPointFor(const Node*) const;
    HTMLContentSelection* selectionFor(const Node*) const;

    HTMLContentSelector& selector();
    const HTMLContentSelector& selector() const;

private:

    DoublyLinkedList<ShadowRoot> m_shadowRoots;
    HTMLContentSelector m_selector;
    bool m_needsRecalculateContent : 1;
    WTF_MAKE_NONCOPYABLE(ShadowTree);
};

inline bool ShadowTree::hasShadowRoot() const
{
    return !m_shadowRoots.isEmpty();
}

inline ShadowRoot* ShadowTree::youngestShadowRoot() const
{
    return m_shadowRoots.head();
}

inline ShadowRoot* ShadowTree::oldestShadowRoot() const
{
    return m_shadowRoots.tail();
}

inline HTMLContentSelector& ShadowTree::selector()
{
    return m_selector;
}

inline const HTMLContentSelector& ShadowTree::selector() const
{
    return m_selector;
}

inline void ShadowTree::clearNeedsReattachHostChildrenAndShadow()
{
    m_needsRecalculateContent = false;
}

inline Element* ShadowTree::host() const
{
    ASSERT(hasShadowRoot());
    return youngestShadowRoot()->host();
}

class ShadowRootVector : public Vector<RefPtr<ShadowRoot> > {
public:
    explicit ShadowRootVector(ShadowTree* tree)
    {
        for (ShadowRoot* root = tree->youngestShadowRoot(); root; root = root->olderShadowRoot())
            append(root);
    }
};

} // namespace

#endif
