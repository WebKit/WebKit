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

#ifndef BackingStoreBackendCairo_h
#define BackingStoreBackendCairo_h

#if USE(CAIRO)

#include "IntRect.h"
#include "RefPtrCairo.h"
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

typedef struct _cairo_surface cairo_surface_t;

namespace WebCore {

class BackingStoreBackendCairo {
    WTF_MAKE_NONCOPYABLE(BackingStoreBackendCairo);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~BackingStoreBackendCairo() = default;

    cairo_surface_t* surface() const { return m_surface.get(); }
    const IntSize& size() const { return m_size; }

    virtual void scroll(const IntRect& scrollRect, const IntSize& scrollOffset) = 0;

protected:
    BackingStoreBackendCairo(const IntSize& size)
        : m_size(size)
    {
    }

    RefPtr<cairo_surface_t> m_surface;
    IntSize m_size;
};


} // namespace WebCore

#endif // USE(CAIRO)

#endif // BackingStoreBackendCairo_h
