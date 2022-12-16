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

#if ENABLE(UI_SIDE_COMPOSITING)

#include "RemoteScrollingCoordinatorTransaction.h"

#include "ArgumentCoders.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollingStateFixedNode.h>
#include <WebCore/ScrollingStateFrameHostingNode.h>
#include <WebCore/ScrollingStateFrameScrollingNode.h>
#include <WebCore/ScrollingStateOverflowScrollProxyNode.h>
#include <WebCore/ScrollingStateOverflowScrollingNode.h>
#include <WebCore/ScrollingStatePositionedNode.h>
#include <WebCore/ScrollingStateStickyNode.h>
#include <WebCore/ScrollingStateTree.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace IPC {
using namespace WebCore;

template<> struct ArgumentCoder<ScrollingStateNode> {
    static void encode(Encoder&, const ScrollingStateNode&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, ScrollingStateNode&);
};

template<> struct ArgumentCoder<ScrollingStateScrollingNode> {
    static void encode(Encoder&, const ScrollingStateScrollingNode&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, ScrollingStateScrollingNode&);
};

template<> struct ArgumentCoder<ScrollingStateFrameHostingNode> {
    static void encode(Encoder&, const ScrollingStateFrameHostingNode&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, ScrollingStateFrameHostingNode&);
};

template<> struct ArgumentCoder<ScrollingStateFrameScrollingNode> {
    static void encode(Encoder&, const ScrollingStateFrameScrollingNode&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, ScrollingStateFrameScrollingNode&);
};

template<> struct ArgumentCoder<ScrollingStateOverflowScrollingNode> {
    static void encode(Encoder&, const ScrollingStateOverflowScrollingNode&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, ScrollingStateOverflowScrollingNode&);
};

template<> struct ArgumentCoder<ScrollingStateOverflowScrollProxyNode> {
    static void encode(Encoder&, const ScrollingStateOverflowScrollProxyNode&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, ScrollingStateOverflowScrollProxyNode&);
};

template<> struct ArgumentCoder<ScrollingStateFixedNode> {
    static void encode(Encoder&, const ScrollingStateFixedNode&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, ScrollingStateFixedNode&);
};

template<> struct ArgumentCoder<ScrollingStateStickyNode> {
    static void encode(Encoder&, const ScrollingStateStickyNode&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, ScrollingStateStickyNode&);
};

template<> struct ArgumentCoder<ScrollingStatePositionedNode> {
    static void encode(Encoder&, const ScrollingStatePositionedNode&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, ScrollingStatePositionedNode&);
};

template<> struct ArgumentCoder<RequestedScrollData> {
    static void encode(Encoder&, const RequestedScrollData&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, RequestedScrollData&);
};

template<> struct ArgumentCoder<FloatScrollSnapOffsetsInfo> {
    static void encode(Encoder&, const FloatScrollSnapOffsetsInfo&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, FloatScrollSnapOffsetsInfo&);
};

template<> struct ArgumentCoder<SnapOffset<float>> {
    static void encode(Encoder&, const SnapOffset<float>&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, SnapOffset<float>&);
};

} // namespace IPC

