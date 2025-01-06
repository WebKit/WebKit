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
#include "ScrollingStatePluginHostingNode.h"
#include "ScrollingStatePluginScrollingNode.h"
#include "ScrollingStatePositionedNode.h"
#include "ScrollingStateStickyNode.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingStateTree);

static bool nodeTypeAndParentMatch(ScrollingStateNode& node, ScrollingNodeType nodeType, ScrollingStateNode* parentNode)
{
    return node.nodeType() == nodeType && node.parent() == parentNode;
}

static void nodeWasReattachedRecursive(ScrollingStateNode& node)
{
    // When a node is re-attached, the ScrollingTree is recreating the ScrollingNode from scratch, so we need to set all the dirty bits.
    node.setPropertyChangesAfterReattach();

    for (auto& child : node.children())
        nodeWasReattachedRecursive(child);
}

ScrollingStateTree::ScrollingStateTree(AsyncScrollingCoordinator* scrollingCoordinator)
    : m_scrollingCoordinator(scrollingCoordinator)
{
}

ScrollingStateTree::ScrollingStateTree(ScrollingStateTree&&) = default;

ScrollingStateTree::~ScrollingStateTree() = default;

std::optional<ScrollingStateTree> ScrollingStateTree::createAfterReconstruction(bool hasNewRootStateNode, bool hasChangedProperties, RefPtr<ScrollingStateFrameScrollingNode>&& rootStateNode)
{
    ScrollingStateTree tree(hasNewRootStateNode, hasChangedProperties, WTFMove(rootStateNode));

    bool allIdentifiersUnique { true };
    if (tree.m_rootStateNode) {
        tree.m_rootStateNode->traverse([&] (auto& node) {
            auto addResult = tree.m_stateNodeMap.add(node.scrollingNodeID(), node);
            if (!addResult.isNewEntry)
                allIdentifiersUnique = false;
        });
    }
    if (!allIdentifiersUnique)
        return std::nullopt;

    return { WTFMove(tree) };
}

ScrollingStateTree::ScrollingStateTree(bool hasNewRootStateNode, bool hasChangedProperties, RefPtr<ScrollingStateFrameScrollingNode>&& rootStateNode)
    : m_rootStateNode(WTFMove(rootStateNode))
    , m_hasNewRootStateNode(hasNewRootStateNode)
{
    setHasChangedProperties(hasChangedProperties);
}

void ScrollingStateTree::attachDeserializedNodes()
{
    // This needs to be done after the move constructor has been called because we deserialize
    // into a ScrollingStateTree then move to a std::unique_ptr<ScrollingStateTree> and if we
    // did this in the constructor, createAfterReconstruction would be setting nodes' tree pointers
    // to the wrong ScrollingStateTree.
    if (m_rootStateNode) {
        m_rootStateNode->attachAfterDeserialization(*this);
        ASSERT(m_rootStateNode->parentPointersAreCorrect());
    }
}

RefPtr<ScrollingStateFrameScrollingNode> ScrollingStateTree::rootStateNode() const
{
    return m_rootStateNode;
}

void ScrollingStateTree::setHasChangedProperties(bool changedProperties)
{
#if ENABLE(ASYNC_SCROLLING)
    bool gainedChangedProperties = !m_hasChangedProperties && changedProperties;
#endif

    m_hasChangedProperties = changedProperties;

#if ENABLE(ASYNC_SCROLLING)
    auto scrollingCoordinator = m_scrollingCoordinator.get();
    if (gainedChangedProperties && scrollingCoordinator)
        scrollingCoordinator->scrollingStateTreePropertiesChanged();
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
    case ScrollingNodeType::PluginScrolling:
        return ScrollingStatePluginScrollingNode::create(*this, nodeType, nodeID);
    case ScrollingNodeType::PluginHosting:
        return ScrollingStatePluginHostingNode::create(*this, nodeID);
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

    if (auto node = stateNodeForID(newNodeID)) {
        if (node->nodeType() == nodeType) {
            // If the node exists and is parented, unparent it. It may already be in m_unparentedNodes.
            unparentNode(newNodeID);
            return newNodeID;
        }

#if ENABLE(ASYNC_SCROLLING)
        // If the type has changed, we need to destroy and recreate the node with a new ID.
        if (nodeType != node->nodeType()) {
            unparentChildrenAndDestroyNode(newNodeID);
            if (auto scrollingCoordinator = m_scrollingCoordinator.get())
                newNodeID = scrollingCoordinator->uniqueScrollingNodeID();
            else
                ASSERT_NOT_REACHED();
        }
#endif
    }

    auto stateNode = createNode(nodeType, newNodeID);
    addNode(stateNode);
    m_unparentedNodes.add(newNodeID, WTFMove(stateNode));
    return newNodeID;
}

