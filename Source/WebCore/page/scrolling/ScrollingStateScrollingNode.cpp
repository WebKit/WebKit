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

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "ScrollingStateTree.h"
#include "TextStream.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

ScrollingStateScrollingNode::ScrollingStateScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingStateNode(nodeType, stateTree, nodeID)
    , m_requestedScrollPositionRepresentsProgrammaticScroll(false)
{
}

ScrollingStateScrollingNode::ScrollingStateScrollingNode(const ScrollingStateScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(stateNode, adoptiveTree)
    , m_scrollableAreaSize(stateNode.scrollableAreaSize())
    , m_totalContentsSize(stateNode.totalContentsSize())
    , m_reachableContentsSize(stateNode.reachableContentsSize())
    , m_scrollPosition(stateNode.scrollPosition())
    , m_requestedScrollPosition(stateNode.requestedScrollPosition())
    , m_scrollOrigin(stateNode.scrollOrigin())
    , m_scrollableAreaParameters(stateNode.scrollableAreaParameters())
    , m_requestedScrollPositionRepresentsProgrammaticScroll(stateNode.requestedScrollPositionRepresentsProgrammaticScroll())
{
}

ScrollingStateScrollingNode::~ScrollingStateScrollingNode()
{
}

void ScrollingStateScrollingNode::setScrollableAreaSize(const FloatSize& size)
{
    if (m_scrollableAreaSize == size)
        return;

    m_scrollableAreaSize = size;
    setPropertyChanged(ScrollableAreaSize);
}

void ScrollingStateScrollingNode::setTotalContentsSize(const FloatSize& totalContentsSize)
{
    if (m_totalContentsSize == totalContentsSize)
        return;

    m_totalContentsSize = totalContentsSize;
    setPropertyChanged(TotalContentsSize);
}

void ScrollingStateScrollingNode::setReachableContentsSize(const FloatSize& reachableContentsSize)
{
    if (m_reachableContentsSize == reachableContentsSize)
        return;

    m_reachableContentsSize = reachableContentsSize;
    setPropertyChanged(ReachableContentsSize);
}

void ScrollingStateScrollingNode::setScrollPosition(const FloatPoint& scrollPosition)
{
    if (m_scrollPosition == scrollPosition)
        return;

    m_scrollPosition = scrollPosition;
    setPropertyChanged(ScrollPosition);
}

void ScrollingStateScrollingNode::setScrollOrigin(const IntPoint& scrollOrigin)
{
    if (m_scrollOrigin == scrollOrigin)
        return;

    m_scrollOrigin = scrollOrigin;
    setPropertyChanged(ScrollOrigin);
}

void ScrollingStateScrollingNode::setScrollableAreaParameters(const ScrollableAreaParameters& parameters)
{
    if (m_scrollableAreaParameters == parameters)
        return;

    m_scrollableAreaParameters = parameters;
    setPropertyChanged(ScrollableAreaParams);
}

void ScrollingStateScrollingNode::setRequestedScrollPosition(const FloatPoint& requestedScrollPosition, bool representsProgrammaticScroll)
{
    m_requestedScrollPosition = requestedScrollPosition;
    m_requestedScrollPositionRepresentsProgrammaticScroll = representsProgrammaticScroll;
    setPropertyChanged(RequestedScrollPosition);
}

void ScrollingStateScrollingNode::dumpProperties(TextStream& ts, int indent) const
{
    if (m_scrollPosition != FloatPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(scroll position "
            << TextStream::FormatNumberRespectingIntegers(m_scrollPosition.x()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_scrollPosition.y()) << ")\n";
    }

    if (!m_scrollableAreaSize.isEmpty()) {
        writeIndent(ts, indent + 1);
        ts << "(scrollable area size "
            << TextStream::FormatNumberRespectingIntegers(m_scrollableAreaSize.width()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_scrollableAreaSize.height()) << ")\n";
    }

    if (!m_totalContentsSize.isEmpty()) {
        writeIndent(ts, indent + 1);
        ts << "(contents size "
            << TextStream::FormatNumberRespectingIntegers(m_totalContentsSize.width()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_totalContentsSize.height()) << ")\n";
    }

    if (m_requestedScrollPosition != IntPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(requested scroll position "
            << TextStream::FormatNumberRespectingIntegers(m_requestedScrollPosition.x()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_requestedScrollPosition.y()) << ")\n";
    }

    if (m_scrollOrigin != IntPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(scroll origin " << m_scrollOrigin.x() << " " << m_scrollOrigin.y() << ")\n";
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
