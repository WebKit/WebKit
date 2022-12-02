/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Adobe. All rights reserved.
 * Copyright (C) 2020 Igalia S.L.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "RenderLayerScrollableArea.h"

#include "Chrome.h"
#include "DebugPageOverlays.h"
#include "DeprecatedGlobalSettings.h"
#include "Editor.h"
#include "ElementRuleCollector.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FrameSelection.h"
#include "HitTestResult.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "RenderFlexibleBox.h"
#include "RenderGeometryMap.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderMarquee.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderView.h"
#include "ScrollAnimator.h"
#include "ScrollbarTheme.h"
#include "ScrollingCoordinator.h"
#include "ShadowRoot.h"
#include <wtf/SetForScope.h>

namespace WebCore {

RenderLayerScrollableArea::RenderLayerScrollableArea(RenderLayer& layer)
    : m_layer(layer)
{
}

RenderLayerScrollableArea::~RenderLayerScrollableArea()
{
}

void RenderLayerScrollableArea::clear()
{
    auto& renderer = m_layer.renderer();
    if (m_registeredScrollableArea)
        renderer.view().frameView().removeScrollableArea(this);

#if ENABLE(IOS_TOUCH_EVENTS)
    unregisterAsTouchEventListenerForScrolling();
#endif
    if (Element* element = renderer.element())
        element->setSavedLayerScrollPosition(m_scrollPosition);

    destroyScrollbar(ScrollbarOrientation::Horizontal);
    destroyScrollbar(ScrollbarOrientation::Vertical);

    if (auto* scrollingCoordinator = renderer.page().scrollingCoordinator())
        scrollingCoordinator->willDestroyScrollableArea(*this);

    clearScrollCorner();
    clearResizer();
}

void RenderLayerScrollableArea::restoreScrollPosition()
{
    auto* element = m_layer.renderer().element();
    if (!element)
        return;

    if (m_layer.renderBox()) {
        // We save and restore only the scrollOffset as the other scroll values are recalculated.
        m_scrollPosition = element->savedLayerScrollPosition();
        if (!m_scrollPosition.isZero())
            scrollAnimator().setCurrentPosition(m_scrollPosition);
    }

    element->setSavedLayerScrollPosition(IntPoint());
}

bool RenderLayerScrollableArea::shouldPlaceVerticalScrollbarOnLeft() const
{
    return m_layer.renderer().shouldPlaceVerticalScrollbarOnLeft();
}

#if ENABLE(IOS_TOUCH_EVENTS)
bool RenderLayerScrollableArea::handleTouchEvent(const PlatformTouchEvent& touchEvent)
{
    // If we have accelerated scrolling, let the scrolling be handled outside of WebKit.
    if (hasCompositedScrollableOverflow())
        return false;

    return ScrollableArea::handleTouchEvent(touchEvent);
}

void RenderLayerScrollableArea::registerAsTouchEventListenerForScrolling()
{
    auto& renderer = m_layer.renderer();
    if (!renderer.element() || m_registeredAsTouchEventListenerForScrolling)
        return;

    renderer.document().addTouchEventHandler(*renderer.element());
    m_registeredAsTouchEventListenerForScrolling = true;
}

void RenderLayerScrollableArea::unregisterAsTouchEventListenerForScrolling()
{
    auto& renderer = m_layer.renderer();
    if (!renderer.element() || !m_registeredAsTouchEventListenerForScrolling)
        return;

    renderer.document().removeTouchEventHandler(*renderer.element());
    m_registeredAsTouchEventListenerForScrolling = false;
}
#endif // ENABLE(IOS_TOUCH_EVENTS)

IntRect RenderLayerScrollableArea::scrollableAreaBoundingBox(bool* isInsideFixed) const
{
    return m_layer.renderer().absoluteBoundingBoxRect(/* useTransforms */ true, isInsideFixed);
}

bool RenderLayerScrollableArea::isUserScrollInProgress() const
{
    if (!scrollsOverflow())
        return false;

    if (auto scrollingCoordinator = m_layer.page().scrollingCoordinator()) {
        if (scrollingCoordinator->isUserScrollInProgress(scrollingNodeID()))
            return true;
    }

    if (auto scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isUserScrollInProgress();

    return false;
}

bool RenderLayerScrollableArea::isRubberBandInProgress() const
{
#if HAVE(RUBBER_BANDING)
    if (!scrollsOverflow())
        return false;

    if (auto scrollingCoordinator = m_layer.page().scrollingCoordinator()) {
        if (scrollingCoordinator->isRubberBandInProgress(scrollingNodeID()))
            return true;
    }

    if (auto scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isRubberBandInProgress();
#endif

    return false;
}

bool RenderLayerScrollableArea::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    return m_layer.renderer().settings().scrollingPerformanceTestingEnabled();
}

// FIXME: this is only valid after we've made layers.
bool RenderLayerScrollableArea::usesAsyncScrolling() const
{
    return m_layer.compositor().useCoordinatedScrollingForLayer(m_layer);
}

void RenderLayerScrollableArea::setPostLayoutScrollPosition(std::optional<ScrollPosition> position)
{
    m_postLayoutScrollPosition = position;
}

void RenderLayerScrollableArea::applyPostLayoutScrollPositionIfNeeded()
{
    if (!m_postLayoutScrollPosition)
        return;

    scrollToOffset(scrollOffsetFromPosition(m_postLayoutScrollPosition.value()));
    m_postLayoutScrollPosition = std::nullopt;
}

void RenderLayerScrollableArea::scrollToXPosition(int x, const ScrollPositionChangeOptions& options)
{
    ScrollPosition position(x, m_scrollPosition.y());
    setScrollPosition(position, options);
}

void RenderLayerScrollableArea::scrollToYPosition(int y, const ScrollPositionChangeOptions& options)
{
    ScrollPosition position(m_scrollPosition.x(), y);
    setScrollPosition(position, options);
}

void RenderLayerScrollableArea::setScrollPosition(const ScrollPosition& position, const ScrollPositionChangeOptions& options)
{
    scrollToOffset(scrollOffsetFromPosition(position), options);
}

ScrollOffset RenderLayerScrollableArea::clampScrollOffset(const ScrollOffset& scrollOffset) const
{
    return scrollOffset.constrainedBetween(minimumScrollOffset(), maximumScrollOffset());
}

bool RenderLayerScrollableArea::requestScrollPositionUpdate(const ScrollPosition& position, ScrollType scrollType, ScrollClamping clamping)
{
#if ENABLE(ASYNC_SCROLLING)
    LOG_WITH_STREAM(Scrolling, stream << m_layer << " requestScrollPositionUpdate " << position);

    if (auto* scrollingCoordinator = m_layer.page().scrollingCoordinator())
        return scrollingCoordinator->requestScrollPositionUpdate(*this, position, scrollType, clamping);
#else
    UNUSED_PARAM(position);
    UNUSED_PARAM(scrollType);
    UNUSED_PARAM(clamping);
#endif
    return false;
}

bool RenderLayerScrollableArea::requestStartKeyboardScrollAnimation(const KeyboardScroll& scrollData)
{
    if (auto* scrollingCoordinator = m_layer.page().scrollingCoordinator())
        return scrollingCoordinator->requestStartKeyboardScrollAnimation(*this, scrollData);

    return false;
}

bool RenderLayerScrollableArea::requestStopKeyboardScrollAnimation(bool immediate)
{
    if (auto* scrollingCoordinator = m_layer.page().scrollingCoordinator())
        return scrollingCoordinator->requestStopKeyboardScrollAnimation(*this, immediate);

    return false;
}

bool RenderLayerScrollableArea::requestAnimatedScrollToPosition(const ScrollPosition& destinationPosition, ScrollClamping clamping)
{
#if ENABLE(ASYNC_SCROLLING)
    LOG_WITH_STREAM(Scrolling, stream << m_layer << " requestAnimatedScrollToPosition " << destinationPosition);

    if (auto* scrollingCoordinator = m_layer.page().scrollingCoordinator())
        return scrollingCoordinator->requestAnimatedScrollToPosition(*this, destinationPosition, clamping);
#else
    UNUSED_PARAM(destinationPosition);
    UNUSED_PARAM(clamping);
#endif
    return false;
}

void RenderLayerScrollableArea::stopAsyncAnimatedScroll()
{
#if ENABLE(ASYNC_SCROLLING)
    LOG_WITH_STREAM(Scrolling, stream << m_layer << " stopAsyncAnimatedScroll");

    if (auto* scrollingCoordinator = m_layer.page().scrollingCoordinator())
        return scrollingCoordinator->stopAnimatedScroll(*this);
#endif
}

ScrollOffset RenderLayerScrollableArea::scrollToOffset(const ScrollOffset& scrollOffset, const ScrollPositionChangeOptions& options)
{
    if (scrollAnimationStatus() == ScrollAnimationStatus::Animating) {
        scrollAnimator().cancelAnimations();
        stopAsyncAnimatedScroll();
    }
    
    registerScrollableArea();

    ScrollOffset clampedScrollOffset = options.clamping == ScrollClamping::Clamped ? clampScrollOffset(scrollOffset) : scrollOffset;
    if (clampedScrollOffset == this->scrollOffset())
        return clampedScrollOffset;

    auto previousScrollType = currentScrollType();
    setCurrentScrollType(options.type);

    ScrollOffset snappedOffset = ceiledIntPoint(scrollAnimator().scrollOffsetAdjustedForSnapping(clampedScrollOffset, options.snapPointSelectionMethod));
    auto snappedPosition = scrollPositionFromOffset(snappedOffset);
    if (options.animated == ScrollIsAnimated::Yes)
        ScrollableArea::scrollToPositionWithAnimation(snappedPosition);
    else if (!requestScrollPositionUpdate(snappedPosition, options.type, options.clamping))
        scrollToPositionWithoutAnimation(snappedPosition, options.clamping);

    setCurrentScrollType(previousScrollType);
    return snappedOffset;
}

void RenderLayerScrollableArea::scrollTo(const ScrollPosition& position)
{
    RenderBox* box = m_layer.renderBox();
    if (!box)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "RenderLayerScrollableArea [" << scrollingNodeID() << "] scrollTo " << position << " from " << m_scrollPosition << " (is user scroll " << (currentScrollType() == ScrollType::User) << ")");

    ScrollPosition newPosition = position;
    if (!box->isHTMLMarquee()) {
        // Ensure that the dimensions will be computed if they need to be (for overflow:hidden blocks).
        if (m_scrollDimensionsDirty)
            computeScrollDimensions();
    }

    if (m_scrollPosition == newPosition && scrollAnimationStatus() == ScrollAnimationStatus::NotAnimating) {
        // FIXME: Nothing guarantees we get a scrollTo() with an unchanged position at the end of a user gesture.
        // The ScrollingCoordinator probably needs to message the main thread when a gesture ends.
        if (requiresScrollPositionReconciliation()) {
            m_layer.setNeedsCompositingGeometryUpdate();
            updateCompositingLayersAfterScroll();
        }
        return;
    }

    m_scrollPosition = newPosition;

    auto& renderer = m_layer.renderer();
    RenderView& view = renderer.view();

    // Update the positions of our child layers (if needed as only fixed layers should be impacted by a scroll).
    // We don't update compositing layers, because we need to do a deep update from the compositing ancestor.
    if (!view.frameView().layoutContext().isInRenderTreeLayout()) {
        // If we're in the middle of layout, we'll just update layers once layout has finished.
        m_layer.updateLayerPositionsAfterOverflowScroll();

        view.frameView().scheduleUpdateWidgetPositions();

        if (!m_updatingMarqueePosition) {
            // Avoid updating compositing layers if, higher on the stack, we're already updating layer
            // positions. Updating layer positions requires a full walk of up-to-date RenderLayers, and
            // in this case we're still updating their positions; we'll update compositing layers later
            // when that completes.
            if (usesCompositedScrolling()) {
                m_layer.setNeedsCompositingGeometryUpdate();

                // Scroll position can affect the location of a composited descendant (which may be a sibling in z-order),
                // so trigger a descendant walk from the stacking context.
                if (auto* paintParent = m_layer.stackingContext())
                    paintParent->setDescendantsNeedUpdateBackingAndHierarchyTraversal();
            }

            updateCompositingLayersAfterScroll();
        }

        // Update regions, scrolling may change the clip of a particular region.
        renderer.document().invalidateRenderingDependentRegions();
        DebugPageOverlays::didLayout(renderer.frame());
    }

    Frame& frame = renderer.frame();
    auto* repaintContainer = renderer.containerForRepaint().renderer;
    // The caret rect needs to be invalidated after scrolling
    frame.selection().setCaretRectNeedsUpdate();

    LayoutRect rectForRepaint = layer().repaintRects() ? layer().repaintRects()->clippedOverflowRect : renderer.clippedOverflowRectForRepaint(repaintContainer);

    FloatQuad quadForFakeMouseMoveEvent = FloatQuad(rectForRepaint);
    if (repaintContainer)
        quadForFakeMouseMoveEvent = repaintContainer->localToAbsoluteQuad(quadForFakeMouseMoveEvent);
    frame.eventHandler().dispatchFakeMouseMoveEventSoonInQuad(quadForFakeMouseMoveEvent);

    bool requiresRepaint = true;
    if (usesCompositedScrolling()) {
        m_layer.setNeedsCompositingGeometryUpdate();
        m_layer.setDescendantsNeedUpdateBackingAndHierarchyTraversal();
        requiresRepaint = m_layer.backing()->needsRepaintOnCompositedScroll();
    }

    auto isScrolledBy = [](RenderObject& renderer, RenderLayer& scrollableLayer) {
        auto layer = renderer.enclosingLayer();
        return layer && layer->ancestorLayerIsInContainingBlockChain(scrollableLayer);
    };

    // Just schedule a full repaint of our object.
    if (requiresRepaint) {
        renderer.repaintUsingContainer(repaintContainer, rectForRepaint);

        // We also have to repaint any descendant composited layers that have fixed backgrounds.
        if (auto slowRepaintObjects = view.frameView().slowRepaintObjects()) {
            for (auto& renderer : *slowRepaintObjects) {
                if (isScrolledBy(renderer, m_layer))
                    renderer.repaint();
            }
        }
    }

    // Schedule the scroll and scroll-related DOM events.
    if (Element* element = renderer.element())
        element->document().addPendingScrollEventTarget(*element);

    if (scrollsOverflow())
        view.frameView().didChangeScrollOffset();

    view.frameView().viewportContentsChanged();
    frame.editor().renderLayerDidScroll(m_layer);
}

void RenderLayerScrollableArea::updateCompositingLayersAfterScroll()
{
    if (m_layer.compositor().hasContentCompositingLayers()) {
        // Our stacking container is guaranteed to contain all of our descendants that may need
        // repositioning, so update compositing layers from there.
        if (RenderLayer* compositingAncestor = m_layer.stackingContext()->enclosingCompositingLayer()) {
            if (usesCompositedScrolling())
                m_layer.compositor().updateCompositingLayers(CompositingUpdateType::OnCompositedScroll, compositingAncestor);
            else {
                // FIXME: would be nice to only dirty layers whose positions were affected by scrolling.
                compositingAncestor->setDescendantsNeedUpdateBackingAndHierarchyTraversal();
                m_layer.compositor().updateCompositingLayers(CompositingUpdateType::OnScroll, compositingAncestor);
            }
        }
    }
}

int RenderLayerScrollableArea::scrollWidth() const
{
    ASSERT(m_layer.renderBox());
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayerScrollableArea*>(this)->computeScrollDimensions();
    // FIXME: This should use snappedIntSize() instead with absolute coordinates.
    return m_scrollWidth;
}

int RenderLayerScrollableArea::scrollHeight() const
{
    ASSERT(m_layer.renderBox());
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayerScrollableArea*>(this)->computeScrollDimensions();
    // FIXME: This should use snappedIntSize() instead with absolute coordinates.
    return m_scrollHeight;
}

void RenderLayerScrollableArea::updateMarqueePosition()
{
    if (!m_marquee)
        return;

    // FIXME: would like to use SetForScope<> but it doesn't work with bitfields.
    bool oldUpdatingMarqueePosition = m_updatingMarqueePosition;
    m_updatingMarqueePosition = true;
    m_marquee->updateMarqueePosition();
    m_updatingMarqueePosition = oldUpdatingMarqueePosition;
}

void RenderLayerScrollableArea::createOrDestroyMarquee()
{
    auto& renderer = m_layer.renderer();
    if (renderer.isHTMLMarquee() && renderer.style().marqueeBehavior() != MarqueeBehavior::None && renderer.isBox()) {
        if (!m_marquee)
            m_marquee = makeUnique<RenderMarquee>(&m_layer);
        m_marquee->updateMarqueeStyle();
    } else if (m_marquee)
        m_marquee = nullptr;
}

bool RenderLayerScrollableArea::scrollsOverflow() const
{
    auto& renderer = m_layer.renderer();
    if (!is<RenderBox>(renderer))
        return false;

    return downcast<RenderBox>(renderer).scrollsOverflow();
}

bool RenderLayerScrollableArea::canUseCompositedScrolling() const
{
    auto& renderer = m_layer.renderer();
    bool isVisible = renderer.style().visibility() == Visibility::Visible;
    if (renderer.settings().asyncOverflowScrollingEnabled())
        return isVisible && scrollsOverflow() && !m_layer.isInsideSVGForeignObject();

#if PLATFORM(IOS_FAMILY) && ENABLE(OVERFLOW_SCROLLING_TOUCH)
    return isVisible && scrollsOverflow() && renderer.style().useTouchOverflowScrolling();
#else
    return false;
#endif
}

void RenderLayerScrollableArea::setScrollOffset(const ScrollOffset& offset)
{
    scrollTo(scrollPositionFromOffset(offset));
}

ScrollingNodeID RenderLayerScrollableArea::scrollingNodeID() const
{
    if (!m_layer.isComposited())
        return 0;

    return m_layer.backing()->scrollingNodeIDForRole(ScrollCoordinationRole::Scrolling);
}

bool RenderLayerScrollableArea::handleWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, std::optional<WheelScrollGestureState> gestureState)
{
    if (!isScrollableOrRubberbandable())
        return false;

#if ENABLE(ASYNC_SCROLLING)
    if (usesAsyncScrolling() && scrollingNodeID()) {
        if (auto* scrollingCoordinator = m_layer.page().scrollingCoordinator())
            return scrollingCoordinator->handleWheelEventForScrolling(wheelEvent, scrollingNodeID(), gestureState);
    }
#endif

    return ScrollableArea::handleWheelEventForScrolling(wheelEvent, gestureState);
}

