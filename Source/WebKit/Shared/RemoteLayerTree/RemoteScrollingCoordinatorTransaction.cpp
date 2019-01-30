/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/ScrollingStateFixedNode.h>
#include <WebCore/ScrollingStateFrameHostingNode.h>
#include <WebCore/ScrollingStateFrameScrollingNode.h>
#include <WebCore/ScrollingStateOverflowScrollingNode.h>
#include <WebCore/ScrollingStateStickyNode.h>
#include <WebCore/ScrollingStateTree.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

#if ENABLE(ASYNC_SCROLLING)

namespace IPC {
using namespace WebCore;

template<> struct ArgumentCoder<ScrollingStateNode> {
    static void encode(Encoder&, const ScrollingStateNode&);
    static bool decode(Decoder&, ScrollingStateNode&);
};

template<> struct ArgumentCoder<ScrollingStateScrollingNode> {
    static void encode(Encoder&, const ScrollingStateScrollingNode&);
    static bool decode(Decoder&, ScrollingStateScrollingNode&);
};

template<> struct ArgumentCoder<ScrollingStateFrameHostingNode> {
    static void encode(Encoder&, const ScrollingStateFrameHostingNode&);
    static bool decode(Decoder&, ScrollingStateFrameHostingNode&);
};

template<> struct ArgumentCoder<ScrollingStateFrameScrollingNode> {
    static void encode(Encoder&, const ScrollingStateFrameScrollingNode&);
    static bool decode(Decoder&, ScrollingStateFrameScrollingNode&);
};

template<> struct ArgumentCoder<ScrollingStateOverflowScrollingNode> {
    static void encode(Encoder&, const ScrollingStateOverflowScrollingNode&);
    static bool decode(Decoder&, ScrollingStateOverflowScrollingNode&);
};

template<> struct ArgumentCoder<ScrollingStateFixedNode> {
    static void encode(Encoder&, const ScrollingStateFixedNode&);
    static bool decode(Decoder&, ScrollingStateFixedNode&);
};

template<> struct ArgumentCoder<ScrollingStateStickyNode> {
    static void encode(Encoder&, const ScrollingStateStickyNode&);
    static bool decode(Decoder&, ScrollingStateStickyNode&);
};

} // namespace IPC

using namespace IPC;

void ArgumentCoder<ScrollingStateNode>::encode(Encoder& encoder, const ScrollingStateNode& node)
{
    encoder.encodeEnum(node.nodeType());
    encoder << node.scrollingNodeID();
    encoder << node.parentNodeID();
    encoder << node.changedProperties();
    
    if (node.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.layer());
}

bool ArgumentCoder<ScrollingStateNode>::decode(Decoder& decoder, ScrollingStateNode& node)
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

void ArgumentCoder<ScrollingStateScrollingNode>::encode(Encoder& encoder, const ScrollingStateScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ScrollableAreaSize, scrollableAreaSize)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::TotalContentsSize, totalContentsSize)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ReachableContentsSize, reachableContentsSize)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ScrollPosition, scrollPosition)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ScrollOrigin, scrollOrigin)
#if ENABLE(CSS_SCROLL_SNAP)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::HorizontalSnapOffsets, horizontalSnapOffsets)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::VerticalSnapOffsets, verticalSnapOffsets)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::HorizontalSnapOffsetRanges, horizontalSnapOffsetRanges)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::VerticalSnapOffsetRanges, verticalSnapOffsetRanges)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::CurrentHorizontalSnapOffsetIndex, currentHorizontalSnapPointIndex)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::CurrentVerticalSnapOffsetIndex, currentVerticalSnapPointIndex)
#endif
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::ScrollableAreaParams, scrollableAreaParameters)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::RequestedScrollPosition, requestedScrollPosition)
    SCROLLING_NODE_ENCODE(ScrollingStateScrollingNode::RequestedScrollPosition, requestedScrollPositionRepresentsProgrammaticScroll)

    if (node.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.scrolledContentsLayer());
}

void ArgumentCoder<ScrollingStateFrameScrollingNode>::encode(Encoder& encoder, const ScrollingStateFrameScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateScrollingNode&>(node);
    
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::FrameScaleFactor, frameScaleFactor)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::EventTrackingRegion, eventTrackingRegions)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::ReasonsForSynchronousScrolling, synchronousScrollingReasons)
    SCROLLING_NODE_ENCODE_ENUM(ScrollingStateFrameScrollingNode::BehaviorForFixedElements, scrollBehaviorForFixedElements)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::HeaderHeight, headerHeight)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::FooterHeight, footerHeight)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::TopContentInset, topContentInset)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::FixedElementsLayoutRelativeToFrame, fixedElementsLayoutRelativeToFrame)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::VisualViewportEnabled, visualViewportEnabled)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::LayoutViewport, layoutViewport)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::MinLayoutViewportOrigin, minLayoutViewportOrigin)
    SCROLLING_NODE_ENCODE(ScrollingStateFrameScrollingNode::MaxLayoutViewportOrigin, maxLayoutViewportOrigin)

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.counterScrollingLayer());

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.insetClipLayer());

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.contentShadowLayer());

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalScrollbarLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.verticalScrollbarLayer());

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalScrollbarLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.horizontalScrollbarLayer());
}

