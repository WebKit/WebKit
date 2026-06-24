/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <WebCore/GraphicsLayer.h>
#include <WebCore/ScrollingCoordinator.h>
#include <WebCore/ScrollingPlatformLayer.h>
#if USE(COORDINATED_GRAPHICS)
#include <WebCore/CoordinatedPlatformLayer.h>
#endif
#include <stdint.h>
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class GraphicsLayer;
class ScrollingStateTree;

// Used to allow ScrollingStateNodes to refer to layers in various contexts:
// a) Async scrolling, main thread: ScrollingStateNode holds onto a GraphicsLayer, and uses m_layerID
//    to detect whether that GraphicsLayer's underlying PlatformLayer changed.
// b) Threaded scrolling, commit to scrolling thread: ScrollingStateNode wraps a PlatformLayer, which
//    can be passed to the Scrolling Thread
// c) Remote scrolling UI process, where LayerRepresentation wraps just a PlatformLayerID.
class LayerRepresentation {
public:
    enum Type {
        GraphicsLayerRepresentation,
        PlatformLayerRepresentation,
        PlatformLayerIDRepresentation
    };

#if PLATFORM(COCOA)
    using PlatformLayerHolder = PlatformLayerContainer;
#elif USE(COORDINATED_GRAPHICS)
    using PlatformLayerHolder = RefPtr<CoordinatedPlatformLayer>;
#endif

    LayerRepresentation() = default;

    LayerRepresentation(GraphicsLayer* graphicsLayer)
        : m_data(GraphicsLayerData { graphicsLayer, graphicsLayer ? std::optional { graphicsLayer->primaryLayerID() } : std::nullopt })
    { }

    LayerRepresentation(ScrollingPlatformLayer* platformLayer)
        : m_data(PlatformLayerHolder { platformLayer })
    { }

    LayerRepresentation(std::optional<PlatformLayerIdentifier> layerID)
        : m_data(Markable<PlatformLayerIdentifier> { layerID })
    { }

    explicit operator GraphicsLayer*() const
    {
        ASSERT(std::holds_alternative<GraphicsLayerData>(m_data));
        return std::get<GraphicsLayerData>(m_data).graphicsLayer.get();
    }

    explicit operator ScrollingPlatformLayer*() const
    {
        ASSERT(std::holds_alternative<PlatformLayerHolder>(m_data)); // Somehow we can get here without a platform layer: rdar://178173007.
        if (auto* holder = std::get_if<PlatformLayerHolder>(&m_data))
            return holder->get();
        return nullptr;
    }

    std::optional<PlatformLayerIdentifier> layerID() const
    {
        return WTF::switchOn(m_data,
            [](const GraphicsLayerData& data) -> std::optional<PlatformLayerIdentifier> { return data.layerID.asOptional(); },
            [](const Markable<PlatformLayerIdentifier>& layerID) -> std::optional<PlatformLayerIdentifier> { return layerID.asOptional(); },
            [](const auto&) -> std::optional<PlatformLayerIdentifier> { return std::nullopt; }
        );
    }

    explicit operator bool() const
    {
        return WTF::switchOn(m_data,
            [](std::monostate) { return false; },
            [](const GraphicsLayerData& data) { return !!data.graphicsLayer; },
            [](const PlatformLayerHolder& holder) { return !!holder; },
            [](const Markable<PlatformLayerIdentifier>& layerID) { return !!layerID; }
        );
    }

    bool operator==(const LayerRepresentation& other) const = default;

    LayerRepresentation toRepresentation(Type representation) const
    {
        switch (representation) {
        case GraphicsLayerRepresentation: {
            ASSERT(std::holds_alternative<GraphicsLayerData>(m_data));
            auto& data = std::get<GraphicsLayerData>(m_data);
            return LayerRepresentation(data.graphicsLayer.get());
        }
        case PlatformLayerRepresentation: {
            if (std::holds_alternative<GraphicsLayerData>(m_data)) {
                auto& data = std::get<GraphicsLayerData>(m_data);
                if (data.graphicsLayer)
                    return platformLayerFromGraphicsLayer(Ref { *data.graphicsLayer });
            }
            return static_cast<ScrollingPlatformLayer*>(nullptr);
        }
        case PlatformLayerIDRepresentation:
            return LayerRepresentation(layerID());
        }
        ASSERT_NOT_REACHED();
        return LayerRepresentation();
    }

