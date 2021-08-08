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
#include "ScrollController.h"
#include "Timer.h"
#include "WheelEventTestMonitor.h"
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>

namespace WebCore {

class FloatPoint;
class KeyboardScrollingAnimator;
class PlatformTouchEvent;
class ScrollAnimation;
class ScrollableArea;
class Scrollbar;
class WheelEventTestMonitor;

class ScrollControllerTimer;

class ScrollAnimator : private ScrollControllerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<ScrollAnimator> create(ScrollableArea&);

    explicit ScrollAnimator(ScrollableArea&);
    virtual ~ScrollAnimator();

    enum ScrollBehavior {
        DoDirectionalSnapping = 1 << 0,
        NeverAnimate = 1 << 1,
    };

    // Computes a scroll destination for the given parameters.  Returns false if
    // already at the destination.  Otherwise, starts scrolling towards the
    // destination and returns true.  Scrolling may be immediate or animated.
    // The base class implementation always scrolls immediately, never animates.
    virtual bool scroll(ScrollbarOrientation, ScrollGranularity, float step, float multiplier, OptionSet<ScrollBehavior>);

    bool scrollToOffsetWithoutAnimation(const FloatPoint&, ScrollClamping = ScrollClamping::Clamped);
    virtual bool scrollToPositionWithoutAnimation(const FloatPoint& position, ScrollClamping = ScrollClamping::Clamped);
    virtual void retargetRunningAnimation(const FloatPoint&);

    bool scrollToOffsetWithAnimation(const FloatPoint&);
    virtual bool scrollToPositionWithAnimation(const FloatPoint&);

    ScrollableArea& scrollableArea() const { return m_scrollableArea; }

    virtual bool handleWheelEvent(const PlatformWheelEvent&);

    KeyboardScrollingAnimator *keyboardScrollingAnimator() const override { return m_keyboardScrollingAnimator.get(); }

#if ENABLE(TOUCH_EVENTS)
    virtual bool handleTouchEvent(const PlatformTouchEvent&);
#endif

#if PLATFORM(COCOA)
    virtual void handleWheelEventPhase(PlatformWheelEventPhase) { }
#endif

    void setCurrentPosition(const FloatPoint&);
    const FloatPoint& currentPosition() const { return m_currentPosition; }

    virtual void cancelAnimations();

    virtual void contentAreaWillPaint() const { }
    virtual void mouseEnteredContentArea() { }
    virtual void mouseExitedContentArea() { }
    virtual void mouseMovedInContentArea() { }
    virtual void mouseEnteredScrollbar(Scrollbar*) const { }
    virtual void mouseExitedScrollbar(Scrollbar*) const { }
    virtual void mouseIsDownInScrollbar(Scrollbar*, bool) const { }
    virtual void willStartLiveResize() { }
    virtual void contentsResized() const;
    virtual void willEndLiveResize();
    virtual void contentAreaDidShow() { }
    virtual void contentAreaDidHide() { }

    virtual void lockOverlayScrollbarStateToHidden(bool) { }
    virtual bool scrollbarsCanBeActive() const { return true; }

    virtual void didAddVerticalScrollbar(Scrollbar*);
    virtual void willRemoveVerticalScrollbar(Scrollbar*) { }
    virtual void didAddHorizontalScrollbar(Scrollbar*);
    virtual void willRemoveHorizontalScrollbar(Scrollbar*) { }

    virtual void invalidateScrollbarPartLayers(Scrollbar*) { }

    virtual void verticalScrollbarLayerDidChange() { }
    virtual void horizontalScrollbarLayerDidChange() { }

    virtual bool shouldScrollbarParticipateInHitTesting(Scrollbar*) { return true; }

    virtual void notifyContentAreaScrolled(const FloatSize& delta) { UNUSED_PARAM(delta); }

    virtual bool isUserScrollInProgress() const { return false; }
    virtual bool isRubberBandInProgress() const { return false; }
    virtual bool isScrollSnapInProgress() const { return false; }

    virtual String horizontalScrollbarStateForTesting() const { return emptyString(); }
    virtual String verticalScrollbarStateForTesting() const { return emptyString(); }

    void setWheelEventTestMonitor(RefPtr<WheelEventTestMonitor>&& testMonitor) { m_wheelEventTestMonitor = testMonitor; }

    FloatPoint adjustScrollOffsetForSnappingIfNeeded(const FloatPoint& offset, ScrollSnapPointSelectionMethod);
    float adjustScrollOffsetForSnappingIfNeeded(ScrollEventAxis, const FloatPoint& newOffset, ScrollSnapPointSelectionMethod);

    std::unique_ptr<ScrollControllerTimer> createTimer(Function<void()>&&) final;

    void startAnimationCallback(ScrollController&) final;
    void stopAnimationCallback(ScrollController&) final;

    void scrollControllerAnimationTimerFired();

    virtual bool processWheelEventForScrollSnap(const PlatformWheelEvent&) { return false; }
    void updateScrollSnapState();
    bool activeScrollSnapIndexDidChange() const;
    std::optional<unsigned> activeScrollSnapIndexForAxis(ScrollEventAxis) const;
    void setActiveScrollSnapIndexForAxis(ScrollEventAxis, std::optional<unsigned> index);
    void setSnapOffsetsInfo(const LayoutScrollSnapOffsetsInfo&);
    const LayoutScrollSnapOffsetsInfo* snapOffsetsInfo() const;
    void resnapAfterLayout();

    // ScrollControllerClient.
    FloatPoint scrollOffset() const override;
    void immediateScrollOnAxis(ScrollEventAxis, float delta) override;
    float pageScaleFactor() const override;
    LayoutSize scrollExtent() const override;
    FloatSize viewportSize() const override;
#if PLATFORM(MAC)
    void deferWheelEventTestCompletionForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason) const override;
    void removeWheelEventTestCompletionDeferralForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason) const override;
#endif

protected:
    virtual void notifyPositionChanged(const FloatSize& delta);
    void updateActiveScrollSnapIndexForOffset();

    FloatPoint offsetFromPosition(const FloatPoint& position);
    FloatPoint positionFromOffset(const FloatPoint& offset);
    FloatSize deltaFromStep(ScrollbarOrientation, float step, float multiplier);

    ScrollableArea& m_scrollableArea;
    RefPtr<WheelEventTestMonitor> m_wheelEventTestMonitor;
    ScrollController m_scrollController;
    Timer m_scrollControllerAnimationTimer;
    FloatPoint m_currentPosition;

    std::unique_ptr<ScrollAnimation> m_scrollAnimation;
    std::unique_ptr<KeyboardScrollingAnimator> m_keyboardScrollingAnimator;
};

} // namespace WebCore