namespace WTF {

template<> struct EnumTraits<WebCore::ScrollingStateNode::Property> {
    using values = EnumValues<
        WebCore::ScrollingStateNode::Property,
        WebCore::ScrollingStateNode::Property::Layer,
        WebCore::ScrollingStateNode::Property::ChildNodes,
        WebCore::ScrollingStateNode::Property::ScrollableAreaSize,
        WebCore::ScrollingStateNode::Property::TotalContentsSize,
        WebCore::ScrollingStateNode::Property::ReachableContentsSize,
        WebCore::ScrollingStateNode::Property::ScrollPosition,
        WebCore::ScrollingStateNode::Property::ScrollOrigin,
        WebCore::ScrollingStateNode::Property::ScrollableAreaParams,
        WebCore::ScrollingStateNode::Property::ReasonsForSynchronousScrolling,
        WebCore::ScrollingStateNode::Property::RequestedScrollPosition,
        WebCore::ScrollingStateNode::Property::SnapOffsetsInfo,
        WebCore::ScrollingStateNode::Property::CurrentHorizontalSnapOffsetIndex,
        WebCore::ScrollingStateNode::Property::CurrentVerticalSnapOffsetIndex,
        WebCore::ScrollingStateNode::Property::IsMonitoringWheelEvents,
        WebCore::ScrollingStateNode::Property::ScrollContainerLayer,
        WebCore::ScrollingStateNode::Property::ScrolledContentsLayer,
        WebCore::ScrollingStateNode::Property::HorizontalScrollbarLayer,
        WebCore::ScrollingStateNode::Property::VerticalScrollbarLayer,
        WebCore::ScrollingStateNode::Property::PainterForScrollbar,
        WebCore::ScrollingStateNode::Property::FrameScaleFactor,
        WebCore::ScrollingStateNode::Property::EventTrackingRegion,
        WebCore::ScrollingStateNode::Property::RootContentsLayer,
        WebCore::ScrollingStateNode::Property::CounterScrollingLayer,
        WebCore::ScrollingStateNode::Property::InsetClipLayer,
        WebCore::ScrollingStateNode::Property::ContentShadowLayer,
        WebCore::ScrollingStateNode::Property::HeaderHeight,
        WebCore::ScrollingStateNode::Property::FooterHeight,
        WebCore::ScrollingStateNode::Property::HeaderLayer,
        WebCore::ScrollingStateNode::Property::FooterLayer,
        WebCore::ScrollingStateNode::Property::BehaviorForFixedElements,
        WebCore::ScrollingStateNode::Property::TopContentInset,
        WebCore::ScrollingStateNode::Property::FixedElementsLayoutRelativeToFrame,
        WebCore::ScrollingStateNode::Property::VisualViewportIsSmallerThanLayoutViewport,
        WebCore::ScrollingStateNode::Property::AsyncFrameOrOverflowScrollingEnabled,
        WebCore::ScrollingStateNode::Property::WheelEventGesturesBecomeNonBlocking,
        WebCore::ScrollingStateNode::Property::ScrollingPerformanceTestingEnabled,
        WebCore::ScrollingStateNode::Property::LayoutViewport,
        WebCore::ScrollingStateNode::Property::MinLayoutViewportOrigin,
        WebCore::ScrollingStateNode::Property::MaxLayoutViewportOrigin,
        WebCore::ScrollingStateNode::Property::OverrideVisualViewportSize,
        WebCore::ScrollingStateNode::Property::RelatedOverflowScrollingNodes,
        WebCore::ScrollingStateNode::Property::LayoutConstraintData,
        WebCore::ScrollingStateNode::Property::ViewportConstraints,
        WebCore::ScrollingStateNode::Property::OverflowScrollingNode,
        WebCore::ScrollingStateNode::Property::KeyboardScrollData
    >;
};

} // namespace WTF

using namespace IPC;

void ArgumentCoder<ScrollingStateNode>::encode(Encoder& encoder, const ScrollingStateNode& node)
{
    encoder << node.nodeType();
    encoder << node.scrollingNodeID();
    encoder << node.parentNodeID();
    encoder << node.changedProperties();
    
    if (node.hasChangedProperty(ScrollingStateNode::Property::Layer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.layer());
}

bool ArgumentCoder<ScrollingStateNode>::decode(Decoder& decoder, ScrollingStateNode& node)
{
    // nodeType, scrollingNodeID and parentNodeID have already been decoded by the caller in order to create the node.
    OptionSet<ScrollingStateNode::Property> changedProperties;
    if (!decoder.decode(changedProperties))
        return false;

    node.setChangedProperties(changedProperties);
    if (node.hasChangedProperty(ScrollingStateNode::Property::Layer)) {
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
        encoder << node.getter();

void ArgumentCoder<ScrollingStateScrollingNode>::encode(Encoder& encoder, const ScrollingStateScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::ScrollableAreaSize, scrollableAreaSize)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::TotalContentsSize, totalContentsSize)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::ReachableContentsSize, reachableContentsSize)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::ScrollPosition, scrollPosition)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::ScrollOrigin, scrollOrigin)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::SnapOffsetsInfo, snapOffsetsInfo)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::CurrentHorizontalSnapOffsetIndex, currentHorizontalSnapPointIndex)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::CurrentVerticalSnapOffsetIndex, currentVerticalSnapPointIndex)
#if ENABLE(SCROLLING_THREAD)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::ReasonsForSynchronousScrolling, synchronousScrollingReasons)
#endif
    // We don't encode isMonitoringWheelEvents; this needs to be a sync message to the UI process.
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::ScrollableAreaParams, scrollableAreaParameters)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::RequestedScrollPosition, requestedScrollData)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::KeyboardScrollData, keyboardScrollData)

    if (node.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.scrollContainerLayer());

    if (node.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.scrolledContentsLayer());

    if (node.hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.horizontalScrollbarLayer());

    if (node.hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.verticalScrollbarLayer());
}

