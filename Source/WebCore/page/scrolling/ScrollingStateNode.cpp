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

#include "ScrollingStateTree.h"

#if ENABLE(THREADED_SCROLLING)

namespace WebCore {

ScrollingStateNode::ScrollingStateNode(ScrollingStateTree* scrollingStateTree)
    : m_scrollingStateTree(scrollingStateTree)
    , m_parent(0)
    , m_scrollLayerDidChange(false)
{
}

// This copy constructor is used for cloning nodes in the tree, and it doesn't make sense
// to clone the relationship pointers, so don't copy that information from the original
// node.
ScrollingStateNode::ScrollingStateNode(ScrollingStateNode* stateNode)
    : m_scrollingStateTree(0)
    , m_parent(0)
    , m_scrollLayerDidChange(stateNode->scrollLayerDidChange())
{
    setScrollLayer(stateNode->platformScrollLayer());
}

ScrollingStateNode::~ScrollingStateNode()
{
}

void ScrollingStateNode::appendChild(PassOwnPtr<ScrollingStateNode> childNode)
{
    childNode->setParent(this);
    
    if (!m_firstChild) {
        m_firstChild = childNode;
        return;
    }

    for (ScrollingStateNode* existingChild = firstChild(); existingChild; existingChild = existingChild->nextSibling()) {
        if (!existingChild->nextSibling()) {
            existingChild->setNextSibling(childNode);
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

ScrollingStateNode* ScrollingStateNode::traverseNext() const
{
    ScrollingStateNode* child = firstChild();
    if (child)
        return child;

    ScrollingStateNode* sibling = nextSibling();
    if (sibling)
        return sibling;

    const ScrollingStateNode* stateNode = this;
    while (!sibling) {
        stateNode = stateNode->parent();
        if (!stateNode)
            return 0;
        sibling = stateNode->nextSibling();
    }

    if (stateNode)
        return sibling;

    return 0;
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
