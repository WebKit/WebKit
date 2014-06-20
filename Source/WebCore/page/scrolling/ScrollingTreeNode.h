/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ScrollingTreeNode_h
#define ScrollingTreeNode_h

#if ENABLE(ASYNC_SCROLLING)

#include "IntRect.h"
#include "ScrollTypes.h"
#include "ScrollingCoordinator.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class ScrollingStateFixedNode;
class ScrollingStateNode;
class ScrollingStateScrollingNode;

class ScrollingTreeNode : public RefCounted<ScrollingTreeNode> {
public:
    virtual ~ScrollingTreeNode();

    ScrollingNodeType nodeType() const { return m_nodeType; }
    ScrollingNodeID scrollingNodeID() const { return m_nodeID; }
    
    bool isFixedNode() const { return nodeType() == FixedNode; }
    bool isStickyNode() const { return nodeType() == StickyNode; }
    bool isScrollingNode() const { return nodeType() == FrameScrollingNode || nodeType() == OverflowScrollingNode; }
    bool isFrameScrollingNode() const { return nodeType() == FrameScrollingNode; }
    bool isOverflowScrollingNode() const { return nodeType() == OverflowScrollingNode; }

    virtual void updateBeforeChildren(const ScrollingStateNode&) = 0;
    virtual void updateAfterChildren(const ScrollingStateNode&) { }

    virtual void updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta) = 0;

    ScrollingTreeNode* parent() const { return m_parent; }
    void setParent(ScrollingTreeNode* parent) { m_parent = parent; }

    typedef Vector<RefPtr<ScrollingTreeNode>> ScrollingTreeChildrenVector;
    ScrollingTreeChildrenVector* children() { return m_children.get(); }
    
    void appendChild(PassRefPtr<ScrollingTreeNode>);
    void removeChild(ScrollingTreeNode*);

protected:
    ScrollingTreeNode(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);
    ScrollingTree& scrollingTree() const { return m_scrollingTree; }

    OwnPtr<ScrollingTreeChildrenVector> m_children;

private:
    ScrollingTree& m_scrollingTree;

    const ScrollingNodeType m_nodeType;
    const ScrollingNodeID m_nodeID;

    ScrollingTreeNode* m_parent;
};

#define SCROLLING_NODE_TYPE_CASTS(ToValueTypeName, predicate) \
    TYPE_CASTS_BASE(ToValueTypeName, ScrollingTreeNode, value, value->predicate, value.predicate)

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingTreeNode_h
