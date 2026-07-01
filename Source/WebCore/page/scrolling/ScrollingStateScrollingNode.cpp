/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
#include "ScrollingStateScrollingNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateTree.h"
#include <wtf/DataRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
#include "ScrollerImpAdwaita.h"
#endif

namespace WebCore {

// Assign to a copy-on-write group member (via DataRef::access) only when it differs, marking the
// node dirty. Consolidates the otherwise-identical compare / access / setPropertyChanged setter bodies.
#define SET_COW_PROPERTY(dataRef, member, newValue, property) do { \
        if ((dataRef)->member != (newValue)) { \
            (dataRef).access().member = (newValue); \
            setPropertyChanged(property); \
        } \
    } while (0)

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingStateScrollingNode);

ScrollingStateScrollingNode::ScrollingStateScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingStateNode(nodeType, stateTree, nodeID)
{
    scrollingStateTree().scrollingNodeAdded();
}

ScrollingStateScrollingNode::ScrollingStateScrollingNode(
    ScrollingNodeType nodeType,
    ScrollingNodeID nodeID,
    Vector<Ref<ScrollingStateNode>>&& children,
    OptionSet<ScrollingStateNodeProperty> changedProperties,
    std::optional<PlatformLayerIdentifier> layerID,
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
    RequestedKeyboardScrollData&& keyboardScrollData
) : ScrollingStateNode(nodeType, nodeID, WTF::move(children), changedProperties, layerID)
    , m_requestedScrollData(WTF::move(requestedScrollData))
{
    SUPPRESS_UNCOUNTED_LOCAL auto& layout = m_staticLayoutData.access();
    layout.scrollableAreaSize = scrollableAreaSize;
    layout.totalContentsSize = totalContentsSize;
    layout.reachableContentsSize = reachableContentsSize;
    layout.snapOffsetsInfo = WTF::move(snapOffsetsInfo);

    SUPPRESS_UNCOUNTED_LOCAL auto& config = m_staticConfigData.access();
    config.scrollOrigin = scrollOrigin;
    config.scrollableAreaParameters = WTF::move(scrollableAreaParameters);
    config.scrollbarColor = WTF::move(scrollbarColor);
    config.scrollContainerLayer = scrollContainerLayer;
    config.scrolledContentsLayer = scrolledContentsLayer;
    config.horizontalScrollbarLayer = horizontalScrollbarLayer;
    config.verticalScrollbarLayer = verticalScrollbarLayer;
#if ENABLE(SCROLLING_THREAD)
    config.synchronousScrollingReasons = synchronousScrollingReasons;
#endif

    m_scrollbarEnabledState = WTF::move(scrollbarEnabledState);
    m_scrollbarLayoutDirection = scrollbarLayoutDirection;
    m_scrollbarWidth = scrollbarWidth;
    m_useDarkAppearanceForScrollbars = useDarkAppearanceForScrollbars;

    m_dynamicState.scrollPosition = scrollPosition;
    m_dynamicState.currentHorizontalSnapPointIndex = currentHorizontalSnapPointIndex;
    m_dynamicState.currentVerticalSnapPointIndex = currentVerticalSnapPointIndex;
    m_dynamicState.scrollbarHoverState = WTF::move(scrollbarHoverState);
    m_dynamicState.mouseLocationState = WTF::move(mouseLocationState);
    m_dynamicState.keyboardScrollData = WTF::move(keyboardScrollData);
    m_dynamicState.isMonitoringWheelEvents = isMonitoringWheelEvents;
    m_dynamicState.mouseIsOverContentArea = mouseIsOverContentArea;

    // scrollingNodeAdded will be called in attachAfterDeserialization.
}