IntRect RenderLayerScrollableArea::visibleContentRectInternal(VisibleContentRectIncludesScrollbars scrollbarInclusion, VisibleContentRectBehavior) const
{
    IntSize scrollbarSpace;
    if (showsOverflowControls() && scrollbarInclusion == IncludeScrollbars)
        scrollbarSpace = scrollbarIntrusion();

    auto visibleSize = this->visibleSize();
    return { scrollPosition(), { std::max(0, visibleSize.width() - scrollbarSpace.width()), std::max(0, visibleSize.height() - scrollbarSpace.height()) } };
}

IntSize RenderLayerScrollableArea::overhangAmount() const
{
#if HAVE(RUBBER_BANDING)
    auto& renderer = m_layer.renderer();
    if (!renderer.settings().rubberBandingForSubScrollableRegionsEnabled())
        return IntSize();

    IntSize stretch;

    // FIXME: use maximumScrollOffset(), or just move this to ScrollableArea.
    ScrollOffset scrollOffset = scrollOffsetFromPosition(scrollPosition());
    auto reachableSize = reachableTotalContentsSize();
    if (scrollOffset.y() < 0)
        stretch.setHeight(scrollOffset.y());
    else if (reachableSize.height() && scrollOffset.y() > reachableSize.height() - visibleHeight())
        stretch.setHeight(scrollOffset.y() - (reachableSize.height() - visibleHeight()));

    if (scrollOffset.x() < 0)
        stretch.setWidth(scrollOffset.x());
    else if (reachableSize.width() && scrollOffset.x() > reachableSize.width() - visibleWidth())
        stretch.setWidth(scrollOffset.x() - (reachableSize.width() - visibleWidth()));

    return stretch;
#else
    return IntSize();
#endif
}

