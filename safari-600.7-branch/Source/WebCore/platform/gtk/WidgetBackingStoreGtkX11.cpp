/*
 * Copyright (C) 2011, Igalia S.L.
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
#include "WidgetBackingStoreGtkX11.h"

#include "CairoUtilities.h"
#include "GtkVersioning.h"
#include "RefPtrCairo.h"
#include <cairo-xlib.h>
#include <cairo.h>
#include <gdk/gdkx.h>

namespace WebCore {

PassOwnPtr<WidgetBackingStore> WidgetBackingStoreGtkX11::create(GtkWidget* widget, const IntSize& size, float deviceScaleFactor)
{
    return adoptPtr(new WidgetBackingStoreGtkX11(widget, size, deviceScaleFactor));
}

WidgetBackingStoreGtkX11::WidgetBackingStoreGtkX11(GtkWidget* widget, const IntSize& size, float deviceScaleFactor)
    : WidgetBackingStore(size, deviceScaleFactor)
{
    IntSize scaledSize = size;
    scaledSize.scale(deviceScaleFactor);

    GdkVisual* visual = gtk_widget_get_visual(widget);
    GdkScreen* screen = gdk_visual_get_screen(visual);
    m_display = GDK_SCREEN_XDISPLAY(screen);
    m_pixmap = XCreatePixmap(m_display, GDK_WINDOW_XID(gdk_screen_get_root_window(screen)),
        scaledSize.width(), scaledSize.height(), gdk_visual_get_depth(visual));
    m_gc = XCreateGC(m_display, m_pixmap, 0, 0);

    m_surface = adoptRef(cairo_xlib_surface_create(m_display, m_pixmap,
        GDK_VISUAL_XVISUAL(visual), scaledSize.width(), scaledSize.height()));

    cairoSurfaceSetDeviceScale(m_surface.get(), deviceScaleFactor, deviceScaleFactor);
}

WidgetBackingStoreGtkX11::~WidgetBackingStoreGtkX11()
{
    // The pixmap needs to exist when the surface is destroyed, so begin by clearing it.
    m_surface.clear();
    XFreePixmap(m_display, m_pixmap);
    XFreeGC(m_display, m_gc);
}

cairo_surface_t* WidgetBackingStoreGtkX11::cairoSurface()
{
    return m_surface.get();
}

void WidgetBackingStoreGtkX11::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    IntRect targetRect(scrollRect);
    targetRect.move(scrollOffset);
    targetRect.intersect(scrollRect);
    if (targetRect.isEmpty())
        return;

    targetRect.scale(m_deviceScaleFactor);

    IntSize scaledScrollOffset = scrollOffset;
    scaledScrollOffset.scale(m_deviceScaleFactor);

    cairo_surface_flush(m_surface.get());
    XCopyArea(m_display, m_pixmap, m_pixmap, m_gc, 
        targetRect.x() - scaledScrollOffset.width(), targetRect.y() - scaledScrollOffset.height(),
        targetRect.width(), targetRect.height(),
        targetRect.x(), targetRect.y());
    cairo_surface_mark_dirty_rectangle(m_surface.get(),
        targetRect.x(), targetRect.y(),
        targetRect.width(), targetRect.height());
}

} // namespace WebCore