ScrollingStateScrollingNode::ScrollingStateScrollingNode(const ScrollingStateScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(stateNode, adoptiveTree)
    , m_staticLayoutData(stateNode.m_staticLayoutData)
    , m_staticConfigData(stateNode.m_staticConfigData)
    , m_requestedScrollData(stateNode.requestedScrollData())
    , m_scrollbarEnabledState(stateNode.m_scrollbarEnabledState)
    , m_scrollbarLayoutDirection(stateNode.m_scrollbarLayoutDirection)
    , m_scrollbarWidth(stateNode.m_scrollbarWidth)
    , m_useDarkAppearanceForScrollbars(stateNode.m_useDarkAppearanceForScrollbars)
#if PLATFORM(MAC) || USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    , m_verticalScrollerImp(stateNode.verticalScrollerImp())
    , m_horizontalScrollerImp(stateNode.horizontalScrollerImp())
#endif
#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    , m_scrollbarOpacity(stateNode.scrollbarOpacity())
#endif
{
    scrollingStateTree().scrollingNodeAdded();

    // Copy dynamic state. The legacy IPC clone path elided snap-point indices from the
    // copy ctor; we no longer do, because (a) the elision was redundant for IPC (the encoder
    // gates serialization on Property::CurrentHorizontal/VerticalSnapOffsetIndex, not on the
    // field value), and (b) elision in the m_lastCommittedTree snapshot caused nodes with a
    // non-default snap index to be perpetually flagged dirty by hasUnchangedGroupsAs.
    m_dynamicState.scrollPosition = stateNode.m_dynamicState.scrollPosition;
    m_dynamicState.currentHorizontalSnapPointIndex = stateNode.m_dynamicState.currentHorizontalSnapPointIndex;
    m_dynamicState.currentVerticalSnapPointIndex = stateNode.m_dynamicState.currentVerticalSnapPointIndex;
#if PLATFORM(MAC) || USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    m_dynamicState.scrollbarHoverState = stateNode.m_dynamicState.scrollbarHoverState;
#endif
#if PLATFORM(MAC)
    m_dynamicState.mouseLocationState = stateNode.m_dynamicState.mouseLocationState;
#endif
    m_dynamicState.keyboardScrollData = stateNode.m_dynamicState.keyboardScrollData;
    m_dynamicState.isMonitoringWheelEvents = stateNode.m_dynamicState.isMonitoringWheelEvents;
    m_dynamicState.mouseIsOverContentArea = stateNode.m_dynamicState.mouseIsOverContentArea;

    if (hasChangedProperty(Property::ScrollContainerLayer))
        setScrollContainerLayer(stateNode.scrollContainerLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(Property::ScrolledContentsLayer))
        setScrolledContentsLayer(stateNode.scrolledContentsLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(Property::VerticalScrollbarLayer))
        setVerticalScrollbarLayer(stateNode.verticalScrollbarLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(Property::HorizontalScrollbarLayer))
        setHorizontalScrollbarLayer(stateNode.horizontalScrollbarLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));
}

ScrollingStateScrollingNode::~ScrollingStateScrollingNode()
{
    if (isAttachedToScrollingStateTree())
        scrollingStateTree().scrollingNodeRemoved();
}

OptionSet<ScrollingStateNode::Property> ScrollingStateScrollingNode::applicableProperties() const
{
    // Note that this list does not include Property::RequestedScrollPosition, which is imperative, not stateful.
    constexpr OptionSet<Property> nodeProperties = {
        Property::ScrollableAreaSize,
        Property::TotalContentsSize,
        Property::ReachableContentsSize,
        Property::ScrollPosition,
        Property::ScrollOrigin,
        Property::ScrollableAreaParams,
#if ENABLE(SCROLLING_THREAD)
        Property::ReasonsForSynchronousScrolling,
#endif
        Property::SnapOffsetsInfo,
        Property::CurrentHorizontalSnapOffsetIndex,
        Property::CurrentVerticalSnapOffsetIndex,
        Property::IsMonitoringWheelEvents,
        Property::ScrollContainerLayer,
        Property::ScrolledContentsLayer,
        Property::HorizontalScrollbarLayer,
        Property::VerticalScrollbarLayer,
        Property::PainterForScrollbar
    };

    auto properties = ScrollingStateNode::applicableProperties();
    properties.add(nodeProperties);
    return properties;
}

