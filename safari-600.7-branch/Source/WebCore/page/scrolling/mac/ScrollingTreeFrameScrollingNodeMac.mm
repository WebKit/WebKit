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
#import "ScrollingTreeFrameScrollingNodeMac.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

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


PassRefPtr<ScrollingTreeFrameScrollingNode> ScrollingTreeFrameScrollingNodeMac::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(new ScrollingTreeFrameScrollingNodeMac(scrollingTree, nodeID));
}

ScrollingTreeFrameScrollingNodeMac::ScrollingTreeFrameScrollingNodeMac(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNode(scrollingTree, nodeID)
    , m_scrollElasticityController(this)
    , m_verticalScrollbarPainter(0)
    , m_horizontalScrollbarPainter(0)
    , m_lastScrollHadUnfilledPixels(false)
{
}

ScrollingTreeFrameScrollingNodeMac::~ScrollingTreeFrameScrollingNodeMac()
{
    if (m_snapRubberbandTimer)
        CFRunLoopTimerInvalidate(m_snapRubberbandTimer.get());
}

void ScrollingTreeFrameScrollingNodeMac::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::updateBeforeChildren(stateNode);
    const auto& scrollingStateNode = toScrollingStateFrameScrollingNode(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_scrollLayer = scrollingStateNode.layer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ScrolledContentsLayer))
        m_scrolledContentsLayer = scrollingStateNode.scrolledContentsLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
        m_counterScrollingLayer = scrollingStateNode.counterScrollingLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer))
        m_insetClipLayer = scrollingStateNode.insetClipLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer))
        m_contentShadowLayer = scrollingStateNode.contentShadowLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
        m_headerLayer = scrollingStateNode.headerLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
        m_footerLayer = scrollingStateNode.footerLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::PainterForScrollbar)) {
        m_verticalScrollbarPainter = scrollingStateNode.verticalScrollbarPainter();
        m_horizontalScrollbarPainter = scrollingStateNode.horizontalScrollbarPainter();
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ReasonsForSynchronousScrolling)) {
        if (shouldUpdateScrollLayerPositionSynchronously()) {
            // We're transitioning to the slow "update scroll layer position on the main thread" mode.
            // Initialize the probable main thread scroll position with the current scroll layer position.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
                m_probableMainThreadScrollPosition = scrollingStateNode.requestedScrollPosition();
            else {
                CGPoint scrollLayerPosition = m_scrollLayer.get().position;
                m_probableMainThreadScrollPosition = FloatPoint(-scrollLayerPosition.x, -scrollLayerPosition.y);
            }
        }

        if (scrollingTree().scrollingPerformanceLoggingEnabled())
            logThreadedScrollingMode(synchronousScrollingReasons());
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::WheelEventHandlerCount)) {
        if (scrollingTree().scrollingPerformanceLoggingEnabled())
            logWheelEventHandlerCountChanged(scrollingStateNode.wheelEventHandlerCount());
    }
}

void ScrollingTreeFrameScrollingNodeMac::updateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::updateAfterChildren(stateNode);

    const auto& scrollingStateNode = toScrollingStateScrollingNode(stateNode);

    // Update the scroll position after child nodes have been updated, because they need to have updated their constraints before any scrolling happens.
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
        setScrollPosition(scrollingStateNode.requestedScrollPosition());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaSize))
        updateMainFramePinState(scrollPosition());
}

void ScrollingTreeFrameScrollingNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHaveScrollbars())
        return;

    if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseBegan) {
        [m_verticalScrollbarPainter setUsePresentationValue:YES];
        [m_horizontalScrollbarPainter setUsePresentationValue:YES];
    }
    if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseEnded || wheelEvent.momentumPhase() == PlatformWheelEventPhaseCancelled) {
        [m_verticalScrollbarPainter setUsePresentationValue:NO];
        [m_horizontalScrollbarPainter setUsePresentationValue:NO];
    }

    m_scrollElasticityController.handleWheelEvent(wheelEvent);
    scrollingTree().setOrClearLatchedNode(wheelEvent, scrollingNodeID());
    scrollingTree().handleWheelEventPhase(wheelEvent.phase());
}

bool ScrollingTreeFrameScrollingNodeMac::allowsHorizontalStretching()
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

bool ScrollingTreeFrameScrollingNodeMac::allowsVerticalStretching()
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

IntSize ScrollingTreeFrameScrollingNodeMac::stretchAmount()
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

