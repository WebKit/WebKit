/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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
#include "ScrollingStateFrameScrollingNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateTree.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollingStateFrameScrollingNode::ScrollingStateFrameScrollingNode(
    bool isMainFrame,
    ScrollingNodeID scrollingNodeID,
    Vector<Ref<WebCore::ScrollingStateNode>>&& children,
    OptionSet<ScrollingStateNodeProperty> changedProperties,
    std::optional<WebCore::PlatformLayerIdentifier> layerID,
    FloatSize scrollableAreaSize,
    FloatSize totalContentsSize,
    FloatSize reachableContentsSize,
    FloatPoint scrollPosition,
    IntPoint scrollOrigin,
    ScrollableAreaParameters&& scrollableAreaParameters,
#if ENABLE(SCROLLING_THREAD)
    OptionSet<SynchronousScrollingReason> synchronousScrollingReasons,
#endif
    RequestedScrollData&& requestedScrollData,
    FloatScrollSnapOffsetsInfo&& snapOffsetsInfo,
    std::optional<unsigned> currentHorizontalSnapPointIndex,
    std::optional<unsigned> currentVerticalSnapPointIndex,
    bool isMonitoringWheelEvents,
    std::optional<PlatformLayerIdentifier> scrollContainerLayer,
    std::optional<PlatformLayerIdentifier> scrolledContentsLayer,
    std::optional<PlatformLayerIdentifier> horizontalScrollbarLayer,
    std::optional<PlatformLayerIdentifier> verticalScrollbarLayer,
    bool mouseIsOverContentArea,
    MouseLocationState&& mouseLocationState,
    ScrollbarHoverState&& scrollbarHoverState,
    ScrollbarEnabledState&& scrollbarEnabledState,
    UserInterfaceLayoutDirection scrollbarLayoutDirection,
    ScrollbarWidth scrollbarWidth,
    RequestedKeyboardScrollData&& keyboardScrollData,
    float frameScaleFactor,
    EventTrackingRegions&& eventTrackingRegions,
    std::optional<PlatformLayerIdentifier> rootContentsLayer,
    std::optional<PlatformLayerIdentifier> counterScrollingLayer,
    std::optional<PlatformLayerIdentifier> insetClipLayer,
    std::optional<PlatformLayerIdentifier> contentShadowLayer,
    int headerHeight,
    int footerHeight,
    ScrollBehaviorForFixedElements&& scrollBehaviorForFixedElements,
    float topContentInset,
    bool fixedElementsLayoutRelativeToFrame,
    bool visualViewportIsSmallerThanLayoutViewport,
    bool asyncFrameOrOverflowScrollingEnabled,
    bool wheelEventGesturesBecomeNonBlocking,
    bool scrollingPerformanceTestingEnabled,
    FloatRect layoutViewport,
    FloatPoint minLayoutViewportOrigin,
    FloatPoint maxLayoutViewportOrigin,
    std::optional<FloatSize> overrideVisualViewportSize,
    bool overlayScrollbarsEnabled
) : ScrollingStateScrollingNode(
    isMainFrame ? ScrollingNodeType::MainFrame : ScrollingNodeType::Subframe,
    scrollingNodeID,
    WTFMove(children),
    changedProperties,
    layerID,
    scrollableAreaSize,
    totalContentsSize,
    reachableContentsSize,
    scrollPosition,
    scrollOrigin,
    WTFMove(scrollableAreaParameters),
#if ENABLE(SCROLLING_THREAD)
    synchronousScrollingReasons,
#endif
    WTFMove(requestedScrollData),
    WTFMove(snapOffsetsInfo),
    currentHorizontalSnapPointIndex,
    currentVerticalSnapPointIndex,
    isMonitoringWheelEvents,
    scrollContainerLayer,
    scrolledContentsLayer,
    horizontalScrollbarLayer,
    verticalScrollbarLayer,
    mouseIsOverContentArea,
    WTFMove(mouseLocationState),
    WTFMove(scrollbarHoverState),
    WTFMove(scrollbarEnabledState),
    scrollbarLayoutDirection,
    scrollbarWidth,
    WTFMove(keyboardScrollData))
    , m_rootContentsLayer(rootContentsLayer.value_or(PlatformLayerIdentifier()))
    , m_counterScrollingLayer(counterScrollingLayer.value_or(PlatformLayerIdentifier()))
    , m_insetClipLayer(insetClipLayer.value_or(PlatformLayerIdentifier()))
    , m_contentShadowLayer(contentShadowLayer.value_or(PlatformLayerIdentifier()))
    , m_eventTrackingRegions(WTFMove(eventTrackingRegions))
    , m_layoutViewport(layoutViewport)
    , m_minLayoutViewportOrigin(minLayoutViewportOrigin)
    , m_maxLayoutViewportOrigin(maxLayoutViewportOrigin)
    , m_overrideVisualViewportSize(overrideVisualViewportSize)
    , m_frameScaleFactor(frameScaleFactor)
    , m_topContentInset(topContentInset)
    , m_headerHeight(headerHeight)
    , m_footerHeight(footerHeight)
    , m_behaviorForFixed(WTFMove(scrollBehaviorForFixedElements))

    , m_fixedElementsLayoutRelativeToFrame(fixedElementsLayoutRelativeToFrame)
    , m_visualViewportIsSmallerThanLayoutViewport(visualViewportIsSmallerThanLayoutViewport)
    , m_asyncFrameOrOverflowScrollingEnabled(asyncFrameOrOverflowScrollingEnabled)
    , m_wheelEventGesturesBecomeNonBlocking(wheelEventGesturesBecomeNonBlocking)
    , m_scrollingPerformanceTestingEnabled(scrollingPerformanceTestingEnabled)
    , m_overlayScrollbarsEnabled(overlayScrollbarsEnabled)
{
    ASSERT(isFrameScrollingNode());
}

