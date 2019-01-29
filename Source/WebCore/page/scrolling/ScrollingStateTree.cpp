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
#include "Logging.h"
#include "ScrollingStateFixedNode.h"
#include "ScrollingStateFrameHostingNode.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingStateOverflowScrollingNode.h"
#include "ScrollingStateStickyNode.h"
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollingStateTree::ScrollingStateTree(AsyncScrollingCoordinator* scrollingCoordinator)
    : m_scrollingCoordinator(scrollingCoordinator)
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
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        return ScrollingStateFrameScrollingNode::create(*this, nodeType, nodeID);
    case ScrollingNodeType::FrameHosting:
        return ScrollingStateFrameHostingNode::create(*this, nodeID);
    case ScrollingNodeType::Overflow:
        return ScrollingStateOverflowScrollingNode::create(*this, nodeID);
    case ScrollingNodeType::Fixed:
        return ScrollingStateFixedNode::create(*this, nodeID);
    case ScrollingNodeType::Sticky:
        return ScrollingStateStickyNode::create(*this, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingStateFixedNode::create(*this, nodeID);
}

bool ScrollingStateTree::nodeTypeAndParentMatch(ScrollingStateNode& node, ScrollingNodeType nodeType, ScrollingStateNode* parentNode) const
{
    if (node.nodeType() != nodeType)
        return false;

    return node.parent() == parentNode;
}

ScrollingNodeID ScrollingStateTree::createUnparentedNode(ScrollingNodeType nodeType, ScrollingNodeID newNodeID)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateTree " << this << " createUnparentedNode " << newNodeID);

    if (auto* node = stateNodeForID(newNodeID)) {
        if (node->nodeType() == nodeType) {
            // If the node exists and is parented, unparent it. It may already be in m_unparentedNodes.
            unparentNode(newNodeID);
            return newNodeID;
        }

#if ENABLE(ASYNC_SCROLLING)
        // If the type has changed, we need to destroy and recreate the node with a new ID.
        if (nodeType != node->nodeType()) {
            unparentChildrenAndDestroyNode(newNodeID);
            newNodeID = m_scrollingCoordinator->uniqueScrollingNodeID();
        }
#endif
    }

    auto stateNode = createNode(nodeType, newNodeID);
    addNode(stateNode.get());
    m_unparentedNodes.add(newNodeID, WTFMove(stateNode));
    return newNodeID;
}

ScrollingNodeID ScrollingStateTree::insertNode(ScrollingNodeType nodeType, ScrollingNodeID newNodeID, ScrollingNodeID parentID, size_t childIndex)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateTree " << this << " insertNode " << newNodeID << " in parent " << parentID << " at " << childIndex);
    ASSERT(newNodeID);

    if (auto* node = stateNodeForID(newNodeID)) {
        auto* parent = stateNodeForID(parentID);
        if (nodeTypeAndParentMatch(*node, nodeType, parent)) {
            if (!parentID)
                return newNodeID;

            size_t currentIndex = parent->indexOfChild(*node);
            if (currentIndex == childIndex)
                return newNodeID;

            ASSERT(currentIndex != notFound);
            Ref<ScrollingStateNode> protectedNode(*node);
            parent->removeChildAtIndex(currentIndex);

            if (childIndex == notFound)
                parent->appendChild(WTFMove(protectedNode));
            else
                parent->insertChild(WTFMove(protectedNode), childIndex);

            return newNodeID;
        }

#if ENABLE(ASYNC_SCROLLING)
        // If the type has changed, we need to destroy and recreate the node with a new ID.
        if (nodeType != node->nodeType())
            newNodeID = m_scrollingCoordinator->uniqueScrollingNodeID();
#endif

        // The node is being re-parented. To do that, we'll remove it, and then create a new node.
        unparentNode(newNodeID);
    }

    ScrollingStateNode* newNode = nullptr;
    if (!parentID) {
        ASSERT(!childIndex || childIndex == notFound);
        // If we're resetting the root node, we should clear the HashMap and destroy the current children.
        clear();

        setRootStateNode(ScrollingStateFrameScrollingNode::create(*this, ScrollingNodeType::MainFrame, newNodeID));
        newNode = rootStateNode();
        m_hasNewRootStateNode = true;
    } else {
        auto* parent = stateNodeForID(parentID);
        if (!parent) {
            ASSERT_NOT_REACHED();
            return 0;
        }

        if (parentID) {
            if (auto unparentedNode = m_unparentedNodes.take(newNodeID)) {
                newNode = unparentedNode.get();
                if (childIndex == notFound)
                    parent->appendChild(unparentedNode.releaseNonNull());
                else
                    parent->insertChild(unparentedNode.releaseNonNull(), childIndex);
            }
        }

        if (!newNode) {
            auto stateNode = createNode(nodeType, newNodeID);
            newNode = stateNode.ptr();
            if (childIndex == notFound)
                parent->appendChild(WTFMove(stateNode));
            else
                parent->insertChild(WTFMove(stateNode), childIndex);
        }
    }

    addNode(*newNode);
    m_nodesRemovedSinceLastCommit.remove(newNodeID);
    return newNodeID;
}

