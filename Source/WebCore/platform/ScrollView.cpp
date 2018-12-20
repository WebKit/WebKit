/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ScrollView.h"

#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HostWindow.h"
#include "Logging.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollAnimator.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollView::ScrollView() = default;

ScrollView::~ScrollView() = default;

void ScrollView::addChild(Widget& child)
{
    ASSERT(&child != this);
    ASSERT(!child.parent());
    child.setParent(this);
    m_children.add(child);
    if (child.platformWidget())
        platformAddChild(&child);
}

void ScrollView::removeChild(Widget& child)
{
    ASSERT(child.parent() == this);
    child.setParent(nullptr);
    m_children.remove(&child);
    if (child.platformWidget())
        platformRemoveChild(&child);
}

bool ScrollView::setHasHorizontalScrollbar(bool hasBar, bool* contentSizeAffected)
{
    return setHasScrollbarInternal(m_horizontalScrollbar, HorizontalScrollbar, hasBar, contentSizeAffected);
}

bool ScrollView::setHasVerticalScrollbar(bool hasBar, bool* contentSizeAffected)
{
    return setHasScrollbarInternal(m_verticalScrollbar, VerticalScrollbar, hasBar, contentSizeAffected);
}

bool ScrollView::setHasScrollbarInternal(RefPtr<Scrollbar>& scrollbar, ScrollbarOrientation orientation, bool hasBar, bool* contentSizeAffected)
{
    ASSERT(!hasBar || !avoidScrollbarCreation());

    if (hasBar && !scrollbar) {
        scrollbar = createScrollbar(orientation);
        addChild(*scrollbar);
        didAddScrollbar(scrollbar.get(), orientation);
        scrollbar->styleChanged();
        if (contentSizeAffected)
            *contentSizeAffected = !scrollbar->isOverlayScrollbar();
        return true;
    }
    
    if (!hasBar && scrollbar) {
        bool wasOverlayScrollbar = scrollbar->isOverlayScrollbar();
        willRemoveScrollbar(scrollbar.get(), orientation);
        removeChild(*scrollbar);
        scrollbar = nullptr;
        if (contentSizeAffected)
            *contentSizeAffected = !wasOverlayScrollbar;
        return true;
    }

    return false;
}

Ref<Scrollbar> ScrollView::createScrollbar(ScrollbarOrientation orientation)
{
    return Scrollbar::createNativeScrollbar(*this, orientation, RegularScrollbar);
}

void ScrollView::setScrollbarModes(ScrollbarMode horizontalMode, ScrollbarMode verticalMode,
                                   bool horizontalLock, bool verticalLock)
{
    bool needsUpdate = false;

    if (horizontalMode != horizontalScrollbarMode() && !m_horizontalScrollbarLock) {
        m_horizontalScrollbarMode = horizontalMode;
        needsUpdate = true;
    }

    if (verticalMode != verticalScrollbarMode() && !m_verticalScrollbarLock) {
        m_verticalScrollbarMode = verticalMode;
        needsUpdate = true;
    }

    if (horizontalLock)
        setHorizontalScrollbarLock();

    if (verticalLock)
        setVerticalScrollbarLock();

    if (!needsUpdate)
        return;

    if (platformWidget())
        platformSetScrollbarModes();
    else
        updateScrollbars(scrollPosition());
}

void ScrollView::scrollbarModes(ScrollbarMode& horizontalMode, ScrollbarMode& verticalMode) const
{
    if (platformWidget()) {
        platformScrollbarModes(horizontalMode, verticalMode);
        return;
    }
    horizontalMode = m_horizontalScrollbarMode;
    verticalMode = m_verticalScrollbarMode;
}

void ScrollView::setCanHaveScrollbars(bool canScroll)
{
    ScrollbarMode newHorizontalMode;
    ScrollbarMode newVerticalMode;
    
    scrollbarModes(newHorizontalMode, newVerticalMode);
    
    if (canScroll && newVerticalMode == ScrollbarAlwaysOff)
        newVerticalMode = ScrollbarAuto;
    else if (!canScroll)
        newVerticalMode = ScrollbarAlwaysOff;
    
    if (canScroll && newHorizontalMode == ScrollbarAlwaysOff)
        newHorizontalMode = ScrollbarAuto;
    else if (!canScroll)
        newHorizontalMode = ScrollbarAlwaysOff;
    
    setScrollbarModes(newHorizontalMode, newVerticalMode);
}

void ScrollView::setCanBlitOnScroll(bool b)
{
    if (platformWidget()) {
        platformSetCanBlitOnScroll(b);
        return;
    }

    m_canBlitOnScroll = b;
}

bool ScrollView::canBlitOnScroll() const
{
    if (platformWidget())
        return platformCanBlitOnScroll();

    return m_canBlitOnScroll;
}

void ScrollView::setPaintsEntireContents(bool paintsEntireContents)
{
    m_paintsEntireContents = paintsEntireContents;
}

void ScrollView::setDelegatesScrolling(bool delegatesScrolling)
{
    if (m_delegatesScrolling == delegatesScrolling)
        return;

    m_delegatesScrolling = delegatesScrolling;
    delegatesScrollingDidChange();
}

IntPoint ScrollView::contentsScrollPosition() const
{
#if PLATFORM(IOS_FAMILY)
    if (platformWidget())
        return actualScrollPosition();
#endif
    return scrollPosition();
}

void ScrollView::setContentsScrollPosition(const IntPoint& position)
{
#if PLATFORM(IOS_FAMILY)
    if (platformWidget())
        setActualScrollPosition(position);
#endif
    setScrollPosition(position);
}

#if !PLATFORM(IOS_FAMILY)
IntRect ScrollView::unobscuredContentRect(VisibleContentRectIncludesScrollbars scrollbarInclusion) const
{
    return unobscuredContentRectInternal(scrollbarInclusion);
}
#endif

IntRect ScrollView::unobscuredContentRectInternal(VisibleContentRectIncludesScrollbars scrollbarInclusion) const
{
    FloatSize visibleContentSize = sizeForUnobscuredContent(scrollbarInclusion);
    visibleContentSize.scale(1 / visibleContentScaleFactor());
    return IntRect(m_scrollPosition, expandedIntSize(visibleContentSize));
}

IntSize ScrollView::sizeForVisibleContent(VisibleContentRectIncludesScrollbars scrollbarInclusion) const
{
    if (platformWidget())
        return platformVisibleContentSizeIncludingObscuredArea(scrollbarInclusion == IncludeScrollbars);

#if USE(COORDINATED_GRAPHICS)
    if (m_useFixedLayout && !m_fixedVisibleContentRect.isEmpty())
        return m_fixedVisibleContentRect.size();
#endif

    IntSize scrollbarSpace;
    if (scrollbarInclusion == ExcludeScrollbars)
        scrollbarSpace = scrollbarIntrusion();

    return IntSize(width() - scrollbarSpace.width(), height() - scrollbarSpace.height()).expandedTo(IntSize());
}
    