void ArgumentCoder<ScrollingStateFrameHostingNode>::encode(Encoder& encoder, const ScrollingStateFrameHostingNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    // ParentRelativeScrollableRect isn't used so we don't encode it.
}

void ArgumentCoder<ScrollingStateOverflowScrollingNode>::encode(Encoder& encoder, const ScrollingStateOverflowScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateScrollingNode&>(node);
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

bool ArgumentCoder<ScrollingStateScrollingNode>::decode(Decoder& decoder, ScrollingStateScrollingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::ScrollableAreaSize, FloatSize, setScrollableAreaSize);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::TotalContentsSize, FloatSize, setTotalContentsSize);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::ReachableContentsSize, FloatSize, setReachableContentsSize);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::ScrollPosition, FloatPoint, setScrollPosition);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::ScrollOrigin, IntPoint, setScrollOrigin);
#if ENABLE(CSS_SCROLL_SNAP)
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::HorizontalSnapOffsets, Vector<float>, setHorizontalSnapOffsets);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::VerticalSnapOffsets, Vector<float>, setVerticalSnapOffsets);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::HorizontalSnapOffsetRanges, Vector<ScrollOffsetRange<float>>, setHorizontalSnapOffsetRanges)
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::VerticalSnapOffsetRanges, Vector<ScrollOffsetRange<float>>, setVerticalSnapOffsetRanges)
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::CurrentHorizontalSnapOffsetIndex, unsigned, setCurrentHorizontalSnapPointIndex);
    SCROLLING_NODE_DECODE(ScrollingStateScrollingNode::CurrentVerticalSnapOffsetIndex, unsigned, setCurrentVerticalSnapPointIndex);
#endif
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

    if (node.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setScrolledContentsLayer(layerID);
    }

    return true;
}

bool ArgumentCoder<ScrollingStateFrameScrollingNode>::decode(Decoder& decoder, ScrollingStateFrameScrollingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateScrollingNode&>(node)))
        return false;

    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::FrameScaleFactor, float, setFrameScaleFactor);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::EventTrackingRegion, EventTrackingRegions, setEventTrackingRegions);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::ReasonsForSynchronousScrolling, SynchronousScrollingReasons, setSynchronousScrollingReasons);
    SCROLLING_NODE_DECODE_ENUM(ScrollingStateFrameScrollingNode::BehaviorForFixedElements, ScrollBehaviorForFixedElements, setScrollBehaviorForFixedElements);

    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::HeaderHeight, int, setHeaderHeight);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::FooterHeight, int, setFooterHeight);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::TopContentInset, float, setTopContentInset);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::FixedElementsLayoutRelativeToFrame, bool, setFixedElementsLayoutRelativeToFrame);
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::VisualViewportEnabled, bool, setVisualViewportEnabled)
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::LayoutViewport, FloatRect, setLayoutViewport)
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::MinLayoutViewportOrigin, FloatPoint, setMinLayoutViewportOrigin)
    SCROLLING_NODE_DECODE(ScrollingStateFrameScrollingNode::MaxLayoutViewportOrigin, FloatPoint, setMaxLayoutViewportOrigin)

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

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalScrollbarLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setVerticalScrollbarLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalScrollbarLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setHorizontalScrollbarLayer(layerID);
    }

    return true;
}

bool ArgumentCoder<ScrollingStateFrameHostingNode>::decode(Decoder& decoder, ScrollingStateFrameHostingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    return true;
}

bool ArgumentCoder<ScrollingStateOverflowScrollingNode>::decode(Decoder& decoder, ScrollingStateOverflowScrollingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateScrollingNode&>(node)))
        return false;

    return true;
}

void ArgumentCoder<ScrollingStateFixedNode>::encode(Encoder& encoder, const ScrollingStateFixedNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    
    if (node.hasChangedProperty(ScrollingStateFixedNode::ViewportConstraints))
        encoder << node.viewportConstraints();
}

bool ArgumentCoder<ScrollingStateFixedNode>::decode(Decoder& decoder, ScrollingStateFixedNode& node)
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

