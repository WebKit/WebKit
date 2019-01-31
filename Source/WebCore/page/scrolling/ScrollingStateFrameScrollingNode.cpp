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

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "ScrollingStateTree.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingStateFrameScrollingNode> ScrollingStateFrameScrollingNode::create(ScrollingStateTree& stateTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingStateFrameScrollingNode(stateTree, nodeType, nodeID));
}

ScrollingStateFrameScrollingNode::ScrollingStateFrameScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingStateScrollingNode(stateTree, nodeType, nodeID)
{
    ASSERT(isFrameScrollingNode());
}

ScrollingStateFrameScrollingNode::ScrollingStateFrameScrollingNode(const ScrollingStateFrameScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateScrollingNode(stateNode, adoptiveTree)
#if PLATFORM(MAC)
    , m_verticalScrollerImp(stateNode.verticalScrollerImp())
    , m_horizontalScrollerImp(stateNode.horizontalScrollerImp())
#endif
    , m_eventTrackingRegions(stateNode.eventTrackingRegions())
    , m_requestedScrollPosition(stateNode.requestedScrollPosition())
    , m_layoutViewport(stateNode.layoutViewport())
    , m_minLayoutViewportOrigin(stateNode.minLayoutViewportOrigin())
    , m_maxLayoutViewportOrigin(stateNode.maxLayoutViewportOrigin())
    , m_frameScaleFactor(stateNode.frameScaleFactor())
    , m_topContentInset(stateNode.topContentInset())
    , m_headerHeight(stateNode.headerHeight())
    , m_footerHeight(stateNode.footerHeight())
    , m_synchronousScrollingReasons(stateNode.synchronousScrollingReasons())
    , m_behaviorForFixed(stateNode.scrollBehaviorForFixedElements())
    , m_requestedScrollPositionRepresentsProgrammaticScroll(stateNode.requestedScrollPositionRepresentsProgrammaticScroll())
    , m_fixedElementsLayoutRelativeToFrame(stateNode.fixedElementsLayoutRelativeToFrame())
    , m_visualViewportEnabled(stateNode.visualViewportEnabled())
    , m_asyncFrameOrOverflowScrollingEnabled(stateNode.asyncFrameOrOverflowScrollingEnabled())
{
    if (hasChangedProperty(CounterScrollingLayer))
        setCounterScrollingLayer(stateNode.counterScrollingLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(InsetClipLayer))
        setInsetClipLayer(stateNode.insetClipLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(ContentShadowLayer))
        setContentShadowLayer(stateNode.contentShadowLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(HeaderLayer))
        setHeaderLayer(stateNode.headerLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(FooterLayer))
        setFooterLayer(stateNode.footerLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(VerticalScrollbarLayer))
        setVerticalScrollbarLayer(stateNode.verticalScrollbarLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(HorizontalScrollbarLayer))
        setHorizontalScrollbarLayer(stateNode.horizontalScrollbarLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));
}

ScrollingStateFrameScrollingNode::~ScrollingStateFrameScrollingNode() = default;

Ref<ScrollingStateNode> ScrollingStateFrameScrollingNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(*new ScrollingStateFrameScrollingNode(*this, adoptiveTree));
}

void ScrollingStateFrameScrollingNode::setAllPropertiesChanged()
{
    setPropertyChangedBit(FrameScaleFactor);
    setPropertyChangedBit(EventTrackingRegion);
    setPropertyChangedBit(ReasonsForSynchronousScrolling);
    setPropertyChangedBit(ScrolledContentsLayer);
    setPropertyChangedBit(CounterScrollingLayer);
    setPropertyChangedBit(InsetClipLayer);
    setPropertyChangedBit(ContentShadowLayer);
    setPropertyChangedBit(HeaderHeight);
    setPropertyChangedBit(FooterHeight);
    setPropertyChangedBit(HeaderLayer);
    setPropertyChangedBit(FooterLayer);
    setPropertyChangedBit(VerticalScrollbarLayer);
    setPropertyChangedBit(HorizontalScrollbarLayer);
    setPropertyChangedBit(PainterForScrollbar);
    setPropertyChangedBit(BehaviorForFixedElements);
    setPropertyChangedBit(TopContentInset);
    setPropertyChangedBit(FixedElementsLayoutRelativeToFrame);
    setPropertyChangedBit(VisualViewportEnabled);
    setPropertyChangedBit(AsyncFrameOrOverflowScrollingEnabled);
    setPropertyChangedBit(LayoutViewport);
    setPropertyChangedBit(MinLayoutViewportOrigin);
    setPropertyChangedBit(MaxLayoutViewportOrigin);

    ScrollingStateScrollingNode::setAllPropertiesChanged();
}

