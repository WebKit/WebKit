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
#include "TextStream.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

ScrollingStateNode::ScrollingStateNode(ScrollingNodeType nodeType, ScrollingStateTree& scrollingStateTree, ScrollingNodeID nodeID)
    : m_nodeType(nodeType)
    , m_nodeID(nodeID)
    , m_changedProperties(0)
    , m_scrollingStateTree(scrollingStateTree)
    , m_parent(nullptr)
{
}

// This copy constructor is used for cloning nodes in the tree, and it doesn't make sense
// to clone the relationship pointers, so don't copy that information from the original node.
ScrollingStateNode::ScrollingStateNode(const ScrollingStateNode& stateNode, ScrollingStateTree& adoptiveTree)
    : m_nodeType(stateNode.nodeType())
    , m_nodeID(stateNode.scrollingNodeID())
    , m_changedProperties(stateNode.changedProperties())
    , m_scrollingStateTree(adoptiveTree)
    , m_parent(nullptr)
{
    if (hasChangedProperty(ScrollLayer))
        setLayer(stateNode.layer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));
    scrollingStateTree().addNode(this);
}

ScrollingStateNode::~ScrollingStateNode()
{
}

void ScrollingStateNode::setPropertyChanged(unsigned propertyBit)
{
    if (m_changedProperties & (1 << propertyBit))
        return;

    m_changedProperties |= (1 << propertyBit);
    m_scrollingStateTree.setHasChangedProperties();
}

PassRefPtr<ScrollingStateNode> ScrollingStateNode::cloneAndReset(ScrollingStateTree& adoptiveTree)
{
    RefPtr<ScrollingStateNode> clone = this->clone(adoptiveTree);

    // Now that this node is cloned, reset our change properties.
    resetChangedProperties();

    cloneAndResetChildren(*clone, adoptiveTree);
    return clone.release();
}

void ScrollingStateNode::cloneAndResetChildren(ScrollingStateNode& clone, ScrollingStateTree& adoptiveTree)
{
    if (!m_children)
        return;

    for (auto& child : *m_children)
        clone.appendChild(child->cloneAndReset(adoptiveTree));
}

void ScrollingStateNode::appendChild(PassRefPtr<ScrollingStateNode> childNode)
{
    childNode->setParent(this);

    if (!m_children)
        m_children = adoptPtr(new Vector<RefPtr<ScrollingStateNode>>);

    m_children->append(childNode);
}

void ScrollingStateNode::setLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_layer)
        return;
    
    m_layer = layerRepresentation;

    setPropertyChanged(ScrollLayer);
}

void ScrollingStateNode::dump(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    dumpProperties(ts, indent);

    if (m_children) {
        writeIndent(ts, indent + 1);
        ts << "(children " << children()->size() << "\n";
        
        for (auto& child : *m_children)
            child->dump(ts, indent + 2);
        writeIndent(ts, indent + 1);
        ts << ")\n";
    }

    writeIndent(ts, indent);
    ts << ")\n";
}

String ScrollingStateNode::scrollingStateTreeAsText() const
{
    TextStream ts;

    dump(ts, 0);
    return ts.release();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
