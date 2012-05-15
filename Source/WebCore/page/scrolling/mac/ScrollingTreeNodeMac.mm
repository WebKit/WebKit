/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "ScrollingTreeNodeMac.h"

#if ENABLE(THREADED_SCROLLING)

#include "PlatformWheelEvent.h"
#include "ScrollingTree.h"
#include "ScrollingTreeState.h"

namespace WebCore {

PassOwnPtr<ScrollingTreeNode> ScrollingTreeNode::create(ScrollingTree* scrollingTree)
{
    return adoptPtr(new ScrollingTreeNodeMac(scrollingTree));
}

ScrollingTreeNodeMac::ScrollingTreeNodeMac(ScrollingTree* scrollingTree)
    : ScrollingTreeNode(scrollingTree)
    , m_scrollElasticityController(this)
{
}

ScrollingTreeNodeMac::~ScrollingTreeNodeMac()
{
    if (m_snapRubberbandTimer)
        CFRunLoopTimerInvalidate(m_snapRubberbandTimer.get());
}

void ScrollingTreeNodeMac::update(ScrollingTreeState* state)
{
    ScrollingTreeNode::update(state);

    if (state->changedProperties() & ScrollingTreeState::ScrollLayer)
        m_scrollLayer = state->platformScrollLayer();

    if (state->changedProperties() & ScrollingTreeState::RequestedScrollPosition)
        setScrollPosition(state->requestedScrollPosition());

    if (state->changedProperties() & (ScrollingTreeState::ScrollLayer | ScrollingTreeState::ContentsSize | ScrollingTreeState::ViewportRect))
        updateMainFramePinState(scrollPosition());

    if ((state->changedProperties() & ScrollingTreeState::ShouldUpdateScrollLayerPositionOnMainThread) && shouldUpdateScrollLayerPositionOnMainThread()) {
        // We're transitioning to the slow "update scroll layer position on the main thread" mode.
        // Initialize the probable main thread scroll position with the current scroll layer position.
        if (state->changedProperties() & ScrollingTreeState::RequestedScrollPosition)
            m_probableMainThreadScrollPosition = state->requestedScrollPosition();
        else {
            CGPoint scrollLayerPosition = m_scrollLayer.get().position;
            m_probableMainThreadScrollPosition = IntPoint(-scrollLayerPosition.x, -scrollLayerPosition.y);
        }
    }
}

void ScrollingTreeNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHaveScrollbars())
        return;

    m_scrollElasticityController.handleWheelEvent(wheelEvent);
    scrollingTree()->handleWheelEventPhase(wheelEvent.phase());
}

