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
#import "PlatformWheelEvent.h"
#import "ScrollableArea.h"
#import "ScrollingCoordinator.h"
#import "ScrollingStateTree.h"
#import "ScrollingTree.h"
#import "TileController.h"
#import "WebLayer.h"
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/text/TextStream.h>

#import <QuartzCore/QuartzCore.h>
#import <wtf/Deque.h>
#import <wtf/text/CString.h>

namespace WebCore {

Ref<ScrollingTreeFrameScrollingNode> ScrollingTreeFrameScrollingNodeMac::create(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameScrollingNodeMac(scrollingTree, nodeType, nodeID));
}

ScrollingTreeFrameScrollingNodeMac::ScrollingTreeFrameScrollingNodeMac(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNode(scrollingTree, nodeType, nodeID)
    , m_scrollController(*this)
    , m_verticalScrollerImp(nullptr)
    , m_horizontalScrollerImp(nullptr)
{
}

ScrollingTreeFrameScrollingNodeMac::~ScrollingTreeFrameScrollingNodeMac()
{
    releaseReferencesToScrollerImpsOnTheMainThread();
}

void ScrollingTreeFrameScrollingNodeMac::releaseReferencesToScrollerImpsOnTheMainThread()
{
    if (m_verticalScrollerImp || m_horizontalScrollerImp) {
        // FIXME: This is a workaround in place for the time being since NSScrollerImps cannot be deallocated
        // on a non-main thread. rdar://problem/24535055
        WTF::callOnMainThread([verticalScrollerImp = WTFMove(m_verticalScrollerImp), horizontalScrollerImp = WTFMove(m_horizontalScrollerImp)] {
        });
    }
}

#if ENABLE(CSS_SCROLL_SNAP)
static inline Vector<LayoutUnit> convertToLayoutUnits(const Vector<float>& snapOffsetsAsFloat)
{
    Vector<LayoutUnit> snapOffsets;
    snapOffsets.reserveInitialCapacity(snapOffsetsAsFloat.size());
    for (auto offset : snapOffsetsAsFloat)
        snapOffsets.uncheckedAppend(offset);

    return snapOffsets;
}

static inline Vector<ScrollOffsetRange<LayoutUnit>> convertToLayoutUnits(const Vector<ScrollOffsetRange<float>>& snapOffsetRangesAsFloat)
{
    Vector<ScrollOffsetRange<LayoutUnit>> snapOffsetRanges;
    snapOffsetRanges.reserveInitialCapacity(snapOffsetRangesAsFloat.size());
    for (auto range : snapOffsetRangesAsFloat)
        snapOffsetRanges.uncheckedAppend({ range.start, range.end });

    return snapOffsetRanges;
}
#endif

void ScrollingTreeFrameScrollingNodeMac::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateBeforeChildren(stateNode);
    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_scrollLayer = scrollingStateNode.layer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
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
        releaseReferencesToScrollerImpsOnTheMainThread();
        m_verticalScrollerImp = scrollingStateNode.verticalScrollerImp();
        m_horizontalScrollerImp = scrollingStateNode.horizontalScrollerImp();
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

    if (logScrollingMode && scrollingTree().scrollingPerformanceLoggingEnabled())
        scrollingTree().reportSynchronousScrollingReasonsChanged(MonotonicTime::now(), synchronousScrollingReasons());

#if ENABLE(CSS_SCROLL_SNAP)
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalSnapOffsets) || scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalSnapOffsetRanges))
        m_scrollController.updateScrollSnapPoints(ScrollEventAxis::Horizontal, convertToLayoutUnits(scrollingStateNode.horizontalSnapOffsets()), convertToLayoutUnits(scrollingStateNode.horizontalSnapOffsetRanges()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalSnapOffsets) || scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalSnapOffsetRanges))
        m_scrollController.updateScrollSnapPoints(ScrollEventAxis::Vertical, convertToLayoutUnits(scrollingStateNode.verticalSnapOffsets()), convertToLayoutUnits(scrollingStateNode.verticalSnapOffsetRanges()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::CurrentHorizontalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Horizontal, scrollingStateNode.currentHorizontalSnapPointIndex());
    
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::CurrentVerticalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Vertical, scrollingStateNode.currentVerticalSnapPointIndex());
#endif

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ExpectsWheelEventTestTrigger))
        m_expectsWheelEventTestTrigger = scrollingStateNode.expectsWheelEventTestTrigger();

    m_hadFirstUpdate = true;
}

void ScrollingTreeFrameScrollingNodeMac::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateAfterChildren(stateNode);

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
        [m_verticalScrollerImp setUsePresentationValue:YES];
        [m_horizontalScrollerImp setUsePresentationValue:YES];
    }
    if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseEnded || wheelEvent.momentumPhase() == PlatformWheelEventPhaseCancelled) {
        [m_verticalScrollerImp setUsePresentationValue:NO];
        [m_horizontalScrollerImp setUsePresentationValue:NO];
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

    if (scrollingTree().scrollingPerformanceLoggingEnabled()) {
        unsigned unfilledArea = exposedUnfilledArea();
        if (unfilledArea || m_lastScrollHadUnfilledPixels)
            scrollingTree().reportExposedUnfilledArea(MonotonicTime::now(), unfilledArea);

        m_lastScrollHadUnfilledPixels = unfilledArea;
    }
}