void ScrollingStateFrameScrollingNode::setFrameScaleFactor(float scaleFactor)
{
    if (m_frameScaleFactor == scaleFactor)
        return;

    m_frameScaleFactor = scaleFactor;

    setPropertyChanged(FrameScaleFactor);
}

void ScrollingStateFrameScrollingNode::setEventTrackingRegions(const EventTrackingRegions& eventTrackingRegions)
{
    if (m_eventTrackingRegions == eventTrackingRegions)
        return;

    m_eventTrackingRegions = eventTrackingRegions;
    setPropertyChanged(EventTrackingRegion);
}

void ScrollingStateFrameScrollingNode::setSynchronousScrollingReasons(SynchronousScrollingReasons reasons)
{
    if (m_synchronousScrollingReasons == reasons)
        return;

    m_synchronousScrollingReasons = reasons;
    setPropertyChanged(ReasonsForSynchronousScrolling);
}

void ScrollingStateFrameScrollingNode::setScrollBehaviorForFixedElements(ScrollBehaviorForFixedElements behaviorForFixed)
{
    if (m_behaviorForFixed == behaviorForFixed)
        return;

    m_behaviorForFixed = behaviorForFixed;
    setPropertyChanged(BehaviorForFixedElements);
}

void ScrollingStateFrameScrollingNode::setLayoutViewport(const FloatRect& r)
{
    if (m_layoutViewport == r)
        return;

    m_layoutViewport = r;
    setPropertyChanged(LayoutViewport);
}

void ScrollingStateFrameScrollingNode::setMinLayoutViewportOrigin(const FloatPoint& p)
{
    if (m_minLayoutViewportOrigin == p)
        return;

    m_minLayoutViewportOrigin = p;
    setPropertyChanged(MinLayoutViewportOrigin);
}

void ScrollingStateFrameScrollingNode::setMaxLayoutViewportOrigin(const FloatPoint& p)
{
    if (m_maxLayoutViewportOrigin == p)
        return;

    m_maxLayoutViewportOrigin = p;
    setPropertyChanged(MaxLayoutViewportOrigin);
}

void ScrollingStateFrameScrollingNode::setHeaderHeight(int headerHeight)
{
    if (m_headerHeight == headerHeight)
        return;

    m_headerHeight = headerHeight;
    setPropertyChanged(HeaderHeight);
}

void ScrollingStateFrameScrollingNode::setFooterHeight(int footerHeight)
{
    if (m_footerHeight == footerHeight)
        return;

    m_footerHeight = footerHeight;
    setPropertyChanged(FooterHeight);
}

void ScrollingStateFrameScrollingNode::setTopContentInset(float topContentInset)
{
    if (m_topContentInset == topContentInset)
        return;

    m_topContentInset = topContentInset;
    setPropertyChanged(TopContentInset);
}

void ScrollingStateFrameScrollingNode::setCounterScrollingLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_counterScrollingLayer)
        return;
    
    m_counterScrollingLayer = layerRepresentation;
    setPropertyChanged(CounterScrollingLayer);
}

void ScrollingStateFrameScrollingNode::setInsetClipLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_insetClipLayer)
        return;
    
    m_insetClipLayer = layerRepresentation;
    setPropertyChanged(InsetClipLayer);
}

void ScrollingStateFrameScrollingNode::setContentShadowLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_contentShadowLayer)
        return;
    
    m_contentShadowLayer = layerRepresentation;
    setPropertyChanged(ContentShadowLayer);
}

void ScrollingStateFrameScrollingNode::setHeaderLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_headerLayer)
        return;
    
    m_headerLayer = layerRepresentation;
    setPropertyChanged(HeaderLayer);
}

void ScrollingStateFrameScrollingNode::setFooterLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_footerLayer)
        return;
    
    m_footerLayer = layerRepresentation;
    setPropertyChanged(FooterLayer);
}

void ScrollingStateFrameScrollingNode::setVerticalScrollbarLayer(const LayerRepresentation& layer)
{
    if (layer == m_verticalScrollbarLayer)
        return;

    m_verticalScrollbarLayer = layer;
    setPropertyChanged(VerticalScrollbarLayer);
}

