/*
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef BackingStoreCompositingSurface_h
#define BackingStoreCompositingSurface_h

#if USE(ACCELERATED_COMPOSITING)

#include <BlackBerryPlatformIntRectRegion.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace BlackBerry {

namespace Platform {
namespace Graphics {
struct Buffer;
}
}

namespace WebKit {
class CompositingSurfaceBuffer {
public:
    CompositingSurfaceBuffer(const Platform::IntSize&);
    ~CompositingSurfaceBuffer();
    Platform::IntSize surfaceSize() const;

    Platform::Graphics::Buffer* nativeBuffer() const;

private:
    Platform::IntSize m_size;
    mutable Platform::Graphics::Buffer* m_buffer;
};


class BackingStoreCompositingSurface : public ThreadSafeRefCounted<BackingStoreCompositingSurface> {
public:
    static PassRefPtr<BackingStoreCompositingSurface> create(const Platform::IntSize& size, bool doubleBuffered)
    {
        return adoptRef(new BackingStoreCompositingSurface(size, doubleBuffered));
    }

    ~BackingStoreCompositingSurface();

    CompositingSurfaceBuffer* frontBuffer() const;
    CompositingSurfaceBuffer* backBuffer() const;
    bool isDoubleBuffered() const { return m_isDoubleBuffered; }
    void swapBuffers();

    bool needsSync() const { return m_needsSync; }
    void setNeedsSync(bool needsSync) { m_needsSync = needsSync; }

private:
    BackingStoreCompositingSurface(const Platform::IntSize&, bool doubleBuffered);

    mutable CompositingSurfaceBuffer* m_frontBuffer;
    mutable CompositingSurfaceBuffer* m_backBuffer;
    bool m_isDoubleBuffered;
    bool m_needsSync;
};

} // namespace WebKit
} // namespace BlackBerry

#endif // USE(ACCELERATED_COMPOSITING)

#endif // BackingStoreCompositingSurface_h
