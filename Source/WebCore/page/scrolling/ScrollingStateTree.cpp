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

#if ENABLE(ASYNC_SCROLLING)

#include "AsyncScrollingCoordinator.h"
#include "Logging.h"
#include "ScrollingStateFixedNode.h"
#include "ScrollingStateFrameHostingNode.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingStateOverflowScrollProxyNode.h"
#include "ScrollingStateOverflowScrollingNode.h"
#include "ScrollingStatePositionedNode.h"
#include "ScrollingStateStickyNode.h"
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static bool nodeTypeAndParentMatch(ScrollingStateNode& node, ScrollingNodeType nodeType, ScrollingStateNode* parentNode)
{
    return node.nodeType() == nodeType && node.parent() == parentNode;
}

static void nodeWasReattachedRecursive(ScrollingStateNode& node)
{
    // When a node is re-attached, the ScrollingTree is recreating the ScrollingNode from scratch, so we need to set all the dirty bits.
    node.setPropertyChangesAfterReattach();

    if (auto* children = node.children()) {
        for (auto& child : *children)
            nodeWasReattachedRecursive(*child);
    }
}

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
    case ScrollingNodeType::OverflowProxy:
        return ScrollingStateOverflowScrollProxyNode::create(*this, nodeID);
    case ScrollingNodeType::Fixed:
        return ScrollingStateFixedNode::create(*this, nodeID);
    case ScrollingNodeType::Sticky:
        return ScrollingStateStickyNode::create(*this, nodeID);
    case ScrollingNodeType::Positioned:
        return ScrollingStatePositionedNode::create(*this, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingStateFixedNode::create(*this, nodeID);
}

ScrollingNodeID ScrollingStateTree::createUnparentedNode(ScrollingNodeType nodeType, ScrollingNodeID newNodeID)
{
    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " createUnparentedNode " << newNodeID);

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
    addNode(stateNode);
    m_unparentedNodes.add(newNodeID, WTFMove(stateNode));
    return newNodeID;
}

ScrollingNodeID ScrollingStateTree::insertNode(ScrollingNodeType nodeType, ScrollingNodeID newNodeID, ScrollingNodeID parentID, size_t childIndex)
{
    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " insertNode " << newNodeID << " in parent " << parentID << " at " << childIndex);
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
            Ref protectedNode { *node };
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
        RELEASE_ASSERT(nodeType == ScrollingNodeType::MainFrame);
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

        ASSERT(parentID);
        if (auto unparentedNode = m_unparentedNodes.take(newNodeID)) {
            LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " insertNode reattaching node " << newNodeID);
            newNode = unparentedNode.get();
            nodeWasReattachedRecursive(*unparentedNode);

            if (childIndex == notFound)
                parent->appendChild(unparentedNode.releaseNonNull());
            else
                parent->insertChild(unparentedNode.releaseNonNull(), childIndex);
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
    return newNodeID;
}

void ScrollingStateTree::unparentNode(ScrollingNodeID nodeID)
{
    if (!nodeID)
        return;

    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " unparentNode " << nodeID);

    // The node may not be found if clear() was recently called.
    RefPtr protectedNode = m_stateNodeMap.get(nodeID);
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

    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " unparentChildrenAndDestroyNode " << nodeID);

    // The node may not be found if clear() was recently called.
    RefPtr protectedNode = m_stateNodeMap.take(nodeID);
    if (!protectedNode)
        return;

    if (protectedNode == m_rootStateNode)
        m_rootStateNode = nullptr;

    if (auto isolatedChildren = protectedNode->takeChildren()) {
        for (auto child : *isolatedChildren) {
            child->removeFromParent();
            LOG_WITH_STREAM(ScrollingTree, stream << " moving " << child->scrollingNodeID() << " to unparented nodes");
            m_unparentedNodes.add(child->scrollingNodeID(), WTFMove(child));
        }
    }
    
    protectedNode->removeFromParent();
    m_unparentedNodes.remove(nodeID);
    willRemoveNode(*protectedNode);
}

void ScrollingStateTree::detachAndDestroySubtree(ScrollingNodeID nodeID)
{
    if (!nodeID)
        return;

    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " detachAndDestroySubtree " << nodeID);

    // The node may not be found if clear() was recently called.
    auto node = m_stateNodeMap.take(nodeID);
    if (!node)
        return;

    // If the node was unparented, remove it from m_unparentedNodes (keeping it alive until this function returns).
    auto unparentedNode = m_unparentedNodes.take(nodeID);
    removeNodeAndAllDescendants(*node);
}

