/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "HostWindow.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include <wtf/StdLibExtras.h>

using std::max;

namespace WebCore {

ScrollView::ScrollView()
    : m_horizontalScrollbarMode(ScrollbarAuto)
    , m_verticalScrollbarMode(ScrollbarAuto)
    , m_prohibitsScrolling(false)
    , m_canBlitOnScroll(true)
    , m_scrollbarsAvoidingResizer(0)
    , m_scrollbarsSuppressed(false)
    , m_updatingScrollbarAttributes(false)
    , m_updateScrollbarsPass(0)
    , m_drawPanScrollIcon(false)
    , m_useFixedLayout(false)
{
    platformInit();
}

ScrollView::~ScrollView()
{
    platformDestroy();
}

void ScrollView::addChild(Widget* child) 
{
    ASSERT(child != this && !child->parent());
    child->setParent(this);
    m_children.add(child);
    if (child->platformWidget())
        platformAddChild(child);
}

void ScrollView::removeChild(Widget* child)
{
    ASSERT(child->parent() == this);
    child->setParent(0);
    m_children.remove(child);
    if (child->platformWidget())
        platformRemoveChild(child);
}

void ScrollView::setHasHorizontalScrollbar(bool hasBar)
{
    if (hasBar && !m_horizontalScrollbar && !platformHasHorizontalAdjustment()) {
        m_horizontalScrollbar = createScrollbar(HorizontalScrollbar);
        addChild(m_horizontalScrollbar.get());
    } else if (!hasBar && m_horizontalScrollbar) {
        removeChild(m_horizontalScrollbar.get());
        m_horizontalScrollbar = 0;
    }
}

void ScrollView::setHasVerticalScrollbar(bool hasBar)
{
    if (hasBar && !m_verticalScrollbar && !platformHasVerticalAdjustment()) {
        m_verticalScrollbar = createScrollbar(VerticalScrollbar);
        addChild(m_verticalScrollbar.get());
    } else if (!hasBar && m_verticalScrollbar) {
        removeChild(m_verticalScrollbar.get());
        m_verticalScrollbar = 0;
    }
}

PassRefPtr<Scrollbar> ScrollView::createScrollbar(ScrollbarOrientation orientation)
{
    return Scrollbar::createNativeScrollbar(this, orientation, RegularScrollbar);
}

void ScrollView::setScrollbarModes(ScrollbarMode horizontalMode, ScrollbarMode verticalMode)
{
    if (horizontalMode == horizontalScrollbarMode() && verticalMode == verticalScrollbarMode())
        return;
    m_horizontalScrollbarMode = horizontalMode;
    m_verticalScrollbarMode = verticalMode;
    if (platformWidget())
        platformSetScrollbarModes();
    else
        updateScrollbars(scrollOffset());
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

IntRect ScrollView::visibleContentRect(bool includeScrollbars) const
{
    if (platformWidget())
        return platformVisibleContentRect(includeScrollbars);
    return IntRect(IntPoint(m_scrollOffset.width(), m_scrollOffset.height()),
                   IntSize(max(0, width() - (verticalScrollbar() && !includeScrollbars ? verticalScrollbar()->width() : 0)), 
                           max(0, height() - (horizontalScrollbar() && !includeScrollbars ? horizontalScrollbar()->height() : 0))));
}

int ScrollView::layoutWidth() const
{
    return m_fixedLayoutSize.isEmpty() || !m_useFixedLayout ? visibleWidth() : m_fixedLayoutSize.width();
}

int ScrollView::layoutHeight() const
{
    return m_fixedLayoutSize.isEmpty() || !m_useFixedLayout ? visibleHeight() : m_fixedLayoutSize.height();
}

IntSize ScrollView::fixedLayoutSize() const
{
    return m_fixedLayoutSize;
}

void ScrollView::setFixedLayoutSize(const IntSize& newSize)
{
    if (fixedLayoutSize() == newSize)
        return;
    m_fixedLayoutSize = newSize;
    updateScrollbars(scrollOffset());
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
    updateScrollbars(scrollOffset());
}

IntSize ScrollView::contentsSize() const
{
    if (platformWidget())
        return platformContentsSize();
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
        updateScrollbars(scrollOffset());
}

IntPoint ScrollView::maximumScrollPosition() const
{
    IntSize maximumOffset = contentsSize() - visibleContentRect().size();
    maximumOffset.clampNegativeToZero();
    return IntPoint(maximumOffset.width(), maximumOffset.height());
}

void ScrollView::valueChanged(Scrollbar* scrollbar)
{
    // Figure out if we really moved.
    IntSize newOffset = m_scrollOffset;
    if (scrollbar) {
        if (scrollbar == m_horizontalScrollbar)
            newOffset.setWidth(scrollbar->value());
        else if (scrollbar == m_verticalScrollbar)
            newOffset.setHeight(scrollbar->value());
    }

    IntSize scrollDelta = newOffset - m_scrollOffset;
    if (scrollDelta == IntSize())
        return;
    m_scrollOffset = newOffset;

    if (scrollbarsSuppressed())
        return;

    scrollContents(scrollDelta);
}

void ScrollView::scrollRectIntoViewRecursively(const IntRect& r)
{
    // FIXME: This method is not transform-aware.  It should just be moved to FrameView so that an accurate
    // position for the child view can be determined.
    IntRect rect = r;
    ScrollView* view = this;
    while (view) {
        if (view->prohibitsScrolling()) // Allow the views to scroll into view recursively until we hit one that prohibits scrolling.
            return;
        view->setScrollPosition(rect.location());
        rect.move(view->x() - view->scrollOffset().width(), view->y() - view->scrollOffset().height());
        if (view->parent())
            rect.intersect(view->frameRect());
        view = view->parent();
    }
    
    // We may be embedded inside some containing platform scroll view that we don't manage.  This is the case
    // in Mail.app on OS X, for example, where the WebKit view for message bodies is inside a Cocoa NSScrollView
    // that contains both it and message headers.  Let the HostWindow know about this scroll so that it can pass the message
    // on up the view chain.
    // This rect is not clamped, since Mail actually relies on receiving an unclamped rect with negative coordinates in order to
    // expose the headers.
    hostWindow()->scrollRectIntoView(rect, this);
}

void ScrollView::setScrollPosition(const IntPoint& scrollPoint)
{
    if (prohibitsScrolling())
        return;

    if (platformWidget()) {
        platformSetScrollPosition(scrollPoint);
        return;
    }

    IntPoint newScrollPosition = scrollPoint.shrunkTo(maximumScrollPosition());
    newScrollPosition.clampNegativeToZero();

    if (newScrollPosition == scrollPosition())
        return;

    updateScrollbars(IntSize(newScrollPosition.x(), newScrollPosition.y()));
}

bool ScrollView::scroll(ScrollDirection direction, ScrollGranularity granularity)
{
    if (platformWidget())
        return platformScroll(direction, granularity);

    if (direction == ScrollUp || direction == ScrollDown) {
        if (m_verticalScrollbar)
            return m_verticalScrollbar->scroll(direction, granularity);
    } else {
        if (m_horizontalScrollbar)
            return m_horizontalScrollbar->scroll(direction, granularity);
    }
    return false;
}

static const unsigned cMaxUpdateScrollbarsPass = 2;

void ScrollView::updateScrollbars(const IntSize& desiredOffset)
{
    if (m_updatingScrollbarAttributes || prohibitsScrolling() || platformWidget())
        return;

    bool hasHorizontalScrollbar = m_horizontalScrollbar;
    bool hasVerticalScrollbar = m_verticalScrollbar;
    
    bool newHasHorizontalScrollbar = hasHorizontalScrollbar;
    bool newHasVerticalScrollbar = hasVerticalScrollbar;
   
    ScrollbarMode hScroll = m_horizontalScrollbarMode;
    ScrollbarMode vScroll = m_verticalScrollbarMode;

    if (m_scrollbarsSuppressed || (hScroll != ScrollbarAuto && vScroll != ScrollbarAuto)) {
        if (hScroll != ScrollbarAuto)
            newHasHorizontalScrollbar = (hScroll == ScrollbarAlwaysOn);
        if (vScroll != ScrollbarAuto)
            newHasVerticalScrollbar = (vScroll == ScrollbarAlwaysOn);
        if (hasHorizontalScrollbar != newHasHorizontalScrollbar)
            setHasHorizontalScrollbar(newHasHorizontalScrollbar);
        if (hasVerticalScrollbar != newHasVerticalScrollbar)
            setHasVerticalScrollbar(newHasVerticalScrollbar);
    } else {
        bool sendContentResizedNotification = false;
        
        IntSize docSize = contentsSize();
        
        if (hScroll == ScrollbarAuto)
            newHasHorizontalScrollbar = docSize.width() > visibleWidth();
        if (vScroll == ScrollbarAuto)
            newHasVerticalScrollbar = docSize.height() > visibleHeight();
        
        // If we ever turn one scrollbar off, always turn the other one off too.  Never ever
        // try to both gain/lose a scrollbar in the same pass.
        if (!newHasHorizontalScrollbar && hasHorizontalScrollbar)
            newHasVerticalScrollbar = false;
        if (!newHasVerticalScrollbar && hasVerticalScrollbar)
            newHasHorizontalScrollbar = false;

        if (hasHorizontalScrollbar != newHasHorizontalScrollbar) {
            setHasHorizontalScrollbar(newHasHorizontalScrollbar);
            sendContentResizedNotification = true;
        }
 
        if (hasVerticalScrollbar != newHasVerticalScrollbar) {
            setHasVerticalScrollbar(newHasVerticalScrollbar);
            sendContentResizedNotification = true;
        }

        if (sendContentResizedNotification && m_updateScrollbarsPass < cMaxUpdateScrollbarsPass) {
            m_updateScrollbarsPass++;
            visibleContentsResized();
            IntSize newDocSize = contentsSize();
            if (newDocSize != docSize) {
                // The layout with the new scroll state had no impact on
                // the document's overall size, so updateScrollbars didn't get called.
                // Recur manually.
                updateScrollbars(desiredOffset);
            }
            m_updateScrollbarsPass--;
        }
    }
    
    // Set up the range (and page step/line step), but only do this if we're not in a nested call (to avoid
    // doing it multiple times).
    if (m_updateScrollbarsPass)
        return;

    m_updatingScrollbarAttributes = true;
    IntSize maxScrollPosition(contentsWidth() - visibleWidth(), contentsHeight() - visibleHeight());
    IntSize scroll = desiredOffset.shrunkTo(maxScrollPosition);
    scroll.clampNegativeToZero();
 
    if (!platformHandleHorizontalAdjustment(scroll) && m_horizontalScrollbar) {
        int clientWidth = visibleWidth();
        m_horizontalScrollbar->setEnabled(contentsWidth() > clientWidth);
        int pageStep = (clientWidth - cAmountToKeepWhenPaging);
        if (pageStep < 0)
            pageStep = clientWidth;
        IntRect oldRect(m_horizontalScrollbar->frameRect());
        IntRect hBarRect = IntRect(0,
                                   height() - m_horizontalScrollbar->height(),
                                   width() - (m_verticalScrollbar ? m_verticalScrollbar->width() : 0),
                                   m_horizontalScrollbar->height());
        m_horizontalScrollbar->setFrameRect(hBarRect);
        if (!m_scrollbarsSuppressed && oldRect != m_horizontalScrollbar->frameRect())
            m_horizontalScrollbar->invalidate();

        if (m_scrollbarsSuppressed)
            m_horizontalScrollbar->setSuppressInvalidation(true);
        m_horizontalScrollbar->setSteps(cScrollbarPixelsPerLineStep, pageStep);
        m_horizontalScrollbar->setProportion(clientWidth, contentsWidth());
        m_horizontalScrollbar->setValue(scroll.width());
        if (m_scrollbarsSuppressed)
            m_horizontalScrollbar->setSuppressInvalidation(false); 
    } 

    if (!platformHandleVerticalAdjustment(scroll) && m_verticalScrollbar) {
        int clientHeight = visibleHeight();
        m_verticalScrollbar->setEnabled(contentsHeight() > clientHeight);
        int pageStep = (clientHeight - cAmountToKeepWhenPaging);
        if (pageStep < 0)
            pageStep = clientHeight;
        IntRect oldRect(m_verticalScrollbar->frameRect());
        IntRect vBarRect = IntRect(width() - m_verticalScrollbar->width(), 
                                   0,
                                   m_verticalScrollbar->width(),
                                   height() - (m_horizontalScrollbar ? m_horizontalScrollbar->height() : 0));
        m_verticalScrollbar->setFrameRect(vBarRect);
        if (!m_scrollbarsSuppressed && oldRect != m_verticalScrollbar->frameRect())
            m_verticalScrollbar->invalidate();

        if (m_scrollbarsSuppressed)
            m_verticalScrollbar->setSuppressInvalidation(true);
        m_verticalScrollbar->setSteps(cScrollbarPixelsPerLineStep, pageStep);
        m_verticalScrollbar->setProportion(clientHeight, contentsHeight());
        m_verticalScrollbar->setValue(scroll.height());
        if (m_scrollbarsSuppressed)
            m_verticalScrollbar->setSuppressInvalidation(false);
    }

    if (hasHorizontalScrollbar != (m_horizontalScrollbar != 0) || hasVerticalScrollbar != (m_verticalScrollbar != 0))
        frameRectsChanged();

    // See if our offset has changed in a situation where we might not have scrollbars.
    // This can happen when editing a body with overflow:hidden and scrolling to reveal selection.
    // It can also happen when maximizing a window that has scrollbars (but the new maximized result
    // does not).
    IntSize scrollDelta = scroll - m_scrollOffset;
    if (scrollDelta != IntSize()) {
       m_scrollOffset = scroll;
       scrollContents(scrollDelta);
    }

    m_updatingScrollbarAttributes = false;
}

const int panIconSizeLength = 20;

void ScrollView::scrollContents(const IntSize& scrollDelta)
{
    if (!hostWindow())
        return;

    // Since scrolling is double buffered, we will be blitting the scroll view's intersection
    // with the clip rect every time to keep it smooth.
    IntRect clipRect = windowClipRect();
    IntRect scrollViewRect = convertToContainingWindow(IntRect(0, 0, visibleWidth(), visibleHeight()));
    IntRect updateRect = clipRect;
    updateRect.intersect(scrollViewRect);

    // Invalidate the window (not the backing store).
    hostWindow()->repaint(updateRect, false);

    if (m_drawPanScrollIcon) {
        int panIconDirtySquareSizeLength = 2 * (panIconSizeLength + max(abs(scrollDelta.width()), abs(scrollDelta.height()))); // We only want to repaint what's necessary
        IntPoint panIconDirtySquareLocation = IntPoint(m_panScrollIconPoint.x() - (panIconDirtySquareSizeLength / 2), m_panScrollIconPoint.y() - (panIconDirtySquareSizeLength / 2));
        IntRect panScrollIconDirtyRect = IntRect(panIconDirtySquareLocation , IntSize(panIconDirtySquareSizeLength, panIconDirtySquareSizeLength));
        panScrollIconDirtyRect.intersect(clipRect);
        hostWindow()->repaint(panScrollIconDirtyRect, true);
    }

    if (canBlitOnScroll() && !rootPreventsBlitting()) { // The main frame can just blit the WebView window
       // FIXME: Find a way to blit subframes without blitting overlapping content
       hostWindow()->scroll(-scrollDelta, scrollViewRect, clipRect);
    } else { 
       // We need to go ahead and repaint the entire backing store.  Do it now before moving the
       // plugins.
       hostWindow()->repaint(updateRect, true, false, true); // Invalidate the backing store and repaint it synchronously
    }

    // This call will move children with native widgets (plugins) and invalidate them as well.
    frameRectsChanged();

    // Now update the window (which should do nothing but a blit of the backing store's updateRect and so should
    // be very fast).
    hostWindow()->paint();
}

IntPoint ScrollView::windowToContents(const IntPoint& windowPoint) const
{
    IntPoint viewPoint = convertFromContainingWindow(windowPoint);
    return viewPoint + scrollOffset();
}

IntPoint ScrollView::contentsToWindow(const IntPoint& contentsPoint) const
{
    IntPoint viewPoint = contentsPoint - scrollOffset();
    return convertToContainingWindow(viewPoint);  
}

IntRect ScrollView::windowToContents(const IntRect& windowRect) const
{
    IntRect viewRect = convertFromContainingWindow(windowRect);
    viewRect.move(scrollOffset());
    return viewRect;
}

IntRect ScrollView::contentsToWindow(const IntRect& contentsRect) const
{
    IntRect viewRect = contentsRect;
    viewRect.move(-scrollOffset());
    return convertToContainingWindow(viewRect);
}

IntRect ScrollView::contentsToScreen(const IntRect& rect) const
{
    if (platformWidget())
        return platformContentsToScreen(rect);
    return hostWindow()->windowToScreen(contentsToWindow(rect));
}

IntPoint ScrollView::screenToContents(const IntPoint& point) const
{
    if (platformWidget())
        return platformScreenToContents(point);
    return windowToContents(hostWindow()->screenToWindow(point));
}

bool ScrollView::containsScrollbarsAvoidingResizer() const
{
    return !m_scrollbarsAvoidingResizer;
}

void ScrollView::adjustScrollbarsAvoidingResizerCount(int overlapDelta)
{
    int oldCount = m_scrollbarsAvoidingResizer;
    m_scrollbarsAvoidingResizer += overlapDelta;
    if (parent())
        parent()->adjustScrollbarsAvoidingResizerCount(overlapDelta);
    else if (!scrollbarsSuppressed()) {
        // If we went from n to 0 or from 0 to n and we're the outermost view,
        // we need to invalidate the windowResizerRect(), since it will now need to paint
        // differently.
        if ((oldCount > 0 && m_scrollbarsAvoidingResizer == 0) ||
            (oldCount == 0 && m_scrollbarsAvoidingResizer > 0))
            invalidateRect(windowResizerRect());
    }
}

void ScrollView::setParent(ScrollView* parentView)
{
    if (parentView == parent())
        return;

    if (m_scrollbarsAvoidingResizer && parent())
        parent()->adjustScrollbarsAvoidingResizerCount(-m_scrollbarsAvoidingResizer);

    Widget::setParent(parentView);
    
    if (m_scrollbarsAvoidingResizer && parent())
        parent()->adjustScrollbarsAvoidingResizerCount(m_scrollbarsAvoidingResizer);
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
        IntRect hCorner;
        if (m_horizontalScrollbar && width() - m_horizontalScrollbar->width() > 0) {
            hCorner = IntRect(m_horizontalScrollbar->width(),
                              height() - m_horizontalScrollbar->height(),
                              width() - m_horizontalScrollbar->width(),
                              m_horizontalScrollbar->height());
            invalidateRect(hCorner);
        }

        if (m_verticalScrollbar && height() - m_verticalScrollbar->height() > 0) {
            IntRect vCorner(width() - m_verticalScrollbar->width(),
                            m_verticalScrollbar->height(),
                            m_verticalScrollbar->width(),
                            height() - m_verticalScrollbar->height());
            if (vCorner != hCorner)
                invalidateRect(vCorner);
        }
    }
}

Scrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent)
{
    if (platformWidget())
        return 0;

    IntPoint viewPoint = convertFromContainingWindow(mouseEvent.pos());
    if (m_horizontalScrollbar && m_horizontalScrollbar->frameRect().contains(viewPoint))
        return m_horizontalScrollbar.get();
    if (m_verticalScrollbar && m_verticalScrollbar->frameRect().contains(viewPoint))
        return m_verticalScrollbar.get();
    return 0;
}