IntRect RenderLayerScrollableArea::scrollCornerRect() const
{
    return overflowControlsRects().scrollCorner;
}

bool RenderLayerScrollableArea::isScrollCornerVisible() const
{
    ASSERT(m_layer.renderer().isBox());
    return !scrollCornerRect().isEmpty();
}

IntRect RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
{
    auto& renderer = m_layer.renderer();
    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));
    return renderer.view().frameView().convertFromRendererToContainingView(&renderer, rect);
}

IntRect RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
{
    auto& renderer = m_layer.renderer();
    IntRect rect = renderer.view().frameView().convertFromContainingViewToRenderer(&renderer, parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
{
    auto& renderer = m_layer.renderer();
    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return renderer.view().frameView().convertFromRendererToContainingView(&renderer, point);
}

IntPoint RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
{
    auto& renderer = m_layer.renderer();
    IntPoint point = renderer.view().frameView().convertFromContainingViewToRenderer(&renderer, parentPoint);
    point.move(-scrollbarOffset(scrollbar));
    return point;
}

IntSize RenderLayerScrollableArea::visibleSize() const
{
    return m_layer.visibleSize();
}

IntSize RenderLayerScrollableArea::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

IntSize RenderLayerScrollableArea::reachableTotalContentsSize() const
{
    IntSize contentsSize = this->contentsSize();

    if (!hasScrollableHorizontalOverflow())
        contentsSize.setWidth(std::min(contentsSize.width(), visibleSize().width()));

    if (!hasScrollableVerticalOverflow())
        contentsSize.setHeight(std::min(contentsSize.height(), visibleSize().height()));

    return contentsSize;
}

void RenderLayerScrollableArea::availableContentSizeChanged(AvailableSizeChangeReason reason)
{
    ScrollableArea::availableContentSizeChanged(reason);

    auto& renderer = m_layer.renderer();
    if (reason == AvailableSizeChangeReason::ScrollbarsChanged) {
        if (is<RenderBlock>(renderer))
            downcast<RenderBlock>(renderer).setShouldForceRelayoutChildren(true);
        renderer.setNeedsLayout();
    }
}

bool RenderLayerScrollableArea::shouldSuspendScrollAnimations() const
{
    auto& renderer = m_layer.renderer();
    return renderer.view().frameView().shouldSuspendScrollAnimations();
}

#if PLATFORM(IOS_FAMILY)
void RenderLayerScrollableArea::didStartScroll()
{
    m_layer.page().chrome().client().didStartOverflowScroll();
}

void RenderLayerScrollableArea::didEndScroll()
{
    m_layer.page().chrome().client().didEndOverflowScroll();
}

void RenderLayerScrollableArea::didUpdateScroll()
{
    // Send this notification when we scroll, since this is how we keep selection updated.
    m_layer.page().chrome().client().didLayout(ChromeClient::Scroll);
}
#endif

RenderLayer::OverflowControlRects RenderLayerScrollableArea::overflowControlsRects() const
{
    auto& renderer = m_layer.renderer();
    ASSERT(is<RenderBox>(renderer));
    auto& renderBox = downcast<RenderBox>(renderer);
    // Scrollbars sit inside the border box.
    auto overflowControlsPositioningRect = snappedIntRect(renderBox.paddingBoxRectIncludingScrollbar());

    auto horizontalScrollbarHeight = m_hBar ? m_hBar->height() : 0;
    auto verticalScrollbarWidth = m_vBar ? m_vBar->width() : 0;

    auto isNonOverlayScrollbar = [](const Scrollbar* scrollbar) {
        return scrollbar && !scrollbar->isOverlayScrollbar();
    };

    bool haveNonOverlayHorizontalScrollbar = isNonOverlayScrollbar(m_hBar.get());
    bool haveNonOverlayVerticalScrollbar = isNonOverlayScrollbar(m_vBar.get());
    bool placeVerticalScrollbarOnTheLeft = shouldPlaceVerticalScrollbarOnLeft();
    bool haveResizer = renderer.style().resize() != Resize::None;
    bool scrollbarsAvoidCorner = (haveNonOverlayHorizontalScrollbar && haveNonOverlayVerticalScrollbar) || (haveResizer && (haveNonOverlayHorizontalScrollbar || haveNonOverlayVerticalScrollbar));

    IntSize cornerSize;
    if (scrollbarsAvoidCorner) {
        // If only one scrollbar is present, the corner is square.
        cornerSize = IntSize {
            verticalScrollbarWidth ? verticalScrollbarWidth : horizontalScrollbarHeight,
            horizontalScrollbarHeight ? horizontalScrollbarHeight : verticalScrollbarWidth
        };
    }

    RenderLayer::OverflowControlRects result;

    if (m_hBar) {
        auto barRect = overflowControlsPositioningRect;
        barRect.shiftYEdgeTo(barRect.maxY() - horizontalScrollbarHeight);
        if (scrollbarsAvoidCorner) {
            if (placeVerticalScrollbarOnTheLeft)
                barRect.shiftXEdgeTo(barRect.x() + cornerSize.width());
            else
                barRect.contract(cornerSize.width(), 0);
        }

        result.horizontalScrollbar = barRect;
    }

    if (m_vBar) {
        auto barRect = overflowControlsPositioningRect;
        if (placeVerticalScrollbarOnTheLeft)
            barRect.setWidth(verticalScrollbarWidth);
        else
            barRect.shiftXEdgeTo(barRect.maxX() - verticalScrollbarWidth);

        if (scrollbarsAvoidCorner)
            barRect.contract(0, cornerSize.height());

        result.verticalScrollbar = barRect;
    }

    auto cornerRect = [&](IntSize cornerSize) {
        if (placeVerticalScrollbarOnTheLeft) {
            auto bottomLeftCorner = overflowControlsPositioningRect.minXMaxYCorner();
            return IntRect { { bottomLeftCorner.x(), bottomLeftCorner.y() - cornerSize.height(), }, cornerSize };
        }
        return IntRect { overflowControlsPositioningRect.maxXMaxYCorner() - cornerSize, cornerSize };
    };

    if (scrollbarsAvoidCorner)
        result.scrollCorner = cornerRect(cornerSize);

    if (haveResizer) {
        if (scrollbarsAvoidCorner)
            result.resizer = result.scrollCorner;
        else {
            auto scrollbarThickness = ScrollbarTheme::theme().scrollbarThickness();
            result.resizer = cornerRect({ scrollbarThickness, scrollbarThickness });
        }
    }

    return result;
}

IntSize RenderLayerScrollableArea::scrollbarOffset(const Scrollbar& scrollbar) const
{
    auto rects = overflowControlsRects();

    if (&scrollbar == m_vBar.get())
        return toIntSize(rects.verticalScrollbar.location());

    if (&scrollbar == m_hBar.get())
        return toIntSize(rects.horizontalScrollbar.location());

    ASSERT_NOT_REACHED();
    return { };
}

void RenderLayerScrollableArea::invalidateScrollbarRect(Scrollbar& scrollbar, const IntRect& rect)
{
    if (!showsOverflowControls())
        return;

    if (&scrollbar == m_vBar.get()) {
        if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    } else {
        if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    }

    auto scrollRect = rect;
    RenderBox* box = m_layer.renderBox();
    ASSERT(box);
    // If we are not yet inserted into the tree, there is no need to repaint.
    if (!box->parent())
        return;

    auto rects = overflowControlsRects();

    if (&scrollbar == m_vBar.get())
        scrollRect.moveBy(rects.verticalScrollbar.location());
    else
        scrollRect.moveBy(rects.horizontalScrollbar.location());

    LayoutRect repaintRect = scrollRect;
    box->flipForWritingMode(repaintRect);
    box->repaintRectangle(repaintRect);
}

void RenderLayerScrollableArea::invalidateScrollCornerRect(const IntRect& rect)
{
    if (!showsOverflowControls())
        return;

    if (GraphicsLayer* layer = layerForScrollCorner()) {
        layer->setNeedsDisplayInRect(rect);
        return;
    }

    if (m_scrollCorner)
        m_scrollCorner->repaintRectangle(rect);
    if (m_resizer)
        m_resizer->repaintRectangle(rect);
}

static bool scrollbarHiddenByStyle(Scrollbar* scrollbar)
{
    return scrollbar && scrollbar->isHiddenByStyle();
}

bool RenderLayerScrollableArea::horizontalScrollbarHiddenByStyle() const
{
    return scrollbarHiddenByStyle(horizontalScrollbar());
}

bool RenderLayerScrollableArea::verticalScrollbarHiddenByStyle() const
{
    return scrollbarHiddenByStyle(verticalScrollbar());
}

bool RenderLayerScrollableArea::canShowNonOverlayScrollbars() const
{
    return canHaveScrollbars() && !(m_layer.renderBox() && m_layer.renderBox()->canUseOverlayScrollbars());
}

static inline RenderElement* rendererForScrollbar(RenderLayerModelObject& renderer)
{
    if (Element* element = renderer.element()) {
        if (ShadowRoot* shadowRoot = element->containingShadowRoot()) {
            if (shadowRoot->mode() == ShadowRootMode::UserAgent)
                return shadowRoot->host()->renderer();
        }
    }

    return &renderer;
}

Ref<Scrollbar> RenderLayerScrollableArea::createScrollbar(ScrollbarOrientation orientation)
{
    auto& renderer = m_layer.renderer();
    RefPtr<Scrollbar> widget;
    ASSERT(rendererForScrollbar(renderer));
    auto& actualRenderer = *rendererForScrollbar(renderer);
    bool hasCustomScrollbarStyle = is<RenderBox>(actualRenderer) && downcast<RenderBox>(actualRenderer).style().hasPseudoStyle(PseudoId::Scrollbar);
    auto element = downcast<RenderBox>(actualRenderer).element();
    if (hasCustomScrollbarStyle && element)
        widget = RenderScrollbar::createCustomScrollbar(*this, orientation, element);
    else {
        widget = Scrollbar::createNativeScrollbar(*this, orientation, ScrollbarControlSize::Regular);
        didAddScrollbar(widget.get(), orientation);
        if (m_layer.page().isMonitoringWheelEvents())
            scrollAnimator().setWheelEventTestMonitor(m_layer.page().wheelEventTestMonitor());
    }
    renderer.view().frameView().addChild(*widget);
    return widget.releaseNonNull();
}

void RenderLayerScrollableArea::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == ScrollbarOrientation::Horizontal ? m_hBar : m_vBar;
    if (!scrollbar)
        return;

    if (!scrollbar->isCustomScrollbar())
        willRemoveScrollbar(scrollbar.get(), orientation);

    scrollbar->removeFromParent();
    scrollbar = nullptr;
}

void RenderLayerScrollableArea::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasHorizontalScrollbar())
        return;

    if (hasScrollbar) {
        m_hBar = createScrollbar(ScrollbarOrientation::Horizontal);
#if HAVE(RUBBER_BANDING)
        auto& renderer = m_layer.renderer();
        ScrollElasticity elasticity = scrollsOverflow() && renderer.settings().rubberBandingForSubScrollableRegionsEnabled() ? ScrollElasticity::Automatic : ScrollElasticity::None;
        ScrollableArea::setHorizontalScrollElasticity(elasticity);
#endif
    } else {
        destroyScrollbar(ScrollbarOrientation::Horizontal);
#if HAVE(RUBBER_BANDING)
        ScrollableArea::setHorizontalScrollElasticity(ScrollElasticity::None);
#endif
    }

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();
}