IntSize ScrollView::sizeForUnobscuredContent(VisibleContentRectIncludesScrollbars scrollbarInclusion) const
{
    if (platformWidget())
        return platformVisibleContentSize(scrollbarInclusion == IncludeScrollbars);

    IntSize visibleContentSize = sizeForVisibleContent(scrollbarInclusion);

#if USE(COORDINATED_GRAPHICS)
    if (m_useFixedLayout && !m_fixedVisibleContentRect.isEmpty())
        return visibleContentSize;
#endif

    visibleContentSize.setHeight(visibleContentSize.height() - topContentInset());
    return visibleContentSize;
}

IntRect ScrollView::visibleContentRectInternal(VisibleContentRectIncludesScrollbars scrollbarInclusion, VisibleContentRectBehavior visibleContentRectBehavior) const
{
#if PLATFORM(IOS_FAMILY)
    if (visibleContentRectBehavior == LegacyIOSDocumentViewRect) {
        if (platformWidget())
            return platformVisibleContentRect(scrollbarInclusion == IncludeScrollbars);
    }
    
    if (platformWidget())
        return unobscuredContentRect(scrollbarInclusion);
#else
    UNUSED_PARAM(visibleContentRectBehavior);
#endif

    if (platformWidget())
        return platformVisibleContentRect(scrollbarInclusion == IncludeScrollbars);

#if USE(COORDINATED_GRAPHICS)
    if (m_useFixedLayout && !m_fixedVisibleContentRect.isEmpty())
        return m_fixedVisibleContentRect;
#endif

    return unobscuredContentRect(scrollbarInclusion);
}

IntSize ScrollView::layoutSize() const
{
    return m_fixedLayoutSize.isEmpty() || !m_useFixedLayout ? sizeForUnobscuredContent() : m_fixedLayoutSize;
}

IntSize ScrollView::fixedLayoutSize() const
{
    return m_fixedLayoutSize;
}

void ScrollView::setFixedLayoutSize(const IntSize& newSize)
{
    if (fixedLayoutSize() == newSize)
        return;

    LOG_WITH_STREAM(Layout, stream << "ScrollView " << this << " setFixedLayoutSize " << newSize);
    m_fixedLayoutSize = newSize;
    if (m_useFixedLayout)
        availableContentSizeChanged(AvailableSizeChangeReason::AreaSizeChanged);
}

bool ScrollView::useFixedLayout() const
{
    return m_useFixedLayout;
}

void ScrollView::setUseFixedLayout(bool enable)
{
    if (useFixedLayout() == enable)
        return;
    m_useFixedLayout = enable;
    if (!m_fixedLayoutSize.isEmpty())
        availableContentSizeChanged(AvailableSizeChangeReason::AreaSizeChanged);
}

void ScrollView::availableContentSizeChanged(AvailableSizeChangeReason reason)
{
    ScrollableArea::availableContentSizeChanged(reason);

    if (platformWidget())
        return;

    if (reason != AvailableSizeChangeReason::ScrollbarsChanged)
        updateScrollbars(scrollPosition());
}

IntSize ScrollView::contentsSize() const
{
    return m_contentsSize;
}

void ScrollView::setContentsSize(const IntSize& newSize)
{
    if (contentsSize() == newSize)
        return;
    m_contentsSize = newSize;
    if (platformWidget())
        platformSetContentsSize();
    else
        updateScrollbars(scrollPosition());
    updateOverhangAreas();
}

ScrollPosition ScrollView::maximumScrollPosition() const
{
    ScrollPosition maximumPosition = ScrollableArea::maximumScrollPosition();
    // FIXME: can this be moved into the base class?
    maximumPosition.clampNegativeToZero();
    return maximumPosition;
}

ScrollPosition ScrollView::adjustScrollPositionWithinRange(const ScrollPosition& scrollPoint) const
{
    if (!constrainsScrollingToContentEdge() || m_allowsUnclampedScrollPosition)
        return scrollPoint;

    return scrollPoint.constrainedBetween(minimumScrollPosition(), maximumScrollPosition());
}

ScrollPosition ScrollView::documentScrollPositionRelativeToViewOrigin() const
{
    return scrollPosition() - IntSize(
        shouldPlaceBlockDirectionScrollbarOnLeft() && m_verticalScrollbar ? m_verticalScrollbar->occupiedWidth() : 0,
        headerHeight() + topContentInset(TopContentInsetType::WebCoreOrPlatformContentInset));
}

ScrollPosition ScrollView::documentScrollPositionRelativeToScrollableAreaOrigin() const
{
    return scrollPosition() - IntSize(0, headerHeight());
}

int ScrollView::scrollSize(ScrollbarOrientation orientation) const
{
    // If no scrollbars are present, it does not indicate content is not be scrollable.
    if (!m_horizontalScrollbar && !m_verticalScrollbar && !prohibitsScrolling()) {
        IntSize scrollSize = m_contentsSize - visibleContentRect(LegacyIOSDocumentVisibleRect).size();
        scrollSize.clampNegativeToZero();
        return orientation == HorizontalScrollbar ? scrollSize.width() : scrollSize.height();
    }

    Scrollbar* scrollbar = ((orientation == HorizontalScrollbar) ? m_horizontalScrollbar : m_verticalScrollbar).get();
    return scrollbar ? (scrollbar->totalSize() - scrollbar->visibleSize()) : 0;
}

void ScrollView::notifyPageThatContentAreaWillPaint() const
{
}

void ScrollView::setScrollOffset(const ScrollOffset& offset)
{
    LOG_WITH_STREAM(Scrolling, stream << "\nScrollView::setScrollOffset " << offset << " constrains " << constrainsScrollingToContentEdge());

    IntPoint constrainedOffset = offset;
    if (constrainsScrollingToContentEdge())
        constrainedOffset = constrainedOffset.constrainedBetween(IntPoint(), maximumScrollOffset());

    scrollTo(scrollPositionFromOffset(constrainedOffset));
}

void ScrollView::scrollOffsetChangedViaPlatformWidget(const ScrollOffset& oldOffset, const ScrollOffset& newOffset)
{
    // We should not attempt to actually modify (paint) platform widgets if the layout phase
    // is not complete. Instead, defer the scroll event until the layout finishes.
    if (shouldDeferScrollUpdateAfterContentSizeChange()) {
        // We only care about the most recent scroll position change request
        m_deferredScrollOffsets = std::make_pair(oldOffset, newOffset);
        return;
    }

    scrollOffsetChangedViaPlatformWidgetImpl(oldOffset, newOffset);
}

void ScrollView::handleDeferredScrollUpdateAfterContentSizeChange()
{
    ASSERT(!shouldDeferScrollUpdateAfterContentSizeChange());

    if (!m_deferredScrollDelta && !m_deferredScrollOffsets)
        return;

    ASSERT(static_cast<bool>(m_deferredScrollDelta) != static_cast<bool>(m_deferredScrollOffsets));

    if (m_deferredScrollDelta)
        completeUpdatesAfterScrollTo(m_deferredScrollDelta.value());
    else if (m_deferredScrollOffsets)
        scrollOffsetChangedViaPlatformWidgetImpl(m_deferredScrollOffsets.value().first, m_deferredScrollOffsets.value().second);
    
    m_deferredScrollDelta = WTF::nullopt;
    m_deferredScrollOffsets = WTF::nullopt;
}