    bool representsGraphicsLayer() const { return std::holds_alternative<GraphicsLayerData>(m_data); }
    bool representsPlatformLayerID() const { return std::holds_alternative<Markable<PlatformLayerIdentifier>>(m_data); }

private:
    struct GraphicsLayerData {
        RefPtr<GraphicsLayer> graphicsLayer;
        Markable<PlatformLayerIdentifier> layerID;

        friend bool operator==(const GraphicsLayerData&, const GraphicsLayerData&) = default;
    };

    WEBCORE_EXPORT static ScrollingPlatformLayer* platformLayerFromGraphicsLayer(GraphicsLayer&);

    Variant<std::monostate, GraphicsLayerData, PlatformLayerHolder, Markable<PlatformLayerIdentifier>> m_data;
};

enum class ScrollingStateNodeProperty : uint64_t {
    // ScrollingStateNode
    Layer                                       = 1LLU << 0,
    // ScrollingStateScrollingNode
    ScrollableAreaSize                          = Layer << 1, // Same value as RelatedOverflowScrollingNodes, ViewportConstraints and OverflowScrollingNode
    TotalContentsSize                           = ScrollableAreaSize << 1, // Same value as LayoutConstraintData
    ReachableContentsSize                       = TotalContentsSize << 1,
    ScrollPosition                              = ReachableContentsSize << 1,
    ScrollOrigin                                = ScrollPosition << 1,
    ScrollableAreaParams                        = ScrollOrigin << 1,
#if ENABLE(SCROLLING_THREAD)
    ReasonsForSynchronousScrolling              = ScrollableAreaParams << 1,
    RequestedScrollPosition                     = ReasonsForSynchronousScrolling << 1,
#else
    RequestedScrollPosition                     = ScrollableAreaParams << 1,
#endif
    SnapOffsetsInfo                             = RequestedScrollPosition << 1,
    CurrentHorizontalSnapOffsetIndex            = SnapOffsetsInfo << 1,
    CurrentVerticalSnapOffsetIndex              = CurrentHorizontalSnapOffsetIndex << 1,
    IsMonitoringWheelEvents                     = CurrentVerticalSnapOffsetIndex << 1,
    ScrollContainerLayer                        = IsMonitoringWheelEvents << 1,
    ScrolledContentsLayer                       = ScrollContainerLayer << 1,
    HorizontalScrollbarLayer                    = ScrolledContentsLayer << 1,
    VerticalScrollbarLayer                      = HorizontalScrollbarLayer << 1,
    ContentAreaHoverState                       = VerticalScrollbarLayer << 1,
    MouseActivityState                          = ContentAreaHoverState << 1,
    ScrollbarHoverState                         = MouseActivityState << 1,
    ScrollbarEnabledState                       = ScrollbarHoverState << 1,
    ScrollbarColor                              = ScrollbarEnabledState << 1,
    ScrollbarLayoutDirection                    = ScrollbarColor << 1,
    ScrollbarWidth                              = ScrollbarLayoutDirection << 1,
    UseDarkAppearanceForScrollbars              = ScrollbarWidth << 1,
    // ScrollingStateFrameScrollingNode
    KeyboardScrollData                          = UseDarkAppearanceForScrollbars << 1,
    FrameScaleFactor                            = KeyboardScrollData << 1,
    EventTrackingRegion                         = FrameScaleFactor << 1,
    RootContentsLayer                           = EventTrackingRegion << 1,
    CounterScrollingLayer                       = RootContentsLayer << 1,
    InsetClipLayer                              = CounterScrollingLayer << 1,
    ContentShadowLayer                          = InsetClipLayer << 1,
    HeaderHeight                                = ContentShadowLayer << 1,
    FooterHeight                                = HeaderHeight << 1,
    BehaviorForFixedElements                    = FooterHeight << 1,
    ObscuredContentInsets                       = BehaviorForFixedElements << 1,
#if HAVE(NSREFRESHCONTROLLER)
    TopScrollStretchForRefreshController        = ObscuredContentInsets << 1,
    VisualViewportIsSmallerThanLayoutViewport   = TopScrollStretchForRefreshController << 1,
#else
    VisualViewportIsSmallerThanLayoutViewport   = ObscuredContentInsets << 1,
#endif
    AsyncFrameOrOverflowScrollingEnabled        = VisualViewportIsSmallerThanLayoutViewport << 1,
    WheelEventGesturesBecomeNonBlocking         = AsyncFrameOrOverflowScrollingEnabled << 1,
    ScrollingPerformanceTestingEnabled          = WheelEventGesturesBecomeNonBlocking << 1,
    LayoutViewport                              = ScrollingPerformanceTestingEnabled << 1,
    SizeForVisibleContent                       = LayoutViewport << 1,
    MinLayoutViewportOrigin                     = SizeForVisibleContent << 1,
    MaxLayoutViewportOrigin                     = MinLayoutViewportOrigin << 1,
    OverrideVisualViewportSize                  = MaxLayoutViewportOrigin << 1,
    OverlayScrollbarsEnabled                    = OverrideVisualViewportSize << 1,
    ChildNodes                                  = OverlayScrollbarsEnabled << 1,
    // ScrollingStatePositionedNode
    RelatedOverflowScrollingNodes               = ScrollableAreaSize,
    LayoutConstraintData                        = TotalContentsSize,
    // ScrollingStateFixedNode, ScrollingStateStickyNode
    ViewportConstraints                         = ScrollableAreaSize,
    ViewportAnchorLayer                         = TotalContentsSize,
    // ScrollingStateOverflowScrollProxyNode
    OverflowScrollingNode                       = ScrollableAreaSize,
    // ScrollingStateFrameHostingNode
    LayerHostingContextIdentifier               = ScrollableAreaSize,

