/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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
#include "Font.h"
#include "FrameGdk.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "RenderObject.h"
#include "NotImplemented.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

namespace WebCore {

class WidgetPrivate {
public:
    GdkDrawable* drawable;
    GtkWidget* widget;
    WidgetClient* client;
    IntRect geometry;
    Font font;
};

Widget::Widget()
    : data(new WidgetPrivate)
{
    data->drawable = 0;
    data->widget = 0;
}

Widget::Widget(GtkWidget* widget)
    : data(new WidgetPrivate)
{
    setGtkWidget(widget);
}

GdkDrawable* Widget::drawable() const
{
    return data->drawable;
}

GtkWidget* Widget::gtkWidget() const
{
    return data->widget;
}

void Widget::setGtkWidget(GtkWidget* widget)
{
    data->drawable = GTK_IS_LAYOUT(widget) ? GTK_LAYOUT(widget)->bin_window : widget->window;
    data->widget = widget;
}

Widget::~Widget()
{
    delete data;
}

void Widget::setClient(WidgetClient* c)
{
    data->client = c;
}

WidgetClient* Widget::client() const
{
    return data->client;
}

IntRect Widget::frameGeometry() const
{
    return data->geometry;
}

void Widget::setFocus()
{
    GtkWidget *widget = data->widget;
    if (!widget)
        return;
    gtk_widget_grab_focus(widget);
}

void Widget::setCursor(const Cursor& cursor)
{
    GdkCursor* pcur = cursor.impl();
    if (!pcur)
        return;

    GdkDrawable* drawable = data->drawable;
    if (!drawable || !GDK_IS_WINDOW(drawable))
        return;
    GdkWindow* window = GDK_WINDOW(drawable);
    gdk_window_set_cursor(window, pcur);

}

void Widget::show()
{
    GdkDrawable* drawable = data->drawable;
    if (!drawable || !GDK_IS_WINDOW(drawable))
        return;
    GdkWindow* window = GDK_WINDOW(drawable);
    gdk_window_show(window);
}

void Widget::hide()
{
    GdkDrawable* drawable = data->drawable;
    if (!drawable || !GDK_IS_WINDOW(drawable))
        return;
    GdkWindow* window = GDK_WINDOW(drawable);
    gdk_window_hide(window);
}

void Widget::setFrameGeometry(const IntRect& r)
{
    data->geometry = r;
    g_return_if_fail(data->widget);

    GtkAllocation allocation = { r.x(), r.y(), r.width(), r.height() };
    gtk_widget_size_allocate(data->widget, &allocation);
}

void Widget::setEnabled(bool)
{
    notImplemented();
}

bool Widget::isEnabled() const
{
    notImplemented();
    return false;
}

void Widget::removeFromParent()
{
    notImplemented();
}

void Widget::paint(GraphicsContext*, IntRect const&)
{
    notImplemented();
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

void Widget::invalidate()
{
    if (data->widget)
        gtk_widget_queue_draw(data->widget);
}

void Widget::invalidateRect(const IntRect& rect)
{
    if (data->widget)
        gtk_widget_queue_draw_area(data->widget, rect.x(), rect.y(),
                                   rect.width(), rect.height());
}

}
