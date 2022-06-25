/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ScrollingTreeOverflowScrollingNodeMac.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#import "Logging.h"
#import "ScrollingStateOverflowScrollingNode.h"
#import "ScrollingTree.h"
#import "WebCoreCALayerExtras.h"
#import <wtf/BlockObjCExceptions.h>
#import <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeOverflowScrollingNodeMac> ScrollingTreeOverflowScrollingNodeMac::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollingNodeMac(scrollingTree, nodeID));
}

ScrollingTreeOverflowScrollingNodeMac::ScrollingTreeOverflowScrollingNodeMac(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollingNode(scrollingTree, nodeID)
    , m_delegate(*this)
{
}

ScrollingTreeOverflowScrollingNodeMac::~ScrollingTreeOverflowScrollingNodeMac() = default;

void ScrollingTreeOverflowScrollingNodeMac::willBeDestroyed()
{
    m_delegate.nodeWillBeDestroyed();
}

void ScrollingTreeOverflowScrollingNodeMac::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeOverflowScrollingNode::commitStateBeforeChildren(stateNode);
    m_delegate.updateFromStateNode(downcast<ScrollingStateOverflowScrollingNode>(stateNode));
}

WheelEventHandlingResult ScrollingTreeOverflowScrollingNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent, EventTargeting eventTargeting)
{
#if ENABLE(SCROLLING_THREAD)
    if (hasSynchronousScrollingReasons() && eventTargeting != EventTargeting::NodeOnly)
        return { { WheelEventProcessingSteps::MainThreadForScrolling, WheelEventProcessingSteps::MainThreadForNonBlockingDOMEventDispatch }, false };
#endif

    if (!canHandleWheelEvent(wheelEvent, eventTargeting))
        return WheelEventHandlingResult::unhandled();

    return WheelEventHandlingResult::result(m_delegate.handleWheelEvent(wheelEvent));
}

bool ScrollingTreeOverflowScrollingNodeMac::startAnimatedScrollToPosition(FloatPoint destinationPosition)
{
    bool started = m_delegate.startAnimatedScrollToPosition(destinationPosition);
    if (started)
        willStartAnimatedScroll();
    return started;
}

void ScrollingTreeOverflowScrollingNodeMac::stopAnimatedScroll()
{
    m_delegate.stopAnimatedScroll();
}

void ScrollingTreeOverflowScrollingNodeMac::serviceScrollAnimation(MonotonicTime currentTime)
{
    m_delegate.serviceScrollAnimation(currentTime);
}

void ScrollingTreeOverflowScrollingNodeMac::willDoProgrammaticScroll(const FloatPoint& targetScrollPosition)
{
    m_delegate.willDoProgrammaticScroll(targetScrollPosition);
}

void ScrollingTreeOverflowScrollingNodeMac::currentScrollPositionChanged(ScrollType scrollType, ScrollingLayerPositionAction action)
{
    ScrollingTreeOverflowScrollingNode::currentScrollPositionChanged(scrollType, action);
    m_delegate.currentScrollPositionChanged();
}

FloatPoint ScrollingTreeOverflowScrollingNodeMac::adjustedScrollPosition(const FloatPoint& position, ScrollClamping clamp) const
{
    FloatPoint scrollPosition(roundf(position.x()), roundf(position.y()));
    return ScrollingTreeOverflowScrollingNode::adjustedScrollPosition(scrollPosition, clamp);
}

void ScrollingTreeOverflowScrollingNodeMac::repositionScrollingLayers()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [static_cast<CALayer*>(scrollContainerLayer()) _web_setLayerBoundsOrigin:currentScrollOffset()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeOverflowScrollingNodeMac::repositionRelatedLayers()
{
    m_delegate.updateScrollbarPainters();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
