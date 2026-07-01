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

#pragma once

#if ENABLE(ASYNC_SCROLLING)

#include <WebCore/BoxExtents.h>
#include <WebCore/EventTrackingRegions.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollbarThemeComposite.h>
#include <WebCore/ScrollingCoordinator.h>
#include <WebCore/ScrollingStateScrollingNode.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

class Scrollbar;

class ScrollingStateFrameScrollingNode final : public ScrollingStateScrollingNode {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ScrollingStateFrameScrollingNode, WEBCORE_EXPORT);
public:
    template<typename... Args> static Ref<ScrollingStateFrameScrollingNode> create(Args&&... args) { return adoptRef(*new ScrollingStateFrameScrollingNode(std::forward<Args>(args)...)); }

    Ref<ScrollingStateNode> clone(ScrollingStateTree&) override;

    virtual ~ScrollingStateFrameScrollingNode();

    float frameScaleFactor() const { return m_dynamicState.frameScaleFactor; }
    WEBCORE_EXPORT void setFrameScaleFactor(float);

    const EventTrackingRegions& eventTrackingRegions() const LIFETIME_BOUND { return m_staticLayoutData->eventTrackingRegions; }
    WEBCORE_EXPORT void setEventTrackingRegions(const EventTrackingRegions&);

    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const { return m_behaviorForFixed; }
    WEBCORE_EXPORT void setScrollBehaviorForFixedElements(ScrollBehaviorForFixedElements);

    FloatRect layoutViewport() const { return m_staticLayoutData->layoutViewport; };
    WEBCORE_EXPORT void setLayoutViewport(const FloatRect&);

    FloatSize sizeForVisibleContent() const { return m_staticLayoutData->sizeForVisibleContent; }
    WEBCORE_EXPORT void setSizeForVisibleContent(const FloatSize&);

    FloatPoint minLayoutViewportOrigin() const { return m_staticLayoutData->minLayoutViewportOrigin; }
    WEBCORE_EXPORT void setMinLayoutViewportOrigin(const FloatPoint&);

    FloatPoint maxLayoutViewportOrigin() const { return m_staticLayoutData->maxLayoutViewportOrigin; }
    WEBCORE_EXPORT void setMaxLayoutViewportOrigin(const FloatPoint&);

    std::optional<FloatSize> overrideVisualViewportSize() const { return m_staticConfigData->overrideVisualViewportSize; };
    WEBCORE_EXPORT void setOverrideVisualViewportSize(std::optional<FloatSize>);

    int headerHeight() const { return m_headerHeight; }
    WEBCORE_EXPORT void setHeaderHeight(int);

    int footerHeight() const { return m_footerHeight; }
    WEBCORE_EXPORT void setFooterHeight(int);

    FloatBoxExtent obscuredContentInsets() const { return m_staticConfigData->obscuredContentInsets; }
    WEBCORE_EXPORT void setObscuredContentInsets(const FloatBoxExtent&);

    // The target offset for rubber band animations when refresh controller is present.
    // When non-zero, rubber banding will snap to this offset instead of the edge.
#if HAVE(NSREFRESHCONTROLLER)
    float topScrollStretchForRefreshController() const { return m_dynamicState.topScrollStretchForRefreshController; }
    WEBCORE_EXPORT void setTopScrollStretchForRefreshController(float);