    // The following flags are not serialized.
    PainterForScrollbar                         = ChildNodes << 1,
#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    ScrollbarOpacity                            = PainterForScrollbar << 1,
    HeaderLayer                                 = ScrollbarOpacity << 1,
#else
    HeaderLayer                                 = PainterForScrollbar << 1,
#endif
    FooterLayer                                 = HeaderLayer << 1,
};

class ScrollingStateNode : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ScrollingStateNode> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ScrollingStateNode, WEBCORE_EXPORT);
public:
    virtual ~ScrollingStateNode();

    using Property = ScrollingStateNodeProperty;

    ScrollingNodeType nodeType() const { return m_nodeType; }

    bool isFixedNode() const { return m_nodeType == ScrollingNodeType::Fixed; }
    bool isStickyNode() const { return m_nodeType == ScrollingNodeType::Sticky; }
    bool isPositionedNode() const { return m_nodeType == ScrollingNodeType::Positioned; }
    bool isScrollingNode() const { return isFrameScrollingNode() || isOverflowScrollingNode() || isPluginScrollingNode(); }
    bool isFrameScrollingNode() const { return m_nodeType == ScrollingNodeType::MainFrame || m_nodeType == ScrollingNodeType::Subframe; }
    bool isFrameHostingNode() const { return m_nodeType == ScrollingNodeType::FrameHosting; }
    bool isPluginScrollingNode() const { return m_nodeType == ScrollingNodeType::PluginScrolling; }
    bool isPluginHostingNode() const { return m_nodeType == ScrollingNodeType::PluginHosting; }
    bool isOverflowScrollingNode() const { return m_nodeType == ScrollingNodeType::Overflow; }
    bool isOverflowScrollProxyNode() const { return m_nodeType == ScrollingNodeType::OverflowProxy; }

    virtual Ref<ScrollingStateNode> clone(ScrollingStateTree& adoptiveTree) = 0;
    Ref<ScrollingStateNode> cloneAndReset(ScrollingStateTree& adoptiveTree);
    void cloneAndResetChildren(ScrollingStateNode&, ScrollingStateTree& adoptiveTree);
    
    bool hasChangedProperties() const { return !m_changedProperties.isEmpty(); }
    bool hasChangedProperty(Property property) const { return m_changedProperties.contains(property); }
    void resetChangedProperties() { m_changedProperties = { }; }
    void setPropertyChanged(Property);

    void setPropertyChangesAfterReattach();

    OptionSet<Property> changedProperties() const { return m_changedProperties; }
    void setChangedProperties(OptionSet<Property> changedProperties) { m_changedProperties = changedProperties; }
    
    virtual void reconcileLayerPositionForViewportRect(const LayoutRect& /*viewportRect*/, ScrollingLayerPositionAction) { }

    const LayerRepresentation& layer() const LIFETIME_BOUND { return m_layer; }
    WEBCORE_EXPORT void setLayer(const LayerRepresentation&);

    bool isAttachedToScrollingStateTree() const { return !!m_scrollingStateTree; }
    ScrollingStateTree& scrollingStateTree() const
    {
        ASSERT(m_scrollingStateTree);
        return *m_scrollingStateTree;
    }
    void NODELETE attachAfterDeserialization(ScrollingStateTree&);

    ScrollingNodeID scrollingNodeID() const { return m_nodeID; }

    RefPtr<ScrollingStateNode> parent() const { return m_parent; }
    void setParent(RefPtr<ScrollingStateNode>&& parent) { m_parent = parent; }
    std::optional<ScrollingNodeID> parentNodeID() const;

    Vector<Ref<ScrollingStateNode>>& children() LIFETIME_BOUND { return m_children; }
    const Vector<Ref<ScrollingStateNode>>& children() const LIFETIME_BOUND { return m_children; }
    Vector<Ref<ScrollingStateNode>> takeChildren() { return std::exchange(m_children, { }); }
    WEBCORE_EXPORT void setChildren(Vector<Ref<ScrollingStateNode>>&&);
    void traverse(NOESCAPE const Function<void(ScrollingStateNode&)>&);

    void appendChild(Ref<ScrollingStateNode>&&);
    void insertChild(Ref<ScrollingStateNode>&&, size_t index);

    // Note that node ownership is via the parent, so these functions can trigger node deletion.
    void removeFromParent();
    void removeChild(ScrollingStateNode&);

    RefPtr<ScrollingStateNode> childAtIndex(size_t) const;

    String scrollingStateTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> = { }) const;
