/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther zecke@selfish.org
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "PlatformScrollBar.h"

#include "IntRect.h"
#include "GraphicsContext.h"
#include "FrameView.h"
#include "NotImplemented.h"
#include "gtkdrawing.h"

#include <gtk/gtk.h>

using namespace WebCore;

static gboolean gtkScrollEventCallback(GtkWidget* widget, GdkEventScroll* event, PlatformScrollbar*)
{
    /* Scroll only if our parent rejects the scroll event. The rationale for
     * this is that we want the main frame to scroll when we move the mouse
     * wheel over a child scrollbar in most cases. */
    return gtk_widget_event(gtk_widget_get_parent(widget), reinterpret_cast<GdkEvent*>(event));
}

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation,
                                     ScrollbarControlSize controlSize)
    : Scrollbar(client, orientation, controlSize)
    , m_adjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)))
{
    GtkScrollbar* scrollBar = orientation == HorizontalScrollbar ?
                              GTK_SCROLLBAR(::gtk_hscrollbar_new(m_adjustment)) :
                              GTK_SCROLLBAR(::gtk_vscrollbar_new(m_adjustment));
    gtk_widget_show(GTK_WIDGET(scrollBar));
    g_object_ref(G_OBJECT(scrollBar));
    g_signal_connect(G_OBJECT(scrollBar), "value-changed", G_CALLBACK(PlatformScrollbar::gtkValueChanged), this);
    g_signal_connect(G_OBJECT(scrollBar), "scroll-event", G_CALLBACK(gtkScrollEventCallback), this);

    setGtkWidget(GTK_WIDGET(scrollBar));

    /*
     * assign a sane default width and height to the ScrollBar, otherwise
     * we will end up with a 0 width scrollbar.
     */
    resize(PlatformScrollbar::horizontalScrollbarHeight(),
           PlatformScrollbar::verticalScrollbarWidth());
}

PlatformScrollbar::~PlatformScrollbar()
{
    /*
     * the Widget does not take over ownership.
     */
    g_signal_handlers_disconnect_by_func(G_OBJECT(gtkWidget()), (gpointer)PlatformScrollbar::gtkValueChanged, this);
    g_signal_handlers_disconnect_by_func(G_OBJECT(gtkWidget()), (gpointer)gtkScrollEventCallback, this);
    g_object_unref(G_OBJECT(gtkWidget()));
}

int PlatformScrollbar::width() const
{
    return Widget::width();
}

int PlatformScrollbar::height() const
{
    return Widget::height();
}

void PlatformScrollbar::setEnabled(bool enabled)
{
    Widget::setEnabled(enabled);
}

void PlatformScrollbar::paint(GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    Widget::paint(graphicsContext, damageRect);
}

void PlatformScrollbar::updateThumbPosition()
{
    if (m_adjustment->value != m_currentPos) {
        m_adjustment->value = m_currentPos;
        gtk_adjustment_value_changed(m_adjustment);
    }
}

void PlatformScrollbar::updateThumbProportion()
{
    m_adjustment->step_increment = m_lineStep;
    m_adjustment->page_increment = m_pageStep;
    m_adjustment->page_size = m_visibleSize;
    m_adjustment->upper = m_totalSize;
    gtk_adjustment_changed(m_adjustment);
}

void PlatformScrollbar::setRect(const IntRect& rect)
{
    setFrameGeometry(rect);
    geometryChanged();
}

void PlatformScrollbar::geometryChanged()
{
    if (!parent())
        return;

    ASSERT(parent()->isFrameView());

    FrameView* frameView = static_cast<FrameView*>(parent());
    IntRect windowRect = IntRect(frameView->contentsToWindow(frameGeometry().location()), frameGeometry().size());
    GtkAllocation allocation = { windowRect.x(), windowRect.y(), windowRect.width(), windowRect.height() };
    gtk_widget_size_allocate(gtkWidget(), &allocation);
}

void PlatformScrollbar::gtkValueChanged(GtkAdjustment*, PlatformScrollbar* that)
{
    that->setValue(static_cast<int>(gtk_adjustment_get_value(that->m_adjustment)));
}

static int scrollbarSize()
{
    static int size = 0;

    if (size)
        return size;

    MozGtkScrollbarMetrics metrics;
    moz_gtk_get_scrollbar_metrics(&metrics);
    size = metrics.slider_width;

    return size;
}

int PlatformScrollbar::horizontalScrollbarHeight()
{
    return scrollbarSize();
}

int PlatformScrollbar::verticalScrollbarWidth()
{
    return scrollbarSize();
}