bool ScrollingStateScrollingNode::hasUnchangedGroupsAs(const ScrollingStateNode& other) const
{
    if (!ScrollingStateNode::hasUnchangedGroupsAs(other))
        return false;
    auto& otherScrolling = downcast<ScrollingStateScrollingNode>(other);
    if (m_staticLayoutData.ptr() != otherScrolling.m_staticLayoutData.ptr())
        return false;
    if (m_staticConfigData.ptr() != otherScrolling.m_staticConfigData.ptr())
        return false;
    if (m_dynamicState != otherScrolling.m_dynamicState)
        return false;
    // Direct (non-CoW) members are mutated outside the groups above, so they must be
    // compared here too, otherwise a node dirtied only through one of them is dropped
    // from the dirty-node list.
    if (m_requestedScrollData != otherScrolling.m_requestedScrollData)
        return false;
    if (m_scrollbarEnabledState != otherScrolling.m_scrollbarEnabledState)
        return false;
    if (m_scrollbarLayoutDirection != otherScrolling.m_scrollbarLayoutDirection)
        return false;
    if (m_scrollbarWidth != otherScrolling.m_scrollbarWidth)
        return false;
    if (m_useDarkAppearanceForScrollbars != otherScrolling.m_useDarkAppearanceForScrollbars)
        return false;
    return true;
}

void ScrollingStateScrollingNode::clearLayerFieldsForUnchangedProperties()
{
    // Match the pre-refactor direct-member default-empty behavior: any layer field whose
    // Property bit is NOT set on the clone must be empty in the IPC payload. Without this,
    // the in-process Coordinated Graphics scrolling-tree receiver dereferences the inherited
    // (un-translated) layer as ScrollingPlatformLayer* and asserts on the variant type.
    bool needsAccess = (!hasChangedProperty(Property::ScrollContainerLayer) && m_staticConfigData->scrollContainerLayer)
        || (!hasChangedProperty(Property::ScrolledContentsLayer) && m_staticConfigData->scrolledContentsLayer)
        || (!hasChangedProperty(Property::HorizontalScrollbarLayer) && m_staticConfigData->horizontalScrollbarLayer)
        || (!hasChangedProperty(Property::VerticalScrollbarLayer) && m_staticConfigData->verticalScrollbarLayer);
    if (!needsAccess)
        return;

    SUPPRESS_UNCOUNTED_LOCAL auto& config = m_staticConfigData.access();
    if (!hasChangedProperty(Property::ScrollContainerLayer))
        config.scrollContainerLayer = { };
    if (!hasChangedProperty(Property::ScrolledContentsLayer))
        config.scrolledContentsLayer = { };
    if (!hasChangedProperty(Property::HorizontalScrollbarLayer))
        config.horizontalScrollbarLayer = { };
    if (!hasChangedProperty(Property::VerticalScrollbarLayer))
        config.verticalScrollbarLayer = { };
}

#if ASSERT_ENABLED
void ScrollingStateScrollingNode::verifyClearedLayerFieldsForUnchangedProperties() const
{
    ASSERT(hasChangedProperty(Property::ScrollContainerLayer) || !m_staticConfigData->scrollContainerLayer);
    ASSERT(hasChangedProperty(Property::ScrolledContentsLayer) || !m_staticConfigData->scrolledContentsLayer);
    ASSERT(hasChangedProperty(Property::HorizontalScrollbarLayer) || !m_staticConfigData->horizontalScrollbarLayer);
    ASSERT(hasChangedProperty(Property::VerticalScrollbarLayer) || !m_staticConfigData->verticalScrollbarLayer);
}
#endif

void ScrollingStateScrollingNode::setScrollableAreaSize(const FloatSize& size)
{
    SET_COW_PROPERTY(m_staticLayoutData, scrollableAreaSize, size, Property::ScrollableAreaSize);
}

void ScrollingStateScrollingNode::setTotalContentsSize(const FloatSize& totalContentsSize)
{
    SET_COW_PROPERTY(m_staticLayoutData, totalContentsSize, totalContentsSize, Property::TotalContentsSize);
}

