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
#import <QuartzCore/QuartzCore.h>
#import <wtf/Deque.h>
#import <wtf/text/CString.h>
#import <wtf/text/TextStream.h>

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

ScrollingTreeFrameScrollingNodeMac::~ScrollingTreeFrameScrollingNodeMac() = default;

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

    bool logScrollingMode = !m_hadFirstUpdate;
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ReasonsForSynchronousScrolling))
        logScrollingMode = true;

    if (logScrollingMode && isRootNode() && scrollingTree().scrollingPerformanceLoggingEnabled())
        scrollingTree().reportSynchronousScrollingReasonsChanged(MonotonicTime::now(), synchronousScrollingReasons());

    m_delegate.updateFromStateNode(scrollingStateNode);

#if ENABLE(CSS_SCROLL_SNAP)
    // FIXME: this should move to the delegate and be shared with overflow.
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
        scrollTo(scrollingStateNode.requestedScrollPosition());

    if (isRootNode()
        && (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaSize)))
        updateMainFramePinState();
}

ScrollingEventResult ScrollingTreeFrameScrollingNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHaveScrollbars())
        return ScrollingEventResult::DidNotHandleEvent;

    m_delegate.handleWheelEvent(wheelEvent);

#if ENABLE(CSS_SCROLL_SNAP)
    if (isRootNode())
        scrollingTree().setMainFrameIsScrollSnapping(m_delegate.isScrollSnapInProgress());

    if (m_delegate.activeScrollSnapIndexDidChange())
        scrollingTree().setActiveScrollSnapIndices(scrollingNodeID(), m_delegate.activeScrollSnapIndexForAxis(ScrollEventAxis::Horizontal), m_delegate.activeScrollSnapIndexForAxis(ScrollEventAxis::Vertical));
#endif
    scrollingTree().setOrClearLatchedNode(wheelEvent, scrollingNodeID());
    scrollingTree().handleWheelEventPhase(wheelEvent.phase());
    
    // FIXME: This needs to return whether the event was handled.
    return ScrollingEventResult::DidHandleEvent;
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::adjustedScrollPosition(const FloatPoint& position, ScrollPositionClamp clamp) const
{
    FloatPoint scrollPosition(roundf(position.x()), roundf(position.y()));
    return ScrollingTreeFrameScrollingNode::adjustedScrollPosition(scrollPosition, clamp);
}

void ScrollingTreeFrameScrollingNodeMac::currentScrollPositionChanged()
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFrameScrollingNodeMac::currentScrollPositionChanged to " << currentScrollPosition() << " min: " << minimumScrollPosition() << " max: " << maximumScrollPosition() << " sync: " << shouldUpdateScrollLayerPositionSynchronously());

    if (isRootNode())
        updateMainFramePinState();

    if (shouldUpdateScrollLayerPositionSynchronously())
        scrollingTree().scrollingTreeNodeDidScroll(*this, ScrollingLayerPositionAction::Set);
    else
        ScrollingTreeFrameScrollingNode::currentScrollPositionChanged();

    if (scrollingTree().scrollingPerformanceLoggingEnabled()) {
        unsigned unfilledArea = exposedUnfilledArea();
        if (unfilledArea || m_lastScrollHadUnfilledPixels)
            scrollingTree().reportExposedUnfilledArea(MonotonicTime::now(), unfilledArea);

        m_lastScrollHadUnfilledPixels = unfilledArea;
    }
}

void ScrollingTreeFrameScrollingNodeMac::repositionScrollingLayers()
{
    scrolledContentsLayer().position = -currentScrollPosition();
}

void ScrollingTreeFrameScrollingNodeMac::repositionRelatedLayers()
{
    auto scrollPosition = currentScrollPosition();
    auto layoutViewport = this->layoutViewport();

    FloatRect visibleContentRect(scrollPosition, scrollableAreaSize());

    if (m_counterScrollingLayer)
        m_counterScrollingLayer.get().position = layoutViewport.location();

    float topContentInset = this->topContentInset();
    if (m_insetClipLayer && m_rootContentsLayer && topContentInset) {
        m_insetClipLayer.get().position = FloatPoint(m_insetClipLayer.get().position.x, FrameView::yPositionForInsetClipLayer(scrollPosition, topContentInset));
        m_rootContentsLayer.get().position = FrameView::positionForRootContentLayer(scrollPosition, scrollOrigin(), topContentInset, headerHeight());
        if (m_contentShadowLayer)
            m_contentShadowLayer.get().position = m_rootContentsLayer.get().position;
    }

    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute layoutViewport.x() for the banner with a scale factor of 1.
        float horizontalScrollOffsetForBanner = layoutViewport.x();
        if (m_headerLayer)
            m_headerLayer.get().position = FloatPoint(horizontalScrollOffsetForBanner, FrameView::yPositionForHeaderLayer(scrollPosition, topContentInset));

        if (m_footerLayer)
            m_footerLayer.get().position = FloatPoint(horizontalScrollOffsetForBanner, FrameView::yPositionForFooterLayer(scrollPosition, topContentInset, totalContentsSize().height(), footerHeight()));
    }

    m_delegate.updateScrollbarPainters();
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

void ScrollingTreeFrameScrollingNodeMac::updateMainFramePinState()
{
    ASSERT(isRootNode());

    auto scrollPosition = currentScrollPosition();
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

    FloatPoint scrollPosition = currentScrollPosition();
    FloatRect viewPortRect({ }, scrollableAreaSize());
    return TileController::blankPixelCountForTiles(tiles, viewPortRect, IntPoint(-scrollPosition.x(), -scrollPosition.y()));
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