void RenderLayerScrollableArea::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasVerticalScrollbar())
        return;

    if (hasScrollbar) {
        m_vBar = createScrollbar(ScrollbarOrientation::Vertical);
#if HAVE(RUBBER_BANDING)
        auto& renderer = m_layer.renderer();
        ScrollElasticity elasticity = scrollsOverflow() && renderer.settings().rubberBandingForSubScrollableRegionsEnabled() ? ScrollElasticity::Automatic : ScrollElasticity::None;
        ScrollableArea::setVerticalScrollElasticity(elasticity);
#endif
    } else {
        destroyScrollbar(ScrollbarOrientation::Vertical);
#if HAVE(RUBBER_BANDING)
        ScrollableArea::setVerticalScrollElasticity(ScrollElasticity::None);
#endif
    }

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();
}

ScrollableArea* RenderLayerScrollableArea::enclosingScrollableArea() const
{
    if (auto* scrollableLayer = m_layer.enclosingScrollableLayer(IncludeSelfOrNot::ExcludeSelf, CrossFrameBoundaries::No))
        return scrollableLayer->scrollableArea();

    auto& renderer = m_layer.renderer();
    return &renderer.view().frameView();
}

bool RenderLayerScrollableArea::isScrollableOrRubberbandable()
{
    auto& renderer = m_layer.renderer();
    return renderer.isScrollableOrRubberbandableBox();
}

bool RenderLayerScrollableArea::hasScrollableOrRubberbandableAncestor()
{
    for (auto* nextLayer = m_layer.enclosingContainingBlockLayer(CrossFrameBoundaries::Yes); nextLayer; nextLayer = nextLayer->enclosingContainingBlockLayer(CrossFrameBoundaries::Yes)) {
        if (nextLayer->renderer().isScrollableOrRubberbandableBox())
            return true;
    }

    return false;
}

int RenderLayerScrollableArea::verticalScrollbarWidth(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_vBar
        || !showsOverflowControls()
        || (m_vBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_vBar->shouldParticipateInHitTesting())))
        return 0;

    return m_vBar->width();
}

int RenderLayerScrollableArea::horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_hBar
        || !showsOverflowControls()
        || (m_hBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_hBar->shouldParticipateInHitTesting())))
        return 0;

    return m_hBar->height();
}

OverscrollBehavior RenderLayerScrollableArea::horizontalOverscrollBehavior() const
{
    if (m_layer.renderBox() && m_layer.renderBox()->canBeScrolledAndHasScrollableArea())
        return m_layer.renderer().style().overscrollBehaviorX();
    return OverscrollBehavior::Auto;
}

OverscrollBehavior RenderLayerScrollableArea::verticalOverscrollBehavior() const
{
    if (m_layer.renderBox() && m_layer.renderBox()->canBeScrolledAndHasScrollableArea())
        return m_layer.renderer().style().overscrollBehaviorY();
    return OverscrollBehavior::Auto;
}

bool RenderLayerScrollableArea::hasOverflowControls() const
{
    return m_hBar || m_vBar || m_scrollCorner || m_layer.renderer().style().resize() != Resize::None;
}

