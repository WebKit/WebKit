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

#pragma once

#include <wtf/Platform.h>
#if ENABLE(ASYNC_SCROLLING)

#include <WebCore/ScrollSnapOffsetsInfo.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollingCoordinator.h>
#include <WebCore/ScrollingStateNode.h>
#include <wtf/Forward.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSScrollerImp;
#endif

namespace WebCore {

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
class ScrollerImpAdwaita;
#endif

struct ScrollbarHoverState {
#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    ScrollbarPart hoveredPartInHorizontalScrollbar { NoPart };
    ScrollbarPart hoveredPartInVerticalScrollbar { NoPart };
    ScrollbarPart pressedPartInHorizontalScrollbar { NoPart };
    ScrollbarPart pressedPartInVerticalScrollbar { NoPart };
#else
    bool mouseIsOverHorizontalScrollbar { false };
    bool mouseIsOverVerticalScrollbar { false };
#endif

    friend bool operator==(const ScrollbarHoverState&, const ScrollbarHoverState&) = default;
};

struct MouseLocationState {
    IntPoint locationInHorizontalScrollbar;
    IntPoint locationInVerticalScrollbar;

    friend bool operator==(const MouseLocationState&, const MouseLocationState&) = default;
};

struct ScrollbarEnabledState {
    bool horizontalScrollbarIsEnabled { false };
    bool verticalScrollbarIsEnabled { false };

    friend bool operator==(const ScrollbarEnabledState&, const ScrollbarEnabledState&) = default;
};

class ScrollingStateScrollingNode : public ScrollingStateNode {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ScrollingStateScrollingNode, WEBCORE_EXPORT);
public:
    virtual ~ScrollingStateScrollingNode();

    const FloatSize& scrollableAreaSize() const LIFETIME_BOUND { return m_staticLayoutData->scrollableAreaSize; }
    WEBCORE_EXPORT void setScrollableAreaSize(const FloatSize&);

    const FloatSize& totalContentsSize() const LIFETIME_BOUND { return m_staticLayoutData->totalContentsSize; }
    WEBCORE_EXPORT void setTotalContentsSize(const FloatSize&);

    const FloatSize& reachableContentsSize() const LIFETIME_BOUND { return m_staticLayoutData->reachableContentsSize; }
    WEBCORE_EXPORT void setReachableContentsSize(const FloatSize&);

    const FloatPoint& scrollPosition() const LIFETIME_BOUND { return m_dynamicState.scrollPosition; }
    WEBCORE_EXPORT void setScrollPosition(const FloatPoint&);

    const IntPoint& scrollOrigin() const LIFETIME_BOUND { return m_staticConfigData->scrollOrigin; }
    WEBCORE_EXPORT void setScrollOrigin(const IntPoint&);

    const FloatScrollSnapOffsetsInfo& snapOffsetsInfo() const LIFETIME_BOUND { return m_staticLayoutData->snapOffsetsInfo; }
    WEBCORE_EXPORT void setSnapOffsetsInfo(const FloatScrollSnapOffsetsInfo& newOffsetsInfo);

    std::optional<unsigned> currentHorizontalSnapPointIndex() const { return m_dynamicState.currentHorizontalSnapPointIndex; }
    WEBCORE_EXPORT void setCurrentHorizontalSnapPointIndex(std::optional<unsigned>);

    std::optional<unsigned> currentVerticalSnapPointIndex() const { return m_dynamicState.currentVerticalSnapPointIndex; }
    WEBCORE_EXPORT void setCurrentVerticalSnapPointIndex(std::optional<unsigned>);