void ArgumentCoder<ScrollingStateFrameScrollingNode>::encode(Encoder& encoder, const ScrollingStateFrameScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateScrollingNode&>(node);

    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::FrameScaleFactor, frameScaleFactor)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::EventTrackingRegion, eventTrackingRegions)
    SCROLLING_NODE_ENCODE_ENUM(ScrollingStateNode::Property::BehaviorForFixedElements, scrollBehaviorForFixedElements)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::HeaderHeight, headerHeight)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::FooterHeight, footerHeight)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::TopContentInset, topContentInset)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::FixedElementsLayoutRelativeToFrame, fixedElementsLayoutRelativeToFrame)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::AsyncFrameOrOverflowScrollingEnabled, asyncFrameOrOverflowScrollingEnabled)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::VisualViewportIsSmallerThanLayoutViewport, visualViewportIsSmallerThanLayoutViewport)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::WheelEventGesturesBecomeNonBlocking, wheelEventGesturesBecomeNonBlocking)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::ScrollingPerformanceTestingEnabled, scrollingPerformanceTestingEnabled)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::LayoutViewport, layoutViewport)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::MinLayoutViewportOrigin, minLayoutViewportOrigin)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::MaxLayoutViewportOrigin, maxLayoutViewportOrigin)
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::OverrideVisualViewportSize, overrideVisualViewportSize)

    if (node.hasChangedProperty(ScrollingStateNode::Property::CounterScrollingLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.counterScrollingLayer());

    if (node.hasChangedProperty(ScrollingStateNode::Property::InsetClipLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.insetClipLayer());

    if (node.hasChangedProperty(ScrollingStateNode::Property::ContentShadowLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.contentShadowLayer());

    if (node.hasChangedProperty(ScrollingStateNode::Property::RootContentsLayer))
        encoder << static_cast<GraphicsLayer::PlatformLayerID>(node.rootContentsLayer());
}

void ArgumentCoder<ScrollingStateFrameHostingNode>::encode(Encoder& encoder, const ScrollingStateFrameHostingNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
}

void ArgumentCoder<ScrollingStateOverflowScrollingNode>::encode(Encoder& encoder, const ScrollingStateOverflowScrollingNode& node)
{
    encoder << static_cast<const ScrollingStateScrollingNode&>(node);
}

void ArgumentCoder<ScrollingStateOverflowScrollProxyNode>::encode(Encoder& encoder, const ScrollingStateOverflowScrollProxyNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    SCROLLING_NODE_ENCODE(ScrollingStateNode::Property::OverflowScrollingNode, overflowScrollingNode)
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
        if (!decoder.decode(decodedValue)) \
            return false; \
        node.setter(decodedValue); \
    }

bool ArgumentCoder<ScrollingStateScrollingNode>::decode(Decoder& decoder, ScrollingStateScrollingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::ScrollableAreaSize, FloatSize, setScrollableAreaSize);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::TotalContentsSize, FloatSize, setTotalContentsSize);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::ReachableContentsSize, FloatSize, setReachableContentsSize);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::ScrollPosition, FloatPoint, setScrollPosition);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::ScrollOrigin, IntPoint, setScrollOrigin);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::SnapOffsetsInfo, FloatScrollSnapOffsetsInfo, setSnapOffsetsInfo);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::CurrentHorizontalSnapOffsetIndex, std::optional<unsigned>, setCurrentHorizontalSnapPointIndex);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::CurrentVerticalSnapOffsetIndex, std::optional<unsigned>, setCurrentVerticalSnapPointIndex);
#if ENABLE(SCROLLING_THREAD)
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::ReasonsForSynchronousScrolling, OptionSet<SynchronousScrollingReason>, setSynchronousScrollingReasons)
#endif
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::ScrollableAreaParams, ScrollableAreaParameters, setScrollableAreaParameters);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::RequestedScrollPosition, RequestedScrollData, setRequestedScrollData);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::KeyboardScrollData, RequestedKeyboardScrollData, setKeyboardScrollData);

    if (node.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setScrollContainerLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setScrolledContentsLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setHorizontalScrollbarLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setVerticalScrollbarLayer(layerID);
    }

    return true;
}

