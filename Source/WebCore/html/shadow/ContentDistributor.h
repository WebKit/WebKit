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

#ifndef ContentDistributor_h
#define ContentDistributor_h

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Element;
class InsertionPoint;
class Node;
class ShadowRoot;

class ContentDistribution {
public:
    // TODO: The class should be reduced into simple Node* which is kept in a vector: https://bugs.webkit.org/show_bug.cgi?id=86350
    class Item : public RefCounted<Item> {
    public:
        friend class ContentDistribution;

        InsertionPoint* insertionPoint() const { return m_insertionPoint; }
        Node* node() const { return m_node.get(); }
        Item* next() const { return m_next.get(); }
        Item* previous() const { return m_previous.get(); }

    private:
        static PassRefPtr<Item> create(InsertionPoint* insertionPoint, Node* node) {  return adoptRef(new Item(insertionPoint, node)); }

        Item(InsertionPoint* insertionPoint, Node* node)
            : m_insertionPoint(insertionPoint)
            , m_node(node)
        { }

        InsertionPoint* m_insertionPoint;
        RefPtr<Node> m_node;
        RefPtr<Item> m_next;
        RefPtr<Item> m_previous;
    };
    
    ContentDistribution();
    ~ContentDistribution();

    Item* first() const { return m_first.get(); }
    Item* last() const { return m_last.get(); }
    Node* firstNode() const { return m_first ? m_first->node() : 0; }
    Node* lastNode() const { return m_first ? m_last->node() : 0; }
    
    Item* find(Node*) const;
    bool isEmpty() const { return !m_first; }

    void clear();
    void append(InsertionPoint*, Node*);

private:

    RefPtr<Item> m_first;
    RefPtr<Item> m_last;
};

class ContentDistributor {
    WTF_MAKE_NONCOPYABLE(ContentDistributor);
public:
    ContentDistributor();
    ~ContentDistributor();

    void distribute(InsertionPoint*, ContentDistribution*);
    void clearDistribution(ContentDistribution*);
    ContentDistribution::Item* findFor(const Node* key) const;
    InsertionPoint* findInsertionPointFor(const Node* key) const;

    void willDistribute();
    bool inDistribution() const;
    void didDistribute();

    void preparePoolFor(Element* shadowHost);
    bool poolIsReady() const;

private:
    struct Translator {
    public:
        static unsigned hash(const Node* key) { return PtrHash<const Node*>::hash(key); }
        static bool equal(const ContentDistribution::Item* item, const Node* node) { return item->node() == node; }
    };

    struct Hash {
        static unsigned hash(ContentDistribution::Item* key) { return PtrHash<const Node*>::hash(key->node()); }
        static bool equal(ContentDistribution::Item* a, ContentDistribution::Item* b) { return a->node() == b->node(); }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    // Used as a table from Node to InseretionPoint
    typedef HashSet<ContentDistribution::Item*, Hash> InvertedTable;

    enum DistributionPhase {
        Prevented,
        Started,
        Prepared,
    };

    Vector<RefPtr<Node> > m_pool;
    DistributionPhase m_phase;
    InvertedTable m_nodeToInsertionPoint;
};

inline bool ContentDistributor::inDistribution() const
{
    return m_phase != Prevented;
}

inline bool ContentDistributor::poolIsReady() const
{
    return m_phase == Prepared;
}

}

#endif
