/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2019, 2021, 2024 Igalia S.L.
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
#include "ScrollingTreeScrollingNodeDelegateCoordinated.h"

#if ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
#include "ScrollerCoordinated.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingTreeOverflowScrollingNode.h"

namespace WebCore {

ScrollingTreeScrollingNodeDelegateCoordinated::ScrollingTreeScrollingNodeDelegateCoordinated(ScrollingTreeScrollingNode& scrollingNode, bool scrollAnimatorEnabled)
    : ThreadedScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_scrollAnimatorEnabled(scrollAnimatorEnabled)
#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    , m_scrollerPair(ScrollerPairCoordinated::create(scrollingNode))
#endif
{
    ASSERT(isMainThread());
#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    if (is<ScrollingTreeOverflowScrollingNode>(scrollingNode) && ScrollbarTheme::theme().usesOverlayScrollbars()) {
        protect(m_scrollerPair->horizontalScroller())->setOverlayScrollbarEnabled(true);
        protect(m_scrollerPair->verticalScroller())->setOverlayScrollbarEnabled(true);
    }
#endif
}

ScrollingTreeScrollingNodeDelegateCoordinated::~ScrollingTreeScrollingNodeDelegateCoordinated() = default;

void ScrollingTreeScrollingNodeDelegateCoordinated::updateVisibleLengths()
{
    m_scrollController.contentsSizeChanged();
#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    m_scrollerPair->updateValues();
#endif
}

bool ScrollingTreeScrollingNodeDelegateCoordinated::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    auto deferrer = ScrollingTreeWheelEventTestMonitorCompletionDeferrer { *scrollingTree(), scrollingNode()->scrollingNodeID(), WheelEventTestMonitor::DeferReason::HandlingWheelEvent };

    updateUserScrollInProgressForEvent(wheelEvent);

    return m_scrollController.handleWheelEvent(wheelEvent);
}

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
void ScrollingTreeScrollingNodeDelegateCoordinated::updateFromStateNode(const ScrollingStateScrollingNode& scrollingStateNode)
{
    ASSERT(isMainThread());
    CheckedRef horizontalScroller = m_scrollerPair->horizontalScroller();
    CheckedRef verticalScroller = m_scrollerPair->verticalScroller();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::PainterForScrollbar)) {
        horizontalScroller->setScrollerImp(scrollingStateNode.horizontalScrollerImp());
        verticalScroller->setScrollerImp(scrollingStateNode.verticalScrollerImp());
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollbarHoverState)) {
        auto hoverState = scrollingStateNode.scrollbarHoverState();
        verticalScroller->setHoveredAndPressedParts(hoverState.hoveredPartInVerticalScrollbar, hoverState.pressedPartInVerticalScrollbar);
        horizontalScroller->setHoveredAndPressedParts(hoverState.hoveredPartInHorizontalScrollbar, hoverState.pressedPartInHorizontalScrollbar);
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer))
        horizontalScroller->setHostLayer(static_cast<CoordinatedPlatformLayer*>(scrollingStateNode.horizontalScrollbarLayer()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer))
        verticalScroller->setHostLayer(static_cast<CoordinatedPlatformLayer*>(scrollingStateNode.verticalScrollbarLayer()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollbarEnabledState)) {
        auto scrollbarEnabledState = scrollingStateNode.scrollbarEnabledState();
        horizontalScroller->setEnabled(scrollbarEnabledState.horizontalScrollbarIsEnabled);
        verticalScroller->setEnabled(scrollbarEnabledState.verticalScrollbarIsEnabled);
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollbarLayoutDirection)) {
        auto scrollbarLayoutDirection = scrollingStateNode.scrollbarLayoutDirection();
        horizontalScroller->setScrollbarLayoutDirection(scrollbarLayoutDirection);
        verticalScroller->setScrollbarLayoutDirection(scrollbarLayoutDirection);
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::UseDarkAppearanceForScrollbars)) {
        bool useDarkAppearanceForScrollbars = scrollingStateNode.useDarkAppearanceForScrollbars();
        horizontalScroller->setUseDarkAppearance(useDarkAppearanceForScrollbars);
        verticalScroller->setUseDarkAppearance(useDarkAppearanceForScrollbars);
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::OverlayScrollbarsEnabled)) {
        if (auto* scrollingStateFrameScrollingNode = dynamicDowncast<ScrollingStateFrameScrollingNode>(&scrollingStateNode)) {
            auto overlayScrollbarsEnabled = scrollingStateFrameScrollingNode->overlayScrollbarsEnabled();
            horizontalScroller->setOverlayScrollbarEnabled(overlayScrollbarsEnabled);
            verticalScroller->setOverlayScrollbarEnabled(overlayScrollbarsEnabled);
        }
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollbarOpacity)) {
        float scrollbarOpacity = scrollingStateNode.scrollbarOpacity();
        horizontalScroller->setOpacity(scrollbarOpacity);
        verticalScroller->setOpacity(scrollbarOpacity);
    }

    ThreadedScrollingTreeScrollingNodeDelegate::updateFromStateNode(scrollingStateNode);
}
#endif

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