void ScrollingStateFrameScrollingNode::setHorizontalScrollbarLayer(const LayerRepresentation& layer)
{
    if (layer == m_horizontalScrollbarLayer)
        return;

    m_horizontalScrollbarLayer = layer;
    setPropertyChanged(HorizontalScrollbarLayer);
}

void ScrollingStateFrameScrollingNode::setFixedElementsLayoutRelativeToFrame(bool fixedElementsLayoutRelativeToFrame)
{
    if (fixedElementsLayoutRelativeToFrame == m_fixedElementsLayoutRelativeToFrame)
        return;
    
    m_fixedElementsLayoutRelativeToFrame = fixedElementsLayoutRelativeToFrame;
    setPropertyChanged(FixedElementsLayoutRelativeToFrame);
}

// Only needed while visual viewports are runtime-switchable.
void ScrollingStateFrameScrollingNode::setVisualViewportEnabled(bool visualViewportEnabled)
{
    if (visualViewportEnabled == m_visualViewportEnabled)
        return;
    
    m_visualViewportEnabled = visualViewportEnabled;
    setPropertyChanged(VisualViewportEnabled);
}

void ScrollingStateFrameScrollingNode::setAsyncFrameOrOverflowScrollingEnabled(bool enabled)
{
    if (enabled == m_asyncFrameOrOverflowScrollingEnabled)
        return;
    
    m_asyncFrameOrOverflowScrollingEnabled = enabled;
    setPropertyChanged(AsyncFrameOrOverflowScrollingEnabled);
}

#if !PLATFORM(MAC)
void ScrollingStateFrameScrollingNode::setScrollerImpsFromScrollbars(Scrollbar*, Scrollbar*)
{
}
#endif

void ScrollingStateFrameScrollingNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "Frame scrolling node";
    
    ScrollingStateScrollingNode::dumpProperties(ts, behavior);
    
    if (behavior & ScrollingStateTreeAsTextBehaviorIncludeLayerIDs) {
        ts.dumpProperty("counter scrolling layer ID", m_counterScrollingLayer.layerID());
        ts.dumpProperty("inset clip layer ID", m_insetClipLayer.layerID());
        ts.dumpProperty("content shadow layer ID", m_contentShadowLayer.layerID());
        ts.dumpProperty("header layer ID", m_headerLayer.layerID());
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
    
    if (m_visualViewportEnabled) {
        ts.dumpProperty("visual viewport enabled", m_visualViewportEnabled);
        ts.dumpProperty("layout viewport", m_layoutViewport);
        ts.dumpProperty("min layout viewport origin", m_minLayoutViewportOrigin);
        ts.dumpProperty("max layout viewport origin", m_maxLayoutViewportOrigin);
    }
    
    if (m_behaviorForFixed == StickToViewportBounds)
        ts.dumpProperty("behavior for fixed", m_behaviorForFixed);

    if (!m_eventTrackingRegions.asynchronousDispatchRegion.isEmpty()) {
        TextStream::GroupScope scope(ts);
        ts << "asynchronous event dispatch region";
        for (auto rect : m_eventTrackingRegions.asynchronousDispatchRegion.rects()) {
            ts << "\n";
            ts << indent << rect;
        }
    }

    if (!m_eventTrackingRegions.eventSpecificSynchronousDispatchRegions.isEmpty()) {
        for (const auto& synchronousEventRegion : m_eventTrackingRegions.eventSpecificSynchronousDispatchRegions) {
            TextStream::GroupScope scope(ts);
            ts << "synchronous event dispatch region for event " << synchronousEventRegion.key;
            for (auto rect : synchronousEventRegion.value.rects()) {
                ts << "\n";
                ts << indent << rect;
            }
        }
    }

    if (m_synchronousScrollingReasons)
        ts.dumpProperty("Scrolling on main thread because:", ScrollingCoordinator::synchronousScrollingReasonsAsText(m_synchronousScrollingReasons));
    
    ts.dumpProperty("behavior for fixed", m_behaviorForFixed);

    if (m_requestedScrollPosition != FloatPoint())
        ts.dumpProperty("requested scroll position", m_requestedScrollPosition);
    if (m_requestedScrollPositionRepresentsProgrammaticScroll)
        ts.dumpProperty("requested scroll position represents programmatic scroll", m_requestedScrollPositionRepresentsProgrammaticScroll);

    if (m_fixedElementsLayoutRelativeToFrame)
        ts.dumpProperty("fixed elements lay out relative to frame", m_fixedElementsLayoutRelativeToFrame);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
