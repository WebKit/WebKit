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

#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "Scrollbar.h"

using std::max;

namespace WebCore {

void ScrollView::init()
{
    m_canBlitOnScroll = true;
    m_horizontalScrollbarMode = m_verticalScrollbarMode = ScrollbarAuto;
    if (platformWidget())
        platformSetCanBlitOnScroll();
    m_scrollbarsAvoidingResizer = 0;
    m_scrollbarsSuppressed = false;
}

void ScrollView::addChild(Widget* child) 
{
    ASSERT(child != this && !child->parent());
    child->setParent(this);
    m_children.add(child);
    if (!child->platformWidget()) {
        child->setContainingWindow(containingWindow());
        return;
    }
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

void ScrollView::setScrollbarModes(ScrollbarMode horizontalMode, ScrollbarMode verticalMode)
{
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

void ScrollView::setAllowsScrolling(bool canScroll)
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
    if (m_canBlitOnScroll == b)
        return;
    m_canBlitOnScroll = b;
    if (platformWidget())
        platformSetCanBlitOnScroll();
}

IntRect ScrollView::visibleContentRect(bool includeScrollbars) const
{
    if (platformWidget())
        return platformVisibleContentRect(includeScrollbars);
    return IntRect(IntPoint(m_scrollOffset.width(), m_scrollOffset.height()),
                   IntSize(max(0, width() - (verticalScrollbar() && !includeScrollbars ? verticalScrollbar()->width() : 0)), 
                           max(0, height() - (horizontalScrollbar() && !includeScrollbars ? horizontalScrollbar()->height() : 0))));
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

void ScrollView::scrollRectIntoViewRecursively(const IntRect& r)
{
    IntPoint p(max(0, r.x()), max(0, r.y()));
    ScrollView* view = this;
    while (view) {
        view->setScrollPosition(p);
        p.move(view->x() - view->scrollOffset().width(), view->y() - view->scrollOffset().height());
        view = view->parent();
    }
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
        if (oldCount > 0 && m_scrollbarsAvoidingResizer == 0 ||
            oldCount == 0 && m_scrollbarsAvoidingResizer > 0)
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
    if (!allowsScrolling() || platformWidget())
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
        if (e.granularity() == ScrollByLineWheelEvent) {
            deltaX *= cMouseWheelPixelsPerLineStep;
            deltaY *= cMouseWheelPixelsPerLineStep;
        } else if (e.granularity() == ScrollByPageWheelEvent) {
            ASSERT(deltaX == 0);
            bool negative = deltaY < 0;
            deltaY = max(0, visibleHeight() - cAmountToKeepWhenPaging);
            if (negative)
                deltaY = -deltaY;
        }
        scrollBy(IntSize(-deltaX, -deltaY));
    }
}

#if !PLATFORM(MAC)
void ScrollView::platformSetCanBlitOnScroll()
{
}

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
#endif

}