void ScrollView::scrollTo(const ScrollPosition& newPosition)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollView::scrollTo " << newPosition << " min: " << minimumScrollPosition() << " max: " << maximumScrollPosition());

    IntSize scrollDelta = newPosition - m_scrollPosition;
    if (scrollDelta.isZero())
        return;

    m_scrollPosition = newPosition;

    if (scrollbarsSuppressed())
        return;

#if USE(COORDINATED_GRAPHICS)
    if (delegatesScrolling()) {
        requestScrollPositionUpdate(newPosition);
        return;
    }
#endif
    // We should not attempt to actually modify layer contents if the layout phase
    // is not complete. Instead, defer the scroll event until the layout finishes.
    if (shouldDeferScrollUpdateAfterContentSizeChange()) {
        ASSERT(!m_deferredScrollDelta);
        m_deferredScrollDelta = scrollDelta;
        return;
    }

    completeUpdatesAfterScrollTo(scrollDelta);
}

void ScrollView::completeUpdatesAfterScrollTo(const IntSize& scrollDelta)
{
    updateLayerPositionsAfterScrolling();
    scrollContents(scrollDelta);
    updateCompositingLayersAfterScrolling();
}

int ScrollView::scrollOffset(ScrollbarOrientation orientation) const
{
    ScrollOffset offset = scrollOffsetFromPosition(scrollPosition());

    if (orientation == HorizontalScrollbar)
        return offset.x();

    if (orientation == VerticalScrollbar)
        return offset.y();

    return 0;
}

void ScrollView::setScrollPosition(const ScrollPosition& scrollPosition)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollView::setScrollPosition " << scrollPosition);

    if (prohibitsScrolling())
        return;

    if (platformWidget()) {
        platformSetScrollPosition(scrollPosition);
        return;
    }

    ScrollPosition newScrollPosition = !delegatesScrolling() ? adjustScrollPositionWithinRange(scrollPosition) : scrollPosition;

    if ((!delegatesScrolling() || !inProgrammaticScroll()) && newScrollPosition == this->scrollPosition())
        return;

    if (requestScrollPositionUpdate(newScrollPosition))
        return;

    updateScrollbars(newScrollPosition);
}

bool ScrollView::scroll(ScrollDirection direction, ScrollGranularity granularity)
{
    if (platformWidget())
        return platformScroll(direction, granularity);

    return ScrollableArea::scroll(direction, granularity);
}

bool ScrollView::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity)
{
    return scroll(logicalToPhysical(direction, isVerticalDocument(), isFlippedDocument()), granularity);
}

IntSize ScrollView::overhangAmount() const
{
    IntSize stretch;

    // FIXME: use maximumScrollOffset()
    ScrollOffset scrollOffset = scrollOffsetFromPosition(scrollPosition());
    if (scrollOffset.y() < 0)
        stretch.setHeight(scrollOffset.y());
    else if (totalContentsSize().height() && scrollOffset.y() > totalContentsSize().height() - visibleHeight())
        stretch.setHeight(scrollOffset.y() - (totalContentsSize().height() - visibleHeight()));

    if (scrollOffset.x() < 0)
        stretch.setWidth(scrollOffset.x());
    else if (contentsWidth() && scrollOffset.x() > contentsWidth() - visibleWidth())
        stretch.setWidth(scrollOffset.x() - (contentsWidth() - visibleWidth()));

    return stretch;
}

