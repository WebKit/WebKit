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

#ifndef ScrollingStateFrameScrollingNode_h
#define ScrollingStateFrameScrollingNode_h

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "Region.h"
#include "ScrollTypes.h"
#include "ScrollbarThemeComposite.h"
#include "ScrollingCoordinator.h"
#include "ScrollingStateScrollingNode.h"

namespace WebCore {

class Scrollbar;

class ScrollingStateFrameScrollingNode final : public ScrollingStateScrollingNode {
public:
    static PassRefPtr<ScrollingStateFrameScrollingNode> create(ScrollingStateTree&, ScrollingNodeID);

    virtual PassRefPtr<ScrollingStateNode> clone(ScrollingStateTree&);

    virtual ~ScrollingStateFrameScrollingNode();

    enum ChangedProperty {
        FrameScaleFactor = NumScrollingStateNodeBits,
        NonFastScrollableRegion,
        WheelEventHandlerCount,
        ReasonsForSynchronousScrolling,
        ScrolledContentsLayer,
        CounterScrollingLayer,
        InsetClipLayer,
        ContentShadowLayer,
        HeaderHeight,
        FooterHeight,
        HeaderLayer,
        FooterLayer,
        PainterForScrollbar,
        BehaviorForFixedElements,
        TopContentInset
    };

    float frameScaleFactor() const { return m_frameScaleFactor; }
    void setFrameScaleFactor(float);

    const Region& nonFastScrollableRegion() const { return m_nonFastScrollableRegion; }
    void setNonFastScrollableRegion(const Region&);

    unsigned wheelEventHandlerCount() const { return m_wheelEventHandlerCount; }
    void setWheelEventHandlerCount(unsigned);

    SynchronousScrollingReasons synchronousScrollingReasons() const { return m_synchronousScrollingReasons; }
    void setSynchronousScrollingReasons(SynchronousScrollingReasons);

    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const { return m_behaviorForFixed; }
    void setScrollBehaviorForFixedElements(ScrollBehaviorForFixedElements);

    int headerHeight() const { return m_headerHeight; }
    void setHeaderHeight(int);

    int footerHeight() const { return m_footerHeight; }
    void setFooterHeight(int);

    float topContentInset() const { return m_topContentInset; }
    void setTopContentInset(float);

    const LayerRepresentation& scrolledContentsLayer() const { return m_scrolledContentsLayer; }
    void setScrolledContentsLayer(const LayerRepresentation&);

    // This is a layer moved in the opposite direction to scrolling, for example for background-attachment:fixed
    const LayerRepresentation& counterScrollingLayer() const { return m_counterScrollingLayer; }
    void setCounterScrollingLayer(const LayerRepresentation&);

    // This is a clipping layer that will scroll with the page for all y-delta scroll values between 0
    // and topContentInset(). Once the y-deltas get beyond the content inset point, this layer no longer
    // needs to move. If the topContentInset() is 0, this layer does not need to move at all. This is
    // only used on the Mac.
    const LayerRepresentation& insetClipLayer() const { return m_insetClipLayer; }
    void setInsetClipLayer(const LayerRepresentation&);

    const LayerRepresentation& contentShadowLayer() const { return m_contentShadowLayer; }
    void setContentShadowLayer(const LayerRepresentation&);

    // The header and footer layers scroll vertically with the page, they should remain fixed when scrolling horizontally.
    const LayerRepresentation& headerLayer() const { return m_headerLayer; }
    void setHeaderLayer(const LayerRepresentation&);

    // The header and footer layers scroll vertically with the page, they should remain fixed when scrolling horizontally.
    const LayerRepresentation& footerLayer() const { return m_footerLayer; }
    void setFooterLayer(const LayerRepresentation&);

#if PLATFORM(MAC)
    ScrollbarPainter verticalScrollbarPainter() const { return m_verticalScrollbarPainter.get(); }
    ScrollbarPainter horizontalScrollbarPainter() const { return m_horizontalScrollbarPainter.get(); }
#endif
    void setScrollbarPaintersFromScrollbars(Scrollbar* verticalScrollbar, Scrollbar* horizontalScrollbar);

    virtual void dumpProperties(TextStream&, int indent) const override;

private:
    ScrollingStateFrameScrollingNode(ScrollingStateTree&, ScrollingNodeID);
    ScrollingStateFrameScrollingNode(const ScrollingStateFrameScrollingNode&, ScrollingStateTree&);

    LayerRepresentation m_counterScrollingLayer;
    LayerRepresentation m_insetClipLayer;
    LayerRepresentation m_scrolledContentsLayer;
    LayerRepresentation m_contentShadowLayer;
    LayerRepresentation m_headerLayer;
    LayerRepresentation m_footerLayer;

#if PLATFORM(MAC)
    RetainPtr<ScrollbarPainter> m_verticalScrollbarPainter;
    RetainPtr<ScrollbarPainter> m_horizontalScrollbarPainter;
#endif

    Region m_nonFastScrollableRegion;
    float m_frameScaleFactor;
    unsigned m_wheelEventHandlerCount;
    SynchronousScrollingReasons m_synchronousScrollingReasons;
    ScrollBehaviorForFixedElements m_behaviorForFixed;
    int m_headerHeight;
    int m_footerHeight;
    FloatPoint m_requestedScrollPosition;
    bool m_requestedScrollPositionRepresentsProgrammaticScroll;
    float m_topContentInset;
};

SCROLLING_STATE_NODE_TYPE_CASTS(ScrollingStateFrameScrollingNode, isFrameScrollingNode());

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#endif // ScrollingStateFrameScrollingNode_h