void ScrollingStateScrollingNode::setReachableContentsSize(const FloatSize& reachableContentsSize)
{
    SET_COW_PROPERTY(m_staticLayoutData, reachableContentsSize, reachableContentsSize, Property::ReachableContentsSize);
}

void ScrollingStateScrollingNode::setScrollPosition(const FloatPoint& scrollPosition)
{
    if (m_dynamicState.scrollPosition == scrollPosition)
        return;

    m_dynamicState.scrollPosition = scrollPosition;
    setPropertyChanged(Property::ScrollPosition);
}

void ScrollingStateScrollingNode::setScrollOrigin(const IntPoint& scrollOrigin)
{
    SET_COW_PROPERTY(m_staticConfigData, scrollOrigin, scrollOrigin, Property::ScrollOrigin);
}

void ScrollingStateScrollingNode::setSnapOffsetsInfo(const FloatScrollSnapOffsetsInfo& info)
{
    if (m_staticLayoutData->snapOffsetsInfo.isEqual(info))
        return;

    m_staticLayoutData.access().snapOffsetsInfo = info;
    setPropertyChanged(Property::SnapOffsetsInfo);
}

void ScrollingStateScrollingNode::setCurrentHorizontalSnapPointIndex(std::optional<unsigned> index)
{
    if (m_dynamicState.currentHorizontalSnapPointIndex == index)
        return;

    m_dynamicState.currentHorizontalSnapPointIndex = index;
    setPropertyChanged(Property::CurrentHorizontalSnapOffsetIndex);
}

void ScrollingStateScrollingNode::setCurrentVerticalSnapPointIndex(std::optional<unsigned> index)
{
    if (m_dynamicState.currentVerticalSnapPointIndex == index)
        return;

    m_dynamicState.currentVerticalSnapPointIndex = index;
    setPropertyChanged(Property::CurrentVerticalSnapOffsetIndex);
}

void ScrollingStateScrollingNode::setScrollableAreaParameters(const ScrollableAreaParameters& parameters)
{
    SET_COW_PROPERTY(m_staticConfigData, scrollableAreaParameters, parameters, Property::ScrollableAreaParams);
}

#if ENABLE(SCROLLING_THREAD)
void ScrollingStateScrollingNode::setSynchronousScrollingReasons(OptionSet<SynchronousScrollingReason> reasons)
{
    SET_COW_PROPERTY(m_staticConfigData, synchronousScrollingReasons, reasons, Property::ReasonsForSynchronousScrolling);
}
#endif


void ScrollingStateScrollingNode::setKeyboardScrollData(const RequestedKeyboardScrollData& scrollData)
{
    // One-shot command, not durable state: never de-dupe, or a repeated identical command
    // across commits is dropped and the scroll won't fire. Mirrors setRequestedScrollData.
    m_dynamicState.keyboardScrollData = scrollData;
    setPropertyChanged(Property::KeyboardScrollData);
}

