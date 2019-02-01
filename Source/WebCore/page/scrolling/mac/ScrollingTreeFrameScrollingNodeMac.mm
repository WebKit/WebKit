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
    , m_delegate(*this)
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

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::RootContentsLayer))
        m_rootContentsLayer = scrollingStateNode.rootContentsLayer();

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
                CGPoint scrollLayerPosition = scrolledContentsLayer().position;
                m_probableMainThreadScrollPosition = FloatPoint(-scrollLayerPosition.x, -scrollLayerPosition.y);
            }
        }

        logScrollingMode = true;
    }

    if (logScrollingMode && scrollingTree().scrollingPerformanceLoggingEnabled())
        scrollingTree().reportSynchronousScrollingReasonsChanged(MonotonicTime::now(), synchronousScrollingReasons());

#if ENABLE(CSS_SCROLL_SNAP)
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalSnapOffsets) || scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalSnapOffsetRanges))
        m_delegate.updateScrollSnapPoints(ScrollEventAxis::Horizontal, convertToLayoutUnits(scrollingStateNode.horizontalSnapOffsets()), convertToLayoutUnits(scrollingStateNode.horizontalSnapOffsetRanges()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalSnapOffsets) || scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalSnapOffsetRanges))
        m_delegate.updateScrollSnapPoints(ScrollEventAxis::Vertical, convertToLayoutUnits(scrollingStateNode.verticalSnapOffsets()), convertToLayoutUnits(scrollingStateNode.verticalSnapOffsetRanges()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::CurrentHorizontalSnapOffsetIndex))
        m_delegate.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Horizontal, scrollingStateNode.currentHorizontalSnapPointIndex());
    
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::CurrentVerticalSnapOffsetIndex))
        m_delegate.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Vertical, scrollingStateNode.currentVerticalSnapPointIndex());
#endif

    m_hadFirstUpdate = true;
}

void ScrollingTreeFrameScrollingNodeMac::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateAfterChildren(stateNode);

    const auto& scrollingStateNode = downcast<ScrollingStateScrollingNode>(stateNode);

    // Update the scroll position after child nodes have been updated, because they need to have updated their constraints before any scrolling happens.
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
        setScrollPosition(scrollingStateNode.requestedScrollPosition());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaSize))
        updateMainFramePinState(scrollPosition());
}

ScrollingEventResult ScrollingTreeFrameScrollingNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHaveScrollbars())
        return ScrollingEventResult::DidNotHandleEvent;

    if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseBegan) {
        [m_verticalScrollerImp setUsePresentationValue:YES];
        [m_horizontalScrollerImp setUsePresentationValue:YES];
    }
    if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseEnded || wheelEvent.momentumPhase() == PlatformWheelEventPhaseCancelled) {
        [m_verticalScrollerImp setUsePresentationValue:NO];
        [m_horizontalScrollerImp setUsePresentationValue:NO];
    }

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    if (expectsWheelEventTestTrigger()) {
        if (scrollingTree().shouldHandleWheelEventSynchronously(wheelEvent))
            m_delegate.removeTestDeferralForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(scrollingNodeID()), WheelEventTestTrigger::ScrollingThreadSyncNeeded);
        else
            m_delegate.deferTestsForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(scrollingNodeID()), WheelEventTestTrigger::ScrollingThreadSyncNeeded);
    }
#endif

    m_delegate.handleWheelEvent(wheelEvent);

#if ENABLE(CSS_SCROLL_SNAP)
    scrollingTree().setMainFrameIsScrollSnapping(m_delegate.isScrollSnapInProgress());
    if (m_delegate.activeScrollSnapIndexDidChange())
        scrollingTree().setActiveScrollSnapIndices(scrollingNodeID(), m_delegate.activeScrollSnapIndexForAxis(ScrollEventAxis::Horizontal), m_delegate.activeScrollSnapIndexForAxis(ScrollEventAxis::Vertical));
#endif
    scrollingTree().setOrClearLatchedNode(wheelEvent, scrollingNodeID());
    scrollingTree().handleWheelEventPhase(wheelEvent.phase());
    
    // FIXME: This needs to return whether the event was handled.
    return ScrollingEventResult::DidHandleEvent;
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::scrollPosition() const
{
    if (shouldUpdateScrollLayerPositionSynchronously())
        return m_probableMainThreadScrollPosition;

    return -scrolledContentsLayer().position;
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

    scrolledContentsLayer().position = -position;

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
    if (m_insetClipLayer && m_rootContentsLayer && topContentInset) {
        m_insetClipLayer.get().position = FloatPoint(m_insetClipLayer.get().position.x, FrameView::yPositionForInsetClipLayer(position, topContentInset));
        m_rootContentsLayer.get().position = FrameView::positionForRootContentLayer(position, scrollOrigin(), topContentInset, headerHeight());
        if (m_contentShadowLayer)
            m_contentShadowLayer.get().position = m_rootContentsLayer.get().position;
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
    
    if (isRootNode() && scrollingTree().scrollPinningBehavior() == PinToBottom)
        position.setY(maximumScrollPosition().y());

    return position;
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::maximumScrollPosition() const
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(FloatPoint(totalContentsSizeForRubberBand() - scrollableAreaSize()), toFloatSize(scrollOrigin()));
    position = position.expandedTo(FloatPoint());

    if (isRootNode() && scrollingTree().scrollPinningBehavior() == PinToTop)
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
    layerQueue.append(scrolledContentsLayer());
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

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
