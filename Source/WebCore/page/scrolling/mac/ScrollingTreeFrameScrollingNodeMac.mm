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
#import "ScrollingThread.h"
#import "ScrollingTree.h"
#import "TileController.h"
#import "WebCoreCALayerExtras.h"
#import <wtf/BlockObjCExceptions.h>
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

void ScrollingTreeFrameScrollingNodeMac::willBeDestroyed()
{
    m_delegate.nodeWillBeDestroyed();
}

void ScrollingTreeFrameScrollingNodeMac::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateBeforeChildren(stateNode);
    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::RootContentsLayer))
        m_rootContentsLayer = static_cast<CALayer*>(scrollingStateNode.rootContentsLayer());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CounterScrollingLayer))
        m_counterScrollingLayer = static_cast<CALayer*>(scrollingStateNode.counterScrollingLayer());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::InsetClipLayer))
        m_insetClipLayer = static_cast<CALayer*>(scrollingStateNode.insetClipLayer());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ContentShadowLayer))
        m_contentShadowLayer = static_cast<CALayer*>(scrollingStateNode.contentShadowLayer());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HeaderLayer))
        m_headerLayer = static_cast<CALayer*>(scrollingStateNode.headerLayer());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::FooterLayer))
        m_footerLayer = static_cast<CALayer*>(scrollingStateNode.footerLayer());

    bool logScrollingMode = !m_hadFirstUpdate;
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ReasonsForSynchronousScrolling))
        logScrollingMode = true;

    if (logScrollingMode && isRootNode() && scrollingTree().scrollingPerformanceTestingEnabled())
        scrollingTree().reportSynchronousScrollingReasonsChanged(MonotonicTime::now(), synchronousScrollingReasons());

    m_delegate.updateFromStateNode(scrollingStateNode);

    m_hadFirstUpdate = true;
}

void ScrollingTreeFrameScrollingNodeMac::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateAfterChildren(stateNode);

    const auto& scrollingStateNode = downcast<ScrollingStateScrollingNode>(stateNode);
    if (isRootNode()
        && (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer)
        || scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::TotalContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollableAreaSize)))
        updateMainFramePinAndRubberbandState();
}

WheelEventHandlingResult ScrollingTreeFrameScrollingNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent, EventTargeting eventTargeting)
{
    if (!canHandleWheelEvent(wheelEvent, eventTargeting))
        return WheelEventHandlingResult::unhandled();

    bool handled = m_delegate.handleWheelEvent(wheelEvent);

    setScrollSnapInProgress(m_delegate.isScrollSnapInProgress());

    if (m_delegate.activeScrollSnapIndexDidChange())
        scrollingTree().setActiveScrollSnapIndices(scrollingNodeID(), m_delegate.activeScrollSnapIndexForAxis(ScrollEventAxis::Horizontal), m_delegate.activeScrollSnapIndexForAxis(ScrollEventAxis::Vertical));

    return WheelEventHandlingResult::result(handled);
}

bool ScrollingTreeFrameScrollingNodeMac::startAnimatedScrollToPosition(FloatPoint destinationPosition)
{
    return m_delegate.startAnimatedScrollToPosition(destinationPosition);
}

void ScrollingTreeFrameScrollingNodeMac::stopAnimatedScroll()
{
    m_delegate.stopAnimatedScroll();
}

void ScrollingTreeFrameScrollingNodeMac::serviceScrollAnimation(MonotonicTime currentTime)
{
    m_delegate.serviceScrollAnimation(currentTime);
}

void ScrollingTreeFrameScrollingNodeMac::willDoProgrammaticScroll(const FloatPoint& targetScrollPosition)
{
    m_delegate.willDoProgrammaticScroll(targetScrollPosition);
}

FloatPoint ScrollingTreeFrameScrollingNodeMac::adjustedScrollPosition(const FloatPoint& position, ScrollClamping clamp) const
{
    FloatPoint scrollPosition(roundf(position.x()), roundf(position.y()));
    return ScrollingTreeFrameScrollingNode::adjustedScrollPosition(scrollPosition, clamp);
}

