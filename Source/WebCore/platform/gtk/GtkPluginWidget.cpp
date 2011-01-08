/*
 * Copyright (C) 2009 Holger Hans Peter Freyther
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
#include "GtkPluginWidget.h"

#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "ScrollView.h"

#include <gtk/gtk.h>

namespace WebCore {

GtkPluginWidget::GtkPluginWidget(GtkWidget* widget)
    : Widget(widget)
{
    gtk_widget_hide(widget);
}

void GtkPluginWidget::invalidateRect(const IntRect& _rect)
{
    /* no need to */
    if (!gtk_widget_get_has_window(platformWidget()))
        return;

    GdkWindow* window = gtk_widget_get_window(platformWidget());
    if (!window)
        return;

    GdkRectangle rect = _rect;
    gdk_window_invalidate_rect(window, &rect, FALSE);
}

void GtkPluginWidget::frameRectsChanged()
{
    IntRect rect = frameRect();
    IntPoint loc = parent()->contentsToWindow(rect.location());
    GtkAllocation allocation = { loc.x(), loc.y(), rect.width(), rect.height() };

    gtk_widget_set_size_request(platformWidget(), rect.width(), rect.height());
    gtk_widget_size_allocate(platformWidget(), &allocation);
    gtk_widget_show(platformWidget());
}

void GtkPluginWidget::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!context->gdkExposeEvent())
        return;

    /* only paint widgets with no window this way */
    if (gtk_widget_get_has_window(platformWidget()))
        return;

    GtkWidget* widget = platformWidget();
    ASSERT(!gtk_widget_get_has_window(widget));

    GdkEvent* event = gdk_event_new(GDK_EXPOSE);
    event->expose = *context->gdkExposeEvent();
    event->expose.area = static_cast<GdkRectangle>(rect);

    IntPoint loc = parent()->contentsToWindow(rect.location());

    event->expose.area.x = loc.x();
    event->expose.area.y = loc.y();

#ifdef GTK_API_VERSION_2
    event->expose.region = gdk_region_rectangle(&event->expose.area);
#else
    event->expose.region = cairo_region_create_rectangle(&event->expose.area);
#endif

    /*
     * This will be unref'ed by gdk_event_free.
     */
    g_object_ref(event->expose.window);

    /*
     * If we are going to paint do the translation and GtkAllocation manipulation.
     */
#ifdef GTK_API_VERSION_2
    if (!gdk_region_empty(event->expose.region))
#else
    if (!cairo_region_is_empty(event->expose.region))
#endif
        gtk_widget_send_expose(widget, event);

    gdk_event_free(event);
}

}
