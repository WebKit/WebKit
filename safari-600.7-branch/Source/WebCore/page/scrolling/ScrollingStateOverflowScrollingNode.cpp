/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "ScrollingStateOverflowScrollingNode.h"

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "ScrollingStateTree.h"
#include "TextStream.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

PassRefPtr<ScrollingStateOverflowScrollingNode> ScrollingStateOverflowScrollingNode::create(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
{
    return adoptRef(new ScrollingStateOverflowScrollingNode(stateTree, nodeID));
}

ScrollingStateOverflowScrollingNode::ScrollingStateOverflowScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
    : ScrollingStateScrollingNode(stateTree, OverflowScrollingNode, nodeID)
{
}

ScrollingStateOverflowScrollingNode::ScrollingStateOverflowScrollingNode(const ScrollingStateOverflowScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateScrollingNode(stateNode, adoptiveTree)
{
    if (hasChangedProperty(ScrolledContentsLayer))
        setScrolledContentsLayer(stateNode.scrolledContentsLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));
}

ScrollingStateOverflowScrollingNode::~ScrollingStateOverflowScrollingNode()
{
}

PassRefPtr<ScrollingStateNode> ScrollingStateOverflowScrollingNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(new ScrollingStateOverflowScrollingNode(*this, adoptiveTree));
}
    
void ScrollingStateOverflowScrollingNode::setScrolledContentsLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_scrolledContentsLayer)
        return;
    
    m_scrolledContentsLayer = layerRepresentation;
    setPropertyChanged(ScrolledContentsLayer);
}

void ScrollingStateOverflowScrollingNode::dumpProperties(TextStream& ts, int indent) const
{
    ts << "(" << "Overflow scrolling node" << "\n";
    
    ScrollingStateScrollingNode::dumpProperties(ts, indent);
    
    if (m_scrolledContentsLayer.layerID()) {
        writeIndent(ts, indent + 1);
        ts << "(scrolled contents layer " << m_scrolledContentsLayer.layerID() << ")\n";
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
