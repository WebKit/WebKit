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
#include "GtkWidgetBackingStore.h"

#ifndef XP_UNIX

#include "RefPtrCairo.h"
#include <cairo/cairo.h>
#include <gtk/gtk.h>

namespace WebCore {

static PassRefPtr<cairo_surface_t> createSurfaceForBackingStore(GtkWidget* widget, const IntSize& size)
{
    return gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                             CAIRO_CONTENT_COLOR_ALPHA,
                                             size.width(), size.height());
}


class GtkWidgetBackingStorePrivate {
    WTF_MAKE_NONCOPYABLE(GtkWidgetBackingStorePrivate); WTF_MAKE_FAST_ALLOCATED;

public:
    RefPtr<cairo_surface_t> m_surface;
    RefPtr<cairo_surface_t> m_scrollSurface;

    static PassOwnPtr<GtkWidgetBackingStorePrivate> create(GtkWidget* widget, const IntSize& size)
    {
        return adoptPtr(new GtkWidgetBackingStorePrivate(widget, size));
    }

private:
    // We keep two copies of the surface here, which will double the memory usage, but increase
    // scrolling performance since we do not have to keep reallocating a memory region during
    // quick scrolling requests.
    GtkWidgetBackingStorePrivate(GtkWidget* widget, const IntSize& size)
        : m_surface(createSurfaceForBackingStore(widget, size))
        , m_scrollSurface(createSurfaceForBackingStore(widget, size))
    {
    }
};

PassOwnPtr<GtkWidgetBackingStore> GtkWidgetBackingStore::create(GtkWidget* widget, const IntSize& size)
{
    return adoptPtr(new GtkWidgetBackingStore(widget, size));
}

GtkWidgetBackingStore::GtkWidgetBackingStore(GtkWidget* widget, const IntSize& size)
    : m_private(GtkWidgetBackingStorePrivate::create(widget, size))
{
}

GtkWidgetBackingStore::~GtkWidgetBackingStore()
{
}

cairo_surface_t* GtkWidgetBackingStore::cairoSurface()
{
    return m_private->m_surface.get();
}

static void copyRectFromOneSurfaceToAnother(cairo_surface_t* from, cairo_surface_t* to, const IntSize& offset, const IntRect& rect)
{
    RefPtr<cairo_t> context = adoptRef(cairo_create(to));
    cairo_set_source_surface(context.get(), from, offset.width(), offset.height());
    cairo_rectangle(context.get(), rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill(context.get());
}

void GtkWidgetBackingStore::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    IntRect targetRect(scrollRect);
    targetRect.move(scrollOffset);
    targetRect.shiftMaxXEdgeTo(targetRect.maxX() - scrollOffset.width());
    targetRect.shiftMaxYEdgeTo(targetRect.maxY() - scrollOffset.height());
    if (targetRect.isEmpty())
        return;

    copyRectFromOneSurfaceToAnother(m_private->m_surface.get(), m_private->m_scrollSurface.get(),
                                    scrollOffset, targetRect);
    copyRectFromOneSurfaceToAnother(m_private->m_scrollSurface.get(), m_private->m_surface.get(),
                                    IntSize(), targetRect);
}

} // namespace WebCore

#endif // !XP_UNIX