bool ArgumentCoder<ScrollingStateFrameScrollingNode>::decode(Decoder& decoder, ScrollingStateFrameScrollingNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateScrollingNode&>(node)))
        return false;

    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::FrameScaleFactor, float, setFrameScaleFactor);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::EventTrackingRegion, EventTrackingRegions, setEventTrackingRegions);
    SCROLLING_NODE_DECODE_ENUM(ScrollingStateNode::Property::BehaviorForFixedElements, ScrollBehaviorForFixedElements, setScrollBehaviorForFixedElements);

    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::HeaderHeight, int, setHeaderHeight);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::FooterHeight, int, setFooterHeight);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::TopContentInset, float, setTopContentInset);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::FixedElementsLayoutRelativeToFrame, bool, setFixedElementsLayoutRelativeToFrame);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::AsyncFrameOrOverflowScrollingEnabled, bool, setAsyncFrameOrOverflowScrollingEnabled);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::VisualViewportIsSmallerThanLayoutViewport, bool, setVisualViewportIsSmallerThanLayoutViewport);
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::WheelEventGesturesBecomeNonBlocking, bool, setWheelEventGesturesBecomeNonBlocking)
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::ScrollingPerformanceTestingEnabled, bool, setScrollingPerformanceTestingEnabled)
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::LayoutViewport, FloatRect, setLayoutViewport)
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::MinLayoutViewportOrigin, FloatPoint, setMinLayoutViewportOrigin)
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::MaxLayoutViewportOrigin, FloatPoint, setMaxLayoutViewportOrigin)
    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::OverrideVisualViewportSize, std::optional<FloatSize>, setOverrideVisualViewportSize)

    if (node.hasChangedProperty(ScrollingStateNode::Property::CounterScrollingLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setCounterScrollingLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateNode::Property::InsetClipLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setInsetClipLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateNode::Property::ContentShadowLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setContentShadowLayer(layerID);
    }

    if (node.hasChangedProperty(ScrollingStateNode::Property::RootContentsLayer)) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;
        node.setRootContentsLayer(layerID);
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

bool ArgumentCoder<ScrollingStateOverflowScrollProxyNode>::decode(Decoder& decoder, ScrollingStateOverflowScrollProxyNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    SCROLLING_NODE_DECODE(ScrollingStateNode::Property::OverflowScrollingNode, ScrollingNodeID, setOverflowScrollingNode);
    return true;
}

void ArgumentCoder<ScrollingStateFixedNode>::encode(Encoder& encoder, const ScrollingStateFixedNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);
    
    if (node.hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints))
        encoder << node.viewportConstraints();
}

bool ArgumentCoder<ScrollingStateFixedNode>::decode(Decoder& decoder, ScrollingStateFixedNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    if (node.hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints)) {
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
    
    if (node.hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints))
        encoder << node.viewportConstraints();
}

bool ArgumentCoder<ScrollingStateStickyNode>::decode(Decoder& decoder, ScrollingStateStickyNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    if (node.hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints)) {
        StickyPositionViewportConstraints decodedValue;
        if (!decoder.decode(decodedValue))
            return false;
        node.updateConstraints(decodedValue);
    }

    return true;
}

