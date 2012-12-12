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

#include "config.h"
#include "ScrollingStateScrollingNode.h"

#include "ScrollingStateTree.h"
#include "TextStream.h"
#include <wtf/OwnPtr.h>

#if ENABLE(THREADED_SCROLLING)

namespace WebCore {

PassOwnPtr<ScrollingStateScrollingNode> ScrollingStateScrollingNode::create(ScrollingStateTree* stateTree, ScrollingNodeID nodeID)
{
    return adoptPtr(new ScrollingStateScrollingNode(stateTree, nodeID));
}

ScrollingStateScrollingNode::ScrollingStateScrollingNode(ScrollingStateTree* stateTree, ScrollingNodeID nodeID)
    : ScrollingStateNode(stateTree, nodeID)
    , m_changedProperties(0)
    , m_wheelEventHandlerCount(0)
    , m_shouldUpdateScrollLayerPositionOnMainThread(0)
    , m_horizontalScrollElasticity(ScrollElasticityNone)
    , m_verticalScrollElasticity(ScrollElasticityNone)
    , m_hasEnabledHorizontalScrollbar(false)
    , m_hasEnabledVerticalScrollbar(false)
    , m_requestedScrollPositionRepresentsProgrammaticScroll(false)
    , m_horizontalScrollbarMode(ScrollbarAuto)
    , m_verticalScrollbarMode(ScrollbarAuto)
{
}

ScrollingStateScrollingNode::ScrollingStateScrollingNode(const ScrollingStateScrollingNode& stateNode)
    : ScrollingStateNode(stateNode)
    , m_changedProperties(stateNode.changedProperties())
    , m_viewportRect(stateNode.viewportRect())
    , m_contentsSize(stateNode.contentsSize())
    , m_nonFastScrollableRegion(stateNode.nonFastScrollableRegion())
    , m_wheelEventHandlerCount(stateNode.wheelEventHandlerCount())
    , m_shouldUpdateScrollLayerPositionOnMainThread(stateNode.shouldUpdateScrollLayerPositionOnMainThread())
    , m_horizontalScrollElasticity(stateNode.horizontalScrollElasticity())
    , m_verticalScrollElasticity(stateNode.verticalScrollElasticity())
    , m_hasEnabledHorizontalScrollbar(stateNode.hasEnabledHorizontalScrollbar())
    , m_hasEnabledVerticalScrollbar(stateNode.hasEnabledVerticalScrollbar())
    , m_requestedScrollPositionRepresentsProgrammaticScroll(stateNode.requestedScrollPositionRepresentsProgrammaticScroll())
    , m_horizontalScrollbarMode(stateNode.horizontalScrollbarMode())
    , m_verticalScrollbarMode(stateNode.verticalScrollbarMode())
    , m_requestedScrollPosition(stateNode.requestedScrollPosition())
    , m_scrollOrigin(stateNode.scrollOrigin())
{
}

ScrollingStateScrollingNode::~ScrollingStateScrollingNode()
{
}

PassOwnPtr<ScrollingStateNode> ScrollingStateScrollingNode::clone()
{
    return adoptPtr(new ScrollingStateScrollingNode(*this));
}

void ScrollingStateScrollingNode::setViewportRect(const IntRect& viewportRect)
{
    if (m_viewportRect == viewportRect)
        return;

    m_viewportRect = viewportRect;
    m_changedProperties |= ViewportRect;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setContentsSize(const IntSize& contentsSize)
{
    if (m_contentsSize == contentsSize)
        return;

    m_contentsSize = contentsSize;
    m_changedProperties |= ContentsSize;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setNonFastScrollableRegion(const Region& nonFastScrollableRegion)
{
    if (m_nonFastScrollableRegion == nonFastScrollableRegion)
        return;

    m_nonFastScrollableRegion = nonFastScrollableRegion;
    m_changedProperties |= NonFastScrollableRegion;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setWheelEventHandlerCount(unsigned wheelEventHandlerCount)
{
    if (m_wheelEventHandlerCount == wheelEventHandlerCount)
        return;

    m_wheelEventHandlerCount = wheelEventHandlerCount;
    m_changedProperties |= WheelEventHandlerCount;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setShouldUpdateScrollLayerPositionOnMainThread(MainThreadScrollingReasons reasons)
{
    if (m_shouldUpdateScrollLayerPositionOnMainThread == reasons)
        return;

    m_shouldUpdateScrollLayerPositionOnMainThread = reasons;
    m_changedProperties |= ShouldUpdateScrollLayerPositionOnMainThread;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setHorizontalScrollElasticity(ScrollElasticity horizontalScrollElasticity)
{
    if (m_horizontalScrollElasticity == horizontalScrollElasticity)
        return;

    m_horizontalScrollElasticity = horizontalScrollElasticity;
    m_changedProperties |= HorizontalScrollElasticity;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setVerticalScrollElasticity(ScrollElasticity verticalScrollElasticity)
{
    if (m_verticalScrollElasticity == verticalScrollElasticity)
        return;

    m_verticalScrollElasticity = verticalScrollElasticity;
    m_changedProperties |= VerticalScrollElasticity;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setHasEnabledHorizontalScrollbar(bool hasEnabledHorizontalScrollbar)
{
    if (m_hasEnabledHorizontalScrollbar == hasEnabledHorizontalScrollbar)
        return;

    m_hasEnabledHorizontalScrollbar = hasEnabledHorizontalScrollbar;
    m_changedProperties |= HasEnabledHorizontalScrollbar;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setHasEnabledVerticalScrollbar(bool hasEnabledVerticalScrollbar)
{
    if (m_hasEnabledVerticalScrollbar == hasEnabledVerticalScrollbar)
        return;

    m_hasEnabledVerticalScrollbar = hasEnabledVerticalScrollbar;
    m_changedProperties |= HasEnabledVerticalScrollbar;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setHorizontalScrollbarMode(ScrollbarMode horizontalScrollbarMode)
{
    if (m_horizontalScrollbarMode == horizontalScrollbarMode)
        return;

    m_horizontalScrollbarMode = horizontalScrollbarMode;
    m_changedProperties |= HorizontalScrollbarMode;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setVerticalScrollbarMode(ScrollbarMode verticalScrollbarMode)
{
    if (m_verticalScrollbarMode == verticalScrollbarMode)
        return;

    m_verticalScrollbarMode = verticalScrollbarMode;
    m_changedProperties |= VerticalScrollbarMode;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setRequestedScrollPosition(const IntPoint& requestedScrollPosition, bool representsProgrammaticScroll)
{
    m_requestedScrollPosition = requestedScrollPosition;
    m_requestedScrollPositionRepresentsProgrammaticScroll = representsProgrammaticScroll;
    m_changedProperties |= RequestedScrollPosition;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::setScrollOrigin(const IntPoint& scrollOrigin)
{
    if (m_scrollOrigin == scrollOrigin)
        return;

    m_scrollOrigin = scrollOrigin;
    m_changedProperties |= ScrollOrigin;
    m_scrollingStateTree->setHasChangedProperties(true);
}

void ScrollingStateScrollingNode::dumpProperties(TextStream& ts, int indent) const
{
    ts << "(" << "Scrolling node" << "\n";

    if (!m_viewportRect.isEmpty()) {
        writeIndent(ts, indent + 1);
        ts << "(viewport rect " << m_viewportRect.x() << " " << m_viewportRect.y() << " " << m_viewportRect.width() << " " << m_viewportRect.height() << ")\n";
    }

    if (!m_contentsSize.isEmpty()) {
        writeIndent(ts, indent + 1);
        ts << "(contents size " << m_contentsSize.width() << " " << m_contentsSize.height() << ")\n";
    }

    if (m_shouldUpdateScrollLayerPositionOnMainThread) {
        writeIndent(ts, indent + 1);
        ts << "(Scrolling on main thread because: " << ScrollingCoordinator::mainThreadScrollingReasonsAsText(m_shouldUpdateScrollLayerPositionOnMainThread) << ")\n";
    }

    if (m_requestedScrollPosition != IntPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(requested scroll position " << m_requestedScrollPosition.x() << " " << m_requestedScrollPosition.y() << ")\n";
    }

    if (m_scrollOrigin != IntPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(scroll origin " << m_scrollOrigin.x() << " " << m_scrollOrigin.y() << ")\n";
    }

}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
