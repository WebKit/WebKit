/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
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

#include "FrameView.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "IntPoint.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "NotImplemented.h"
#include "Frame.h"
#include "Page.h"
#include "GraphicsContext.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"

#include <QDebug>
#include <QWidget>
#include <QPainter>
#include <QApplication>
#include <QPalette>
#include <QStyleOption>

#ifdef Q_WS_MAC
#include <Carbon/Carbon.h>
#endif

#include "qwebframe.h"
#include "qwebpage.h"

// #define DEBUG_SCROLLVIEW

namespace WebCore {

class ScrollView::ScrollViewPrivate : public ScrollbarClient {
public:
    ScrollViewPrivate(ScrollView* view)
      : m_view(view)
      , m_platformWidgets(0)
      , m_inUpdateScrollbars(false)
    {
    }

    ~ScrollViewPrivate()
    {
        setHasHorizontalScrollbar(false);
        setHasVerticalScrollbar(false);
    }

    void setHasHorizontalScrollbar(bool hasBar);
    void setHasVerticalScrollbar(bool hasBar);

    virtual void valueChanged(Scrollbar*);
    virtual IntRect windowClipRect() const;
    virtual bool isActive() const;

    void scrollBackingStore(const IntSize& scrollDelta);

    ScrollView* m_view;
    int  m_platformWidgets;
    bool m_inUpdateScrollbars;
};

void ScrollView::ScrollViewPrivate::setHasHorizontalScrollbar(bool hasBar)
{
    if (hasBar && !m_view->m_horizontalScrollbar) {
        m_view->m_horizontalScrollbar = Scrollbar::createNativeScrollbar(this, HorizontalScrollbar, RegularScrollbar);
        m_view->addChild(m_view->m_horizontalScrollbar.get());
    } else if (!hasBar && m_view->m_horizontalScrollbar) {
        m_view->removeChild(m_view->m_horizontalScrollbar.get());
        m_view->m_horizontalScrollbar = 0;
    }
}

void ScrollView::ScrollViewPrivate::setHasVerticalScrollbar(bool hasBar)
{
    if (hasBar && !m_view->m_verticalScrollbar) {
        m_view->m_verticalScrollbar = Scrollbar::createNativeScrollbar(this, VerticalScrollbar, RegularScrollbar);
        m_view->addChild(m_view->m_verticalScrollbar.get());
    } else if (!hasBar && m_view->m_verticalScrollbar) {
        m_view->removeChild(m_view->m_verticalScrollbar.get());
        m_view->m_verticalScrollbar = 0;
    }
}

void ScrollView::ScrollViewPrivate::valueChanged(Scrollbar* bar)
{
    // Figure out if we really moved.
    IntSize newOffset = m_view->m_scrollOffset;
    if (bar) {
        if (bar == m_view->m_horizontalScrollbar)
            newOffset.setWidth(bar->value());
        else if (bar == m_view->m_verticalScrollbar)
            newOffset.setHeight(bar->value());
    }
    IntSize scrollDelta = newOffset - m_view->m_scrollOffset;
    if (scrollDelta == IntSize())
        return;
    m_view->m_scrollOffset = newOffset;

    if (m_view->scrollbarsSuppressed())
        return;

    scrollBackingStore(scrollDelta);
    static_cast<FrameView*>(m_view)->frame()->sendScrollEvent();
}

void ScrollView::ScrollViewPrivate::scrollBackingStore(const IntSize& scrollDelta)
{
    // Since scrolling is double buffered, we will be blitting the scroll view's intersection
    // with the clip rect every time to keep it smooth.
    IntRect clipRect = m_view->windowClipRect();
    IntRect scrollViewRect = m_view->convertToContainingWindow(IntRect(0, 0, m_view->visibleWidth(), m_view->visibleHeight()));

    IntRect updateRect = clipRect;
    updateRect.intersect(scrollViewRect);

    if (m_view->canBlitOnScroll() && !m_view->root()->hasNativeWidgets()) {
       m_view->scrollBackingStore(-scrollDelta.width(), -scrollDelta.height(),
                                  scrollViewRect, clipRect);
    } else  {
       // We need to go ahead and repaint the entire backing store.
       m_view->addToDirtyRegion(updateRect);
       m_view->updateBackingStore();
    }

    m_view->frameRectsChanged();
}

IntRect ScrollView::ScrollViewPrivate::windowClipRect() const
{
    return static_cast<const FrameView*>(m_view)->windowClipRect(false);
}

bool ScrollView::ScrollViewPrivate::isActive() const
{
    Page* page = static_cast<const FrameView*>(m_view)->frame()->page();
    return page && page->focusController()->isActive();
}

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate(this))
{
    init();
}

