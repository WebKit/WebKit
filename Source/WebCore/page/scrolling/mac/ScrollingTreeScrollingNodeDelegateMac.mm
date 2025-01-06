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
#import "ScrollingTreeScrollingNodeDelegateMac.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#import "Logging.h"
#import "ScrollExtents.h"
#import "ScrollingStateScrollingNode.h"
#import "ScrollingTree.h"
#import "ScrollingTreeFrameScrollingNode.h"
#import "ScrollingTreeScrollingNode.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingTreeScrollingNodeDelegateMac);

ScrollingTreeScrollingNodeDelegateMac::ScrollingTreeScrollingNodeDelegateMac(ScrollingTreeScrollingNode& scrollingNode)
    : ThreadedScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_scrollerPair(ScrollerPairMac::create(scrollingNode))
{
}

ScrollingTreeScrollingNodeDelegateMac::~ScrollingTreeScrollingNodeDelegateMac()
{
}

void ScrollingTreeScrollingNodeDelegateMac::nodeWillBeDestroyed()
{
    m_scrollController.stopAllTimers();
}

void ScrollingTreeScrollingNodeDelegateMac::updateFromStateNode(const ScrollingStateScrollingNode& scrollingStateNode)
{
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::PainterForScrollbar)) {
        auto horizontalScrollbar = scrollingStateNode.horizontalScrollerImp();
        auto verticalScrollbar = scrollingStateNode.verticalScrollerImp();
        if (horizontalScrollbar || verticalScrollbar) {
            m_scrollerPair->releaseReferencesToScrollerImpsOnTheMainThread();
            m_scrollerPair->horizontalScroller().setScrollerImp(horizontalScrollbar);
            m_scrollerPair->verticalScroller().setScrollerImp(verticalScrollbar);
        }
    }
    
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollbarHoverState))
        m_scrollerPair->mouseIsInScrollbar(scrollingStateNode.scrollbarHoverState());
    
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer))
        m_scrollerPair->horizontalScroller().setHostLayer(static_cast<CALayer*>(scrollingStateNode.horizontalScrollbarLayer()));
    
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer))
        m_scrollerPair->verticalScroller().setHostLayer(static_cast<CALayer*>(scrollingStateNode.verticalScrollbarLayer()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollableAreaParams)) {
        m_scrollerPair->horizontalScroller().setHiddenByStyle(scrollingStateNode.scrollableAreaParameters().horizontalNativeScrollbarVisibility);
        m_scrollerPair->verticalScroller().setHiddenByStyle(scrollingStateNode.scrollableAreaParameters().verticalNativeScrollbarVisibility);
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollbarEnabledState)) {
        auto scrollbarEnabledState = scrollingStateNode.scrollbarEnabledState();
        m_scrollerPair->horizontalScroller().setEnabled(scrollbarEnabledState.horizontalScrollbarIsEnabled);
        m_scrollerPair->verticalScroller().setEnabled(scrollbarEnabledState.verticalScrollbarIsEnabled);
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollbarLayoutDirection)) {
        auto scrollbarLayoutDirection = scrollingStateNode.scrollbarLayoutDirection();
        m_scrollerPair->horizontalScroller().setScrollbarLayoutDirection(scrollbarLayoutDirection);
        m_scrollerPair->verticalScroller().setScrollbarLayoutDirection(scrollbarLayoutDirection);
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollbarWidth)) {
        auto scrollbarWidth = scrollingStateNode.scrollbarWidth();
        m_scrollerPair->setScrollbarWidth(scrollbarWidth);
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::UseDarkAppearanceForScrollbars))
        m_scrollerPair->setUseDarkAppearance(scrollingStateNode.useDarkAppearanceForScrollbars());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ContentAreaHoverState)) {
        if (scrollingStateNode.mouseIsOverContentArea())
            m_scrollerPair->mouseEnteredContentArea();
        else
            m_scrollerPair->mouseExitedContentArea();
    }
    
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::MouseActivityState))
        m_scrollerPair->mouseMovedInContentArea(scrollingStateNode.mouseLocationState());

    m_scrollerPair->updateValues();

    ThreadedScrollingTreeScrollingNodeDelegate::updateFromStateNode(scrollingStateNode);
}

bool ScrollingTreeScrollingNodeDelegateMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    bool wasInMomentumPhase = m_inMomentumPhase;

    if (wheelEvent.momentumPhase() == PlatformWheelEventPhase::Began)
        m_inMomentumPhase = true;
    else if (wheelEvent.momentumPhase() == PlatformWheelEventPhase::Ended || wheelEvent.momentumPhase() == PlatformWheelEventPhase::Cancelled)
        m_inMomentumPhase = false;
    
    if (wasInMomentumPhase != m_inMomentumPhase)
        m_scrollerPair->setUsePresentationValues(m_inMomentumPhase);

    auto deferrer = ScrollingTreeWheelEventTestMonitorCompletionDeferrer { *scrollingTree(), scrollingNode().scrollingNodeID(), WheelEventTestMonitor::DeferReason::HandlingWheelEvent };

    updateUserScrollInProgressForEvent(wheelEvent);

    // PlatformWheelEventPhase::MayBegin fires when two fingers touch the trackpad, and is used to flash overlay scrollbars.
    // We know we're scrollable at this point, so handle the event.
    if (wheelEvent.phase() == PlatformWheelEventPhase::MayBegin)
        return true;

    return m_scrollController.handleWheelEvent(wheelEvent);
}

void ScrollingTreeScrollingNodeDelegateMac::willDoProgrammaticScroll(const FloatPoint& targetPosition)
{
    if (scrollPositionIsNotRubberbandingEdge(targetPosition)) {
        LOG(Scrolling, "ScrollingTreeScrollingNodeDelegateMac::willDoProgrammaticScroll() - scrolling away from rubberbanding edge so stopping rubberbanding");
        m_scrollController.stopRubberBanding();
    }
}

bool ScrollingTreeScrollingNodeDelegateMac::scrollPositionIsNotRubberbandingEdge(const FloatPoint& targetPosition) const
{
    if (!m_scrollController.isRubberBandInProgress())
        return false;

    auto rubberbandingEdges = m_scrollController.rubberBandingEdges();

    auto minimumScrollPosition = this->minimumScrollPosition();
    auto maximumScrollPosition = this->maximumScrollPosition();

    for (auto side : allBoxSides) {
        if (!rubberbandingEdges[side])
            continue;

        switch (side) {
        case BoxSide::Top:
            if (targetPosition.y() != minimumScrollPosition.y())
                return true;
            break;
        case BoxSide::Right:
            if (targetPosition.x() != maximumScrollPosition.x())
                return true;
            break;
        case BoxSide::Bottom:
            if (targetPosition.y() != maximumScrollPosition.y())
                return true;
            break;
        case BoxSide::Left:
            if (targetPosition.x() != minimumScrollPosition.x())
                return true;
            break;
        }
    }
    return false;
}

void ScrollingTreeScrollingNodeDelegateMac::currentScrollPositionChanged()
{
    m_scrollController.scrollPositionChanged();
}

bool ScrollingTreeScrollingNodeDelegateMac::isRubberBandInProgress() const
{
    return m_scrollController.isRubberBandInProgress();
}

