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
#include "ScrollExtents.h"
#include "ScrollingTreeFrameScrollingNode.h"

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebCore {
class ScrollAnimation;
class ScrollAnimationKinetic;

ScrollingTreeScrollingNodeDelegateNicosia::ScrollingTreeScrollingNodeDelegateNicosia(ScrollingTreeScrollingNode& scrollingNode, bool scrollAnimatorEnabled)
    : ScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_scrollController(*this)
    , m_scrollAnimatorEnabled(scrollAnimatorEnabled)
{
}

ScrollingTreeScrollingNodeDelegateNicosia::~ScrollingTreeScrollingNodeDelegateNicosia() = default;

void ScrollingTreeScrollingNodeDelegateNicosia::updateFromStateNode(const ScrollingStateScrollingNode& scrollingStateNode)
{
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::SnapOffsetsInfo))
        m_scrollController.setSnapOffsetsInfo(scrollingStateNode.snapOffsetsInfo().convertUnits<LayoutScrollSnapOffsetsInfo>());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CurrentHorizontalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Horizontal, scrollingStateNode.currentHorizontalSnapPointIndex());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CurrentVerticalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Vertical, scrollingStateNode.currentVerticalSnapPointIndex());
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

void ScrollingTreeScrollingNodeDelegateNicosia::updateVisibleLengths()
{
    m_scrollController.contentsSizeChanged();
}

WheelEventHandlingResult ScrollingTreeScrollingNodeDelegateNicosia::handleWheelEvent(const PlatformWheelEvent& wheelEvent, EventTargeting eventTargeting)
{
    bool wasInUserScroll = m_scrollController.isUserScrollInProgress();
    bool handled = scrollingNode().canHandleWheelEvent(wheelEvent, eventTargeting) && m_scrollController.handleWheelEvent(wheelEvent);
    bool isInUserScroll = m_scrollController.isUserScrollInProgress();

    if (isInUserScroll != wasInUserScroll)
        scrollingNode().setUserScrollInProgress(isInUserScroll);

    return handled ? WheelEventHandlingResult::handled() : WheelEventHandlingResult::unhandled();
}

std::unique_ptr<ScrollingEffectsControllerTimer> ScrollingTreeScrollingNodeDelegateNicosia::createTimer(Function<void()>&& function)
{
    return makeUnique<ScrollingEffectsControllerTimer>(RunLoop::current(), [function = WTFMove(function), protectedNode = Ref { scrollingNode() }] {
        Locker locker { protectedNode->scrollingTree().treeLock() };
        function();
    });
}

void ScrollingTreeScrollingNodeDelegateNicosia::startAnimationCallback(ScrollingEffectsController&)
{
    scrollingNode().setScrollAnimationInProgress(true);
}

void ScrollingTreeScrollingNodeDelegateNicosia::stopAnimationCallback(ScrollingEffectsController&)
{
    scrollingNode().setScrollAnimationInProgress(false);
}

void ScrollingTreeScrollingNodeDelegateNicosia::serviceScrollAnimation(MonotonicTime currentTime)
{
    m_scrollController.animationCallback(currentTime);
}

bool ScrollingTreeScrollingNodeDelegateNicosia::allowsHorizontalScrolling() const
{
    return ScrollingTreeScrollingNodeDelegate::allowsHorizontalScrolling();
}

bool ScrollingTreeScrollingNodeDelegateNicosia::allowsVerticalScrolling() const
{
    return ScrollingTreeScrollingNodeDelegate::allowsVerticalScrolling();
}

void ScrollingTreeScrollingNodeDelegateNicosia::immediateScrollBy(const FloatSize& delta, ScrollClamping clamping)
{
    auto updateScope = createUpdateScope();
    scrollingNode().scrollBy(delta, clamping);
}

void ScrollingTreeScrollingNodeDelegateNicosia::adjustScrollPositionToBoundsIfNecessary()
{
    FloatPoint scrollPosition = currentScrollPosition();
    FloatPoint constrainedPosition = scrollPosition.constrainedBetween(minimumScrollPosition(), maximumScrollPosition());
    immediateScrollBy(constrainedPosition - scrollPosition);
}

FloatPoint ScrollingTreeScrollingNodeDelegateNicosia::scrollOffset() const
{
    return ScrollableArea::scrollOffsetFromPosition(currentScrollPosition(), scrollOrigin());
}

void ScrollingTreeScrollingNodeDelegateNicosia::willStartScrollSnapAnimation()
{
    scrollingNode().setScrollSnapInProgress(true);
}

void ScrollingTreeScrollingNodeDelegateNicosia::didStopScrollSnapAnimation()
{
    scrollingNode().setScrollSnapInProgress(false);
}

void ScrollingTreeScrollingNodeDelegateNicosia::didStopAnimatedScroll()
{
    scrollingNode().didStopAnimatedScroll();
}

float ScrollingTreeScrollingNodeDelegateNicosia::pageScaleFactor() const
{
    // FIXME: What should this return for non-root frames, and overflow?
    // Also, this should not have to access ScrollingTreeFrameScrollingNode.
    return is<ScrollingTreeFrameScrollingNode>(scrollingNode()) ?
        downcast<ScrollingTreeFrameScrollingNode>(scrollingNode()).frameScaleFactor() : 1.;
}

ScrollExtents ScrollingTreeScrollingNodeDelegateNicosia::scrollExtents() const
{
    return {
        scrollingNode().totalContentsSize(),
        scrollingNode().scrollableAreaSize()
    };
}

bool ScrollingTreeScrollingNodeDelegateNicosia::startAnimatedScrollToPosition(FloatPoint destinationPosition)
{
    auto currentOffset = ScrollableArea::scrollOffsetFromPosition(currentScrollPosition(), scrollOrigin());
    auto destinationOffset = ScrollableArea::scrollOffsetFromPosition(destinationPosition, scrollOrigin());
    
    return m_scrollController.startAnimatedScrollToDestination(currentOffset, destinationOffset);
}

void ScrollingTreeScrollingNodeDelegateNicosia::stopAnimatedScroll()
{
    m_scrollController.stopAnimatedScroll();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