bool ScrollingTreeNodeMac::allowsHorizontalStretching()
{
    switch (horizontalScrollElasticity()) {
    case ScrollElasticityAutomatic:
        return hasEnabledHorizontalScrollbar() || !hasEnabledVerticalScrollbar();
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollingTreeNodeMac::allowsVerticalStretching()
{
    switch (verticalScrollElasticity()) {
    case ScrollElasticityAutomatic:
        return hasEnabledVerticalScrollbar() || !hasEnabledHorizontalScrollbar();
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

IntSize ScrollingTreeNodeMac::stretchAmount()
{
    IntSize stretch;

    if (scrollPosition().y() < minimumScrollPosition().y())
        stretch.setHeight(scrollPosition().y() - minimumScrollPosition().y());
    else if (scrollPosition().y() > maximumScrollPosition().y())
        stretch.setHeight(scrollPosition().y() - maximumScrollPosition().y());

    if (scrollPosition().x() < minimumScrollPosition().x())
        stretch.setWidth(scrollPosition().x() - minimumScrollPosition().x());
    else if (scrollPosition().x() > maximumScrollPosition().x())
        stretch.setWidth(scrollPosition().x() - maximumScrollPosition().x());

    return stretch;
}

bool ScrollingTreeNodeMac::pinnedInDirection(const FloatSize& delta)
{
    FloatSize limitDelta;

    if (fabsf(delta.height()) >= fabsf(delta.width())) {
        if (delta.height() < 0) {
            // We are trying to scroll up.  Make sure we are not pinned to the top
            limitDelta.setHeight(scrollPosition().y() - minimumScrollPosition().y());
        } else {
            // We are trying to scroll down.  Make sure we are not pinned to the bottom
            limitDelta.setHeight(maximumScrollPosition().y() - scrollPosition().y());
        }
    } else if (delta.width()) {
        if (delta.width() < 0) {
            // We are trying to scroll left.  Make sure we are not pinned to the left
            limitDelta.setHeight(scrollPosition().x() - minimumScrollPosition().x());
        } else {
            // We are trying to scroll right.  Make sure we are not pinned to the right
            limitDelta.setHeight(maximumScrollPosition().x() - scrollPosition().x());
        }
    }

    if ((delta.width() || delta.height()) && (limitDelta.width() < 1 && limitDelta.height() < 1))        
        return true;

    return false;
}

bool ScrollingTreeNodeMac::canScrollHorizontally()
{
    return hasEnabledHorizontalScrollbar();
}

bool ScrollingTreeNodeMac::canScrollVertically()
{
    return hasEnabledVerticalScrollbar();
}

bool ScrollingTreeNodeMac::shouldRubberBandInDirection(ScrollDirection direction)
{
    if (direction == ScrollLeft)
        return !scrollingTree()->canGoBack();
    if (direction == ScrollRight)
        return !scrollingTree()->canGoForward();

    ASSERT_NOT_REACHED();
    return false;
}

IntPoint ScrollingTreeNodeMac::absoluteScrollPosition()
{
    return scrollPosition();
}

void ScrollingTreeNodeMac::immediateScrollBy(const FloatSize& offset)
{
    scrollBy(roundedIntSize(offset));
}

void ScrollingTreeNodeMac::immediateScrollByWithoutContentEdgeConstraints(const FloatSize& offset)
{
    scrollByWithoutContentEdgeConstraints(roundedIntSize(offset));
}

void ScrollingTreeNodeMac::startSnapRubberbandTimer()
{
    ASSERT(!m_snapRubberbandTimer);

    CFTimeInterval timerInterval = 1.0 / 60.0;

    m_snapRubberbandTimer = adoptCF(CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + timerInterval, timerInterval, 0, 0, ^(CFRunLoopTimerRef) {
        m_scrollElasticityController.snapRubberBandTimerFired();
    }));
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), m_snapRubberbandTimer.get(), kCFRunLoopDefaultMode);
}

void ScrollingTreeNodeMac::stopSnapRubberbandTimer()
{
    if (!m_snapRubberbandTimer)
        return;

    CFRunLoopTimerInvalidate(m_snapRubberbandTimer.get());
    m_snapRubberbandTimer = nullptr;
}

IntPoint ScrollingTreeNodeMac::scrollPosition() const
{
    if (shouldUpdateScrollLayerPositionOnMainThread())
        return m_probableMainThreadScrollPosition;

    CGPoint scrollLayerPosition = m_scrollLayer.get().position;
    return IntPoint(-scrollLayerPosition.x + scrollOrigin().x(), -scrollLayerPosition.y + scrollOrigin().y());
}

void ScrollingTreeNodeMac::setScrollPosition(const IntPoint& scrollPosition)
{
    IntPoint newScrollPosition = scrollPosition;
    newScrollPosition = newScrollPosition.shrunkTo(maximumScrollPosition());
    newScrollPosition = newScrollPosition.expandedTo(minimumScrollPosition());

    setScrollPositionWithoutContentEdgeConstraints(newScrollPosition);
}

void ScrollingTreeNodeMac::setScrollPositionWithoutContentEdgeConstraints(const IntPoint& scrollPosition)
{
    updateMainFramePinState(scrollPosition);

    if (shouldUpdateScrollLayerPositionOnMainThread()) {
        m_probableMainThreadScrollPosition = scrollPosition;
        scrollingTree()->updateMainFrameScrollPositionAndScrollLayerPosition(scrollPosition);
        return;
    }

    setScrollLayerPosition(scrollPosition);
    scrollingTree()->updateMainFrameScrollPosition(scrollPosition);
}

void ScrollingTreeNodeMac::setScrollLayerPosition(const IntPoint& position)
{
    ASSERT(!shouldUpdateScrollLayerPositionOnMainThread());
    m_scrollLayer.get().position = CGPointMake(-position.x() + scrollOrigin().x(), -position.y() + scrollOrigin().y());
}

IntPoint ScrollingTreeNodeMac::minimumScrollPosition() const
{
    return IntPoint(0, 0);
}

IntPoint ScrollingTreeNodeMac::maximumScrollPosition() const
{
    IntPoint position(contentsSize().width() - viewportRect().width(),
                      contentsSize().height() - viewportRect().height());

    position.clampNegativeToZero();

    return position;
}

void ScrollingTreeNodeMac::scrollBy(const IntSize& offset)
{
    setScrollPosition(scrollPosition() + offset);
}

void ScrollingTreeNodeMac::scrollByWithoutContentEdgeConstraints(const IntSize& offset)
{
    setScrollPositionWithoutContentEdgeConstraints(scrollPosition() + offset);
}

void ScrollingTreeNodeMac::updateMainFramePinState(const IntPoint& scrollPosition)
{
    bool pinnedToTheLeft = scrollPosition.x() <= minimumScrollPosition().x();
    bool pinnedToTheRight = scrollPosition.x() >= maximumScrollPosition().x();

    scrollingTree()->setMainFramePinState(pinnedToTheLeft, pinnedToTheRight);
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
