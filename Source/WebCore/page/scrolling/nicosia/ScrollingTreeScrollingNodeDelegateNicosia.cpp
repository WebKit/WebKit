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

namespace WebCore {
class ScrollAnimation;
class ScrollAnimationKinetic;

ScrollingTreeScrollingNodeDelegateNicosia::ScrollingTreeScrollingNodeDelegateNicosia(ScrollingTreeScrollingNode& scrollingNode, bool scrollAnimatorEnabled)
    : ScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_scrollAnimatorEnabled(scrollAnimatorEnabled)
{
}

ScrollingTreeScrollingNodeDelegateNicosia::~ScrollingTreeScrollingNodeDelegateNicosia()
{
}

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
        m_smoothAnimation->setCurrentPosition(scrollingNode().currentScrollPosition());
#endif
}

void ScrollingTreeScrollingNodeDelegateNicosia::updateVisibleLengths()
{
#if ENABLE(SMOOTH_SCROLLING)
    if (m_smoothAnimation)
        m_smoothAnimation->updateVisibleLengths();
#endif
}

#if ENABLE(KINETIC_SCROLLING)
void ScrollingTreeScrollingNodeDelegateNicosia::ensureScrollAnimationKinetic()
{
    if (m_kineticAnimation)
        return;

    m_kineticAnimation = makeUnique<ScrollAnimationKinetic>(
        [this]() -> ScrollExtents {
            return { IntPoint(minimumScrollPosition()), IntPoint(maximumScrollPosition()), IntSize(scrollableAreaSize()) };
        },
        [this](FloatPoint&& position) {
#if ENABLE(SMOOTH_SCROLLING)
            if (m_smoothAnimation)
                m_smoothAnimation->setCurrentPosition(position);
#endif
            auto updateScope = createUpdateScope();
            scrollingNode().scrollTo(position);
        });
}
#endif

#if ENABLE(SMOOTH_SCROLLING)
void ScrollingTreeScrollingNodeDelegateNicosia::ensureScrollAnimationSmooth()
{
    if (m_smoothAnimation)
        return;

    m_smoothAnimation = makeUnique<ScrollAnimationSmooth>(
        [this]() -> ScrollExtents {
            return { IntPoint(minimumScrollPosition()), IntPoint(maximumScrollPosition()), IntSize(scrollableAreaSize()) };
        },
        currentScrollPosition(),
        [this](FloatPoint&& position) {
            auto updateScope = createUpdateScope();
            scrollingNode().scrollTo(position);
        },
        [] { });
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
        m_kineticAnimation->start(currentScrollPosition(), m_kineticAnimation->computeVelocity(), canHaveHorizontalScrollbar, canHaveVerticalScrollbar);
        m_kineticAnimation->clearScrollHistory();
        return WheelEventHandlingResult::handled();
    }
    if (wheelEvent.isTransitioningToMomentumScroll()) {
        m_kineticAnimation->start(currentScrollPosition(), wheelEvent.swipeVelocity(), canHaveHorizontalScrollbar, canHaveVerticalScrollbar);
        m_kineticAnimation->clearScrollHistory();
        return WheelEventHandlingResult::handled();
    }
#endif

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
        m_smoothAnimation->scroll(HorizontalScrollbar, ScrollByPixel, 1, deltaX);
        m_smoothAnimation->scroll(VerticalScrollbar, ScrollByPixel, 1, deltaY);
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
}

float ScrollingTreeScrollingNodeDelegateNicosia::pageScaleFactor()
{
    // FIXME: What should this return for non-root frames, and overflow?
    // Also, this should not have to access ScrollingTreeFrameScrollingNode.
    return is<ScrollingTreeFrameScrollingNode>(scrollingNode()) ?
        downcast<ScrollingTreeFrameScrollingNode>(scrollingNode()).frameScaleFactor() : 1.;
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
