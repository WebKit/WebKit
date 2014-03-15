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
#include "RemoteScrollingCoordinatorTransaction.h"

#include "ArgumentCoders.h"
#include "MessageDecoder.h"
#include "MessageEncoder.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/ScrollingStateFixedNode.h>
#include <WebCore/ScrollingStateScrollingNode.h>
#include <WebCore/ScrollingStateStickyNode.h>

#include <wtf/HashMap.h>

using namespace WebCore;

#if ENABLE(ASYNC_SCROLLING)

namespace IPC {

template<> struct ArgumentCoder<ScrollingStateNode> {
    static void encode(ArgumentEncoder&, const ScrollingStateNode&);
    static bool decode(ArgumentDecoder&, ScrollingStateNode&);
};

template<> struct ArgumentCoder<ScrollingStateScrollingNode> {
    static void encode(ArgumentEncoder&, const ScrollingStateScrollingNode&);
    static bool decode(ArgumentDecoder&, ScrollingStateScrollingNode&);
};

template<> struct ArgumentCoder<ScrollingStateFixedNode> {
    static void encode(ArgumentEncoder&, const ScrollingStateFixedNode&);
    static bool decode(ArgumentDecoder&, ScrollingStateFixedNode&);
};

template<> struct ArgumentCoder<ScrollingStateStickyNode> {
    static void encode(ArgumentEncoder&, const ScrollingStateStickyNode&);
    static bool decode(ArgumentDecoder&, ScrollingStateStickyNode&);
};

} // namespace IPC

using namespace IPC;

void ArgumentCoder<ScrollingStateNode>::encode(ArgumentEncoder& encoder, const ScrollingStateNode& node)
{
    encoder.encodeEnum(node.nodeType());
    encoder << node.scrollingNodeID();
    encoder << node.parentNodeID();
    encoder << node.changedProperties();
    
    if (node.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.layer());
}

