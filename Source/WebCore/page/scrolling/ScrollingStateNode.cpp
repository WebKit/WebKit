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
#include "ScrollingStateNode.h"

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "ScrollingStateFixedNode.h"
#include "ScrollingStateTree.h"
#include <wtf/text/TextStream.h>

#include <wtf/text/WTFString.h>

namespace WebCore {

ScrollingStateNode::ScrollingStateNode(ScrollingNodeType nodeType, ScrollingStateTree& scrollingStateTree, ScrollingNodeID nodeID)
    : m_nodeType(nodeType)
    , m_nodeID(nodeID)
    , m_scrollingStateTree(scrollingStateTree)
{
}

// This copy constructor is used for cloning nodes in the tree, and it doesn't make sense
// to clone the relationship pointers, so don't copy that information from the original node.
ScrollingStateNode::ScrollingStateNode(const ScrollingStateNode& stateNode, ScrollingStateTree& adoptiveTree)
    : m_nodeType(stateNode.nodeType())
    , m_nodeID(stateNode.scrollingNodeID())
    , m_changedProperties(stateNode.changedProperties())
    , m_scrollingStateTree(adoptiveTree)
{
    if (hasChangedProperty(ScrollLayer))
        setLayer(stateNode.layer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));
    scrollingStateTree().addNode(*this);
}

ScrollingStateNode::~ScrollingStateNode() = default;

void ScrollingStateNode::setPropertyChanged(unsigned propertyBit)
{
    if (hasChangedProperty(propertyBit))
        return;

    m_changedProperties |= (static_cast<ChangedProperties>(1) << propertyBit);
    m_scrollingStateTree.setHasChangedProperties();
}

Ref<ScrollingStateNode> ScrollingStateNode::cloneAndReset(ScrollingStateTree& adoptiveTree)
{
    auto clone = this->clone(adoptiveTree);

    // Now that this node is cloned, reset our change properties.
    resetChangedProperties();

    cloneAndResetChildren(clone.get(), adoptiveTree);

    return clone;
}

void ScrollingStateNode::cloneAndResetChildren(ScrollingStateNode& clone, ScrollingStateTree& adoptiveTree)
{
    if (!m_children)
        return;

    for (auto& child : *m_children)
        clone.appendChild(child->cloneAndReset(adoptiveTree));
}

void ScrollingStateNode::appendChild(Ref<ScrollingStateNode>&& childNode)
{
    childNode->setParent(this);

    if (!m_children)
        m_children = std::make_unique<Vector<RefPtr<ScrollingStateNode>>>();
    m_children->append(WTFMove(childNode));
    setPropertyChanged(ChildNodes);
}

void ScrollingStateNode::insertChild(Ref<ScrollingStateNode>&& childNode, size_t index)
{
    childNode->setParent(this);

    if (!m_children) {
        ASSERT(!index);
        m_children = std::make_unique<Vector<RefPtr<ScrollingStateNode>>>();
    }

    m_children->insert(index, WTFMove(childNode));
    setPropertyChanged(ChildNodes);
}

void ScrollingStateNode::removeFromParent()
{
    if (!m_parent)
        return;

    m_parent->removeChild(*this);
}

void ScrollingStateNode::removeChild(ScrollingStateNode& childNode)
{
    auto childIndex = indexOfChild(childNode);
    if (childIndex != notFound)
        removeChildAtIndex(childIndex);
}

void ScrollingStateNode::removeChildAtIndex(size_t index)
{
    ASSERT(m_children && index < m_children->size());
    if (m_children && index < m_children->size()) {
        m_children->remove(index);
        setPropertyChanged(ChildNodes);
    }
}

size_t ScrollingStateNode::indexOfChild(ScrollingStateNode& childNode) const
{
    if (!m_children)
        return notFound;

    return m_children->find(&childNode);
}

void ScrollingStateNode::reconcileLayerPositionForViewportRect(const LayoutRect& viewportRect, ScrollingLayerPositionAction action)
{
    if (!m_children)
        return;

    for (auto& child : *m_children)
        child->reconcileLayerPositionForViewportRect(viewportRect, action);
}

void ScrollingStateNode::setLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_layer)
        return;
    
    m_layer = layerRepresentation;

    setPropertyChanged(ScrollLayer);
}

void ScrollingStateNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    if (behavior & ScrollingStateTreeAsTextBehaviorIncludeNodeIDs)
        ts.dumpProperty("nodeID", scrollingNodeID());
    
    if (behavior & ScrollingStateTreeAsTextBehaviorIncludeLayerIDs)
        ts.dumpProperty("layerID", layer().layerID());
}

void ScrollingStateNode::dump(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "\n";
    ts << indent << "(";
    ts.increaseIndent();
    dumpProperties(ts, behavior);

    if (m_children) {
        ts << "\n";
        ts << indent <<"(";
        {
            TextStream::IndentScope indentScope(ts);
            ts << "children " << children()->size();
            for (auto& child : *m_children)
                child->dump(ts, behavior);
            ts << "\n";
        }
        ts << indent << ")";
    }
    ts << "\n";
    ts.decreaseIndent();
    ts << indent << ")";
}

String ScrollingStateNode::scrollingStateTreeAsText(ScrollingStateTreeAsTextBehavior behavior) const
{
    TextStream ts(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);

    dump(ts, behavior);
    ts << "\n";
    return ts.release();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