ScrollingStateFrameScrollingNode::ScrollingStateFrameScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingStateScrollingNode(stateTree, nodeType, nodeID)
{
    ASSERT(isFrameScrollingNode());
}

ScrollingStateFrameScrollingNode::ScrollingStateFrameScrollingNode(const ScrollingStateFrameScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateScrollingNode(stateNode, adoptiveTree)
    , m_eventTrackingRegions(stateNode.eventTrackingRegions())
    , m_layoutViewport(stateNode.layoutViewport())
    , m_minLayoutViewportOrigin(stateNode.minLayoutViewportOrigin())
    , m_maxLayoutViewportOrigin(stateNode.maxLayoutViewportOrigin())
    , m_overrideVisualViewportSize(stateNode.overrideVisualViewportSize())
    , m_frameScaleFactor(stateNode.frameScaleFactor())
    , m_topContentInset(stateNode.topContentInset())
    , m_headerHeight(stateNode.headerHeight())
    , m_footerHeight(stateNode.footerHeight())
    , m_behaviorForFixed(stateNode.scrollBehaviorForFixedElements())
    , m_fixedElementsLayoutRelativeToFrame(stateNode.fixedElementsLayoutRelativeToFrame())
    , m_visualViewportIsSmallerThanLayoutViewport(stateNode.visualViewportIsSmallerThanLayoutViewport())
    , m_asyncFrameOrOverflowScrollingEnabled(stateNode.asyncFrameOrOverflowScrollingEnabled())
    , m_wheelEventGesturesBecomeNonBlocking(stateNode.wheelEventGesturesBecomeNonBlocking())
    , m_scrollingPerformanceTestingEnabled(stateNode.scrollingPerformanceTestingEnabled())
    , m_overlayScrollbarsEnabled(stateNode.overlayScrollbarsEnabled())
{
    if (hasChangedProperty(Property::RootContentsLayer))
        setRootContentsLayer(stateNode.rootContentsLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(Property::CounterScrollingLayer))
        setCounterScrollingLayer(stateNode.counterScrollingLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(Property::InsetClipLayer))
        setInsetClipLayer(stateNode.insetClipLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(Property::ContentShadowLayer))
        setContentShadowLayer(stateNode.contentShadowLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(Property::HeaderLayer))
        setHeaderLayer(stateNode.headerLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(Property::FooterLayer))
        setFooterLayer(stateNode.footerLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));
}

