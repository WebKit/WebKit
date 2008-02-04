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
#include "IntRect.h"
#include "NotImplemented.h"
#include "RenderObject.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

namespace WebCore {

class WidgetPrivate {
public:
    GtkWidget* widget;
    WidgetClient* client;
    IntRect frameRect;

    ScrollView* parent;
    GtkContainer* containingWindow;
    bool suppressInvalidation;
    GdkCursor* cursor;

    GdkDrawable* gdkDrawable() const
    {
        return widget ? widget->window : 0;
    }
};

Widget::Widget()
    : data(new WidgetPrivate)
{
    data->widget = 0;
    data->parent = 0;
    data->containingWindow = 0;
    data->suppressInvalidation = false;
    data->cursor = 0;
}

GtkWidget* Widget::gtkWidget() const
{
    return data->widget;
}

void Widget::setGtkWidget(GtkWidget* widget)
{
    data->widget = widget;
}

Widget::~Widget()
{
    ASSERT(!parent());
    delete data;
}

void Widget::setContainingWindow(GtkContainer* containingWindow)
{
    data->containingWindow = containingWindow;
}

GtkContainer* Widget::containingWindow() const
{
    return data->containingWindow;
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
    return data->frameRect;
}

void Widget::setFrameGeometry(const IntRect& r)
{
    data->frameRect = r;
}

void Widget::setParent(ScrollView* v)
{
    data->parent = v;
}

ScrollView* Widget::parent() const
{
    return data->parent;
}

void Widget::setFocus()
{
    gtk_widget_grab_focus(gtkWidget() ? gtkWidget() : GTK_WIDGET(containingWindow()));
}

Cursor Widget::cursor()
{
    return Cursor(data->cursor);
}

void Widget::setCursor(const Cursor& cursor)
{
    GdkCursor* pcur = cursor.impl();

    // http://bugs.webkit.org/show_bug.cgi?id=16388
    // [GTK] Widget::setCursor() gets called frequently
    //
    // gdk_window_set_cursor() in certain GDK backends seems to be an
    // expensive operation, so avoid it if possible.

    if (pcur == data->cursor)
        return;

    gdk_window_set_cursor(data->gdkDrawable() ? GDK_WINDOW(data->gdkDrawable()) : GTK_WIDGET(containingWindow())->window, pcur);
    data->cursor = pcur;
}

void Widget::show()
{
    if (!gtkWidget())
         return;
    gtk_widget_show(gtkWidget());
}

void Widget::hide()
{
    if (!gtkWidget())
         return;
    gtk_widget_hide(gtkWidget());
}

void Widget::setEnabled(bool shouldEnable)
{
    if (!gtkWidget())
        return;
    gtk_widget_set_sensitive(gtkWidget(), shouldEnable);
}

bool Widget::isEnabled() const
{
    if (!gtkWidget())
        return false;
    return GTK_WIDGET_IS_SENSITIVE(gtkWidget());
}

void Widget::removeFromParent()
{
    if (parent())
        parent()->removeChild(this);
}

/*
 * Strategy to painting a Widget:
 *  1.) do not paint if there is no GtkWidget set
 *  2.) We assume that GTK_NO_WINDOW is set and that geometryChanged positioned
 *      the widget correctly. ATM we do not honor the GraphicsContext translation.
 */
void Widget::paint(GraphicsContext* context, const IntRect&)
{
    if (!gtkWidget())
        return;

    if (!context->gdkExposeEvent())
        return;

    GtkWidget* widget = gtkWidget();
    ASSERT(GTK_WIDGET_NO_WINDOW(widget));

    GdkEvent* event = gdk_event_new(GDK_EXPOSE);
    event->expose = *context->gdkExposeEvent();
    event->expose.region = gtk_widget_region_intersect(widget, event->expose.region);

    /*
     * This will be unref'ed by gdk_event_free.
     */
    g_object_ref(event->expose.window);

    /*
     * If we are going to paint do the translation and GtkAllocation manipulation.
     */
    if (!gdk_region_empty(event->expose.region)) {
        gdk_region_get_clipbox(event->expose.region, &event->expose.area);
        gtk_widget_send_expose(widget, event);
    }

    gdk_event_free(event);
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

void Widget::invalidate()
{
    invalidateRect(IntRect(0, 0, width(), height()));
}

void Widget::invalidateRect(const IntRect& rect)
{
    if (data->suppressInvalidation)
        return;

    if (!parent()) {
        gtk_widget_queue_draw_area(GTK_WIDGET(containingWindow()), rect.x(), rect.y(),
                                   rect.width(), rect.height());
        if (isFrameView())
            static_cast<FrameView*>(this)->addToDirtyRegion(rect);
        return;
    }

    // Get the root widget.
    ScrollView* outermostView = parent();
    while (outermostView && outermostView->parent())
        outermostView = outermostView->parent();
    if (!outermostView)
        return;

    IntRect windowRect = convertToContainingWindow(rect);
    gtk_widget_queue_draw_area(GTK_WIDGET(containingWindow()), windowRect.x(), windowRect.y(),
                               windowRect.width(), windowRect.height());
    outermostView->addToDirtyRegion(windowRect);
}

IntPoint Widget::convertToContainingWindow(const IntPoint& point) const
{
    IntPoint windowPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent())
        windowPoint = parentWidget->convertChildToSelf(childWidget, windowPoint);
    return windowPoint;
}

IntPoint Widget::convertFromContainingWindow(const IntPoint& point) const
{
    IntPoint widgetPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent())
        widgetPoint = parentWidget->convertSelfToChild(childWidget, widgetPoint);
    return widgetPoint;
}

IntRect Widget::convertToContainingWindow(const IntRect& rect) const
{
    IntRect convertedRect = rect;
    convertedRect.setLocation(convertToContainingWindow(convertedRect.location()));
    return convertedRect;
}

IntPoint Widget::convertChildToSelf(const Widget* child, const IntPoint& point) const
{
    return IntPoint(point.x() + child->x(), point.y() + child->y());
}

IntPoint Widget::convertSelfToChild(const Widget* child, const IntPoint& point) const
{
    return IntPoint(point.x() - child->x(), point.y() - child->y());
}

bool Widget::suppressInvalidation() const
{
    return data->suppressInvalidation;
}

void Widget::setSuppressInvalidation(bool suppress)
{
    data->suppressInvalidation = suppress;
}

void Widget::geometryChanged() const
{
}
}
