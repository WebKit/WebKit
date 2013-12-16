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

#if ENABLE(THREADED_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "IntRect.h"
#include "Region.h"
#include "ScrollTypes.h"
#include "ScrollbarThemeComposite.h"
#include "ScrollingCoordinator.h"
#include "ScrollingStateNode.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class Scrollbar;

class ScrollingStateScrollingNode FINAL : public ScrollingStateNode {
public:
    static PassOwnPtr<ScrollingStateScrollingNode> create(ScrollingStateTree*, ScrollingNodeID);

    virtual PassOwnPtr<ScrollingStateNode> clone();

    virtual ~ScrollingStateScrollingNode();

    enum ChangedProperty {
        ViewportRect = NumStateNodeBits,
        TotalContentsSize,
        ScrollOrigin,
        ScrollableAreaParams,
        FrameScaleFactor,
        NonFastScrollableRegion,
        WheelEventHandlerCount,
        ShouldUpdateScrollLayerPositionOnMainThread,
        RequestedScrollPosition,
        CounterScrollingLayer,
        HeaderHeight,
        FooterHeight,
        HeaderLayer,
        FooterLayer,
        PainterForScrollbar,
        BehaviorForFixedElements
    };

    const IntRect& viewportRect() const { return m_viewportRect; }
    void setViewportRect(const IntRect&);

    const IntSize& totalContentsSize() const { return m_totalContentsSize; }
    void setTotalContentsSize(const IntSize&);

    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }
    void setScrollOrigin(const IntPoint&);

    float frameScaleFactor() const { return m_frameScaleFactor; }
    void setFrameScaleFactor(float);

    const Region& nonFastScrollableRegion() const { return m_nonFastScrollableRegion; }
    void setNonFastScrollableRegion(const Region&);

    unsigned wheelEventHandlerCount() const { return m_wheelEventHandlerCount; }
    void setWheelEventHandlerCount(unsigned);

    MainThreadScrollingReasons shouldUpdateScrollLayerPositionOnMainThread() const { return m_shouldUpdateScrollLayerPositionOnMainThread; }
    void setShouldUpdateScrollLayerPositionOnMainThread(MainThreadScrollingReasons);

    const ScrollableAreaParameters& scrollableAreaParameters() const { return m_scrollableAreaParameters; }
    void setScrollableAreaParameters(const ScrollableAreaParameters& params);

    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const { return m_behaviorForFixed; }
    void setScrollBehaviorForFixedElements(ScrollBehaviorForFixedElements);

    const IntPoint& requestedScrollPosition() const { return m_requestedScrollPosition; }
    void setRequestedScrollPosition(const IntPoint&, bool representsProgrammaticScroll);

    int headerHeight() const { return m_headerHeight; }
    void setHeaderHeight(int);

    int footerHeight() const { return m_footerHeight; }
    void setFooterHeight(int);

    // This is a layer moved in the opposite direction to scrolling, for example for background-attachment:fixed
    GraphicsLayer* counterScrollingLayer() const { return m_counterScrollingLayer; }
    void setCounterScrollingLayer(GraphicsLayer*);
    PlatformLayer* counterScrollingPlatformLayer() const;

    // The header and footer layers scroll vertically with the page, they should remain fixed when scrolling horizontally.
    GraphicsLayer* headerLayer() const { return m_headerLayer; }
    void setHeaderLayer(GraphicsLayer*);
    PlatformLayer* headerPlatformLayer() const;

    // The header and footer layers scroll vertically with the page, they should remain fixed when scrolling horizontally.
    GraphicsLayer* footerLayer() const { return m_footerLayer; }
    void setFooterLayer(GraphicsLayer*);
    PlatformLayer* footerPlatformLayer() const;

#if PLATFORM(MAC)
    ScrollbarPainter verticalScrollbarPainter() const { return m_verticalScrollbarPainter; }
    ScrollbarPainter horizontalScrollbarPainter() const { return m_horizontalScrollbarPainter; }
#endif
    void setScrollbarPaintersFromScrollbars(Scrollbar* verticalScrollbar, Scrollbar* horizontalScrollbar);

    bool requestedScrollPositionRepresentsProgrammaticScroll() const { return m_requestedScrollPositionRepresentsProgrammaticScroll; }

    virtual void dumpProperties(TextStream&, int indent) const OVERRIDE;

private:
    ScrollingStateScrollingNode(ScrollingStateTree*, ScrollingNodeID);
    ScrollingStateScrollingNode(const ScrollingStateScrollingNode&);

    virtual bool isScrollingNode() const OVERRIDE { return true; }

    GraphicsLayer* m_counterScrollingLayer;
    GraphicsLayer* m_headerLayer;
    GraphicsLayer* m_footerLayer;
#if PLATFORM(MAC)
    RetainPtr<PlatformLayer> m_counterScrollingPlatformLayer;
    RetainPtr<PlatformLayer> m_headerPlatformLayer;
    RetainPtr<PlatformLayer> m_footerPlatformLayer;
    ScrollbarPainter m_verticalScrollbarPainter;
    ScrollbarPainter m_horizontalScrollbarPainter;
#endif

    IntRect m_viewportRect;
    IntSize m_totalContentsSize;
    IntPoint m_scrollOrigin;
    
    ScrollableAreaParameters m_scrollableAreaParameters;
    Region m_nonFastScrollableRegion;
    float m_frameScaleFactor;
    unsigned m_wheelEventHandlerCount;
    MainThreadScrollingReasons m_shouldUpdateScrollLayerPositionOnMainThread;
    ScrollBehaviorForFixedElements m_behaviorForFixed;
    int m_headerHeight;
    int m_footerHeight;
    IntPoint m_requestedScrollPosition;
    bool m_requestedScrollPositionRepresentsProgrammaticScroll;
};

SCROLLING_STATE_NODE_TYPE_CASTS(ScrollingStateScrollingNode, isScrollingNode());

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING) || USE(COORDINATED_GRAPHICS)

#endif // ScrollingStateScrollingNode_h
