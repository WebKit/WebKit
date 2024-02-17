/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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

#include <WebCore/GraphicsLayer.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollingStateFixedNode.h>
#include <WebCore/ScrollingStateFrameHostingNode.h>
#include <WebCore/ScrollingStateFrameScrollingNode.h>
#include <WebCore/ScrollingStateOverflowScrollProxyNode.h>
#include <WebCore/ScrollingStateOverflowScrollingNode.h>
#include <WebCore/ScrollingStatePluginHostingNode.h>
#include <WebCore/ScrollingStatePluginScrollingNode.h>
#include <WebCore/ScrollingStatePositionedNode.h>
#include <WebCore/ScrollingStateStickyNode.h>
#include <WebCore/ScrollingStateTree.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

RemoteScrollingCoordinatorTransaction::RemoteScrollingCoordinatorTransaction() = default;

RemoteScrollingCoordinatorTransaction::RemoteScrollingCoordinatorTransaction(std::unique_ptr<WebCore::ScrollingStateTree>&& scrollingStateTree, bool clearScrollLatching, WebCore::FrameIdentifier frameID, FromDeserialization fromDeserialization)
    : m_scrollingStateTree(WTFMove(scrollingStateTree))
    , m_clearScrollLatching(clearScrollLatching)
    , m_rootFrameID(frameID)
{
    if (!m_scrollingStateTree)
        m_scrollingStateTree = makeUnique<WebCore::ScrollingStateTree>();
    if (fromDeserialization == FromDeserialization::Yes)
        m_scrollingStateTree->attachDeserializedNodes();
}

RemoteScrollingCoordinatorTransaction::RemoteScrollingCoordinatorTransaction(RemoteScrollingCoordinatorTransaction&&) = default;

RemoteScrollingCoordinatorTransaction& RemoteScrollingCoordinatorTransaction::operator=(RemoteScrollingCoordinatorTransaction&&) = default;

RemoteScrollingCoordinatorTransaction::~RemoteScrollingCoordinatorTransaction() = default;

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
        ts.dumpProperty("requested-type", requestedScrollData.requestType);
        if (requestedScrollData.requestType != ScrollRequestType::CancelAnimatedScroll) {
            if (requestedScrollData.requestType == ScrollRequestType::DeltaUpdate)
                ts.dumpProperty("requested-scroll-delta", std::get<FloatSize>(requestedScrollData.scrollPositionOrDelta));
            else
                ts.dumpProperty("requested-scroll-position", std::get<FloatPoint>(requestedScrollData.scrollPositionOrDelta));

            ts.dumpProperty("requested-scroll-position-is-programatic", requestedScrollData.scrollType);
            ts.dumpProperty("requested-scroll-position-clamping", requestedScrollData.clamping);
            ts.dumpProperty("requested-scroll-position-animated", requestedScrollData.animated == ScrollIsAnimated::Yes);
        }
    }

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
        ts.dumpProperty("scroll-container-layer", static_cast<PlatformLayerIdentifier>(node.scrollContainerLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
        ts.dumpProperty("scrolled-contents-layer", static_cast<PlatformLayerIdentifier>(node.scrolledContentsLayer()));

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

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::IsMonitoringWheelEvents))
        ts.dumpProperty("is monitoring wheel events", node.isMonitoringWheelEvents());

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
        ts.dumpProperty("clip-inset-layer", static_cast<PlatformLayerIdentifier>(node.insetClipLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::ContentShadowLayer))
        ts.dumpProperty("content-shadow-layer", static_cast<PlatformLayerIdentifier>(node.contentShadowLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::HeaderLayer))
        ts.dumpProperty("header-layer", static_cast<PlatformLayerIdentifier>(node.headerLayer()));

    if (!changedPropertiesOnly || node.hasChangedProperty(ScrollingStateNode::Property::FooterLayer))
        ts.dumpProperty("footer-layer", static_cast<PlatformLayerIdentifier>(node.footerLayer()));
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

static void dump(TextStream& ts, const ScrollingStatePluginHostingNode& node, bool changedPropertiesOnly)
{
}

static void dump(TextStream& ts, const ScrollingStatePluginScrollingNode& node, bool changedPropertiesOnly)
{
    dump(ts, static_cast<const ScrollingStateScrollingNode&>(node), changedPropertiesOnly);
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
        ts.dumpProperty("layer", static_cast<PlatformLayerIdentifier>(node.layer()));
    
    switch (node.nodeType()) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        dump(ts, downcast<ScrollingStateFrameScrollingNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::FrameHosting:
        dump(ts, downcast<ScrollingStateFrameHostingNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::PluginScrolling:
        dump(ts, downcast<ScrollingStatePluginScrollingNode>(node), changedPropertiesOnly);
        break;
    case ScrollingNodeType::PluginHosting:
        dump(ts, downcast<ScrollingStatePluginHostingNode>(node), changedPropertiesOnly);
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

    if (!node.children().isEmpty()) {
        TextStream::GroupScope group(ts);
        ts << "children";

        for (auto& childNode : node.children())
            recursiveDumpNodes(ts, childNode, changedPropertiesOnly);
    }
}

static void dump(TextStream& ts, const ScrollingStateTree& stateTree, bool changedPropertiesOnly)
{
    ts.dumpProperty("has changed properties", stateTree.hasChangedProperties());
    ts.dumpProperty("has new root node", stateTree.hasNewRootStateNode());

    if (stateTree.rootStateNode())
        recursiveDumpNodes(ts, Ref { *stateTree.rootStateNode() }, changedPropertiesOnly);
}

String RemoteScrollingCoordinatorTransaction::description() const
{
    TextStream ts;

    if (m_clearScrollLatching)
        ts.dumpProperty("clear scroll latching", clearScrollLatching());

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
    WTFLogAlways("%s", description().utf8().data());
}
#endif // !defined(NDEBUG) || !LOG_DISABLED

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
