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

#ifndef ScrollingTreeFrameScrollingNodeMac_h
#define ScrollingTreeFrameScrollingNodeMac_h

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#include "ScrollElasticityController.h"
#include "ScrollbarThemeMac.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;

namespace WebCore {

class ScrollingTreeFrameScrollingNodeMac : public ScrollingTreeFrameScrollingNode, private ScrollElasticityControllerClient {
public:
    static PassRefPtr<ScrollingTreeFrameScrollingNode> create(ScrollingTree&, ScrollingNodeID);
    virtual ~ScrollingTreeFrameScrollingNodeMac();

private:
    ScrollingTreeFrameScrollingNodeMac(ScrollingTree&, ScrollingNodeID);

    // ScrollingTreeNode member functions.
    virtual void updateBeforeChildren(const ScrollingStateNode&) override;
    virtual void updateAfterChildren(const ScrollingStateNode&) override;
    virtual void handleWheelEvent(const PlatformWheelEvent&) override;

    // ScrollElasticityController member functions.
    virtual bool allowsHorizontalStretching() override;
    virtual bool allowsVerticalStretching() override;
    virtual IntSize stretchAmount() override;
    virtual bool pinnedInDirection(const FloatSize&) override;
    virtual bool canScrollHorizontally() override;
    virtual bool canScrollVertically() override;
    virtual bool shouldRubberBandInDirection(ScrollDirection) override;
    virtual IntPoint absoluteScrollPosition() override;
    virtual void immediateScrollBy(const FloatSize&) override;
    virtual void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) override;
    virtual void startSnapRubberbandTimer() override;
    virtual void stopSnapRubberbandTimer() override;
    virtual void adjustScrollPositionToBoundsIfNecessary() override;

    virtual FloatPoint scrollPosition() const override;
    virtual void setScrollPosition(const FloatPoint&) override;
    virtual void setScrollPositionWithoutContentEdgeConstraints(const FloatPoint&) override;

    virtual void updateLayersAfterViewportChange(const FloatRect& fixedPositionRect, double scale) override;

    virtual void setScrollLayerPosition(const FloatPoint&) override;

    virtual FloatPoint minimumScrollPosition() const override;
    virtual FloatPoint maximumScrollPosition() const override;

    void updateMainFramePinState(const FloatPoint& scrollPosition);

    void logExposedUnfilledArea();

    ScrollElasticityController m_scrollElasticityController;
    RetainPtr<CFRunLoopTimerRef> m_snapRubberbandTimer;

    RetainPtr<CALayer> m_scrollLayer;
    RetainPtr<CALayer> m_scrolledContentsLayer;
    RetainPtr<CALayer> m_counterScrollingLayer;
    RetainPtr<CALayer> m_insetClipLayer;
    RetainPtr<CALayer> m_contentShadowLayer;
    RetainPtr<CALayer> m_headerLayer;
    RetainPtr<CALayer> m_footerLayer;
    RetainPtr<ScrollbarPainter> m_verticalScrollbarPainter;
    RetainPtr<ScrollbarPainter> m_horizontalScrollbarPainter;
    FloatPoint m_probableMainThreadScrollPosition;
    bool m_lastScrollHadUnfilledPixels;
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#endif // ScrollingTreeFrameScrollingNodeMac_h