void ScrollingTreeFrameScrollingNodeMac::setScrollPositionWithoutContentEdgeConstraints(const FloatPoint& scrollPosition)
{
    updateMainFramePinState(scrollPosition);

    Optional<FloatPoint> layoutViewportOrigin;
    if (scrollingTree().visualViewportEnabled()) {
        FloatPoint visibleContentOrigin = scrollPosition;
        FloatRect newLayoutViewport = layoutViewportForScrollPosition(visibleContentOrigin, frameScaleFactor());
        setLayoutViewport(newLayoutViewport);
        layoutViewportOrigin = newLayoutViewport.location();
    }

    if (shouldUpdateScrollLayerPositionSynchronously()) {
        m_probableMainThreadScrollPosition = scrollPosition;
        scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition, layoutViewportOrigin, ScrollingLayerPositionAction::Set);
        return;
    }

    setScrollLayerPosition(scrollPosition, layoutViewport());
    scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition, layoutViewportOrigin);
}

void ScrollingTreeFrameScrollingNodeMac::setScrollLayerPosition(const FloatPoint& position, const FloatRect& layoutViewport)
{
    ASSERT(!shouldUpdateScrollLayerPositionSynchronously());

    m_scrollLayer.get().position = -position;

    FloatRect visibleContentRect(position, scrollableAreaSize());
    FloatRect fixedPositionRect;
    ScrollBehaviorForFixedElements behaviorForFixed = StickToViewportBounds;

    if (scrollingTree().visualViewportEnabled())
        fixedPositionRect = layoutViewport;
    else {
        behaviorForFixed = scrollBehaviorForFixedElements();
        
        FloatPoint scrollPositionForFixedChildren = FrameView::scrollPositionForFixedPosition(enclosingLayoutRect(visibleContentRect), LayoutSize(totalContentsSize()),
            LayoutPoint(position), scrollOrigin(), frameScaleFactor(), fixedElementsLayoutRelativeToFrame(), behaviorForFixed, headerHeight(), footerHeight());

        fixedPositionRect = { scrollPositionForFixedChildren, visibleContentRect.size() };
    }
    
    if (m_counterScrollingLayer)
        m_counterScrollingLayer.get().position = fixedPositionRect.location();

    float topContentInset = this->topContentInset();
    if (m_insetClipLayer && m_scrolledContentsLayer && topContentInset) {
        m_insetClipLayer.get().position = FloatPoint(m_insetClipLayer.get().position.x, FrameView::yPositionForInsetClipLayer(position, topContentInset));
        m_scrolledContentsLayer.get().position = FrameView::positionForRootContentLayer(position, scrollOrigin(), topContentInset, headerHeight());
        if (m_contentShadowLayer)
            m_contentShadowLayer.get().position = m_scrolledContentsLayer.get().position;
    }

    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute fixedPositionRect.x() for the banner with a scale factor of 1.
        float horizontalScrollOffsetForBanner = fixedPositionRect.x();
        if (!scrollingTree().visualViewportEnabled() && frameScaleFactor() != 1) {
            horizontalScrollOffsetForBanner = FrameView::scrollPositionForFixedPosition(enclosingLayoutRect(visibleContentRect), LayoutSize(totalContentsSize()),
                LayoutPoint(position), scrollOrigin(), 1, fixedElementsLayoutRelativeToFrame(), behaviorForFixed, headerHeight(), footerHeight()).x();
        }

        if (m_headerLayer)
            m_headerLayer.get().position = FloatPoint(horizontalScrollOffsetForBanner, FrameView::yPositionForHeaderLayer(position, topContentInset));

        if (m_footerLayer)
            m_footerLayer.get().position = FloatPoint(horizontalScrollOffsetForBanner, FrameView::yPositionForFooterLayer(position, topContentInset, totalContentsSize().height(), footerHeight()));
    }

    if (m_verticalScrollerImp || m_horizontalScrollerImp) {
        [CATransaction begin];
        [CATransaction lock];

        if ([m_verticalScrollerImp shouldUsePresentationValue]) {
            float presentationValue;
            float overhangAmount;
            ScrollableArea::computeScrollbarValueAndOverhang(position.y(), totalContentsSize().height(), visibleContentRect.height(), presentationValue, overhangAmount);
            [m_verticalScrollerImp setPresentationValue:presentationValue];
        }

        if ([m_horizontalScrollerImp shouldUsePresentationValue]) {
            float presentationValue;
            float overhangAmount;
            ScrollableArea::computeScrollbarValueAndOverhang(position.x(), totalContentsSize().width(), visibleContentRect.width(), presentationValue, overhangAmount);
            [m_horizontalScrollerImp setPresentationValue:presentationValue];
        }

        [CATransaction unlock];
        [CATransaction commit];
    }

    if (!m_children)
        return;

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, fixedPositionRect, FloatSize());
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

unsigned ScrollingTreeFrameScrollingNodeMac::exposedUnfilledArea() const
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
    return TileController::blankPixelCountForTiles(tiles, viewPortRect, IntPoint(-scrollPosition.x(), -scrollPosition.y()));
}

#if ENABLE(CSS_SCROLL_SNAP)
FloatPoint ScrollingTreeFrameScrollingNodeMac::scrollOffset() const
{
    return scrollPosition();
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

FloatSize ScrollingTreeFrameScrollingNodeMac::viewportSize() const
{
    return scrollableAreaSize();
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
