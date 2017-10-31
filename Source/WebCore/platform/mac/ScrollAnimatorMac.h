
/*
 * Copyright (C) 2010, 2011, 2014-2015 Apple Inc. All rights reserved.
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

#if ENABLE(SMOOTH_SCROLLING)

#include "IntRect.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "ScrollAnimator.h"
#include "Timer.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS WebScrollAnimationHelperDelegate;
OBJC_CLASS WebScrollerImpPairDelegate;
OBJC_CLASS WebScrollerImpDelegate;

typedef id ScrollerImpPair;

namespace WebCore {

class Scrollbar;

class ScrollAnimatorMac : public ScrollAnimator {

public:
    ScrollAnimatorMac(ScrollableArea&);
    virtual ~ScrollAnimatorMac();

    void immediateScrollToPositionForScrollAnimation(const FloatPoint& newPosition);
    bool haveScrolledSincePageLoad() const { return m_haveScrolledSincePageLoad; }

    void updateScrollerStyle();

    bool scrollbarPaintTimerIsActive() const;
    void startScrollbarPaintTimer();
    void stopScrollbarPaintTimer();

    void setVisibleScrollerThumbRect(const IntRect&);

    bool scrollbarsCanBeActive() const final;

private:
    RetainPtr<id> m_scrollAnimationHelper;
    RetainPtr<WebScrollAnimationHelperDelegate> m_scrollAnimationHelperDelegate;

    RetainPtr<ScrollerImpPair> m_scrollerImpPair;
    RetainPtr<WebScrollerImpPairDelegate> m_scrollerImpPairDelegate;
    RetainPtr<WebScrollerImpDelegate> m_horizontalScrollerImpDelegate;
    RetainPtr<WebScrollerImpDelegate> m_verticalScrollerImpDelegate;

    void initialScrollbarPaintTimerFired();
    Timer m_initialScrollbarPaintTimer;

    void sendContentAreaScrolledTimerFired();
    Timer m_sendContentAreaScrolledTimer;
    FloatSize m_contentAreaScrolledTimerScrollDelta;

    bool scroll(ScrollbarOrientation, ScrollGranularity, float step, float multiplier) override;
    void scrollToOffsetWithoutAnimation(const FloatPoint&, ScrollClamping) override;

#if ENABLE(RUBBER_BANDING)
    bool shouldForwardWheelEventsToParent(const PlatformWheelEvent&);
    bool handleWheelEvent(const PlatformWheelEvent&) override;
#endif

    void handleWheelEventPhase(PlatformWheelEventPhase) override;

    void cancelAnimations() override;
    
    void notifyPositionChanged(const FloatSize& delta) override;
    void contentAreaWillPaint() const override;
    void mouseEnteredContentArea() override;
    void mouseExitedContentArea() override;
    void mouseMovedInContentArea() override;
    void mouseEnteredScrollbar(Scrollbar*) const override;
    void mouseExitedScrollbar(Scrollbar*) const override;
    void mouseIsDownInScrollbar(Scrollbar*, bool) const override;
    void willStartLiveResize() override;
    void contentsResized() const override;
    void willEndLiveResize() override;
    void contentAreaDidShow() override;
    void contentAreaDidHide() override;
    void didBeginScrollGesture() const;
    void didEndScrollGesture() const;
    void mayBeginScrollGesture() const;

    void lockOverlayScrollbarStateToHidden(bool shouldLockState) final;

    void didAddVerticalScrollbar(Scrollbar*) override;
    void willRemoveVerticalScrollbar(Scrollbar*) override;
    void didAddHorizontalScrollbar(Scrollbar*) override;
    void willRemoveHorizontalScrollbar(Scrollbar*) override;

    void invalidateScrollbarPartLayers(Scrollbar*) override;

    void verticalScrollbarLayerDidChange() override;
    void horizontalScrollbarLayerDidChange() override;

    bool shouldScrollbarParticipateInHitTesting(Scrollbar*) override;

    void notifyContentAreaScrolled(const FloatSize& delta) override;

    // sendContentAreaScrolledSoon() will do the same work that sendContentAreaScrolled() does except
    // it does it after a zero-delay timer fires. This will prevent us from updating overlay scrollbar 
    // information during layout.
    void sendContentAreaScrolled(const FloatSize& scrollDelta);
    void sendContentAreaScrolledSoon(const FloatSize& scrollDelta);

    FloatPoint adjustScrollPositionIfNecessary(const FloatPoint&) const;

    void immediateScrollToPosition(const FloatPoint&, ScrollClamping = ScrollClamping::Clamped);

    bool isRubberBandInProgress() const override;
    bool isScrollSnapInProgress() const override;

#if ENABLE(RUBBER_BANDING)
    /// ScrollControllerClient member functions.
    IntSize stretchAmount() override;
    bool allowsHorizontalStretching(const PlatformWheelEvent&) override;
    bool allowsVerticalStretching(const PlatformWheelEvent&) override;
    bool pinnedInDirection(const FloatSize&) override;
    bool canScrollHorizontally() override;
    bool canScrollVertically() override;
    bool shouldRubberBandInDirection(ScrollDirection) override;
    void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) override;
    void immediateScrollBy(const FloatSize&) override;
    void adjustScrollPositionToBoundsIfNecessary() override;

    bool isAlreadyPinnedInDirectionOfGesture(const PlatformWheelEvent&, ScrollEventAxis);
#endif

    bool m_haveScrolledSincePageLoad;
    bool m_needsScrollerStyleUpdate;
    IntRect m_visibleScrollerThumbRect;
};

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)