bool ScrollingTreeScrollingNodeDelegateMac::allowsHorizontalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (horizontalScrollElasticity()) {
    case ScrollElasticity::Automatic: {
        bool scrollbarsAllowStretching = allowsHorizontalScrolling() || !allowsVerticalScrolling();
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Horizontal);
        bool eventPreventsStretching = wheelEvent.isGestureStart() && relevantSide && isPinnedOnSide(*relevantSide);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticity::None:
        return false;
    case ScrollElasticity::Allowed: {
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Horizontal);
        if (relevantSide)
            return shouldRubberBandOnSide(*relevantSide);
        return true;
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollingTreeScrollingNodeDelegateMac::allowsVerticalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (verticalScrollElasticity()) {
    case ScrollElasticity::Automatic: {
        bool scrollbarsAllowStretching = allowsVerticalScrolling() || !allowsHorizontalScrolling();
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Vertical);
        bool eventPreventsStretching = wheelEvent.isGestureStart() && relevantSide && isPinnedOnSide(*relevantSide);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticity::None:
        return false;
    case ScrollElasticity::Allowed: {
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Vertical);
        if (relevantSide)
            return shouldRubberBandOnSide(*relevantSide);
        return true;
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}

IntSize ScrollingTreeScrollingNodeDelegateMac::stretchAmount() const
{
    IntSize stretch;
    auto scrollPosition = currentScrollPosition();

    if (scrollPosition.y() < minimumScrollPosition().y())
        stretch.setHeight(scrollPosition.y() - minimumScrollPosition().y());
    else if (scrollPosition.y() > maximumScrollPosition().y())
        stretch.setHeight(scrollPosition.y() - maximumScrollPosition().y());

    if (scrollPosition.x() < minimumScrollPosition().x())
        stretch.setWidth(scrollPosition.x() - minimumScrollPosition().x());
    else if (scrollPosition.x() > maximumScrollPosition().x())
        stretch.setWidth(scrollPosition.x() - maximumScrollPosition().x());

    return stretch;
}

bool ScrollingTreeScrollingNodeDelegateMac::isPinnedOnSide(BoxSide side) const
{
    switch (side) {
    case BoxSide::Top:
        if (!allowsVerticalScrolling())
            return true;
        return currentScrollPosition().y() <= minimumScrollPosition().y();
    case BoxSide::Bottom:
        if (!allowsVerticalScrolling())
            return true;
        return currentScrollPosition().y() >= maximumScrollPosition().y();
    case BoxSide::Left:
        if (!allowsHorizontalScrolling())
            return true;
        return currentScrollPosition().x() <= minimumScrollPosition().x();
    case BoxSide::Right:
        if (!allowsHorizontalScrolling())
            return true;
        return currentScrollPosition().x() >= maximumScrollPosition().x();
    }
    return false;
}

RectEdges<bool> ScrollingTreeScrollingNodeDelegateMac::edgePinnedState() const
{
    return scrollingNode().edgePinnedState();
}

bool ScrollingTreeScrollingNodeDelegateMac::shouldRubberBandOnSide(BoxSide side) const
{
    if (scrollingNode().isRootNode())
        return scrollingTree()->clientAllowsMainFrameRubberBandingOnSide(side);

    switch (side) {
    case BoxSide::Top:
    case BoxSide::Bottom:
        return allowsVerticalScrolling();
    case BoxSide::Left:
    case BoxSide::Right:
        return allowsHorizontalScrolling();
    }
    return true;
}

void ScrollingTreeScrollingNodeDelegateMac::didStopRubberBandAnimation()
{
    // Since the rubberband timer has stopped, totalContentsSizeForRubberBand can be synchronized with totalContentsSize.
    scrollingNode().setTotalContentsSizeForRubberBand(totalContentsSize());
}

void ScrollingTreeScrollingNodeDelegateMac::rubberBandingStateChanged(bool inRubberBand)
{
    scrollingTree()->setRubberBandingInProgressForNode(scrollingNode().scrollingNodeID(), inRubberBand);
}

void ScrollingTreeScrollingNodeDelegateMac::updateScrollbarPainters()
{
    if (m_inMomentumPhase && m_scrollerPair->hasScrollerImp() && m_scrollerPair->isUsingPresentationValues()) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [CATransaction lock];

        auto horizontalValues = m_scrollerPair->valuesForOrientation(ScrollbarOrientation::Horizontal);
        m_scrollerPair->setHorizontalScrollbarPresentationValue(horizontalValues.value);

        auto verticalValues = m_scrollerPair->valuesForOrientation(ScrollbarOrientation::Vertical);
        m_scrollerPair->setVerticalScrollbarPresentationValue(verticalValues.value);

        [CATransaction unlock];
        END_BLOCK_OBJC_EXCEPTIONS
    }
}

void ScrollingTreeScrollingNodeDelegateMac::initScrollbars()
{
    m_scrollerPair->init();
}

void ScrollingTreeScrollingNodeDelegateMac::updateScrollbarLayers()
{
    m_scrollerPair->updateValues();
}

void ScrollingTreeScrollingNodeDelegateMac::handleWheelEventPhase(const PlatformWheelEventPhase wheelEventPhase)
{
    m_scrollerPair->handleWheelEventPhase(wheelEventPhase);
}

void ScrollingTreeScrollingNodeDelegateMac::viewWillStartLiveResize()
{
    return m_scrollerPair->viewWillStartLiveResize();
}

void ScrollingTreeScrollingNodeDelegateMac::viewWillEndLiveResize()
{
    return m_scrollerPair->viewWillEndLiveResize();
}

void ScrollingTreeScrollingNodeDelegateMac::viewSizeDidChange()
{
    return m_scrollerPair->contentsSizeChanged();
}

String ScrollingTreeScrollingNodeDelegateMac::scrollbarStateForOrientation(ScrollbarOrientation orientation) const
{
    return m_scrollerPair->scrollbarStateForOrientation(orientation);
}

} // namespace WebCore

#endif // PLATFORM(MAC) && ENABLE(ASYNC_SCROLLING)
