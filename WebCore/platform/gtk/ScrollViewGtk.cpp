/*
 * Copyright (C) 2006, 2007, 2008 Apple Computer, Inc. All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora Ltd.
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

#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "Page.h"
#include "RenderLayer.h"
#include "ScrollbarGtk.h"
#include "ScrollbarTheme.h"

#include <gtk/gtk.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate
{
public:
    ScrollViewPrivate(ScrollView* _view)
        : view(_view)
        , horizontalAdjustment(0)
        , verticalAdjustment(0)
    {}

    ~ScrollViewPrivate()
    {
        if (horizontalAdjustment) {
            g_signal_handlers_disconnect_by_func(G_OBJECT(horizontalAdjustment), (gpointer)ScrollViewPrivate::adjustmentChanged, this);
            g_object_unref(horizontalAdjustment);
        }

        if (verticalAdjustment) {
            g_signal_handlers_disconnect_by_func(G_OBJECT(verticalAdjustment), (gpointer)ScrollViewPrivate::adjustmentChanged, this);
            g_object_unref(verticalAdjustment);
        }
    }

    void scrollBackingStore(const IntSize& scrollDelta);

    static void adjustmentChanged(GtkAdjustment*, gpointer);

    ScrollView* view;
    IntSize viewPortSize;
    GtkAdjustment* horizontalAdjustment;
    GtkAdjustment* verticalAdjustment;
};

void ScrollView::ScrollViewPrivate::scrollBackingStore(const IntSize& scrollDelta)
{
    // Since scrolling is double buffered, we will be blitting the scroll view's intersection
    // with the clip rect every time to keep it smooth.
    IntRect clipRect = view->windowClipRect();
    IntRect scrollViewRect = view->convertToContainingWindow(IntRect(0, 0, view->visibleWidth(), view->visibleHeight()));

    IntRect updateRect = clipRect;
    updateRect.intersect(scrollViewRect);

    //FIXME update here?

    if (view->canBlitOnScroll()) // The main frame can just blit the WebView window
       // FIXME: Find a way to blit subframes without blitting overlapping content
       view->scrollBackingStore(-scrollDelta.width(), -scrollDelta.height(), scrollViewRect, clipRect);
    else  {
       // We need to go ahead and repaint the entire backing store.  Do it now before moving the
       // plugins.
       view->addToDirtyRegion(updateRect);
       view->updateBackingStore();
    }

    view->frameRectsChanged();

    // Now update the window (which should do nothing but a blit of the backing store's updateRect and so should
    // be very fast).
    invalidate();
}

void ScrollView::ScrollViewPrivate::adjustmentChanged(GtkAdjustment* adjustment, gpointer _that)
{
    ScrollViewPrivate* that = reinterpret_cast<ScrollViewPrivate*>(_that);

    // Figure out if we really moved.
    IntSize newOffset = that->view->m_scrollOffset;
    if (adjustment == that->horizontalAdjustment)
        newOffset.setWidth(static_cast<int>(gtk_adjustment_get_value(adjustment)));
    else if (adjustment == that->verticalAdjustment)
        newOffset.setHeight(static_cast<int>(gtk_adjustment_get_value(adjustment)));

    IntSize scrollDelta = newOffset - that->view->m_scrollOffset;
    if (scrollDelta == IntSize())
        return;
    that->view->m_scrollOffset = newOffset;

    if (that->view->scrollbarsSuppressed())
        return;

    that->scrollBackingStore(scrollDelta);
    static_cast<FrameView*>(that->view)->frame()->sendScrollEvent();
}

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate(this))
{
    init();
}

ScrollView::~ScrollView()
{
    destroy();
    delete m_data;
}

/*
 * The following is assumed:
 *   (hadj && vadj) || (!hadj && !vadj)
 */
void ScrollView::setGtkAdjustments(GtkAdjustment* hadj, GtkAdjustment* vadj)
{
    ASSERT(!hadj == !vadj);

    if (m_data->horizontalAdjustment) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_data->horizontalAdjustment), (gpointer)ScrollViewPrivate::adjustmentChanged, m_data);
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_data->verticalAdjustment), (gpointer)ScrollViewPrivate::adjustmentChanged, m_data);
        g_object_unref(m_data->horizontalAdjustment);
        g_object_unref(m_data->verticalAdjustment);
    }

    m_data->horizontalAdjustment = hadj;
    m_data->verticalAdjustment = vadj;

    if (m_data->horizontalAdjustment) {
        g_signal_connect(m_data->horizontalAdjustment, "value-changed", G_CALLBACK(ScrollViewPrivate::adjustmentChanged), m_data);
        g_signal_connect(m_data->verticalAdjustment, "value-changed", G_CALLBACK(ScrollViewPrivate::adjustmentChanged), m_data);

        /*
         * disable the scrollbars (if we have any) as the GtkAdjustment over
         */
        setHasVerticalScrollbar(false);
        setHasHorizontalScrollbar(false);

        g_object_ref(m_data->horizontalAdjustment);
        g_object_ref(m_data->verticalAdjustment);
    }

    updateScrollbars(m_scrollOffset);
}

void ScrollView::platformAddChild(Widget* child)
{
    if (!GTK_IS_SOCKET(child->platformWidget()))
        gtk_container_add(GTK_CONTAINER(containingWindow()), child->platformWidget());
}

void ScrollView::platformRemoveChild(Widget* child)
{
    if (GTK_WIDGET(containingWindow()) == GTK_WIDGET(child->platformWidget())->parent)
        gtk_container_remove(GTK_CONTAINER(containingWindow()), child->platformWidget());
}

bool ScrollView::platformHandleHorizontalAdjustment(const IntSize& scroll)
{
    if (m_data->horizontalAdjustment) {
        m_data->horizontalAdjustment->page_size = visibleWidth();
        m_data->horizontalAdjustment->step_increment = visibleWidth() / 10.0;
        m_data->horizontalAdjustment->page_increment = visibleWidth() * 0.9;
        m_data->horizontalAdjustment->lower = 0;
        m_data->horizontalAdjustment->upper = contentsWidth();
        gtk_adjustment_changed(m_data->horizontalAdjustment);

        if (m_scrollOffset.width() != scroll.width()) {
            m_data->horizontalAdjustment->value = scroll.width();
            gtk_adjustment_value_changed(m_data->horizontalAdjustment);
        }
        return true;
    }
    return false;
}

bool ScrollView::platformHandleVerticalAdjustment(const IntSize& scroll)
{
    if (m_data->verticalAdjustment) {
        m_data->verticalAdjustment->page_size = visibleHeight();
        m_data->verticalAdjustment->step_increment = visibleHeight() / 10.0;
        m_data->verticalAdjustment->page_increment = visibleHeight() * 0.9;
        m_data->verticalAdjustment->lower = 0;
        m_data->verticalAdjustment->upper = contentsHeight();
        gtk_adjustment_changed(m_data->verticalAdjustment);

        if (m_scrollOffset.height() != scroll.height()) {
            m_data->verticalAdjustment->value = scroll.height();
            gtk_adjustment_value_changed(m_data->verticalAdjustment);
        }
        return true;
    } 
    return false;
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

}
