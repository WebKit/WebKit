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
#include "ScrollbarGtk.h"

#include "IntRect.h"
#include "GraphicsContext.h"
#include "FrameView.h"
#include "ScrollbarTheme.h"
#include "gtkdrawing.h"

#include <gtk/gtk.h>

using namespace WebCore;

PassRefPtr<Scrollbar> Scrollbar::createNativeScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
{
    return adoptRef(new ScrollbarGtk(client, orientation, size));
}

static gboolean gtkScrollEventCallback(GtkWidget* widget, GdkEventScroll* event, ScrollbarGtk*)
{
    /* Scroll only if our parent rejects the scroll event. The rationale for
     * this is that we want the main frame to scroll when we move the mouse
     * wheel over a child scrollbar in most cases. */
    return gtk_widget_event(gtk_widget_get_parent(widget), reinterpret_cast<GdkEvent*>(event));
}

ScrollbarGtk::ScrollbarGtk(ScrollbarClient* client, ScrollbarOrientation orientation,
                           ScrollbarControlSize controlSize)
    : Scrollbar(client, orientation, controlSize)
    , m_adjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)))
{
    GtkWidget* scrollBar = orientation == HorizontalScrollbar ?
                           gtk_hscrollbar_new(m_adjustment):
                           gtk_vscrollbar_new(m_adjustment);
    gtk_widget_show(scrollBar);
    g_signal_connect(scrollBar, "value-changed", G_CALLBACK(ScrollbarGtk::gtkValueChanged), this);
    g_signal_connect(scrollBar, "scroll-event", G_CALLBACK(gtkScrollEventCallback), this);

    setPlatformWidget(scrollBar);

    /*
     * assign a sane default width and height to the Scrollbar, otherwise
     * we will end up with a 0 width scrollbar.
     */
    resize(ScrollbarTheme::nativeTheme()->scrollbarThickness(),
           ScrollbarTheme::nativeTheme()->scrollbarThickness());
}

IntPoint ScrollbarGtk::getLocationInParentWindow(const IntRect& rect)
{
    IntPoint loc;

    if (parent()->isScrollViewScrollbar(this))
        loc = parent()->convertToContainingWindow(rect.location());
    else
        loc = parent()->contentsToWindow(rect.location());

    return loc;
}

void ScrollbarGtk::frameRectsChanged()
{
    if (!parent())
        return;

    IntPoint loc = getLocationInParentWindow(frameRect());

    // Don't allow the allocation size to be negative
    IntSize sz = frameRect().size();
    sz.clampNegativeToZero();

    GtkAllocation allocation = { loc.x(), loc.y(), sz.width(), sz.height() };
    gtk_widget_size_allocate(platformWidget(), &allocation);
}

void ScrollbarGtk::updateThumbPosition()
{
    if (m_adjustment->value != m_currentPos) {
        m_adjustment->value = m_currentPos;
        gtk_adjustment_value_changed(m_adjustment);
    }
}

void ScrollbarGtk::updateThumbProportion()
{
    m_adjustment->step_increment = m_lineStep;
    m_adjustment->page_increment = m_pageStep;
    m_adjustment->page_size = m_visibleSize;
    m_adjustment->upper = m_totalSize;
    gtk_adjustment_changed(m_adjustment);
}

void ScrollbarGtk::setFrameRect(const IntRect& rect)
{
    Widget::setFrameRect(rect);
    frameRectsChanged();
}

void ScrollbarGtk::gtkValueChanged(GtkAdjustment*, ScrollbarGtk* that)
{
    that->setValue(static_cast<int>(gtk_adjustment_get_value(that->m_adjustment)));
}

void ScrollbarGtk::setEnabled(bool shouldEnable)
{
    if (enabled() == shouldEnable)
        return;
        
    Scrollbar::setEnabled(shouldEnable);
    if (platformWidget()) 
        gtk_widget_set_sensitive(platformWidget(), shouldEnable);
}

/*
 * Strategy to painting a Widget:
 *  1.) do not paint if there is no GtkWidget set
 *  2.) We assume that GTK_NO_WINDOW is set and that frameRectsChanged positioned
 *      the widget correctly. ATM we do not honor the GraphicsContext translation.
 */
void ScrollbarGtk::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!platformWidget())
        return;

    if (!context->gdkExposeEvent())
        return;

    GtkWidget* widget = platformWidget();
    ASSERT(GTK_WIDGET_NO_WINDOW(widget));

    GdkEvent* event = gdk_event_new(GDK_EXPOSE);
    event->expose = *context->gdkExposeEvent();
    event->expose.area = static_cast<GdkRectangle>(rect);

    IntPoint loc = getLocationInParentWindow(rect);

    event->expose.area.x = loc.x();
    event->expose.area.y = loc.y();

    event->expose.region = gdk_region_rectangle(&event->expose.area);

    /*
     * This will be unref'ed by gdk_event_free.
     */
    g_object_ref(event->expose.window);

    /*
     * If we are going to paint do the translation and GtkAllocation manipulation.
     */
    if (!gdk_region_empty(event->expose.region))
        gtk_widget_send_expose(widget, event);

    gdk_event_free(event);
}