void RenderLayerScrollableArea::positionOverflowControls(const IntSize& offsetFromRoot)
{
    if (!m_hBar && !m_vBar && !m_layer.canResize())
        return;

    if (!m_layer.renderBox())
        return;

    auto rects = overflowControlsRects();

    if (m_vBar) {
        rects.verticalScrollbar.move(offsetFromRoot);
        m_vBar->setFrameRect(rects.verticalScrollbar);
    }

    if (m_hBar) {
        rects.horizontalScrollbar.move(offsetFromRoot);
        m_hBar->setFrameRect(rects.horizontalScrollbar);
    }

    if (m_scrollCorner)
        m_scrollCorner->setFrameRect(rects.scrollCorner);

    if (m_resizer)
        m_resizer->setFrameRect(rects.resizer);
}

LayoutUnit RenderLayerScrollableArea::overflowTop() const
{
    RenderBox* box = m_layer.renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.y();
}

LayoutUnit RenderLayerScrollableArea::overflowBottom() const
{
    RenderBox* box = m_layer.renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.maxY();
}

LayoutUnit RenderLayerScrollableArea::overflowLeft() const
{
    RenderBox* box = m_layer.renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.x();
}

LayoutUnit RenderLayerScrollableArea::overflowRight() const
{
    RenderBox* box = m_layer.renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.maxX();
}

void RenderLayerScrollableArea::computeScrollDimensions()
{
    m_scrollDimensionsDirty = false;

    m_scrollWidth = roundToInt(overflowRight() - overflowLeft());
    m_scrollHeight = roundToInt(overflowBottom() - overflowTop());

    computeScrollOrigin();
    computeHasCompositedScrollableOverflow();
}

void RenderLayerScrollableArea::computeScrollOrigin()
{
    RenderBox* box = m_layer.renderBox();
    ASSERT(box);

    int scrollableLeftOverflow = roundToInt(overflowLeft() - box->borderLeft());
    if (shouldPlaceVerticalScrollbarOnLeft() /*|| box->style().writingMode() == WritingMode::RightToLeft*/)
        scrollableLeftOverflow -= verticalScrollbarWidth();
    int scrollableTopOverflow = roundToInt(overflowTop() - box->borderTop());
    setScrollOrigin(IntPoint(-scrollableLeftOverflow, -scrollableTopOverflow));

    // Horizontal scrollbar offsets depend on the scroll origin when vertical
    // scrollbars are on the left.
    if (m_hBar)
        m_hBar->offsetDidChange();
}

void RenderLayerScrollableArea::computeHasCompositedScrollableOverflow()
{
    m_hasCompositedScrollableOverflow = canUseCompositedScrolling() && (hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

bool RenderLayerScrollableArea::hasScrollableHorizontalOverflow() const
{
    return hasHorizontalOverflow() && m_layer.renderBox()->scrollsOverflowX();
}

bool RenderLayerScrollableArea::hasScrollableVerticalOverflow() const
{
    return hasVerticalOverflow() && m_layer.renderBox()->scrollsOverflowY();
}

bool RenderLayerScrollableArea::hasHorizontalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollWidth() > roundToInt(m_layer.renderBox()->clientWidth());
}

bool RenderLayerScrollableArea::hasVerticalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollHeight() > roundToInt(m_layer.renderBox()->clientHeight());
}

void RenderLayerScrollableArea::updateScrollbarPresenceAndState(std::optional<bool> hasHorizontalOverflow, std::optional<bool> hasVerticalOverflow)
{
    auto* box = m_layer.renderBox();
    ASSERT(box);

    enum class ScrollbarState {
        NoScrollbar,
        Enabled,
        Disabled
    };

    auto scrollbarForAxis = [&](ScrollbarOrientation orientation) -> RefPtr<Scrollbar>& {
        return orientation == ScrollbarOrientation::Horizontal ? m_hBar : m_vBar;
    };

    auto stateForScrollbar = [&](ScrollbarOrientation orientation, std::optional<bool> hasOverflow, ScrollbarState nonScrollableState) {
        if (hasOverflow)
            return *hasOverflow ? ScrollbarState::Enabled : nonScrollableState;
        
        // If we don't have information about overflow (because we haven't done layout yet), just return the current state of the scrollbar.
        auto existingScrollbar = scrollbarForAxis(orientation);
        return (existingScrollbar && existingScrollbar->enabled()) ? ScrollbarState::Enabled : nonScrollableState;
    };

    auto stateForScrollbarOnAxis = [&](ScrollbarOrientation orientation, std::optional<bool> hasOverflow) {
        if (box->hasAlwaysPresentScrollbar(orientation))
            return stateForScrollbar(orientation, hasOverflow, ScrollbarState::Disabled);

        if (box->hasAutoScrollbar(orientation))
            return stateForScrollbar(orientation, hasOverflow, ScrollbarState::NoScrollbar);

        return ScrollbarState::NoScrollbar;
    };

    auto horizontalBarState = stateForScrollbarOnAxis(ScrollbarOrientation::Horizontal, hasHorizontalOverflow);
    setHasHorizontalScrollbar(horizontalBarState != ScrollbarState::NoScrollbar);
    if (horizontalBarState != ScrollbarState::NoScrollbar)
        m_hBar->setEnabled(horizontalBarState == ScrollbarState::Enabled);

    auto verticalBarState = stateForScrollbarOnAxis(ScrollbarOrientation::Vertical, hasVerticalOverflow);
    setHasVerticalScrollbar(verticalBarState != ScrollbarState::NoScrollbar);
    if (verticalBarState != ScrollbarState::NoScrollbar)
        m_vBar->setEnabled(verticalBarState == ScrollbarState::Enabled);
}

void RenderLayerScrollableArea::updateScrollbarsAfterStyleChange(const RenderStyle* oldStyle)
{
    // Overflow is a box concept.
    RenderBox* box = m_layer.renderBox();
    if (!box)
        return;

    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style().effectiveAppearance() == ListboxPart)
        return;

    bool hadVerticalScrollbar = hasVerticalScrollbar();
    updateScrollbarPresenceAndState();
    bool hasVerticalScrollbar = this->hasVerticalScrollbar();

    if (hadVerticalScrollbar != hasVerticalScrollbar || (hasVerticalScrollbar && oldStyle && oldStyle->shouldPlaceVerticalScrollbarOnLeft() != box->style().shouldPlaceVerticalScrollbarOnLeft()))
        computeScrollOrigin();

    if (!m_scrollDimensionsDirty)
        updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