    const ScrollableAreaParameters& scrollableAreaParameters() const LIFETIME_BOUND { return m_staticConfigData->scrollableAreaParameters; }
    WEBCORE_EXPORT void setScrollableAreaParameters(const ScrollableAreaParameters& params);

#if ENABLE(SCROLLING_THREAD)
    OptionSet<SynchronousScrollingReason> synchronousScrollingReasons() const { return m_staticConfigData->synchronousScrollingReasons; }
    WEBCORE_EXPORT void setSynchronousScrollingReasons(OptionSet<SynchronousScrollingReason>);
    bool hasSynchronousScrollingReasons() const { return !m_staticConfigData->synchronousScrollingReasons.isEmpty(); }
#endif

    const RequestedKeyboardScrollData& keyboardScrollData() const LIFETIME_BOUND { return m_dynamicState.keyboardScrollData; }
    WEBCORE_EXPORT void setKeyboardScrollData(const RequestedKeyboardScrollData&);

    const ScrollRequestData& requestedScrollData() const LIFETIME_BOUND { return m_requestedScrollData; }

    WEBCORE_EXPORT void setRequestedScrollData(RequestedScrollData&&);

    WEBCORE_EXPORT bool NODELETE hasScrollPositionRequest() const;

    bool isMonitoringWheelEvents() const { return m_dynamicState.isMonitoringWheelEvents; }
    WEBCORE_EXPORT void setIsMonitoringWheelEvents(bool);

    const LayerRepresentation& scrollContainerLayer() const LIFETIME_BOUND { return m_staticConfigData->scrollContainerLayer; }
    WEBCORE_EXPORT void setScrollContainerLayer(const LayerRepresentation&);

    // This is a layer with the contents that move.
    const LayerRepresentation& scrolledContentsLayer() const LIFETIME_BOUND { return m_staticConfigData->scrolledContentsLayer; }
    WEBCORE_EXPORT void setScrolledContentsLayer(const LayerRepresentation&);

    const LayerRepresentation& horizontalScrollbarLayer() const LIFETIME_BOUND { return m_staticConfigData->horizontalScrollbarLayer; }
    WEBCORE_EXPORT void setHorizontalScrollbarLayer(const LayerRepresentation&);

    const LayerRepresentation& verticalScrollbarLayer() const LIFETIME_BOUND { return m_staticConfigData->verticalScrollbarLayer; }
    WEBCORE_EXPORT void setVerticalScrollbarLayer(const LayerRepresentation&);

#if PLATFORM(MAC)
    NSScrollerImp *verticalScrollerImp() const LIFETIME_BOUND { return m_verticalScrollerImp.get(); }
    NSScrollerImp *horizontalScrollerImp() const LIFETIME_BOUND { return m_horizontalScrollerImp.get(); }
#elif USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    ScrollerImpAdwaita* verticalScrollerImp() const { return m_verticalScrollerImp; }
    ScrollerImpAdwaita* horizontalScrollerImp() const { return m_horizontalScrollerImp; }
#endif
    ScrollbarHoverState scrollbarHoverState() const { return m_dynamicState.scrollbarHoverState; }
    WEBCORE_EXPORT void setScrollbarHoverState(ScrollbarHoverState);

    ScrollbarEnabledState scrollbarEnabledState() const { return m_scrollbarEnabledState; }
    WEBCORE_EXPORT void setScrollbarEnabledState(ScrollbarOrientation, bool);

    const std::optional<ScrollbarColor>& scrollbarColor() const LIFETIME_BOUND { return m_staticConfigData->scrollbarColor; }
    WEBCORE_EXPORT void setScrollbarColor(std::optional<ScrollbarColor>);

    void setScrollerImpsFromScrollbars(Scrollbar* verticalScrollbar, Scrollbar* horizontalScrollbar);

    WEBCORE_EXPORT void setMouseIsOverContentArea(bool);
    bool mouseIsOverContentArea() const { return m_dynamicState.mouseIsOverContentArea; }

    WEBCORE_EXPORT void setMouseMovedInContentArea(const MouseLocationState&);
    const MouseLocationState& mouseLocationState() const LIFETIME_BOUND { return m_dynamicState.mouseLocationState; }

