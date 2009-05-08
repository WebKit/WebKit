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
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollbarGtk.h"
#include "ScrollbarTheme.h"

#include <gtk/gtk.h>

using namespace std;

namespace WebCore {

static void adjustmentChanged(GtkAdjustment* adjustment, gpointer _that)
{
    ScrollView* that = reinterpret_cast<ScrollView*>(_that);

    // Figure out if we really moved.
    IntSize newOffset = that->scrollOffset();
    if (adjustment == that->m_horizontalAdjustment)
        newOffset.setWidth(static_cast<int>(gtk_adjustment_get_value(adjustment)));
    else if (adjustment == that->m_verticalAdjustment)
        newOffset.setHeight(static_cast<int>(gtk_adjustment_get_value(adjustment)));

    IntSize scrollDelta = newOffset - that->scrollOffset();
    if (scrollDelta == IntSize())
        return;
    that->setScrollOffset(newOffset);

    if (that->scrollbarsSuppressed())
        return;

    that->scrollContents(scrollDelta);
}

void ScrollView::platformInit()
{
    m_horizontalAdjustment = 0;
    m_verticalAdjustment = 0;
}

void ScrollView::platformDestroy()
{
    if (m_horizontalAdjustment) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_horizontalAdjustment), (gpointer)adjustmentChanged, this);
        g_object_unref(m_horizontalAdjustment);
    }

    if (m_verticalAdjustment) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_verticalAdjustment), (gpointer)adjustmentChanged, this);
        g_object_unref(m_verticalAdjustment);
    }
}

/*
 * The following is assumed:
 *   (hadj && vadj) || (!hadj && !vadj)
 */
void ScrollView::setGtkAdjustments(GtkAdjustment* hadj, GtkAdjustment* vadj)
{
    ASSERT(!hadj == !vadj);

    if (m_horizontalAdjustment) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_horizontalAdjustment), (gpointer)adjustmentChanged, this);
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_verticalAdjustment), (gpointer)adjustmentChanged, this);
        g_object_unref(m_horizontalAdjustment);
        g_object_unref(m_verticalAdjustment);
    }

    m_horizontalAdjustment = hadj;
    m_verticalAdjustment = vadj;

    if (m_horizontalAdjustment) {
        g_signal_connect(m_horizontalAdjustment, "value-changed", G_CALLBACK(adjustmentChanged), this);
        g_signal_connect(m_verticalAdjustment, "value-changed", G_CALLBACK(adjustmentChanged), this);

        /*
         * disable the scrollbars (if we have any) as the GtkAdjustment over
         */
        setHasVerticalScrollbar(false);
        setHasHorizontalScrollbar(false);

        g_object_ref(m_horizontalAdjustment);
        g_object_ref(m_verticalAdjustment);
    }

    updateScrollbars(m_scrollOffset);
}

void ScrollView::platformAddChild(Widget* child)
{
    if (!GTK_IS_SOCKET(child->platformWidget()))
        gtk_container_add(GTK_CONTAINER(hostWindow()->platformWindow()), child->platformWidget());
}

void ScrollView::platformRemoveChild(Widget* child)
{
    GtkWidget* parent;

    // HostWindow can be NULL here. If that's the case
    // let's grab the child's parent instead.
    if (hostWindow())
        parent = GTK_WIDGET(hostWindow()->platformWindow());
    else
        parent = GTK_WIDGET(child->platformWidget()->parent);

    if (GTK_IS_CONTAINER(parent) && parent == child->platformWidget()->parent)
        gtk_container_remove(GTK_CONTAINER(parent), child->platformWidget());
}

bool ScrollView::platformHandleHorizontalAdjustment(const IntSize& scroll)
{
    if (m_horizontalAdjustment) {
        m_horizontalAdjustment->page_size = visibleWidth();
        m_horizontalAdjustment->step_increment = cScrollbarPixelsPerLineStep;
        m_horizontalAdjustment->page_increment = visibleWidth() - cAmountToKeepWhenPaging;
        m_horizontalAdjustment->lower = 0;
        m_horizontalAdjustment->upper = contentsWidth();
        gtk_adjustment_changed(m_horizontalAdjustment);

        if (m_horizontalAdjustment->value != scroll.width()) {
            m_horizontalAdjustment->value = scroll.width();
            gtk_adjustment_value_changed(m_horizontalAdjustment);
        }
        return true;
    }
    return false;
}

bool ScrollView::platformHandleVerticalAdjustment(const IntSize& scroll)
{
    if (m_verticalAdjustment) {
        m_verticalAdjustment->page_size = visibleHeight();
        m_verticalAdjustment->step_increment = cScrollbarPixelsPerLineStep;
        m_verticalAdjustment->page_increment = visibleHeight() - cAmountToKeepWhenPaging;
        m_verticalAdjustment->lower = 0;
        m_verticalAdjustment->upper = contentsHeight();
        gtk_adjustment_changed(m_verticalAdjustment);

        if (m_verticalAdjustment->value != scroll.height()) {
            m_verticalAdjustment->value = scroll.height();
            gtk_adjustment_value_changed(m_verticalAdjustment);
        }
        return true;
    } 
    return false;
}

bool ScrollView::platformHasHorizontalAdjustment() const
{
    return m_horizontalAdjustment != 0;
}

bool ScrollView::platformHasVerticalAdjustment() const
{
    return m_verticalAdjustment != 0;
}

}