void ScrollView::updateScrollbars(const ScrollPosition& desiredPosition)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollView::updateScrollbars " << desiredPosition);

    if (m_inUpdateScrollbars || prohibitsScrolling() || platformWidget())
        return;
    
    if (delegatesScrolling()) {
        if (scrollOriginChanged()) {
            ScrollableArea::scrollToOffsetWithoutAnimation(scrollOffsetFromPosition(desiredPosition));
            resetScrollOriginChanged();
        }
        return;
    }

    bool hasOverlayScrollbars = (!m_horizontalScrollbar || m_horizontalScrollbar->isOverlayScrollbar()) && (!m_verticalScrollbar || m_verticalScrollbar->isOverlayScrollbar());

    // If we came in here with the view already needing a layout then do that first.
    // (This will be the common case, e.g., when the page changes due to window resizing for example).
    // This layout will not re-enter updateScrollbars and does not count towards our max layout pass total.
    if (!m_scrollbarsSuppressed && !hasOverlayScrollbars) {
        m_inUpdateScrollbars = true;
        updateContentsSize();
        m_inUpdateScrollbars = false;
    }

    IntRect oldScrollCornerRect = scrollCornerRect();

    bool hasHorizontalScrollbar = m_horizontalScrollbar;
    bool hasVerticalScrollbar = m_verticalScrollbar;
    
    bool newHasHorizontalScrollbar = hasHorizontalScrollbar;
    bool newHasVerticalScrollbar = hasVerticalScrollbar;
   
    ScrollbarMode hScroll = m_horizontalScrollbarMode;
    ScrollbarMode vScroll = m_verticalScrollbarMode;

    if (hScroll != ScrollbarAuto)
        newHasHorizontalScrollbar = (hScroll == ScrollbarAlwaysOn);
    if (vScroll != ScrollbarAuto)
        newHasVerticalScrollbar = (vScroll == ScrollbarAlwaysOn);

    bool scrollbarAddedOrRemoved = false;

    if (m_scrollbarsSuppressed || (hScroll != ScrollbarAuto && vScroll != ScrollbarAuto)) {
        if (hasHorizontalScrollbar != newHasHorizontalScrollbar && (hasHorizontalScrollbar || !avoidScrollbarCreation())) {
            if (setHasHorizontalScrollbar(newHasHorizontalScrollbar))
                scrollbarAddedOrRemoved = true;
        }

        if (hasVerticalScrollbar != newHasVerticalScrollbar && (hasVerticalScrollbar || !avoidScrollbarCreation())) {
            if (setHasVerticalScrollbar(newHasVerticalScrollbar))
                scrollbarAddedOrRemoved = true;
        }
    } else {
        bool sendContentResizedNotification = false;
        
        IntSize docSize = totalContentsSize();
        IntSize fullVisibleSize = unobscuredContentRectIncludingScrollbars().size();

        if (hScroll == ScrollbarAuto)
            newHasHorizontalScrollbar = docSize.width() > visibleWidth();
        if (vScroll == ScrollbarAuto)
            newHasVerticalScrollbar = docSize.height() > visibleHeight();

        bool needAnotherPass = false;
        if (!hasOverlayScrollbars) {
            // If we ever turn one scrollbar off, do not turn the other one on. Never ever
            // try to both gain/lose a scrollbar in the same pass.
            if (!m_updateScrollbarsPass && docSize.width() <= fullVisibleSize.width() && docSize.height() <= fullVisibleSize.height()) {
                if (hScroll == ScrollbarAuto)
                    newHasHorizontalScrollbar = false;
                if (vScroll == ScrollbarAuto)
                    newHasVerticalScrollbar = false;
            }
            if (!newHasHorizontalScrollbar && hasHorizontalScrollbar && vScroll != ScrollbarAlwaysOn && !hasVerticalScrollbar) {
                newHasVerticalScrollbar = false;
                needAnotherPass = true;
            }
            if (!newHasVerticalScrollbar && hasVerticalScrollbar && hScroll != ScrollbarAlwaysOn && !hasHorizontalScrollbar) {
                newHasHorizontalScrollbar = false;
                needAnotherPass = true;
            }
        }

        if (hasHorizontalScrollbar != newHasHorizontalScrollbar && (hasHorizontalScrollbar || !avoidScrollbarCreation())) {
            if (scrollOrigin().y() && !newHasHorizontalScrollbar)
                ScrollableArea::setScrollOrigin(IntPoint(scrollOrigin().x(), scrollOrigin().y() - m_horizontalScrollbar->occupiedHeight()));
            if (m_horizontalScrollbar)
                m_horizontalScrollbar->invalidate();

            bool changeAffectsContentSize = false;
            if (setHasHorizontalScrollbar(newHasHorizontalScrollbar, &changeAffectsContentSize)) {
                scrollbarAddedOrRemoved = true;
                sendContentResizedNotification |= changeAffectsContentSize;
            }
        }

        if (hasVerticalScrollbar != newHasVerticalScrollbar && (hasVerticalScrollbar || !avoidScrollbarCreation())) {
            if (scrollOrigin().x() && !newHasVerticalScrollbar)
                ScrollableArea::setScrollOrigin(IntPoint(scrollOrigin().x() - m_verticalScrollbar->occupiedWidth(), scrollOrigin().y()));
            if (m_verticalScrollbar)
                m_verticalScrollbar->invalidate();

            bool changeAffectsContentSize = false;
            if (setHasVerticalScrollbar(newHasVerticalScrollbar, &changeAffectsContentSize)) {
                scrollbarAddedOrRemoved = true;
                sendContentResizedNotification |= changeAffectsContentSize;
            }
        }

        const unsigned cMaxUpdateScrollbarsPass = 2;
        if ((sendContentResizedNotification || needAnotherPass) && m_updateScrollbarsPass < cMaxUpdateScrollbarsPass) {
            m_updateScrollbarsPass++;
            availableContentSizeChanged(AvailableSizeChangeReason::ScrollbarsChanged);
            updateContentsSize();
            IntSize newDocSize = totalContentsSize();
            if (newDocSize == docSize) {
                // The layout with the new scroll state had no impact on
                // the document's overall size, so updateScrollbars didn't get called.
                // Recur manually.
                updateScrollbars(desiredPosition);
            }
            m_updateScrollbarsPass--;
        }
    }

    if (scrollbarAddedOrRemoved)
        addedOrRemovedScrollbar();

    // Set up the range (and page step/line step), but only do this if we're not in a nested call (to avoid
    // doing it multiple times).
    if (m_updateScrollbarsPass)
        return;

    m_inUpdateScrollbars = true;

    if (m_horizontalScrollbar) {
        int clientWidth = visibleWidth();
        int pageStep = Scrollbar::pageStep(clientWidth);
        IntRect oldRect(m_horizontalScrollbar->frameRect());
        IntRect hBarRect(shouldPlaceBlockDirectionScrollbarOnLeft() && m_verticalScrollbar ? m_verticalScrollbar->occupiedWidth() : 0,
            height() - m_horizontalScrollbar->height(),
            width() - (m_verticalScrollbar ? m_verticalScrollbar->occupiedWidth() : 0),
            m_horizontalScrollbar->height());
        m_horizontalScrollbar->setFrameRect(hBarRect);
        if (!m_scrollbarsSuppressed && oldRect != m_horizontalScrollbar->frameRect())
            m_horizontalScrollbar->invalidate();

        if (m_scrollbarsSuppressed)
            m_horizontalScrollbar->setSuppressInvalidation(true);
        m_horizontalScrollbar->setEnabled(contentsWidth() > clientWidth);
        m_horizontalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_horizontalScrollbar->setProportion(clientWidth, contentsWidth());
        if (m_scrollbarsSuppressed)
            m_horizontalScrollbar->setSuppressInvalidation(false); 
    } 

    if (m_verticalScrollbar) {
        int clientHeight = visibleHeight();
        int pageStep = Scrollbar::pageStep(clientHeight);
        IntRect oldRect(m_verticalScrollbar->frameRect());
        IntRect vBarRect(shouldPlaceBlockDirectionScrollbarOnLeft() ? 0 : width() - m_verticalScrollbar->width(),
            topContentInset(),
            m_verticalScrollbar->width(),
            height() - topContentInset() - (m_horizontalScrollbar ? m_horizontalScrollbar->occupiedHeight() : 0));
        m_verticalScrollbar->setFrameRect(vBarRect);
        if (!m_scrollbarsSuppressed && oldRect != m_verticalScrollbar->frameRect())
            m_verticalScrollbar->invalidate();

        if (m_scrollbarsSuppressed)
            m_verticalScrollbar->setSuppressInvalidation(true);
        m_verticalScrollbar->setEnabled(totalContentsSize().height() > clientHeight);
        m_verticalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_verticalScrollbar->setProportion(clientHeight, totalContentsSize().height());
        if (m_scrollbarsSuppressed)
            m_verticalScrollbar->setSuppressInvalidation(false);
    }

    if (hasHorizontalScrollbar != newHasHorizontalScrollbar || hasVerticalScrollbar != newHasVerticalScrollbar) {
        // FIXME: Is frameRectsChanged really necessary here? Have any frame rects changed?
        frameRectsChanged();
        positionScrollbarLayers();
        updateScrollCorner();
        if (!m_horizontalScrollbar && !m_verticalScrollbar)
            invalidateScrollCornerRect(oldScrollCornerRect);
    }

    IntPoint adjustedScrollPosition = desiredPosition;
    if (!isRubberBandInProgress())
        adjustedScrollPosition = adjustScrollPositionWithinRange(adjustedScrollPosition);

    if (adjustedScrollPosition != scrollPosition() || scrollOriginChanged()) {
        ScrollableArea::scrollToOffsetWithoutAnimation(scrollOffsetFromPosition(adjustedScrollPosition));
        resetScrollOriginChanged();
    }

    // Make sure the scrollbar offsets are up to date.
    if (m_horizontalScrollbar)
        m_horizontalScrollbar->offsetDidChange();
    if (m_verticalScrollbar)
        m_verticalScrollbar->offsetDidChange();

    m_inUpdateScrollbars = false;
}

const int panIconSizeLength = 16;

IntRect ScrollView::rectToCopyOnScroll() const
{
    IntRect scrollViewRect = convertToRootView(IntRect(0, 0, visibleWidth(), visibleHeight()));
    if (hasOverlayScrollbars()) {
        int verticalScrollbarWidth = (verticalScrollbar() && !hasLayerForVerticalScrollbar()) ? verticalScrollbar()->width() : 0;
        int horizontalScrollbarHeight = (horizontalScrollbar() && !hasLayerForHorizontalScrollbar()) ? horizontalScrollbar()->height() : 0;
        
        scrollViewRect.setWidth(scrollViewRect.width() - verticalScrollbarWidth);
        scrollViewRect.setHeight(scrollViewRect.height() - horizontalScrollbarHeight);
    }
    return scrollViewRect;
}

