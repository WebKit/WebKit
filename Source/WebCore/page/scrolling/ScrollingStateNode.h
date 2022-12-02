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

#include "GraphicsLayer.h"
#include "ScrollingCoordinator.h"
#include <stdint.h>
#include <wtf/ThreadSafeRefCounted.h>
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
        EmptyRepresentation,
        GraphicsLayerRepresentation,
        PlatformLayerRepresentation,
        PlatformLayerIDRepresentation
    };

    LayerRepresentation() = default;

    LayerRepresentation(GraphicsLayer* graphicsLayer)
        : m_graphicsLayer(graphicsLayer)
        , m_layerID(graphicsLayer ? graphicsLayer->primaryLayerID() : 0)
        , m_representation(GraphicsLayerRepresentation)
    { }

    LayerRepresentation(PlatformLayer* platformLayer)
        : m_typelessPlatformLayer(makePlatformLayerTypeless(platformLayer))
        , m_representation(PlatformLayerRepresentation)
    {
        retainPlatformLayer(m_typelessPlatformLayer);
    }

    LayerRepresentation(GraphicsLayer::PlatformLayerID layerID)
        : m_layerID(layerID)
        , m_representation(PlatformLayerIDRepresentation)
    {
    }

    LayerRepresentation(const LayerRepresentation& other)
        : m_typelessPlatformLayer(other.m_typelessPlatformLayer)
        , m_layerID(other.m_layerID)
        , m_representation(other.m_representation)
    {
        if (m_representation == PlatformLayerRepresentation)
            retainPlatformLayer(m_typelessPlatformLayer);
    }

    ~LayerRepresentation()
    {
        if (m_representation == PlatformLayerRepresentation)
            releasePlatformLayer(m_typelessPlatformLayer);
    }

    explicit operator GraphicsLayer*() const
    {
        ASSERT(m_representation == GraphicsLayerRepresentation);
        return m_graphicsLayer.get();
    }

    explicit operator PlatformLayer*() const
    {
        ASSERT(m_representation == PlatformLayerRepresentation);
        return makePlatformLayerTyped(m_typelessPlatformLayer);
    }
    
    GraphicsLayer::PlatformLayerID layerID() const
    {
        return m_layerID;
    }

    explicit operator GraphicsLayer::PlatformLayerID() const
    {
        ASSERT(m_representation != PlatformLayerRepresentation);
        return m_layerID;
    }

    LayerRepresentation& operator=(const LayerRepresentation& other)
    {
        m_graphicsLayer = other.m_graphicsLayer;
        m_typelessPlatformLayer = other.m_typelessPlatformLayer;
        m_layerID = other.m_layerID;
        m_representation = other.m_representation;

        if (m_representation == PlatformLayerRepresentation)
            retainPlatformLayer(m_typelessPlatformLayer);

        return *this;
    }

    explicit operator bool() const
    {
        switch (m_representation) {
        case EmptyRepresentation:
            return false;
        case GraphicsLayerRepresentation:
            return !!m_graphicsLayer;
        case PlatformLayerRepresentation:
            return !!m_typelessPlatformLayer;
        case PlatformLayerIDRepresentation:
            return !!m_layerID;
        }
        ASSERT_NOT_REACHED();
        return false;
    }

    bool operator==(const LayerRepresentation& other) const
    {
        if (m_representation != other.m_representation)
            return false;
        switch (m_representation) {
        case EmptyRepresentation:
            return true;
        case GraphicsLayerRepresentation:
            return m_graphicsLayer == other.m_graphicsLayer
                && m_layerID == other.m_layerID;
        case PlatformLayerRepresentation:
            return m_typelessPlatformLayer == other.m_typelessPlatformLayer;
        case PlatformLayerIDRepresentation:
            return m_layerID == other.m_layerID;
        }
        ASSERT_NOT_REACHED();
        return true;
    }
    
    LayerRepresentation toRepresentation(Type representation) const
    {
        switch (representation) {
        case EmptyRepresentation:
            return LayerRepresentation();
        case GraphicsLayerRepresentation:
            ASSERT(m_representation == GraphicsLayerRepresentation);
            return LayerRepresentation(m_graphicsLayer.get());
        case PlatformLayerRepresentation:
            return m_graphicsLayer ? m_graphicsLayer->platformLayer() : nullptr;
        case PlatformLayerIDRepresentation:
            return LayerRepresentation(m_layerID);
        }
        ASSERT_NOT_REACHED();
        return LayerRepresentation();
    }

    bool representsGraphicsLayer() const { return m_representation == GraphicsLayerRepresentation; }
    bool representsPlatformLayerID() const { return m_representation == PlatformLayerIDRepresentation; }
    
