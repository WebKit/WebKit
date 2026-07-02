/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2019, 2024 Igalia S.L.
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
#include "ScrollingTreeOverflowScrollingNodeCoordinated.h"

#if ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayer.h"
#include "ScrollingTreeScrollingNodeDelegateCoordinated.h"
#include "ThreadedScrollingTree.h"

namespace WebCore {

Ref<ScrollingTreeOverflowScrollingNode> ScrollingTreeOverflowScrollingNodeCoordinated::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollingNodeCoordinated(scrollingTree, nodeID));
}

ScrollingTreeOverflowScrollingNodeCoordinated::ScrollingTreeOverflowScrollingNodeCoordinated(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollingNode(scrollingTree, nodeID)
{
    m_delegate = makeUnique<ScrollingTreeScrollingNodeDelegateCoordinated>(*this, downcast<ThreadedScrollingTree>(scrollingTree).scrollAnimatorEnabled());
}

ScrollingTreeOverflowScrollingNodeCoordinated::~ScrollingTreeOverflowScrollingNodeCoordinated() = default;

ScrollingTreeScrollingNodeDelegateCoordinated& ScrollingTreeOverflowScrollingNodeCoordinated::delegate() const
{
    return *static_cast<ScrollingTreeScrollingNodeDelegateCoordinated*>(m_delegate.get());
}

bool ScrollingTreeOverflowScrollingNodeCoordinated::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    if (!ScrollingTreeOverflowScrollingNode::commitStateBeforeChildren(stateNode))
        return false;

    m_delegate->updateFromStateNode(downcast<ScrollingStateScrollingNode>(stateNode));
    return true;
}

void ScrollingTreeOverflowScrollingNodeCoordinated::repositionScrollingLayers()
{
    auto* scrollLayer = static_cast<CoordinatedPlatformLayer*>(scrollContainerLayer());
    ASSERT(scrollLayer);

    auto scrollOffset = currentScrollOffset();
    scrollLayer->setBoundsOriginForScrolling(scrollOffset);

    delegate().updateVisibleLengths();
}

WheelEventHandlingResult ScrollingTreeOverflowScrollingNodeCoordinated::handleWheelEvent(const PlatformWheelEvent& wheelEvent, EventTargeting eventTargeting)
{
    if (!canHandleWheelEvent(wheelEvent, eventTargeting))
        return WheelEventHandlingResult::unhandled();

    return WheelEventHandlingResult::result(delegate().handleWheelEvent(wheelEvent));
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
