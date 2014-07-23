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
#include <WebCore/ScrollingStateFrameScrollingNode.h>
#include <WebCore/ScrollingStateOverflowScrollingNode.h>
#include <WebCore/ScrollingStateStickyNode.h>
#include <WebCore/ScrollingStateTree.h>
#include <WebCore/TextStream.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

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
    
template<> struct ArgumentCoder<ScrollingStateFrameScrollingNode> {
    static void encode(ArgumentEncoder&, const ScrollingStateFrameScrollingNode&);
    static bool decode(ArgumentDecoder&, ScrollingStateFrameScrollingNode&);
};
    
template<> struct ArgumentCoder<ScrollingStateOverflowScrollingNode> {
    static void encode(ArgumentEncoder&, const ScrollingStateOverflowScrollingNode&);
    static bool decode(ArgumentDecoder&, ScrollingStateOverflowScrollingNode&);
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
    if (node.hasChangedProperty(property)) \
        encoder << node.getter();

#define SCROLLING_NODE_ENCODE_ENUM(property, getter) \
    if (node.hasChangedProperty(property)) \
        encoder.encodeEnum(node.getter());

void ArgumentCoder<ScrollingStateScrollingNode>::encode(ArgumentEncoder& encoder, const ScrollingStateScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ScrollableAreaSize, scrollableAreaSize)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::TotalContentsSize, totalContentsSize)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ReachableContentsSize, reachableContentsSize)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ScrollPosition, scrollPosition)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ScrollOrigin, scrollOrigin)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ScrollableAreaParams, scrollableAreaParameters)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::RequestedScrollPosition, requestedScrollPosition)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::RequestedScrollPosition, requestedScrollPositionRepresentsProgrammaticScroll)
}

void ArgumentCoder<ScrollingStateFrameScrollingNode>::encode(ArgumentEncoder& encoder, const ScrollingStateFrameScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateScrollingNode&>(node);
    
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::FrameScaleFactor, frameScaleFactor)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::NonFastScrollableRegion, nonFastScrollableRegion)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::WheelEventHandlerCount, wheelEventHandlerCount)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::ReasonsForSynchronousScrolling, synchronousScrollingReasons)
    SCROLLING_NODE_ENCODE_ENUM(ScrollingStateFrameScrollingNode::BehaviorForFixedElements, scrollBehaviorForFixedElements)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::HeaderHeight, headerHeight)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::FooterHeight, footerHeight)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::TopContentInset, topContentInset)

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::ScrolledContentsLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.scrolledContentsLayer());

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.counterScrollingLayer());

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.insetClipLayer());

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.contentShadowLayer());
}

void ArgumentCoder<ScrollingStateOverflowScrollingNode>::encode(ArgumentEncoder& encoder, const ScrollingStateOverflowScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateScrollingNode&>(node);
    
    if (node.hasChangedProperty(ScrollingStateOverflowScrollingNode::ScrolledContentsLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.scrolledContentsLayer());
}

#define SCROLLING_NODE_DECODE(property, type, setter) \
    if (node.hasChangedProperty(property)) { \
        type decodedValue; \
        if (!decoder.decode(decodedValue)) \
            return false; \
        node.setter(decodedValue); \
    }

#define SCROLLING_NODE_DECODE_ENUM(property, type, setter) \
    if (node.hasChangedProperty(property)) { \
        type decodedValue; \
        if (!decoder.decodeEnum(decodedValue)) \
            return false; \
        node.setter(decodedValue); \
    }

bool ArgumentCoder<ScrollingStateScrollingNode>::decode(ArgumentDecoder& decoder, ScrollingStateScrollingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::ScrollableAreaSize, FloatSize, setScrollableAreaSize);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::TotalContentsSize, FloatSize, setTotalContentsSize);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::ReachableContentsSize, FloatSize, setReachableContentsSize);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::ScrollPosition, FloatPoint, setScrollPosition);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::ScrollOrigin, IntPoint, setScrollOrigin);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::ScrollableAreaParams, ScrollableAreaParameters, setScrollableAreaParameters);
    
    if (node.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition)) {
        FloatPoint scrollPosition;
        if (!decoder.decode(scrollPosition))
            return false;

        bool representsProgrammaticScroll;
        if (!decoder.decode(representsProgrammaticScroll))
            return false;

        node.setRequestedScrollPosition(scrollPosition, representsProgrammaticScroll);
    }

    return true;
}