void RenderLayerScrollableArea::updateScrollbarsAfterLayout()
{
    RenderBox* box = m_layer.renderBox();
    ASSERT(box);

    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style().effectiveAppearance() == ListboxPart)
        return;

    bool hadHorizontalScrollbar = hasHorizontalScrollbar();
    bool hadVerticalScrollbar = hasVerticalScrollbar();

    updateScrollbarPresenceAndState(hasHorizontalOverflow(), hasVerticalOverflow());

    // Scrollbars with auto behavior may need to lay out again if scrollbars got added or removed.
    bool autoHorizontalScrollBarChanged = box->hasAutoScrollbar(ScrollbarOrientation::Horizontal) && (hadHorizontalScrollbar != hasHorizontalScrollbar());
    bool autoVerticalScrollBarChanged = box->hasAutoScrollbar(ScrollbarOrientation::Vertical) && (hadVerticalScrollbar != hasVerticalScrollbar());

    if (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged) {
        if (autoVerticalScrollBarChanged && shouldPlaceVerticalScrollbarOnLeft())
            computeScrollOrigin();

        m_layer.updateSelfPaintingLayer();

        auto& renderer = m_layer.renderer();
        renderer.repaint();

        if (renderer.style().overflowX() == Overflow::Auto || renderer.style().overflowY() == Overflow::Auto) {
            if (!m_inOverflowRelayout) {
                SetForScope inOverflowRelayoutScope(m_inOverflowRelayout, true);
                renderer.setNeedsLayout(MarkOnlyThis);
                if (is<RenderBlock>(renderer)) {
                    auto& block = downcast<RenderBlock>(renderer);
                    block.scrollbarsChanged(autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
                    block.layoutBlock(true);
                } else
                    renderer.layout();
            }
        }

        // FIXME: This does not belong here.
        RenderObject* parent = renderer.parent();
        if (parent && parent->isFlexibleBox() && renderer.isBox())
            downcast<RenderFlexibleBox>(parent)->clearCachedMainSizeForChild(*m_layer.renderBox());
    }

    // Set up the range.
    if (m_hBar)
        m_hBar->setProportion(roundToInt(box->clientWidth()), m_scrollWidth);
    if (m_vBar)
        m_vBar->setProportion(roundToInt(box->clientHeight()), m_scrollHeight);

    updateScrollbarSteps();

    updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

void RenderLayerScrollableArea::updateScrollbarSteps()
{
    RenderBox* box = m_layer.renderBox();
    ASSERT(box);

    LayoutRect paddedLayerBounds(0_lu, 0_lu, box->clientWidth(), box->clientHeight());
    paddedLayerBounds.contract(box->scrollPaddingForViewportRect(paddedLayerBounds));

    // Set up the  page step/line step.
    if (m_hBar) {
        int pageStep = Scrollbar::pageStep(roundToInt(paddedLayerBounds.width()));
        m_hBar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
    }
    if (m_vBar) {
        int pageStep = Scrollbar::pageStep(roundToInt(paddedLayerBounds.height()));
        m_vBar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
    }
}


// This is called from layout code (before updateLayerPositions).
void RenderLayerScrollableArea::updateScrollInfoAfterLayout()
{
    RenderBox* box = m_layer.renderBox();
    if (!box)
        return;

    m_scrollDimensionsDirty = true;
    ScrollPosition originalScrollPosition = scrollPosition();

    computeScrollDimensions();
    m_layer.updateSelfPaintingLayer();

    // FIXME: Ensure that offsets are also updated in case of programmatic style changes.
    // https://bugs.webkit.org/show_bug.cgi?id=135964
    updateSnapOffsets();

    if (!box->isHTMLMarquee() && !isRubberBandInProgress() && !isUserScrollInProgress()) {
        // Layout may cause us to be at an invalid scroll position. In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        ScrollOffset clampedScrollOffset = clampScrollOffset(scrollOffset());
        if (clampedScrollOffset != scrollOffset())
            scrollToOffset(clampedScrollOffset);
    }

    updateScrollbarsAfterLayout();

    LOG_WITH_STREAM(Scrolling, stream << "RenderLayerScrollableArea [" << scrollingNodeID() << "] updateScrollInfoAfterLayout - new scroll width " << m_scrollWidth << " scroll height " << m_scrollHeight
        << " rubber banding " << isRubberBandInProgress() << " user scrolling " << isUserScrollInProgress() << " scroll position updated from " << originalScrollPosition << " to " << scrollPosition());

    if (originalScrollPosition != scrollPosition())
        scrollToPositionWithoutAnimation(IntPoint(scrollPosition()));

    if (m_layer.isComposited()) {
        m_layer.setNeedsCompositingGeometryUpdate();
        m_layer.setNeedsCompositingConfigurationUpdate();
    }

    if (canUseCompositedScrolling())
        m_layer.setNeedsPostLayoutCompositingUpdate();

    resnapAfterLayout();

    InspectorInstrumentation::didAddOrRemoveScrollbars(m_layer.renderer());
}

bool RenderLayerScrollableArea::overflowControlsIntersectRect(const IntRect& localRect) const
{
    auto rects = overflowControlsRects();

    if (rects.horizontalScrollbar.intersects(localRect))
        return true;

    if (rects.verticalScrollbar.intersects(localRect))
        return true;

    if (rects.scrollCorner.intersects(localRect))
        return true;

    if (rects.resizer.intersects(localRect))
        return true;

    return false;
}

bool RenderLayerScrollableArea::showsOverflowControls() const
{
#if PLATFORM(IOS_FAMILY)
    // On iOS, the scrollbars are made in the UI process.
    return !canUseCompositedScrolling();
#endif

    return true;
}

void RenderLayerScrollableArea::paintOverflowControls(GraphicsContext& context, const IntPoint& paintOffset, const IntRect& damageRect, bool paintingOverlayControls)
{
    // Don't do anything if we have no overflow.
    auto& renderer = m_layer.renderer();
    if (!renderer.hasNonVisibleOverflow())
        return;

    if (!showsOverflowControls())
        return;

    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the
    // second pass doesn't need to re-enter the RenderTree to get it right.
    if (hasOverlayScrollbars() && !paintingOverlayControls) {
        m_cachedOverlayScrollbarOffset = paintOffset;

        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((m_hBar && layerForHorizontalScrollbar()) || (m_vBar && layerForVerticalScrollbar()))
            return;
        IntRect localDamgeRect = damageRect;
        localDamgeRect.moveBy(-paintOffset);
        if (!overflowControlsIntersectRect(localDamgeRect))
            return;

        RenderLayer* paintingRoot = m_layer.enclosingCompositingLayer();
        if (!paintingRoot)
            paintingRoot = renderer.view().layer();

        if (auto* scrollableArea = paintingRoot->scrollableArea())
            scrollableArea->setContainsDirtyOverlayScrollbars(true);
        return;
    }

    // This check is required to avoid painting custom CSS scrollbars twice.
    if (paintingOverlayControls && !hasOverlayScrollbars())
        return;

    IntPoint adjustedPaintOffset = paintOffset;
    if (paintingOverlayControls)
        adjustedPaintOffset = m_cachedOverlayScrollbarOffset;

    // Move the scrollbar widgets if necessary. We normally move and resize widgets during layout, but sometimes
    // widgets can move without layout occurring (most notably when you scroll a document that
    // contains fixed positioned elements).
    positionOverflowControls(toIntSize(adjustedPaintOffset));

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar && !layerForHorizontalScrollbar())
        m_hBar->paint(context, damageRect);
    if (m_vBar && !layerForVerticalScrollbar())
        m_vBar->paint(context, damageRect);

    if (layerForScrollCorner())
        return;

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    paintScrollCorner(context, adjustedPaintOffset, damageRect);

    // Paint our resizer last, since it sits on top of the scroll corner.
    paintResizer(context, adjustedPaintOffset, damageRect);
}

void RenderLayerScrollableArea::paintScrollCorner(GraphicsContext& context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    IntRect absRect = scrollCornerRect();
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context.invalidatingControlTints()) {
        updateScrollCornerStyle();
        return;
    }

    if (m_scrollCorner) {
        m_scrollCorner->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    // We don't want to paint a corner if we have overlay scrollbars, since we need
    // to see what is behind it.
    if (!hasOverlayScrollbars())
        ScrollbarTheme::theme().paintScrollCorner(*this, context, absRect);
}

void RenderLayerScrollableArea::drawPlatformResizerImage(GraphicsContext& context, const LayoutRect& resizerCornerRect)
{
    auto& renderer = m_layer.renderer();
    RefPtr<Image> resizeCornerImage;
    FloatSize cornerResizerSize;
    if (renderer.document().deviceScaleFactor() >= 2) {
        static NeverDestroyed<Image*> resizeCornerImageHiRes(&Image::loadPlatformResource("textAreaResizeCorner@2x").leakRef());
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        static NeverDestroyed<Image*> resizeCornerImageLoRes(&Image::loadPlatformResource("textAreaResizeCorner").leakRef());
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (shouldPlaceVerticalScrollbarOnLeft()) {
        context.save();
        context.translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context.scale(FloatSize(-1.0, 1.0));
        if (resizeCornerImage)
            context.drawImage(*resizeCornerImage, FloatRect(FloatPoint(), cornerResizerSize));
        context.restore();
        return;
    }

    if (!resizeCornerImage)
        return;
    FloatRect imageRect = snapRectToDevicePixels(LayoutRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize), renderer.document().deviceScaleFactor());
    context.drawImage(*resizeCornerImage, imageRect);
}

void RenderLayerScrollableArea::paintResizer(GraphicsContext& context, const LayoutPoint& paintOffset, const LayoutRect& damageRect)
{
    auto& renderer = m_layer.renderer();
    if (renderer.style().resize() == Resize::None)
        return;

    auto rects = overflowControlsRects();

    LayoutRect resizerAbsRect = rects.resizer;
    resizerAbsRect.moveBy(paintOffset);
    if (!resizerAbsRect.intersects(damageRect))
        return;

    if (context.invalidatingControlTints()) {
        updateResizerStyle();
        return;
    }

    if (m_resizer) {
        m_resizer->paintIntoRect(context, paintOffset, resizerAbsRect);
        return;
    }

    drawPlatformResizerImage(context, resizerAbsRect);

    // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
    // Clipping will exclude the right and bottom edges of this frame.
    if (!hasOverlayScrollbars() && (m_vBar || m_hBar)) {
        GraphicsContextStateSaver stateSaver(context);
        context.clip(resizerAbsRect);
        LayoutRect largerCorner = resizerAbsRect;
        largerCorner.setSize(LayoutSize(largerCorner.width() + 1_lu, largerCorner.height() + 1_lu));
        context.setStrokeColor(SRGBA<uint8_t> { 217, 217, 217 });
        context.setStrokeThickness(1.0f);
        context.setFillColor(Color::transparentBlack);
        context.drawRect(snappedIntRect(largerCorner));
    }
}

