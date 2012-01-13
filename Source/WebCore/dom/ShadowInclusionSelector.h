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

#ifndef ShadowInclusionSelector_h
#define ShadowInclusionSelector_h

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Element;
class Node;
class ShadowRoot;
class ShadowContentElement;

class ShadowInclusion : public RefCounted<ShadowInclusion> {
public:
    static PassRefPtr<ShadowInclusion> create(ShadowContentElement*, Node*);

    ShadowContentElement* includer() const { return m_includer; }
    Node* content() const { return m_content.get(); }
    ShadowInclusion* next() const { return m_next.get(); }
    ShadowInclusion* previous() const { return m_previous.get(); }

    void append(PassRefPtr<ShadowInclusion>);
    void unlink();

private:
    ShadowInclusion(ShadowContentElement*, Node*);

    ShadowContentElement* m_includer;
    RefPtr<Node> m_content;
    RefPtr<ShadowInclusion> m_next;
    RefPtr<ShadowInclusion> m_previous;
};

inline ShadowInclusion::ShadowInclusion(ShadowContentElement* includer, Node* content)
    : m_includer(includer), m_content(content)
{ }

inline PassRefPtr<ShadowInclusion> ShadowInclusion::create(ShadowContentElement* includer, Node* content)
{
    return adoptRef(new ShadowInclusion(includer, content));
}

class ShadowInclusionList {
public:
    ShadowInclusionList();
    ~ShadowInclusionList();

    ShadowInclusion* first() const { return m_first.get(); }
    ShadowInclusion* last() const { return m_last.get(); }
    ShadowInclusion* find(Node*) const;
    bool isEmpty() const { return !m_first; }

    void clear();
    void append(PassRefPtr<ShadowInclusion>);

private:
    RefPtr<ShadowInclusion> m_first;
    RefPtr<ShadowInclusion> m_last;
};


class ShadowInclusionSet {
public:
    void add(ShadowInclusion* value) { m_set.add(value); }
    void remove(ShadowInclusion* value) { m_set.remove(value); }
    bool isEmpty() const { return m_set.isEmpty(); }
    ShadowInclusion* find(Node* key) const;

private:
    struct Translator {
    public:
        static unsigned hash(const Node* key) { return PtrHash<const Node*>::hash(key); }
        static bool equal(const ShadowInclusion* inclusion, const Node* content) { return inclusion->content() == content; }
    };

    struct Hash {
        static unsigned hash(ShadowInclusion* key) { return PtrHash<const Node*>::hash(key->content()); }
        static bool equal(ShadowInclusion* a, ShadowInclusion* b) { return a->content() == b->content(); }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    typedef HashSet<ShadowInclusion*, Hash> PointerSet;

    PointerSet m_set;
};

inline ShadowInclusion* ShadowInclusionSet::find(Node* key) const
{
    PointerSet::iterator found = m_set.find<Node*, ShadowInclusionSet::Translator>(key);
    return found != m_set.end() ? *found : 0;
}

class ShadowInclusionSelector {
    WTF_MAKE_NONCOPYABLE(ShadowInclusionSelector);
public:
    ShadowInclusionSelector();
    ~ShadowInclusionSelector();

    void select(ShadowContentElement*, ShadowInclusionList*);
    void unselect(ShadowInclusionList*);
    ShadowInclusion* findFor(Node* key) const;

    void willSelectOver(ShadowRoot*);
    void didSelect();
    bool hasCandidates() const { return !m_candidates.isEmpty(); }

private:
    void removeFromSet(ShadowInclusionList*);
    void addToSet(ShadowInclusionList*);

    Vector<RefPtr<Node> > m_candidates;
    ShadowInclusionSet m_inclusionSet;
};

}

#endif