void ScrollView::scrollContents(const IntSize& scrollDelta)
{
    HostWindow* window = hostWindow();
    if (!window)
        return;

    // Since scrolling is double buffered, we will be blitting the scroll view's intersection
    // with the clip rect every time to keep it smooth.
    IntRect clipRect = windowClipRect();
    IntRect scrollViewRect = rectToCopyOnScroll();    
    IntRect updateRect = clipRect;
    updateRect.intersect(scrollViewRect);

    // Invalidate the root view (not the backing store).
    window->invalidateRootView(updateRect);

    if (m_drawPanScrollIcon) {
        // FIXME: the pan icon is broken when accelerated compositing is on, since it will draw under the compositing layers.
        // https://bugs.webkit.org/show_bug.cgi?id=47837
        int panIconDirtySquareSizeLength = 2 * (panIconSizeLength + std::max(abs(scrollDelta.width()), abs(scrollDelta.height()))); // We only want to repaint what's necessary
        IntPoint panIconDirtySquareLocation = IntPoint(m_panScrollIconPoint.x() - (panIconDirtySquareSizeLength / 2), m_panScrollIconPoint.y() - (panIconDirtySquareSizeLength / 2));
        IntRect panScrollIconDirtyRect = IntRect(panIconDirtySquareLocation, IntSize(panIconDirtySquareSizeLength, panIconDirtySquareSizeLength));
        panScrollIconDirtyRect.intersect(clipRect);
        window->invalidateContentsAndRootView(panScrollIconDirtyRect);
    }

    if (canBlitOnScroll()) { // The main frame can just blit the WebView window
        // FIXME: Find a way to scroll subframes with this faster path
        if (!scrollContentsFastPath(-scrollDelta, scrollViewRect, clipRect))
            scrollContentsSlowPath(updateRect);
    } else { 
        // We need to repaint the entire backing store. Do it now before moving the windowed plugins.
        scrollContentsSlowPath(updateRect);
    }

    // Invalidate the overhang areas if they are visible.
    updateOverhangAreas();

    // This call will move children with native widgets (plugins) and invalidate them as well.
    frameRectsChanged();

    // Now blit the backingstore into the window which should be very fast.
    window->invalidateRootView(IntRect());
}

void ScrollView::scrollContentsSlowPath(const IntRect& updateRect)
{
    hostWindow()->invalidateContentsForSlowScroll(updateRect);
}

IntPoint ScrollView::viewToContents(const IntPoint& point) const
{
    return point + toIntSize(documentScrollPositionRelativeToViewOrigin());
}

IntPoint ScrollView::contentsToView(const IntPoint& point) const
{
    return point - toIntSize(documentScrollPositionRelativeToViewOrigin());
}

IntRect ScrollView::viewToContents(IntRect rect) const
{
    rect.moveBy(documentScrollPositionRelativeToViewOrigin());
    return rect;
}

FloatRect ScrollView::viewToContents(FloatRect rect) const
{
    rect.moveBy(documentScrollPositionRelativeToViewOrigin());
    return rect;
}

IntRect ScrollView::contentsToView(IntRect rect) const
{
    rect.moveBy(-documentScrollPositionRelativeToViewOrigin());
    return rect;
}

FloatRect ScrollView::contentsToView(FloatRect rect) const
{
    rect.moveBy(-documentScrollPositionRelativeToViewOrigin());
    return rect;
}

IntPoint ScrollView::contentsToContainingViewContents(const IntPoint& point) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntPoint pointInContainingView = convertToContainingView(contentsToView(point));
        return parentScrollView->viewToContents(pointInContainingView);
    }

    return contentsToView(point);
}

IntRect ScrollView::contentsToContainingViewContents(IntRect rect) const
{
    if (delegatesScrolling())
        return convertToContainingView(contentsToView(rect));
    
    if (const ScrollView* parentScrollView = parent()) {
        IntRect rectInContainingView = convertToContainingView(contentsToView(rect));
        return parentScrollView->viewToContents(rectInContainingView);
    }

    return contentsToView(rect);
}

IntPoint ScrollView::rootViewToContents(const IntPoint& rootViewPoint) const
{
    if (delegatesScrolling())
        return convertFromRootView(rootViewPoint);

    return viewToContents(convertFromRootView(rootViewPoint));
}

IntPoint ScrollView::contentsToRootView(const IntPoint& contentsPoint) const
{
    if (delegatesScrolling())
        return convertToRootView(contentsPoint);

    return convertToRootView(contentsToView(contentsPoint));
}

IntRect ScrollView::rootViewToContents(const IntRect& rootViewRect) const
{
    if (delegatesScrolling())
        return convertFromRootView(rootViewRect);

    return viewToContents(convertFromRootView(rootViewRect));
}

FloatRect ScrollView::rootViewToContents(const FloatRect& rootViewRect) const
{
    if (delegatesScrolling())
        return convertFromRootView(rootViewRect);

    return viewToContents(convertFromRootView(rootViewRect));
}

IntPoint ScrollView::rootViewToTotalContents(const IntPoint& rootViewPoint) const
{
    if (delegatesScrolling())
        return convertFromRootView(rootViewPoint);

    IntPoint viewPoint = convertFromRootView(rootViewPoint);
    // Like rootViewToContents(), but ignores headerHeight.
    return viewPoint + toIntSize(scrollPosition()) - IntSize(0, topContentInset(TopContentInsetType::WebCoreOrPlatformContentInset));
}

IntRect ScrollView::contentsToRootView(const IntRect& contentsRect) const
{
    if (delegatesScrolling())
        return convertToRootView(contentsRect);

    return convertToRootView(contentsToView(contentsRect));
}

IntPoint ScrollView::windowToContents(const IntPoint& windowPoint) const
{
    if (delegatesScrolling())
        return convertFromContainingWindow(windowPoint);

    return viewToContents(convertFromContainingWindow(windowPoint));
}

IntPoint ScrollView::contentsToWindow(const IntPoint& contentsPoint) const
{
    if (delegatesScrolling())
        return convertToContainingWindow(contentsPoint);

    return convertToContainingWindow(contentsToView(contentsPoint));
}

IntRect ScrollView::windowToContents(const IntRect& windowRect) const
{
    if (delegatesScrolling())
        return convertFromContainingWindow(windowRect);

    return viewToContents(convertFromContainingWindow(windowRect));
}

IntRect ScrollView::contentsToWindow(const IntRect& contentsRect) const
{
    if (delegatesScrolling())
        return convertToContainingWindow(contentsRect);

    return convertToContainingWindow(contentsToView(contentsRect));
}

IntRect ScrollView::contentsToScreen(const IntRect& rect) const
{
    HostWindow* window = hostWindow();
    if (platformWidget())
        return platformContentsToScreen(rect);
    if (!window)
        return IntRect();
    return window->rootViewToScreen(contentsToRootView(rect));
}

