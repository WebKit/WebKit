/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatPoint.h"
#include "PlatformWheelEvent.h"
#include "ScrollTypes.h"
#include "WheelEventTestTrigger.h"
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>

#if ENABLE(RUBBER_BANDING) || ENABLE(CSS_SCROLL_SNAP)
#include "ScrollController.h"
#endif

namespace WebCore {

class FloatPoint;
class PlatformTouchEvent;
class ScrollableArea;
class Scrollbar;
class WheelEventTestTrigger;

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
class ScrollAnimator : private ScrollControllerClient {
#else
class ScrollAnimator {
#endif
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<ScrollAnimator> create(ScrollableArea&);

    explicit ScrollAnimator(ScrollableArea&);
    virtual ~ScrollAnimator();

    // Computes a scroll destination for the given parameters.  Returns false if
    // already at the destination.  Otherwise, starts scrolling towards the
    // destination and returns true.  Scrolling may be immediate or animated.
    // The base class implementation always scrolls immediately, never animates.
    virtual bool scroll(ScrollbarOrientation, ScrollGranularity, float step, float multiplier);

    virtual void scrollToOffsetWithoutAnimation(const FloatPoint&, ScrollClamping = ScrollClamping::Clamped);

    ScrollableArea& scrollableArea() const { return m_scrollableArea; }

    virtual bool handleWheelEvent(const PlatformWheelEvent&);

#if ENABLE(TOUCH_EVENTS)
    virtual bool handleTouchEvent(const PlatformTouchEvent&);
#endif

#if PLATFORM(COCOA)
    virtual void handleWheelEventPhase(PlatformWheelEventPhase) { }
#endif

    void setCurrentPosition(const FloatPoint&);
    const FloatPoint& currentPosition() const { return m_currentPosition; }

    virtual void cancelAnimations() { }
    virtual void serviceScrollAnimations() { }

    virtual void contentAreaWillPaint() const { }
    virtual void mouseEnteredContentArea() { }
    virtual void mouseExitedContentArea() { }
    virtual void mouseMovedInContentArea() { }
    virtual void mouseEnteredScrollbar(Scrollbar*) const { }
    virtual void mouseExitedScrollbar(Scrollbar*) const { }
    virtual void mouseIsDownInScrollbar(Scrollbar*, bool) const { }
    virtual void willStartLiveResize() { }
    virtual void contentsResized() const { }
    virtual void willEndLiveResize() { }
    virtual void contentAreaDidShow() { }
    virtual void contentAreaDidHide() { }

    virtual void lockOverlayScrollbarStateToHidden(bool) { }
    virtual bool scrollbarsCanBeActive() const { return true; }

    virtual void didAddVerticalScrollbar(Scrollbar*) { }
    virtual void willRemoveVerticalScrollbar(Scrollbar*) { }
    virtual void didAddHorizontalScrollbar(Scrollbar*) { }
    virtual void willRemoveHorizontalScrollbar(Scrollbar*) { }

    virtual void invalidateScrollbarPartLayers(Scrollbar*) { }

    virtual void verticalScrollbarLayerDidChange() { }
    virtual void horizontalScrollbarLayerDidChange() { }

    virtual bool shouldScrollbarParticipateInHitTesting(Scrollbar*) { return true; }

    virtual void notifyContentAreaScrolled(const FloatSize& delta) { UNUSED_PARAM(delta); }

    virtual bool isRubberBandInProgress() const { return false; }
    virtual bool isScrollSnapInProgress() const { return false; }

    void setWheelEventTestTrigger(RefPtr<WheelEventTestTrigger>&& testTrigger) { m_wheelEventTestTrigger = testTrigger; }
#if (ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)) && PLATFORM(MAC)
    void deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) const override;
    void removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) const override;
#endif
    
#if ENABLE(CSS_SCROLL_SNAP)
#if PLATFORM(MAC)
    bool processWheelEventForScrollSnap(const PlatformWheelEvent&);
#endif
    void updateScrollSnapState();
    FloatPoint scrollOffset() const override;
    void immediateScrollOnAxis(ScrollEventAxis, float delta) override;
    bool activeScrollSnapIndexDidChange() const;
    unsigned activeScrollSnapIndexForAxis(ScrollEventAxis) const;
    LayoutSize scrollExtent() const override;
    FloatSize viewportSize() const override;
#endif

protected:
    virtual void notifyPositionChanged(const FloatSize& delta);
    void updateActiveScrollSnapIndexForOffset();

    ScrollableArea& m_scrollableArea;
    RefPtr<WheelEventTestTrigger> m_wheelEventTestTrigger;
#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    ScrollController m_scrollController;
#endif
    FloatPoint m_currentPosition;
};

} // namespace WebCore