#endif

    const LayerRepresentation& rootContentsLayer() const LIFETIME_BOUND { return m_staticConfigData->rootContentsLayer; }
    WEBCORE_EXPORT void setRootContentsLayer(const LayerRepresentation&);

    // This is a layer moved in the opposite direction to scrolling, for example for background-attachment:fixed
    const LayerRepresentation& counterScrollingLayer() const LIFETIME_BOUND { return m_staticConfigData->counterScrollingLayer; }
    WEBCORE_EXPORT void setCounterScrollingLayer(const LayerRepresentation&);

    // This is a clipping layer that will scroll with the page for all y-delta scroll values between 0
    // and obscuredInset().top. Once the y-deltas get beyond the content inset point, this layer no longer
    // needs to move. If the obscuredInset().top is 0, this layer does not need to move at all. This is
    // only used on the Mac.
    const LayerRepresentation& insetClipLayer() const LIFETIME_BOUND { return m_staticConfigData->insetClipLayer; }
    WEBCORE_EXPORT void setInsetClipLayer(const LayerRepresentation&);

    const LayerRepresentation& contentShadowLayer() const LIFETIME_BOUND { return m_staticConfigData->contentShadowLayer; }
    WEBCORE_EXPORT void setContentShadowLayer(const LayerRepresentation&);

    // The header and footer layers scroll vertically with the page, they should remain fixed when scrolling horizontally.
    const LayerRepresentation& headerLayer() const LIFETIME_BOUND { return m_staticConfigData->headerLayer; }
    WEBCORE_EXPORT void setHeaderLayer(const LayerRepresentation&);

    // The header and footer layers scroll vertically with the page, they should remain fixed when scrolling horizontally.
    const LayerRepresentation& footerLayer() const LIFETIME_BOUND { return m_staticConfigData->footerLayer; }
    WEBCORE_EXPORT void setFooterLayer(const LayerRepresentation&);

    // True when the visual viewport is smaller than the layout viewport, indicating that panning should be possible.
    bool visualViewportIsSmallerThanLayoutViewport() const { return m_visualViewportIsSmallerThanLayoutViewport; }
    WEBCORE_EXPORT void setVisualViewportIsSmallerThanLayoutViewport(bool);

    bool asyncFrameOrOverflowScrollingEnabled() const { return m_asyncFrameOrOverflowScrollingEnabled; }
    WEBCORE_EXPORT void setAsyncFrameOrOverflowScrollingEnabled(bool);

    bool scrollingPerformanceTestingEnabled() const { return m_scrollingPerformanceTestingEnabled; }
    WEBCORE_EXPORT void setScrollingPerformanceTestingEnabled(bool);

    bool wheelEventGesturesBecomeNonBlocking() const { return m_wheelEventGesturesBecomeNonBlocking; }
    WEBCORE_EXPORT void setWheelEventGesturesBecomeNonBlocking(bool);
    
    bool overlayScrollbarsEnabled() const { return m_overlayScrollbarsEnabled; }
    WEBCORE_EXPORT void setOverlayScrollbarsEnabled(bool);

    WEBCORE_EXPORT bool NODELETE isMainFrame() const;
    
    void dumpProperties(WTF::TextStream&, OptionSet<ScrollingStateTreeAsTextBehavior>) const override;
    bool hasUnchangedGroupsAs(const ScrollingStateNode&) const final;
    void clearLayerFieldsForUnchangedProperties() final;
#if ASSERT_ENABLED
    void verifyClearedLayerFieldsForUnchangedProperties() const final;
#endif