IntPoint ScrollView::screenToContents(const IntPoint& point) const
{
    HostWindow* window = hostWindow();
    if (platformWidget())
        return platformScreenToContents(point);
    if (!window)
        return IntPoint();
    return rootViewToContents(window->screenToRootView(point));
}

void ScrollView::setScrollbarsSuppressed(bool suppressed, bool repaintOnUnsuppress)
{
    if (suppressed == m_scrollbarsSuppressed)
        return;

    m_scrollbarsSuppressed = suppressed;

    if (platformWidget())
        platformSetScrollbarsSuppressed(repaintOnUnsuppress);
    else if (repaintOnUnsuppress && !suppressed) {
        if (m_horizontalScrollbar)
            m_horizontalScrollbar->invalidate();
        if (m_verticalScrollbar)
            m_verticalScrollbar->invalidate();

        // Invalidate the scroll corner too on unsuppress.
        invalidateRect(scrollCornerRect());
    }
}

Scrollbar* ScrollView::scrollbarAtPoint(const IntPoint& windowPoint)
{
    if (platformWidget())
        return 0;

    // convertFromContainingWindow doesn't do what it sounds like it does. We need it here just to get this
    // point into the right coordinates if this is the ScrollView of a sub-frame.
    IntPoint convertedPoint = convertFromContainingWindow(windowPoint);
    if (m_horizontalScrollbar && m_horizontalScrollbar->shouldParticipateInHitTesting() && m_horizontalScrollbar->frameRect().contains(convertedPoint))
        return m_horizontalScrollbar.get();
    if (m_verticalScrollbar && m_verticalScrollbar->shouldParticipateInHitTesting() && m_verticalScrollbar->frameRect().contains(convertedPoint))
        return m_verticalScrollbar.get();
    return 0;
}

void ScrollView::setScrollbarOverlayStyle(ScrollbarOverlayStyle overlayStyle)
{
    ScrollableArea::setScrollbarOverlayStyle(overlayStyle);
    platformSetScrollbarOverlayStyle(overlayStyle);
}

void ScrollView::setFrameRect(const IntRect& newRect)
{
    Ref<ScrollView> protectedThis(*this);
    IntRect oldRect = frameRect();
    
    if (newRect == oldRect)
        return;

    Widget::setFrameRect(newRect);
    frameRectsChanged();

    updateScrollbars(scrollPosition());
    
    if (!m_useFixedLayout && oldRect.size() != newRect.size())
        availableContentSizeChanged(AvailableSizeChangeReason::AreaSizeChanged);
}

void ScrollView::frameRectsChanged()
{
    if (platformWidget())
        return;
    for (auto& child : m_children)
        child->frameRectsChanged();
}

void ScrollView::clipRectChanged()
{
    for (auto& child : m_children)
        child->clipRectChanged();
}

static void positionScrollbarLayer(GraphicsLayer* graphicsLayer, Scrollbar* scrollbar)
{
    if (!graphicsLayer || !scrollbar)
        return;

    IntRect scrollbarRect = scrollbar->frameRect();
    graphicsLayer->setPosition(scrollbarRect.location());

    if (scrollbarRect.size() == graphicsLayer->size())
        return;

    graphicsLayer->setSize(scrollbarRect.size());

    if (graphicsLayer->usesContentsLayer()) {
        graphicsLayer->setContentsRect(IntRect(0, 0, scrollbarRect.width(), scrollbarRect.height()));
        return;
    }

    graphicsLayer->setDrawsContent(true);
    graphicsLayer->setNeedsDisplay();
}

static void positionScrollCornerLayer(GraphicsLayer* graphicsLayer, const IntRect& cornerRect)
{
    if (!graphicsLayer)
        return;
    graphicsLayer->setDrawsContent(!cornerRect.isEmpty());
    graphicsLayer->setPosition(cornerRect.location());
    if (cornerRect.size() != graphicsLayer->size())
        graphicsLayer->setNeedsDisplay();
    graphicsLayer->setSize(cornerRect.size());
}

void ScrollView::positionScrollbarLayers()
{
    positionScrollbarLayer(layerForHorizontalScrollbar(), horizontalScrollbar());
    positionScrollbarLayer(layerForVerticalScrollbar(), verticalScrollbar());
    positionScrollCornerLayer(layerForScrollCorner(), scrollCornerRect());
}

void ScrollView::repaintContentRectangle(const IntRect& rect)
{
    IntRect paintRect = rect;
    if (!paintsEntireContents())
        paintRect.intersect(visibleContentRect(LegacyIOSDocumentVisibleRect));
    if (paintRect.isEmpty())
        return;

    if (platformWidget()) {
        notifyPageThatContentAreaWillPaint();
        platformRepaintContentRectangle(paintRect);
        return;
    }

    if (HostWindow* window = hostWindow())
        window->invalidateContentsAndRootView(contentsToWindow(paintRect));
}

IntRect ScrollView::scrollCornerRect() const
{
    IntRect cornerRect;

    if (hasOverlayScrollbars())
        return cornerRect;

    int heightTrackedByScrollbar = height() - topContentInset();

    if (m_horizontalScrollbar && width() - m_horizontalScrollbar->width() > 0) {
        cornerRect.unite(IntRect(shouldPlaceBlockDirectionScrollbarOnLeft() ? 0 : m_horizontalScrollbar->width(),
            height() - m_horizontalScrollbar->height(),
            width() - m_horizontalScrollbar->width(),
            m_horizontalScrollbar->height()));
    }

    if (m_verticalScrollbar && heightTrackedByScrollbar - m_verticalScrollbar->height() > 0) {
        cornerRect.unite(IntRect(shouldPlaceBlockDirectionScrollbarOnLeft() ? 0 : width() - m_verticalScrollbar->width(),
            m_verticalScrollbar->height() + topContentInset(),
            m_verticalScrollbar->width(),
            heightTrackedByScrollbar - m_verticalScrollbar->height()));
    }

    return cornerRect;
}

bool ScrollView::isScrollCornerVisible() const
{
    return !scrollCornerRect().isEmpty();
}

void ScrollView::scrollbarStyleChanged(ScrollbarStyle newStyle, bool forceUpdate)
{
    ScrollableArea::scrollbarStyleChanged(newStyle, forceUpdate);
    if (!forceUpdate)
        return;

    updateScrollbars(scrollPosition());
    positionScrollbarLayers();
}

void ScrollView::paintScrollCorner(GraphicsContext& context, const IntRect& cornerRect)
{
    ScrollbarTheme::theme().paintScrollCorner(context, cornerRect);
}

void ScrollView::paintScrollbar(GraphicsContext& context, Scrollbar& bar, const IntRect& rect)
{
    bar.paint(context, rect);
}

void ScrollView::invalidateScrollCornerRect(const IntRect& rect)
{
    invalidateRect(rect);
}