    WEBCORE_EXPORT void setScrollbarLayoutDirection(UserInterfaceLayoutDirection);
    UserInterfaceLayoutDirection scrollbarLayoutDirection() const { return m_scrollbarLayoutDirection; }

    WEBCORE_EXPORT void setScrollbarWidth(ScrollbarWidth);
    ScrollbarWidth scrollbarWidth() const { return m_scrollbarWidth; }

    WEBCORE_EXPORT void setUseDarkAppearanceForScrollbars(bool);
    bool useDarkAppearanceForScrollbars() const { return m_useDarkAppearanceForScrollbars; }

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    void setScrollbarOpacity(float);
    float scrollbarOpacity() const { return m_scrollbarOpacity; };
#endif

protected:
    ScrollingStateScrollingNode(
        ScrollingNodeType,
        ScrollingNodeID,
        Vector<Ref<ScrollingStateNode>>&&,
        OptionSet<ScrollingStateNodeProperty>,
        std::optional<PlatformLayerIdentifier>,
        FloatSize scrollableAreaSize,
        FloatSize totalContentsSize,
        FloatSize reachableContentsSize,
        FloatPoint scrollPosition,
        IntPoint scrollOrigin,
        ScrollableAreaParameters&&,
#if ENABLE(SCROLLING_THREAD)
        OptionSet<SynchronousScrollingReason>,
#endif
        ScrollRequestData&&,
        FloatScrollSnapOffsetsInfo&&,
        std::optional<unsigned> currentHorizontalSnapPointIndex,
        std::optional<unsigned> currentVerticalSnapPointIndex,
        bool isMonitoringWheelEvents,
        std::optional<PlatformLayerIdentifier> scrollContainerLayer,
        std::optional<PlatformLayerIdentifier> scrolledContentsLayer,
        std::optional<PlatformLayerIdentifier> horizontalScrollbarLayer,
        std::optional<PlatformLayerIdentifier> verticalScrollbarLayer,
        bool mouseIsOverContentArea,
        MouseLocationState&&,
        ScrollbarHoverState&&,
        ScrollbarEnabledState&&,
        std::optional<ScrollbarColor>&&,
        UserInterfaceLayoutDirection,
        ScrollbarWidth,
        bool useDarkAppearanceForScrollbars,
        RequestedKeyboardScrollData&&
    );
    ScrollingStateScrollingNode(ScrollingStateTree&, ScrollingNodeType, ScrollingNodeID);
    ScrollingStateScrollingNode(const ScrollingStateScrollingNode&, ScrollingStateTree&);

    OptionSet<Property> applicableProperties() const override;
    void dumpProperties(WTF::TextStream&, OptionSet<ScrollingStateTreeAsTextBehavior>) const override;
    bool hasUnchangedGroupsAs(const ScrollingStateNode&) const override;
    void clearLayerFieldsForUnchangedProperties() override;
#if ASSERT_ENABLED
    void verifyClearedLayerFieldsForUnchangedProperties() const override;
#endif

    // Per-frame mutable scrolling state. Every commit is expected to differ in at least one field.
    struct DynamicScrollState {
        FloatPoint scrollPosition;
        std::optional<unsigned> currentHorizontalSnapPointIndex;
        std::optional<unsigned> currentVerticalSnapPointIndex;
        ScrollbarHoverState scrollbarHoverState;
        MouseLocationState mouseLocationState;
        RequestedKeyboardScrollData keyboardScrollData;
        bool isMonitoringWheelEvents { false };
        bool mouseIsOverContentArea { false };

        friend bool operator==(const DynamicScrollState&, const DynamicScrollState&) = default;
    };

    // Layout-derived scrolling state. Recomputed on layout, stable between scrolls.
    class StaticLayoutScrollState : public ThreadSafeRefCounted<StaticLayoutScrollState> {
    public:
        static Ref<StaticLayoutScrollState> create() { return adoptRef(*new StaticLayoutScrollState); }
        Ref<StaticLayoutScrollState> copy() const { return adoptRef(*new StaticLayoutScrollState(*this)); }

