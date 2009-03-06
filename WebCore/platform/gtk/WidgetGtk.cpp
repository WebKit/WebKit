/*
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
#include "Widget.h"

#include "Cursor.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "RenderObject.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

namespace WebCore {

static GdkCursor* lastSetCursor;

Widget::Widget(PlatformWidget widget)
{
    init(widget);
}

Widget::~Widget()
{
    ASSERT(!parent());
    releasePlatformWidget();
}

void Widget::setFocus()
{
    gtk_widget_grab_focus(platformWidget() ? platformWidget() : GTK_WIDGET(root()->hostWindow()->platformWindow()));
}

static GdkDrawable* gdkDrawable(PlatformWidget widget)
{
    return widget ? widget->window : 0;
}
    
void Widget::setCursor(const Cursor& cursor)
{
    GdkCursor* platformCursor = cursor.impl();

    // http://bugs.webkit.org/show_bug.cgi?id=16388
    // [GTK] Widget::setCursor() gets called frequently
    //
    // gdk_window_set_cursor() in certain GDK backends seems to be an
    // expensive operation, so avoid it if possible.

    if (platformCursor == lastSetCursor)
        return;

    gdk_window_set_cursor(gdkDrawable(platformWidget()) ? GDK_WINDOW(gdkDrawable(platformWidget())) : GTK_WIDGET(root()->hostWindow()->platformWindow())->window, platformCursor);
    lastSetCursor = platformCursor;
}

void Widget::show()
{
    if (!platformWidget())
         return;
    gtk_widget_show(platformWidget());
}

void Widget::hide()
{
    if (!platformWidget())
         return;
    gtk_widget_hide(platformWidget());
}

/*
 * Strategy to painting a Widget:
 *  1.) do not paint if there is no GtkWidget set
 *  2.) We assume that GTK_NO_WINDOW is set and that frameRectsChanged positioned
 *      the widget correctly. ATM we do not honor the GraphicsContext translation.
 */
void Widget::paint(GraphicsContext* context, const IntRect& rect)
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

    IntPoint loc = parent()->contentsToWindow(rect.location());
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

void Widget::setIsSelected(bool)
{
    notImplemented();
}

IntRect Widget::frameRect() const
{
    return m_frame;
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;
}

void Widget::releasePlatformWidget()
{
    if (!platformWidget())
         return;
    g_object_unref(platformWidget());
}

void Widget::retainPlatformWidget()
{
    if (!platformWidget())
         return;
    g_object_ref_sink(platformWidget());
}

}
