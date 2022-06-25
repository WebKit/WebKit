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

#ifndef BackingStoreBackendCairoX11_h
#define BackingStoreBackendCairoX11_h

#include "BackingStoreBackendCairo.h"

#if USE(CAIRO) && PLATFORM(X11)
#include "XUniquePtr.h"
#include "XUniqueResource.h"

namespace WebCore {

class BackingStoreBackendCairoX11 final : public BackingStoreBackendCairo {
public:
    BackingStoreBackendCairoX11(unsigned long rootWindowID, Visual*, int depth, const IntSize&, float deviceScaleFactor);
    virtual ~BackingStoreBackendCairoX11();

    void scroll(const IntRect& scrollRect, const IntSize& scrollOffset) override;

private:
    XUniquePixmap m_pixmap;
    XUniqueGC m_gc;
};

} // namespace WebCore

#endif // USE(CAIRO) && PLATFORM(X11)

#endif // GtkWidgetBackingStoreX11_h
