
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

#ifndef ScrollAnimatorMac_h
#define ScrollAnimatorMac_h

#if ENABLE(SMOOTH_SCROLLING)

#include "IntRect.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "ScrollAnimator.h"
#include "Timer.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS WebScrollAnimationHelperDelegate;
OBJC_CLASS WebScrollbarPainterControllerDelegate;
OBJC_CLASS WebScrollbarPainterDelegate;

typedef id ScrollbarPainterController;

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

    virtual bool scrollbarsCanBeActive() const override final;

private:
    RetainPtr<id> m_scrollAnimationHelper;
    RetainPtr<WebScrollAnimationHelperDelegate> m_scrollAnimationHelperDelegate;

    RetainPtr<ScrollbarPainterController> m_scrollbarPainterController;
    RetainPtr<WebScrollbarPainterControllerDelegate> m_scrollbarPainterControllerDelegate;
    RetainPtr<WebScrollbarPainterDelegate> m_horizontalScrollbarPainterDelegate;
    RetainPtr<WebScrollbarPainterDelegate> m_verticalScrollbarPainterDelegate;

    void initialScrollbarPaintTimerFired();
    Timer m_initialScrollbarPaintTimer;

    void sendContentAreaScrolledTimerFired();
    Timer m_sendContentAreaScrolledTimer;
    FloatSize m_contentAreaScrolledTimerScrollDelta;

    virtual bool scroll(ScrollbarOrientation, ScrollGranularity, float step, float multiplier) override;
    virtual void scrollToOffsetWithoutAnimation(const FloatPoint&) override;

#if ENABLE(RUBBER_BANDING)
    bool shouldForwardWheelEventsToParent(const PlatformWheelEvent&);
    virtual bool handleWheelEvent(const PlatformWheelEvent&) override;
#endif

    virtual void handleWheelEventPhase(PlatformWheelEventPhase) override;

    virtual void cancelAnimations() override;
    
    virtual void notifyPositionChanged(const FloatSize& delta) override;
    virtual void contentAreaWillPaint() const override;
    virtual void mouseEnteredContentArea() const override;
    virtual void mouseExitedContentArea() const override;
    virtual void mouseMovedInContentArea() const override;
    virtual void mouseEnteredScrollbar(Scrollbar*) const override;
    virtual void mouseExitedScrollbar(Scrollbar*) const override;
    virtual void mouseIsDownInScrollbar(Scrollbar*, bool) const override;
    virtual void willStartLiveResize() override;
    virtual void contentsResized() const override;
    virtual void willEndLiveResize() override;
    virtual void contentAreaDidShow() const override;
    virtual void contentAreaDidHide() const override;
    void didBeginScrollGesture() const;
    void didEndScrollGesture() const;
    void mayBeginScrollGesture() const;

    virtual void lockOverlayScrollbarStateToHidden(bool shouldLockState) override final;

    virtual void didAddVerticalScrollbar(Scrollbar*) override;
    virtual void willRemoveVerticalScrollbar(Scrollbar*) override;
    virtual void didAddHorizontalScrollbar(Scrollbar*) override;
    virtual void willRemoveHorizontalScrollbar(Scrollbar*) override;

    void invalidateScrollbarPartLayers(Scrollbar*) override;

    virtual void verticalScrollbarLayerDidChange() override;
    virtual void horizontalScrollbarLayerDidChange() override;

    virtual bool shouldScrollbarParticipateInHitTesting(Scrollbar*) override;

    virtual void notifyContentAreaScrolled(const FloatSize& delta) override;

    // sendContentAreaScrolledSoon() will do the same work that sendContentAreaScrolled() does except
    // it does it after a zero-delay timer fires. This will prevent us from updating overlay scrollbar 
    // information during layout.
    void sendContentAreaScrolled(const FloatSize& scrollDelta);
    void sendContentAreaScrolledSoon(const FloatSize& scrollDelta);

    FloatPoint adjustScrollPositionIfNecessary(const FloatPoint&) const;

    void immediateScrollToPosition(const FloatPoint&);

    bool isRubberBandInProgress() const override;
    bool isScrollSnapInProgress() const override;

#if ENABLE(RUBBER_BANDING)
    /// ScrollControllerClient member functions.
    virtual IntSize stretchAmount() override;
    virtual bool allowsHorizontalStretching(const PlatformWheelEvent&) override;
    virtual bool allowsVerticalStretching(const PlatformWheelEvent&) override;
    virtual bool pinnedInDirection(const FloatSize&) override;
    virtual bool canScrollHorizontally() override;
    virtual bool canScrollVertically() override;
    virtual bool shouldRubberBandInDirection(ScrollDirection) override;
    virtual void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) override;
    virtual void immediateScrollBy(const FloatSize&) override;
    virtual void adjustScrollPositionToBoundsIfNecessary() override;

    bool isAlreadyPinnedInDirectionOfGesture(const PlatformWheelEvent&, ScrollEventAxis);
#endif

    bool m_haveScrolledSincePageLoad;
    bool m_needsScrollerStyleUpdate;
    IntRect m_visibleScrollerThumbRect;
};

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)

#endif // ScrollAnimatorMac_h
