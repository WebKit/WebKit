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
#include "ScrollingTreeScrollingNodeMac.h"

#if ENABLE(THREADED_SCROLLING)

#include "FrameView.h"
#include "PlatformWheelEvent.h"
#include "ScrollingCoordinator.h"
#include "ScrollingTree.h"
#include "ScrollingStateTree.h"
#include "Settings.h"
#include "TileCache.h"
#include "WebTileLayer.h"

#include <wtf/CurrentTime.h>
#include <wtf/Deque.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/CString.h>

namespace WebCore {

static void logThreadedScrollingMode(unsigned mainThreadScrollingReasons);
static void logWheelEventHandlerCountChanged(unsigned);


PassOwnPtr<ScrollingTreeScrollingNode> ScrollingTreeScrollingNode::create(ScrollingTree* scrollingTree, ScrollingNodeID nodeID)
{
    return adoptPtr(new ScrollingTreeScrollingNodeMac(scrollingTree, nodeID));
}

ScrollingTreeScrollingNodeMac::ScrollingTreeScrollingNodeMac(ScrollingTree* scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeScrollingNode(scrollingTree, nodeID)
    , m_scrollElasticityController(this)
    , m_lastScrollHadUnfilledPixels(false)
{
}

ScrollingTreeScrollingNodeMac::~ScrollingTreeScrollingNodeMac()
{
    if (m_snapRubberbandTimer)
        CFRunLoopTimerInvalidate(m_snapRubberbandTimer.get());
}

void ScrollingTreeScrollingNodeMac::updateBeforeChildren(ScrollingStateNode* stateNode)
{
    ScrollingTreeScrollingNode::updateBeforeChildren(stateNode);
    ScrollingStateScrollingNode* scrollingStateNode = toScrollingStateScrollingNode(stateNode);

    if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_scrollLayer = scrollingStateNode->platformScrollLayer();

    if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::CounterScrollingLayer))
        m_counterScrollingLayer = scrollingStateNode->counterScrollingPlatformLayer();

    if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::ShouldUpdateScrollLayerPositionOnMainThread)) {
        unsigned mainThreadScrollingReasons = this->shouldUpdateScrollLayerPositionOnMainThread();

        if (mainThreadScrollingReasons) {
            // We're transitioning to the slow "update scroll layer position on the main thread" mode.
            // Initialize the probable main thread scroll position with the current scroll layer position.
            if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
                m_probableMainThreadScrollPosition = scrollingStateNode->requestedScrollPosition();
            else {
                CGPoint scrollLayerPosition = m_scrollLayer.get().position;
                m_probableMainThreadScrollPosition = IntPoint(-scrollLayerPosition.x, -scrollLayerPosition.y);
            }
        }

        if (scrollingTree()->scrollingPerformanceLoggingEnabled())
            logThreadedScrollingMode(mainThreadScrollingReasons);
    }

    if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::WheelEventHandlerCount)) {
        if (scrollingTree()->scrollingPerformanceLoggingEnabled())
            logWheelEventHandlerCountChanged(scrollingStateNode->wheelEventHandlerCount());
    }
}

void ScrollingTreeScrollingNodeMac::updateAfterChildren(ScrollingStateNode* stateNode)
{
    ScrollingTreeScrollingNode::updateAfterChildren(stateNode);

    ScrollingStateScrollingNode* scrollingStateNode = toScrollingStateScrollingNode(stateNode);

    // Update the scroll position after child nodes have been updated, because they need to have updated their constraints before any scrolling happens.
    if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
        setScrollPosition(scrollingStateNode->requestedScrollPosition());

    if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::ScrollLayer) || scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::ContentsSize) || scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::ViewportRect))
        updateMainFramePinState(scrollPosition());
}

void ScrollingTreeScrollingNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHaveScrollbars())
        return;

    m_scrollElasticityController.handleWheelEvent(wheelEvent);
    scrollingTree()->handleWheelEventPhase(wheelEvent.phase());
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

    if (scrollingTree()->rootNode() == this) {
        if (stretch.isZero())
            scrollingTree()->setMainFrameIsRubberBanding(false);
        else
            scrollingTree()->setMainFrameIsRubberBanding(true);
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

bool ScrollingTreeScrollingNodeMac::canScrollHorizontally()
{
    return hasEnabledHorizontalScrollbar();
}

bool ScrollingTreeScrollingNodeMac::canScrollVertically()
{
    return hasEnabledVerticalScrollbar();
}

bool ScrollingTreeScrollingNodeMac::shouldRubberBandInDirection(ScrollDirection direction)
{
    if (direction == ScrollLeft)
        return !scrollingTree()->canGoBack();
    if (direction == ScrollRight)
        return !scrollingTree()->canGoForward();

    ASSERT_NOT_REACHED();
    return false;
}

IntPoint ScrollingTreeScrollingNodeMac::absoluteScrollPosition()
{
    return scrollPosition();
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

    scrollingTree()->setMainFrameIsRubberBanding(false);

    CFRunLoopTimerInvalidate(m_snapRubberbandTimer.get());
    m_snapRubberbandTimer = nullptr;
}

IntPoint ScrollingTreeScrollingNodeMac::scrollPosition() const
{
    if (shouldUpdateScrollLayerPositionOnMainThread())
        return m_probableMainThreadScrollPosition;

    CGPoint scrollLayerPosition = m_scrollLayer.get().position;
    return IntPoint(-scrollLayerPosition.x + scrollOrigin().x(), -scrollLayerPosition.y + scrollOrigin().y());
}

void ScrollingTreeScrollingNodeMac::setScrollPosition(const IntPoint& scrollPosition)
{
    IntPoint newScrollPosition = scrollPosition;
    newScrollPosition = newScrollPosition.shrunkTo(maximumScrollPosition());
    newScrollPosition = newScrollPosition.expandedTo(minimumScrollPosition());

    setScrollPositionWithoutContentEdgeConstraints(newScrollPosition);

    if (scrollingTree()->scrollingPerformanceLoggingEnabled())
        logExposedUnfilledArea();
}

void ScrollingTreeScrollingNodeMac::setScrollPositionWithoutContentEdgeConstraints(const IntPoint& scrollPosition)
{
    updateMainFramePinState(scrollPosition);

    if (shouldUpdateScrollLayerPositionOnMainThread()) {
        m_probableMainThreadScrollPosition = scrollPosition;
        scrollingTree()->updateMainFrameScrollPosition(scrollPosition, SetScrollingLayerPosition);
        return;
    }

    setScrollLayerPosition(scrollPosition);
    scrollingTree()->updateMainFrameScrollPosition(scrollPosition);
}

void ScrollingTreeScrollingNodeMac::setScrollLayerPosition(const IntPoint& position)
{
    ASSERT(!shouldUpdateScrollLayerPositionOnMainThread());
    m_scrollLayer.get().position = CGPointMake(-position.x() + scrollOrigin().x(), -position.y() + scrollOrigin().y());

    IntSize scrollOffsetForFixedChildren = FrameView::scrollOffsetForFixedPosition(viewportRect(), contentsSize(), position, scrollOrigin(), frameScaleFactor(), false);
    if (m_counterScrollingLayer)
        m_counterScrollingLayer.get().position = FloatPoint(scrollOffsetForFixedChildren);

    if (!m_children)
        return;

    IntRect viewportRect = this->viewportRect();
    viewportRect.setLocation(IntPoint(scrollOffsetForFixedChildren));

    size_t size = m_children->size();
    for (size_t i = 0; i < size; ++i)
        m_children->at(i)->parentScrollPositionDidChange(viewportRect, FloatSize());
}

IntPoint ScrollingTreeScrollingNodeMac::minimumScrollPosition() const
{
    return IntPoint(0, 0);
}

IntPoint ScrollingTreeScrollingNodeMac::maximumScrollPosition() const
{
    IntPoint position(contentsSize().width() - viewportRect().width(),
                      contentsSize().height() - viewportRect().height());

    position.clampNegativeToZero();

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

void ScrollingTreeScrollingNodeMac::updateMainFramePinState(const IntPoint& scrollPosition)
{
    bool pinnedToTheLeft = scrollPosition.x() <= minimumScrollPosition().x();
    bool pinnedToTheRight = scrollPosition.x() >= maximumScrollPosition().x();

    scrollingTree()->setMainFramePinState(pinnedToTheLeft, pinnedToTheRight);
}

void ScrollingTreeScrollingNodeMac::logExposedUnfilledArea()
{
    Region paintedVisibleTiles;

    Deque<CALayer*> layerQueue;
    layerQueue.append(m_scrollLayer.get());
    WebTileLayerList tiles;

    while (!layerQueue.isEmpty() && tiles.isEmpty()) {
        CALayer* layer = layerQueue.takeFirst();
        NSArray* sublayers = [[layer sublayers] copy];

        // If this layer is the parent of a tile, it is the parent of all of the tiles and nothing else.
        if ([[sublayers objectAtIndex:0] isKindOfClass:[WebTileLayer class]]) {
            for (CALayer* sublayer in sublayers) {
                ASSERT([sublayer isKindOfClass:[WebTileLayer class]]);
                tiles.append(static_cast<WebTileLayer*>(sublayer));
            }
        } else {
            for (CALayer* sublayer in sublayers)
                layerQueue.append(sublayer);
        }

        [sublayers release];
    }

    IntPoint scrollPosition = this->scrollPosition();
    unsigned unfilledArea = TileCache::blankPixelCountForTiles(tiles, viewportRect(), IntPoint(-scrollPosition.x(), -scrollPosition.y()));

    if (unfilledArea || m_lastScrollHadUnfilledPixels)
        WTFLogAlways("SCROLLING: Exposed tileless area. Time: %f Unfilled Pixels: %u\n", WTF::monotonicallyIncreasingTime(), unfilledArea);

    m_lastScrollHadUnfilledPixels = unfilledArea;
}

static void logThreadedScrollingMode(unsigned mainThreadScrollingReasons)
{
    if (mainThreadScrollingReasons) {
        StringBuilder reasonsDescription;

        if (mainThreadScrollingReasons & ScrollingCoordinator::ForcedOnMainThread)
            reasonsDescription.append("forced,");
        if (mainThreadScrollingReasons & ScrollingCoordinator::HasSlowRepaintObjects)
            reasonsDescription.append("slow-repaint objects,");
        if (mainThreadScrollingReasons & ScrollingCoordinator::HasViewportConstrainedObjectsWithoutSupportingFixedLayers)
            reasonsDescription.append("viewport-constrained objects,");
        if (mainThreadScrollingReasons & ScrollingCoordinator::HasNonLayerViewportConstrainedObjects)
            reasonsDescription.append("non-layer viewport-constrained objects,");
        if (mainThreadScrollingReasons & ScrollingCoordinator::IsImageDocument)
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

#endif // ENABLE(THREADED_SCROLLING)