void ScrollingTreeFrameScrollingNodeMac::currentScrollPositionChanged(ScrollType scrollType, ScrollingLayerPositionAction action)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFrameScrollingNodeMac " << scrollingNodeID() << " currentScrollPositionChanged to " << currentScrollPosition() << " min: " << minimumScrollPosition() << " max: " << maximumScrollPosition() << " sync: " << hasSynchronousScrollingReasons() << " is animating: " << scrollingTree().isScrollAnimationInProgressForNode(scrollingNodeID()));

    m_delegate.currentScrollPositionChanged();

    if (isRootNode())
        updateMainFramePinAndRubberbandState();

    ScrollingTreeFrameScrollingNode::currentScrollPositionChanged(scrollType, hasSynchronousScrollingReasons() ? ScrollingLayerPositionAction::Set : action);

    if (scrollingTree().scrollingPerformanceTestingEnabled()) {
        unsigned unfilledArea = exposedUnfilledArea();
        if (unfilledArea || m_lastScrollHadUnfilledPixels)
            scrollingTree().reportExposedUnfilledArea(MonotonicTime::now(), unfilledArea);

        m_lastScrollHadUnfilledPixels = unfilledArea;
    }
}

void ScrollingTreeFrameScrollingNodeMac::repositionScrollingLayers()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    auto* layer = static_cast<CALayer*>(scrolledContentsLayer());
    if (ScrollingThread::isCurrentThread()) {
        // If we're committing on the scrolling thread, it means that ThreadedScrollingTree is in "desynchronized" mode.
        // The main thread may already have set the same layer position, but here we need to trigger a scrolling thread commit to
        // ensure that the scroll happens even when the main thread commit is taking a long time. So make sure the layer property changes
        // when there has been a scroll position change.
        if (!scrollingTree().isScrollingSynchronizedWithMainThread())
            layer.position = CGPointZero;
    }

    // We use scroll position here because the root content layer is offset to account for scrollOrigin (see FrameView::positionForRootContentLayer).
    layer.position = -currentScrollPosition();
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeFrameScrollingNodeMac::repositionRelatedLayers()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto scrollPosition = currentScrollPosition();
    auto layoutViewport = this->layoutViewport();

    FloatRect visibleContentRect(scrollPosition, scrollableAreaSize());

    if (m_counterScrollingLayer)
        m_counterScrollingLayer.get().position = layoutViewport.location();

    float topContentInset = this->topContentInset();
    if (m_insetClipLayer && m_rootContentsLayer) {
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
    END_BLOCK_OBJC_EXCEPTIONS

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

void ScrollingTreeFrameScrollingNodeMac::updateMainFramePinAndRubberbandState()
{
    ASSERT(isRootNode());
    scrollingTree().setMainFramePinnedState(edgePinnedState());
}

unsigned ScrollingTreeFrameScrollingNodeMac::exposedUnfilledArea() const
{
    Region paintedVisibleTiles;

    Deque<CALayer*> layerQueue;
    layerQueue.append(scrolledContentsLayer());
    PlatformLayerList tiles;

    while (!layerQueue.isEmpty() && tiles.isEmpty()) {
        CALayer* layer = layerQueue.takeFirst();
        auto sublayers = adoptNS([[layer sublayers] copy]);

        // If this layer is the parent of a tile, it is the parent of all of the tiles and nothing else.
        if ([[[sublayers objectAtIndex:0] valueForKey:@"isTile"] boolValue]) {
            for (CALayer* sublayer in sublayers.get())
                tiles.append(sublayer);
        } else {
            for (CALayer* sublayer in sublayers.get())
                layerQueue.append(sublayer);
        }
    }

    FloatPoint clampedScrollPosition = clampScrollPosition(currentScrollPosition());
    FloatRect viewportRect({ }, scrollableAreaSize());
    return TileController::blankPixelCountForTiles(tiles, viewportRect, IntPoint(-clampedScrollPosition.x(), -clampedScrollPosition.y()));
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
