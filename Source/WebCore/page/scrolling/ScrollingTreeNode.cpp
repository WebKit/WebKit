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

#include "config.h"
#include "ScrollingTreeNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateTree.h"
#include "ScrollingTree.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollingTreeNode::ScrollingTreeNode(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : m_scrollingTree(scrollingTree)
    , m_nodeType(nodeType)
    , m_nodeID(nodeID)
    , m_parent(nullptr)
{
}

ScrollingTreeNode::~ScrollingTreeNode() = default;

void ScrollingTreeNode::appendChild(Ref<ScrollingTreeNode>&& childNode)
{
    childNode->setParent(this);

    if (!m_children)
        m_children = std::make_unique<Vector<RefPtr<ScrollingTreeNode>>>();
    m_children->append(WTFMove(childNode));
}

void ScrollingTreeNode::removeChild(ScrollingTreeNode& node)
{
    if (!m_children)
        return;

    size_t index = m_children->find(&node);

    // The index will be notFound if the node to remove is a deeper-than-1-level descendant or
    // if node is the root state node.
    if (index != notFound) {
        m_children->remove(index);
        return;
    }

    for (auto& child : *m_children)
        child->removeChild(node);
}

bool ScrollingTreeNode::isRootNode() const
{
    return m_scrollingTree.rootNode() == this;
}

void ScrollingTreeNode::relatedNodeScrollPositionDidChange(const ScrollingTreeScrollingNode&, const FloatRect&, FloatSize&)
{
}

void ScrollingTreeNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    if (behavior & ScrollingStateTreeAsTextBehaviorIncludeNodeIDs)
        ts.dumpProperty("nodeID", scrollingNodeID());
}

ScrollingTreeFrameScrollingNode* ScrollingTreeNode::enclosingFrameNodeIncludingSelf()
{
    auto* node = this;
    while (node && !node->isFrameScrollingNode())
        node = node->parent();

    return downcast<ScrollingTreeFrameScrollingNode>(node);
}

ScrollingTreeScrollingNode* ScrollingTreeNode::enclosingScrollingNodeIncludingSelf()
{
    auto* node = this;
    while (node && !node->isScrollingNode())
        node = node->parent();

    return downcast<ScrollingTreeScrollingNode>(node);
}

void ScrollingTreeNode::dump(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    dumpProperties(ts, behavior);

    if (m_children) {
        for (auto& child : *m_children) {
            TextStream::GroupScope scope(ts);
            child->dump(ts, behavior);
        }
    }
}

ScrollingTreeScrollingNode* ScrollingTreeNode::scrollingNodeForPoint(LayoutPoint parentPoint) const
{
    LayoutPoint localPoint = parentToLocalPoint(parentPoint);
    LayoutPoint contentsPoint = localToContentsPoint(localPoint);

    if (children()) {
        for (auto iterator = children()->rbegin(), end = children()->rend(); iterator != end; iterator++) {
            if (auto node = (**iterator).scrollingNodeForPoint(contentsPoint))
                return node;
        }
    }

    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