void ScrollingStateScrollingNode::mergeOrAppendScrollRequest(RequestedScrollData&& scrollRequest)
{
    // This logic is based on the follow invariants:
    // * all ScrollRequestTypes other than ImplicitDeltaUpdate cancel ongoing animated scrolls
    // * a CancelAnimatedScroll followed by a ImplicitDeltaUpdate will become a DeltaUpdate
    // * thus, if a CancelAnimatedScroll is present, it will be the only item
    // * the only valid two-entry combinations have an animated update as the second entry
    // The logic needs to ensure that scrollData.identifier is always present in the new state (we must not lose the most recent identifier).

    auto replaceExisting = [](ScrollRequestData& scrollRequests, RequestedScrollData&& newRequest) {
        scrollRequests.resize(1);
        scrollRequests[0] = WTF::move(newRequest);
    };

    auto replaceAnimation = [](ScrollRequestData& scrollRequests, const RequestedScrollData& newRequest) {
        ASSERT(isAnimatedUpdate(newRequest.requestType));
        if (scrollRequests.size() == 2) {
            ASSERT(isAnimatedUpdate(scrollRequests[1].requestType));
            scrollRequests[1] = newRequest;
            return true;
        }

        return false;
    };

    auto accumulateDelta = [](ScrollRequestData& scrollRequests, RequestedScrollData&& newRequest) {
        ASSERT(scrollRequests.size() == 1);
        auto& request = scrollRequests[0];
        switch (request.requestType) {
        case ScrollRequestType::PositionUpdate:
            request.scrollPositionOrDelta = std::get<FloatPoint>(request.scrollPositionOrDelta) + std::get<FloatSize>(newRequest.scrollPositionOrDelta);
            if (request.identifier && newRequest.identifier)
                request.identifier = std::max(*request.identifier, *newRequest.identifier);
            break;
        case ScrollRequestType::DeltaUpdate:
        case ScrollRequestType::ImplicitDeltaUpdate:
            std::get<FloatSize>(request.scrollPositionOrDelta) += std::get<FloatSize>(newRequest.scrollPositionOrDelta);
            if (request.identifier && newRequest.identifier)
                request.identifier = std::max(*request.identifier, *newRequest.identifier);
            break;

        case ScrollRequestType::AnimatedPositionUpdate:
        case ScrollRequestType::AnimatedDeltaUpdate:
        case ScrollRequestType::CancelAnimatedScroll:
            ASSERT_NOT_REACHED();
            break;
        }
    };

    if (m_requestedScrollData.isEmpty()) {
        ASSERT_NOT_REACHED();
        m_requestedScrollData.append(WTF::move(scrollRequest));
        return;
    }

    switch (scrollRequest.requestType) {
    case ScrollRequestType::PositionUpdate:
        // A position update will automatically cancel any animated scroll, and overrule an existing position or delta scroll.
        replaceExisting(m_requestedScrollData, WTF::move(scrollRequest));
        break;

    case ScrollRequestType::DeltaUpdate:
    case ScrollRequestType::ImplicitDeltaUpdate:
        ASSERT_IMPLIES(m_requestedScrollData.size() > 1, isAnimatedUpdate(m_requestedScrollData[1].requestType));
        // The delta update removes any existing animation update.
        m_requestedScrollData.resize(1);

        switch (m_requestedScrollData[0].requestType) {
        case ScrollRequestType::PositionUpdate:
        case ScrollRequestType::DeltaUpdate:
            accumulateDelta(m_requestedScrollData, WTF::move(scrollRequest));
            break;
        case ScrollRequestType::ImplicitDeltaUpdate:
            m_requestedScrollData[0].requestType = ScrollRequestType::DeltaUpdate;
            accumulateDelta(m_requestedScrollData, WTF::move(scrollRequest));
            break;
        case ScrollRequestType::AnimatedPositionUpdate:
        case ScrollRequestType::AnimatedDeltaUpdate:
            m_requestedScrollData[0] = WTF::move(scrollRequest);
            break;
        case ScrollRequestType::CancelAnimatedScroll:
            m_requestedScrollData[0] = WTF::move(scrollRequest);
            if (m_requestedScrollData[0].requestType == ScrollRequestType::ImplicitDeltaUpdate)
                m_requestedScrollData[0].requestType = ScrollRequestType::DeltaUpdate;
            break;
        }
        break;

    case ScrollRequestType::AnimatedPositionUpdate:
    case ScrollRequestType::AnimatedDeltaUpdate:
        if (replaceAnimation(m_requestedScrollData, scrollRequest))
            break;

        switch (m_requestedScrollData[0].requestType) {
        case ScrollRequestType::PositionUpdate:
        case ScrollRequestType::DeltaUpdate:
            m_requestedScrollData.append(WTF::move(scrollRequest));
            break;
        case ScrollRequestType::ImplicitDeltaUpdate:
            m_requestedScrollData[0].requestType = ScrollRequestType::DeltaUpdate;
            m_requestedScrollData.append(WTF::move(scrollRequest));
            break;
        case ScrollRequestType::AnimatedPositionUpdate:
        case ScrollRequestType::AnimatedDeltaUpdate:
        case ScrollRequestType::CancelAnimatedScroll:
            m_requestedScrollData[0] = WTF::move(scrollRequest);
            break;
        }
        break;

    case ScrollRequestType::CancelAnimatedScroll:
        if (m_requestedScrollData.size() == 2) {
            ASSERT(isAnimatedUpdate(m_requestedScrollData[1].requestType));
            m_requestedScrollData.resize(1);
            m_requestedScrollData[0].identifier = scrollRequest.identifier;
            break;
        }

        switch (m_requestedScrollData[0].requestType) {
        case ScrollRequestType::PositionUpdate:
        case ScrollRequestType::DeltaUpdate:
        case ScrollRequestType::CancelAnimatedScroll:
            m_requestedScrollData[0].identifier = scrollRequest.identifier;
            break;
        case ScrollRequestType::ImplicitDeltaUpdate:
            m_requestedScrollData[0].requestType = ScrollRequestType::DeltaUpdate;
            m_requestedScrollData[0].identifier = scrollRequest.identifier;
            break;

        case ScrollRequestType::AnimatedPositionUpdate:
        case ScrollRequestType::AnimatedDeltaUpdate:
            m_requestedScrollData[0] = WTF::move(scrollRequest);
            break;
        }
        break;
    }
}

