/*
 * Copyright (C) 2012, 2014-2015 Apple Inc. All rights reserved.
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
#import "LayoutSize.h"
#import "Logging.h"
#import "NSScrollerImpSPI.h"
#import "PlatformWheelEvent.h"
#import "ScrollableArea.h"
#import "ScrollingCoordinator.h"
#import "ScrollingTree.h"
#import "ScrollingStateTree.h"
#import "Settings.h"
#import "TextStream.h"
#import "TileController.h"
#import "WebLayer.h"

#import <QuartzCore/QuartzCore.h>
#import <wtf/CurrentTime.h>
#import <wtf/Deque.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/CString.h>

namespace WebCore {

static void logThreadedScrollingMode(unsigned synchronousScrollingReasons);

Ref<ScrollingTreeFrameScrollingNode> ScrollingTreeFrameScrollingNodeMac::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameScrollingNodeMac(scrollingTree, nodeID));
}

ScrollingTreeFrameScrollingNodeMac::ScrollingTreeFrameScrollingNodeMac(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNode(scrollingTree, nodeID)
    , m_scrollController(*this)
    , m_verticalScrollbarPainter(nullptr)
    , m_horizontalScrollbarPainter(nullptr)
{
}

ScrollingTreeFrameScrollingNodeMac::~ScrollingTreeFrameScrollingNodeMac()
{
    releaseReferencesToScrollbarPaintersOnTheMainThread();
}

void ScrollingTreeFrameScrollingNodeMac::releaseReferencesToScrollbarPaintersOnTheMainThread()
{
    if (m_verticalScrollbarPainter || m_horizontalScrollbarPainter) {
        // FIXME: This is a workaround in place for the time being since NSScrollerImps cannot be deallocated
        // on a non-main thread. rdar://problem/24535055
        ScrollbarPainter retainedVerticalScrollbarPainter = m_verticalScrollbarPainter.leakRef();
        ScrollbarPainter retainedHorizontalScrollbarPainter = m_horizontalScrollbarPainter.leakRef();
        WTF::callOnMainThread([retainedVerticalScrollbarPainter, retainedHorizontalScrollbarPainter] {
            [retainedVerticalScrollbarPainter release];
            [retainedHorizontalScrollbarPainter release];
        });
    }
}

#if ENABLE(CSS_SCROLL_SNAP)
static inline Vector<LayoutUnit> convertToLayoutUnits(const Vector<float>& snapOffsetsAsFloat)
{
    Vector<LayoutUnit> snapOffsets;
    snapOffsets.reserveInitialCapacity(snapOffsetsAsFloat.size());
    for (auto offset : snapOffsetsAsFloat)
        snapOffsets.append(offset);

    return snapOffsets;
}
#endif

void ScrollingTreeFrameScrollingNodeMac::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::updateBeforeChildren(stateNode);
    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

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
        releaseReferencesToScrollbarPaintersOnTheMainThread();
        m_verticalScrollbarPainter = scrollingStateNode.verticalScrollbarPainter();
        m_horizontalScrollbarPainter = scrollingStateNode.horizontalScrollbarPainter();
    }

    bool logScrollingMode = !m_hadFirstUpdate;
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

        logScrollingMode = true;
    }

    if (logScrollingMode) {
        if (scrollingTree().scrollingPerformanceLoggingEnabled())
            logThreadedScrollingMode(synchronousScrollingReasons());
    }

#if ENABLE(CSS_SCROLL_SNAP)
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalSnapOffsets))
        m_scrollController.updateScrollSnapPoints(ScrollEventAxis::Horizontal, convertToLayoutUnits(scrollingStateNode.horizontalSnapOffsets()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalSnapOffsets))
        m_scrollController.updateScrollSnapPoints(ScrollEventAxis::Vertical, convertToLayoutUnits(scrollingStateNode.verticalSnapOffsets()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::CurrentHorizontalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Horizontal, scrollingStateNode.currentHorizontalSnapPointIndex());
    
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::CurrentVerticalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Vertical, scrollingStateNode.currentVerticalSnapPointIndex());
#endif

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ExpectsWheelEventTestTrigger))
        m_expectsWheelEventTestTrigger = scrollingStateNode.expectsWheelEventTestTrigger();

    m_hadFirstUpdate = true;
}

void ScrollingTreeFrameScrollingNodeMac::updateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::updateAfterChildren(stateNode);

    const auto& scrollingStateNode = downcast<ScrollingStateScrollingNode>(stateNode);

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

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    if (m_expectsWheelEventTestTrigger) {
        if (scrollingTree().shouldHandleWheelEventSynchronously(wheelEvent))
            removeTestDeferralForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(scrollingNodeID()), WheelEventTestTrigger::ScrollingThreadSyncNeeded);
        else
            deferTestsForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(scrollingNodeID()), WheelEventTestTrigger::ScrollingThreadSyncNeeded);
    }
#endif

    m_scrollController.handleWheelEvent(wheelEvent);
#if ENABLE(CSS_SCROLL_SNAP)
    scrollingTree().setMainFrameIsScrollSnapping(m_scrollController.isScrollSnapInProgress());
    if (m_scrollController.activeScrollSnapIndexDidChange())
        scrollingTree().setActiveScrollSnapIndices(scrollingNodeID(), m_scrollController.activeScrollSnapIndexForAxis(ScrollEventAxis::Horizontal), m_scrollController.activeScrollSnapIndexForAxis(ScrollEventAxis::Vertical));
#endif
    scrollingTree().setOrClearLatchedNode(wheelEvent, scrollingNodeID());
    scrollingTree().handleWheelEventPhase(wheelEvent.phase());
}

// FIXME: We should find a way to share some of the code from newGestureIsStarting(), isAlreadyPinnedInDirectionOfGesture(),
// allowsVerticalStretching(), and allowsHorizontalStretching() with the implementation in ScrollAnimatorMac.
static bool newGestureIsStarting(const PlatformWheelEvent& wheelEvent)
{
    return wheelEvent.phase() == PlatformWheelEventPhaseMayBegin || wheelEvent.phase() == PlatformWheelEventPhaseBegan;
}

bool ScrollingTreeFrameScrollingNodeMac::isAlreadyPinnedInDirectionOfGesture(const PlatformWheelEvent& wheelEvent, ScrollEventAxis axis)
{
    switch (axis) {
    case ScrollEventAxis::Vertical:
        return (wheelEvent.deltaY() > 0 && scrollPosition().y() <= minimumScrollPosition().y()) || (wheelEvent.deltaY() < 0 && scrollPosition().y() >= maximumScrollPosition().y());
    case ScrollEventAxis::Horizontal:
        return (wheelEvent.deltaX() > 0 && scrollPosition().x() <= minimumScrollPosition().x()) || (wheelEvent.deltaX() < 0 && scrollPosition().x() >= maximumScrollPosition().x());
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollingTreeFrameScrollingNodeMac::allowsHorizontalStretching(const PlatformWheelEvent& wheelEvent)
{
    switch (horizontalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        bool scrollbarsAllowStretching = hasEnabledHorizontalScrollbar() || !hasEnabledVerticalScrollbar();
        bool eventPreventsStretching = newGestureIsStarting(wheelEvent) && isAlreadyPinnedInDirectionOfGesture(wheelEvent, ScrollEventAxis::Horizontal);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollingTreeFrameScrollingNodeMac::allowsVerticalStretching(const PlatformWheelEvent& wheelEvent)
{
    switch (verticalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        bool scrollbarsAllowStretching = hasEnabledVerticalScrollbar() || !hasEnabledHorizontalScrollbar();
        bool eventPreventsStretching = newGestureIsStarting(wheelEvent) && isAlreadyPinnedInDirectionOfGesture(wheelEvent, ScrollEventAxis::Vertical);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
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

void ScrollingTreeFrameScrollingNodeMac::immediateScrollBy(const FloatSize& delta)
{
    scrollBy(delta);
}

void ScrollingTreeFrameScrollingNodeMac::immediateScrollByWithoutContentEdgeConstraints(const FloatSize& offset)
{
    scrollByWithoutContentEdgeConstraints(offset);
}

void ScrollingTreeFrameScrollingNodeMac::stopSnapRubberbandTimer()
{
    scrollingTree().setMainFrameIsRubberBanding(false);

    // Since the rubberband timer has stopped, totalContentsSizeForRubberBand can be synchronized with totalContentsSize.
    setTotalContentsSizeForRubberBand(totalContentsSize());
}

void ScrollingTreeFrameScrollingNodeMac::adjustScrollPositionToBoundsIfNecessary()
{
    FloatPoint currentScrollPosition = scrollPosition();
    FloatPoint constainedPosition = currentScrollPosition.constrainedBetween(minimumScrollPosition(), maximumScrollPosition());
    immediateScrollBy(constainedPosition - currentScrollPosition);
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::scrollPosition() const
{
    if (shouldUpdateScrollLayerPositionSynchronously())
        return m_probableMainThreadScrollPosition;

    return -m_scrollLayer.get().position;
}

void ScrollingTreeFrameScrollingNodeMac::setScrollPosition(const FloatPoint& scrollPosition)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFrameScrollingNodeMac::setScrollPosition " << scrollPosition << " scrollPosition(): " << this->scrollPosition() << " min: " << minimumScrollPosition() << " max: " << maximumScrollPosition());

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

    m_scrollLayer.get().position = -position;

    ScrollBehaviorForFixedElements behaviorForFixed = scrollBehaviorForFixedElements();
    FloatRect viewportRect(position, scrollableAreaSize());
    
    FloatPoint scrollPositionForFixedChildren = FrameView::scrollPositionForFixedPosition(enclosingLayoutRect(viewportRect), LayoutSize(totalContentsSize()), LayoutPoint(position), scrollOrigin(), frameScaleFactor(),
        fixedElementsLayoutRelativeToFrame(), behaviorForFixed, headerHeight(), footerHeight());
    
    if (m_counterScrollingLayer)
        m_counterScrollingLayer.get().position = scrollPositionForFixedChildren;

    float topContentInset = this->topContentInset();
    if (m_insetClipLayer && m_scrolledContentsLayer && topContentInset) {
        m_insetClipLayer.get().position = FloatPoint(0, FrameView::yPositionForInsetClipLayer(position, topContentInset));
        m_scrolledContentsLayer.get().position = FrameView::positionForRootContentLayer(position, scrollOrigin(), topContentInset, headerHeight());
        if (m_contentShadowLayer)
            m_contentShadowLayer.get().position = m_scrolledContentsLayer.get().position;
    }

    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute scrollOffsetForFixedChildren for the banner with a scale factor of 1.
        float horizontalScrollOffsetForBanner = scrollPositionForFixedChildren.x();
        if (frameScaleFactor() != 1)
            horizontalScrollOffsetForBanner = FrameView::scrollPositionForFixedPosition(enclosingLayoutRect(viewportRect), LayoutSize(totalContentsSize()), LayoutPoint(position), scrollOrigin(), 1, fixedElementsLayoutRelativeToFrame(), behaviorForFixed, headerHeight(), footerHeight()).x();

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

    viewportRect.setLocation(scrollPositionForFixedChildren);

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, viewportRect, FloatSize());
}

void ScrollingTreeFrameScrollingNodeMac::updateLayersAfterViewportChange(const FloatRect&, double)
{
    ASSERT_NOT_REACHED();
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::minimumScrollPosition() const
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(FloatPoint(), toFloatSize(scrollOrigin()));
    
    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToBottom)
        position.setY(maximumScrollPosition().y());

    return position;
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::maximumScrollPosition() const
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(FloatPoint(totalContentsSizeForRubberBand() - scrollableAreaSize()), toFloatSize(scrollOrigin()));
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
            reasonsDescription.appendLiteral("forced,");
        if (synchronousScrollingReasons & ScrollingCoordinator::HasSlowRepaintObjects)
            reasonsDescription.appendLiteral("slow-repaint objects,");
        if (synchronousScrollingReasons & ScrollingCoordinator::HasViewportConstrainedObjectsWithoutSupportingFixedLayers)
            reasonsDescription.appendLiteral("viewport-constrained objects,");
        if (synchronousScrollingReasons & ScrollingCoordinator::HasNonLayerViewportConstrainedObjects)
            reasonsDescription.appendLiteral("non-layer viewport-constrained objects,");
        if (synchronousScrollingReasons & ScrollingCoordinator::IsImageDocument)
            reasonsDescription.appendLiteral("image document,");

        // Strip the trailing comma.
        String reasonsDescriptionTrimmed = reasonsDescription.toString().left(reasonsDescription.length() - 1);

        WTFLogAlways("SCROLLING: Switching to main-thread scrolling mode. Time: %f Reason(s): %s\n", WTF::monotonicallyIncreasingTime(), reasonsDescriptionTrimmed.ascii().data());
    } else
        WTFLogAlways("SCROLLING: Switching to threaded scrolling mode. Time: %f\n", WTF::monotonicallyIncreasingTime());
}

#if ENABLE(CSS_SCROLL_SNAP)
LayoutUnit ScrollingTreeFrameScrollingNodeMac::scrollOffsetOnAxis(ScrollEventAxis axis) const
{
    const FloatPoint& currentPosition = scrollPosition();
    return axis == ScrollEventAxis::Horizontal ? currentPosition.x() : currentPosition.y();
}

void ScrollingTreeFrameScrollingNodeMac::immediateScrollOnAxis(ScrollEventAxis axis, float delta)
{
    const FloatPoint& currentPosition = scrollPosition();
    FloatPoint change;
    if (axis == ScrollEventAxis::Horizontal)
        change = FloatPoint(currentPosition.x() + delta, currentPosition.y());
    else
        change = FloatPoint(currentPosition.x(), currentPosition.y() + delta);

    immediateScrollBy(change - currentPosition);
}

float ScrollingTreeFrameScrollingNodeMac::pageScaleFactor() const
{
    return frameScaleFactor();
}

void ScrollingTreeFrameScrollingNodeMac::startScrollSnapTimer()
{
    scrollingTree().setMainFrameIsScrollSnapping(true);
}

void ScrollingTreeFrameScrollingNodeMac::stopScrollSnapTimer()
{
    scrollingTree().setMainFrameIsScrollSnapping(false);
}
    
LayoutSize ScrollingTreeFrameScrollingNodeMac::scrollExtent() const
{
    return LayoutSize(totalContentsSize());
}
#endif

void ScrollingTreeFrameScrollingNodeMac::deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier identifier, WheelEventTestTrigger::DeferTestTriggerReason reason) const
{
    if (!m_expectsWheelEventTestTrigger)
        return;

    LOG(WheelEventTestTriggers, "  ScrollingTreeFrameScrollingNodeMac::deferTestsForReason: STARTING deferral for %p because of %d", identifier, reason);
    scrollingTree().deferTestsForReason(identifier, reason);
}
    
void ScrollingTreeFrameScrollingNodeMac::removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier identifier, WheelEventTestTrigger::DeferTestTriggerReason reason) const
{
    if (!m_expectsWheelEventTestTrigger)
        return;
    
    LOG(WheelEventTestTriggers, "   ScrollingTreeFrameScrollingNodeMac::deferTestsForReason: ENDING deferral for %p because of %d", identifier, reason);
    scrollingTree().removeTestDeferralForReason(identifier, reason);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