void ArgumentCoder<ScrollingStateStickyNode>::encode(Encoder& encoder, const ScrollingStateStickyNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    
    if (node.hasChangedProperty(ScrollingStateStickyNode::ViewportConstraints))
        encoder << node.viewportConstraints();
}

bool ArgumentCoder<ScrollingStateStickyNode>::decode(Decoder& decoder, ScrollingStateStickyNode& node)
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

static void encodeNodeAndDescendants(IPC::Encoder& encoder, const ScrollingStateNode& stateNode, int& encodedNodeCount)
{
    ++encodedNodeCount;

    switch (stateNode.nodeType()) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        encoder << downcast<ScrollingStateFrameScrollingNode>(stateNode);
        break;
    case ScrollingNodeType::FrameHosting:
        encoder << downcast<ScrollingStateFrameHostingNode>(stateNode);
        break;
    case ScrollingNodeType::Overflow:
        encoder << downcast<ScrollingStateOverflowScrollingNode>(stateNode);
        break;
    case ScrollingNodeType::Fixed:
        encoder << downcast<ScrollingStateFixedNode>(stateNode);
        break;
    case ScrollingNodeType::Sticky:
        encoder << downcast<ScrollingStateStickyNode>(stateNode);
        break;
    }

    if (!stateNode.children())
        return;

    for (const auto& child : *stateNode.children())
        encodeNodeAndDescendants(encoder, *child.get(), encodedNodeCount);
}

void RemoteScrollingCoordinatorTransaction::encode(IPC::Encoder& encoder) const
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
    } else
        encoder << Vector<ScrollingNodeID>();
}

bool RemoteScrollingCoordinatorTransaction::decode(IPC::Decoder& decoder, RemoteScrollingCoordinatorTransaction& transaction)
{
    return transaction.decode(decoder);
}

bool RemoteScrollingCoordinatorTransaction::decode(IPC::Decoder& decoder)
{
    int numNodes;
    if (!decoder.decode(numNodes))
        return false;

    bool hasNewRootNode;
    if (!decoder.decode(hasNewRootNode))
        return false;
    
    m_scrollingStateTree = std::make_unique<ScrollingStateTree>();

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

        m_scrollingStateTree->insertNode(nodeType, nodeID, parentNodeID, notFound);
        ScrollingStateNode* newNode = m_scrollingStateTree->stateNodeForID(nodeID);
        ASSERT(newNode);
        ASSERT(!parentNodeID || newNode->parent());
        
        switch (nodeType) {
        case ScrollingNodeType::MainFrame:
        case ScrollingNodeType::Subframe:
            if (!decoder.decode(downcast<ScrollingStateFrameScrollingNode>(*newNode)))
                return false;
            break;
        case ScrollingNodeType::FrameHosting:
            if (!decoder.decode(downcast<ScrollingStateFrameHostingNode>(*newNode)))
                return false;
            break;
        case ScrollingNodeType::Overflow:
            if (!decoder.decode(downcast<ScrollingStateOverflowScrollingNode>(*newNode)))
                return false;
            break;
        case ScrollingNodeType::Fixed:
            if (!decoder.decode(downcast<ScrollingStateFixedNode>(*newNode)))
                return false;
            break;
        case ScrollingNodeType::Sticky:
            if (!decoder.decode(downcast<ScrollingStateStickyNode>(*newNode)))
                return false;
            break;
        }
    }

    m_scrollingStateTree->setHasNewRootStateNode(hasNewRootNode);

    return true;
}

#if !defined(NDEBUG) || !LOG_DISABLED

static void dump(TextStream& ts, const ScrollingStateScrollingNode& node, bool changedPropertiesOnly)
{
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaSize))
        ts.dumpProperty("scrollable-area-size", node.scrollableAreaSize());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize))
        ts.dumpProperty("total-contents-size", node.totalContentsSize());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::ReachableContentsSize))
        ts.dumpProperty("reachable-contents-size", node.reachableContentsSize());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::ScrollPosition))
        ts.dumpProperty("scroll-position", node.scrollPosition());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::ScrollOrigin))
        ts.dumpProperty("scroll-origin", node.scrollOrigin());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition)) {
        ts.dumpProperty("requested-scroll-position", node.requestedScrollPosition());
        ts.dumpProperty("requested-scroll-position-is-programatic", node.requestedScrollPositionRepresentsProgrammaticScroll());
    }

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
        ts.dumpProperty("scrolled-contents-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.scrolledContentsLayer()));
}

static void dump(TextStream& ts, const ScrollingStateFrameHostingNode& node, bool changedPropertiesOnly)
{
}