bool ScrollingTreeFrameScrollingNodeMac::pinnedInDirection(const FloatSize& delta)
{
    FloatSize limitDelta;

    if (fabsf(delta.height()) >= fabsf(delta.width())) {
        if (delta.height() < 0) {
            // We are trying to scroll up. Make sure we are not pinned to the top.
            limitDelta.setHeight(scrollPosition().y() - minimumScrollPosition().y());
        } else {
            // We are trying to scroll down. Make sure we are not pinned to the bottom.
            limitDelta.setHeight(maximumScrollPosition().y() - scrollPosition().y());
        }
    } else if (delta.width()) {
        if (delta.width() < 0) {
            // We are trying to scroll left. Make sure we are not pinned to the left.
            limitDelta.setWidth(scrollPosition().x() - minimumScrollPosition().x());
        } else {
            // We are trying to scroll right. Make sure we are not pinned to the right.
            limitDelta.setWidth(maximumScrollPosition().x() - scrollPosition().x());
        }
    }

    if ((delta.width() || delta.height()) && (limitDelta.width() < 1 && limitDelta.height() < 1))
        return true;

    return false;
}

bool ScrollingTreeFrameScrollingNodeMac::canScrollHorizontally()
{
    return hasEnabledHorizontalScrollbar();
}

bool ScrollingTreeFrameScrollingNodeMac::canScrollVertically()
{
    return hasEnabledVerticalScrollbar();
}

bool ScrollingTreeFrameScrollingNodeMac::shouldRubberBandInDirection(ScrollDirection)
{
    return true;
}

IntPoint ScrollingTreeFrameScrollingNodeMac::absoluteScrollPosition()
{
    return roundedIntPoint(scrollPosition());
}

void ScrollingTreeFrameScrollingNodeMac::immediateScrollBy(const FloatSize& offset)
{
    scrollBy(offset);
}

void ScrollingTreeFrameScrollingNodeMac::immediateScrollByWithoutContentEdgeConstraints(const FloatSize& offset)
{
    scrollByWithoutContentEdgeConstraints(offset);
}

void ScrollingTreeFrameScrollingNodeMac::startSnapRubberbandTimer()
{
    ASSERT(!m_snapRubberbandTimer);

    CFTimeInterval timerInterval = 1.0 / 60.0;

    m_snapRubberbandTimer = adoptCF(CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + timerInterval, timerInterval, 0, 0, ^(CFRunLoopTimerRef) {
        m_scrollElasticityController.snapRubberBandTimerFired();
    }));
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), m_snapRubberbandTimer.get(), kCFRunLoopDefaultMode);
}

void ScrollingTreeFrameScrollingNodeMac::stopSnapRubberbandTimer()
{
    if (!m_snapRubberbandTimer)
        return;

    scrollingTree().setMainFrameIsRubberBanding(false);

    // Since the rubberband timer has stopped, totalContentsSizeForRubberBand can be synchronized with totalContentsSize.
    setTotalContentsSizeForRubberBand(totalContentsSize());

    CFRunLoopTimerInvalidate(m_snapRubberbandTimer.get());
    m_snapRubberbandTimer = nullptr;
}

