/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingTreeOverflowScrollingNodeNicosia.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "NicosiaPlatformLayer.h"
#if ENABLE(KINETIC_SCROLLING)
#include "ScrollAnimationKinetic.h"
#endif
#if ENABLE(SMOOTH_SCROLLING)
#include "ScrollAnimationSmooth.h"
#endif
#include "ScrollingStateOverflowScrollingNode.h"
#include "ScrollingTree.h"

namespace WebCore {

Ref<ScrollingTreeOverflowScrollingNode> ScrollingTreeOverflowScrollingNodeNicosia::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollingNodeNicosia(scrollingTree, nodeID));
}

ScrollingTreeOverflowScrollingNodeNicosia::ScrollingTreeOverflowScrollingNodeNicosia(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollingNode(scrollingTree, nodeID)
{
#if ENABLE(KINETIC_SCROLLING)
    m_kineticAnimation = makeUnique<ScrollAnimationKinetic>(
        [this]() -> ScrollExtents {
            return { IntPoint(minimumScrollPosition()), IntPoint(maximumScrollPosition()), IntSize(scrollableAreaSize()) };
        },
        [this](FloatPoint&& position) {
#if ENABLE(SMOOTH_SCROLLING)
            m_smoothAnimation->setCurrentPosition(position);
#endif

            auto* scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrollContainerLayer());
            ASSERT(scrollLayer);
            auto& compositionLayer = downcast<Nicosia::CompositionLayer>(*scrollLayer);

            auto updateScope = compositionLayer.createUpdateScope();
            scrollTo(position);
        });
#endif
#if ENABLE(SMOOTH_SCROLLING)
    m_smoothAnimation = makeUnique<ScrollAnimationSmooth>(
        [this]() -> ScrollExtents {
            return { IntPoint(minimumScrollPosition()), IntPoint(maximumScrollPosition()), IntSize(scrollableAreaSize()) };
        },
        currentScrollPosition(),
        [this](FloatPoint&& position) {
            auto* scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrollContainerLayer());
            ASSERT(scrollLayer);
            auto& compositionLayer = downcast<Nicosia::CompositionLayer>(*scrollLayer);

            auto updateScope = compositionLayer.createUpdateScope();
            scrollTo(position);
        },
        [] { });
#endif
}

ScrollingTreeOverflowScrollingNodeNicosia::~ScrollingTreeOverflowScrollingNodeNicosia() = default;

void ScrollingTreeOverflowScrollingNodeNicosia::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeOverflowScrollingNode::commitStateAfterChildren(stateNode);

    const auto& overflowStateNode = downcast<ScrollingStateOverflowScrollingNode>(stateNode);
    if (overflowStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition)) {
        stopScrollAnimations();
        const auto& requestedScrollData = overflowStateNode.requestedScrollData();
        scrollTo(requestedScrollData.scrollPosition, requestedScrollData.scrollType, requestedScrollData.clamping);
#if ENABLE(SMOOTH_SCROLLING)
        m_smoothAnimation->setCurrentPosition(currentScrollPosition());
#endif
    }
}

FloatPoint ScrollingTreeOverflowScrollingNodeNicosia::adjustedScrollPosition(const FloatPoint& position, ScrollClamping clamping) const
{
    FloatPoint scrollPosition(roundf(position.x()), roundf(position.y()));
    return ScrollingTreeOverflowScrollingNode::adjustedScrollPosition(scrollPosition, clamping);
}

void ScrollingTreeOverflowScrollingNodeNicosia::repositionScrollingLayers()
{
    auto* scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrolledContentsLayer());
    ASSERT(scrollLayer);
    auto& compositionLayer = downcast<Nicosia::CompositionLayer>(*scrollLayer);

    auto scrollOffset = currentScrollOffset();

    compositionLayer.updateState(
        [&scrollOffset](Nicosia::CompositionLayer::LayerState& state)
        {
            state.boundsOrigin = scrollOffset;
            state.delta.boundsOriginChanged = true;
        });
}

WheelEventHandlingResult ScrollingTreeOverflowScrollingNodeNicosia::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHandleWheelEvent(wheelEvent))
        return WheelEventHandlingResult::unhandled();

#if ENABLE(KINETIC_SCROLLING)
    m_kineticAnimation->appendToScrollHistory(wheelEvent);
    m_kineticAnimation->stop();
    if (wheelEvent.isEndOfNonMomentumScroll()) {
        m_kineticAnimation->start(currentScrollPosition(), m_kineticAnimation->computeVelocity(), canHaveHorizontalScrollbar(), canHaveVerticalScrollbar());
        m_kineticAnimation->clearScrollHistory();
        return WheelEventHandlingResult::handled();
    }
    if (wheelEvent.isTransitioningToMomentumScroll()) {
        m_kineticAnimation->start(currentScrollPosition(), wheelEvent.swipeVelocity(), canHaveHorizontalScrollbar(), canHaveVerticalScrollbar());
        m_kineticAnimation->clearScrollHistory();
        return WheelEventHandlingResult::handled();
    }
#endif

    float deltaX = canHaveHorizontalScrollbar() ? wheelEvent.deltaX() : 0;
    float deltaY = canHaveVerticalScrollbar() ? wheelEvent.deltaY() : 0;
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

#if ENABLE(SMOOTH_SCROLLING)
    m_smoothAnimation->scroll(HorizontalScrollbar, ScrollByPixel, 1, -deltaX);
    m_smoothAnimation->scroll(VerticalScrollbar, ScrollByPixel, 1, -deltaY);
#else
    auto* scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrollContainerLayer());
    ASSERT(scrollLayer);
    auto& compositionLayer = downcast<Nicosia::CompositionLayer>(*scrollLayer);

    auto updateScope = compositionLayer.createUpdateScope();
    scrollBy({ -deltaX, -deltaY });
#endif

    return WheelEventHandlingResult::handled();
}

void ScrollingTreeOverflowScrollingNodeNicosia::stopScrollAnimations()
{
#if ENABLE(KINETIC_SCROLLING)
    m_kineticAnimation->stop();
    m_kineticAnimation->clearScrollHistory();
#endif
#if ENABLE(SMOOTH_SCROLLING)
    m_smoothAnimation->stop();
#endif
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
