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
class InsertionPoint;
class Node;
class ShadowRoot;

class HTMLContentSelection : public RefCounted<HTMLContentSelection> {
public:
    static PassRefPtr<HTMLContentSelection> create(InsertionPoint*, Node*);

    InsertionPoint* insertionPoint() const { return m_insertionPoint; }
    Node* node() const { return m_node.get(); }
    HTMLContentSelection* next() const { return m_next.get(); }
    HTMLContentSelection* previous() const { return m_previous.get(); }

    void append(PassRefPtr<HTMLContentSelection>);
    void unlink();

private:
    HTMLContentSelection(InsertionPoint*, Node*);

    InsertionPoint* m_insertionPoint;
    RefPtr<Node> m_node;
    RefPtr<HTMLContentSelection> m_next;
    RefPtr<HTMLContentSelection> m_previous;
};

inline HTMLContentSelection::HTMLContentSelection(InsertionPoint* insertionPoint, Node* node)
    : m_insertionPoint(insertionPoint), m_node(node)
{ }

inline PassRefPtr<HTMLContentSelection> HTMLContentSelection::create(InsertionPoint* insertionPoint, Node* node)
{
    return adoptRef(new HTMLContentSelection(insertionPoint, node));
}

class HTMLContentSelectionList {
public:
    HTMLContentSelectionList();
    ~HTMLContentSelectionList();

    HTMLContentSelection* first() const { return m_first.get(); }
    HTMLContentSelection* last() const { return m_last.get(); }
    HTMLContentSelection* find(Node*) const;
    bool isEmpty() const { return !m_first; }

    void clear();
    void append(PassRefPtr<HTMLContentSelection>);

private:
    RefPtr<HTMLContentSelection> m_first;
    RefPtr<HTMLContentSelection> m_last;
};


class HTMLContentSelectionSet {
public:
    void add(HTMLContentSelection* value) { m_set.add(value); }
    void remove(HTMLContentSelection* value) { m_set.remove(value); }
    bool isEmpty() const { return m_set.isEmpty(); }
    HTMLContentSelection* find(const Node* key) const;

private:
    struct Translator {
    public:
        static unsigned hash(const Node* key) { return PtrHash<const Node*>::hash(key); }
        static bool equal(const HTMLContentSelection* selection, const Node* node) { return selection->node() == node; }
    };

    struct Hash {
        static unsigned hash(HTMLContentSelection* key) { return PtrHash<const Node*>::hash(key->node()); }
        static bool equal(HTMLContentSelection* a, HTMLContentSelection* b) { return a->node() == b->node(); }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    typedef HashSet<HTMLContentSelection*, Hash> PointerSet;

    PointerSet m_set;
};

inline HTMLContentSelection* HTMLContentSelectionSet::find(const Node* key) const
{
    PointerSet::iterator found = m_set.find<const Node*, HTMLContentSelectionSet::Translator>(key);
    return found != m_set.end() ? *found : 0;
}

class HTMLContentSelector {
    WTF_MAKE_NONCOPYABLE(HTMLContentSelector);
public:
    HTMLContentSelector();
    ~HTMLContentSelector();

    void select(InsertionPoint*, HTMLContentSelectionList*);
    void unselect(HTMLContentSelectionList*);
    HTMLContentSelection* findFor(const Node* key) const;

    void willSelect();
    bool isSelecting() const;
    void didSelect();

    void populateIfNecessary(Element* shadowHost);
    bool hasPopulated() const;

private:
    enum SelectingPhase {
        SelectionPrevented,
        SelectionStarted,
        HostChildrenPopulated,
    };

    void removeFromSet(HTMLContentSelectionList*);
    void addToSet(HTMLContentSelectionList*);

    Vector<RefPtr<Node> > m_candidates;
    HTMLContentSelectionSet m_selectionSet;
    SelectingPhase m_phase;
};

inline bool HTMLContentSelector::isSelecting() const
{
    return m_phase != SelectionPrevented;
}

inline bool HTMLContentSelector::hasPopulated() const
{
    return m_phase == HostChildrenPopulated;
}

}

#endif