std::optional<ScrollingNodeID> ScrollingStateTree::insertNode(ScrollingNodeType nodeType, ScrollingNodeID newNodeID, std::optional<ScrollingNodeID> parentID, size_t childIndex)
{
    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " insertNode " << newNodeID << " in parent " << parentID << " at " << childIndex);

    if (auto node = stateNodeForID(newNodeID)) {
        auto parent = stateNodeForID(parentID);
        if (nodeTypeAndParentMatch(*node, nodeType, parent.get())) {
            if (!parentID)
                return newNodeID;

            if (parent->childAtIndex(childIndex) == node)
                return newNodeID;

            parent->removeChild(*node);

            if (childIndex == notFound)
                parent->appendChild(node.releaseNonNull());
            else
                parent->insertChild(node.releaseNonNull(), childIndex);

            return newNodeID;
        }

#if ENABLE(ASYNC_SCROLLING)
        // If the type has changed, we need to destroy and recreate the node with a new ID.
        if (nodeType != node->nodeType()) {
            if (auto scrollingCoordinator = m_scrollingCoordinator.get())
                newNodeID = scrollingCoordinator->uniqueScrollingNodeID();
            else
                ASSERT_NOT_REACHED();
        }
#endif

        // The node is being re-parented. To do that, we'll remove it, and then create a new node.
        unparentNode(newNodeID);
    }

    RefPtr<ScrollingStateNode> newNode;
    if (!parentID) {
        RELEASE_ASSERT(nodeType == ScrollingNodeType::MainFrame || nodeType == ScrollingNodeType::Subframe);
        ASSERT(!childIndex || childIndex == notFound);
        // If we're resetting the root node, we should clear the UncheckedKeyHashMap and destroy the current children.
        clear();

        setRootStateNode(ScrollingStateFrameScrollingNode::create(*this, nodeType, newNodeID));
        newNode = rootStateNode();
        m_hasNewRootStateNode = true;
    } else {
        auto parent = stateNodeForID(parentID);
        if (!parent) {
            ASSERT_NOT_REACHED();
            return std::nullopt;
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

void ScrollingStateTree::unparentNode(std::optional<ScrollingNodeID> nodeID)
{
    if (!nodeID)
        return;

    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " unparentNode " << *nodeID);

    // The node may not be found if clear() was recently called.
    RefPtr protectedNode = m_stateNodeMap.get(*nodeID);
    if (!protectedNode)
        return;

    if (protectedNode == m_rootStateNode)
        m_rootStateNode = nullptr;

    protectedNode->removeFromParent();
    m_unparentedNodes.add(*nodeID, WTFMove(protectedNode));
}

void ScrollingStateTree::unparentChildrenAndDestroyNode(std::optional<ScrollingNodeID> nodeID)
{
    if (!nodeID)
        return;

    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " unparentChildrenAndDestroyNode " << *nodeID);

    // The node may not be found if clear() was recently called.
    RefPtr protectedNode = m_stateNodeMap.take(*nodeID);
    if (!protectedNode)
        return;

    if (protectedNode == m_rootStateNode)
        m_rootStateNode = nullptr;

    for (auto child : protectedNode->takeChildren()) {
        child->removeFromParent();
        LOG_WITH_STREAM(ScrollingTree, stream << " moving " << child->scrollingNodeID() << " to unparented nodes");
        m_unparentedNodes.add(child->scrollingNodeID(), WTFMove(child));
    }
    
    protectedNode->removeFromParent();
    m_unparentedNodes.remove(*nodeID);
    willRemoveNode(*protectedNode);
}

void ScrollingStateTree::detachAndDestroySubtree(std::optional<ScrollingNodeID> nodeID)
{
    if (!nodeID)
        return;

    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateTree " << this << " detachAndDestroySubtree " << *nodeID);

    // The node may not be found if clear() was recently called.
    auto node = m_stateNodeMap.take(*nodeID);
    if (!node)
        return;

    // If the node was unparented, remove it from m_unparentedNodes (keeping it alive until this function returns).
    auto unparentedNode = m_unparentedNodes.take(*nodeID);
    removeNodeAndAllDescendants(*node);
}

void ScrollingStateTree::clear()
{
    if (auto node = rootStateNode())
        removeNodeAndAllDescendants(*node);

    m_stateNodeMap.clear();
    m_unparentedNodes.clear();
}

std::unique_ptr<ScrollingStateTree> ScrollingStateTree::commit(LayerRepresentation::Type preferredLayerRepresentation)
{
    ASSERT(isValid());

    if (!m_unparentedNodes.isEmpty()) {
        // We expect temporarily to have unparented nodes when committing when connecting across iframe boundaries, but unparented nodes should not stick around for a long time.
        LOG(ScrollingTree, "Committing with %u unparented nodes", m_unparentedNodes.size());
    }

    // This function clones and resets the current state tree, but leaves the tree structure intact.
    auto treeStateClone = makeUnique<ScrollingStateTree>();
    treeStateClone->setPreferredLayerRepresentation(preferredLayerRepresentation);

    if (m_rootStateNode)
        treeStateClone->setRootStateNode(downcast<ScrollingStateFrameScrollingNode>(m_rootStateNode->cloneAndReset(*treeStateClone)));

    // Now the clone tree has changed properties, and the original tree does not.
    treeStateClone->m_hasChangedProperties = std::exchange(m_hasChangedProperties, false);
    treeStateClone->m_hasNewRootStateNode = std::exchange(m_hasNewRootStateNode, false);

    return treeStateClone;
}

