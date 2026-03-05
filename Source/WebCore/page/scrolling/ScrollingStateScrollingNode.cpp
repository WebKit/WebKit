/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
#include "ScrollerImpAdwaita.h"
#endif

namespace WebCore {

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
    , m_scrollableAreaSize(scrollableAreaSize)
    , m_totalContentsSize(totalContentsSize)
    , m_reachableContentsSize(reachableContentsSize)
    , m_scrollPosition(scrollPosition)
    , m_scrollOrigin(scrollOrigin)
    , m_snapOffsetsInfo(WTF::move(snapOffsetsInfo))
    , m_currentHorizontalSnapPointIndex(currentHorizontalSnapPointIndex)
    , m_currentVerticalSnapPointIndex(currentVerticalSnapPointIndex)
    , m_scrollContainerLayer(scrollContainerLayer)
    , m_scrolledContentsLayer(scrolledContentsLayer)
    , m_horizontalScrollbarLayer(horizontalScrollbarLayer)
    , m_verticalScrollbarLayer(verticalScrollbarLayer)
    , m_scrollbarHoverState(WTF::move(scrollbarHoverState))
    , m_mouseLocationState(WTF::move(mouseLocationState))
    , m_scrollbarEnabledState(WTF::move(scrollbarEnabledState))
    , m_scrollbarColor(WTF::move(scrollbarColor))
    , m_scrollableAreaParameters(WTF::move(scrollableAreaParameters))
    , m_requestedScrollData(WTF::move(requestedScrollData))
    , m_keyboardScrollData(WTF::move(keyboardScrollData))
#if ENABLE(SCROLLING_THREAD)
    , m_synchronousScrollingReasons(synchronousScrollingReasons)
#endif
    , m_scrollbarLayoutDirection(scrollbarLayoutDirection)
    , m_scrollbarWidth(scrollbarWidth)
    , m_useDarkAppearanceForScrollbars(useDarkAppearanceForScrollbars)
    , m_isMonitoringWheelEvents(isMonitoringWheelEvents)
    , m_mouseIsOverContentArea(mouseIsOverContentArea)
{
    // scrollingNodeAdded will be called in attachAfterDeserialization.
}

ScrollingStateScrollingNode::ScrollingStateScrollingNode(const ScrollingStateScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(stateNode, adoptiveTree)
    , m_scrollableAreaSize(stateNode.scrollableAreaSize())
    , m_totalContentsSize(stateNode.totalContentsSize())
    , m_reachableContentsSize(stateNode.reachableContentsSize())
    , m_scrollPosition(stateNode.scrollPosition())
    , m_scrollOrigin(stateNode.scrollOrigin())
    , m_snapOffsetsInfo(stateNode.m_snapOffsetsInfo)
#if PLATFORM(MAC) || USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    , m_scrollbarHoverState(stateNode.scrollbarHoverState())
#endif
#if PLATFORM(MAC)
    , m_mouseLocationState(stateNode.mouseLocationState())
#endif
#if PLATFORM(MAC) || USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    , m_scrollbarEnabledState(stateNode.scrollbarEnabledState())
    , m_verticalScrollerImp(stateNode.verticalScrollerImp())
    , m_horizontalScrollerImp(stateNode.horizontalScrollerImp())
#endif
#if PLATFORM(COCOA)
    , m_scrollbarColor(stateNode.scrollbarColor())
#endif
    , m_scrollableAreaParameters(stateNode.scrollableAreaParameters())
    , m_requestedScrollData(stateNode.requestedScrollData())
    , m_keyboardScrollData(stateNode.keyboardScrollData())
#if ENABLE(SCROLLING_THREAD)
    , m_synchronousScrollingReasons(stateNode.synchronousScrollingReasons())
#endif
    , m_scrollbarLayoutDirection(stateNode.scrollbarLayoutDirection())
    , m_scrollbarWidth(stateNode.scrollbarWidth())
    , m_useDarkAppearanceForScrollbars(stateNode.useDarkAppearanceForScrollbars())
    , m_isMonitoringWheelEvents(stateNode.isMonitoringWheelEvents())
    , m_mouseIsOverContentArea(stateNode.mouseIsOverContentArea())
#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    , m_scrollbarOpacity(stateNode.scrollbarOpacity())
#endif
{
    scrollingStateTree().scrollingNodeAdded();

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

void ScrollingStateScrollingNode::setScrollableAreaSize(const FloatSize& size)
{
    if (m_scrollableAreaSize == size)
        return;

    m_scrollableAreaSize = size;
    setPropertyChanged(Property::ScrollableAreaSize);
}

void ScrollingStateScrollingNode::setTotalContentsSize(const FloatSize& totalContentsSize)
{
    if (m_totalContentsSize == totalContentsSize)
        return;

    m_totalContentsSize = totalContentsSize;
    setPropertyChanged(Property::TotalContentsSize);
}

void ScrollingStateScrollingNode::setReachableContentsSize(const FloatSize& reachableContentsSize)
{
    if (m_reachableContentsSize == reachableContentsSize)
        return;

    m_reachableContentsSize = reachableContentsSize;
    setPropertyChanged(Property::ReachableContentsSize);
}

void ScrollingStateScrollingNode::setScrollPosition(const FloatPoint& scrollPosition)
{
    if (m_scrollPosition == scrollPosition)
        return;

    m_scrollPosition = scrollPosition;
    setPropertyChanged(Property::ScrollPosition);
}

void ScrollingStateScrollingNode::setScrollOrigin(const IntPoint& scrollOrigin)
{
    if (m_scrollOrigin == scrollOrigin)
        return;

    m_scrollOrigin = scrollOrigin;
    setPropertyChanged(Property::ScrollOrigin);
}

void ScrollingStateScrollingNode::setSnapOffsetsInfo(const FloatScrollSnapOffsetsInfo& info)
{
    if (m_snapOffsetsInfo.isEqual(info))
        return;

    m_snapOffsetsInfo = info;
    setPropertyChanged(Property::SnapOffsetsInfo);
}

void ScrollingStateScrollingNode::setCurrentHorizontalSnapPointIndex(std::optional<unsigned> index)
{
    if (m_currentHorizontalSnapPointIndex == index)
        return;
    
    m_currentHorizontalSnapPointIndex = index;
    setPropertyChanged(Property::CurrentHorizontalSnapOffsetIndex);
}

void ScrollingStateScrollingNode::setCurrentVerticalSnapPointIndex(std::optional<unsigned> index)
{
    if (m_currentVerticalSnapPointIndex == index)
        return;
    
    m_currentVerticalSnapPointIndex = index;
    setPropertyChanged(Property::CurrentVerticalSnapOffsetIndex);
}

void ScrollingStateScrollingNode::setScrollableAreaParameters(const ScrollableAreaParameters& parameters)
{
    if (m_scrollableAreaParameters == parameters)
        return;

    m_scrollableAreaParameters = parameters;
    setPropertyChanged(Property::ScrollableAreaParams);
}

#if ENABLE(SCROLLING_THREAD)
void ScrollingStateScrollingNode::setSynchronousScrollingReasons(OptionSet<SynchronousScrollingReason> reasons)
{
    if (m_synchronousScrollingReasons == reasons)
        return;

    m_synchronousScrollingReasons = reasons;
    setPropertyChanged(Property::ReasonsForSynchronousScrolling);
}
#endif


void ScrollingStateScrollingNode::setKeyboardScrollData(const RequestedKeyboardScrollData& scrollData)
{
    m_keyboardScrollData = scrollData;
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
    if (isMonitoringWheelEvents == m_isMonitoringWheelEvents)
        return;

    m_isMonitoringWheelEvents = isMonitoringWheelEvents;
    setPropertyChanged(Property::IsMonitoringWheelEvents);
}

void ScrollingStateScrollingNode::setScrollContainerLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_scrollContainerLayer)
        return;

    m_scrollContainerLayer = layerRepresentation;
    setPropertyChanged(Property::ScrollContainerLayer);
}

void ScrollingStateScrollingNode::setScrolledContentsLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_scrolledContentsLayer)
        return;

    m_scrolledContentsLayer = layerRepresentation;
    setPropertyChanged(Property::ScrolledContentsLayer);
}