bool ArgumentCoder<ScrollingStateFrameScrollingNode>::decode(ArgumentDecoder& decoder, ScrollingStateFrameScrollingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateScrollingNode&>(node)))
        return false;

    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::FrameScaleFactor, float, setFrameScaleFactor);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::NonFastScrollableRegion, Region, setNonFastScrollableRegion);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::WheelEventHandlerCount, int, setWheelEventHandlerCount);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::ReasonsForSynchronousScrolling, SynchronousScrollingReasons, setSynchronousScrollingReasons);
    SCROLLING_NODE_DECODE_ENUM(ScrollingStateFrameScrollingNode::BehaviorForFixedElements, ScrollBehaviorForFixedElements, setScrollBehaviorForFixedElements);

    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::HeaderHeight, int, setHeaderHeight);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::FooterHeight, int, setFooterHeight);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::TopContentInset, float, setTopContentInset);

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::ScrolledContentsLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setScrolledContentsLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setCounterScrollingLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setInsetClipLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setContentShadowLayer(layerID);
    }

    return true;
}

bool ArgumentCoder<ScrollingStateOverflowScrollingNode>::decode(ArgumentDecoder& decoder, ScrollingStateOverflowScrollingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateScrollingNode&>(node)))
        return false;

    if (node.hasChangedProperty(ScrollingStateOverflowScrollingNode::ScrolledContentsLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setScrolledContentsLayer(layerID);
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

static void encodeNodeAndDescendants(IPC::ArgumentEncoder& encoder, const ScrollingStateNode& stateNode, int& encodedNodeCount)
{
    ++encodedNodeCount;

    switch (stateNode.nodeType()) {
    case FrameScrollingNode:
        encoder << toScrollingStateFrameScrollingNode(stateNode);
        break;
    case OverflowScrollingNode:
        encoder << toScrollingStateOverflowScrollingNode(stateNode);
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

    for (const auto& child : *stateNode.children())
        encodeNodeAndDescendants(encoder, *child.get(), encodedNodeCount);
}

void RemoteScrollingCoordinatorTransaction::encode(IPC::ArgumentEncoder& encoder) const
{
    int numNodes = m_scrollingStateTree ? m_scrollingStateTree->nodeCount() : 0;
    encoder << numNodes;
    
    bool hasNewRootNode = m_scrollingStateTree ? m_scrollingStateTree->hasNewRootStateNode() : false;
    encoder << hasNewRootNode;

    if (m_scrollingStateTree) {
        encoder << m_scrollingStateTree->hasChangedProperties();

        int numNodesEncoded = 0;
        if (const ScrollingStateNode* rootNode = m_scrollingStateTree->rootStateNode())
            encodeNodeAndDescendants(encoder, *rootNode, numNodesEncoded);

        ASSERT_UNUSED(numNodesEncoded, numNodesEncoded == numNodes);
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

    bool hasChangedProperties;
    if (!decoder.decode(hasChangedProperties))
        return false;

    m_scrollingStateTree->setHasChangedProperties(hasChangedProperties);

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
        case FrameScrollingNode:
            if (!decoder.decode(*toScrollingStateFrameScrollingNode(newNode)))
                return false;
            break;
        case OverflowScrollingNode:
            if (!decoder.decode(*toScrollingStateOverflowScrollingNode(newNode)))
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
    HashSet<ScrollingNodeID> removedNodes;
    if (!decoder.decode(removedNodes))
        return false;
    
    if (removedNodes.size())
        m_scrollingStateTree->setRemovedNodes(removedNodes);

    return true;
}

#if !defined(NDEBUG) || !LOG_DISABLED

class RemoteScrollingTreeTextStream : public TextStream {
public:
    using TextStream::operator<<;

    RemoteScrollingTreeTextStream()
        : m_indent(0)
    {
    }

    RemoteScrollingTreeTextStream& operator<<(FloatRect);
    RemoteScrollingTreeTextStream& operator<<(ScrollingNodeType);

    RemoteScrollingTreeTextStream& operator<<(const FixedPositionViewportConstraints&);
    RemoteScrollingTreeTextStream& operator<<(const StickyPositionViewportConstraints&);

    void dump(const ScrollingStateTree&, bool changedPropertiesOnly = true);

    void dump(const ScrollingStateNode&, bool changedPropertiesOnly = true);
    void dump(const ScrollingStateScrollingNode&, bool changedPropertiesOnly = true);
    void dump(const ScrollingStateFrameScrollingNode&, bool changedPropertiesOnly = true);
    void dump(const ScrollingStateOverflowScrollingNode&, bool changedPropertiesOnly = true);
    void dump(const ScrollingStateFixedNode&, bool changedPropertiesOnly = true);
    void dump(const ScrollingStateStickyNode&, bool changedPropertiesOnly = true);

    void increaseIndent() { ++m_indent; }
    void decreaseIndent() { --m_indent; ASSERT(m_indent >= 0); }

    void writeIndent();

private:
    void recursiveDumpNodes(const ScrollingStateNode&, bool changedPropertiesOnly);

    int m_indent;
};

void RemoteScrollingTreeTextStream::writeIndent()
{
    for (int i = 0; i < m_indent; ++i)
        *this << "  ";
}

template <class T>
static void dumpProperty(RemoteScrollingTreeTextStream& ts, String name, T value)
{
    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(" << name << " ";
    ts << value << ")";
    ts.decreaseIndent();
}

RemoteScrollingTreeTextStream& RemoteScrollingTreeTextStream::operator<<(FloatRect rect)
{
    RemoteScrollingTreeTextStream& ts = *this;
    ts << rect.x() << " " << rect.y() << " " << rect.width() << " " << rect.height();
    return ts;
}

RemoteScrollingTreeTextStream& RemoteScrollingTreeTextStream::operator<<(ScrollingNodeType nodeType)
{
    RemoteScrollingTreeTextStream& ts = *this;

    switch (nodeType) {
    case FrameScrollingNode: ts << "frame-scrolling"; break;
    case OverflowScrollingNode: ts << "overflow-scrolling"; break;
    case FixedNode: ts << "fixed"; break;
    case StickyNode: ts << "sticky"; break;
    }

    return ts;
}

RemoteScrollingTreeTextStream& RemoteScrollingTreeTextStream::operator<<(const FixedPositionViewportConstraints& constraints)
{
    RemoteScrollingTreeTextStream& ts = *this;

    dumpProperty(ts, "viewport-rect-at-last-layout", constraints.viewportRectAtLastLayout());
    dumpProperty(ts, "layer-position-at-last-layout", constraints.layerPositionAtLastLayout());

    return ts;
}

RemoteScrollingTreeTextStream& RemoteScrollingTreeTextStream::operator<<(const StickyPositionViewportConstraints& constraints)
{
    RemoteScrollingTreeTextStream& ts = *this;

    dumpProperty(ts, "sticky-position-at-last-layout", constraints.stickyOffsetAtLastLayout());
    dumpProperty(ts, "layer-position-at-last-layout", constraints.layerPositionAtLastLayout());

    return ts;
}

void RemoteScrollingTreeTextStream::dump(const ScrollingStateNode& node, bool changedPropertiesOnly)
{
    RemoteScrollingTreeTextStream& ts = *this;

    ts << "(node " << node.scrollingNodeID();

    dumpProperty(ts, "type", node.nodeType());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        dumpProperty(ts, "layer", static_cast<GraphicsLayer::PlatformLayerID>(node.layer()));
    
    switch (node.nodeType()) {
    case FrameScrollingNode:
        dump(toScrollingStateFrameScrollingNode(node), changedPropertiesOnly);
        break;
    case OverflowScrollingNode:
        dump(toScrollingStateOverflowScrollingNode(node), changedPropertiesOnly);
        break;
    case FixedNode:
        dump(toScrollingStateFixedNode(node), changedPropertiesOnly);
        break;
    case StickyNode:
        dump(toScrollingStateStickyNode(node), changedPropertiesOnly);
        break;
    }
}
    
void RemoteScrollingTreeTextStream::dump(const ScrollingStateScrollingNode& node, bool changedPropertiesOnly)
{
    RemoteScrollingTreeTextStream& ts = *this;
    
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaSize))
        dumpProperty(ts, "scrollable-area-size", node.scrollableAreaSize());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize))
        dumpProperty(ts, "total-contents-size", node.totalContentsSize());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::ReachableContentsSize))
        dumpProperty(ts, "reachable-contents-size", node.reachableContentsSize());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::ScrollPosition))
        dumpProperty(ts, "scroll-position", node.scrollPosition());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::ScrollOrigin))
        dumpProperty(ts, "scroll-origin", node.scrollOrigin());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition)) {
        dumpProperty(ts, "requested-scroll-position", node.requestedScrollPosition());
        dumpProperty(ts, "requested-scroll-position-is-programatic", node.requestedScrollPositionRepresentsProgrammaticScroll());
    }
}
    
