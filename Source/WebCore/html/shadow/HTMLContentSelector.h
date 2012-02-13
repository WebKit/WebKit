/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#ifndef HTMLContentSelector_h
#define HTMLContentSelector_h

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Element;
class HTMLContentElement;
class Node;
class ShadowRoot;

class HTMLContentSeleciton : public RefCounted<HTMLContentSeleciton> {
public:
    static PassRefPtr<HTMLContentSeleciton> create(HTMLContentElement*, Node*);

    HTMLContentElement* insertionPoint() const { return m_insertionPoint; }
    Node* node() const { return m_node.get(); }
    HTMLContentSeleciton* next() const { return m_next.get(); }
    HTMLContentSeleciton* previous() const { return m_previous.get(); }

    void append(PassRefPtr<HTMLContentSeleciton>);
    void unlink();

private:
    HTMLContentSeleciton(HTMLContentElement*, Node*);

    HTMLContentElement* m_insertionPoint;
    RefPtr<Node> m_node;
    RefPtr<HTMLContentSeleciton> m_next;
    RefPtr<HTMLContentSeleciton> m_previous;
};

inline HTMLContentSeleciton::HTMLContentSeleciton(HTMLContentElement* insertionPoint, Node* node)
    : m_insertionPoint(insertionPoint), m_node(node)
{ }

inline PassRefPtr<HTMLContentSeleciton> HTMLContentSeleciton::create(HTMLContentElement* insertionPoint, Node* node)
{
    return adoptRef(new HTMLContentSeleciton(insertionPoint, node));
}

class HTMLContentSelectionList {
public:
    HTMLContentSelectionList();
    ~HTMLContentSelectionList();

    HTMLContentSeleciton* first() const { return m_first.get(); }
    HTMLContentSeleciton* last() const { return m_last.get(); }
    HTMLContentSeleciton* find(Node*) const;
    bool isEmpty() const { return !m_first; }

    void clear();
    void append(PassRefPtr<HTMLContentSeleciton>);

private:
    RefPtr<HTMLContentSeleciton> m_first;
    RefPtr<HTMLContentSeleciton> m_last;
};


class HTMLContentSelectionSet {
public:
    void add(HTMLContentSeleciton* value) { m_set.add(value); }
    void remove(HTMLContentSeleciton* value) { m_set.remove(value); }
    bool isEmpty() const { return m_set.isEmpty(); }
    HTMLContentSeleciton* find(Node* key) const;

private:
    struct Translator {
    public:
        static unsigned hash(const Node* key) { return PtrHash<const Node*>::hash(key); }
        static bool equal(const HTMLContentSeleciton* selection, const Node* node) { return selection->node() == node; }
    };

    struct Hash {
        static unsigned hash(HTMLContentSeleciton* key) { return PtrHash<const Node*>::hash(key->node()); }
        static bool equal(HTMLContentSeleciton* a, HTMLContentSeleciton* b) { return a->node() == b->node(); }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    typedef HashSet<HTMLContentSeleciton*, Hash> PointerSet;

    PointerSet m_set;
};

inline HTMLContentSeleciton* HTMLContentSelectionSet::find(Node* key) const
{
    PointerSet::iterator found = m_set.find<Node*, HTMLContentSelectionSet::Translator>(key);
    return found != m_set.end() ? *found : 0;
}

class HTMLContentSelector {
    WTF_MAKE_NONCOPYABLE(HTMLContentSelector);
public:
    HTMLContentSelector();
    ~HTMLContentSelector();

    void select(HTMLContentElement*, HTMLContentSelectionList*);
    void unselect(HTMLContentSelectionList*);
    HTMLContentSeleciton* findFor(Node* key) const;

    void willSelectOver(ShadowRoot*);
    void didSelect();
    bool hasCandidates() const { return !m_candidates.isEmpty(); }

private:
    void removeFromSet(HTMLContentSelectionList*);
    void addToSet(HTMLContentSelectionList*);

    Vector<RefPtr<Node> > m_candidates;
    HTMLContentSelectionSet m_selectionSet;
};

}

#endif