#if ASSERT_ENABLED
    bool parentPointersAreCorrect() const;
#endif

protected:
    ScrollingStateNode(const ScrollingStateNode&, ScrollingStateTree&);
    ScrollingStateNode(ScrollingNodeType, ScrollingStateTree&, ScrollingNodeID);
    ScrollingStateNode(ScrollingNodeType, ScrollingNodeID, Vector<Ref<ScrollingStateNode>>&&, OptionSet<ScrollingStateNodeProperty>, std::optional<PlatformLayerIdentifier>);

    void setPropertyChangedInternal(Property property) { m_changedProperties.add(property); }
    void setPropertiesChangedInternal(OptionSet<Property> properties) { m_changedProperties.add(properties); }

    virtual void dumpProperties(WTF::TextStream&, OptionSet<ScrollingStateTreeAsTextBehavior>) const;
    virtual OptionSet<Property> applicableProperties() const;

private:
    void dump(WTF::TextStream&, OptionSet<ScrollingStateTreeAsTextBehavior>) const;

    const ScrollingNodeType m_nodeType;
    const ScrollingNodeID m_nodeID;
    OptionSet<Property> m_changedProperties;

    CheckedPtr<ScrollingStateTree> m_scrollingStateTree; // Only null between deserialization and attachAfterDeserialization.

    ThreadSafeWeakPtr<ScrollingStateNode> m_parent;
    Vector<Ref<ScrollingStateNode>> m_children;

    LayerRepresentation m_layer;
};

inline std::optional<ScrollingNodeID> ScrollingStateNode::parentNodeID() const
{
    auto parent = m_parent.get();
    if (!parent)
        return std::nullopt;
    return parent->scrollingNodeID();
}

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_SCROLLING_STATE_NODE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::ScrollingStateNode& node) { return node.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(ASYNC_SCROLLING)