static void dump(TextStream& ts, const ScrollingStateFrameScrollingNode& node, bool changedPropertiesOnly)
{
    dump(ts, static_cast<const ScrollingStateScrollingNode&>(node), changedPropertiesOnly);
    
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::FrameScaleFactor))
        ts.dumpProperty("frame-scale-factor", node.frameScaleFactor());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::EventTrackingRegion)) {
        {
            TextStream::GroupScope group(ts);
            ts << "asynchronous-event-tracking-region";
            for (auto rect : node.eventTrackingRegions().asynchronousDispatchRegion.rects()) {
                ts << "\n";
                ts.writeIndent();
                ts << rect;
            }
        }
        for (const auto& synchronousEventRegion : node.eventTrackingRegions().eventSpecificSynchronousDispatchRegions) {
            TextStream::GroupScope group(ts);
            ts << "synchronous-event-tracking-region for event " << synchronousEventRegion.key;

            for (auto rect : synchronousEventRegion.value.rects()) {
                ts << "\n";
                ts.writeIndent();
                ts << rect;
            }
        }
    }

    // FIXME: dump synchronousScrollingReasons
    // FIXME: dump scrollableAreaParameters
    // FIXME: dump scrollBehaviorForFixedElements

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderHeight))
        ts.dumpProperty("header-height", node.headerHeight());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterHeight))
        ts.dumpProperty("footer-height", node.footerHeight());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::TopContentInset))
        ts.dumpProperty("top-content-inset", node.topContentInset());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::FrameScaleFactor))
        ts.dumpProperty("frame-scale-factor", node.frameScaleFactor());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer))
        ts.dumpProperty("clip-inset-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.insetClipLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer))
        ts.dumpProperty("content-shadow-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.contentShadowLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
        ts.dumpProperty("header-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.headerLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
        ts.dumpProperty("footer-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.footerLayer()));
}
    
static void dump(TextStream& ts, const ScrollingStateOverflowScrollingNode& node, bool changedPropertiesOnly)
{
    dump(ts, static_cast<const ScrollingStateScrollingNode&>(node), changedPropertiesOnly);
}

static void dump(TextStream& ts, const ScrollingStateFixedNode& node, bool changedPropertiesOnly)
{
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFixedNode::ViewportConstraints))
        ts << node.viewportConstraints();
}

static void dump(TextStream& ts, const ScrollingStateStickyNode& node, bool changedPropertiesOnly)
{
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateFixedNode::ViewportConstraints))
        ts << node.viewportConstraints();
}

static void dump(TextStream& ts, const ScrollingStateNode& node, bool changedPropertiesOnly)
{
    ts.dumpProperty("type", node.nodeType());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        ts.dumpProperty("layer", static_cast<GraphicsLayer::PlatformLayerID>(node.layer()));
    
    switch (node.nodeType()) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        dump(ts, downcast<ScrollingStateFrameScrollingNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::FrameHosting:
        dump(ts, downcast<ScrollingStateFrameHostingNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::Overflow:
        dump(ts, downcast<ScrollingStateOverflowScrollingNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::Fixed:
        dump(ts, downcast<ScrollingStateFixedNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::Sticky:
        dump(ts, downcast<ScrollingStateStickyNode>(node), changedPropertiesOnly);
        break;
    }
}

static void recursiveDumpNodes(TextStream& ts, const ScrollingStateNode& node, bool changedPropertiesOnly)
{
    TextStream::GroupScope group(ts);
    ts << "node " << node.scrollingNodeID();
    dump(ts, node, changedPropertiesOnly);

    if (node.children()) {
        TextStream::GroupScope group(ts);
        ts << "children";

        for (auto& childNode : *node.children())
            recursiveDumpNodes(ts, *childNode, changedPropertiesOnly);
    }
}

static void dump(TextStream& ts, const ScrollingStateTree& stateTree, bool changedPropertiesOnly)
{
    ts.dumpProperty("has changed properties", stateTree.hasChangedProperties());
    ts.dumpProperty("has new root node", stateTree.hasNewRootStateNode());

    if (stateTree.rootStateNode())
        recursiveDumpNodes(ts, *stateTree.rootStateNode(), changedPropertiesOnly);
}

WTF::CString RemoteScrollingCoordinatorTransaction::description() const
{
    TextStream ts;

    ts.startGroup();
    ts << "scrolling state tree";

    if (m_scrollingStateTree) {
        if (!m_scrollingStateTree->hasChangedProperties())
            ts << " - no changes";
        else
            WebKit::dump(ts, *m_scrollingStateTree.get(), true);
    } else
        ts << " - none";

    ts.endGroup();

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

void RemoteScrollingCoordinatorTransaction::encode(IPC::Encoder&) const
{
}

bool RemoteScrollingCoordinatorTransaction::decode(IPC::Decoder& decoder, RemoteScrollingCoordinatorTransaction& transaction)
{
    return true;
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