void ScrollingStateTree::traverse(const ScrollingStateNode& node, const Function<void(const ScrollingStateNode&)>& traversalFunc) const
{
    traversalFunc(node);

    for (auto childNode : node.children())
        traverse(childNode, traversalFunc);
}

bool ScrollingStateTree::isValid() const
{
    if (!m_rootStateNode)
        return true;

    bool isValid = true;
    traverse(*m_rootStateNode, [&](const ScrollingStateNode& node) {
        switch (node.nodeType()) {
        case ScrollingNodeType::MainFrame:
            break;
        case ScrollingNodeType::Subframe:
            break;
        case ScrollingNodeType::FrameHosting:
            break;
        case ScrollingNodeType::PluginScrolling:
            break;
        case ScrollingNodeType::PluginHosting:
            break;
        case ScrollingNodeType::Overflow:
            break;
        case ScrollingNodeType::OverflowProxy: {
            auto& proxyNode = downcast<ScrollingStateOverflowScrollProxyNode>(node);
            if (!proxyNode.overflowScrollingNode() || !nodeMap().contains(*proxyNode.overflowScrollingNode())) {
                LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStateOverflowScrollProxyNode " << node.scrollingNodeID() << " refers to non-existant overflow node " << proxyNode.overflowScrollingNode());
                isValid = false;
            }
            break;
        }
        case ScrollingNodeType::Fixed:
            break;
        case ScrollingNodeType::Sticky:
            break;
        case ScrollingNodeType::Positioned: {
            auto& positionedNode = downcast<ScrollingStatePositionedNode>(node);
            for (auto relatedNodeID : positionedNode.relatedOverflowScrollingNodes()) {
                if (!nodeMap().contains(relatedNodeID)) {
                    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingStatePositionedNode " << node.scrollingNodeID() << " refers to non-existant overflow node " << relatedNodeID);
                    isValid = false;
                }
            }
            break;
        }
        }
    });
    
    return isValid;
}

void ScrollingStateTree::setRootStateNode(Ref<ScrollingStateFrameScrollingNode>&& rootStateNode)
{
    m_rootStateNode = WTFMove(rootStateNode);
}

void ScrollingStateTree::addNode(ScrollingStateNode& node)
{
    m_stateNodeMap.add(node.scrollingNodeID(), node);
}

void ScrollingStateTree::removeNodeAndAllDescendants(ScrollingStateNode& node)
{
    auto parent = node.parent();

    recursiveNodeWillBeRemoved(node);

    if (&node == m_rootStateNode)
        m_rootStateNode = nullptr;
    else if (parent) {
        auto& children = parent->children();
        size_t index = children.findIf([&](auto& child) {
            return child.ptr() == &node;
        });
        if (index != notFound)
            children.remove(index);
        else
            ASSERT_NOT_REACHED();
    }
}

void ScrollingStateTree::recursiveNodeWillBeRemoved(ScrollingStateNode& currentNode)
{
    currentNode.setParent(nullptr);
    willRemoveNode(currentNode);

    for (auto& child : currentNode.children())
        recursiveNodeWillBeRemoved(child);
}

void ScrollingStateTree::willRemoveNode(ScrollingStateNode& node)
{
    m_stateNodeMap.remove(node.scrollingNodeID());
    setHasChangedProperties();
}

RefPtr<ScrollingStateNode> ScrollingStateTree::stateNodeForID(std::optional<ScrollingNodeID> nodeID) const
{
    RefPtr node = nodeID ? m_stateNodeMap.get(*nodeID) : nullptr;
    RELEASE_ASSERT(!node || node->scrollingNodeID() == nodeID);
    return node;
}

static void reconcileLayerPositionsRecursive(ScrollingStateNode& currentNode, const LayoutRect& layoutViewport, ScrollingLayerPositionAction action)
{
    currentNode.reconcileLayerPositionForViewportRect(layoutViewport, action);
    
    for (auto& child : currentNode.children()) {
        // Never need to cross frame boundaries, since viewport rect reconciliation is per frame.
        if (is<ScrollingStateFrameScrollingNode>(child))
            continue;

        reconcileLayerPositionsRecursive(child, layoutViewport, action);
    }
}

void ScrollingStateTree::reconcileViewportConstrainedLayerPositions(std::optional<ScrollingNodeID> scrollingNodeID, const LayoutRect& layoutViewport, ScrollingLayerPositionAction action)
{
    if (auto scrollingNode = stateNodeForID(scrollingNodeID))
        reconcileLayerPositionsRecursive(*scrollingNode, layoutViewport, action);
}

String ScrollingStateTree::scrollingStateTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    if (!rootStateNode())
        return emptyString();

    StringBuilder stateTreeAsString;
    stateTreeAsString.append(rootStateNode()->scrollingStateTreeAsText(behavior));
    if (!m_unparentedNodes.isEmpty())
        stateTreeAsString.append("\nunparented node count: "_s, m_unparentedNodes.size());
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