bool ArgumentCoder<ScrollingStateNode>::decode(ArgumentDecoder& decoder, ScrollingStateNode& node)
{
    // nodeType, scrollingNodeID and parentNodeID have already been decoded by the caller in order to create the node.
    ScrollingStateNode::ChangedProperties changedProperties;
    if (!decoder.decode(changedProperties))
        return false;

    node.setChangedProperties(changedProperties);
    if (node.hasChangedProperty(ScrollingStateNode::ScrollLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setLayer(layerID);
    }

    return true;
}

#define SCROLLING_NODE_ENCODE(property, getter) \
    if (node.hasChangedProperty(ScrollingStateScrollingNode::property)) \
        encoder << node.getter();

#define SCROLLING_NODE_ENCODE_ENUM(property, getter) \
    if (node.hasChangedProperty(ScrollingStateScrollingNode::property)) \
        encoder.encodeEnum(node.getter());

void ArgumentCoder<ScrollingStateScrollingNode>::encode(ArgumentEncoder& encoder, const ScrollingStateScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    
    SCROLLING_NODE_ENCODE(ViewportSize, viewportSize)
    SCROLLING_NODE_ENCODE(TotalContentsSize, totalContentsSize)
    SCROLLING_NODE_ENCODE(ScrollPosition, scrollPosition)
    SCROLLING_NODE_ENCODE(ScrollOrigin, scrollOrigin)
    SCROLLING_NODE_ENCODE(FrameScaleFactor, frameScaleFactor)
    SCROLLING_NODE_ENCODE(NonFastScrollableRegion, nonFastScrollableRegion)
    SCROLLING_NODE_ENCODE(WheelEventHandlerCount, wheelEventHandlerCount)
    SCROLLING_NODE_ENCODE(ReasonsForSynchronousScrolling, synchronousScrollingReasons)
    SCROLLING_NODE_ENCODE(ScrollableAreaParams, scrollableAreaParameters)
    SCROLLING_NODE_ENCODE_ENUM(BehaviorForFixedElements, scrollBehaviorForFixedElements)
    SCROLLING_NODE_ENCODE(RequestedScrollPosition, requestedScrollPosition)
    SCROLLING_NODE_ENCODE(RequestedScrollPosition, requestedScrollPositionRepresentsProgrammaticScroll)
    SCROLLING_NODE_ENCODE(HeaderHeight, headerHeight)
    SCROLLING_NODE_ENCODE(FooterHeight, footerHeight)

    if (node.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.scrolledContentsLayer());

    if (node.hasChangedProperty(ScrollingStateScrollingNode::CounterScrollingLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.counterScrollingLayer());
}

#define SCROLLING_NODE_DECODE(property, type, setter) \
    if (node.hasChangedProperty(ScrollingStateScrollingNode::property)) { \
        type decodedValue; \
        if (!decoder.decode(decodedValue)) \
            return false; \
        node.setter(decodedValue); \
    }

#define SCROLLING_NODE_DECODE_ENUM(property, type, setter) \
    if (node.hasChangedProperty(ScrollingStateScrollingNode::property)) { \
        type decodedValue; \
        if (!decoder.decodeEnum(decodedValue)) \
            return false; \
        node.setter(decodedValue); \
    }

bool ArgumentCoder<ScrollingStateScrollingNode>::decode(ArgumentDecoder& decoder, ScrollingStateScrollingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    SCROLLING_NODE_DECODE(ViewportSize, FloatSize, setViewportSize);
    SCROLLING_NODE_DECODE(TotalContentsSize, IntSize, setTotalContentsSize);
    SCROLLING_NODE_DECODE(ScrollPosition, FloatPoint, setScrollPosition);
    SCROLLING_NODE_DECODE(ScrollOrigin, IntPoint, setScrollOrigin);
    SCROLLING_NODE_DECODE(FrameScaleFactor, float, setFrameScaleFactor);
    SCROLLING_NODE_DECODE(NonFastScrollableRegion, Region, setNonFastScrollableRegion);
    SCROLLING_NODE_DECODE(WheelEventHandlerCount, int, setWheelEventHandlerCount);
    SCROLLING_NODE_DECODE(ReasonsForSynchronousScrolling, SynchronousScrollingReasons, setSynchronousScrollingReasons);
    SCROLLING_NODE_DECODE(ScrollableAreaParams, ScrollableAreaParameters, setScrollableAreaParameters);
    SCROLLING_NODE_DECODE_ENUM(BehaviorForFixedElements, ScrollBehaviorForFixedElements, setScrollBehaviorForFixedElements);

    if (node.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition)) {
        FloatPoint scrollPosition;
        if (!decoder.decode(scrollPosition))
            return false;

        bool representsProgrammaticScroll;
        if (!decoder.decode(representsProgrammaticScroll))
            return false;

        node.setRequestedScrollPosition(scrollPosition, representsProgrammaticScroll);
    }

    SCROLLING_NODE_DECODE(HeaderHeight, int, setHeaderHeight);
    SCROLLING_NODE_DECODE(FooterHeight, int, setFooterHeight);

    if (node.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setScrolledContentsLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateScrollingNode::CounterScrollingLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setCounterScrollingLayer(layerID);
    }

    return true;
}

void ArgumentCoder<ScrollingStateFixedNode>::encode(ArgumentEncoder& encoder, const ScrollingStateFixedNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    
    if (node.hasChangedProperty(ScrollingStateFixedNode::ViewportConstraints))
        encoder << node.viewportConstraints();
}

bool ArgumentCoder<ScrollingStateFixedNode>::decode(ArgumentDecoder& decoder, ScrollingStateFixedNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    if (node.hasChangedProperty(ScrollingStateFixedNode::ViewportConstraints)) {
        FixedPositionViewportConstraints decodedValue;
        if (!decoder.decode(decodedValue))
            return false;
        node.updateConstraints(decodedValue);
    }

    return true;
}

void ArgumentCoder<ScrollingStateStickyNode>::encode(ArgumentEncoder& encoder, const ScrollingStateStickyNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    
    if (node.hasChangedProperty(ScrollingStateStickyNode::ViewportConstraints))
        encoder << node.viewportConstraints();
}

bool ArgumentCoder<ScrollingStateStickyNode>::decode(ArgumentDecoder& decoder, ScrollingStateStickyNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    if (node.hasChangedProperty(ScrollingStateStickyNode::ViewportConstraints)) {
        StickyPositionViewportConstraints decodedValue;
        if (!decoder.decode(decodedValue))
            return false;
        node.updateConstraints(decodedValue);
    }

    return true;
}

namespace WebKit {

static void encodeNodeAndDescendants(IPC::ArgumentEncoder& encoder, const ScrollingStateNode& stateNode)
{
    switch (stateNode.nodeType()) {
    case ScrollingNode:
        encoder << toScrollingStateScrollingNode(stateNode);
        break;
    case FixedNode:
        encoder << toScrollingStateFixedNode(stateNode);
        break;
    case StickyNode:
        encoder << toScrollingStateStickyNode(stateNode);
        break;
    }

    if (!stateNode.children())
        return;

    for (size_t i = 0; i < stateNode.children()->size(); ++i) {
        const OwnPtr<ScrollingStateNode>& child = stateNode.children()->at(i);
        encodeNodeAndDescendants(encoder, *child.get());
    }
}

void RemoteScrollingCoordinatorTransaction::encode(IPC::ArgumentEncoder& encoder) const
{
    int numNodes = m_scrollingStateTree ? m_scrollingStateTree->nodeCount() : 0;
    encoder << numNodes;
    
    bool hasNewRootNode = m_scrollingStateTree ? m_scrollingStateTree->hasNewRootStateNode() : false;
    encoder << hasNewRootNode;

    if (m_scrollingStateTree) {
        if (const ScrollingStateNode* rootNode = m_scrollingStateTree->rootStateNode())
            encodeNodeAndDescendants(encoder, *rootNode);

        encoder << m_scrollingStateTree->removedNodes();
    } else
        encoder << Vector<ScrollingNodeID>();
}

bool RemoteScrollingCoordinatorTransaction::decode(IPC::ArgumentDecoder& decoder, RemoteScrollingCoordinatorTransaction& transaction)
{
    return transaction.decode(decoder);
}

bool RemoteScrollingCoordinatorTransaction::decode(IPC::ArgumentDecoder& decoder)
{
    int numNodes;
    if (!decoder.decode(numNodes))
        return false;

    bool hasNewRootNode;
    if (!decoder.decode(hasNewRootNode))
        return false;
    
    m_scrollingStateTree = ScrollingStateTree::create();
    
    for (int i = 0; i < numNodes; ++i) {
        ScrollingNodeType nodeType;
        if (!decoder.decodeEnum(nodeType))
            return false;

        ScrollingNodeID nodeID;
        if (!decoder.decode(nodeID))
            return false;

        ScrollingNodeID parentNodeID;
        if (!decoder.decode(parentNodeID))
            return false;

        m_scrollingStateTree->attachNode(nodeType, nodeID, parentNodeID);
        ScrollingStateNode* newNode = m_scrollingStateTree->stateNodeForID(nodeID);
        ASSERT(newNode);
        ASSERT(!parentNodeID || newNode->parent());
        
        switch (nodeType) {
        case ScrollingNode:
            if (!decoder.decode(*toScrollingStateScrollingNode(newNode)))
                return false;
            break;
        case FixedNode:
            if (!decoder.decode(*toScrollingStateFixedNode(newNode)))
                return false;
            break;
        case StickyNode:
            if (!decoder.decode(*toScrollingStateStickyNode(newNode)))
                return false;
            break;
        }
    }

    m_scrollingStateTree->setHasNewRootStateNode(hasNewRootNode);

    // Removed nodes
    Vector<ScrollingNodeID> removedNodes;
    if (!decoder.decode(removedNodes))
        return false;
    
    if (removedNodes.size())
        m_scrollingStateTree->setRemovedNodes(removedNodes);

    return true;
}

} // namespace WebKit

#else // !ENABLE(ASYNC_SCROLLING)

namespace WebKit {

void RemoteScrollingCoordinatorTransaction::encode(IPC::ArgumentEncoder&) const
{
}

bool RemoteScrollingCoordinatorTransaction::decode(IPC::ArgumentDecoder& decoder, RemoteScrollingCoordinatorTransaction& transaction)
{
    return true;
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
