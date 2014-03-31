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

#ifndef ScrollingTreeScrollingNodeMac_h
#define ScrollingTreeScrollingNodeMac_h

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollElasticityController.h"
#include "ScrollbarThemeMac.h"
#include "ScrollingTreeScrollingNode.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;

namespace WebCore {

class ScrollingTreeScrollingNodeMac : public ScrollingTreeScrollingNode, private ScrollElasticityControllerClient {
public:
    static PassOwnPtr<ScrollingTreeScrollingNode> create(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);
    virtual ~ScrollingTreeScrollingNodeMac();

private:
    ScrollingTreeScrollingNodeMac(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);

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

    FloatPoint scrollPosition() const;
    virtual void setScrollPosition(const FloatPoint&) override;
    virtual void setScrollPositionWithoutContentEdgeConstraints(const FloatPoint&) override;

    virtual void updateLayersAfterViewportChange(const FloatRect& viewportRect, double scale) override;

    void setScrollLayerPosition(const FloatPoint&);

    FloatPoint minimumScrollPosition() const;
    FloatPoint maximumScrollPosition() const;

    void scrollBy(const IntSize&);
    void scrollByWithoutContentEdgeConstraints(const IntSize&);

    void updateMainFramePinState(const FloatPoint& scrollPosition);

    void logExposedUnfilledArea();

    ScrollElasticityController m_scrollElasticityController;
    RetainPtr<CFRunLoopTimerRef> m_snapRubberbandTimer;

    RetainPtr<CALayer> m_scrollLayer;
    RetainPtr<CALayer> m_scrolledContentsLayer;
    RetainPtr<CALayer> m_counterScrollingLayer;
    RetainPtr<CALayer> m_headerLayer;
    RetainPtr<CALayer> m_footerLayer;
    RetainPtr<ScrollbarPainter> m_verticalScrollbarPainter;
    RetainPtr<ScrollbarPainter> m_horizontalScrollbarPainter;
    FloatPoint m_probableMainThreadScrollPosition;
    bool m_lastScrollHadUnfilledPixels;
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingTreeScrollingNodeMac_h
