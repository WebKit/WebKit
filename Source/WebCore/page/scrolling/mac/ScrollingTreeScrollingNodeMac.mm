/*
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
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
#import "ScrollingTreeScrollingNodeMac.h"

#if ENABLE(ASYNC_SCROLLING)

#import "FrameView.h"
#import "NSScrollerImpDetails.h"
#import "PlatformWheelEvent.h"
#import "ScrollingCoordinator.h"
#import "ScrollingTree.h"
#import "ScrollingStateTree.h"
#import "Settings.h"
#import "TileController.h"
#import "WebLayer.h"

#import <QuartzCore/QuartzCore.h>
#import <wtf/CurrentTime.h>
#import <wtf/Deque.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/CString.h>

namespace WebCore {

static void logThreadedScrollingMode(unsigned synchronousScrollingReasons);
static void logWheelEventHandlerCountChanged(unsigned);


PassOwnPtr<ScrollingTreeScrollingNode> ScrollingTreeScrollingNodeMac::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptPtr(new ScrollingTreeScrollingNodeMac(scrollingTree, nodeID));
}

ScrollingTreeScrollingNodeMac::ScrollingTreeScrollingNodeMac(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeScrollingNode(scrollingTree, nodeID)
    , m_scrollElasticityController(this)
    , m_verticalScrollbarPainter(0)
    , m_horizontalScrollbarPainter(0)
    , m_lastScrollHadUnfilledPixels(false)
{
}

ScrollingTreeScrollingNodeMac::~ScrollingTreeScrollingNodeMac()
{
    if (m_snapRubberbandTimer)
        CFRunLoopTimerInvalidate(m_snapRubberbandTimer.get());
}

void ScrollingTreeScrollingNodeMac::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeScrollingNode::updateBeforeChildren(stateNode);
    const auto& scrollingStateNode = toScrollingStateScrollingNode(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_scrollLayer = scrollingStateNode.layer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
        m_scrolledContentsLayer = scrollingStateNode.scrolledContentsLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::CounterScrollingLayer))
        m_counterScrollingLayer = scrollingStateNode.counterScrollingLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::HeaderLayer))
        m_headerLayer = scrollingStateNode.headerLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::FooterLayer))
        m_footerLayer = scrollingStateNode.footerLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::PainterForScrollbar)) {
        m_verticalScrollbarPainter = scrollingStateNode.verticalScrollbarPainter();
        m_horizontalScrollbarPainter = scrollingStateNode.horizontalScrollbarPainter();
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ReasonsForSynchronousScrolling)) {
        if (shouldUpdateScrollLayerPositionSynchronously()) {
            // We're transitioning to the slow "update scroll layer position on the main thread" mode.
            // Initialize the probable main thread scroll position with the current scroll layer position.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
                m_probableMainThreadScrollPosition = scrollingStateNode.requestedScrollPosition();
            else {
                CGPoint scrollLayerPosition = m_scrollLayer.get().position;
                m_probableMainThreadScrollPosition = IntPoint(-scrollLayerPosition.x, -scrollLayerPosition.y);
            }
        }

        if (scrollingTree().scrollingPerformanceLoggingEnabled())
            logThreadedScrollingMode(synchronousScrollingReasons());
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::WheelEventHandlerCount)) {
        if (scrollingTree().scrollingPerformanceLoggingEnabled())
            logWheelEventHandlerCountChanged(scrollingStateNode.wheelEventHandlerCount());
    }
}

void ScrollingTreeScrollingNodeMac::updateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeScrollingNode::updateAfterChildren(stateNode);

    const auto& scrollingStateNode = toScrollingStateScrollingNode(stateNode);

    // Update the scroll position after child nodes have been updated, because they need to have updated their constraints before any scrolling happens.
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
        setScrollPosition(scrollingStateNode.requestedScrollPosition());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer) || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize) || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ViewportSize))
        updateMainFramePinState(scrollPosition());
}

void ScrollingTreeScrollingNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHaveScrollbars())
        return;

    if (wheelEvent.phase() == PlatformWheelEventPhaseEnded || wheelEvent.phase() == PlatformWheelEventPhaseCancelled) {
        // If the wheel event is ending or cancelled, then we can tell the ScrollbarPainter API that we won't
        // be updating the position from our scrolling thread anymore for the time being.
        if (m_verticalScrollbarPainter)
            [m_verticalScrollbarPainter setUsePresentationValue:NO];
        if (m_horizontalScrollbarPainter)
            [m_horizontalScrollbarPainter setUsePresentationValue:NO];
    }

    m_scrollElasticityController.handleWheelEvent(wheelEvent);
    scrollingTree().setOrClearLatchedNode(wheelEvent, scrollingNodeID());
    scrollingTree().handleWheelEventPhase(wheelEvent.phase());
}

bool ScrollingTreeScrollingNodeMac::allowsHorizontalStretching()
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

bool ScrollingTreeScrollingNodeMac::allowsVerticalStretching()
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

IntSize ScrollingTreeScrollingNodeMac::stretchAmount()
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

    if (scrollingTree().rootNode() == this) {
        if (stretch.isZero())
            scrollingTree().setMainFrameIsRubberBanding(false);
        else
            scrollingTree().setMainFrameIsRubberBanding(true);
    }

    return stretch;
}

bool ScrollingTreeScrollingNodeMac::pinnedInDirection(const FloatSize& delta)
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
            limitDelta.setWidth(scrollPosition().x() - minimumScrollPosition().x());
        } else {
            // We are trying to scroll right.  Make sure we are not pinned to the right
            limitDelta.setWidth(maximumScrollPosition().x() - scrollPosition().x());
        }
    }

    if ((delta.width() || delta.height()) && (limitDelta.width() < 1 && limitDelta.height() < 1))
        return true;

    return false;
}

bool ScrollingTreeScrollingNodeMac::canScrollHorizontally()
{
    return hasEnabledHorizontalScrollbar();
}

bool ScrollingTreeScrollingNodeMac::canScrollVertically()
{
    return hasEnabledVerticalScrollbar();
}

bool ScrollingTreeScrollingNodeMac::shouldRubberBandInDirection(ScrollDirection)
{
    return true;
}

IntPoint ScrollingTreeScrollingNodeMac::absoluteScrollPosition()
{
    return roundedIntPoint(scrollPosition());
}

void ScrollingTreeScrollingNodeMac::immediateScrollBy(const FloatSize& offset)
{
    scrollBy(roundedIntSize(offset));
}

void ScrollingTreeScrollingNodeMac::immediateScrollByWithoutContentEdgeConstraints(const FloatSize& offset)
{
    scrollByWithoutContentEdgeConstraints(roundedIntSize(offset));
}

void ScrollingTreeScrollingNodeMac::startSnapRubberbandTimer()
{
    ASSERT(!m_snapRubberbandTimer);

    CFTimeInterval timerInterval = 1.0 / 60.0;

    m_snapRubberbandTimer = adoptCF(CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + timerInterval, timerInterval, 0, 0, ^(CFRunLoopTimerRef) {
        m_scrollElasticityController.snapRubberBandTimerFired();
    }));
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), m_snapRubberbandTimer.get(), kCFRunLoopDefaultMode);
}

void ScrollingTreeScrollingNodeMac::stopSnapRubberbandTimer()
{
    if (!m_snapRubberbandTimer)
        return;

    scrollingTree().setMainFrameIsRubberBanding(false);

    // Since the rubberband timer has stopped, totalContentsSizeForRubberBand can be synchronized with totalContentsSize.
    setTotalContentsSizeForRubberBand(totalContentsSize());

    CFRunLoopTimerInvalidate(m_snapRubberbandTimer.get());
    m_snapRubberbandTimer = nullptr;
}

void ScrollingTreeScrollingNodeMac::adjustScrollPositionToBoundsIfNecessary()
{
    FloatPoint currentScrollPosition = absoluteScrollPosition();
    FloatPoint minPosition = minimumScrollPosition();
    FloatPoint maxPosition = maximumScrollPosition();

    float nearestXWithinBounds = std::max(std::min(currentScrollPosition.x(), maxPosition.x()), minPosition.x());
    float nearestYWithinBounds = std::max(std::min(currentScrollPosition.y(), maxPosition.y()), minPosition.y());

    FloatPoint nearestPointWithinBounds(nearestXWithinBounds, nearestYWithinBounds);
    immediateScrollBy(nearestPointWithinBounds - currentScrollPosition);
}

FloatPoint ScrollingTreeScrollingNodeMac::scrollPosition() const
{
    if (shouldUpdateScrollLayerPositionSynchronously())
        return m_probableMainThreadScrollPosition;

    CGPoint scrollLayerPosition = m_scrollLayer.get().position;
    return IntPoint(-scrollLayerPosition.x + scrollOrigin().x(), -scrollLayerPosition.y + scrollOrigin().y());
}

void ScrollingTreeScrollingNodeMac::setScrollPosition(const FloatPoint& scrollPosition)
{
    FloatPoint newScrollPosition = scrollPosition;
    newScrollPosition = newScrollPosition.shrunkTo(maximumScrollPosition());
    newScrollPosition = newScrollPosition.expandedTo(minimumScrollPosition());

    setScrollPositionWithoutContentEdgeConstraints(newScrollPosition);

    if (scrollingTree().scrollingPerformanceLoggingEnabled())
        logExposedUnfilledArea();
}

void ScrollingTreeScrollingNodeMac::setScrollPositionWithoutContentEdgeConstraints(const FloatPoint& scrollPosition)
{
    updateMainFramePinState(scrollPosition);

    if (shouldUpdateScrollLayerPositionSynchronously()) {
        m_probableMainThreadScrollPosition = scrollPosition;
        scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition, SetScrollingLayerPosition);
        return;
    }

    setScrollLayerPosition(scrollPosition);
    scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition);
}

void ScrollingTreeScrollingNodeMac::setScrollLayerPosition(const FloatPoint& position)
{
    ASSERT(!shouldUpdateScrollLayerPositionSynchronously());
    m_scrollLayer.get().position = CGPointMake(-position.x() + scrollOrigin().x(), -position.y() + scrollOrigin().y());

    ScrollBehaviorForFixedElements behaviorForFixed = scrollBehaviorForFixedElements();
    FloatPoint scrollOffset = position - toFloatSize(scrollOrigin());
    FloatRect viewportRect(FloatPoint(), viewportSize());
    
    // FIXME: scrollOffsetForFixedPosition() needs to do float math.
    FloatSize scrollOffsetForFixedChildren = FrameView::scrollOffsetForFixedPosition(enclosingLayoutRect(viewportRect),
        totalContentsSize(), flooredIntPoint(scrollOffset), scrollOrigin(), frameScaleFactor(), false, behaviorForFixed, headerHeight(), footerHeight());
    
    if (m_counterScrollingLayer)
        m_counterScrollingLayer.get().position = FloatPoint(scrollOffsetForFixedChildren);

    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute scrollOffsetForFixedChildren for the banner with a scale factor of 1.
        float horizontalScrollOffsetForBanner = scrollOffsetForFixedChildren.width();
        if (frameScaleFactor() != 1)
            horizontalScrollOffsetForBanner = FrameView::scrollOffsetForFixedPosition(enclosingLayoutRect(viewportRect), totalContentsSize(), flooredIntPoint(scrollOffset), scrollOrigin(), 1, false, behaviorForFixed, headerHeight(), footerHeight()).width();

        if (m_headerLayer)
            m_headerLayer.get().position = FloatPoint(horizontalScrollOffsetForBanner, 0);

        if (m_footerLayer)
            m_footerLayer.get().position = FloatPoint(horizontalScrollOffsetForBanner, totalContentsSize().height() - footerHeight());
    }

    if (m_verticalScrollbarPainter || m_horizontalScrollbarPainter) {
        [CATransaction begin];
        [CATransaction lock];

        if (m_verticalScrollbarPainter) {
            [m_verticalScrollbarPainter setUsePresentationValue:YES];
            float presentationValue;
            float overhangAmount;
            ScrollableArea::computeScrollbarValueAndOverhang(position.y(), totalContentsSize().height(), viewportRect.height(), presentationValue, overhangAmount);
            [m_verticalScrollbarPainter setPresentationValue:presentationValue];
        }

        if (m_horizontalScrollbarPainter) {
            [m_horizontalScrollbarPainter setUsePresentationValue:YES];
            float presentationValue;
            float overhangAmount;
            ScrollableArea::computeScrollbarValueAndOverhang(position.x(), totalContentsSize().width(), viewportRect.width(), presentationValue, overhangAmount);
            [m_horizontalScrollbarPainter setPresentationValue:presentationValue];
        }
        [CATransaction unlock];
        [CATransaction commit];
    }

    if (!m_children)
        return;

    viewportRect.setLocation(FloatPoint() + scrollOffsetForFixedChildren);

    size_t size = m_children->size();
    for (size_t i = 0; i < size; ++i)
        m_children->at(i)->parentScrollPositionDidChange(viewportRect, FloatSize());
}

FloatPoint ScrollingTreeScrollingNodeMac::minimumScrollPosition() const
{
    IntPoint position;
    
    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToBottom)
        position.setY(maximumScrollPosition().y());

    return position;
}

FloatPoint ScrollingTreeScrollingNodeMac::maximumScrollPosition() const
{
    FloatPoint position(totalContentsSizeForRubberBand().width() - viewportSize().width(),
        totalContentsSizeForRubberBand().height() - viewportSize().height());

    position = position.expandedTo(FloatPoint());

    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToTop)
        position.setY(minimumScrollPosition().y());

    return position;
}

void ScrollingTreeScrollingNodeMac::scrollBy(const IntSize& offset)
{
    setScrollPosition(scrollPosition() + offset);
}

void ScrollingTreeScrollingNodeMac::scrollByWithoutContentEdgeConstraints(const IntSize& offset)
{
    setScrollPositionWithoutContentEdgeConstraints(scrollPosition() + offset);
}

void ScrollingTreeScrollingNodeMac::updateMainFramePinState(const FloatPoint& scrollPosition)
{
    bool pinnedToTheLeft = scrollPosition.x() <= minimumScrollPosition().x();
    bool pinnedToTheRight = scrollPosition.x() >= maximumScrollPosition().x();
    bool pinnedToTheTop = scrollPosition.y() <= minimumScrollPosition().y();
    bool pinnedToTheBottom = scrollPosition.y() >= maximumScrollPosition().y();

    scrollingTree().setMainFramePinState(pinnedToTheLeft, pinnedToTheRight, pinnedToTheTop, pinnedToTheBottom);
}

void ScrollingTreeScrollingNodeMac::logExposedUnfilledArea()
{
    Region paintedVisibleTiles;

    Deque<CALayer*> layerQueue;
    layerQueue.append(m_scrollLayer.get());
    PlatformLayerList tiles;

    while (!layerQueue.isEmpty() && tiles.isEmpty()) {
        CALayer* layer = layerQueue.takeFirst();
        NSArray* sublayers = [[layer sublayers] copy];

        // If this layer is the parent of a tile, it is the parent of all of the tiles and nothing else.
        if ([[[sublayers objectAtIndex:0] valueForKey:@"isTile"] boolValue]) {
            for (CALayer* sublayer in sublayers)
                tiles.append(sublayer);
        } else {
            for (CALayer* sublayer in sublayers)
                layerQueue.append(sublayer);
        }

        [sublayers release];
    }

    FloatPoint scrollPosition = this->scrollPosition();
    FloatRect viewPortRect(scrollPosition, viewportSize());
    unsigned unfilledArea = TileController::blankPixelCountForTiles(tiles, viewPortRect, IntPoint(-scrollPosition.x(), -scrollPosition.y()));

    if (unfilledArea || m_lastScrollHadUnfilledPixels)
        WTFLogAlways("SCROLLING: Exposed tileless area. Time: %f Unfilled Pixels: %u\n", WTF::monotonicallyIncreasingTime(), unfilledArea);

    m_lastScrollHadUnfilledPixels = unfilledArea;
}

static void logThreadedScrollingMode(unsigned synchronousScrollingReasons)
{
    if (synchronousScrollingReasons) {
        StringBuilder reasonsDescription;

        if (synchronousScrollingReasons & ScrollingCoordinator::ForcedOnMainThread)
            reasonsDescription.append("forced,");
        if (synchronousScrollingReasons & ScrollingCoordinator::HasSlowRepaintObjects)
            reasonsDescription.append("slow-repaint objects,");
        if (synchronousScrollingReasons & ScrollingCoordinator::HasViewportConstrainedObjectsWithoutSupportingFixedLayers)
            reasonsDescription.append("viewport-constrained objects,");
        if (synchronousScrollingReasons & ScrollingCoordinator::HasNonLayerViewportConstrainedObjects)
            reasonsDescription.append("non-layer viewport-constrained objects,");
        if (synchronousScrollingReasons & ScrollingCoordinator::IsImageDocument)
            reasonsDescription.append("image document,");

        // Strip the trailing comma.
        String reasonsDescriptionTrimmed = reasonsDescription.toString().left(reasonsDescription.length() - 1);

        WTFLogAlways("SCROLLING: Switching to main-thread scrolling mode. Time: %f Reason(s): %s\n", WTF::monotonicallyIncreasingTime(), reasonsDescriptionTrimmed.ascii().data());
    } else
        WTFLogAlways("SCROLLING: Switching to threaded scrolling mode. Time: %f\n", WTF::monotonicallyIncreasingTime());
}

void logWheelEventHandlerCountChanged(unsigned count)
{
    WTFLogAlways("SCROLLING: Wheel event handler count changed. Time: %f Count: %u\n", WTF::monotonicallyIncreasingTime(), count);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
