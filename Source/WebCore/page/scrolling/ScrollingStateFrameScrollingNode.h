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

#pragma once

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "EventTrackingRegions.h"
#include "ScrollTypes.h"
#include "ScrollbarThemeComposite.h"
#include "ScrollingCoordinator.h"
#include "ScrollingStateScrollingNode.h"

namespace WebCore {

class Scrollbar;

class ScrollingStateFrameScrollingNode final : public ScrollingStateScrollingNode {
public:
    static Ref<ScrollingStateFrameScrollingNode> create(ScrollingStateTree&, ScrollingNodeType, ScrollingNodeID);

    Ref<ScrollingStateNode> clone(ScrollingStateTree&) override;

    virtual ~ScrollingStateFrameScrollingNode();

    enum ChangedProperty {
        FrameScaleFactor = NumScrollingStateNodeBits,
        EventTrackingRegion,
        ReasonsForSynchronousScrolling,
        ScrolledContentsLayer,
        CounterScrollingLayer,
        InsetClipLayer,
        ContentShadowLayer,
        HeaderHeight,
        FooterHeight,
        HeaderLayer,
        FooterLayer,
        VerticalScrollbarLayer,
        HorizontalScrollbarLayer,
        PainterForScrollbar,
        BehaviorForFixedElements,
        TopContentInset,
        FixedElementsLayoutRelativeToFrame,
        VisualViewportEnabled,
        AsyncFrameOrOverflowScrollingEnabled,
        LayoutViewport,
        MinLayoutViewportOrigin,
        MaxLayoutViewportOrigin,
    };

    float frameScaleFactor() const { return m_frameScaleFactor; }
    WEBCORE_EXPORT void setFrameScaleFactor(float);

    const EventTrackingRegions& eventTrackingRegions() const { return m_eventTrackingRegions; }
    WEBCORE_EXPORT void setEventTrackingRegions(const EventTrackingRegions&);

    SynchronousScrollingReasons synchronousScrollingReasons() const { return m_synchronousScrollingReasons; }
    WEBCORE_EXPORT void setSynchronousScrollingReasons(SynchronousScrollingReasons);

    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const { return m_behaviorForFixed; }
    WEBCORE_EXPORT void setScrollBehaviorForFixedElements(ScrollBehaviorForFixedElements);

    FloatRect layoutViewport() const { return m_layoutViewport; };
    WEBCORE_EXPORT void setLayoutViewport(const FloatRect&);

    FloatPoint minLayoutViewportOrigin() const { return m_minLayoutViewportOrigin; }
    WEBCORE_EXPORT void setMinLayoutViewportOrigin(const FloatPoint&);

    FloatPoint maxLayoutViewportOrigin() const { return m_maxLayoutViewportOrigin; }
    WEBCORE_EXPORT void setMaxLayoutViewportOrigin(const FloatPoint&);

    int headerHeight() const { return m_headerHeight; }
    WEBCORE_EXPORT void setHeaderHeight(int);

    int footerHeight() const { return m_footerHeight; }
    WEBCORE_EXPORT void setFooterHeight(int);

    float topContentInset() const { return m_topContentInset; }
    WEBCORE_EXPORT void setTopContentInset(float);

    // This is a layer moved in the opposite direction to scrolling, for example for background-attachment:fixed
    const LayerRepresentation& counterScrollingLayer() const { return m_counterScrollingLayer; }
    WEBCORE_EXPORT void setCounterScrollingLayer(const LayerRepresentation&);

    // This is a clipping layer that will scroll with the page for all y-delta scroll values between 0
    // and topContentInset(). Once the y-deltas get beyond the content inset point, this layer no longer
    // needs to move. If the topContentInset() is 0, this layer does not need to move at all. This is
    // only used on the Mac.
    const LayerRepresentation& insetClipLayer() const { return m_insetClipLayer; }
    WEBCORE_EXPORT void setInsetClipLayer(const LayerRepresentation&);