void ScrollView::wheelEvent(PlatformWheelEvent& e)
{
    // We don't allow mouse wheeling to happen in a ScrollView that has had its scrollbars explicitly disabled.
    if (!canHaveScrollbars() || platformWidget())
        return;

    // Determine how much we want to scroll.  If we can move at all, we will accept the event.
    IntSize maxScrollDelta = maximumScrollPosition() - scrollPosition();
    if ((e.deltaX() < 0 && maxScrollDelta.width() > 0) ||
        (e.deltaX() > 0 && scrollOffset().width() > 0) ||
        (e.deltaY() < 0 && maxScrollDelta.height() > 0) ||
        (e.deltaY() > 0 && scrollOffset().height() > 0)) {
        e.accept();
        float deltaX = e.deltaX();
        float deltaY = e.deltaY();
        if (e.granularity() == ScrollByPageWheelEvent) {
            ASSERT(deltaX == 0);
            bool negative = deltaY < 0;
            deltaY = max(0, visibleHeight() - cAmountToKeepWhenPaging);
            if (negative)
                deltaY = -deltaY;
        }
        scrollBy(IntSize(-deltaX, -deltaY));
    }
}

void ScrollView::setFrameRect(const IntRect& newRect)
{
    IntRect oldRect = frameRect();
    
    if (newRect == oldRect)
        return;

    Widget::setFrameRect(newRect);

    if (platformWidget())
        return;
    
    if (newRect.width() != oldRect.width() || newRect.height() != oldRect.height()) {
        updateScrollbars(m_scrollOffset);
        contentsResized();
    }

    frameRectsChanged();
}