void ScrollingStateScrollingNode::setRequestedScrollData(RequestedScrollData&& scrollData)
{
    if (hasChangedProperty(Property::RequestedScrollPosition)) {
        ASSERT(m_requestedScrollData.size());
        mergeOrAppendScrollRequest(WTF::move(scrollData));
        return;
    }

    m_requestedScrollData.resize(1);
    m_requestedScrollData[0] = WTF::move(scrollData);

    setPropertyChanged(Property::RequestedScrollPosition);
}

bool ScrollingStateScrollingNode::hasScrollPositionRequest() const
{
    return hasChangedProperty(Property::RequestedScrollPosition) && m_requestedScrollData.size() && m_requestedScrollData[0].requestType != ScrollRequestType::CancelAnimatedScroll;
}

void ScrollingStateScrollingNode::setIsMonitoringWheelEvents(bool isMonitoringWheelEvents)
{
    if (isMonitoringWheelEvents == m_dynamicState.isMonitoringWheelEvents)
        return;

    m_dynamicState.isMonitoringWheelEvents = isMonitoringWheelEvents;
    setPropertyChanged(Property::IsMonitoringWheelEvents);
}

void ScrollingStateScrollingNode::setScrollContainerLayer(const LayerRepresentation& layerRepresentation)
{
    SET_COW_PROPERTY(m_staticConfigData, scrollContainerLayer, layerRepresentation, Property::ScrollContainerLayer);
}

void ScrollingStateScrollingNode::setScrolledContentsLayer(const LayerRepresentation& layerRepresentation)
{
    SET_COW_PROPERTY(m_staticConfigData, scrolledContentsLayer, layerRepresentation, Property::ScrolledContentsLayer);
}

void ScrollingStateScrollingNode::setHorizontalScrollbarLayer(const LayerRepresentation& layer)
{
    SET_COW_PROPERTY(m_staticConfigData, horizontalScrollbarLayer, layer, Property::HorizontalScrollbarLayer);
}

void ScrollingStateScrollingNode::setVerticalScrollbarLayer(const LayerRepresentation& layer)
{
    SET_COW_PROPERTY(m_staticConfigData, verticalScrollbarLayer, layer, Property::VerticalScrollbarLayer);
}

#if !PLATFORM(MAC) && !USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
void ScrollingStateScrollingNode::setScrollerImpsFromScrollbars(Scrollbar*, Scrollbar*)
{
}
#endif

void ScrollingStateScrollingNode::setMouseIsOverContentArea(bool flag)
{
    if (flag == m_dynamicState.mouseIsOverContentArea)
        return;

    m_dynamicState.mouseIsOverContentArea = flag;
    setPropertyChanged(Property::ContentAreaHoverState);
}

void ScrollingStateScrollingNode::setMouseMovedInContentArea(const MouseLocationState& mouseLocationState)
{
    if (m_dynamicState.mouseLocationState == mouseLocationState)
        return;
    m_dynamicState.mouseLocationState = mouseLocationState;
    setPropertyChanged(Property::MouseActivityState);
}