ScrollingStateFrameScrollingNode::~ScrollingStateFrameScrollingNode() = default;

Ref<ScrollingStateNode> ScrollingStateFrameScrollingNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(*new ScrollingStateFrameScrollingNode(*this, adoptiveTree));
}

OptionSet<ScrollingStateNode::Property> ScrollingStateFrameScrollingNode::applicableProperties() const
{
    constexpr OptionSet<Property> nodeProperties = {
        Property::FrameScaleFactor,
        Property::EventTrackingRegion,
        Property::RootContentsLayer,
        Property::CounterScrollingLayer,
        Property::InsetClipLayer,
        Property::ContentShadowLayer,
        Property::HeaderHeight,
        Property::FooterHeight,
        Property::HeaderLayer,
        Property::FooterLayer,
        Property::BehaviorForFixedElements,
        Property::TopContentInset,
        Property::FixedElementsLayoutRelativeToFrame,
        Property::VisualViewportIsSmallerThanLayoutViewport,
        Property::AsyncFrameOrOverflowScrollingEnabled,
        Property::WheelEventGesturesBecomeNonBlocking,
        Property::ScrollingPerformanceTestingEnabled,
        Property::LayoutViewport,
        Property::MinLayoutViewportOrigin,
        Property::MaxLayoutViewportOrigin,
        Property::OverrideVisualViewportSize,
        Property::OverlayScrollbarsEnabled,
    };

    auto properties = ScrollingStateScrollingNode::applicableProperties();
    properties.add(nodeProperties);
    return properties;
}

void ScrollingStateFrameScrollingNode::setFrameScaleFactor(float scaleFactor)
{
    if (m_frameScaleFactor == scaleFactor)
        return;

    m_frameScaleFactor = scaleFactor;

    setPropertyChanged(Property::FrameScaleFactor);
}

void ScrollingStateFrameScrollingNode::setEventTrackingRegions(const EventTrackingRegions& eventTrackingRegions)
{
    if (m_eventTrackingRegions == eventTrackingRegions)
        return;

    m_eventTrackingRegions = eventTrackingRegions;
    setPropertyChanged(Property::EventTrackingRegion);
}

void ScrollingStateFrameScrollingNode::setScrollBehaviorForFixedElements(ScrollBehaviorForFixedElements behaviorForFixed)
{
    if (m_behaviorForFixed == behaviorForFixed)
        return;

    m_behaviorForFixed = behaviorForFixed;
    setPropertyChanged(Property::BehaviorForFixedElements);
}

void ScrollingStateFrameScrollingNode::setLayoutViewport(const FloatRect& r)
{
    if (m_layoutViewport == r)
        return;

    m_layoutViewport = r;
    setPropertyChanged(Property::LayoutViewport);
}

void ScrollingStateFrameScrollingNode::setMinLayoutViewportOrigin(const FloatPoint& p)
{
    if (m_minLayoutViewportOrigin == p)
        return;

    m_minLayoutViewportOrigin = p;
    setPropertyChanged(Property::MinLayoutViewportOrigin);
}

void ScrollingStateFrameScrollingNode::setMaxLayoutViewportOrigin(const FloatPoint& p)
{
    if (m_maxLayoutViewportOrigin == p)
        return;

    m_maxLayoutViewportOrigin = p;
    setPropertyChanged(Property::MaxLayoutViewportOrigin);
}

void ScrollingStateFrameScrollingNode::setOverrideVisualViewportSize(std::optional<FloatSize> viewportSize)
{
    if (viewportSize == m_overrideVisualViewportSize)
        return;

    m_overrideVisualViewportSize = viewportSize;
    setPropertyChanged(Property::OverrideVisualViewportSize);
}

