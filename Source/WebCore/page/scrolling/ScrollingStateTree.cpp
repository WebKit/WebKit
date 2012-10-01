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
    : m_rootStateNode(ScrollingStateScrollingNode::create(this))
    , m_hasChangedProperties(false)
{
}

ScrollingStateTree::~ScrollingStateTree()
{
}

PassOwnPtr<ScrollingStateTree> ScrollingStateTree::commit()
{
    // This function clones the entire ScrollingStateTree.
    OwnPtr<ScrollingStateTree> treeState = ScrollingStateTree::create();

    // currentNode is the node that we are currently cloning.
    ScrollingStateNode* currentNode = m_rootStateNode.get();

    // nextNode represents the nextNode we will clone. Some tree traversal is required to find it.
    ScrollingStateNode* nextNode = 0;

    // As we clone each node, we want to set the cloned nodes relationship pointers to the corresponding
    // cloned nodes in the clone tree. In order to do that, we need to keep track of the appropriate
    // nodes.
    ScrollingStateNode* cloneParent = 0;
    ScrollingStateNode* clonePreviousSibling = 0;

    // Now traverse the tree and clone each node.
    while (currentNode) {
        PassOwnPtr<ScrollingStateNode> cloneNode = currentNode->cloneNode();

        // Set relationships for the newly cloned node.
        cloneNode->setScrollingStateTree(treeState.get());
        cloneNode->setParent(cloneParent);
        if (cloneParent) {
            if (!cloneParent->firstChild())
                cloneParent->setFirstChild(cloneNode);
            if (clonePreviousSibling)
                clonePreviousSibling->setNextSibling(cloneNode);
        }

        // Now find the next node, and set up the cloneParent and clonePreviousSibling pointer appropriately.
        if (currentNode->firstChild()) {
            nextNode = currentNode->firstChild();
            cloneParent = cloneNode.get();
            clonePreviousSibling = 0;
        } else if (currentNode->nextSibling()) {
            nextNode = currentNode->nextSibling();
            cloneParent = cloneNode->parent();
            clonePreviousSibling = cloneNode.get();
        } else {
            // If there is no first child and no next sibling, then we have to traverse up and over in the tree.
            nextNode = 0;
            ScrollingStateNode* traversalNode = currentNode;
            ScrollingStateNode* cloneTraversalNode = cloneNode.get();
            clonePreviousSibling = 0;
            while (!nextNode) {
                traversalNode = traversalNode->parent();
                cloneTraversalNode = cloneTraversalNode->parent();
                if (!traversalNode) {
                    nextNode = 0;
                    break;
                }
                nextNode = traversalNode->nextSibling();
                clonePreviousSibling = cloneTraversalNode;
                cloneParent = clonePreviousSibling ? clonePreviousSibling->parent() : 0;
            }
        }

        if (currentNode == m_rootStateNode)
            treeState->setRootStateNode(static_pointer_cast<ScrollingStateScrollingNode>(cloneNode));

        // Before we move on, reset all of our indicators that properties have changed on the original tree.
        currentNode->setScrollLayerDidChange(false);
        currentNode->resetChangedProperties();

        currentNode = nextNode;
    }

    // Now the clone tree has changed properties, and the original tree does not.
    treeState->setHasChangedProperties(true);
    m_hasChangedProperties = false;

    return treeState.release();
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
