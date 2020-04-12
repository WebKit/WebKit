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
}

ScrollingTreeOverflowScrollingNodeNicosia::~ScrollingTreeOverflowScrollingNodeNicosia() = default;

void ScrollingTreeOverflowScrollingNodeNicosia::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeOverflowScrollingNode::commitStateAfterChildren(stateNode);

    const auto& overflowStateNode = downcast<ScrollingStateOverflowScrollingNode>(stateNode);
    if (overflowStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition)) {
        const auto& requestedScrollData = overflowStateNode.requestedScrollData();
        scrollTo(requestedScrollData.scrollPosition, requestedScrollData.scrollType, requestedScrollData.clamping);
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

ScrollingEventResult ScrollingTreeOverflowScrollingNodeNicosia::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHaveScrollbars())
        return ScrollingEventResult::DidNotHandleEvent;

    if (wheelEvent.deltaX() || wheelEvent.deltaY()) {
        auto* scrollLayer = static_cast<Nicosia::PlatformLayer*>(scrollContainerLayer());
        ASSERT(scrollLayer);
        auto& compositionLayer = downcast<Nicosia::CompositionLayer>(*scrollLayer);

        auto updateScope = compositionLayer.createUpdateScope();
        scrollBy({ -wheelEvent.deltaX(), -wheelEvent.deltaY() });
    }

    scrollingTree().setOrClearLatchedNode(wheelEvent, scrollingNodeID());

    return ScrollingEventResult::DidHandleEvent;
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
