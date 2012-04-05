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
#include "WidgetBackingStore.h"

#include "GtkVersioning.h"
#include "RefPtrCairo.h"
#include <X11/Xlib.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>
#include <gdk/gdkx.h>

namespace WebCore {

class WidgetBackingStorePrivate {
    WTF_MAKE_NONCOPYABLE(WidgetBackingStorePrivate);
    WTF_MAKE_FAST_ALLOCATED;

public:
    Display* m_display;
    Pixmap m_pixmap;
    GC m_gc;
    RefPtr<cairo_surface_t> m_surface;

    static PassOwnPtr<WidgetBackingStorePrivate> create(GtkWidget* widget, const IntSize& size)
    {
        return adoptPtr(new WidgetBackingStorePrivate(widget, size));
    }

    ~WidgetBackingStorePrivate()
    {
        XFreePixmap(m_display, m_pixmap);
        XFreeGC(m_display, m_gc);
    }

private:
    // We keep two copies of the surface here, which will double the memory usage, but increase
    // scrolling performance since we do not have to keep reallocating a memory region during
    // quick scrolling requests.
    WidgetBackingStorePrivate(GtkWidget* widget, const IntSize& size)
    {
        GdkVisual* visual = gtk_widget_get_visual(widget);
        GdkScreen* screen = gdk_visual_get_screen(visual);
        m_display = GDK_SCREEN_XDISPLAY(screen);
        m_pixmap = XCreatePixmap(m_display,
                                 GDK_WINDOW_XID(gdk_screen_get_root_window(screen)),
                                 size.width(), size.height(),
                                 gdk_visual_get_depth(visual));
        m_gc = XCreateGC(m_display, m_pixmap, 0, 0);

        m_surface = adoptRef(cairo_xlib_surface_create(m_display, m_pixmap,
                                                       GDK_VISUAL_XVISUAL(visual),
                                                       size.width(), size.height()));
    }
};

PassOwnPtr<WidgetBackingStore> WidgetBackingStore::create(GtkWidget* widget, const IntSize& size)
{
    return adoptPtr(new WidgetBackingStore(widget, size));
}

WidgetBackingStore::WidgetBackingStore(GtkWidget* widget, const IntSize& size)
    : m_private(WidgetBackingStorePrivate::create(widget, size))
    , m_size(size)
{
}

WidgetBackingStore::~WidgetBackingStore()
{
}

cairo_surface_t* WidgetBackingStore::cairoSurface()
{
    return m_private->m_surface.get();
}

void WidgetBackingStore::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    IntRect targetRect(scrollRect);
    targetRect.move(scrollOffset);
    targetRect.intersect(scrollRect);
    if (targetRect.isEmpty())
        return;

    cairo_surface_flush(m_private->m_surface.get());
    XCopyArea(m_private->m_display, m_private->m_pixmap, m_private->m_pixmap, m_private->m_gc, 
              targetRect.x() - scrollOffset.width(), targetRect.y() - scrollOffset.height(),
              targetRect.width(), targetRect.height(),
              targetRect.x(), targetRect.y());
    cairo_surface_mark_dirty_rectangle(m_private->m_surface.get(),
                                       targetRect.x(), targetRect.y(),
                                       targetRect.width(), targetRect.height());
}

} // namespace WebCore
