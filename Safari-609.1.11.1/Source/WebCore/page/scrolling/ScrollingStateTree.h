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

#pragma once

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingCoordinator.h"
#include "ScrollingStateNode.h"
#include <wtf/RefPtr.h>
 
namespace WebCore {

class AsyncScrollingCoordinator;
class ScrollingStateFrameScrollingNode;

// The ScrollingStateTree is a tree that managed ScrollingStateNodes. The nodes keep track of the current
// state of scrolling related properties. Whenever any properties change, the scrolling coordinator
// will be informed and will schedule a timer that will clone the new state tree and send it over to
// the scrolling thread, avoiding locking. 

class ScrollingStateTree {
    WTF_MAKE_FAST_ALLOCATED;
    friend class ScrollingStateNode;
public:
    WEBCORE_EXPORT ScrollingStateTree(AsyncScrollingCoordinator* = nullptr);
    WEBCORE_EXPORT ~ScrollingStateTree();

    ScrollingStateFrameScrollingNode* rootStateNode() const { return m_rootStateNode.get(); }
    WEBCORE_EXPORT ScrollingStateNode* stateNodeForID(ScrollingNodeID) const;

    ScrollingNodeID createUnparentedNode(ScrollingNodeType, ScrollingNodeID);
    WEBCORE_EXPORT ScrollingNodeID insertNode(ScrollingNodeType, ScrollingNodeID, ScrollingNodeID parentID, size_t childIndex);
    void unparentNode(ScrollingNodeID);
    void unparentChildrenAndDestroyNode(ScrollingNodeID);
    void detachAndDestroySubtree(ScrollingNodeID);
    void clear();

    // Copies the current tree state and clears the changed properties mask in the original.
    WEBCORE_EXPORT std::unique_ptr<ScrollingStateTree> commit(LayerRepresentation::Type preferredLayerRepresentation);

    WEBCORE_EXPORT void setHasChangedProperties(bool = true);
    bool hasChangedProperties() const { return m_hasChangedProperties; }

    bool hasNewRootStateNode() const { return m_hasNewRootStateNode; }
    void setHasNewRootStateNode(bool hasNewRoot) { m_hasNewRootStateNode = hasNewRoot; }

    unsigned nodeCount() const { return m_stateNodeMap.size(); }

    typedef HashMap<ScrollingNodeID, RefPtr<ScrollingStateNode>> StateNodeMap;
    const StateNodeMap& nodeMap() const { return m_stateNodeMap; }

    LayerRepresentation::Type preferredLayerRepresentation() const { return m_preferredLayerRepresentation; }
    void setPreferredLayerRepresentation(LayerRepresentation::Type representation) { m_preferredLayerRepresentation = representation; }

    void reconcileViewportConstrainedLayerPositions(ScrollingNodeID, const LayoutRect& viewportRect, ScrollingLayerPositionAction);

private:
    void setRootStateNode(Ref<ScrollingStateFrameScrollingNode>&&);
    void addNode(ScrollingStateNode&);

    void nodeWasReattachedRecursive(ScrollingStateNode&);

    Ref<ScrollingStateNode> createNode(ScrollingNodeType, ScrollingNodeID);

    bool nodeTypeAndParentMatch(ScrollingStateNode&, ScrollingNodeType, ScrollingStateNode* parentNode) const;

    void removeNodeAndAllDescendants(ScrollingStateNode*);

    void recursiveNodeWillBeRemoved(ScrollingStateNode*);
    void willRemoveNode(ScrollingStateNode*);

    void reconcileLayerPositionsRecursive(ScrollingStateNode&, const LayoutRect& viewportRect, ScrollingLayerPositionAction);

    AsyncScrollingCoordinator* m_scrollingCoordinator;
    // Contains all the nodes we know about (those in the m_rootStateNode tree, and in m_unparentedNodes subtrees).
    StateNodeMap m_stateNodeMap;
    // Owns roots of unparented subtrees.
    HashMap<ScrollingNodeID, RefPtr<ScrollingStateNode>> m_unparentedNodes;

    RefPtr<ScrollingStateFrameScrollingNode> m_rootStateNode;
    bool m_hasChangedProperties { false };
    bool m_hasNewRootStateNode { false };
    LayerRepresentation::Type m_preferredLayerRepresentation { LayerRepresentation::GraphicsLayerRepresentation };
};

} // namespace WebCore

#ifndef NDEBUG
void showScrollingStateTree(const WebCore::ScrollingStateTree*);
void showScrollingStateTree(const WebCore::ScrollingStateNode*);
#endif

#endif // ENABLE(ASYNC_SCROLLING)