void ScrollingStateFrameScrollingNode::setHeaderHeight(int headerHeight)
{
    if (m_headerHeight == headerHeight)
        return;

    m_headerHeight = headerHeight;
    setPropertyChanged(Property::HeaderHeight);
}

void ScrollingStateFrameScrollingNode::setFooterHeight(int footerHeight)
{
    if (m_footerHeight == footerHeight)
        return;

    m_footerHeight = footerHeight;
    setPropertyChanged(Property::FooterHeight);
}

void ScrollingStateFrameScrollingNode::setTopContentInset(float topContentInset)
{
    if (m_topContentInset == topContentInset)
        return;

    m_topContentInset = topContentInset;
    setPropertyChanged(Property::TopContentInset);
}

void ScrollingStateFrameScrollingNode::setRootContentsLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_rootContentsLayer)
        return;

    m_rootContentsLayer = layerRepresentation;
    setPropertyChanged(Property::RootContentsLayer);
}

void ScrollingStateFrameScrollingNode::setCounterScrollingLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_counterScrollingLayer)
        return;
    
    m_counterScrollingLayer = layerRepresentation;
    setPropertyChanged(Property::CounterScrollingLayer);
}

void ScrollingStateFrameScrollingNode::setInsetClipLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_insetClipLayer)
        return;
    
    m_insetClipLayer = layerRepresentation;
    setPropertyChanged(Property::InsetClipLayer);
}

void ScrollingStateFrameScrollingNode::setContentShadowLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_contentShadowLayer)
        return;
    
    m_contentShadowLayer = layerRepresentation;
    setPropertyChanged(Property::ContentShadowLayer);
}

void ScrollingStateFrameScrollingNode::setHeaderLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_headerLayer)
        return;
    
    m_headerLayer = layerRepresentation;
    setPropertyChanged(Property::HeaderLayer);
}

void ScrollingStateFrameScrollingNode::setFooterLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_footerLayer)
        return;
    
    m_footerLayer = layerRepresentation;
    setPropertyChanged(Property::FooterLayer);
}

void ScrollingStateFrameScrollingNode::setVisualViewportIsSmallerThanLayoutViewport(bool visualViewportIsSmallerThanLayoutViewport)
{
    if (visualViewportIsSmallerThanLayoutViewport == m_visualViewportIsSmallerThanLayoutViewport)
        return;
    
    m_visualViewportIsSmallerThanLayoutViewport = visualViewportIsSmallerThanLayoutViewport;
    setPropertyChanged(Property::VisualViewportIsSmallerThanLayoutViewport);
}

void ScrollingStateFrameScrollingNode::setFixedElementsLayoutRelativeToFrame(bool fixedElementsLayoutRelativeToFrame)
{
    if (fixedElementsLayoutRelativeToFrame == m_fixedElementsLayoutRelativeToFrame)
        return;
    
    m_fixedElementsLayoutRelativeToFrame = fixedElementsLayoutRelativeToFrame;
    setPropertyChanged(Property::FixedElementsLayoutRelativeToFrame);
}

void ScrollingStateFrameScrollingNode::setAsyncFrameOrOverflowScrollingEnabled(bool enabled)
{
    if (enabled == m_asyncFrameOrOverflowScrollingEnabled)
        return;
    
    m_asyncFrameOrOverflowScrollingEnabled = enabled;
    setPropertyChanged(Property::AsyncFrameOrOverflowScrollingEnabled);
}

void ScrollingStateFrameScrollingNode::setWheelEventGesturesBecomeNonBlocking(bool enabled)
{
    if (enabled == m_wheelEventGesturesBecomeNonBlocking)
        return;
    
    m_wheelEventGesturesBecomeNonBlocking = enabled;
    setPropertyChanged(Property::WheelEventGesturesBecomeNonBlocking);
}