void ScrollingStateScrollingNode::setHorizontalScrollbarLayer(const LayerRepresentation& layer)
{
    if (layer == m_horizontalScrollbarLayer)
        return;

    m_horizontalScrollbarLayer = layer;
    setPropertyChanged(Property::HorizontalScrollbarLayer);
}

void ScrollingStateScrollingNode::setVerticalScrollbarLayer(const LayerRepresentation& layer)
{
    if (layer == m_verticalScrollbarLayer)
        return;
    m_verticalScrollbarLayer = layer;
    setPropertyChanged(Property::VerticalScrollbarLayer);
}

#if !PLATFORM(MAC) && !USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
void ScrollingStateScrollingNode::setScrollerImpsFromScrollbars(Scrollbar*, Scrollbar*)
{
}
#endif

void ScrollingStateScrollingNode::setMouseIsOverContentArea(bool flag)
{
    if (flag == m_mouseIsOverContentArea)
        return;

    m_mouseIsOverContentArea = flag;
    setPropertyChanged(Property::ContentAreaHoverState);
}

void ScrollingStateScrollingNode::setMouseMovedInContentArea(const MouseLocationState& mouseLocationState)
{
    m_mouseLocationState = mouseLocationState;
    setPropertyChanged(Property::MouseActivityState);
}
    
