/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "ScrollElasticityController.h"
#include "Timer.h"
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@class WebScrollAnimationHelperDelegate;
@class WebScrollbarPainterControllerDelegate;
@class WebScrollbarPainterDelegate;
#else
class WebScrollAnimationHelperDelegate;
class WebScrollbarPainterControllerDelegate;
class WebScrollbarPainterDelegate;
#endif

#if USE(SCROLLBAR_PAINTER)
typedef id ScrollbarPainterController;
#endif

#if !ENABLE(RUBBER_BANDING)
class ScrollElasticityControllerClient { };
#endif

namespace WebCore {

class Scrollbar;

class ScrollAnimatorMac : public ScrollAnimator, private ScrollElasticityControllerClient {

public:
    ScrollAnimatorMac(ScrollableArea*);
    virtual ~ScrollAnimatorMac();

    virtual bool scroll(ScrollbarOrientation, ScrollGranularity, float step, float multiplier);
    virtual void scrollToOffsetWithoutAnimation(const FloatPoint&);

#if ENABLE(RUBBER_BANDING)
    virtual bool handleWheelEvent(const PlatformWheelEvent&) OVERRIDE;
#if ENABLE(GESTURE_EVENTS)
    virtual void handleGestureEvent(const PlatformGestureEvent&);
#endif
#endif

    virtual void cancelAnimations();

    void immediateScrollToPoint(const FloatPoint& newPosition);
    void immediateScrollByDeltaX(float deltaX);
    void immediateScrollByDeltaY(float deltaY);

    void immediateScrollToPointForScrollAnimation(const FloatPoint& newPosition);

    void setIsDrawingIntoLayer(bool b) { m_drawingIntoLayer = b; }
    bool isDrawingIntoLayer() const { return m_drawingIntoLayer; }

    bool haveScrolledSincePageLoad() const { return m_haveScrolledSincePageLoad; }

    virtual void setIsActive();

#if USE(SCROLLBAR_PAINTER)
    void updateScrollerStyle();

    bool scrollbarPaintTimerIsActive() const;
    void startScrollbarPaintTimer();
    void stopScrollbarPaintTimer();
#endif

    void setVisibleScrollerThumbRect(const IntRect&);

private:
    RetainPtr<id> m_scrollAnimationHelper;
    RetainPtr<WebScrollAnimationHelperDelegate> m_scrollAnimationHelperDelegate;

#if USE(SCROLLBAR_PAINTER)
    RetainPtr<ScrollbarPainterController> m_scrollbarPainterController;
    RetainPtr<WebScrollbarPainterControllerDelegate> m_scrollbarPainterControllerDelegate;
    RetainPtr<WebScrollbarPainterDelegate> m_horizontalScrollbarPainterDelegate;
    RetainPtr<WebScrollbarPainterDelegate> m_verticalScrollbarPainterDelegate;

    void initialScrollbarPaintTimerFired(Timer<ScrollAnimatorMac>*);
    Timer<ScrollAnimatorMac> m_initialScrollbarPaintTimer;
#endif
    
    virtual void notifyPositionChanged();
    virtual void contentAreaWillPaint() const;
    virtual void mouseEnteredContentArea() const;
    virtual void mouseExitedContentArea() const;
    virtual void mouseMovedInContentArea() const;
    virtual void mouseEnteredScrollbar(Scrollbar*) const;
    virtual void mouseExitedScrollbar(Scrollbar*) const;
    virtual void willStartLiveResize();
    virtual void contentsResized() const;
    virtual void willEndLiveResize();
    virtual void contentAreaDidShow() const;
    virtual void contentAreaDidHide() const;
    void didBeginScrollGesture() const;
    void didEndScrollGesture() const;

    virtual void didAddVerticalScrollbar(Scrollbar*);
    virtual void willRemoveVerticalScrollbar(Scrollbar*);
    virtual void didAddHorizontalScrollbar(Scrollbar*);
    virtual void willRemoveHorizontalScrollbar(Scrollbar*);

    void setNeedsScrollerStyleUpdate(bool needsUpdate) { m_needsScrollerStyleUpdate = needsUpdate; }
    bool needsScrollerStyleUpdate() const { return m_needsScrollerStyleUpdate; }

    float adjustScrollXPositionIfNecessary(float) const;
    float adjustScrollYPositionIfNecessary(float) const;
    FloatPoint adjustScrollPositionIfNecessary(const FloatPoint&) const;

#if ENABLE(RUBBER_BANDING)
    /// ScrollElasticityControllerClient member functions.
    virtual bool isHorizontalScrollerPinnedToMinimumPosition() OVERRIDE;
    virtual bool isHorizontalScrollerPinnedToMaximumPosition() OVERRIDE;
    virtual IntSize stretchAmount() OVERRIDE;
    virtual void startSnapRubberbandTimer() OVERRIDE;
    virtual void stopSnapRubberbandTimer() OVERRIDE;

    bool allowsVerticalStretching() const;
    bool allowsHorizontalStretching() const;
    bool pinnedInDirection(float deltaX, float deltaY);
    void snapRubberBand();
    void snapRubberBandTimerFired(Timer<ScrollAnimatorMac>*);
    void smoothScrollWithEvent(const PlatformWheelEvent&);
    void beginScrollGesture();
    void endScrollGesture();

    ScrollElasticityController m_scrollElasticityController;
    Timer<ScrollAnimatorMac> m_snapRubberBandTimer;
#endif

    bool m_drawingIntoLayer;
    bool m_haveScrolledSincePageLoad;
    bool m_needsScrollerStyleUpdate;
    IntRect m_visibleScrollerThumbRect;
};

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)

#endif // ScrollAnimatorMac_h