ScrollView::~ScrollView()
{
    delete m_data;
}

bool ScrollView::isOffscreen() const
{
    return false;
}

void ScrollView::updateScrollbars(const IntSize& desiredOffset)
{
    // Don't allow re-entrancy into this function.
    if (m_data->m_inUpdateScrollbars)
        return;

    // FIXME: This code is here so we don't have to fork FrameView.h/.cpp.
    // In the end, FrameView should just merge with ScrollView.
    if (static_cast<const FrameView*>(this)->frame()->prohibitsScrolling())
        return;
    
    m_data->m_inUpdateScrollbars = true;

    bool hasVerticalScrollbar = m_verticalScrollbar;
    bool hasHorizontalScrollbar = m_horizontalScrollbar;
    bool oldHasVertical = hasVerticalScrollbar;
    bool oldHasHorizontal = hasHorizontalScrollbar;
    ScrollbarMode hScroll = m_horizontalScrollbarMode;
    ScrollbarMode vScroll = m_verticalScrollbarMode;

    const int cVerticalWidth = ScrollbarTheme::nativeTheme()->scrollbarThickness();
    const int cHorizontalHeight = ScrollbarTheme::nativeTheme()->scrollbarThickness();

    for (int pass = 0; pass < 2; pass++) {
        bool scrollsVertically;
        bool scrollsHorizontally;

        if (!m_scrollbarsSuppressed && (hScroll == ScrollbarAuto || vScroll == ScrollbarAuto)) {
            // Do a layout if pending before checking if scrollbars are needed.
            if (hasVerticalScrollbar != oldHasVertical || hasHorizontalScrollbar != oldHasHorizontal)
                static_cast<FrameView*>(this)->layout();
             
            scrollsVertically = (vScroll == ScrollbarAlwaysOn) || (vScroll == ScrollbarAuto && contentsHeight() > height());
            if (scrollsVertically)
                scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) || (hScroll == ScrollbarAuto && contentsWidth() + cVerticalWidth > width());
            else {
                scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) || (hScroll == ScrollbarAuto && contentsWidth() > width());
                if (scrollsHorizontally)
                    scrollsVertically = (vScroll == ScrollbarAlwaysOn) || (vScroll == ScrollbarAuto && contentsHeight() + cHorizontalHeight > height());
            }
        }
        else {
            scrollsHorizontally = (hScroll == ScrollbarAuto) ? hasHorizontalScrollbar : (hScroll == ScrollbarAlwaysOn);
            scrollsVertically = (vScroll == ScrollbarAuto) ? hasVerticalScrollbar : (vScroll == ScrollbarAlwaysOn);
        }
        
        if (hasVerticalScrollbar != scrollsVertically) {
            m_data->setHasVerticalScrollbar(scrollsVertically);
            hasVerticalScrollbar = scrollsVertically;
        }

        if (hasHorizontalScrollbar != scrollsHorizontally) {
            m_data->setHasHorizontalScrollbar(scrollsHorizontally);
            hasHorizontalScrollbar = scrollsHorizontally;
        }
    }
    
    // Set up the range (and page step/line step).
    IntSize maxScrollPosition(contentsWidth() - visibleWidth(), contentsHeight() - visibleHeight());
    IntSize scroll = desiredOffset.shrunkTo(maxScrollPosition);
    scroll.clampNegativeToZero();

    QPoint scrollbarOffset;