void ArgumentCoder<ScrollingStatePositionedNode>::encode(Encoder& encoder, const ScrollingStatePositionedNode& node)
{
    encoder << static_cast<const ScrollingStateNode&>(node);

    if (node.hasChangedProperty(ScrollingStateNode::Property::RelatedOverflowScrollingNodes))
        encoder << node.relatedOverflowScrollingNodes();

    if (node.hasChangedProperty(ScrollingStateNode::Property::LayoutConstraintData))
        encoder << node.layoutConstraints();
}

bool ArgumentCoder<ScrollingStatePositionedNode>::decode(Decoder& decoder, ScrollingStatePositionedNode& node)
{
    if (!decoder.decode(static_cast<ScrollingStateNode&>(node)))
        return false;

    if (node.hasChangedProperty(ScrollingStateNode::Property::RelatedOverflowScrollingNodes)) {
        Vector<ScrollingNodeID> decodedValue;
        if (!decoder.decode(decodedValue))
            return false;
        node.setRelatedOverflowScrollingNodes(WTFMove(decodedValue));
    }

    if (node.hasChangedProperty(ScrollingStateNode::Property::LayoutConstraintData)) {
        AbsolutePositionConstraints decodedValue;
        if (!decoder.decode(decodedValue))
            return false;
        node.updateConstraints(decodedValue);
    }

    return true;
}


void ArgumentCoder<RequestedScrollData>::encode(Encoder& encoder, const RequestedScrollData& scrollData)
{
    encoder << scrollData.requestType;
    encoder << scrollData.scrollPosition;
    encoder << scrollData.scrollType;
    encoder << scrollData.clamping;
    encoder << scrollData.animated;
}

bool ArgumentCoder<RequestedScrollData>::decode(Decoder& decoder, RequestedScrollData& scrollData)
{
    if (!decoder.decode(scrollData.requestType))
        return false;

    if (!decoder.decode(scrollData.scrollPosition))
        return false;

    if (!decoder.decode(scrollData.scrollType))
        return false;

    if (!decoder.decode(scrollData.clamping))
        return false;

    if (!decoder.decode(scrollData.animated))
        return false;

    return true;
}

void ArgumentCoder<SnapOffset<float>>::encode(Encoder& encoder, const SnapOffset<float>& offset)
{
    encoder << offset.offset;
    encoder << offset.stop;
    encoder << offset.hasSnapAreaLargerThanViewport;
    encoder << offset.snapTargetID;
    encoder << offset.snapAreaIndices;
}

bool ArgumentCoder<SnapOffset<float>>::decode(Decoder& decoder, SnapOffset<float>& offset)
{
    if (!decoder.decode(offset.offset))
        return false;
    if (!decoder.decode(offset.stop))
        return false;
    if (!decoder.decode(offset.hasSnapAreaLargerThanViewport))
        return false;
    if (!decoder.decode(offset.snapTargetID))
        return false;
    if (!decoder.decode(offset.snapAreaIndices))
        return false;
    return true;
}

void ArgumentCoder<FloatScrollSnapOffsetsInfo>::encode(Encoder& encoder, const FloatScrollSnapOffsetsInfo& info)
{
    encoder << info.horizontalSnapOffsets;
    encoder << info.verticalSnapOffsets;
    encoder << info.strictness;
    encoder << info.snapAreas;
}