    const LayerRepresentation& contentShadowLayer() const { return m_contentShadowLayer; }
    WEBCORE_EXPORT void setContentShadowLayer(const LayerRepresentation&);

    // The header and footer layers scroll vertically with the page, they should remain fixed when scrolling horizontally.
    const LayerRepresentation& headerLayer() const { return m_headerLayer; }
    WEBCORE_EXPORT void setHeaderLayer(const LayerRepresentation&);

    // The header and footer layers scroll vertically with the page, they should remain fixed when scrolling horizontally.
    const LayerRepresentation& footerLayer() const { return m_footerLayer; }
    WEBCORE_EXPORT void setFooterLayer(const LayerRepresentation&);

    const LayerRepresentation& verticalScrollbarLayer() const { return m_verticalScrollbarLayer; }
    WEBCORE_EXPORT void setVerticalScrollbarLayer(const LayerRepresentation&);

    const LayerRepresentation& horizontalScrollbarLayer() const { return m_horizontalScrollbarLayer; }
    WEBCORE_EXPORT void setHorizontalScrollbarLayer(const LayerRepresentation&);

    // These are more like Settings, and should probably move to the Scrolling{State}Tree itself.
    bool fixedElementsLayoutRelativeToFrame() const { return m_fixedElementsLayoutRelativeToFrame; }
    WEBCORE_EXPORT void setFixedElementsLayoutRelativeToFrame(bool);

    bool visualViewportEnabled() const { return m_visualViewportEnabled; };
    WEBCORE_EXPORT void setVisualViewportEnabled(bool);

    bool asyncFrameOrOverflowScrollingEnabled() const { return m_asyncFrameOrOverflowScrollingEnabled; }
    void setAsyncFrameOrOverflowScrollingEnabled(bool);

#if PLATFORM(MAC)
    NSScrollerImp *verticalScrollerImp() const { return m_verticalScrollerImp.get(); }
    NSScrollerImp *horizontalScrollerImp() const { return m_horizontalScrollerImp.get(); }
#endif
    void setScrollerImpsFromScrollbars(Scrollbar* verticalScrollbar, Scrollbar* horizontalScrollbar);

    void dumpProperties(WTF::TextStream&, ScrollingStateTreeAsTextBehavior) const override;

private:
    ScrollingStateFrameScrollingNode(ScrollingStateTree&, ScrollingNodeType, ScrollingNodeID);
    ScrollingStateFrameScrollingNode(const ScrollingStateFrameScrollingNode&, ScrollingStateTree&);

    void setAllPropertiesChanged() override;

    LayerRepresentation m_counterScrollingLayer;
    LayerRepresentation m_insetClipLayer;
    LayerRepresentation m_contentShadowLayer;
    LayerRepresentation m_headerLayer;
    LayerRepresentation m_footerLayer;
    LayerRepresentation m_verticalScrollbarLayer;
    LayerRepresentation m_horizontalScrollbarLayer;

#if PLATFORM(MAC)
    RetainPtr<NSScrollerImp> m_verticalScrollerImp;
    RetainPtr<NSScrollerImp> m_horizontalScrollerImp;
#endif

    EventTrackingRegions m_eventTrackingRegions;
    FloatPoint m_requestedScrollPosition;

    FloatRect m_layoutViewport;
    FloatPoint m_minLayoutViewportOrigin;
    FloatPoint m_maxLayoutViewportOrigin;

    float m_frameScaleFactor { 1 };
    float m_topContentInset { 0 };
    int m_headerHeight { 0 };
    int m_footerHeight { 0 };
    SynchronousScrollingReasons m_synchronousScrollingReasons { 0 };
    ScrollBehaviorForFixedElements m_behaviorForFixed { StickToDocumentBounds };
    bool m_requestedScrollPositionRepresentsProgrammaticScroll { false };
    bool m_fixedElementsLayoutRelativeToFrame { false };
    bool m_visualViewportEnabled { false };
    bool m_asyncFrameOrOverflowScrollingEnabled { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_STATE_NODE(ScrollingStateFrameScrollingNode, isFrameScrollingNode())

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