void ScrollingStateTree::clear()
{
    if (auto* node = rootStateNode())
        removeNodeAndAllDescendants(*node);

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
    auto treeStateClone = makeUnique<ScrollingStateTree>();
    treeStateClone->setPreferredLayerRepresentation(preferredLayerRepresentation);

    if (m_rootStateNode)
        treeStateClone->setRootStateNode(static_reference_cast<ScrollingStateFrameScrollingNode>(m_rootStateNode->cloneAndReset(*treeStateClone)));

    // Now the clone tree has changed properties, and the original tree does not.
    treeStateClone->m_hasChangedProperties = std::exchange(m_hasChangedProperties, false);
    treeStateClone->m_hasNewRootStateNode = std::exchange(m_hasNewRootStateNode, false);

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

void ScrollingStateTree::removeNodeAndAllDescendants(ScrollingStateNode& node)
{
    auto* parent = node.parent();

    recursiveNodeWillBeRemoved(node);

    if (&node == m_rootStateNode)
        m_rootStateNode = nullptr;
    else if (parent) {
        ASSERT(parent->children());
        ASSERT(parent->children()->contains(&node));
        if (auto* children = parent->children()) {
            if (size_t index = children->find(&node); index != notFound)
                children->remove(index);
        }
    }
}

void ScrollingStateTree::recursiveNodeWillBeRemoved(ScrollingStateNode& currentNode)
{
    currentNode.setParent(nullptr);
    willRemoveNode(currentNode);

    if (auto* children = currentNode.children()) {
        for (auto& child : *children)
            recursiveNodeWillBeRemoved(*child);
    }
}

void ScrollingStateTree::willRemoveNode(ScrollingStateNode& node)
{
    m_stateNodeMap.remove(node.scrollingNodeID());
    setHasChangedProperties();
}

ScrollingStateNode* ScrollingStateTree::stateNodeForID(ScrollingNodeID scrollLayerID) const
{
    auto* node = scrollLayerID ? m_stateNodeMap.get(scrollLayerID) : nullptr;
    ASSERT(!node || node->scrollingNodeID() == scrollLayerID);
    return node;
}

static void reconcileLayerPositionsRecursive(ScrollingStateNode& currentNode, const LayoutRect& layoutViewport, ScrollingLayerPositionAction action)
{
    currentNode.reconcileLayerPositionForViewportRect(layoutViewport, action);

    if (!currentNode.children())
        return;
    
    for (auto& child : *currentNode.children()) {
        // Never need to cross frame boundaries, since viewport rect reconciliation is per frame.
        if (is<ScrollingStateFrameScrollingNode>(child))
            continue;

        reconcileLayerPositionsRecursive(*child, layoutViewport, action);
    }
}

void ScrollingStateTree::reconcileViewportConstrainedLayerPositions(ScrollingNodeID scrollingNodeID, const LayoutRect& layoutViewport, ScrollingLayerPositionAction action)
{
    if (auto* scrollingNode = stateNodeForID(scrollingNodeID))
        reconcileLayerPositionsRecursive(*scrollingNode, layoutViewport, action);
}

String ScrollingStateTree::scrollingStateTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    if (!rootStateNode())
        return emptyString();

    StringBuilder stateTreeAsString;
    stateTreeAsString.append(rootStateNode()->scrollingStateTreeAsText(behavior));
    if (!m_unparentedNodes.isEmpty())
        stateTreeAsString.append("\nunparented node count: ", m_unparentedNodes.size());
    return stateTreeAsString.toString();
}

} // namespace WebCore

#ifndef NDEBUG
void showScrollingStateTree(const WebCore::ScrollingStateTree& tree)
{
    auto rootNode = tree.rootStateNode();
    if (!rootNode) {
        WTFLogAlways("Scrolling state tree %p with no root node\n", &tree);
        return;
    }

    String output = rootNode->scrollingStateTreeAsText(WebCore::debugScrollingStateTreeAsTextBehaviors);
    WTFLogAlways("%s\n", output.utf8().data());
}

void showScrollingStateTree(const WebCore::ScrollingStateNode& node)
{
    showScrollingStateTree(node.scrollingStateTree());
}

#endif

#endif // ENABLE(ASYNC_SCROLLING)
