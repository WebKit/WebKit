/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef InsertionPoint_h
#define InsertionPoint_h

#include "CSSSelectorList.h"
#include "ContentDistributor.h"
#include "ElementShadow.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "ShadowRoot.h"
#include <wtf/Forward.h>

namespace WebCore {

class InsertionPoint : public HTMLElement {
public:
    enum Type {
        ShadowInsertionPoint,
        ContentInsertionPoint
    };

    virtual ~InsertionPoint();

    bool hasDistribution() const { return !m_distribution.isEmpty(); }
    void setDistribution(ContentDistribution& distribution) { m_distribution.swap(distribution); }
    void clearDistribution() { m_distribution.clear(); }
    bool isShadowBoundary() const;
    bool isActive() const;

    PassRefPtr<NodeList> getDistributedNodes() const;

    virtual const AtomicString& select() const = 0;
    virtual bool isSelectValid() = 0;
    virtual const CSSSelectorList& selectorList() = 0;
    virtual Type insertionPointType() const = 0;

    bool resetStyleInheritance() const;
    void setResetStyleInheritance(bool);

    virtual void attach();
    virtual void detach();

    bool shouldUseFallbackElements() const;

    size_t indexOf(Node* node) const { return m_distribution.find(node); }
    bool contains(const Node* node) const { return m_distribution.contains(const_cast<Node*>(node)) || (node->isShadowRoot() && toShadowRoot(node)->assignedTo() == this); }
    size_t size() const { return m_distribution.size(); }
    Node* at(size_t index)  const { return m_distribution.at(index).get(); }
    Node* first() const { return m_distribution.isEmpty() ? 0 : m_distribution.first().get(); }
    Node* last() const { return m_distribution.isEmpty() ? 0 : m_distribution.last().get(); }
    Node* nextTo(const Node* node) const { return m_distribution.nextTo(node); }
    Node* previousTo(const Node* node) const { return m_distribution.previousTo(node); }

protected:
    InsertionPoint(const QualifiedName&, Document*);
    virtual bool rendererIsNeeded(const NodeRenderingContext&) OVERRIDE;
    virtual void childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta) OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

private:

    ContentDistribution m_distribution;
    bool m_registeredWithShadowRoot;
};

inline InsertionPoint* toInsertionPoint(Node* node)
{
    ASSERT(!node || node->isInsertionPoint());
    return static_cast<InsertionPoint*>(node);
}

inline const InsertionPoint* toInsertionPoint(const Node* node)
{
    ASSERT(!node || node->isInsertionPoint());
    return static_cast<const InsertionPoint*>(node);
}

inline bool isActiveInsertionPoint(const Node* node)
{
    return node->isInsertionPoint() && toInsertionPoint(node)->isActive();
}

inline bool isLowerEncapsulationBoundary(Node* node)
{
    if (!node || !node->isInsertionPoint())
        return false;
    return toInsertionPoint(node)->isShadowBoundary();
}

inline Node* parentNodeForDistribution(const Node* node)
{
    ASSERT(node);

    if (Node* parent = node->parentNode()) {
        if (parent->isInsertionPoint() && toInsertionPoint(parent)->shouldUseFallbackElements())
            return parent->parentNode();
        return parent;
    }

    return 0;
}

inline Element* parentElementForDistribution(const Node* node)
{
    if (Node* parent = parentNodeForDistribution(node)) {
        if (parent->isElementNode())
            return toElement(parent);
    }

    return 0;
}

inline ElementShadow* shadowOfParentForDistribution(const Node* node)
{
    ASSERT(node);
    if (Element* parent = parentElementForDistribution(node))
        return parent->shadow();

    return 0;
}

inline InsertionPoint* resolveReprojection(const Node* projectedNode)
{
    InsertionPoint* insertionPoint = 0;
    const Node* current = projectedNode;

    while (current) {
        if (ElementShadow* shadow = shadowOfParentForDistribution(current)) {
            shadow->ensureDistribution();
            if (InsertionPoint* insertedTo = shadow->distributor().findInsertionPointFor(projectedNode)) {
                current = insertedTo;
                insertionPoint = insertedTo;
                continue;
            }
        }

        if (Node* parent = parentNodeForDistribution(current)) {
            if (InsertionPoint* insertedTo = parent->isShadowRoot() ? toShadowRoot(parent)->assignedTo() : 0) {
                current = insertedTo;
                insertionPoint = insertedTo;
                continue;
            }
        }

        break;
    }

    return insertionPoint;
}

} // namespace WebCore

#endif // InsertionPoint_h
