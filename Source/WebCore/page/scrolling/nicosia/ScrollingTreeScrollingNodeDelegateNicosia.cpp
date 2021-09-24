/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2019, 2021 Igalia S.L.
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

#include "config.h"
#include "ScrollingTreeScrollingNodeDelegateNicosia.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "NicosiaPlatformLayer.h"
#include "ScrollingTreeFrameScrollingNode.h"

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebCore {
class ScrollAnimation;
class ScrollAnimationKinetic;

ScrollingTreeScrollingNodeDelegateNicosia::ScrollingTreeScrollingNodeDelegateNicosia(ScrollingTreeScrollingNode& scrollingNode, bool scrollAnimatorEnabled)
    : ScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_scrollAnimatorEnabled(scrollAnimatorEnabled)
#if ENABLE(KINETIC_SCROLLING) || ENABLE(SMOOTH_SCROLLING)
    , m_animationTimer(RunLoop::current(), this, &ScrollingTreeScrollingNodeDelegateNicosia::animationTimerFired)
#endif
{
#if ENABLE(KINETIC_SCROLLING) || ENABLE(SMOOTH_SCROLLING)
#if USE(GLIB_EVENT_LOOP)
    m_animationTimer.setPriority(WTF::RunLoopSourcePriority::DisplayRefreshMonitorTimer);
#endif
#endif    
}

ScrollingTreeScrollingNodeDelegateNicosia::~ScrollingTreeScrollingNodeDelegateNicosia() = default;

std::unique_ptr<Nicosia::SceneIntegration::UpdateScope> ScrollingTreeScrollingNodeDelegateNicosia::createUpdateScope()
{
    auto* scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrollingNode().scrollContainerLayer());
    if (is<ScrollingTreeFrameScrollingNode>(scrollingNode()))
        scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrollingNode().scrolledContentsLayer());

    ASSERT(scrollLayer);
    auto& compositionLayer = downcast<Nicosia::CompositionLayer>(*scrollLayer);
    return compositionLayer.createUpdateScope();
}

void ScrollingTreeScrollingNodeDelegateNicosia::resetCurrentPosition()
{
#if ENABLE(SMOOTH_SCROLLING)
    if (m_smoothAnimation)
        m_smoothAnimation->stop();
#endif
}

void ScrollingTreeScrollingNodeDelegateNicosia::updateVisibleLengths()
{
#if ENABLE(SMOOTH_SCROLLING)
    if (m_smoothAnimation)
        m_smoothAnimation->updateScrollExtents();
#endif
}

#if ENABLE(KINETIC_SCROLLING)
void ScrollingTreeScrollingNodeDelegateNicosia::ensureScrollAnimationKinetic()
{
    if (m_kineticAnimation)
        return;

    m_kineticAnimation = makeUnique<ScrollAnimationKinetic>(*this);
    startTimerIfNecessary();
}
#endif

#if ENABLE(SMOOTH_SCROLLING)
void ScrollingTreeScrollingNodeDelegateNicosia::ensureScrollAnimationSmooth()
{
    if (m_smoothAnimation)
        return;

    m_smoothAnimation = makeUnique<ScrollAnimationSmooth>(*this);
    startTimerIfNecessary();
}
#endif