void RemoteScrollingTreeTextStream::dump(const ScrollingStateFrameScrollingNode& node, bool changedPropertiesOnly)
{
    RemoteScrollingTreeTextStream& ts = *this;
    
    dump(static_cast<const ScrollingStateScrollingNode&>(node), changedPropertiesOnly);
    
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::FrameScaleFactor))
        dumpProperty(ts, "frame-scale-factor", node.frameScaleFactor());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::NonFastScrollableRegion)) {
        ts << "\n";
        ts.increaseIndent();
        ts.writeIndent();
        ts << "(non-fast-scrollable-region";
        ts.increaseIndent();
        for (auto rect : node.nonFastScrollableRegion().rects()) {
            ts << "\n";
            ts.writeIndent();
            ts << rect;
        }
        ts << ")\n";
        ts.decreaseIndent();
        ts.decreaseIndent();
    }

    // FIXME: dump wheelEventHandlerCount
    // FIXME: dump synchronousScrollingReasons
    // FIXME: dump scrollableAreaParameters
    // FIXME: dump scrollBehaviorForFixedElements

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderHeight))
        dumpProperty(ts, "header-height", node.headerHeight());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterHeight))
        dumpProperty(ts, "footer-height", node.footerHeight());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::TopContentInset))
        dumpProperty(ts, "top-content-inset", node.topContentInset());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::FrameScaleFactor))
        dumpProperty(ts, "frame-scale-factor", node.frameScaleFactor());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::ScrolledContentsLayer))
        dumpProperty(ts, "scrolled-contents-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.scrolledContentsLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer))
        dumpProperty(ts, "clip-inset-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.insetClipLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer))
        dumpProperty(ts, "content-shadow-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.contentShadowLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
        dumpProperty(ts, "header-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.headerLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
        dumpProperty(ts, "footer-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.footerLayer()));
}
    
void RemoteScrollingTreeTextStream::dump(const ScrollingStateOverflowScrollingNode& node, bool changedPropertiesOnly)
{
    RemoteScrollingTreeTextStream& ts = *this;
    
    dump(static_cast<const ScrollingStateScrollingNode&>(node), changedPropertiesOnly);
    
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateOverflowScrollingNode::ScrolledContentsLayer))
        dumpProperty(ts, "scrolled-contents-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.scrolledContentsLayer()));
}

void RemoteScrollingTreeTextStream::dump(const ScrollingStateFixedNode& node, bool changedPropertiesOnly)
{
    RemoteScrollingTreeTextStream& ts = *this;

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFixedNode::ViewportConstraints))
        ts << node.viewportConstraints();
}

void RemoteScrollingTreeTextStream::dump(const ScrollingStateStickyNode& node, bool changedPropertiesOnly)
{
    RemoteScrollingTreeTextStream& ts = *this;

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFixedNode::ViewportConstraints))
        ts << node.viewportConstraints();
}

