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

static const Seconds scrollHysteresisDuration { 300_ms };

static RefPtr<cairo_surface_t> createCairoImageSurfaceWithFastMalloc(const IntSize& size, double deviceScaleFactor)
{
    static cairo_user_data_key_t s_surfaceDataKey;
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, size.width());
    auto* surfaceData = fastZeroedMalloc(size.height() * stride);
    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(surfaceData), CAIRO_FORMAT_ARGB32, size.width(), size.height(), stride));
    cairo_surface_set_user_data(surface.get(), &s_surfaceDataKey, surfaceData, [](void* data) { fastFree(data); });
    cairoSurfaceSetDeviceScale(surface.get(), deviceScaleFactor, deviceScaleFactor);
    return surface;
}

BackingStoreBackendCairoImpl::BackingStoreBackendCairoImpl(const IntSize& size, float deviceScaleFactor)
    : BackingStoreBackendCairo(size)
    , m_scrolledHysteresis([this](PAL::HysteresisState state) { if (state == PAL::HysteresisState::Stopped) m_scrollSurface = nullptr; }, scrollHysteresisDuration)
{
    IntSize scaledSize = m_size;
    scaledSize.scale(deviceScaleFactor);
    m_surface = createCairoImageSurfaceWithFastMalloc(scaledSize, deviceScaleFactor);
}

BackingStoreBackendCairoImpl::~BackingStoreBackendCairoImpl() = default;

void BackingStoreBackendCairoImpl::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    IntRect targetRect = scrollRect;
    targetRect.move(scrollOffset);
    targetRect.intersect(scrollRect);
    if (targetRect.isEmpty())
        return;

    if (!m_scrollSurface) {
        IntSize size(cairo_image_surface_get_width(m_surface.get()), cairo_image_surface_get_height(m_surface.get()));
        double xScale, yScale;
        cairoSurfaceGetDeviceScale(m_surface.get(), xScale, yScale);
        ASSERT(xScale == yScale);
        m_scrollSurface = createCairoImageSurfaceWithFastMalloc(size, xScale);
    }
    copyRectFromOneSurfaceToAnother(m_surface.get(), m_scrollSurface.get(), scrollOffset, targetRect);
    copyRectFromOneSurfaceToAnother(m_scrollSurface.get(), m_surface.get(), IntSize(), targetRect);

    m_scrolledHysteresis.impulse();
}

} // namespace WebCore

#endif // USE(CAIRO)