void ScrollingStateScrollingNode::setScrollbarHoverState(ScrollbarHoverState hoverState)
{
    if (hoverState == m_dynamicState.scrollbarHoverState)
        return;

    m_dynamicState.scrollbarHoverState = hoverState;
    setPropertyChanged(Property::ScrollbarHoverState);
}

void ScrollingStateScrollingNode::setScrollbarEnabledState(ScrollbarOrientation orientation, bool enabled)
{
    bool& field = (orientation == ScrollbarOrientation::Horizontal)
        ? m_scrollbarEnabledState.horizontalScrollbarIsEnabled
        : m_scrollbarEnabledState.verticalScrollbarIsEnabled;
    if (field == enabled)
        return;
    field = enabled;
    setPropertyChanged(Property::ScrollbarEnabledState);
}

void ScrollingStateScrollingNode::setScrollbarColor(std::optional<ScrollbarColor> state)
{
    SET_COW_PROPERTY(m_staticConfigData, scrollbarColor, state, Property::ScrollbarColor);
}

void ScrollingStateScrollingNode::setScrollbarLayoutDirection(UserInterfaceLayoutDirection scrollbarLayoutDirection)
{
    if (scrollbarLayoutDirection == m_scrollbarLayoutDirection)
        return;
    m_scrollbarLayoutDirection = scrollbarLayoutDirection;
    setPropertyChanged(Property::ScrollbarLayoutDirection);
}

void ScrollingStateScrollingNode::setScrollbarWidth(ScrollbarWidth scrollbarWidth)
{
    if (scrollbarWidth == m_scrollbarWidth)
        return;
    m_scrollbarWidth = scrollbarWidth;
    setPropertyChanged(Property::ScrollbarWidth);
}

void ScrollingStateScrollingNode::setUseDarkAppearanceForScrollbars(bool useDarkAppearanceForScrollbars)
{
    if (useDarkAppearanceForScrollbars == m_useDarkAppearanceForScrollbars)
        return;
    m_useDarkAppearanceForScrollbars = useDarkAppearanceForScrollbars;
    setPropertyChanged(Property::UseDarkAppearanceForScrollbars);
}

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
void ScrollingStateScrollingNode::setScrollbarOpacity(float scrollbarOpacity)
{
    if (scrollbarOpacity == m_scrollbarOpacity)
        return;
    m_scrollbarOpacity = scrollbarOpacity;
    setPropertyChanged(Property::ScrollbarOpacity);
}
#endif

void ScrollingStateScrollingNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ScrollingStateNode::dumpProperties(ts, behavior);

    SUPPRESS_UNCOUNTED_LOCAL auto& layout = m_staticLayoutData.get();
    SUPPRESS_UNCOUNTED_LOCAL auto& config = m_staticConfigData.get();

    if (!m_dynamicState.scrollPosition.isZero()) {
        TextStream::GroupScope scope(ts);
        ts << "scroll position "_s
            << TextStream::FormatNumberRespectingIntegers(m_dynamicState.scrollPosition.x()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_dynamicState.scrollPosition.y());
    }

    if (!layout.scrollableAreaSize.isEmpty()) {
        TextStream::GroupScope scope(ts);
        ts << "scrollable area size "_s
            << TextStream::FormatNumberRespectingIntegers(layout.scrollableAreaSize.width()) << " "
            << TextStream::FormatNumberRespectingIntegers(layout.scrollableAreaSize.height());
    }

    if (!layout.totalContentsSize.isEmpty()) {
        TextStream::GroupScope scope(ts);
        ts << "contents size "_s
            << TextStream::FormatNumberRespectingIntegers(layout.totalContentsSize.width()) << " "
            << TextStream::FormatNumberRespectingIntegers(layout.totalContentsSize.height());
    }

    if (layout.reachableContentsSize != layout.totalContentsSize)
        ts.dumpProperty("reachable contents size"_s, layout.reachableContentsSize);

    auto dumpRequest = [&](const RequestedScrollData& request) {
        if (request.requestType == ScrollRequestType::PositionUpdate || request.requestType == ScrollRequestType::AnimatedPositionUpdate) {
            auto scrollPosition = std::get<FloatPoint>(request.scrollPositionOrDelta);
            if (!scrollPosition.isZero()) {
                TextStream::GroupScope scope(ts);
                ts << "requested scroll position "_s
                << TextStream::FormatNumberRespectingIntegers(scrollPosition.x()) << " "
                << TextStream::FormatNumberRespectingIntegers(scrollPosition.y());
            }
        } else if (request.requestType == ScrollRequestType::DeltaUpdate || request.requestType == ScrollRequestType::AnimatedDeltaUpdate || request.requestType == ScrollRequestType::ImplicitDeltaUpdate) {
            auto scrollDelta = std::get<FloatSize>(request.scrollPositionOrDelta);
            if (!scrollDelta.isZero()) {
                TextStream::GroupScope scope(ts);
                if (request.requestType == ScrollRequestType::DeltaUpdate || request.requestType == ScrollRequestType::AnimatedDeltaUpdate)
                    ts << "requested scroll delta "_s;
                else
                    ts << "requested scroll implicit delta "_s;
                ts << TextStream::FormatNumberRespectingIntegers(scrollDelta.width()) << " "
                    << TextStream::FormatNumberRespectingIntegers(scrollDelta.height());
            }
        }

        if (request.scrollType == ScrollType::Programmatic)
            ts.dumpProperty("requested scroll position represents programmatic scroll"_s, true);

        if (request.clamping == ScrollClamping::Unclamped)
            ts.dumpProperty("requested scroll position clamping"_s, request.clamping);

        if (isAnimatedUpdate(request.requestType))
            ts.dumpProperty("requested scroll position is animated"_s, true);
    };

    for (auto& request : m_requestedScrollData)
        dumpRequest(request);

    if (!config.scrollOrigin.isZero())
        ts.dumpProperty("scroll origin"_s, config.scrollOrigin);

    if (layout.snapOffsetsInfo.horizontalSnapOffsets.size())
        ts.dumpProperty("horizontal snap offsets"_s, layout.snapOffsetsInfo.horizontalSnapOffsets);

    if (layout.snapOffsetsInfo.verticalSnapOffsets.size())
        ts.dumpProperty("vertical snap offsets"_s, layout.snapOffsetsInfo.verticalSnapOffsets);

    if (m_dynamicState.currentHorizontalSnapPointIndex)
        ts.dumpProperty("current horizontal snap point index"_s, m_dynamicState.currentHorizontalSnapPointIndex);

    if (m_dynamicState.currentVerticalSnapPointIndex)
        ts.dumpProperty("current vertical snap point index"_s, m_dynamicState.currentVerticalSnapPointIndex);

    ts.dumpProperty("scrollable area parameters"_s, config.scrollableAreaParameters);

#if ENABLE(SCROLLING_THREAD)
    if (!config.synchronousScrollingReasons.isEmpty())
        ts.dumpProperty("Scrolling on main thread because:"_s, ScrollingCoordinator::synchronousScrollingReasonsAsText(config.synchronousScrollingReasons));
#endif

    if (m_useDarkAppearanceForScrollbars)
        ts.dumpProperty("uses dark appearance for scrollbars"_s, m_useDarkAppearanceForScrollbars);

    if (m_dynamicState.isMonitoringWheelEvents)
        ts.dumpProperty("expects wheel event test trigger"_s, m_dynamicState.isMonitoringWheelEvents);

    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeLayerIDs) {
        if (config.scrollContainerLayer.layerID())
            ts.dumpProperty("scroll container layer"_s, config.scrollContainerLayer.layerID());
        if (config.scrolledContentsLayer.layerID())
            ts.dumpProperty("scrolled contents layer"_s, config.scrolledContentsLayer.layerID());
    }
}

} // namespace WebCore

#undef SET_COW_PROPERTY

#endif // ENABLE(ASYNC_SCROLLING)