void RemoteScrollingTreeTextStream::recursiveDumpNodes(const ScrollingStateNode& node, bool changedPropertiesOnly)
{
    RemoteScrollingTreeTextStream& ts = *this;

    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    dump(node, changedPropertiesOnly);

    if (node.children()) {
        ts << "\n";
        ts.increaseIndent();
        ts.writeIndent();
        ts << "(children";
        ts.increaseIndent();

        for (auto& childNode : *node.children())
            recursiveDumpNodes(*childNode, changedPropertiesOnly);

        ts << ")";
        ts.decreaseIndent();
        ts.decreaseIndent();
    }

    ts << ")";
    ts.decreaseIndent();
}

void RemoteScrollingTreeTextStream::dump(const ScrollingStateTree& stateTree, bool changedPropertiesOnly)
{
    RemoteScrollingTreeTextStream& ts = *this;

    dumpProperty(ts, "has changed properties", stateTree.hasChangedProperties());
    dumpProperty(ts, "has new root node", stateTree.hasNewRootStateNode());

    if (stateTree.rootStateNode())
        recursiveDumpNodes(*stateTree.rootStateNode(), changedPropertiesOnly);

    if (!stateTree.removedNodes().isEmpty()) {
        Vector<ScrollingNodeID> removedNodes;
        copyToVector(stateTree.removedNodes(), removedNodes);
        dumpProperty<Vector<ScrollingNodeID>>(ts, "removed-nodes", removedNodes);
    }
}

WTF::CString RemoteScrollingCoordinatorTransaction::description() const
{
    RemoteScrollingTreeTextStream ts;

    ts << "(\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(scrolling state tree";

    if (m_scrollingStateTree) {
        if (!m_scrollingStateTree->hasChangedProperties())
            ts << " - no changes";
        else
            ts.dump(*m_scrollingStateTree.get());
    } else
        ts << " - none";

    ts << ")\n";
    ts.decreaseIndent();

    return ts.release().utf8();
}

void RemoteScrollingCoordinatorTransaction::dump() const
{
    fprintf(stderr, "%s", description().data());
}
#endif

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
