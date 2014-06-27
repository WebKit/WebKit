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

#ifndef ScrollingStateScrollingNode_h
#define ScrollingStateScrollingNode_h

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "ScrollTypes.h"
#include "ScrollingCoordinator.h"
#include "ScrollingStateNode.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class ScrollingStateScrollingNode : public ScrollingStateNode {
public:
    virtual ~ScrollingStateScrollingNode();

    enum ChangedProperty {
        ScrollableAreaSize = NumStateNodeBits,
        TotalContentsSize,
        ReachableContentsSize,
        ScrollPosition,
        ScrollOrigin,
        ScrollableAreaParams,
        RequestedScrollPosition,
        NumScrollingStateNodeBits,
    };

    const FloatSize& scrollableAreaSize() const { return m_scrollableAreaSize; }
    void setScrollableAreaSize(const FloatSize&);

    const FloatSize& totalContentsSize() const { return m_totalContentsSize; }
    void setTotalContentsSize(const FloatSize&);

    const FloatSize& reachableContentsSize() const { return m_reachableContentsSize; }
    void setReachableContentsSize(const FloatSize&);

    const FloatPoint& scrollPosition() const { return m_scrollPosition; }
    void setScrollPosition(const FloatPoint&);

    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }
    void setScrollOrigin(const IntPoint&);

    const ScrollableAreaParameters& scrollableAreaParameters() const { return m_scrollableAreaParameters; }
    void setScrollableAreaParameters(const ScrollableAreaParameters& params);

    const FloatPoint& requestedScrollPosition() const { return m_requestedScrollPosition; }
    bool requestedScrollPositionRepresentsProgrammaticScroll() const { return m_requestedScrollPositionRepresentsProgrammaticScroll; }
    void setRequestedScrollPosition(const FloatPoint&, bool representsProgrammaticScroll);
    
    virtual void dumpProperties(TextStream&, int indent) const override;
    
protected:
    ScrollingStateScrollingNode(ScrollingStateTree&, ScrollingNodeType, ScrollingNodeID);
    ScrollingStateScrollingNode(const ScrollingStateScrollingNode&, ScrollingStateTree&);
    
private:
    FloatSize m_scrollableAreaSize;
    FloatSize m_totalContentsSize;
    FloatSize m_reachableContentsSize;
    FloatPoint m_scrollPosition;
    FloatPoint m_requestedScrollPosition;
    IntPoint m_scrollOrigin;
    ScrollableAreaParameters m_scrollableAreaParameters;
    bool m_requestedScrollPositionRepresentsProgrammaticScroll;
};

SCROLLING_STATE_NODE_TYPE_CASTS(ScrollingStateScrollingNode, isScrollingNode());

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#endif // ScrollingStateScrollingNode_h