bool RenderLayerScrollableArea::hitTestOverflowControls(HitTestResult& result, const IntPoint& localPoint)
{
    if (!m_hBar && !m_vBar && !m_layer.canResize())
        return false;

    auto rects = overflowControlsRects();

    IntRect resizeControlRect;
    auto& renderer = m_layer.renderer();
    if (renderer.style().resize() != Resize::None) {
        if (rects.resizer.contains(localPoint))
            return true;
    }

    // FIXME: We should hit test the m_scrollCorner and pass it back through the result.
    if (m_vBar && m_vBar->shouldParticipateInHitTesting()) {
        if (rects.verticalScrollbar.contains(localPoint)) {
            result.setScrollbar(m_vBar.get());
            return true;
        }
    }

    if (m_hBar && m_hBar->shouldParticipateInHitTesting()) {
        if (rects.horizontalScrollbar.contains(localPoint)) {
            result.setScrollbar(m_hBar.get());
            return true;
        }
    }

    return false;
}

bool RenderLayerScrollableArea::scroll(ScrollDirection direction, ScrollGranularity granularity, unsigned stepCount)
{
    return ScrollableArea::scroll(direction, granularity, stepCount);
}

bool RenderLayerScrollableArea::isActive() const
{
    return m_layer.page().focusController().isActive();
}

IntPoint RenderLayerScrollableArea::lastKnownMousePositionInView() const
{
    return m_layer.renderer().view().frameView().lastKnownMousePositionInView();
}

bool RenderLayerScrollableArea::isHandlingWheelEvent() const
{
    return m_layer.renderer().frame().eventHandler().isHandlingWheelEvent();
}

bool RenderLayerScrollableArea::useDarkAppearance() const
{
    return m_layer.renderer().useDarkAppearance();
}

void RenderLayerScrollableArea::updateSnapOffsets()
{
    // FIXME: Extend support beyond HTMLElements.
    if (!is<HTMLElement>(m_layer.enclosingElement()) || !m_layer.enclosingElement()->renderBox())
        return;

    RenderBox* box = m_layer.enclosingElement()->renderBox();
    updateSnapOffsetsForScrollableArea(*this, *box, box->style(), box->paddingBoxRect(), box->style().writingMode(), box->style().direction(), m_layer.renderer().document().focusedElement());
}

bool RenderLayerScrollableArea::isScrollSnapInProgress() const
{
    if (!scrollsOverflow())
        return false;

    if (auto* scrollingCoordinator = m_layer.page().scrollingCoordinator()) {
        if (scrollingCoordinator->isScrollSnapInProgress(scrollingNodeID()))
            return true;
    }

    if (auto* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isScrollSnapInProgress();

    return false;
}

bool RenderLayerScrollableArea::scrollAnimatorEnabled() const
{
    return m_layer.page().settings().scrollAnimatorEnabled();
}

void RenderLayerScrollableArea::paintOverlayScrollbars(GraphicsContext& context, const LayoutRect& damageRect, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot)
{
    if (!m_containsDirtyOverlayScrollbars)
        return;

    RenderLayer::LayerPaintingInfo paintingInfo(&m_layer, enclosingIntRect(damageRect), paintBehavior, LayoutSize(), subtreePaintRoot);
    m_layer.paintLayer(context, paintingInfo, RenderLayer::PaintLayerFlag::PaintingOverlayScrollbars);

    m_containsDirtyOverlayScrollbars = false;
}

bool RenderLayerScrollableArea::hitTestResizerInFragments(const LayerFragments& layerFragments, const HitTestLocation& hitTestLocation, LayoutPoint& pointInFragment) const
{
    if (layerFragments.isEmpty())
        return false;

    auto& renderer = m_layer.renderer();
    if (!renderer.visibleToHitTesting())
        return false;

    auto borderBoxRect = snappedIntRect(downcast<RenderBox>(renderer).borderBoxRect());
    auto rects = overflowControlsRects();

    auto cornerRectInFragment = [&](const IntRect& fragmentBounds, const IntRect& resizerRect) {
        if (shouldPlaceVerticalScrollbarOnLeft()) {
            IntSize offsetFromBottomLeft = borderBoxRect.minXMaxYCorner() - resizerRect.minXMaxYCorner();
            return IntRect { fragmentBounds.minXMaxYCorner() - offsetFromBottomLeft - IntSize { 0, resizerRect.height() }, resizerRect.size() };
        }
        IntSize offsetFromBottomRight = borderBoxRect.maxXMaxYCorner() - resizerRect.maxXMaxYCorner();
        return IntRect { fragmentBounds.maxXMaxYCorner() - offsetFromBottomRight - resizerRect.size(), resizerRect.size() };
    };

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = layerFragments.at(i);
        auto resizerRectInFragment = cornerRectInFragment(snappedIntRect(fragment.layerBounds), rects.resizer);
        if (fragment.backgroundRect.intersects(hitTestLocation) && resizerRectInFragment.contains(hitTestLocation.roundedPoint())) {
            pointInFragment = toLayoutPoint(hitTestLocation.point() - fragment.layerBounds.location());
            return true;
        }
    }

    return false;
}

GraphicsLayer* RenderLayerScrollableArea::layerForHorizontalScrollbar() const
{
    return m_layer.backing() ? m_layer.backing()->layerForHorizontalScrollbar() : nullptr;
}

GraphicsLayer* RenderLayerScrollableArea::layerForVerticalScrollbar() const
{
    return m_layer.backing() ? m_layer.backing()->layerForVerticalScrollbar() : nullptr;
}

GraphicsLayer* RenderLayerScrollableArea::layerForScrollCorner() const
{
    return m_layer.backing() ? m_layer.backing()->layerForScrollCorner() : nullptr;
}

bool RenderLayerScrollableArea::scrollingMayRevealBackground() const
{
    return scrollsOverflow() || usesCompositedScrolling();
}
bool RenderLayerScrollableArea::isVisibleToHitTesting() const
{
    auto& renderer = m_layer.renderer();
    FrameView& frameView = renderer.view().frameView();
    return renderer.visibleToHitTesting() && frameView.isVisibleToHitTesting();
}

void RenderLayerScrollableArea::updateScrollableAreaSet(bool hasOverflow)
{
    auto& renderer = m_layer.renderer();
    FrameView& frameView = renderer.view().frameView();

    bool isVisibleToHitTest = renderer.visibleToHitTesting();
    if (HTMLFrameOwnerElement* owner = frameView.frame().ownerElement())
        isVisibleToHitTest &= owner->renderer() && owner->renderer()->visibleToHitTesting();

    bool needsToBeRegistered = (hasOverflow && isVisibleToHitTest) || scrollAnimationStatus() == ScrollAnimationStatus::Animating;
    bool addedOrRemoved = false;

    if (needsToBeRegistered) {
        if (!m_registeredScrollableArea) {
            addedOrRemoved = frameView.addScrollableArea(this);
            m_registeredScrollableArea = true;
        }
    } else if (m_registeredScrollableArea) {
        addedOrRemoved = frameView.removeScrollableArea(this);
        m_registeredScrollableArea = false;
    }

#if ENABLE(IOS_TOUCH_EVENTS)
    if (addedOrRemoved) {
        if (needsToBeRegistered && !canUseCompositedScrolling())
            registerAsTouchEventListenerForScrolling();
        else {
            // We only need the touch listener for unaccelerated overflow scrolling, so if we became
            // accelerated, remove ourselves as a touch event listener.
            unregisterAsTouchEventListenerForScrolling();
        }
    }
#else
    UNUSED_VARIABLE(addedOrRemoved);
#endif
}

void RenderLayerScrollableArea::registerScrollableArea()
{
    auto& renderer = m_layer.renderer();
    FrameView& frameView = renderer.view().frameView();

    if (!m_registeredScrollableArea) {
        frameView.addScrollableArea(this);
        m_registeredScrollableArea = true;
    }
}

void RenderLayerScrollableArea::updateScrollCornerStyle()
{
    auto& renderer = m_layer.renderer();
    RenderElement* actualRenderer = rendererForScrollbar(renderer);
    auto corner = renderer.hasNonVisibleOverflow() ? actualRenderer->getUncachedPseudoStyle({ PseudoId::ScrollbarCorner }, &actualRenderer->style()) : nullptr;

    if (!corner) {
        clearScrollCorner();
        return;
    }

    if (!m_scrollCorner) {
        m_scrollCorner = createRenderer<RenderScrollbarPart>(renderer.document(), WTFMove(*corner));
        // FIXME: A renderer should be a child of its parent!
        m_scrollCorner->setParent(&renderer);
        m_scrollCorner->initializeStyle();
    } else
        m_scrollCorner->setStyle(WTFMove(*corner));
}

void RenderLayerScrollableArea::clearScrollCorner()
{
    if (!m_scrollCorner)
        return;
    m_scrollCorner->setParent(nullptr);
    m_scrollCorner = nullptr;
}

