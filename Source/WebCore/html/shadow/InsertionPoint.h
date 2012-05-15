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

#include "ContentDistributor.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include <wtf/Forward.h>

namespace WebCore {

class InsertionPoint : public HTMLElement {
public:
    virtual ~InsertionPoint();

    bool hasDistribution() const { return !m_distribution.isEmpty(); }
    bool isShadowBoundary() const;
    bool isActive() const;

    virtual const AtomicString& select() const = 0;
    virtual bool isSelectValid() const = 0;
    virtual bool doesSelectFromHostChildren() const = 0;

    virtual void attach();
    virtual void detach();

    virtual bool isInsertionPoint() const OVERRIDE { return true; }
    ShadowRoot* assignedFrom() const;

    size_t indexOf(Node* node) const { return m_distribution.find(node); }
    size_t size() const { return m_distribution.size(); }
    Node* at(size_t index)  const { return m_distribution.at(index).get(); }
    Node* first() const { return m_distribution.isEmpty() ? 0 : m_distribution.first().get(); }
    Node* last() const { return m_distribution.isEmpty() ? 0 : m_distribution.last().get(); }
    Node* nextTo(const Node*) const;
    Node* previousTo(const Node*) const;

protected:
    InsertionPoint(const QualifiedName&, Document*);
    virtual bool rendererIsNeeded(const NodeRenderingContext&) OVERRIDE;

private:
    void distributeHostChildren(ElementShadow*);
    void clearDistribution(ElementShadow*);
    void attachDistributedNode();

    void assignShadowRoot(ShadowRoot*);
    void clearAssignment(ShadowRoot*);

    ContentDistribution m_distribution;
};

inline bool isInsertionPoint(const Node* node)
{
    if (!node)
        return true;

    if (node->isHTMLElement() && toHTMLElement(node)->isInsertionPoint())
        return true;

    return false;
}

inline InsertionPoint* toInsertionPoint(Node* node)
{
    ASSERT(isInsertionPoint(node));
    return static_cast<InsertionPoint*>(node);
}

inline const InsertionPoint* toInsertionPoint(const Node* node)
{
    ASSERT(isInsertionPoint(node));
    return static_cast<const InsertionPoint*>(node);
}

inline bool isShadowBoundary(Node* node)
{
    if (!isInsertionPoint(node))
        return false;
    return toInsertionPoint(node)->isShadowBoundary();
}

} // namespace WebCore

#endif // InsertionPoint_h
