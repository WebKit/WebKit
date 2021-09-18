
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

#include "FloatPoint.h"
#include "FloatSize.h"
#include "ScrollAnimator.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS WebScrollAnimationHelperDelegate;

namespace WebCore {

class Scrollbar;

class ScrollAnimatorMac final : public ScrollAnimator {
public:
    ScrollAnimatorMac(ScrollableArea&);
    virtual ~ScrollAnimatorMac();

    void immediateScrollToPositionForScrollAnimation(const FloatPoint& newPosition);

private:
    bool scroll(ScrollbarOrientation, ScrollGranularity, float step, float multiplier, OptionSet<ScrollBehavior>) final;
    bool scrollToPositionWithAnimation(const FloatPoint&) final;
    bool scrollToPositionWithoutAnimation(const FloatPoint& position, ScrollClamping = ScrollClamping::Clamped) final;

#if ENABLE(RUBBER_BANDING)
    bool shouldForwardWheelEventsToParent(const PlatformWheelEvent&) const;
    bool handleWheelEvent(const PlatformWheelEvent&) final;
#endif

    void handleWheelEventPhase(PlatformWheelEventPhase) final;
    
    void notifyPositionChanged(const FloatSize& delta) final;

    FloatPoint adjustScrollPositionIfNecessary(const FloatPoint&) const;

    bool isUserScrollInProgress() const final;
    bool isRubberBandInProgress() const final;
    bool isScrollSnapInProgress() const final;

    bool processWheelEventForScrollSnap(const PlatformWheelEvent&) final;

    // ScrollingEffectsControllerClient.
#if ENABLE(RUBBER_BANDING)
    IntSize stretchAmount() const final;
    bool allowsHorizontalStretching(const PlatformWheelEvent&) const final;
    bool allowsVerticalStretching(const PlatformWheelEvent&) const final;
    bool isPinnedForScrollDelta(const FloatSize&) const final;
    RectEdges<bool> edgePinnedState() const final;
    bool allowsHorizontalScrolling() const final;
    bool allowsVerticalScrolling() const final;
    bool shouldRubberBandInDirection(ScrollDirection) const final;
    void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) final;
    void immediateScrollBy(const FloatSize&) final;
    void adjustScrollPositionToBoundsIfNecessary() final;
#endif

    RetainPtr<id> m_scrollAnimationHelper;
    RetainPtr<WebScrollAnimationHelperDelegate> m_scrollAnimationHelperDelegate;
};

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)

