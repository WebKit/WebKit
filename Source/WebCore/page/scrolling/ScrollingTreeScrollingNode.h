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

#ifndef ScrollingTreeScrollingNode_h
#define ScrollingTreeScrollingNode_h

#if ENABLE(ASYNC_SCROLLING)

#include "IntRect.h"
#include "ScrollTypes.h"
#include "ScrollingCoordinator.h"
#include "ScrollingTreeNode.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class PlatformWheelEvent;
class ScrollingTree;
class ScrollingStateScrollingNode;

class ScrollingTreeScrollingNode : public ScrollingTreeNode {
public:
    virtual ~ScrollingTreeScrollingNode();

    virtual void updateBeforeChildren(const ScrollingStateNode&) override;

    // FIXME: We should implement this when we support ScrollingTreeScrollingNodes as children.
    virtual void parentScrollPositionDidChange(const FloatRect& /*viewportRect*/, const FloatSize& /*cumulativeDelta*/) override { }

    virtual void handleWheelEvent(const PlatformWheelEvent&) = 0;
    virtual void setScrollPosition(const FloatPoint&) = 0;
    virtual void setScrollPositionWithoutContentEdgeConstraints(const FloatPoint&) = 0;

    virtual void updateLayersAfterViewportChange(const FloatRect& viewportRect, double scale) = 0;
    virtual void updateLayersAfterDelegatedScroll(const FloatPoint&) { }

    SynchronousScrollingReasons synchronousScrollingReasons() const { return m_synchronousScrollingReasons; }
    bool shouldUpdateScrollLayerPositionSynchronously() const { return m_synchronousScrollingReasons; }

protected:
    ScrollingTreeScrollingNode(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);

    const FloatPoint& scrollPosition() const { return m_scrollPosition; }
    const FloatSize& viewportSize() const { return m_viewportSize; }
    const IntSize& totalContentsSize() const { return m_totalContentsSize; }
    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }

    // If the totalContentsSize changes in the middle of a rubber-band, we still want to use the old totalContentsSize for the sake of
    // computing the stretchAmount(). Using the old value will keep the animation smooth. When there is no rubber-band in progress at
    // all, m_totalContentsSizeForRubberBand should be equivalent to m_totalContentsSize.
    const IntSize& totalContentsSizeForRubberBand() const { return m_totalContentsSizeForRubberBand; }
    void setTotalContentsSizeForRubberBand(const IntSize& totalContentsSizeForRubberBand) { m_totalContentsSizeForRubberBand = totalContentsSizeForRubberBand; }

    float frameScaleFactor() const { return m_frameScaleFactor; }

    ScrollElasticity horizontalScrollElasticity() const { return m_scrollableAreaParameters.horizontalScrollElasticity; }
    ScrollElasticity verticalScrollElasticity() const { return m_scrollableAreaParameters.verticalScrollElasticity; }

    bool hasEnabledHorizontalScrollbar() const { return m_scrollableAreaParameters.hasEnabledHorizontalScrollbar; }
    bool hasEnabledVerticalScrollbar() const { return m_scrollableAreaParameters.hasEnabledVerticalScrollbar; }

    bool canHaveScrollbars() const { return m_scrollableAreaParameters.horizontalScrollbarMode != ScrollbarAlwaysOff || m_scrollableAreaParameters.verticalScrollbarMode != ScrollbarAlwaysOff; }

    int headerHeight() const { return m_headerHeight; }
    int footerHeight() const { return m_footerHeight; }

    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const { return m_behaviorForFixed; }

private:
    FloatSize m_viewportSize;
    IntSize m_totalContentsSize;
    IntSize m_totalContentsSizeForRubberBand;
    FloatPoint m_scrollPosition;
    IntPoint m_scrollOrigin;
    
    ScrollableAreaParameters m_scrollableAreaParameters;
    
    float m_frameScaleFactor;

    int m_headerHeight;
    int m_footerHeight;

    SynchronousScrollingReasons m_synchronousScrollingReasons;
    ScrollBehaviorForFixedElements m_behaviorForFixed;
};

SCROLLING_NODE_TYPE_CASTS(ScrollingTreeScrollingNode, isScrollingNode());

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingTreeScrollingNode_h