private:
    WEBCORE_EXPORT static void retainPlatformLayer(void* typelessPlatformLayer);
    WEBCORE_EXPORT static void releasePlatformLayer(void* typelessPlatformLayer);
    WEBCORE_EXPORT static PlatformLayer* makePlatformLayerTyped(void* typelessPlatformLayer);
    WEBCORE_EXPORT static void* makePlatformLayerTypeless(PlatformLayer*);

    RefPtr<GraphicsLayer> m_graphicsLayer;
    void* m_typelessPlatformLayer { nullptr };
    GraphicsLayer::PlatformLayerID m_layerID { 0 };
    Type m_representation { EmptyRepresentation };
};

class ScrollingStateNode : public ThreadSafeRefCounted<ScrollingStateNode> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ScrollingStateNode();
    
    ScrollingNodeType nodeType() const { return m_nodeType; }

    bool isFixedNode() const { return m_nodeType == ScrollingNodeType::Fixed; }
    bool isStickyNode() const { return m_nodeType == ScrollingNodeType::Sticky; }
    bool isPositionedNode() const { return m_nodeType == ScrollingNodeType::Positioned; }
    bool isScrollingNode() const { return isFrameScrollingNode() || isOverflowScrollingNode(); }
    bool isFrameScrollingNode() const { return m_nodeType == ScrollingNodeType::MainFrame || m_nodeType == ScrollingNodeType::Subframe; }
    bool isFrameHostingNode() const { return m_nodeType == ScrollingNodeType::FrameHosting; }
    bool isOverflowScrollingNode() const { return m_nodeType == ScrollingNodeType::Overflow; }
    bool isOverflowScrollProxyNode() const { return m_nodeType == ScrollingNodeType::OverflowProxy; }

    virtual Ref<ScrollingStateNode> clone(ScrollingStateTree& adoptiveTree) = 0;
    Ref<ScrollingStateNode> cloneAndReset(ScrollingStateTree& adoptiveTree);
    void cloneAndResetChildren(ScrollingStateNode&, ScrollingStateTree& adoptiveTree);

    enum class Property : uint64_t {
        // ScrollingStateNode
        Layer                                       = 1LLU << 0,
        ChildNodes                                  = 1LLU << 1,
        // ScrollingStateScrollingNode
        ScrollableAreaSize                          = 1LLU << 2,
        TotalContentsSize                           = 1LLU << 3,
        ReachableContentsSize                       = 1LLU << 4,
        ScrollPosition                              = 1LLU << 5,
        ScrollOrigin                                = 1LLU << 6,
        ScrollableAreaParams                        = 1LLU << 7,
        ReasonsForSynchronousScrolling              = 1LLU << 8,
        RequestedScrollPosition                     = 1LLU << 9,
        SnapOffsetsInfo                             = 1LLU << 10,
        CurrentHorizontalSnapOffsetIndex            = 1LLU << 11,
        CurrentVerticalSnapOffsetIndex              = 1LLU << 12,
        IsMonitoringWheelEvents                     = 1LLU << 13,
        ScrollContainerLayer                        = 1LLU << 14,
        ScrolledContentsLayer                       = 1LLU << 15,
        HorizontalScrollbarLayer                    = 1LLU << 16,
        VerticalScrollbarLayer                      = 1LLU << 17,
        PainterForScrollbar                         = 1LLU << 18,
        // ScrollingStateFrameScrollingNode
        FrameScaleFactor                            = 1LLU << 19,
        EventTrackingRegion                         = 1LLU << 20,
        RootContentsLayer                           = 1LLU << 21,
        CounterScrollingLayer                       = 1LLU << 22,
        InsetClipLayer                              = 1LLU << 23,
        ContentShadowLayer                          = 1LLU << 24,
        HeaderHeight                                = 1LLU << 25,
        FooterHeight                                = 1LLU << 26,
        HeaderLayer                                 = 1LLU << 27,
        FooterLayer                                 = 1LLU << 28,
        BehaviorForFixedElements                    = 1LLU << 29,
        TopContentInset                             = 1LLU << 30,
        FixedElementsLayoutRelativeToFrame          = 1LLU << 31,
        VisualViewportIsSmallerThanLayoutViewport   = 1LLU << 32,
        AsyncFrameOrOverflowScrollingEnabled        = 1LLU << 33,
        WheelEventGesturesBecomeNonBlocking         = 1LLU << 34,
        ScrollingPerformanceTestingEnabled          = 1LLU << 35,
        LayoutViewport                              = 1LLU << 36,
        MinLayoutViewportOrigin                     = 1LLU << 37,
        MaxLayoutViewportOrigin                     = 1LLU << 38,
        OverrideVisualViewportSize                  = 1LLU << 39,
        // ScrollingStatePositionedNode
        RelatedOverflowScrollingNodes               = 1LLU << 40,
        LayoutConstraintData                        = 1LLU << 41,
        // ScrollingStateFixedNode, ScrollingStateStickyNode
        ViewportConstraints                         = 1LLU << 42,
        // ScrollingStateOverflowScrollProxyNode
        OverflowScrollingNode                       = 1LLU << 43,
        KeyboardScrollData                          = 1LLU << 44,
    };
    
    bool hasChangedProperties() const { return !m_changedProperties.isEmpty(); }
    bool hasChangedProperty(Property property) const { return m_changedProperties.contains(property); }
    void resetChangedProperties() { m_changedProperties = { }; }
    void setPropertyChanged(Property);

    virtual void setPropertyChangesAfterReattach();

    OptionSet<Property> changedProperties() const { return m_changedProperties; }
    void setChangedProperties(OptionSet<Property> changedProperties) { m_changedProperties = changedProperties; }
    
    virtual void reconcileLayerPositionForViewportRect(const LayoutRect& /*viewportRect*/, ScrollingLayerPositionAction) { }

    const LayerRepresentation& layer() const { return m_layer; }
    WEBCORE_EXPORT void setLayer(const LayerRepresentation&);

    ScrollingStateTree& scrollingStateTree() const { return m_scrollingStateTree; }

    ScrollingNodeID scrollingNodeID() const { return m_nodeID; }

    ScrollingStateNode* parent() const { return m_parent; }
    void setParent(ScrollingStateNode* parent) { m_parent = parent; }
    ScrollingNodeID parentNodeID() const { return m_parent ? m_parent->scrollingNodeID() : 0; }

    Vector<RefPtr<ScrollingStateNode>>* children() const { return m_children.get(); }
    std::unique_ptr<Vector<RefPtr<ScrollingStateNode>>> takeChildren() { return WTFMove(m_children); }

    void appendChild(Ref<ScrollingStateNode>&&);
    void insertChild(Ref<ScrollingStateNode>&&, size_t index);

    // Note that node ownership is via the parent, so these functions can trigger node deletion.
    void removeFromParent();
    void removeChildAtIndex(size_t index);
    void removeChild(ScrollingStateNode&);

    size_t indexOfChild(ScrollingStateNode&) const;

    String scrollingStateTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> = { }) const;

protected:
    ScrollingStateNode(const ScrollingStateNode&, ScrollingStateTree&);
    ScrollingStateNode(ScrollingNodeType, ScrollingStateTree&, ScrollingNodeID);

    void setPropertyChangedInternal(Property property) { m_changedProperties.add(property); }
    void setPropertiesChangedInternal(OptionSet<Property> properties) { m_changedProperties.add(properties); }

    virtual void dumpProperties(WTF::TextStream&, OptionSet<ScrollingStateTreeAsTextBehavior>) const;
    virtual OptionSet<Property> applicableProperties() const;

private:
    void dump(WTF::TextStream&, OptionSet<ScrollingStateTreeAsTextBehavior>) const;

    const ScrollingNodeType m_nodeType;
    const ScrollingNodeID m_nodeID;
    OptionSet<Property> m_changedProperties;

    ScrollingStateTree& m_scrollingStateTree;

    ScrollingStateNode* m_parent { nullptr };
    std::unique_ptr<Vector<RefPtr<ScrollingStateNode>>> m_children;

    LayerRepresentation m_layer;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_SCROLLING_STATE_NODE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::ScrollingStateNode& node) { return node.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(ASYNC_SCROLLING)