void RenderLayerScrollableArea::updateResizerStyle()
{
    auto& renderer = m_layer.renderer();
    RenderElement* actualRenderer = rendererForScrollbar(renderer);
    auto resizer = renderer.hasNonVisibleOverflow() ? actualRenderer->getUncachedPseudoStyle({ PseudoId::Resizer }, &actualRenderer->style()) : nullptr;

    if (!resizer) {
        clearResizer();
        return;
    }

    if (!m_resizer) {
        m_resizer = createRenderer<RenderScrollbarPart>(renderer.document(), WTFMove(*resizer));
        // FIXME: A renderer should be a child of its parent!
        m_resizer->setParent(&renderer);
        m_resizer->initializeStyle();
    } else
        m_resizer->setStyle(WTFMove(*resizer));
}

void RenderLayerScrollableArea::clearResizer()
{
    if (!m_resizer)
        return;
    m_resizer->setParent(nullptr);
    m_resizer = nullptr;
}

void RenderLayerScrollableArea::updateAllScrollbarRelatedStyle()
{
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();
    updateScrollCornerStyle();
    updateResizerStyle();
}

// FIXME: this is only valid after we've made layers.
bool RenderLayerScrollableArea::usesCompositedScrolling() const
{
    return m_layer.isComposited() && m_layer.backing()->hasScrollingLayer();
}

static inline int adjustedScrollDelta(int beginningDelta)
{
    // This implemention matches Firefox's.
    // http://mxr.mozilla.org/firefox/source/toolkit/content/widgets/browser.xml#856.
    const int speedReducer = 12;

    int adjustedDelta = beginningDelta / speedReducer;
    if (adjustedDelta > 1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(adjustedDelta))) - 1;
    else if (adjustedDelta < -1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(-adjustedDelta))) + 1;

    return adjustedDelta;
}

static inline IntSize adjustedScrollDelta(const IntSize& delta)
{
    return IntSize(adjustedScrollDelta(delta.width()), adjustedScrollDelta(delta.height()));
}

void RenderLayerScrollableArea::panScrollFromPoint(const IntPoint& sourcePoint)
{
    IntPoint lastKnownMousePosition = m_layer.renderer().frame().eventHandler().lastKnownMousePosition();

    // We need to check if the last known mouse position is out of the window. When the mouse is out of the window, the position is incoherent
    static IntPoint previousMousePosition;
    if (lastKnownMousePosition.x() < 0 || lastKnownMousePosition.y() < 0)
        lastKnownMousePosition = previousMousePosition;
    else
        previousMousePosition = lastKnownMousePosition;

    IntSize delta = lastKnownMousePosition - sourcePoint;

    if (abs(delta.width()) <= ScrollView::noPanScrollRadius) // at the center we let the space for the icon
        delta.setWidth(0);
    if (abs(delta.height()) <= ScrollView::noPanScrollRadius)
        delta.setHeight(0);

    scrollByRecursively(adjustedScrollDelta(delta));
}
static LayoutRect getLocalExposeRect(const LayoutRect& absoluteRect, RenderBox* box, int verticalScrollbarWidth, const LayoutRect& layerBounds)
{
    LayoutRect localExposeRect(box->absoluteToLocalQuad(FloatQuad(FloatRect(absoluteRect))).boundingBox());

    // localExposedRect is now the absolute rect in local coordinates, but relative to the
    // border edge. Make the rectangle relative to the scrollable area.
    localExposeRect.moveBy(-LayoutPoint(box->borderLeft(), box->borderTop()));

    if (box->shouldPlaceVerticalScrollbarOnLeft()) {
        // For `direction: rtl; writing-mode: horizontal-{tb,bt}` and `writing-mode: vertical-rl`
        // boxes, the scroll bar is on the left side. The visible rect starts from the right side
        // of the scroll bar. So the x of localExposeRect should start from the same position too.
        localExposeRect.moveBy(LayoutPoint(-verticalScrollbarWidth, 0));
    }

    // scroll-padding applies to the scroll container, but expand the rectangle that we want to expose in order
    // simulate padding the scroll container. This rectangle is passed up the tree of scrolling elements to
    // ensure that the padding on this scroll container is maintained.
    localExposeRect.expand(box->scrollPaddingForViewportRect(layerBounds));
    return localExposeRect;
}

LayoutRect RenderLayerScrollableArea::scrollRectToVisible(const LayoutRect& absoluteRect, const ScrollRectToVisibleOptions& options)
{
    RenderBox* box = layer().renderBox();
    ASSERT(box);
    
    LayoutRect layerBounds(0_lu, 0_lu, box->clientWidth(), box->clientHeight());

    LayoutRect localExposeRect = getLocalExposeRect(absoluteRect, box, verticalScrollbarWidth(), layerBounds);
    std::optional<LayoutRect> localVisiblityRect;
    if (options.visibilityCheckRect)
        localVisiblityRect = getLocalExposeRect(*options.visibilityCheckRect, box, verticalScrollbarWidth(), layerBounds);

    auto revealRect = getRectToExposeForScrollIntoView(layerBounds, localExposeRect, options.alignX, options.alignY, localVisiblityRect);
    auto scrollPositionOptions = ScrollPositionChangeOptions::createProgrammatic();
    if (!box->frame().eventHandler().autoscrollInProgress() && box->element() && useSmoothScrolling(options.behavior, box->element()))
        scrollPositionOptions.animated = ScrollIsAnimated::Yes;
    if (auto result = updateScrollPositionForScrollIntoView(scrollPositionOptions, revealRect, localExposeRect))
        return result.value();
    return absoluteRect;
}
std::optional<LayoutRect> RenderLayerScrollableArea::updateScrollPositionForScrollIntoView(const ScrollPositionChangeOptions& options, const LayoutRect& revealRect, const LayoutRect& localExposeRect)
{
    RenderBox* box = m_layer.renderBox();
    ASSERT(box);

    ScrollOffset clampedScrollOffset = clampScrollOffset(scrollOffset() + toIntSize(roundedIntRect(revealRect).location()));
    if (clampedScrollOffset == scrollOffset() && scrollAnimationStatus() == ScrollAnimationStatus::NotAnimating)
        return std::nullopt;

    ScrollOffset oldScrollOffset = scrollOffset();
    ScrollOffset realScrollOffset = scrollToOffset(clampedScrollOffset, options);

    IntSize scrollOffsetDifference = realScrollOffset - oldScrollOffset;
    auto localExposeRectScrolled = localExposeRect;
    localExposeRectScrolled.move(-scrollOffsetDifference);
    return LayoutRect(box->localToAbsoluteQuad(FloatQuad(FloatRect(localExposeRectScrolled)), UseTransforms).boundingBox());
}

void RenderLayerScrollableArea::scrollByRecursively(const IntSize& delta, ScrollableArea** scrolledArea)
{
    if (delta.isZero())
        return;

    auto& renderer = m_layer.renderer();
    bool restrictedByLineClamp = false;
    if (renderer.parent())
        restrictedByLineClamp = !renderer.parent()->style().lineClamp().isNone();

    if (renderer.hasNonVisibleOverflow() && !restrictedByLineClamp) {
        ScrollOffset newScrollOffset = scrollOffset() + delta;
        scrollToOffset(newScrollOffset);
        if (scrolledArea)
            *scrolledArea = this;

        // If this layer can't do the scroll we ask the next layer up that can scroll to try
        IntSize remainingScrollOffset = newScrollOffset - scrollOffset();
        if (!remainingScrollOffset.isZero() && renderer.parent()) {
            // FIXME: This skips scrollable frames.
            if (auto* enclosingScrollableLayer = m_layer.enclosingScrollableLayer(IncludeSelfOrNot::ExcludeSelf, CrossFrameBoundaries::Yes)) {
                if (auto* scrollableArea = enclosingScrollableLayer->scrollableArea())
                    scrollableArea->scrollByRecursively(remainingScrollOffset, scrolledArea);
            }

            renderer.frame().eventHandler().updateAutoscrollRenderer();
        }
    } else {
        // If we are here, we were called on a renderer that can be programmatically scrolled, but doesn't
        // have an overflow clip. Which means that it is a document node that can be scrolled.
        renderer.view().frameView().scrollBy(delta);
        if (scrolledArea)
            *scrolledArea = &renderer.view().frameView();

        // FIXME: If we didn't scroll the whole way, do we want to try looking at the frames ownerElement?
        // https://bugs.webkit.org/show_bug.cgi?id=28237
    }
}

bool RenderLayerScrollableArea::mockScrollbarsControllerEnabled() const
{
    return m_layer.renderer().settings().mockScrollbarsControllerEnabled();
}

void RenderLayerScrollableArea::logMockScrollbarsControllerMessage(const String& message) const
{
    m_layer.renderer().document().addConsoleMessage(MessageSource::Other, MessageLevel::Debug, "RenderLayer: " + message);
}

String RenderLayerScrollableArea::debugDescription() const
{
    return m_layer.debugDescription();
}

void RenderLayerScrollableArea::didStartScrollAnimation()
{
    m_layer.page().scheduleRenderingUpdate({ RenderingUpdateStep::Scroll });
}

} // namespace WebCore
