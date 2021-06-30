/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "ScrollingTreeScrollingNodeDelegate.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#include "ScrollController.h"
#include <wtf/RunLoop.h>

OBJC_CLASS NSScrollerImp;

namespace WebCore {

class FloatPoint;
class FloatSize;
class IntPoint;
class ScrollingStateScrollingNode;
class ScrollingTreeScrollingNode;
class ScrollingTree;

class ScrollingTreeScrollingNodeDelegateMac : public ScrollingTreeScrollingNodeDelegate, private ScrollControllerClient {
public:
    explicit ScrollingTreeScrollingNodeDelegateMac(ScrollingTreeScrollingNode&);
    virtual ~ScrollingTreeScrollingNodeDelegateMac();

    void nodeWillBeDestroyed();

    bool handleWheelEvent(const PlatformWheelEvent&);
    
    void willDoProgrammaticScroll(const FloatPoint&);
    void currentScrollPositionChanged();

    bool activeScrollSnapIndexDidChange() const;
    std::optional<unsigned> activeScrollSnapIndexForAxis(ScrollEventAxis) const;
    bool isScrollSnapInProgress() const;

    bool isRubberBandInProgress() const;

    void updateFromStateNode(const ScrollingStateScrollingNode&);
    void updateScrollbarPainters();

    void deferWheelEventTestCompletionForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason) const override;
    void removeWheelEventTestCompletionDeferralForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason) const override;

private:
    bool isPinnedForScrollDeltaOnAxis(float scrollDelta, ScrollEventAxis, float scrollLimit = 0) const;

    // ScrollControllerClient.
    std::unique_ptr<ScrollControllerTimer> createTimer(Function<void()>&&) final;
    void startAnimationCallback(ScrollController&) final;
    void stopAnimationCallback(ScrollController&) final;

    bool allowsHorizontalStretching(const PlatformWheelEvent&) const final;
    bool allowsVerticalStretching(const PlatformWheelEvent&) const final;
    IntSize stretchAmount() const final;
    bool isPinnedForScrollDelta(const FloatSize&) const final;
    RectEdges<bool> edgePinnedState() const final;
    bool allowsHorizontalScrolling() const final;
    bool allowsVerticalScrolling() const final;
    bool shouldRubberBandInDirection(ScrollDirection) const final;
    void immediateScrollBy(const FloatSize&) final;
    void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) final;
    void didStopRubberbandSnapAnimation() final;
    void rubberBandingStateChanged(bool) final;
    void adjustScrollPositionToBoundsIfNecessary() final;

    bool scrollPositionIsNotRubberbandingEdge(const FloatPoint&) const;
    void scrollControllerAnimationTimerFired();

    FloatPoint scrollOffset() const override;
    void immediateScrollOnAxis(ScrollEventAxis, float delta) override;
    float pageScaleFactor() const override;
    void willStartScrollSnapAnimation() final;
    void didStopScrollSnapAnimation() final;
    LayoutSize scrollExtent() const override;
    FloatSize viewportSize() const override;

    void releaseReferencesToScrollerImpsOnTheMainThread();

    ScrollController m_scrollController;

    RetainPtr<NSScrollerImp> m_verticalScrollerImp;
    RetainPtr<NSScrollerImp> m_horizontalScrollerImp;

    std::unique_ptr<RunLoop::Timer<ScrollingTreeScrollingNodeDelegateMac>> m_scrollControllerAnimationTimer;

    bool m_inMomentumPhase { false };
};

} // namespace WebCore

#endif // PLATFORM(MAC) && ENABLE(ASYNC_SCROLLING)
