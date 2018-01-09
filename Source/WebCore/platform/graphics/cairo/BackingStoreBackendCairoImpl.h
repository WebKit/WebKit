/*
 * Copyright (C) 2013,2014 Igalia S.L.
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

#pragma once

#if USE(CAIRO)
#include "BackingStoreBackendCairo.h"
#include <pal/HysteresisActivity.h>

namespace WebCore {

class BackingStoreBackendCairoImpl final : public BackingStoreBackendCairo {
public:
    WEBCORE_EXPORT BackingStoreBackendCairoImpl(const IntSize&, float deviceScaleFactor);
    virtual ~BackingStoreBackendCairoImpl();

private:
    void scroll(const IntRect&, const IntSize&) override;

    RefPtr<cairo_surface_t> m_scrollSurface;
    PAL::HysteresisActivity m_scrolledHysteresis;
};

} // namespace WebCore

#endif // USE(CAIRO)
