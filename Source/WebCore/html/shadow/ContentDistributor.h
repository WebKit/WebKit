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
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class ContainerNode;
class Element;
class InsertionPoint;
class Node;
class ShadowRoot;

class ContentDistribution {
public:
    PassRefPtr<Node> first() const { return m_nodes.first(); }
    PassRefPtr<Node> last() const { return m_nodes.last(); }
    PassRefPtr<Node> at(size_t index) const { return m_nodes.at(index); }

    size_t size() const { return m_nodes.size(); }
    bool isEmpty() const { return m_nodes.isEmpty(); }

    void append(PassRefPtr<Node>);
    void clear() { m_nodes.clear(); m_indices.clear(); }

    bool contains(const Node* node) const { return m_indices.contains(node); }
    size_t find(const Node*) const;
    Node* nextTo(const Node*) const;
    Node* previousTo(const Node*) const;

    void swap(ContentDistribution& other);

    const Vector<RefPtr<Node> >& nodes() const { return m_nodes; }

private:
    Vector<RefPtr<Node> > m_nodes;
    HashMap<const Node*, size_t> m_indices;
};

class ShadowRootContentDistributionData {
public:
    ShadowRootContentDistributionData();

    InsertionPoint* insertionPointAssignedTo() const { return m_insertionPointAssignedTo; }
    void setInsertionPointAssignedTo(InsertionPoint* insertionPoint) { m_insertionPointAssignedTo = insertionPoint; }

    void regiterInsertionPoint(ShadowRoot*, InsertionPoint*);
    void unregisterInsertionPoint(ShadowRoot*, InsertionPoint*);
    bool hasShadowElementChildren() const { return m_numberOfShadowElementChildren > 0; }
    bool hasContentElementChildren() const { return m_numberOfContentElementChildren > 0; }

    void incrementNumberOfElementShadowChildren() { ++m_numberOfElementShadowChildren; }
    void decrementNumberOfElementShadowChildren() { ASSERT(m_numberOfElementShadowChildren > 0); --m_numberOfElementShadowChildren; }
    unsigned numberOfElementShadowChildren() const { return m_numberOfElementShadowChildren; }
    bool hasElementShadowChildren() const { return m_numberOfElementShadowChildren > 0; }

    void invalidateInsertionPointList();
    const Vector<RefPtr<InsertionPoint> >& ensureInsertionPointList(ShadowRoot*);

private:
    InsertionPoint* m_insertionPointAssignedTo;
    unsigned m_numberOfShadowElementChildren;
    unsigned m_numberOfContentElementChildren;
    unsigned m_numberOfElementShadowChildren;
    bool m_insertionPointListIsValid;
    Vector<RefPtr<InsertionPoint> > m_insertionPointList;
};

class ContentDistributor {
    WTF_MAKE_NONCOPYABLE(ContentDistributor);
public:
    enum Validity {
        Valid = 0,
        Invalidated = 1,
        Invalidating = 2,
        Undetermined = 3
    };

    ContentDistributor();
    ~ContentDistributor();

    InsertionPoint* findInsertionPointFor(const Node* key) const;

    void setValidity(Validity validity) { m_validity = validity; }

    void distribute(Element* host);
    bool invalidate(Element* host);
    void finishInivalidation();
    bool needsDistribution() const;
    bool needsInvalidation() const { return m_validity != Invalidated; }

    void distributeSelectionsTo(InsertionPoint*, const ContentDistribution& pool, Vector<bool>& distributed);
    void distributeNodeChildrenTo(InsertionPoint*, ContainerNode*);
    void invalidateDistributionIn(ContentDistribution*);

private:
    void populate(Node*, ContentDistribution&);

    HashMap<const Node*, RefPtr<InsertionPoint> > m_nodeToInsertionPoint;
    unsigned m_validity : 2;
};

inline bool ContentDistributor::needsDistribution() const
{
    // During the invalidation, re-distribution should be supressed.
    return m_validity != Valid && m_validity != Invalidating;
}

}

#endif