void ScrollingStateTree::unparentNode(ScrollingNodeID nodeID)
{
    if (!nodeID)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateTree " << this << " unparentNode " << nodeID);

    // The node may not be found if clear() was recently called.
    RefPtr<ScrollingStateNode> protectedNode = m_stateNodeMap.get(nodeID);
    if (!protectedNode)
        return;

    if (protectedNode == m_rootStateNode)
        m_rootStateNode = nullptr;

    protectedNode->removeFromParent();
    m_unparentedNodes.add(nodeID, WTFMove(protectedNode));
}

void ScrollingStateTree::unparentChildrenAndDestroyNode(ScrollingNodeID nodeID)
{
    if (!nodeID)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateTree " << this << " unparentChildrenAndDestroyNode " << nodeID);

    // The node may not be found if clear() was recently called.
    RefPtr<ScrollingStateNode> protectedNode = m_stateNodeMap.take(nodeID);
    if (!protectedNode)
        return;

    if (protectedNode == m_rootStateNode)
        m_rootStateNode = nullptr;

    if (auto* children = protectedNode->children()) {
        for (auto child : *children) {
            child->removeFromParent();
            LOG_WITH_STREAM(Scrolling, stream << " moving " << child->scrollingNodeID() << " to unparented nodes");
            m_unparentedNodes.add(child->scrollingNodeID(), WTFMove(child));
        }
        children->clear();
    }
    
    protectedNode->removeFromParent();
    willRemoveNode(protectedNode.get());
}

void ScrollingStateTree::detachAndDestroySubtree(ScrollingNodeID nodeID)
{
    if (!nodeID)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateTree " << this << " detachAndDestroySubtree " << nodeID);

    // The node may not be found if clear() was recently called.
    auto* node = m_stateNodeMap.take(nodeID);
    if (!node)
        return;

    // If the node was unparented, remove it from m_unparentedNodes (keeping it alive until this function returns).
    auto unparentedNode = m_unparentedNodes.take(nodeID);
    removeNodeAndAllDescendants(node);
}

void ScrollingStateTree::clear()
{
    if (rootStateNode())
        removeNodeAndAllDescendants(rootStateNode());

    m_stateNodeMap.clear();
    m_unparentedNodes.clear();
}

std::unique_ptr<ScrollingStateTree> ScrollingStateTree::commit(LayerRepresentation::Type preferredLayerRepresentation)
{
    if (!m_unparentedNodes.isEmpty()) {
        // We expect temporarily to have unparented nodes when committing when connecting across iframe boundaries, but unparented nodes should not stick around for a long time.
        LOG(Scrolling, "Committing with %u unparented nodes", m_unparentedNodes.size());
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

void ScrollingStateTree::removeNodeAndAllDescendants(ScrollingStateNode* node)
{
    auto* parent = node->parent();

    recursiveNodeWillBeRemoved(node);

    if (node == m_rootStateNode)
        m_rootStateNode = nullptr;
    else if (parent) {
        ASSERT(parent->children());
        ASSERT(parent->children()->find(node) != notFound);
        if (auto* children = parent->children()) {
            size_t index = children->find(node);
            if (index != notFound)
                children->remove(index);
        }
    }
}

void ScrollingStateTree::recursiveNodeWillBeRemoved(ScrollingStateNode* currNode)
{
    currNode->setParent(nullptr);
    willRemoveNode(currNode);

    if (auto* children = currNode->children()) {
        for (auto& child : *children)
            recursiveNodeWillBeRemoved(child.get());
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
        WTFLogAlways("Scrolling state tree %p with no root node\n", tree);
        return;
    }

    String output = rootNode->scrollingStateTreeAsText(WebCore::ScrollingStateTreeAsTextBehaviorDebug);
    WTFLogAlways("%s\n", output.utf8().data());
}

void showScrollingStateTree(const WebCore::ScrollingStateNode* node)
{
    if (!node)
        return;

    showScrollingStateTree(&node->scrollingStateTree());
}

#endif

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
