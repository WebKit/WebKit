/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef ScrollAnimatorIOS_h
#define ScrollAnimatorIOS_h

#include "ScrollAnimator.h"

#include "IntPoint.h"

namespace WebCore {

class PlatformTouchEvent;

class ScrollAnimatorIOS : public ScrollAnimator {
public:
    ScrollAnimatorIOS(ScrollableArea*);
    virtual ~ScrollAnimatorIOS();

#if ENABLE(TOUCH_EVENTS)
    virtual bool handleTouchEvent(const PlatformTouchEvent&) override;
#endif

private:
    void determineScrollableAreaForTouchSequence(const IntSize& touchDelta);

    // State for handling sequences of touches in defaultTouchEventHandler.
    enum AxisLatch {
        AxisLatchNotComputed,
        AxisLatchNone,
        AxisLatchHorizontal,
        AxisLatchVertical
    };
    AxisLatch m_touchScrollAxisLatch;
    bool m_inTouchSequence;
    bool m_committedToScrollAxis;
    bool m_startedScroll;
    IntPoint m_firstTouchPoint;
    IntPoint m_lastTouchPoint;

    // When we're in a touch sequence, this will point to the scrollable area that
    // should actually be scrolled during the sequence.
    ScrollableArea* m_scrollableAreaForTouchSequence;
};

} // namespace WebCore

#endif // ScrollAnimatorIOS_h