void ScrollView::frameRectsChanged()
{
    if (platformWidget())
        return;

    HashSet<Widget*>::const_iterator end = m_children.end();
    for (HashSet<Widget*>::const_iterator current = m_children.begin(); current != end; ++current)
        (*current)->frameRectsChanged();
}

void ScrollView::repaintContentRectangle(const IntRect& rect, bool now)
{
    if (rect.isEmpty())
        return;

    if (platformWidget()) {
        platformRepaintContentRectangle(rect, now);
        return;
    }

    hostWindow()->repaint(contentsToWindow(rect), true, now);
}

void ScrollView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (platformWidget()) {
        Widget::paint(context, rect);
        return;
    }

    if (context->paintingDisabled() && !context->updatingControlTints())
        return;

    IntRect documentDirtyRect = rect;
    documentDirtyRect.intersect(frameRect());

    context->save();

    context->translate(x(), y());
    documentDirtyRect.move(-x(), -y());

    context->translate(-scrollX(), -scrollY());
    documentDirtyRect.move(scrollX(), scrollY());

    context->clip(visibleContentRect());

    paintContents(context, documentDirtyRect);

    context->restore();

    // Now paint the scrollbars.
    if (!m_scrollbarsSuppressed && (m_horizontalScrollbar || m_verticalScrollbar)) {
        context->save();
        IntRect scrollViewDirtyRect = rect;
        scrollViewDirtyRect.intersect(frameRect());
        context->translate(x(), y());
        scrollViewDirtyRect.move(-x(), -y());
        if (m_horizontalScrollbar)
            m_horizontalScrollbar->paint(context, scrollViewDirtyRect);
        if (m_verticalScrollbar)
            m_verticalScrollbar->paint(context, scrollViewDirtyRect);

        IntRect hCorner;
        if (m_horizontalScrollbar && width() - m_horizontalScrollbar->width() > 0) {
            hCorner = IntRect(m_horizontalScrollbar->width(),
                              height() - m_horizontalScrollbar->height(),
                              width() - m_horizontalScrollbar->width(),
                              m_horizontalScrollbar->height());
            if (hCorner.intersects(scrollViewDirtyRect))
                ScrollbarTheme::nativeTheme()->paintScrollCorner(this, context, hCorner);
        }

        if (m_verticalScrollbar && height() - m_verticalScrollbar->height() > 0) {
            IntRect vCorner(width() - m_verticalScrollbar->width(),
                            m_verticalScrollbar->height(),
                            m_verticalScrollbar->width(),
                            height() - m_verticalScrollbar->height());
            if (vCorner != hCorner && vCorner.intersects(scrollViewDirtyRect))
                ScrollbarTheme::nativeTheme()->paintScrollCorner(this, context, vCorner);
        }

        context->restore();
    }

    // Paint the panScroll Icon
    if (m_drawPanScrollIcon) {
        DEFINE_STATIC_LOCAL(RefPtr<Image>, panScrollIcon, (Image::loadPlatformResource("panIcon")));
        context->drawImage(panScrollIcon.get(), m_panScrollIconPoint);
    }
}