#if defined(Q_WS_MAC) && !defined(QT_MAC_USE_COCOA)
    // On Mac, offset the scrollbars so they don't cover the grow box. Check if the window 
    // has a grow box, and then check if the bottom-right corner of the scroll view 
    // intersercts it. The calculations are done in global coordinates.
    QWidget* contentWidget = containingWindow();
    if (contentWidget) {
        QWidget* windowWidget = contentWidget->window();
        if (windowWidget) {
            HIViewRef growBox = 0;
            HIViewFindByID(HIViewGetRoot(HIViewGetWindow(HIViewRef(contentWidget->winId()))), kHIViewWindowGrowBoxID, &growBox);
            const QPoint contentBr = contentWidget->mapToGlobal(QPoint(0,0)) + contentWidget->size();
            const QPoint windowBr = windowWidget->mapToGlobal(QPoint(0,0)) + windowWidget->size();
            const QPoint contentOffset = (windowBr - contentBr);
            const int growBoxSize = 15;
            const bool enableOffset = (growBox != 0 && contentOffset.x() >= 0 && contentOffset. y() >= 0);
            scrollbarOffset = enableOffset ? QPoint(growBoxSize - qMin(contentOffset.x(), growBoxSize),
                                                    growBoxSize - qMin(contentOffset.y(), growBoxSize))
                                           : QPoint(0,0);
        }
    }
#endif

    if (m_horizontalScrollbar) {
        int clientWidth = visibleWidth();
        m_horizontalScrollbar->setEnabled(contentsWidth() > clientWidth);
        int pageStep = (clientWidth - cAmountToKeepWhenPaging);
        if (pageStep < 0) pageStep = clientWidth;
        IntRect oldRect(m_horizontalScrollbar->frameRect());
        IntRect hBarRect = IntRect(0,
                                   height() - m_horizontalScrollbar->height(),
                                   width() - (m_verticalScrollbar ? m_verticalScrollbar->width() : scrollbarOffset.x()),
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

    if (m_verticalScrollbar) {
        int clientHeight = visibleHeight();
        m_verticalScrollbar->setEnabled(contentsHeight() > clientHeight);
        int pageStep = (clientHeight - cAmountToKeepWhenPaging);
        if (pageStep < 0) pageStep = clientHeight;
        IntRect oldRect(m_verticalScrollbar->frameRect());
        IntRect vBarRect = IntRect(width() - m_verticalScrollbar->width(), 
                                   0,
                                   m_verticalScrollbar->width(),
                                   height() - (m_horizontalScrollbar ? m_horizontalScrollbar->height() : scrollbarOffset.y()));
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

    if (oldHasVertical != (m_verticalScrollbar != 0) || oldHasHorizontal != (m_horizontalScrollbar != 0))
        frameRectsChanged();

    // See if our offset has changed in a situation where we might not have scrollbars.
    // This can happen when editing a body with overflow:hidden and scrolling to reveal selection.
    // It can also happen when maximizing a window that has scrollbars (but the new maximized result
    // does not).
    IntSize scrollDelta = scroll - m_scrollOffset;
    if (scrollDelta != IntSize()) {
       m_scrollOffset = scroll;
       m_data->scrollBackingStore(scrollDelta);
    }

    m_data->m_inUpdateScrollbars = false;
}

void ScrollView::platformAddChild(Widget* child)
{
    root()->incrementNativeWidgetCount();
}

void ScrollView::platformRemoveChild(Widget* child)
{
    child->hide();
    root()->decrementNativeWidgetCount();
}

void ScrollView::addToDirtyRegion(const IntRect& containingWindowRect)
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->addToDirtyRegion(containingWindowRect);
}

void ScrollView::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->scrollBackingStore(dx, dy, scrollViewRect, clipRect);
}

void ScrollView::updateBackingStore()
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->updateBackingStore();
}

void ScrollView::incrementNativeWidgetCount()
{
    ++m_data->m_platformWidgets;
}

void ScrollView::decrementNativeWidgetCount()
{
    --m_data->m_platformWidgets;
}

bool ScrollView::hasNativeWidgets() const
{
    return m_data->m_platformWidgets != 0;
}

}

// vim: ts=4 sw=4 et