        FloatSize scrollableAreaSize;
        FloatSize totalContentsSize;
        FloatSize reachableContentsSize;
        FloatScrollSnapOffsetsInfo snapOffsetsInfo;

    private:
        StaticLayoutScrollState() = default;
        StaticLayoutScrollState(const StaticLayoutScrollState& other)
            : ThreadSafeRefCounted<StaticLayoutScrollState>()
            , scrollableAreaSize(other.scrollableAreaSize)
            , totalContentsSize(other.totalContentsSize)
            , reachableContentsSize(other.reachableContentsSize)
            , snapOffsetsInfo(other.snapOffsetsInfo)
        {
        }
    };

    // Long-lived scrolling configuration. Rarely changes.
    class StaticConfigurationScrollState : public ThreadSafeRefCounted<StaticConfigurationScrollState> {
    public:
        static Ref<StaticConfigurationScrollState> create() { return adoptRef(*new StaticConfigurationScrollState); }
        Ref<StaticConfigurationScrollState> copy() const { return adoptRef(*new StaticConfigurationScrollState(*this)); }

        IntPoint scrollOrigin;
        ScrollableAreaParameters scrollableAreaParameters;
        std::optional<ScrollbarColor> scrollbarColor;
        LayerRepresentation scrollContainerLayer;
        LayerRepresentation scrolledContentsLayer;
        LayerRepresentation horizontalScrollbarLayer;
        LayerRepresentation verticalScrollbarLayer;
#if ENABLE(SCROLLING_THREAD)
        OptionSet<SynchronousScrollingReason> synchronousScrollingReasons;
#endif

    private:
        StaticConfigurationScrollState() = default;
        StaticConfigurationScrollState(const StaticConfigurationScrollState& other)
            : ThreadSafeRefCounted<StaticConfigurationScrollState>()
            , scrollOrigin(other.scrollOrigin)
            , scrollableAreaParameters(other.scrollableAreaParameters)
            , scrollbarColor(other.scrollbarColor)
            , scrollContainerLayer(other.scrollContainerLayer)
            , scrolledContentsLayer(other.scrolledContentsLayer)
            , horizontalScrollbarLayer(other.horizontalScrollbarLayer)
            , verticalScrollbarLayer(other.verticalScrollbarLayer)
#if ENABLE(SCROLLING_THREAD)
            , synchronousScrollingReasons(other.synchronousScrollingReasons)
#endif
        {
        }
    };

    DataRef<StaticLayoutScrollState> m_staticLayoutData { StaticLayoutScrollState::create() };
    DataRef<StaticConfigurationScrollState> m_staticConfigData { StaticConfigurationScrollState::create() };
    DynamicScrollState m_dynamicState;

private:
    void mergeOrAppendScrollRequest(RequestedScrollData&&);

    ScrollRequestData m_requestedScrollData;
    ScrollbarEnabledState m_scrollbarEnabledState;
    UserInterfaceLayoutDirection m_scrollbarLayoutDirection { UserInterfaceLayoutDirection::LTR };
    ScrollbarWidth m_scrollbarWidth { ScrollbarWidth::Auto };
    bool m_useDarkAppearanceForScrollbars { false };

#if PLATFORM(MAC)
    RetainPtr<NSScrollerImp> m_verticalScrollerImp;
    RetainPtr<NSScrollerImp> m_horizontalScrollerImp;
#elif USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    RefPtr<ScrollerImpAdwaita> m_verticalScrollerImp;
    RefPtr<ScrollerImpAdwaita> m_horizontalScrollerImp;
#endif

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    float m_scrollbarOpacity { 1 };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_STATE_NODE(ScrollingStateScrollingNode, isScrollingNode())

#endif // ENABLE(ASYNC_SCROLLING)