bool ArgumentCoder<FloatScrollSnapOffsetsInfo>::decode(Decoder& decoder, FloatScrollSnapOffsetsInfo& info)
{
    if (!decoder.decode(info.horizontalSnapOffsets))
        return false;
    if (!decoder.decode(info.verticalSnapOffsets))
        return false;
    if (!decoder.decode(info.strictness))
        return false;
    if (!decoder.decode(info.snapAreas))
        return false;
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
    case ScrollingNodeType::OverflowProxy:
        encoder << downcast<ScrollingStateOverflowScrollProxyNode>(stateNode);
        break;
    case ScrollingNodeType::Fixed:
        encoder << downcast<ScrollingStateFixedNode>(stateNode);
        break;
    case ScrollingNodeType::Sticky:
        encoder << downcast<ScrollingStateStickyNode>(stateNode);
        break;
    case ScrollingNodeType::Positioned:
        encoder << downcast<ScrollingStatePositionedNode>(stateNode);
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
    
    m_scrollingStateTree = makeUnique<ScrollingStateTree>();

    bool hasChangedProperties;
    if (!decoder.decode(hasChangedProperties))
        return false;

    m_scrollingStateTree->setHasChangedProperties(hasChangedProperties);

    for (int i = 0; i < numNodes; ++i) {
        ScrollingNodeType nodeType;
        if (!decoder.decode(nodeType))
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
        case ScrollingNodeType::OverflowProxy:
            if (!decoder.decode(downcast<ScrollingStateOverflowScrollProxyNode>(*newNode)))
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
        case ScrollingNodeType::Positioned:
            if (!decoder.decode(downcast<ScrollingStatePositionedNode>(*newNode)))
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
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ScrollableAreaSize))
        ts.dumpProperty("scrollable-area-size", node.scrollableAreaSize());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::TotalContentsSize))
        ts.dumpProperty("total-contents-size", node.totalContentsSize());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ReachableContentsSize))
        ts.dumpProperty("reachable-contents-size", node.reachableContentsSize());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ScrollPosition))
        ts.dumpProperty("scroll-position", node.scrollPosition());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ScrollOrigin))
        ts.dumpProperty("scroll-origin", node.scrollOrigin());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::RequestedScrollPosition)) {
        const auto& requestedScrollData = node.requestedScrollData();
        if (requestedScrollData.requestType == ScrollRequestType::CancelAnimatedScroll)
            ts.dumpProperty("requested-type", "cancel animated scroll");
        else {
            ts.dumpProperty("requested-scroll-position", requestedScrollData.scrollPosition);
            ts.dumpProperty("requested-scroll-position-is-programatic", requestedScrollData.scrollType);
            ts.dumpProperty("requested-scroll-position-clamping", requestedScrollData.clamping);
            ts.dumpProperty("requested-scroll-position-animated", requestedScrollData.animated == ScrollIsAnimated::Yes);
        }
    }

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
        ts.dumpProperty("scroll-container-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.scrollContainerLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
        ts.dumpProperty("scrolled-contents-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.scrolledContentsLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::SnapOffsetsInfo)) {
        ts.dumpProperty("horizontal snap offsets", node.snapOffsetsInfo().horizontalSnapOffsets);
        ts.dumpProperty("vertical snap offsets", node.snapOffsetsInfo().verticalSnapOffsets);
        ts.dumpProperty("current horizontal snap point index", node.currentHorizontalSnapPointIndex());
        ts.dumpProperty("current vertical snap point index", node.currentVerticalSnapPointIndex());
    }

#if ENABLE(SCROLLING_THREAD)
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ReasonsForSynchronousScrolling))
        ts.dumpProperty("synchronous scrolling reasons", node.synchronousScrollingReasons());
#endif

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::KeyboardScrollData)) {
        const auto& keyboardScrollData = node.keyboardScrollData();
        if (keyboardScrollData.action == KeyboardScrollAction::StartAnimation && keyboardScrollData.keyboardScroll) {
            ts.dumpProperty("keyboard-scroll-data-action", "start animation");

            ts.dumpProperty("keyboard-scroll-data-scroll-offset", keyboardScrollData.keyboardScroll->offset);
            ts.dumpProperty("keyboard-scroll-data-scroll-maximum-velocity", keyboardScrollData.keyboardScroll->maximumVelocity);
            ts.dumpProperty("keyboard-scroll-data-scroll-force", keyboardScrollData.keyboardScroll->force);
            ts.dumpProperty("keyboard-scroll-data-scroll-granularity", keyboardScrollData.keyboardScroll->granularity);
            ts.dumpProperty("keyboard-scroll-data-scroll-direction", keyboardScrollData.keyboardScroll->direction);
        } else if (keyboardScrollData.action == KeyboardScrollAction::StopWithAnimation)
            ts.dumpProperty("keyboard-scroll-data-action", "stop with animation");
        else if (keyboardScrollData.action == KeyboardScrollAction::StopImmediately)
            ts.dumpProperty("keyboard-scroll-data-action", "stop immediately");
    }
}

