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

#ifndef BackingStoreBackendCairoImpl_h
#define BackingStoreBackendCairoImpl_h

#include "BackingStoreBackendCairo.h"

#if USE(CAIRO)

namespace WebCore {

class BackingStoreBackendCairoImpl final : public BackingStoreBackendCairo {
public:
    BackingStoreBackendCairoImpl(cairo_surface_t*, const IntSize&);
    virtual ~BackingStoreBackendCairoImpl();

    void scroll(const IntRect&, const IntSize&) override;

private:
    RefPtr<cairo_surface_t> m_scrollSurface;
};

} // namespace WebCore

#endif // USE(CAIRO)

#endif // BackingStoreBackendCairoImpl_h