private:
    WEBCORE_EXPORT ScrollingStateFrameScrollingNode(
        bool isMainFrame,
        ScrollingNodeID,
        Vector<Ref<WebCore::ScrollingStateNode>>&& children,
        OptionSet<ScrollingStateNodeProperty> changedProperties,
        std::optional<WebCore::PlatformLayerIdentifier>,
        FloatSize scrollableAreaSize,
        FloatSize totalContentsSize,
        FloatSize reachableContentsSize,
        FloatPoint scrollPosition,
        IntPoint scrollOrigin,
        ScrollableAreaParameters&&,
#if ENABLE(SCROLLING_THREAD)
        OptionSet<SynchronousScrollingReason> synchronousScrollingReasons,
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
        RequestedKeyboardScrollData&&,
        float frameScaleFactor,
        EventTrackingRegions&&,
        std::optional<PlatformLayerIdentifier> rootContentsLayer,
        std::optional<PlatformLayerIdentifier> counterScrollingLayer,
        std::optional<PlatformLayerIdentifier> insetClipLayer,
        std::optional<PlatformLayerIdentifier> contentShadowLayer,
        int headerHeight,
        int footerHeight,
        ScrollBehaviorForFixedElements&&,
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
    );

    ScrollingStateFrameScrollingNode(ScrollingStateTree&, ScrollingNodeType, ScrollingNodeID);
    ScrollingStateFrameScrollingNode(const ScrollingStateFrameScrollingNode&, ScrollingStateTree&);

    OptionSet<ScrollingStateNode::Property> applicableProperties() const final;

    // Per-frame mutable frame-scrolling state.
    struct DynamicFrameState {
        float frameScaleFactor { 1 };
#if HAVE(NSREFRESHCONTROLLER)
        float topScrollStretchForRefreshController { 0 };
#endif

        friend bool operator==(const DynamicFrameState&, const DynamicFrameState&) = default;
    };

    // Layout-derived frame-scrolling state. CoW-shared via DataRef.
    class StaticLayoutFrameState : public ThreadSafeRefCounted<StaticLayoutFrameState> {
    public:
        static Ref<StaticLayoutFrameState> create() { return adoptRef(*new StaticLayoutFrameState); }
        Ref<StaticLayoutFrameState> copy() const { return adoptRef(*new StaticLayoutFrameState(*this)); }

        EventTrackingRegions eventTrackingRegions;
        FloatRect layoutViewport;
        FloatSize sizeForVisibleContent;
        FloatPoint minLayoutViewportOrigin;
        FloatPoint maxLayoutViewportOrigin;

    private:
        StaticLayoutFrameState() = default;
        StaticLayoutFrameState(const StaticLayoutFrameState& other)
            : ThreadSafeRefCounted<StaticLayoutFrameState>()
            , eventTrackingRegions(other.eventTrackingRegions)
            , layoutViewport(other.layoutViewport)
            , sizeForVisibleContent(other.sizeForVisibleContent)
            , minLayoutViewportOrigin(other.minLayoutViewportOrigin)
            , maxLayoutViewportOrigin(other.maxLayoutViewportOrigin)
        {
        }
    };

    // Long-lived frame-scrolling configuration. CoW-shared via DataRef.
    class StaticConfigurationFrameState : public ThreadSafeRefCounted<StaticConfigurationFrameState> {
    public:
        static Ref<StaticConfigurationFrameState> create() { return adoptRef(*new StaticConfigurationFrameState); }
        Ref<StaticConfigurationFrameState> copy() const { return adoptRef(*new StaticConfigurationFrameState(*this)); }

        LayerRepresentation rootContentsLayer;
        LayerRepresentation counterScrollingLayer;
        LayerRepresentation insetClipLayer;
        LayerRepresentation contentShadowLayer;
        LayerRepresentation headerLayer;
        LayerRepresentation footerLayer;
        std::optional<FloatSize> overrideVisualViewportSize;
        FloatBoxExtent obscuredContentInsets;

    private:
        StaticConfigurationFrameState() = default;
        StaticConfigurationFrameState(const StaticConfigurationFrameState& other)
            : ThreadSafeRefCounted<StaticConfigurationFrameState>()
            , rootContentsLayer(other.rootContentsLayer)
            , counterScrollingLayer(other.counterScrollingLayer)
            , insetClipLayer(other.insetClipLayer)
            , contentShadowLayer(other.contentShadowLayer)
            , headerLayer(other.headerLayer)
            , footerLayer(other.footerLayer)
            , overrideVisualViewportSize(other.overrideVisualViewportSize)
            , obscuredContentInsets(other.obscuredContentInsets)
        {
        }
    };

    DataRef<StaticLayoutFrameState> m_staticLayoutData { StaticLayoutFrameState::create() };
    DataRef<StaticConfigurationFrameState> m_staticConfigData { StaticConfigurationFrameState::create() };
    DynamicFrameState m_dynamicState;

    // Tiny PODs kept as direct members rather than inside the CoW group: each mutation
    // would otherwise force a copy of the entire ~hundreds-of-bytes static-config group
    // (containing six LayerRepresentation variants + several other large fields).
    int m_headerHeight { 0 };
    int m_footerHeight { 0 };
    ScrollBehaviorForFixedElements m_behaviorForFixed { ScrollBehaviorForFixedElements::StickToDocumentBounds };
    bool m_visualViewportIsSmallerThanLayoutViewport { false };
    bool m_asyncFrameOrOverflowScrollingEnabled { false };
    bool m_wheelEventGesturesBecomeNonBlocking { false };
    bool m_scrollingPerformanceTestingEnabled { false };
    bool m_overlayScrollbarsEnabled { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_STATE_NODE(ScrollingStateFrameScrollingNode, isFrameScrollingNode())

#endif // ENABLE(ASYNC_SCROLLING)
