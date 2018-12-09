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

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "AsyncScrollingCoordinator.h"
#include "ScrollingStateFixedNode.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingStateOverflowScrollingNode.h"
#include "ScrollingStateStickyNode.h"
#include <wtf/text/CString.h>

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WebCore {

ScrollingStateTree::ScrollingStateTree(AsyncScrollingCoordinator* scrollingCoordinator)
    : m_scrollingCoordinator(scrollingCoordinator)
    , m_hasChangedProperties(false)
    , m_hasNewRootStateNode(false)
    , m_preferredLayerRepresentation(LayerRepresentation::GraphicsLayerRepresentation)
{
}

ScrollingStateTree::~ScrollingStateTree() = default;

void ScrollingStateTree::setHasChangedProperties(bool changedProperties)
{
#if ENABLE(ASYNC_SCROLLING)
    bool gainedChangedProperties = !m_hasChangedProperties && changedProperties;
#endif

    m_hasChangedProperties = changedProperties;

#if ENABLE(ASYNC_SCROLLING)
    if (gainedChangedProperties && m_scrollingCoordinator)
        m_scrollingCoordinator->scrollingStateTreePropertiesChanged();
#endif
}

Ref<ScrollingStateNode> ScrollingStateTree::createNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case FixedNode:
        return ScrollingStateFixedNode::create(*this, nodeID);
    case StickyNode:
        return ScrollingStateStickyNode::create(*this, nodeID);
    case MainFrameScrollingNode:
    case SubframeScrollingNode:
        return ScrollingStateFrameScrollingNode::create(*this, nodeType, nodeID);
    case OverflowScrollingNode:
        return ScrollingStateOverflowScrollingNode::create(*this, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingStateFixedNode::create(*this, nodeID);
}

bool ScrollingStateTree::nodeTypeAndParentMatch(ScrollingStateNode& node, ScrollingNodeType nodeType, ScrollingNodeID parentID) const
{
    if (node.nodeType() != nodeType)
        return false;

    auto* parent = stateNodeForID(parentID);
    if (!parent)
        return true;

    return node.parent() == parent;
}

ScrollingNodeID ScrollingStateTree::attachNode(ScrollingNodeType nodeType, ScrollingNodeID newNodeID, ScrollingNodeID parentID)
{
    ASSERT(newNodeID);

    if (auto* node = stateNodeForID(newNodeID)) {
        if (nodeTypeAndParentMatch(*node, nodeType, parentID))
            return newNodeID;

#if ENABLE(ASYNC_SCROLLING)
        // If the type has changed, we need to destroy and recreate the node with a new ID.
        if (nodeType != node->nodeType())
            newNodeID = m_scrollingCoordinator->uniqueScrollingNodeID();
#endif

        // The node is being re-parented. To do that, we'll remove it, and then create a new node.
        removeNodeAndAllDescendants(node, SubframeNodeRemoval::Orphan);
    }

    ScrollingStateNode* newNode = nullptr;
    if (!parentID) {
        // If we're resetting the root node, we should clear the HashMap and destroy the current children.
        clear();

        setRootStateNode(ScrollingStateFrameScrollingNode::create(*this, MainFrameScrollingNode, newNodeID));
        newNode = rootStateNode();
        m_hasNewRootStateNode = true;
    } else {
        auto* parent = stateNodeForID(parentID);
        if (!parent)
            return 0;

        if (nodeType == SubframeScrollingNode && parentID) {
            if (auto orphanedNode = m_orphanedSubframeNodes.take(newNodeID)) {
                newNode = orphanedNode.get();
                parent->appendChild(orphanedNode.releaseNonNull());
            }
        }

        if (!newNode) {
            auto stateNode = createNode(nodeType, newNodeID);
            newNode = stateNode.ptr();
            parent->appendChild(WTFMove(stateNode));
        }
    }

    m_stateNodeMap.set(newNodeID, newNode);
    m_nodesRemovedSinceLastCommit.remove(newNodeID);
    return newNodeID;
}

void ScrollingStateTree::detachNode(ScrollingNodeID nodeID)
{
    if (!nodeID)
        return;

    // The node may not be found if clearStateTree() was recently called.
    auto* node = m_stateNodeMap.take(nodeID);
    if (!node)
        return;

    removeNodeAndAllDescendants(node, SubframeNodeRemoval::Orphan);
}

void ScrollingStateTree::clear()
{
    if (rootStateNode())
        removeNodeAndAllDescendants(rootStateNode());

    m_stateNodeMap.clear();
    m_orphanedSubframeNodes.clear();
}

std::unique_ptr<ScrollingStateTree> ScrollingStateTree::commit(LayerRepresentation::Type preferredLayerRepresentation)
{
    if (!m_orphanedSubframeNodes.isEmpty()) {
        // If we still have orphaned subtrees, remove them from m_stateNodeMap since they will be deleted 
        // when clearing m_orphanedSubframeNodes.
        for (auto& orphanNode : m_orphanedSubframeNodes.values())
            recursiveNodeWillBeRemoved(orphanNode.get(), SubframeNodeRemoval::Delete);
        m_orphanedSubframeNodes.clear();
    }

    // This function clones and resets the current state tree, but leaves the tree structure intact.
    std::unique_ptr<ScrollingStateTree> treeStateClone = std::make_unique<ScrollingStateTree>();
    treeStateClone->setPreferredLayerRepresentation(preferredLayerRepresentation);

    if (m_rootStateNode)
        treeStateClone->setRootStateNode(static_reference_cast<ScrollingStateFrameScrollingNode>(m_rootStateNode->cloneAndReset(*treeStateClone)));

    // Copy the IDs of the nodes that have been removed since the last commit into the clone.
    treeStateClone->m_nodesRemovedSinceLastCommit.swap(m_nodesRemovedSinceLastCommit);

    // Now the clone tree has changed properties, and the original tree does not.
    treeStateClone->m_hasChangedProperties = m_hasChangedProperties;
    m_hasChangedProperties = false;

    treeStateClone->m_hasNewRootStateNode = m_hasNewRootStateNode;
    m_hasNewRootStateNode = false;

    return treeStateClone;
}

void ScrollingStateTree::setRootStateNode(Ref<ScrollingStateFrameScrollingNode>&& rootStateNode)
{
    m_rootStateNode = WTFMove(rootStateNode);
}

void ScrollingStateTree::addNode(ScrollingStateNode& node)
{
    m_stateNodeMap.add(node.scrollingNodeID(), &node);
}

void ScrollingStateTree::removeNodeAndAllDescendants(ScrollingStateNode* node, SubframeNodeRemoval subframeNodeRemoval)
{
    auto* parent = node->parent();

    recursiveNodeWillBeRemoved(node, subframeNodeRemoval);

    if (node == m_rootStateNode)
        m_rootStateNode = nullptr;
    else if (parent) {
        ASSERT(parent->children());
        ASSERT(parent->children()->find(node) != notFound);
        if (auto children = parent->children()) {
            size_t index = children->find(node);
            if (index != notFound)
                children->remove(index);
        }
    }
}

void ScrollingStateTree::recursiveNodeWillBeRemoved(ScrollingStateNode* currNode, SubframeNodeRemoval subframeNodeRemoval)
{
    currNode->setParent(nullptr);
    if (subframeNodeRemoval == SubframeNodeRemoval::Orphan && currNode != m_rootStateNode && currNode->isFrameScrollingNode()) {
        m_orphanedSubframeNodes.add(currNode->scrollingNodeID(), currNode);
        return;
    }

    willRemoveNode(currNode);

    if (auto children = currNode->children()) {
        for (auto& child : *children)
            recursiveNodeWillBeRemoved(child.get(), subframeNodeRemoval);
    }
}

void ScrollingStateTree::willRemoveNode(ScrollingStateNode* node)
{
    m_nodesRemovedSinceLastCommit.add(node->scrollingNodeID());
    m_stateNodeMap.remove(node->scrollingNodeID());
    setHasChangedProperties();
}

void ScrollingStateTree::setRemovedNodes(HashSet<ScrollingNodeID> nodes)
{
    m_nodesRemovedSinceLastCommit = WTFMove(nodes);
}

ScrollingStateNode* ScrollingStateTree::stateNodeForID(ScrollingNodeID scrollLayerID) const
{
    if (!scrollLayerID)
        return nullptr;

    auto it = m_stateNodeMap.find(scrollLayerID);
    if (it == m_stateNodeMap.end())
        return nullptr;

    ASSERT(it->value->scrollingNodeID() == scrollLayerID);
    return it->value;
}

} // namespace WebCore

#ifndef NDEBUG
void showScrollingStateTree(const WebCore::ScrollingStateTree* tree)
{
    if (!tree)
        return;

    auto rootNode = tree->rootStateNode();
    if (!rootNode) {
        fprintf(stderr, "Scrolling state tree %p with no root node\n", tree);
        return;
    }

    String output = rootNode->scrollingStateTreeAsText(WebCore::ScrollingStateTreeAsTextBehaviorDebug);
    fprintf(stderr, "%s\n", output.utf8().data());
}

void showScrollingStateTree(const WebCore::ScrollingStateNode* node)
{
    if (!node)
        return;

    showScrollingStateTree(&node->scrollingStateTree());
}

#endif

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
