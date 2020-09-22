/*
 * Copyright (C) 2011,2014 Igalia S.L.
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
#include "BackingStoreBackendCairoX11.h"

#if USE(CAIRO) && PLATFORM(X11)

#include "CairoUtilities.h"
#include <cairo-xlib.h>

namespace WebCore {

BackingStoreBackendCairoX11::BackingStoreBackendCairoX11(unsigned long rootWindowID, Visual* visual, int depth, const IntSize& size, float deviceScaleFactor)
    : BackingStoreBackendCairo(size)
{
    IntSize scaledSize = size;
    scaledSize.scale(deviceScaleFactor);

    auto* display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
    m_pixmap = XCreatePixmap(display, rootWindowID, scaledSize.width(), scaledSize.height(), depth);
    m_gc.reset(XCreateGC(display, m_pixmap.get(), 0, nullptr));

    m_surface = adoptRef(cairo_xlib_surface_create(display, m_pixmap.get(), visual, scaledSize.width(), scaledSize.height()));
    cairo_surface_set_device_scale(m_surface.get(), deviceScaleFactor, deviceScaleFactor);
}

BackingStoreBackendCairoX11::~BackingStoreBackendCairoX11()
{
    // The pixmap needs to exist when the surface is destroyed, so begin by clearing it.
    m_surface = nullptr;
}

void BackingStoreBackendCairoX11::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    IntRect targetRect = scrollRect;
    targetRect.move(scrollOffset);
    targetRect.intersect(scrollRect);
    if (targetRect.isEmpty())
        return;

    double xScale, yScale;
    cairo_surface_get_device_scale(m_surface.get(), &xScale, &yScale);
    ASSERT(xScale == yScale);

    IntSize scaledScrollOffset = scrollOffset;
    targetRect.scale(xScale);
    scaledScrollOffset.scale(xScale, yScale);

    cairo_surface_flush(m_surface.get());
    XCopyArea(downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native(), m_pixmap.get(), m_pixmap.get(), m_gc.get(),
        targetRect.x() - scaledScrollOffset.width(), targetRect.y() - scaledScrollOffset.height(),
        targetRect.width(), targetRect.height(), targetRect.x(), targetRect.y());
    cairo_surface_mark_dirty_rectangle(m_surface.get(), targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height());
}

} // namespace WebCore

#endif // USE(CAIRO) && PLATFORM(X11)
