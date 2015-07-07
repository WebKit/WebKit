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

class ScrollingTree;
class ScrollingStateScrollingNode;

class ScrollingTreeScrollingNode : public ScrollingTreeNode {
public:
    virtual ~ScrollingTreeScrollingNode();

    virtual void updateBeforeChildren(const ScrollingStateNode&) override;
    virtual void updateAfterChildren(const ScrollingStateNode&) override;

    virtual void updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta) override;

    virtual void handleWheelEvent(const PlatformWheelEvent&) = 0;
    virtual void setScrollPosition(const FloatPoint&);
    virtual void setScrollPositionWithoutContentEdgeConstraints(const FloatPoint&);

    virtual void updateLayersAfterViewportChange(const FloatRect& fixedPositionRect, double scale) = 0;
    virtual void updateLayersAfterDelegatedScroll(const FloatPoint&) { }

    virtual FloatPoint scrollPosition() const = 0;

protected:
    ScrollingTreeScrollingNode(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);

    virtual FloatPoint minimumScrollPosition() const;
    virtual FloatPoint maximumScrollPosition() const;

    virtual void setScrollLayerPosition(const FloatPoint&) = 0;

    FloatPoint lastCommittedScrollPosition() const { return m_lastCommittedScrollPosition; }
    const FloatSize& scrollableAreaSize() const { return m_scrollableAreaSize; }
    const FloatSize& totalContentsSize() const { return m_totalContentsSize; }
    const FloatSize& reachableContentsSize() const { return m_reachableContentsSize; }
    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }

    // If the totalContentsSize changes in the middle of a rubber-band, we still want to use the old totalContentsSize for the sake of
    // computing the stretchAmount(). Using the old value will keep the animation smooth. When there is no rubber-band in progress at
    // all, m_totalContentsSizeForRubberBand should be equivalent to m_totalContentsSize.
    const FloatSize& totalContentsSizeForRubberBand() const { return m_totalContentsSizeForRubberBand; }
    void setTotalContentsSizeForRubberBand(const FloatSize& totalContentsSizeForRubberBand) { m_totalContentsSizeForRubberBand = totalContentsSizeForRubberBand; }

    ScrollElasticity horizontalScrollElasticity() const { return m_scrollableAreaParameters.horizontalScrollElasticity; }
    ScrollElasticity verticalScrollElasticity() const { return m_scrollableAreaParameters.verticalScrollElasticity; }

    bool hasEnabledHorizontalScrollbar() const { return m_scrollableAreaParameters.hasEnabledHorizontalScrollbar; }
    bool hasEnabledVerticalScrollbar() const { return m_scrollableAreaParameters.hasEnabledVerticalScrollbar; }

    bool canHaveScrollbars() const { return m_scrollableAreaParameters.horizontalScrollbarMode != ScrollbarAlwaysOff || m_scrollableAreaParameters.verticalScrollbarMode != ScrollbarAlwaysOff; }

private:
    FloatSize m_scrollableAreaSize;
    FloatSize m_totalContentsSize;
    FloatSize m_totalContentsSizeForRubberBand;
    FloatSize m_reachableContentsSize;
    FloatPoint m_lastCommittedScrollPosition;
    IntPoint m_scrollOrigin;
    
    ScrollableAreaParameters m_scrollableAreaParameters;
};

SCROLLING_NODE_TYPE_CASTS(ScrollingTreeScrollingNode, isScrollingNode());

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingTreeScrollingNode_h