WheelEventHandlingResult ScrollingTreeScrollingNodeDelegateNicosia::handleWheelEvent(const PlatformWheelEvent& wheelEvent, EventTargeting eventTargeting)
{
    if (!scrollingNode().canHandleWheelEvent(wheelEvent, eventTargeting))
        return WheelEventHandlingResult::unhandled();

    bool canHaveHorizontalScrollbar = scrollingNode().canHaveHorizontalScrollbar();
    bool canHaveVerticalScrollbar = scrollingNode().canHaveVerticalScrollbar();
#if ENABLE(KINETIC_SCROLLING)
    ensureScrollAnimationKinetic();
    m_kineticAnimation->appendToScrollHistory(wheelEvent);
    m_kineticAnimation->stop();
    if (wheelEvent.isEndOfNonMomentumScroll()) {
        m_kineticAnimation->startAnimatedScrollWithInitialVelocity(currentScrollPosition(), m_kineticAnimation->computeVelocity(), canHaveHorizontalScrollbar, canHaveVerticalScrollbar);
        m_kineticAnimation->clearScrollHistory();
        return WheelEventHandlingResult::handled();
    }
    if (wheelEvent.isTransitioningToMomentumScroll()) {
        m_kineticAnimation->startAnimatedScrollWithInitialVelocity(currentScrollPosition(), wheelEvent.swipeVelocity(), canHaveHorizontalScrollbar, canHaveVerticalScrollbar);
        m_kineticAnimation->clearScrollHistory();
        return WheelEventHandlingResult::handled();
    }
#endif

    // FIXME: This needs to share code with ScrollAnimator::handleWheelEvent(), perhaps by moving code into ScrollingEffectsController::handleWheelEvent().
    float deltaX = canHaveHorizontalScrollbar ? wheelEvent.deltaX() : 0;
    float deltaY = canHaveVerticalScrollbar ? wheelEvent.deltaY() : 0;
    if ((deltaX < 0 && currentScrollPosition().x() >= maximumScrollPosition().x())
        || (deltaX > 0 && currentScrollPosition().x() <= minimumScrollPosition().x()))
        deltaX = 0;
    if ((deltaY < 0 && currentScrollPosition().y() >= maximumScrollPosition().y())
        || (deltaY > 0 && currentScrollPosition().y() <= minimumScrollPosition().y()))
        deltaY = 0;

    if (!deltaX && !deltaY)
        return WheelEventHandlingResult::unhandled();

    if (wheelEvent.granularity() == ScrollByPageWheelEvent) {
        if (deltaX) {
            bool negative = deltaX < 0;
            deltaX = Scrollbar::pageStepDelta(scrollableAreaSize().width());
            if (negative)
                deltaX = -deltaX;
        }
        if (deltaY) {
            bool negative = deltaY < 0;
            deltaY = Scrollbar::pageStepDelta(scrollableAreaSize().height());
            if (negative)
                deltaY = -deltaY;
        }
    }

    deltaX = -deltaX;
    deltaY = -deltaY;

    if (!scrollingNode().snapOffsetsInfo().isEmpty()) {
        float scale = pageScaleFactor();
        FloatPoint originalOffset = LayoutPoint(scrollingNode().currentScrollOffset().x() / scale, scrollingNode().currentScrollOffset().y() / scale);
        auto newOffset = (scrollingNode().currentScrollOffset() + FloatSize(deltaX, deltaY));
        newOffset.scale(1.0 / scale);

        auto offsetX = scrollingNode().snapOffsetsInfo().closestSnapOffset(ScrollEventAxis::Horizontal, scrollableAreaSize(), newOffset, deltaX, originalOffset.x()).first;
        auto offsetY = scrollingNode().snapOffsetsInfo().closestSnapOffset(ScrollEventAxis::Vertical, scrollableAreaSize(), newOffset, deltaY, originalOffset.y()).first;

        deltaX = (offsetX - originalOffset.x()) * scale;
        deltaY = (offsetY - originalOffset.y()) * scale;
    }

#if ENABLE(SMOOTH_SCROLLING)
    if (m_scrollAnimatorEnabled && !wheelEvent.hasPreciseScrollingDeltas()) {
        ensureScrollAnimationSmooth();
        auto currentOffset = scrollingNode().currentScrollOffset();
        auto destinationOffset = currentOffset + FloatSize { deltaX, deltaY };
        m_smoothAnimation->startAnimatedScrollToDestination(currentOffset, destinationOffset);
        return WheelEventHandlingResult::handled();
    }
#endif

    auto updateScope = createUpdateScope();
    scrollingNode().scrollBy({ deltaX, deltaY });

    return WheelEventHandlingResult::handled();
}

void ScrollingTreeScrollingNodeDelegateNicosia::stopScrollAnimations()
{
#if ENABLE(KINETIC_SCROLLING)
    if (m_kineticAnimation) {
        m_kineticAnimation->stop();
        m_kineticAnimation->clearScrollHistory();
    }
#endif
#if ENABLE(SMOOTH_SCROLLING)
    if (m_smoothAnimation)
        m_smoothAnimation->stop();
#endif
#if ENABLE(KINETIC_SCROLLING) || ENABLE(SMOOTH_SCROLLING)
    m_animationTimer.stop();
#endif
}

float ScrollingTreeScrollingNodeDelegateNicosia::pageScaleFactor()
{
    // FIXME: What should this return for non-root frames, and overflow?
    // Also, this should not have to access ScrollingTreeFrameScrollingNode.
    return is<ScrollingTreeFrameScrollingNode>(scrollingNode()) ?
        downcast<ScrollingTreeFrameScrollingNode>(scrollingNode()).frameScaleFactor() : 1.;
}

void ScrollingTreeScrollingNodeDelegateNicosia::scrollAnimationDidUpdate(ScrollAnimation& animation, const FloatPoint& offset)
{
#if ENABLE(SMOOTH_SCROLLING)
    if (&animation == m_kineticAnimation.get()) {
        if (m_smoothAnimation)
            m_smoothAnimation->stop();
    }
#endif
    auto updateScope = createUpdateScope();

    // FIXME: Need to convert an offset to a position.
    scrollingNode().scrollTo(offset);
}

void ScrollingTreeScrollingNodeDelegateNicosia::scrollAnimationDidEnd(ScrollAnimation&)
{
}

ScrollExtents ScrollingTreeScrollingNodeDelegateNicosia::scrollExtentsForAnimation(ScrollAnimation&)
{
    return {
        scrollingNode().totalContentsSize(),
        scrollingNode().scrollableAreaSize()
    };
}

void ScrollingTreeScrollingNodeDelegateNicosia::startTimerIfNecessary()
{
#if ENABLE(KINETIC_SCROLLING) || ENABLE(SMOOTH_SCROLLING)
    if (m_animationTimer.isActive())
        return;

    static constexpr double frameRate = 60;
    static constexpr Seconds tickTime = 1_s / frameRate;
    
    m_animationTimer.startRepeating(tickTime);
#endif
}

void ScrollingTreeScrollingNodeDelegateNicosia::animationTimerFired()
{
#if ENABLE(KINETIC_SCROLLING) || ENABLE(SMOOTH_SCROLLING)
    auto now = MonotonicTime::now();
#endif

#if ENABLE(KINETIC_SCROLLING)
    if (m_kineticAnimation)
        m_kineticAnimation->serviceAnimation(now);
#endif
#if ENABLE(SMOOTH_SCROLLING)
    if (m_smoothAnimation)
        m_smoothAnimation->serviceAnimation(now);
#endif    
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