static void dump(TextStream& ts, const ScrollingStateFrameHostingNode& node, bool changedPropertiesOnly)
{
}

static void dump(TextStream& ts, const ScrollingStateFrameScrollingNode& node, bool changedPropertiesOnly)
{
    dump(ts, static_cast<const ScrollingStateScrollingNode&>(node), changedPropertiesOnly);
    
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::FrameScaleFactor))
        ts.dumpProperty("frame-scale-factor", node.frameScaleFactor());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::EventTrackingRegion)) {
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
            ts << "synchronous-event-tracking-region for event " << EventTrackingRegions::eventName(synchronousEventRegion.key);

            for (auto rect : synchronousEventRegion.value.rects()) {
                ts << "\n";
                ts.writeIndent();
                ts << rect;
            }
        }
    }

    // FIXME: dump scrollableAreaParameters
    // FIXME: dump scrollBehaviorForFixedElements

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::HeaderHeight))
        ts.dumpProperty("header-height", node.headerHeight());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::FooterHeight))
        ts.dumpProperty("footer-height", node.footerHeight());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::TopContentInset))
        ts.dumpProperty("top-content-inset", node.topContentInset());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::FrameScaleFactor))
        ts.dumpProperty("frame-scale-factor", node.frameScaleFactor());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::InsetClipLayer))
        ts.dumpProperty("clip-inset-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.insetClipLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ContentShadowLayer))
        ts.dumpProperty("content-shadow-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.contentShadowLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::HeaderLayer))
        ts.dumpProperty("header-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.headerLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::FooterLayer))
        ts.dumpProperty("footer-layer", static_cast<GraphicsLayer::PlatformLayerID>(node.footerLayer()));
}
    
static void dump(TextStream& ts, const ScrollingStateOverflowScrollingNode& node, bool changedPropertiesOnly)
{
    dump(ts, static_cast<const ScrollingStateScrollingNode&>(node), changedPropertiesOnly);
}

static void dump(TextStream& ts, const ScrollingStateOverflowScrollProxyNode& node, bool changedPropertiesOnly)
{
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::OverflowScrollingNode))
        ts.dumpProperty("overflow-scrolling-node", node.overflowScrollingNode());
}

static void dump(TextStream& ts, const ScrollingStateFixedNode& node, bool changedPropertiesOnly)
{
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints))
        ts << node.viewportConstraints();
}

static void dump(TextStream& ts, const ScrollingStateStickyNode& node, bool changedPropertiesOnly)
{
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints))
        ts << node.viewportConstraints();
}

static void dump(TextStream& ts, const ScrollingStatePositionedNode& node, bool changedPropertiesOnly)
{
    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::RelatedOverflowScrollingNodes))
        ts << node.relatedOverflowScrollingNodes();

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::LayoutConstraintData))
        ts << node.layoutConstraints();
}

static void dump(TextStream& ts, const ScrollingStateNode& node, bool changedPropertiesOnly)
{
    ts.dumpProperty("type", node.nodeType());

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::Layer))
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
    case ScrollingNodeType::OverflowProxy:
        dump(ts, downcast<ScrollingStateOverflowScrollProxyNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::Fixed:
        dump(ts, downcast<ScrollingStateFixedNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::Sticky:
        dump(ts, downcast<ScrollingStateStickyNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::Positioned:
        dump(ts, downcast<ScrollingStatePositionedNode>(node), changedPropertiesOnly);
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

String RemoteScrollingCoordinatorTransaction::description() const
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

    return ts.release();
}

void RemoteScrollingCoordinatorTransaction::dump() const
{
    fprintf(stderr, "%s", description().utf8().data());
}
#endif

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