void ScrollingStateFrameScrollingNode::setScrollingPerformanceTestingEnabled(bool enabled)
{
    if (enabled == m_scrollingPerformanceTestingEnabled)
        return;
    
    m_scrollingPerformanceTestingEnabled = enabled;
    setPropertyChanged(Property::ScrollingPerformanceTestingEnabled);
}

void ScrollingStateFrameScrollingNode::setOverlayScrollbarsEnabled(bool enabled)
{
    if (m_overlayScrollbarsEnabled == enabled)
        return;
    m_overlayScrollbarsEnabled = enabled;
    setPropertyChanged(Property::OverlayScrollbarsEnabled);
}

bool ScrollingStateFrameScrollingNode::isMainFrame() const
{
    return nodeType() == ScrollingNodeType::MainFrame;
}

void ScrollingStateFrameScrollingNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "Frame scrolling node";
    
    ScrollingStateScrollingNode::dumpProperties(ts, behavior);
    
    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeLayerIDs) {
        ts.dumpProperty("root contents layer ID", m_rootContentsLayer.layerID());
        if (m_counterScrollingLayer.layerID())
            ts.dumpProperty("counter scrolling layer ID", m_counterScrollingLayer.layerID());
        if (m_insetClipLayer.layerID())
            ts.dumpProperty("inset clip layer ID", m_insetClipLayer.layerID());
        if (m_contentShadowLayer.layerID())
            ts.dumpProperty("content shadow layer ID", m_contentShadowLayer.layerID());
        if (m_headerLayer.layerID())
            ts.dumpProperty("header layer ID", m_headerLayer.layerID());
        if (m_footerLayer.layerID())
            ts.dumpProperty("footer layer ID", m_footerLayer.layerID());
    }

    if (m_frameScaleFactor != 1)
        ts.dumpProperty("frame scale factor", m_frameScaleFactor);
    if (m_topContentInset)
        ts.dumpProperty("top content inset", m_topContentInset);
    if (m_headerHeight)
        ts.dumpProperty("header height", m_headerHeight);
    if (m_footerHeight)
        ts.dumpProperty("footer height", m_footerHeight);
    
    ts.dumpProperty("layout viewport", m_layoutViewport);
    ts.dumpProperty("min layout viewport origin", m_minLayoutViewportOrigin);
    ts.dumpProperty("max layout viewport origin", m_maxLayoutViewportOrigin);
    
    if (m_overrideVisualViewportSize)
        ts.dumpProperty("override visual viewport size", m_overrideVisualViewportSize.value());

    if (!m_eventTrackingRegions.asynchronousDispatchRegion.isEmpty()) {
        TextStream::GroupScope scope(ts);
        ts << "asynchronous event dispatch region";
        for (auto rect : m_eventTrackingRegions.asynchronousDispatchRegion.rects()) {
            ts << "\n";
            ts << indent << rect;
        }
    }

    auto& synchronousDispatchRegionMap = m_eventTrackingRegions.eventSpecificSynchronousDispatchRegions;
    if (!synchronousDispatchRegionMap.isEmpty()) {
        auto eventRegionNames = copyToVector(synchronousDispatchRegionMap.keys());
        std::sort(eventRegionNames.begin(), eventRegionNames.end());
        for (const auto& name : eventRegionNames) {
            const auto& region = synchronousDispatchRegionMap.get(name);
            TextStream::GroupScope scope(ts);
            ts << "synchronous event dispatch region for event " << EventTrackingRegions::eventName(name);
            for (auto rect : region.rects()) {
                ts << "\n";
                ts << indent << rect;
            }
        }
    }

    ts.dumpProperty("behavior for fixed", m_behaviorForFixed);

    if (m_visualViewportIsSmallerThanLayoutViewport)
        ts.dumpProperty("visual viewport smaller than layout viewport", m_visualViewportIsSmallerThanLayoutViewport);

    if (m_fixedElementsLayoutRelativeToFrame)
        ts.dumpProperty("fixed elements lay out relative to frame", m_fixedElementsLayoutRelativeToFrame);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
