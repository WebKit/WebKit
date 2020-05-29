/*
 * Copyright (C) 2012, 2014-2015 Apple Inc. All rights reserved.
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
#include "ScrollingTreeFrameScrollingNodeNicosia.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "FrameView.h"
#include "Logging.h"
#include "NicosiaPlatformLayer.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingTree.h"

#if ENABLE(KINETIC_SCROLLING)
#include "ScrollAnimationKinetic.h"
#endif

namespace WebCore {

Ref<ScrollingTreeFrameScrollingNode> ScrollingTreeFrameScrollingNodeNicosia::create(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameScrollingNodeNicosia(scrollingTree, nodeType, nodeID));
}

ScrollingTreeFrameScrollingNodeNicosia::ScrollingTreeFrameScrollingNodeNicosia(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNode(scrollingTree, nodeType, nodeID)
{
#if ENABLE(KINETIC_SCROLLING)
    m_kineticAnimation = makeUnique<ScrollAnimationKinetic>(
        [this]() -> ScrollAnimationKinetic::ScrollExtents {
            return { IntPoint(minimumScrollPosition()), IntPoint(maximumScrollPosition()) };
        },
        [this](FloatPoint&& position) {
            auto* scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrolledContentsLayer());
            ASSERT(scrollLayer);
            auto& compositionLayer = downcast<Nicosia::CompositionLayer>(*scrollLayer);

            auto updateScope = compositionLayer.createUpdateScope();
            scrollTo(position);
        });
#endif
}

ScrollingTreeFrameScrollingNodeNicosia::~ScrollingTreeFrameScrollingNodeNicosia() = default;

void ScrollingTreeFrameScrollingNodeNicosia::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateBeforeChildren(stateNode);

    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::RootContentsLayer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(scrollingStateNode.rootContentsLayer());
        m_rootContentsLayer = downcast<Nicosia::CompositionLayer>(layer);
    }
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(scrollingStateNode.counterScrollingLayer());
        m_counterScrollingLayer = downcast<Nicosia::CompositionLayer>(layer);
    }
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(scrollingStateNode.insetClipLayer());
        m_insetClipLayer = downcast<Nicosia::CompositionLayer>(layer);
    }
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(scrollingStateNode.contentShadowLayer());
        m_contentShadowLayer = downcast<Nicosia::CompositionLayer>(layer);
    }
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(scrollingStateNode.headerLayer());
        m_headerLayer = downcast<Nicosia::CompositionLayer>(layer);
    }
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(scrollingStateNode.footerLayer());
        m_footerLayer = downcast<Nicosia::CompositionLayer>(layer);
    }
}

void ScrollingTreeFrameScrollingNodeNicosia::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateAfterChildren(stateNode);

    const auto& scrollingStateNode = downcast<ScrollingStateScrollingNode>(stateNode);

    // Update the scroll position after child nodes have been updated, because they need to have updated their constraints before any scrolling happens.
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition)) {
        const auto& requestedScrollData = scrollingStateNode.requestedScrollData();
        scrollTo(requestedScrollData.scrollPosition, requestedScrollData.scrollType, requestedScrollData.clamping);
    }
}

WheelEventHandlingResult ScrollingTreeFrameScrollingNodeNicosia::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHandleWheelEvent(wheelEvent))
        return WheelEventHandlingResult::unhandled();

    if (wheelEvent.deltaX() || wheelEvent.deltaY()) {
        auto* scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrolledContentsLayer());
        ASSERT(scrollLayer);
        auto& compositionLayer = downcast<Nicosia::CompositionLayer>(*scrollLayer);

        auto updateScope = compositionLayer.createUpdateScope();
        scrollBy({ -wheelEvent.deltaX(), -wheelEvent.deltaY() });

#if ENABLE(KINETIC_SCROLLING)
        m_kineticAnimation->appendToScrollHistory(wheelEvent);
#endif
    }

#if ENABLE(KINETIC_SCROLLING)
    m_kineticAnimation->stop();
    if (wheelEvent.isEndOfNonMomentumScroll()) {
        m_kineticAnimation->start(currentScrollPosition(), m_kineticAnimation->computeVelocity(), canHaveHorizontalScrollbar(), canHaveVerticalScrollbar());
        m_kineticAnimation->clearScrollHistory();
    }
    if (wheelEvent.isTransitioningToMomentumScroll()) {
        m_kineticAnimation->start(currentScrollPosition(), wheelEvent.swipeVelocity(), canHaveHorizontalScrollbar(), canHaveVerticalScrollbar());
        m_kineticAnimation->clearScrollHistory();
    }
#endif

    // FIXME: This needs to return whether the event was handled.
    return WheelEventHandlingResult::handled();
}

void ScrollingTreeFrameScrollingNodeNicosia::stopScrollAnimations()
{
#if ENABLE(KINETIC_SCROLLING)
    m_kineticAnimation->stop();
    m_kineticAnimation->clearScrollHistory();
#endif
}

FloatPoint ScrollingTreeFrameScrollingNodeNicosia::adjustedScrollPosition(const FloatPoint& position, ScrollClamping clamping) const
{
    FloatPoint scrollPosition(roundf(position.x()), roundf(position.y()));
    return ScrollingTreeFrameScrollingNode::adjustedScrollPosition(scrollPosition, clamping);
}

void ScrollingTreeFrameScrollingNodeNicosia::currentScrollPositionChanged(ScrollingLayerPositionAction action)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFrameScrollingNodeNicosia::currentScrollPositionChanged to " << currentScrollPosition() << " min: " << minimumScrollPosition() << " max: " << maximumScrollPosition() << " sync: " << hasSynchronousScrollingReasons());

    ScrollingTreeFrameScrollingNode::currentScrollPositionChanged(hasSynchronousScrollingReasons() ? ScrollingLayerPositionAction::Set : action);
}

void ScrollingTreeFrameScrollingNodeNicosia::repositionScrollingLayers()
{
    auto* scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrolledContentsLayer());
    ASSERT(scrollLayer);
    auto& compositionLayer = downcast<Nicosia::CompositionLayer>(*scrollLayer);

    auto scrollPosition = currentScrollPosition();

    compositionLayer.updateState(
        [&scrollPosition](Nicosia::CompositionLayer::LayerState& state)
        {
            state.position = -scrollPosition;
            state.delta.positionChanged = true;
        });
}

void ScrollingTreeFrameScrollingNodeNicosia::repositionRelatedLayers()
{
    auto scrollPosition = currentScrollPosition();
    auto layoutViewport = this->layoutViewport();

    FloatRect visibleContentRect(scrollPosition, scrollableAreaSize());

    auto applyLayerPosition =
        [](auto& layer, auto&& position)
        {
            layer.updateState(
                [&position](Nicosia::CompositionLayer::LayerState& state)
                {
                    state.position = position;
                    state.delta.positionChanged = true;
                });
        };

    if (m_counterScrollingLayer)
        applyLayerPosition(*m_counterScrollingLayer, layoutViewport.location());

    float topContentInset = this->topContentInset();
    if (m_insetClipLayer && m_rootContentsLayer) {
        m_insetClipLayer->updateState(
            [&scrollPosition, &topContentInset](Nicosia::CompositionLayer::LayerState& state)
            {
                state.position = { state.position.x(), FrameView::yPositionForInsetClipLayer(scrollPosition, topContentInset) };
                state.delta.positionChanged = true;
            });

        auto rootContentsPosition = FrameView::positionForRootContentLayer(scrollPosition, scrollOrigin(), topContentInset, headerHeight());
        applyLayerPosition(*m_rootContentsLayer, rootContentsPosition);
        if (m_contentShadowLayer)
            applyLayerPosition(*m_contentShadowLayer, rootContentsPosition);
    }

    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute layoutViewport.x() for the banner with a scale factor of 1.
        float horizontalScrollOffsetForBanner = layoutViewport.x();
        if (m_headerLayer)
            applyLayerPosition(*m_headerLayer, FloatPoint(horizontalScrollOffsetForBanner, FrameView::yPositionForHeaderLayer(scrollPosition, topContentInset)));
        if (m_footerLayer)
            applyLayerPosition(*m_footerLayer, FloatPoint(horizontalScrollOffsetForBanner, FrameView::yPositionForFooterLayer(scrollPosition, topContentInset, totalContentsSize().height(), footerHeight())));
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