void ScrollView::paintScrollbars(GraphicsContext& context, const IntRect& rect)
{
    if (m_horizontalScrollbar && !layerForHorizontalScrollbar())
        paintScrollbar(context, *m_horizontalScrollbar.get(), rect);
    if (m_verticalScrollbar && !layerForVerticalScrollbar())
        paintScrollbar(context, *m_verticalScrollbar.get(), rect);

    if (layerForScrollCorner())
        return;

    paintScrollCorner(context, scrollCornerRect());
}

void ScrollView::paintPanScrollIcon(GraphicsContext& context)
{
    static Image& panScrollIcon = Image::loadPlatformResource("panIcon").leakRef();
    IntPoint iconGCPoint = m_panScrollIconPoint;
    if (parent())
        iconGCPoint = parent()->windowToContents(iconGCPoint);
    context.drawImage(panScrollIcon, iconGCPoint);
}

void ScrollView::paint(GraphicsContext& context, const IntRect& rect, SecurityOriginPaintPolicy securityOriginPaintPolicy)
{
    if (platformWidget()) {
        Widget::paint(context, rect);
        return;
    }

    if (context.paintingDisabled() && !context.performingPaintInvalidation())
        return;

    notifyPageThatContentAreaWillPaint();

    IntRect documentDirtyRect = rect;
    if (!paintsEntireContents()) {
        IntRect visibleAreaWithoutScrollbars(locationOfContents(), visibleContentRect(LegacyIOSDocumentVisibleRect).size());
        documentDirtyRect.intersect(visibleAreaWithoutScrollbars);
    }

    if (!documentDirtyRect.isEmpty()) {
        GraphicsContextStateSaver stateSaver(context);

        IntPoint locationOfContents = this->locationOfContents();
        context.translate(locationOfContents.x(), locationOfContents.y());
        documentDirtyRect.moveBy(-locationOfContents);

        if (!paintsEntireContents()) {
            context.translate(-scrollX(), -scrollY());
            documentDirtyRect.moveBy(scrollPosition());

            context.clip(visibleContentRect(LegacyIOSDocumentVisibleRect));
        }

        paintContents(context, documentDirtyRect, securityOriginPaintPolicy);
    }

#if ENABLE(RUBBER_BANDING)
    if (!layerForOverhangAreas())
        calculateAndPaintOverhangAreas(context, rect);
#else
    calculateAndPaintOverhangAreas(context, rect);
#endif

    // Now paint the scrollbars.
    if (!m_scrollbarsSuppressed && (m_horizontalScrollbar || m_verticalScrollbar)) {
        GraphicsContextStateSaver stateSaver(context);
        IntRect scrollViewDirtyRect = rect;
        IntRect visibleAreaWithScrollbars(location(), unobscuredContentRectIncludingScrollbars().size());
        scrollViewDirtyRect.intersect(visibleAreaWithScrollbars);
        context.translate(x(), y());
        scrollViewDirtyRect.moveBy(-location());
        context.clip(IntRect(IntPoint(), visibleAreaWithScrollbars.size()));

        paintScrollbars(context, scrollViewDirtyRect);
    }

    // Paint the panScroll Icon
    if (m_drawPanScrollIcon)
        paintPanScrollIcon(context);
}

void ScrollView::calculateOverhangAreasForPainting(IntRect& horizontalOverhangRect, IntRect& verticalOverhangRect)
{
    IntSize scrollbarSpace = scrollbarIntrusion();

    // FIXME: use maximumScrollOffset().
    ScrollOffset scrollOffset = scrollOffsetFromPosition(scrollPosition());
    if (scrollOffset.y() < 0) {
        horizontalOverhangRect = frameRect();
        horizontalOverhangRect.setHeight(-scrollOffset.y());
        horizontalOverhangRect.setWidth(horizontalOverhangRect.width() - scrollbarSpace.width());
    } else if (totalContentsSize().height() && scrollOffset.y() > totalContentsSize().height() - visibleHeight()) {
        int height = scrollOffset.y() - (totalContentsSize().height() - visibleHeight());
        horizontalOverhangRect = frameRect();
        horizontalOverhangRect.setY(frameRect().maxY() - height - scrollbarSpace.height());
        horizontalOverhangRect.setHeight(height);
        horizontalOverhangRect.setWidth(horizontalOverhangRect.width() - scrollbarSpace.width());
    }

    if (scrollOffset.x() < 0) {
        verticalOverhangRect.setWidth(-scrollOffset.x());
        verticalOverhangRect.setHeight(frameRect().height() - horizontalOverhangRect.height() - scrollbarSpace.height());
        verticalOverhangRect.setX(frameRect().x());
        if (horizontalOverhangRect.y() == frameRect().y())
            verticalOverhangRect.setY(frameRect().y() + horizontalOverhangRect.height());
        else
            verticalOverhangRect.setY(frameRect().y());
    } else if (contentsWidth() && scrollOffset.x() > contentsWidth() - visibleWidth()) {
        int width = scrollOffset.x() - (contentsWidth() - visibleWidth());
        verticalOverhangRect.setWidth(width);
        verticalOverhangRect.setHeight(frameRect().height() - horizontalOverhangRect.height() - scrollbarSpace.height());
        verticalOverhangRect.setX(frameRect().maxX() - width - scrollbarSpace.width());
        if (horizontalOverhangRect.y() == frameRect().y())
            verticalOverhangRect.setY(frameRect().y() + horizontalOverhangRect.height());
        else
            verticalOverhangRect.setY(frameRect().y());
    }
}

void ScrollView::updateOverhangAreas()
{
    HostWindow* window = hostWindow();
    if (!window)
        return;

    IntRect horizontalOverhangRect;
    IntRect verticalOverhangRect;
    calculateOverhangAreasForPainting(horizontalOverhangRect, verticalOverhangRect);
    if (!horizontalOverhangRect.isEmpty())
        window->invalidateContentsAndRootView(horizontalOverhangRect);
    if (!verticalOverhangRect.isEmpty())
        window->invalidateContentsAndRootView(verticalOverhangRect);
}

void ScrollView::paintOverhangAreas(GraphicsContext& context, const IntRect& horizontalOverhangRect, const IntRect& verticalOverhangRect, const IntRect& dirtyRect)
{
    ScrollbarTheme::theme().paintOverhangAreas(*this, context, horizontalOverhangRect, verticalOverhangRect, dirtyRect);
}

void ScrollView::calculateAndPaintOverhangAreas(GraphicsContext& context, const IntRect& dirtyRect)
{
    IntRect horizontalOverhangRect;
    IntRect verticalOverhangRect;
    calculateOverhangAreasForPainting(horizontalOverhangRect, verticalOverhangRect);

    if (dirtyRect.intersects(horizontalOverhangRect) || dirtyRect.intersects(verticalOverhangRect))
        paintOverhangAreas(context, horizontalOverhangRect, verticalOverhangRect, dirtyRect);
}

