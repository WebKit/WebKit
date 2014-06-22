/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "TextStream.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

PassRefPtr<ScrollingStateFrameScrollingNode> ScrollingStateFrameScrollingNode::create(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
{
    return adoptRef(new ScrollingStateFrameScrollingNode(stateTree, nodeID));
}

ScrollingStateFrameScrollingNode::ScrollingStateFrameScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
    : ScrollingStateScrollingNode(stateTree, FrameScrollingNode, nodeID)
#if PLATFORM(MAC)
    , m_verticalScrollbarPainter(0)
    , m_horizontalScrollbarPainter(0)
#endif
    , m_frameScaleFactor(1)
    , m_wheelEventHandlerCount(0)
    , m_synchronousScrollingReasons(0)
    , m_behaviorForFixed(StickToDocumentBounds)
    , m_headerHeight(0)
    , m_footerHeight(0)
    , m_requestedScrollPositionRepresentsProgrammaticScroll(false)
    , m_topContentInset(0)
{
}

ScrollingStateFrameScrollingNode::ScrollingStateFrameScrollingNode(const ScrollingStateFrameScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateScrollingNode(stateNode, adoptiveTree)
#if PLATFORM(MAC)
    , m_verticalScrollbarPainter(stateNode.verticalScrollbarPainter())
    , m_horizontalScrollbarPainter(stateNode.horizontalScrollbarPainter())
#endif
    , m_nonFastScrollableRegion(stateNode.nonFastScrollableRegion())
    , m_frameScaleFactor(stateNode.frameScaleFactor())
    , m_wheelEventHandlerCount(stateNode.wheelEventHandlerCount())
    , m_synchronousScrollingReasons(stateNode.synchronousScrollingReasons())
    , m_behaviorForFixed(stateNode.scrollBehaviorForFixedElements())
    , m_headerHeight(stateNode.headerHeight())
    , m_footerHeight(stateNode.footerHeight())
    , m_requestedScrollPosition(stateNode.requestedScrollPosition())
    , m_requestedScrollPositionRepresentsProgrammaticScroll(stateNode.requestedScrollPositionRepresentsProgrammaticScroll())
    , m_topContentInset(stateNode.topContentInset())
{
    if (hasChangedProperty(ScrolledContentsLayer))
        setScrolledContentsLayer(stateNode.scrolledContentsLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

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
}

ScrollingStateFrameScrollingNode::~ScrollingStateFrameScrollingNode()
{
}

PassRefPtr<ScrollingStateNode> ScrollingStateFrameScrollingNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(new ScrollingStateFrameScrollingNode(*this, adoptiveTree));
}

void ScrollingStateFrameScrollingNode::setFrameScaleFactor(float scaleFactor)
{
    if (m_frameScaleFactor == scaleFactor)
        return;

    m_frameScaleFactor = scaleFactor;

    setPropertyChanged(FrameScaleFactor);
}

void ScrollingStateFrameScrollingNode::setNonFastScrollableRegion(const Region& nonFastScrollableRegion)
{
    if (m_nonFastScrollableRegion == nonFastScrollableRegion)
        return;

    m_nonFastScrollableRegion = nonFastScrollableRegion;
    setPropertyChanged(NonFastScrollableRegion);
}

void ScrollingStateFrameScrollingNode::setWheelEventHandlerCount(unsigned wheelEventHandlerCount)
{
    if (m_wheelEventHandlerCount == wheelEventHandlerCount)
        return;

    m_wheelEventHandlerCount = wheelEventHandlerCount;
    setPropertyChanged(WheelEventHandlerCount);
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

void ScrollingStateFrameScrollingNode::setScrolledContentsLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_scrolledContentsLayer)
        return;
    
    m_scrolledContentsLayer = layerRepresentation;
    setPropertyChanged(ScrolledContentsLayer);
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

#if !PLATFORM(MAC)
void ScrollingStateFrameScrollingNode::setScrollbarPaintersFromScrollbars(Scrollbar*, Scrollbar*)
{
}
#endif

void ScrollingStateFrameScrollingNode::dumpProperties(TextStream& ts, int indent) const
{
    ts << "(Frame scrolling node" << "\n";
    
    ScrollingStateScrollingNode::dumpProperties(ts, indent);
    
    if (m_frameScaleFactor != 1) {
        writeIndent(ts, indent + 1);
        ts << "(frame scale factor " << m_frameScaleFactor << ")\n";
    }
    
    if (!m_nonFastScrollableRegion.isEmpty()) {
        ++indent;
        writeIndent(ts, indent);
        ts << "(non-fast-scrollable region";
        ++indent;
        for (auto rect : m_nonFastScrollableRegion.rects()) {
            ts << "\n";
            writeIndent(ts, indent);
            ts << rect;
        }
        ts << ")\n";
        indent -= 2;
    }

    if (m_synchronousScrollingReasons) {
        writeIndent(ts, indent + 1);
        ts << "(Scrolling on main thread because: " << ScrollingCoordinator::synchronousScrollingReasonsAsText(m_synchronousScrollingReasons) << ")\n";
    }
    
    // FIXME: dump more properties.
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
