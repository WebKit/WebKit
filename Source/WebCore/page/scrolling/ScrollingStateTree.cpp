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
#include "ScrollingStateTree.h"

#if ENABLE(THREADED_SCROLLING)

namespace WebCore {

PassOwnPtr<ScrollingStateTree> ScrollingStateTree::create()
{
    return adoptPtr(new ScrollingStateTree);
}

ScrollingStateTree::ScrollingStateTree()
    : m_rootStateNode(ScrollingStateScrollingNode::create(this, 0))
    , m_hasChangedProperties(false)
{
}

ScrollingStateTree::~ScrollingStateTree()
{
}

PassOwnPtr<ScrollingStateTree> ScrollingStateTree::commit()
{
    // This function clones and resets the current state tree, but leaves the tree structure intact. 
    OwnPtr<ScrollingStateTree> treeStateClone = ScrollingStateTree::create();
    treeStateClone->setRootStateNode(static_pointer_cast<ScrollingStateScrollingNode>(m_rootStateNode->cloneAndReset()));

    // Copy the IDs of the nodes that have been removed since the last commit into the clone.
    treeStateClone->m_nodesRemovedSinceLastCommit.swap(m_nodesRemovedSinceLastCommit);

    // Now the clone tree has changed properties, and the original tree does not.
    treeStateClone->m_hasChangedProperties = true;
    m_hasChangedProperties = false;

    return treeStateClone.release();
}

void ScrollingStateTree::removeNode(ScrollingStateNode* node)
{
    ASSERT(m_rootStateNode);
    m_rootStateNode->removeChild(node);
}

void ScrollingStateTree::didRemoveNode(ScrollingNodeID nodeID)
{
    m_nodesRemovedSinceLastCommit.append(nodeID);
    m_hasChangedProperties = true;
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
