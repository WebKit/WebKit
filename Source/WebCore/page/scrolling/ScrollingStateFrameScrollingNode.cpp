/*
 * Copyright (C) 2014-2026 Apple Inc. All rights reserved.
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
#include <ranges>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

// Assign to a copy-on-write group member (via DataRef::access) only when it differs, marking the
// node dirty. Consolidates the otherwise-identical compare / access / setPropertyChanged setter bodies.
#define SET_COW_PROPERTY(dataRef, member, newValue, property) do { \
        if ((dataRef)->member != (newValue)) { \
            (dataRef).access().member = (newValue); \
            setPropertyChanged(property); \
        } \
    } while (0)

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingStateFrameScrollingNode);

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
    ScrollRequestData&& requestedScrollData,
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
    std::optional<ScrollbarColor>&& scrollbarColor,
    UserInterfaceLayoutDirection scrollbarLayoutDirection,
    ScrollbarWidth scrollbarWidth,
    bool useDarkAppearanceForScrollbars,
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
    FloatBoxExtent&& obscuredContentInsets,
#if HAVE(NSREFRESHCONTROLLER)
    float topScrollStretchForRefreshController,
#endif
    bool visualViewportIsSmallerThanLayoutViewport,
    bool asyncFrameOrOverflowScrollingEnabled,
    bool wheelEventGesturesBecomeNonBlocking,
    bool scrollingPerformanceTestingEnabled,
    FloatRect layoutViewport,
    FloatSize sizeForVisibleContent,
    FloatPoint minLayoutViewportOrigin,
    FloatPoint maxLayoutViewportOrigin,
    std::optional<FloatSize> overrideVisualViewportSize,
    bool overlayScrollbarsEnabled
) : ScrollingStateScrollingNode(
    isMainFrame ? ScrollingNodeType::MainFrame : ScrollingNodeType::Subframe,
    scrollingNodeID,
    WTF::move(children),
    changedProperties,
    layerID,
    scrollableAreaSize,
    totalContentsSize,
    reachableContentsSize,
    scrollPosition,
    scrollOrigin,
    WTF::move(scrollableAreaParameters),
#if ENABLE(SCROLLING_THREAD)
    synchronousScrollingReasons,
#endif
    WTF::move(requestedScrollData),
    WTF::move(snapOffsetsInfo),
    currentHorizontalSnapPointIndex,
    currentVerticalSnapPointIndex,
    isMonitoringWheelEvents,
    scrollContainerLayer,
    scrolledContentsLayer,
    horizontalScrollbarLayer,
    verticalScrollbarLayer,
    mouseIsOverContentArea,
    WTF::move(mouseLocationState),
    WTF::move(scrollbarHoverState),
    WTF::move(scrollbarEnabledState),
    WTF::move(scrollbarColor),
    scrollbarLayoutDirection,
    scrollbarWidth,
    useDarkAppearanceForScrollbars,
    WTF::move(keyboardScrollData))
{
    SUPPRESS_UNCOUNTED_LOCAL auto& layout = m_staticLayoutData.access();
    layout.eventTrackingRegions = WTF::move(eventTrackingRegions);
    layout.layoutViewport = layoutViewport;
    layout.sizeForVisibleContent = sizeForVisibleContent;
    layout.minLayoutViewportOrigin = minLayoutViewportOrigin;
    layout.maxLayoutViewportOrigin = maxLayoutViewportOrigin;

    SUPPRESS_UNCOUNTED_LOCAL auto& config = m_staticConfigData.access();
    config.rootContentsLayer = rootContentsLayer;
    config.counterScrollingLayer = counterScrollingLayer;
    config.insetClipLayer = insetClipLayer;
    config.contentShadowLayer = contentShadowLayer;
    config.overrideVisualViewportSize = overrideVisualViewportSize;
    config.obscuredContentInsets = WTF::move(obscuredContentInsets);

    m_headerHeight = headerHeight;
    m_footerHeight = footerHeight;
    m_behaviorForFixed = WTF::move(scrollBehaviorForFixedElements);
    m_visualViewportIsSmallerThanLayoutViewport = visualViewportIsSmallerThanLayoutViewport;
    m_asyncFrameOrOverflowScrollingEnabled = asyncFrameOrOverflowScrollingEnabled;
    m_wheelEventGesturesBecomeNonBlocking = wheelEventGesturesBecomeNonBlocking;
    m_scrollingPerformanceTestingEnabled = scrollingPerformanceTestingEnabled;
    m_overlayScrollbarsEnabled = overlayScrollbarsEnabled;

    m_dynamicState.frameScaleFactor = frameScaleFactor;
#if HAVE(NSREFRESHCONTROLLER)
    m_dynamicState.topScrollStretchForRefreshController = topScrollStretchForRefreshController;
#endif

    ASSERT(isFrameScrollingNode());
}

ScrollingStateFrameScrollingNode::ScrollingStateFrameScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingStateScrollingNode(stateTree, nodeType, nodeID)
{
    ASSERT(isFrameScrollingNode());
}

ScrollingStateFrameScrollingNode::ScrollingStateFrameScrollingNode(const ScrollingStateFrameScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateScrollingNode(stateNode, adoptiveTree)
    , m_staticLayoutData(stateNode.m_staticLayoutData)
    , m_staticConfigData(stateNode.m_staticConfigData)
    , m_dynamicState(stateNode.m_dynamicState)
    , m_headerHeight(stateNode.m_headerHeight)
    , m_footerHeight(stateNode.m_footerHeight)
    , m_behaviorForFixed(stateNode.m_behaviorForFixed)
    , m_visualViewportIsSmallerThanLayoutViewport(stateNode.m_visualViewportIsSmallerThanLayoutViewport)
    , m_asyncFrameOrOverflowScrollingEnabled(stateNode.m_asyncFrameOrOverflowScrollingEnabled)
    , m_wheelEventGesturesBecomeNonBlocking(stateNode.m_wheelEventGesturesBecomeNonBlocking)
    , m_scrollingPerformanceTestingEnabled(stateNode.m_scrollingPerformanceTestingEnabled)
    , m_overlayScrollbarsEnabled(stateNode.m_overlayScrollbarsEnabled)
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
        Property::ObscuredContentInsets,
#if HAVE(NSREFRESHCONTROLLER)
        Property::TopScrollStretchForRefreshController,
#endif
        Property::VisualViewportIsSmallerThanLayoutViewport,
        Property::AsyncFrameOrOverflowScrollingEnabled,
        Property::WheelEventGesturesBecomeNonBlocking,
        Property::ScrollingPerformanceTestingEnabled,
        Property::LayoutViewport,
        Property::SizeForVisibleContent,
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
    if (m_dynamicState.frameScaleFactor == scaleFactor)
        return;

    m_dynamicState.frameScaleFactor = scaleFactor;

    setPropertyChanged(Property::FrameScaleFactor);
}

void ScrollingStateFrameScrollingNode::setEventTrackingRegions(const EventTrackingRegions& eventTrackingRegions)
{
    SET_COW_PROPERTY(m_staticLayoutData, eventTrackingRegions, eventTrackingRegions, Property::EventTrackingRegion);
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
    SET_COW_PROPERTY(m_staticLayoutData, layoutViewport, r, Property::LayoutViewport);
}

void ScrollingStateFrameScrollingNode::setSizeForVisibleContent(const FloatSize& size)
{
    SET_COW_PROPERTY(m_staticLayoutData, sizeForVisibleContent, size, Property::SizeForVisibleContent);
}

void ScrollingStateFrameScrollingNode::setMinLayoutViewportOrigin(const FloatPoint& p)
{
    SET_COW_PROPERTY(m_staticLayoutData, minLayoutViewportOrigin, p, Property::MinLayoutViewportOrigin);
}

void ScrollingStateFrameScrollingNode::setMaxLayoutViewportOrigin(const FloatPoint& p)
{
    SET_COW_PROPERTY(m_staticLayoutData, maxLayoutViewportOrigin, p, Property::MaxLayoutViewportOrigin);
}

void ScrollingStateFrameScrollingNode::setOverrideVisualViewportSize(std::optional<FloatSize> viewportSize)
{
    SET_COW_PROPERTY(m_staticConfigData, overrideVisualViewportSize, viewportSize, Property::OverrideVisualViewportSize);
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

void ScrollingStateFrameScrollingNode::setObscuredContentInsets(const FloatBoxExtent& obscuredContentInsets)
{
    SET_COW_PROPERTY(m_staticConfigData, obscuredContentInsets, obscuredContentInsets, Property::ObscuredContentInsets);
}

#if HAVE(NSREFRESHCONTROLLER)

void ScrollingStateFrameScrollingNode::setTopScrollStretchForRefreshController(float topScrollStretchForRefreshController)
{
    if (m_dynamicState.topScrollStretchForRefreshController == topScrollStretchForRefreshController)
        return;

    m_dynamicState.topScrollStretchForRefreshController = topScrollStretchForRefreshController;
    setPropertyChanged(Property::TopScrollStretchForRefreshController);
}

#endif

void ScrollingStateFrameScrollingNode::setRootContentsLayer(const LayerRepresentation& layerRepresentation)
{
    SET_COW_PROPERTY(m_staticConfigData, rootContentsLayer, layerRepresentation, Property::RootContentsLayer);
}

void ScrollingStateFrameScrollingNode::setCounterScrollingLayer(const LayerRepresentation& layerRepresentation)
{
    SET_COW_PROPERTY(m_staticConfigData, counterScrollingLayer, layerRepresentation, Property::CounterScrollingLayer);
}

void ScrollingStateFrameScrollingNode::setInsetClipLayer(const LayerRepresentation& layerRepresentation)
{
    SET_COW_PROPERTY(m_staticConfigData, insetClipLayer, layerRepresentation, Property::InsetClipLayer);
}

void ScrollingStateFrameScrollingNode::setContentShadowLayer(const LayerRepresentation& layerRepresentation)
{
    SET_COW_PROPERTY(m_staticConfigData, contentShadowLayer, layerRepresentation, Property::ContentShadowLayer);
}

void ScrollingStateFrameScrollingNode::setHeaderLayer(const LayerRepresentation& layerRepresentation)
{
    SET_COW_PROPERTY(m_staticConfigData, headerLayer, layerRepresentation, Property::HeaderLayer);
}

void ScrollingStateFrameScrollingNode::setFooterLayer(const LayerRepresentation& layerRepresentation)
{
    SET_COW_PROPERTY(m_staticConfigData, footerLayer, layerRepresentation, Property::FooterLayer);
}

void ScrollingStateFrameScrollingNode::setVisualViewportIsSmallerThanLayoutViewport(bool visualViewportIsSmallerThanLayoutViewport)
{
    if (visualViewportIsSmallerThanLayoutViewport == m_visualViewportIsSmallerThanLayoutViewport)
        return;

    m_visualViewportIsSmallerThanLayoutViewport = visualViewportIsSmallerThanLayoutViewport;
    setPropertyChanged(Property::VisualViewportIsSmallerThanLayoutViewport);
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

bool ScrollingStateFrameScrollingNode::hasUnchangedGroupsAs(const ScrollingStateNode& other) const
{
    if (!ScrollingStateScrollingNode::hasUnchangedGroupsAs(other))
        return false;
    auto& otherFrame = downcast<ScrollingStateFrameScrollingNode>(other);
    if (m_staticLayoutData.ptr() != otherFrame.m_staticLayoutData.ptr())
        return false;
    if (m_staticConfigData.ptr() != otherFrame.m_staticConfigData.ptr())
        return false;
    if (m_dynamicState != otherFrame.m_dynamicState)
        return false;
    // Direct (non-CoW) members, compared for the same reason as in the base class.
    if (m_headerHeight != otherFrame.m_headerHeight)
        return false;
    if (m_footerHeight != otherFrame.m_footerHeight)
        return false;
    if (m_behaviorForFixed != otherFrame.m_behaviorForFixed)
        return false;
    if (m_visualViewportIsSmallerThanLayoutViewport != otherFrame.m_visualViewportIsSmallerThanLayoutViewport)
        return false;
    if (m_asyncFrameOrOverflowScrollingEnabled != otherFrame.m_asyncFrameOrOverflowScrollingEnabled)
        return false;
    if (m_wheelEventGesturesBecomeNonBlocking != otherFrame.m_wheelEventGesturesBecomeNonBlocking)
        return false;
    if (m_scrollingPerformanceTestingEnabled != otherFrame.m_scrollingPerformanceTestingEnabled)
        return false;
    if (m_overlayScrollbarsEnabled != otherFrame.m_overlayScrollbarsEnabled)
        return false;
    return true;
}

void ScrollingStateFrameScrollingNode::clearLayerFieldsForUnchangedProperties()
{
    // First clear scrolling-node layers on the base class, then frame-only layers.
    ScrollingStateScrollingNode::clearLayerFieldsForUnchangedProperties();

    bool needsAccess = (!hasChangedProperty(Property::RootContentsLayer) && m_staticConfigData->rootContentsLayer)
        || (!hasChangedProperty(Property::CounterScrollingLayer) && m_staticConfigData->counterScrollingLayer)
        || (!hasChangedProperty(Property::InsetClipLayer) && m_staticConfigData->insetClipLayer)
        || (!hasChangedProperty(Property::ContentShadowLayer) && m_staticConfigData->contentShadowLayer)
        || (!hasChangedProperty(Property::HeaderLayer) && m_staticConfigData->headerLayer)
        || (!hasChangedProperty(Property::FooterLayer) && m_staticConfigData->footerLayer);
    if (!needsAccess)
        return;

    SUPPRESS_UNCOUNTED_LOCAL auto& config = m_staticConfigData.access();
    if (!hasChangedProperty(Property::RootContentsLayer))
        config.rootContentsLayer = { };
    if (!hasChangedProperty(Property::CounterScrollingLayer))
        config.counterScrollingLayer = { };
    if (!hasChangedProperty(Property::InsetClipLayer))
        config.insetClipLayer = { };
    if (!hasChangedProperty(Property::ContentShadowLayer))
        config.contentShadowLayer = { };
    if (!hasChangedProperty(Property::HeaderLayer))
        config.headerLayer = { };
    if (!hasChangedProperty(Property::FooterLayer))
        config.footerLayer = { };
}

#if ASSERT_ENABLED
void ScrollingStateFrameScrollingNode::verifyClearedLayerFieldsForUnchangedProperties() const
{
    ScrollingStateScrollingNode::verifyClearedLayerFieldsForUnchangedProperties();
    ASSERT(hasChangedProperty(Property::RootContentsLayer) || !m_staticConfigData->rootContentsLayer);
    ASSERT(hasChangedProperty(Property::CounterScrollingLayer) || !m_staticConfigData->counterScrollingLayer);
    ASSERT(hasChangedProperty(Property::InsetClipLayer) || !m_staticConfigData->insetClipLayer);
    ASSERT(hasChangedProperty(Property::ContentShadowLayer) || !m_staticConfigData->contentShadowLayer);
    ASSERT(hasChangedProperty(Property::HeaderLayer) || !m_staticConfigData->headerLayer);
    ASSERT(hasChangedProperty(Property::FooterLayer) || !m_staticConfigData->footerLayer);
}
#endif

void ScrollingStateFrameScrollingNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "Frame scrolling node"_s;
    
    ScrollingStateScrollingNode::dumpProperties(ts, behavior);

    SUPPRESS_UNCOUNTED_LOCAL auto& layout = m_staticLayoutData.get();
    SUPPRESS_UNCOUNTED_LOCAL auto& config = m_staticConfigData.get();

    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeLayerIDs) {
        ts.dumpProperty("root contents layer ID"_s, config.rootContentsLayer.layerID());
        if (config.counterScrollingLayer.layerID())
            ts.dumpProperty("counter scrolling layer ID"_s, config.counterScrollingLayer.layerID());
        if (config.insetClipLayer.layerID())
            ts.dumpProperty("inset clip layer ID"_s, config.insetClipLayer.layerID());
        if (config.contentShadowLayer.layerID())
            ts.dumpProperty("content shadow layer ID"_s, config.contentShadowLayer.layerID());
        if (config.headerLayer.layerID())
            ts.dumpProperty("header layer ID"_s, config.headerLayer.layerID());
        if (config.footerLayer.layerID())
            ts.dumpProperty("footer layer ID"_s, config.footerLayer.layerID());
    }

    if (m_dynamicState.frameScaleFactor != 1)
        ts.dumpProperty("frame scale factor"_s, m_dynamicState.frameScaleFactor);
    if (config.obscuredContentInsets.top())
        ts.dumpProperty("top content inset"_s, config.obscuredContentInsets.top());
    if (config.obscuredContentInsets.bottom())
        ts.dumpProperty("bottom content inset"_s, config.obscuredContentInsets.bottom());
    if (config.obscuredContentInsets.left())
        ts.dumpProperty("left content inset"_s, config.obscuredContentInsets.left());
    if (config.obscuredContentInsets.right())
        ts.dumpProperty("right content inset"_s, config.obscuredContentInsets.right());
#if HAVE(NSREFRESHCONTROLLER)
    if (m_dynamicState.topScrollStretchForRefreshController)
        ts.dumpProperty("top scroll stretch for refresh controller"_s, m_dynamicState.topScrollStretchForRefreshController);
#endif
    if (m_headerHeight)
        ts.dumpProperty("header height"_s, m_headerHeight);
    if (m_footerHeight)
        ts.dumpProperty("footer height"_s, m_footerHeight);

    ts.dumpProperty("layout viewport"_s, layout.layoutViewport);

    if (layout.layoutViewport.size() != layout.sizeForVisibleContent)
        ts.dumpProperty("size for visible content"_s, layout.sizeForVisibleContent);

    ts.dumpProperty("min layout viewport origin"_s, layout.minLayoutViewportOrigin);
    ts.dumpProperty("max layout viewport origin"_s, layout.maxLayoutViewportOrigin);

    if (config.overrideVisualViewportSize)
        ts.dumpProperty("override visual viewport size"_s, config.overrideVisualViewportSize.value());

    if (!layout.eventTrackingRegions.asynchronousDispatchRegion.isEmpty()) {
        TextStream::GroupScope scope(ts);
        ts << "asynchronous event dispatch region"_s;
        for (auto rect : layout.eventTrackingRegions.asynchronousDispatchRegion.rects()) {
            ts << '\n';
            ts << indent << rect;
        }
    }

    auto& synchronousDispatchRegionMap = layout.eventTrackingRegions.eventSpecificSynchronousDispatchRegions;
    if (!synchronousDispatchRegionMap.isEmpty()) {
        auto eventRegionNames = copyToVector(synchronousDispatchRegionMap.keys());
        std::ranges::sort(eventRegionNames);
        for (const auto& name : eventRegionNames) {
            const auto& region = synchronousDispatchRegionMap.get(name);
            TextStream::GroupScope scope(ts);
            ts << "synchronous event dispatch region for event "_s << EventTrackingRegions::eventName(name);
            for (auto rect : region.rects()) {
                ts << '\n';
                ts << indent << rect;
            }
        }
    }

    ts.dumpProperty("behavior for fixed"_s, m_behaviorForFixed);

    if (m_visualViewportIsSmallerThanLayoutViewport)
        ts.dumpProperty("visual viewport smaller than layout viewport"_s, m_visualViewportIsSmallerThanLayoutViewport);
}

} // namespace WebCore

#undef SET_COW_PROPERTY

#endif // ENABLE(ASYNC_SCROLLING)