void ScrollingTreeFrameScrollingNodeMac::adjustScrollPositionToBoundsIfNecessary()
{
    FloatPoint currentScrollPosition = absoluteScrollPosition();
    FloatPoint minPosition = minimumScrollPosition();
    FloatPoint maxPosition = maximumScrollPosition();

    float nearestXWithinBounds = std::max(std::min(currentScrollPosition.x(), maxPosition.x()), minPosition.x());
    float nearestYWithinBounds = std::max(std::min(currentScrollPosition.y(), maxPosition.y()), minPosition.y());

    FloatPoint nearestPointWithinBounds(nearestXWithinBounds, nearestYWithinBounds);
    immediateScrollBy(nearestPointWithinBounds - currentScrollPosition);
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::scrollPosition() const
{
    if (shouldUpdateScrollLayerPositionSynchronously())
        return m_probableMainThreadScrollPosition;

    CGPoint scrollLayerPosition = m_scrollLayer.get().position;
    return FloatPoint(-scrollLayerPosition.x + scrollOrigin().x(), -scrollLayerPosition.y + scrollOrigin().y());
}

void ScrollingTreeFrameScrollingNodeMac::setScrollPosition(const FloatPoint& scrollPosition)
{
    // Scroll deltas can be non-integral with some input devices, so scrollPosition may not be integral.
    // FIXME: when we support half-pixel scroll positions on Retina displays, this will need to round to half pixels.
    FloatPoint roundedPosition(roundf(scrollPosition.x()), roundf(scrollPosition.y()));

    ScrollingTreeFrameScrollingNode::setScrollPosition(roundedPosition);

    if (scrollingTree().scrollingPerformanceLoggingEnabled())
        logExposedUnfilledArea();
}

void ScrollingTreeFrameScrollingNodeMac::setScrollPositionWithoutContentEdgeConstraints(const FloatPoint& scrollPosition)
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

void ScrollingTreeFrameScrollingNodeMac::setScrollLayerPosition(const FloatPoint& position)
{
    ASSERT(!shouldUpdateScrollLayerPositionSynchronously());
    m_scrollLayer.get().position = CGPointMake(-position.x() + scrollOrigin().x(), -position.y() + scrollOrigin().y());

    ScrollBehaviorForFixedElements behaviorForFixed = scrollBehaviorForFixedElements();
    FloatPoint scrollOffset = position - toFloatSize(scrollOrigin());
    FloatRect viewportRect(FloatPoint(), scrollableAreaSize());
    
    FloatSize scrollOffsetForFixedChildren = FrameView::scrollOffsetForFixedPosition(enclosingLayoutRect(viewportRect),
        roundedLayoutSize(totalContentsSize()), roundedLayoutPoint(scrollOffset), scrollOrigin(), frameScaleFactor(), false, behaviorForFixed, headerHeight(), footerHeight());
    
    if (m_counterScrollingLayer)
        m_counterScrollingLayer.get().position = FloatPoint(scrollOffsetForFixedChildren);

    float topContentInset = this->topContentInset();
    if (m_insetClipLayer && m_scrolledContentsLayer && topContentInset) {
        m_insetClipLayer.get().position = FloatPoint(0, FrameView::yPositionForInsetClipLayer(position, topContentInset));
        m_scrolledContentsLayer.get().position = FloatPoint(m_scrolledContentsLayer.get().position.x,
            FrameView::yPositionForRootContentLayer(position, topContentInset, headerHeight()));
        if (m_contentShadowLayer)
            m_contentShadowLayer.get().position = m_scrolledContentsLayer.get().position;
    }

    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute scrollOffsetForFixedChildren for the banner with a scale factor of 1.
        float horizontalScrollOffsetForBanner = scrollOffsetForFixedChildren.width();
        if (frameScaleFactor() != 1)
            horizontalScrollOffsetForBanner = FrameView::scrollOffsetForFixedPosition(enclosingLayoutRect(viewportRect), roundedLayoutSize(totalContentsSize()), roundedLayoutPoint(scrollOffset), scrollOrigin(), 1, false, behaviorForFixed, headerHeight(), footerHeight()).width();

        if (m_headerLayer)
            m_headerLayer.get().position = FloatPoint(horizontalScrollOffsetForBanner, FrameView::yPositionForHeaderLayer(position, topContentInset));

        if (m_footerLayer) {
            m_footerLayer.get().position = FloatPoint(horizontalScrollOffsetForBanner,
                FrameView::yPositionForFooterLayer(position, topContentInset, totalContentsSize().height(), footerHeight()));
        }
    }

    if (m_verticalScrollbarPainter || m_horizontalScrollbarPainter) {
        [CATransaction begin];
        [CATransaction lock];

        if ([m_verticalScrollbarPainter shouldUsePresentationValue]) {
            float presentationValue;
            float overhangAmount;
            ScrollableArea::computeScrollbarValueAndOverhang(position.y(), totalContentsSize().height(), viewportRect.height(), presentationValue, overhangAmount);
            [m_verticalScrollbarPainter setPresentationValue:presentationValue];
        }

        if ([m_horizontalScrollbarPainter shouldUsePresentationValue]) {
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

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, viewportRect, FloatSize());
}

void ScrollingTreeFrameScrollingNodeMac::updateLayersAfterViewportChange(const FloatRect&, double)
{
    ASSERT_NOT_REACHED();
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::minimumScrollPosition() const
{
    FloatPoint position;
    
    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToBottom)
        position.setY(maximumScrollPosition().y());

    return position;
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::maximumScrollPosition() const
{
    FloatPoint position(totalContentsSizeForRubberBand() - scrollableAreaSize());
    position = position.expandedTo(FloatPoint());

    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToTop)
        position.setY(minimumScrollPosition().y());

    return position;
}

void ScrollingTreeFrameScrollingNodeMac::updateMainFramePinState(const FloatPoint& scrollPosition)
{
    bool pinnedToTheLeft = scrollPosition.x() <= minimumScrollPosition().x();
    bool pinnedToTheRight = scrollPosition.x() >= maximumScrollPosition().x();
    bool pinnedToTheTop = scrollPosition.y() <= minimumScrollPosition().y();
    bool pinnedToTheBottom = scrollPosition.y() >= maximumScrollPosition().y();

    scrollingTree().setMainFramePinState(pinnedToTheLeft, pinnedToTheRight, pinnedToTheTop, pinnedToTheBottom);
}

void ScrollingTreeFrameScrollingNodeMac::logExposedUnfilledArea()
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
    FloatRect viewPortRect(FloatPoint(), scrollableAreaSize());
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

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
