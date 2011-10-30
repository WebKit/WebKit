/*
 * Copyright (C) 2011, Igalia S.L.
 * Copyright (C) 2011 Samsung Electronics
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

#include "CairoUtilities.h"
#include "RefPtrCairo.h"
#include <cairo/cairo.h>

#if PLATFORM(GTK)
#include "GtkVersioning.h"
#endif

namespace WebCore {

static PassRefPtr<cairo_surface_t> createSurfaceForBackingStore(PlatformWidget widget, const IntSize& size)
{
#if PLATFORM(GTK)
    return gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                             CAIRO_CONTENT_COLOR_ALPHA,
                                             size.width(), size.height());
#else
    return adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.width(), size.height()));
#endif
}

class WidgetBackingStorePrivate {
    WTF_MAKE_NONCOPYABLE(WidgetBackingStorePrivate);
    WTF_MAKE_FAST_ALLOCATED;

public:
    RefPtr<cairo_surface_t> m_surface;
    RefPtr<cairo_surface_t> m_scrollSurface;

    static PassOwnPtr<WidgetBackingStorePrivate> create(PlatformWidget widget, const IntSize& size)
    {
        return adoptPtr(new WidgetBackingStorePrivate(widget, size));
    }

private:
    // We keep two copies of the surface here, which will double the memory usage, but increase
    // scrolling performance since we do not have to keep reallocating a memory region during
    // quick scrolling requests.
    WidgetBackingStorePrivate(PlatformWidget widget, const IntSize& size)
        : m_surface(createSurfaceForBackingStore(widget, size))
        , m_scrollSurface(createSurfaceForBackingStore(widget, size))
    {
    }
};

PassOwnPtr<WidgetBackingStore> WidgetBackingStore::create(PlatformWidget widget, const IntSize& size)
{
    return adoptPtr(new WidgetBackingStore(widget, size));
}

WidgetBackingStore::WidgetBackingStore(PlatformWidget widget, const IntSize& size)
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
