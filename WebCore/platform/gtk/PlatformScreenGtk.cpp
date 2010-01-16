/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Collabora Ltd.
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
#include "PlatformScreen.h"

#include "HostWindow.h"
#include "ScrollView.h"
#include "Widget.h"

#include <gtk/gtk.h>

#if PLATFORM(X11)
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#endif

namespace WebCore {

static GdkVisual* getVisual(Widget* widget)
{
    if (!widget)
        return 0;

    GtkWidget* container = GTK_WIDGET(widget->root()->hostWindow()->platformPageClient());

    if (!container)
        return 0;

    if (!GTK_WIDGET_REALIZED(container)) {
        GtkWidget* toplevel = gtk_widget_get_toplevel(container);
#if GTK_CHECK_VERSION(2, 18, 0)
        if (gtk_widget_is_toplevel(toplevel))
#else
        if (GTK_WIDGET_TOPLEVEL(toplevel))
#endif
            container = toplevel;
        else
            return 0;
    }


    return gdk_drawable_get_visual(GDK_DRAWABLE(container->window));
}

int screenDepth(Widget* widget)
{
    GdkVisual* visual = getVisual(widget);
    if (!visual)
        return 24;
    return visual->depth;
}

int screenDepthPerComponent(Widget* widget)
{
    GdkVisual* visual = getVisual(widget);
    if (!visual)
        return 8;

    return visual->bits_per_rgb;
}

bool screenIsMonochrome(Widget* widget)
{
    return screenDepth(widget) < 2;
}

FloatRect screenRect(Widget* widget)
{
    if (!widget)
        return FloatRect();

    GtkWidget* container = gtk_widget_get_toplevel(GTK_WIDGET(widget->root()->hostWindow()->platformPageClient()));
#if GTK_CHECK_VERSION(2, 18, 0)
    if (!gtk_widget_is_toplevel(container))
#else
    if (!GTK_WIDGET_TOPLEVEL(container))
#endif
        return FloatRect();

    GdkScreen* screen = gtk_widget_has_screen(container) ? gtk_widget_get_screen(container) : gdk_screen_get_default();
    if (!screen)
        return FloatRect();

    gint monitor = gdk_screen_get_monitor_at_window(screen, GTK_WIDGET(container)->window);
    GdkRectangle geometry;
    gdk_screen_get_monitor_geometry(screen, monitor, &geometry);
    
    return FloatRect(geometry.x, geometry.y, geometry.width, geometry.height);
}

FloatRect screenAvailableRect(Widget* widget)
{
    if (!widget)
        return FloatRect();

#if PLATFORM(X11)
    GtkWidget* container = GTK_WIDGET(widget->root()->hostWindow()->platformPageClient());
    if (!container)
        return FloatRect();

    if (!GTK_WIDGET_REALIZED(container))
        return screenRect(widget);

    GdkDrawable* rootWindow = GDK_DRAWABLE(gtk_widget_get_root_window(container));
    GdkDisplay* display = gdk_drawable_get_display(rootWindow);
    Atom xproperty = gdk_x11_get_xatom_by_name_for_display(display, "_NET_WORKAREA");

    Atom retType;
    int retFormat;
    long *workAreaPos = NULL;
    unsigned long retNItems;
    unsigned long retAfter;
    int xRes = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display), GDK_WINDOW_XWINDOW(rootWindow), xproperty,
        0, 4, FALSE, XA_CARDINAL, &retType, &retFormat, &retNItems, &retAfter, (guchar**)&workAreaPos);

    FloatRect rect;
    if (xRes == Success && workAreaPos != NULL && retType == XA_CARDINAL && retNItems == 4 && retFormat == 32) {
        rect = FloatRect(workAreaPos[0], workAreaPos[1], workAreaPos[2], workAreaPos[3]);
        // rect contains the available space in the whole screen not just in the monitor
        // containing the widget, so we intersect it with the monitor rectangle.
        rect.intersect(screenRect(widget));
    } else
        rect = screenRect(widget);

    if (workAreaPos)
        XFree(workAreaPos);

    return rect;
#else
    return screenRect(widget);
#endif
}

} // namespace WebCore