bool ScrollView::scrollbarCornerPresent() const
{
    return (m_horizontalScrollbar && width() - m_horizontalScrollbar->width() > 0) ||
           (m_verticalScrollbar && height() - m_verticalScrollbar->height() > 0);
}

void ScrollView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;
    
    Widget::setParentVisible(visible);

    if (!isSelfVisible())
        return;
        
    HashSet<Widget*>::iterator end = m_children.end();
    for (HashSet<Widget*>::iterator it = m_children.begin(); it != end; ++it)
        (*it)->setParentVisible(visible);
}

void ScrollView::show()
{
    if (!isSelfVisible()) {
        setSelfVisible(true);
        if (isParentVisible()) {
            HashSet<Widget*>::iterator end = m_children.end();
            for (HashSet<Widget*>::iterator it = m_children.begin(); it != end; ++it)
                (*it)->setParentVisible(true);
        }
    }

    Widget::show();
}

void ScrollView::hide()
{
    if (isSelfVisible()) {
        if (isParentVisible()) {
            HashSet<Widget*>::iterator end = m_children.end();
            for (HashSet<Widget*>::iterator it = m_children.begin(); it != end; ++it)
                (*it)->setParentVisible(false);
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
    m_drawPanScrollIcon = true;    
    m_panScrollIconPoint = IntPoint(iconPosition.x() - panIconSizeLength / 2 , iconPosition.y() - panIconSizeLength / 2) ;
    hostWindow()->repaint(IntRect(m_panScrollIconPoint, IntSize(panIconSizeLength,panIconSizeLength)), true, true);    
}

void ScrollView::removePanScrollIcon()
{
    m_drawPanScrollIcon = false; 
    hostWindow()->repaint(IntRect(m_panScrollIconPoint, IntSize(panIconSizeLength, panIconSizeLength)), true, true);
}

#if !PLATFORM(WX) && !PLATFORM(GTK) && !PLATFORM(QT)
void ScrollView::platformInit()
{
}

void ScrollView::platformDestroy()
{
}
#endif

#if !PLATFORM(WX) && !PLATFORM(GTK) && !PLATFORM(QT) && !PLATFORM(MAC)
void ScrollView::platformAddChild(Widget*)
{
}

void ScrollView::platformRemoveChild(Widget*)
{
}
#endif

#if !PLATFORM(MAC)
void ScrollView::platformSetScrollbarsSuppressed(bool repaintOnUnsuppress)
{
}
#endif

#if !PLATFORM(MAC) && !PLATFORM(WX)
void ScrollView::platformSetScrollbarModes()
{
}

void ScrollView::platformScrollbarModes(ScrollbarMode& horizontal, ScrollbarMode& vertical) const
{
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

IntSize ScrollView::platformContentsSize() const
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

void ScrollView::platformRepaintContentRectangle(const IntRect&, bool now)
{
}

bool ScrollView::platformIsOffscreen() const
{
    return false;
}
#endif

#if !PLATFORM(GTK)
bool ScrollView::platformHandleHorizontalAdjustment(const IntSize&)
{
    return false;
}

bool ScrollView::platformHandleVerticalAdjustment(const IntSize&)
{
    return false;
}

bool ScrollView::platformHasHorizontalAdjustment() const
{
    return false;
}

bool ScrollView::platformHasVerticalAdjustment() const
{
    return false;
}

#endif

}

