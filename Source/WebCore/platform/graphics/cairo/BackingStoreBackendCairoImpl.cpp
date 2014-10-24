/*
 * Copyright (C) 2011,2014 Igalia S.L.
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
#include "BackingStoreBackendCairoImpl.h"

#if USE(CAIRO)

#include "CairoUtilities.h"

namespace WebCore {

BackingStoreBackendCairoImpl::BackingStoreBackendCairoImpl(cairo_surface_t* surface, const IntSize& size)
    : BackingStoreBackendCairo(size)
{
    m_surface = surface;

    // We keep two copies of the surface here, which will double the memory usage, but increase
    // scrolling performance since we do not have to keep reallocating a memory region during
    // quick scrolling requests.
    double xScale, yScale;
    cairoSurfaceGetDeviceScale(m_surface.get(), xScale, yScale);
    IntSize scaledSize = size;
    scaledSize.scale(xScale, yScale);
    m_scrollSurface = adoptRef(cairo_surface_create_similar(surface, CAIRO_CONTENT_COLOR_ALPHA, scaledSize.width(), scaledSize.height()));
}

BackingStoreBackendCairoImpl::~BackingStoreBackendCairoImpl()
{
}

void BackingStoreBackendCairoImpl::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    IntRect targetRect = scrollRect;
    targetRect.move(scrollOffset);
    targetRect.shiftMaxXEdgeTo(targetRect.maxX() - scrollOffset.width());
    targetRect.shiftMaxYEdgeTo(targetRect.maxY() - scrollOffset.height());
    if (targetRect.isEmpty())
        return;

    copyRectFromOneSurfaceToAnother(m_surface.get(), m_scrollSurface.get(), scrollOffset, targetRect);
    copyRectFromOneSurfaceToAnother(m_scrollSurface.get(), m_surface.get(), IntSize(), targetRect);
}

} // namespace WebCore

#endif // USE(CAIRO)