bool ScrollView::isPointInScrollbarCorner(const IntPoint& windowPoint)
{
    if (!scrollbarCornerPresent())
        return false;

    IntPoint viewPoint = convertFromContainingWindow(windowPoint);

    if (m_horizontalScrollbar) {
        int horizontalScrollbarYMin = m_horizontalScrollbar->frameRect().y();
        int horizontalScrollbarYMax = m_horizontalScrollbar->frameRect().y() + m_horizontalScrollbar->frameRect().height();
        int horizontalScrollbarXMin = m_horizontalScrollbar->frameRect().x() + m_horizontalScrollbar->frameRect().width();

        return viewPoint.y() > horizontalScrollbarYMin && viewPoint.y() < horizontalScrollbarYMax && viewPoint.x() > horizontalScrollbarXMin;
    }

    int verticalScrollbarXMin = m_verticalScrollbar->frameRect().x();
    int verticalScrollbarXMax = m_verticalScrollbar->frameRect().x() + m_verticalScrollbar->frameRect().width();
    int verticalScrollbarYMin = m_verticalScrollbar->frameRect().y() + m_verticalScrollbar->frameRect().height();
    
    return viewPoint.x() > verticalScrollbarXMin && viewPoint.x() < verticalScrollbarXMax && viewPoint.y() > verticalScrollbarYMin;
}

bool ScrollView::scrollbarCornerPresent() const
{
    return (m_horizontalScrollbar && width() - m_horizontalScrollbar->width() > 0)
        || (m_verticalScrollbar && height() - m_verticalScrollbar->height() > 0);
}

IntRect ScrollView::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntRect& localRect) const
{
    // Scrollbars won't be transformed within us
    IntRect newRect = localRect;
    newRect.moveBy(scrollbar.location());
    return newRect;
}

IntRect ScrollView::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
{
    IntRect newRect = parentRect;
    // Scrollbars won't be transformed within us
    newRect.moveBy(-scrollbar.location());
    return newRect;
}

// FIXME: test these on windows
IntPoint ScrollView::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntPoint& localPoint) const
{
    // Scrollbars won't be transformed within us
    IntPoint newPoint = localPoint;
    newPoint.moveBy(scrollbar.location());
    return newPoint;
}

IntPoint ScrollView::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
{
    IntPoint newPoint = parentPoint;
    // Scrollbars won't be transformed within us
    newPoint.moveBy(-scrollbar.location());
    return newPoint;
}

void ScrollView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;
    
    Widget::setParentVisible(visible);

    if (!isSelfVisible())
        return;
        
    for (auto& child : m_children)
        child->setParentVisible(visible);
}

void ScrollView::show()
{
    if (!isSelfVisible()) {
        setSelfVisible(true);
        if (isParentVisible()) {
            for (auto& child : m_children)
                child->setParentVisible(true);
        }
    }

    Widget::show();
}

void ScrollView::hide()
{
    if (isSelfVisible()) {
        if (isParentVisible()) {
            for (auto& child : m_children)
                child->setParentVisible(false);
        }
        setSelfVisible(false);
    }

    Widget::hide();
}

bool ScrollView::isOffscreen() const
{
    if (platformWidget())
        return platformIsOffscreen();
    
    if (!isVisible())
        return true;
    
    // FIXME: Add a HostWindow::isOffscreen method here.  Since only Mac implements this method
    // currently, we can add the method when the other platforms decide to implement this concept.
    return false;
}


void ScrollView::addPanScrollIcon(const IntPoint& iconPosition)
{
    HostWindow* window = hostWindow();
    if (!window)
        return;
    m_drawPanScrollIcon = true;    
    m_panScrollIconPoint = IntPoint(iconPosition.x() - panIconSizeLength / 2 , iconPosition.y() - panIconSizeLength / 2) ;
    window->invalidateContentsAndRootView(IntRect(m_panScrollIconPoint, IntSize(panIconSizeLength, panIconSizeLength)));
}

void ScrollView::removePanScrollIcon()
{
    HostWindow* window = hostWindow();
    if (!window)
        return;
    m_drawPanScrollIcon = false; 
    window->invalidateContentsAndRootView(IntRect(m_panScrollIconPoint, IntSize(panIconSizeLength, panIconSizeLength)));
}

void ScrollView::setScrollOrigin(const IntPoint& origin, bool updatePositionAtAll, bool updatePositionSynchronously)
{
    if (scrollOrigin() == origin)
        return;

    ScrollableArea::setScrollOrigin(origin);

    if (platformWidget()) {
        platformSetScrollOrigin(origin, updatePositionAtAll, updatePositionSynchronously);
        return;
    }
    
    // Update if the scroll origin changes, since our position will be different if the content size did not change.
    if (updatePositionAtAll && updatePositionSynchronously)
        updateScrollbars(scrollPosition());
}

void ScrollView::styleDidChange()
{
    if (m_horizontalScrollbar)
        m_horizontalScrollbar->styleChanged();

    if (m_verticalScrollbar)
        m_verticalScrollbar->styleChanged();
}

IntPoint ScrollView::locationOfContents() const
{
    IntPoint result = location();
    if (shouldPlaceBlockDirectionScrollbarOnLeft() && m_verticalScrollbar)
        result.move(m_verticalScrollbar->occupiedWidth(), 0);
    return result;
}

#if !PLATFORM(COCOA)

void ScrollView::platformAddChild(Widget*)
{
}

void ScrollView::platformRemoveChild(Widget*)
{
}

#endif

#if !PLATFORM(COCOA)

void ScrollView::platformSetScrollbarsSuppressed(bool)
{
}

void ScrollView::platformSetScrollOrigin(const IntPoint&, bool, bool)
{
}

void ScrollView::platformSetScrollbarOverlayStyle(ScrollbarOverlayStyle)
{
}

#endif

#if !PLATFORM(COCOA)

void ScrollView::platformSetScrollbarModes()
{
}

void ScrollView::platformScrollbarModes(ScrollbarMode& horizontal, ScrollbarMode& vertical) const
{
    horizontal = ScrollbarAuto;
    vertical = ScrollbarAuto;
}

void ScrollView::platformSetCanBlitOnScroll(bool)
{
}

bool ScrollView::platformCanBlitOnScroll() const
{
    return false;
}

IntRect ScrollView::platformVisibleContentRect(bool) const
{
    return IntRect();
}

float ScrollView::platformTopContentInset() const
{
    return 0;
}

void ScrollView::platformSetTopContentInset(float)
{
}

IntSize ScrollView::platformVisibleContentSize(bool) const
{
    return IntSize();
}

IntRect ScrollView::platformVisibleContentRectIncludingObscuredArea(bool) const
{
    return IntRect();
}

IntSize ScrollView::platformVisibleContentSizeIncludingObscuredArea(bool) const
{
    return IntSize();
}

void ScrollView::platformSetContentsSize()
{
}

IntRect ScrollView::platformContentsToScreen(const IntRect& rect) const
{
    return rect;
}

IntPoint ScrollView::platformScreenToContents(const IntPoint& point) const
{
    return point;
}

void ScrollView::platformSetScrollPosition(const IntPoint&)
{
}

bool ScrollView::platformScroll(ScrollDirection, ScrollGranularity)
{
    return true;
}

void ScrollView::platformRepaintContentRectangle(const IntRect&)
{
}

bool ScrollView::platformIsOffscreen() const
{
    return false;
}

#endif

}
