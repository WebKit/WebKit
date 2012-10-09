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
    , m_children(0)
    , m_scrollLayerDidChange(false)
{
}

// This copy constructor is used for cloning nodes in the tree, and it doesn't make sense
// to clone the relationship pointers, so don't copy that information from the original
// node.
ScrollingStateNode::ScrollingStateNode(ScrollingStateNode* stateNode)
    : m_scrollingStateTree(0)
    , m_parent(0)
    , m_children(0)
    , m_scrollLayerDidChange(stateNode->scrollLayerDidChange())
{
    setScrollLayer(stateNode->platformScrollLayer());
}

ScrollingStateNode::~ScrollingStateNode()
{
    delete m_children;
}

void ScrollingStateNode::cloneAndResetChildNodes(ScrollingStateNode* clone)
{
    if (!m_children)
        return;

    size_t size = m_children->size();
    for (size_t i = 0; i < size; ++i)
        clone->appendChild(m_children->at(i)->cloneAndResetNode());
}

void ScrollingStateNode::appendChild(PassOwnPtr<ScrollingStateNode> childNode)
{
    childNode->setParent(this);

    if (!m_children)
        m_children = new Vector<OwnPtr<ScrollingStateNode> >;

    m_children->append(childNode);
}

void ScrollingStateNode::removeChild(ScrollingStateNode* node)
{
    if (!m_children)
        return;

    if (size_t index = m_children->find(node)) {
        m_children->remove(index);
        return;
    }

    size_t size = m_children->size();
    for (size_t i = 0; i < size; ++i)
        m_children->at(i)->removeChild(node);
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