void ScrollingStateScrollingNode::setScrollbarHoverState(ScrollbarHoverState hoverState)
{
    if (hoverState == m_scrollbarHoverState)
        return;

    m_scrollbarHoverState = hoverState;
    setPropertyChanged(Property::ScrollbarHoverState);
}

void ScrollingStateScrollingNode::setScrollbarEnabledState(ScrollbarOrientation orientation, bool enabled)
{
    if ((orientation == ScrollbarOrientation::Horizontal ? m_scrollbarEnabledState.horizontalScrollbarIsEnabled : m_scrollbarEnabledState.verticalScrollbarIsEnabled) == enabled)
        return;

    if (orientation == ScrollbarOrientation::Horizontal)
        m_scrollbarEnabledState.horizontalScrollbarIsEnabled = enabled;
    else
        m_scrollbarEnabledState.verticalScrollbarIsEnabled = enabled;

    setPropertyChanged(Property::ScrollbarEnabledState);
}

void ScrollingStateScrollingNode::setScrollbarColor(std::optional<ScrollbarColor> state)
{
    if (state == m_scrollbarColor)
        return;

    m_scrollbarColor = state;
    setPropertyChanged(Property::ScrollbarColor);
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
    
    if (!m_scrollPosition.isZero()) {
        TextStream::GroupScope scope(ts);
        ts << "scroll position "_s
            << TextStream::FormatNumberRespectingIntegers(m_scrollPosition.x()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_scrollPosition.y());
    }

    if (!m_scrollableAreaSize.isEmpty()) {
        TextStream::GroupScope scope(ts);
        ts << "scrollable area size "_s
            << TextStream::FormatNumberRespectingIntegers(m_scrollableAreaSize.width()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_scrollableAreaSize.height());
    }

    if (!m_totalContentsSize.isEmpty()) {
        TextStream::GroupScope scope(ts);
        ts << "contents size "_s
            << TextStream::FormatNumberRespectingIntegers(m_totalContentsSize.width()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_totalContentsSize.height());
    }

    if (m_reachableContentsSize != m_totalContentsSize)
        ts.dumpProperty("reachable contents size"_s, m_reachableContentsSize);

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

    if (!m_scrollOrigin.isZero())
        ts.dumpProperty("scroll origin"_s, m_scrollOrigin);

    if (m_snapOffsetsInfo.horizontalSnapOffsets.size())
        ts.dumpProperty("horizontal snap offsets"_s, m_snapOffsetsInfo.horizontalSnapOffsets);

    if (m_snapOffsetsInfo.verticalSnapOffsets.size())
        ts.dumpProperty("vertical snap offsets"_s, m_snapOffsetsInfo.verticalSnapOffsets);

    if (m_currentHorizontalSnapPointIndex)
        ts.dumpProperty("current horizontal snap point index"_s, m_currentHorizontalSnapPointIndex);

    if (m_currentVerticalSnapPointIndex)
        ts.dumpProperty("current vertical snap point index"_s, m_currentVerticalSnapPointIndex);

    ts.dumpProperty("scrollable area parameters"_s, m_scrollableAreaParameters);

#if ENABLE(SCROLLING_THREAD)
    if (!m_synchronousScrollingReasons.isEmpty())
        ts.dumpProperty("Scrolling on main thread because:"_s, ScrollingCoordinator::synchronousScrollingReasonsAsText(m_synchronousScrollingReasons));
#endif

    if (m_isMonitoringWheelEvents)
        ts.dumpProperty("expects wheel event test trigger"_s, m_isMonitoringWheelEvents);

    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeLayerIDs) {
        if (m_scrollContainerLayer.layerID())
            ts.dumpProperty("scroll container layer"_s, m_scrollContainerLayer.layerID());
        if (m_scrolledContentsLayer.layerID())
            ts.dumpProperty("scrolled contents layer"_s, m_scrolledContentsLayer.layerID());
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
